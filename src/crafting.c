#include "conf.h"
#include "sysdep.h"

#include "crafting.h"
#include "structs.h"
#include "utils.h"
#include "comm.h"
#include "act.h"
#include "handler.h"
#include "interpreter.h"
#include "spells.h"
#include "db.h"

#define CRAFT_SCROLL_KEY "__craft_scroll_spells"
#define CRAFT_TOME_KEY "__craft_tome_spells"
#define CRAFT_TOME_COUNT_KEY "__craft_tome_count"
#define CRAFT_POTION_STACK_KEY "__craft_potion_stack"
#define CRAFT_ENCHANT_MARK_KEY "__craft_enchanted"
#define CRAFT_ENCHANT_LIST_KEY "__craft_enchant_recipes"
#define MAX_SCRIBE_SPELLS 4
#define MAX_CRAFT_PAYLOAD 256
#define MAX_POTION_STACK 100
#define MAX_ITEM_ENCHANTS 4

static const int enchant_attempt_penalty[MAX_ITEM_ENCHANTS] = {0, 15, 35, 60};
static const int enchant_stack_scale_pct[MAX_ITEM_ENCHANTS] = {100, 75, 50, 25};
static const int fourth_enchant_fail_destroy_pct = 35;
static const char *crafting_discipline_name(int disc);
static const char *crafting_material_tier_name(int tier);

struct enchant_recipe {
  const char *name;
  const char *label;
  int min_level;
  int apply_loc;
  int base_modifier;
  int required_tier;
  int compat_flags;
};

enum enchant_compat_flags {
  ENCH_COMPAT_ANY_EQUIPPABLE = 1 << 0,
  ENCH_COMPAT_WEAPON         = 1 << 1,
  ENCH_COMPAT_ARMOR          = 1 << 2,
  ENCH_COMPAT_SHIELD         = 1 << 3
};

static const struct enchant_recipe enchant_recipes[] = {
  {"sturdy",        "Sturdy",        1,   APPLY_AC,          -2, CRAFT_MAT_TIER_LESSER,   ENCH_COMPAT_ARMOR | ENCH_COMPAT_SHIELD},
  {"accurate",      "Accurate",      8,   APPLY_HITROLL,      1, CRAFT_MAT_TIER_LESSER,   ENCH_COMPAT_WEAPON},
  {"mighty",        "Mighty",        14,  APPLY_DAMROLL,      1, CRAFT_MAT_TIER_LESSER,   ENCH_COMPAT_WEAPON},
  {"vitality",      "Vitality",      20,  APPLY_HIT,          8, CRAFT_MAT_TIER_LESSER,   ENCH_COMPAT_ANY_EQUIPPABLE},
  {"precision",     "Precision",     28,  APPLY_HITROLL,      2, CRAFT_MAT_TIER_GREATER,  ENCH_COMPAT_WEAPON},
  {"slaying",       "Slaying",       36,  APPLY_DAMROLL,      2, CRAFT_MAT_TIER_GREATER,  ENCH_COMPAT_WEAPON},
  {"fortified",     "Fortified",     44,  APPLY_AC,          -4, CRAFT_MAT_TIER_GREATER,  ENCH_COMPAT_ARMOR | ENCH_COMPAT_SHIELD},
  {"warding",       "Warding",       52,  APPLY_SAVING_SPELL,-1, CRAFT_MAT_TIER_GREATER,  ENCH_COMPAT_ANY_EQUIPPABLE},
  {"focus",         "Focus",         60,  APPLY_WIS,          1, CRAFT_MAT_TIER_GREATER,  ENCH_COMPAT_ANY_EQUIPPABLE},
  {"fury",          "Fury",          68,  APPLY_STR,          1, CRAFT_MAT_TIER_GREATER,  ENCH_COMPAT_ANY_EQUIPPABLE},
  {"swiftness",     "Swiftness",     76,  APPLY_DEX,          1, CRAFT_MAT_TIER_SUPERIOR, ENCH_COMPAT_ANY_EQUIPPABLE},
  {"bulwark",       "Bulwark",       84,  APPLY_AC,          -6, CRAFT_MAT_TIER_SUPERIOR, ENCH_COMPAT_ARMOR | ENCH_COMPAT_SHIELD},
  {"spellward",     "Spellward",     92,  APPLY_SAVING_SPELL,-2, CRAFT_MAT_TIER_SUPERIOR, ENCH_COMPAT_ANY_EQUIPPABLE},
  {"grandmastery",  "Grandmastery",  100, APPLY_HITROLL,      3, CRAFT_MAT_TIER_SUPERIOR, ENCH_COMPAT_WEAPON},
  {NULL, NULL, 0, 0, 0, 0, 0}
};

static int enchant_recipe_index_by_name(const char *name)
{
  int i;

  if (!name || !*name)
    return -1;

  for (i = 0; enchant_recipes[i].name; i++)
    if (!str_cmp(name, enchant_recipes[i].name))
      return i;

  return -1;
}

static const char *find_exdesc_value(const struct obj_data *obj, const char *key)
{
  struct extra_descr_data *ex;

  if (!obj || !key)
    return NULL;

  for (ex = obj->ex_description; ex; ex = ex->next)
    if (ex->keyword && ex->description && !strcmp(ex->keyword, key))
      return ex->description;

  return NULL;
}

static void set_exdesc_value(struct obj_data *obj, const char *key, const char *value)
{
  struct extra_descr_data *ex;

  if (!obj || !key)
    return;

  for (ex = obj->ex_description; ex; ex = ex->next) {
    if (ex->keyword && !strcmp(ex->keyword, key)) {
      if (ex->description)
        free(ex->description);
      ex->description = strdup(value ? value : "");
      return;
    }
  }

  CREATE(ex, struct extra_descr_data, 1);
  ex->keyword = strdup(key);
  ex->description = strdup(value ? value : "");
  ex->next = obj->ex_description;
  obj->ex_description = ex;
}

static int parse_spell_payload(const char *payload, int out[], int max_out)
{
  int count = 0;
  char buf[MAX_CRAFT_PAYLOAD];
  char *p, *tok;

  if (!payload || !*payload || !out || max_out <= 0)
    return 0;

  strlcpy(buf, payload, sizeof(buf));
  p = buf;
  while ((tok = strtok(count == 0 ? p : NULL, " ")) != NULL) {
    int spell;
    if (!*tok)
      continue;
    spell = atoi(tok);
    if (spell <= 0 || spell > TOP_SPELL_DEFINE)
      return 0;
    if (count >= max_out)
      return 0;
    out[count++] = spell;
  }

  return count;
}

static int parse_enchant_payload(const char *payload, int out[], int max_out)
{
  int count = 0;
  char buf[MAX_CRAFT_PAYLOAD];
  char *p, *tok;

  if (!payload || !*payload || !out || max_out <= 0)
    return 0;

  strlcpy(buf, payload, sizeof(buf));
  p = buf;
  while ((tok = strtok(count == 0 ? p : NULL, " ")) != NULL) {
    int idx;
    if (!*tok)
      continue;
    idx = atoi(tok);
    if (idx < 0 || !enchant_recipes[idx].name)
      return 0;
    if (count >= max_out)
      return 0;
    out[count++] = idx;
  }

  return count;
}

static void build_spell_payload(const int spells[], int count, char *out, size_t outsz)
{
  size_t used = 0;
  int i;

  if (!out || outsz == 0)
    return;
  *out = '\0';

  for (i = 0; i < count; i++) {
    int wrote = snprintf(out + used, outsz - used, "%s%d", (i == 0) ? "" : " ", spells[i]);
    if (wrote < 0 || (size_t)wrote >= outsz - used)
      break;
    used += (size_t)wrote;
  }
}

static int get_item_enchant_history(const struct obj_data *obj, int out[], int max_out)
{
  const char *payload;

  if (!obj || !out || max_out <= 0)
    return 0;

  payload = find_exdesc_value(obj, CRAFT_ENCHANT_LIST_KEY);
  if (!payload || !*payload)
    return 0;

  return parse_enchant_payload(payload, out, max_out);
}

static void append_item_enchant_history(struct obj_data *obj, int recipe_index)
{
  int recipes[MAX_ITEM_ENCHANTS];
  int count;
  char payload[MAX_CRAFT_PAYLOAD];

  if (!obj || recipe_index < 0 || !enchant_recipes[recipe_index].name)
    return;

  count = get_item_enchant_history(obj, recipes, MAX_ITEM_ENCHANTS);
  if (count < 0 || count >= MAX_ITEM_ENCHANTS)
    return;

  recipes[count++] = recipe_index;
  build_spell_payload(recipes, count, payload, sizeof(payload));
  set_exdesc_value(obj, CRAFT_ENCHANT_LIST_KEY, payload);
  set_exdesc_value(obj, CRAFT_ENCHANT_MARK_KEY, "1");
}

int crafting_get_enchant_count(const struct obj_data *obj)
{
  int recipes[MAX_ITEM_ENCHANTS];
  return get_item_enchant_history(obj, recipes, MAX_ITEM_ENCHANTS);
}

int crafting_get_enchant_recipe_count(const struct obj_data *obj, const char *recipe_name)
{
  int recipes[MAX_ITEM_ENCHANTS];
  int i, count, idx, found = 0;

  idx = enchant_recipe_index_by_name(recipe_name);
  if (idx < 0)
    return 0;

  count = get_item_enchant_history(obj, recipes, MAX_ITEM_ENCHANTS);
  for (i = 0; i < count; i++)
    if (recipes[i] == idx)
      found++;

  return found;
}

int crafting_is_item_enchanted(const struct obj_data *obj)
{
  const char *marker = find_exdesc_value(obj, CRAFT_ENCHANT_MARK_KEY);
  if (marker && *marker && atoi(marker) > 0)
    return TRUE;
  return crafting_get_enchant_count(obj) > 0;
}

void crafting_build_enchant_tag(const struct obj_data *obj, char *out, size_t outsz)
{
  int count;

  if (!out || outsz == 0)
    return;

  out[0] = '\0';
  if (!obj)
    return;

  count = crafting_get_enchant_count(obj);
  if (count <= 0)
    return;

  snprintf(out, outsz, "@m[@MEn@mch@Ma@mnt@Med@m %d/%d]@n ", MIN(MAX_ITEM_ENCHANTS, count), MAX_ITEM_ENCHANTS);
}

void crafting_build_enchant_recipe_summary(const struct obj_data *obj, char *out, size_t outsz)
{
  int recipes[MAX_ITEM_ENCHANTS];
  int i, count;
  int used = 0;

  if (!out || outsz == 0)
    return;
  out[0] = '\0';

  count = get_item_enchant_history(obj, recipes, MAX_ITEM_ENCHANTS);
  for (i = 0; i < count; i++) {
    int idx = recipes[i];
    int wrote;
    if (idx < 0 || !enchant_recipes[idx].name)
      continue;
    wrote = snprintf(out + used, outsz - (size_t)used, "%s%s", (used == 0) ? "" : ", ", enchant_recipes[idx].name);
    if (wrote < 0 || (size_t)wrote >= outsz - (size_t)used)
      break;
    used += wrote;
  }
}

static int scaled_enchant_modifier(int base_modifier, int stack_count_before_apply)
{
  int scale_idx, pct, scaled;

  if (base_modifier == 0)
    return 0;

  scale_idx = MAX(0, MIN(MAX_ITEM_ENCHANTS - 1, stack_count_before_apply));
  pct = enchant_stack_scale_pct[scale_idx];
  scaled = (abs(base_modifier) * pct + 99) / 100;
  if (scaled <= 0)
    scaled = 1;
  return (base_modifier > 0) ? scaled : -scaled;
}

static int item_can_resolve_to_hold_slot(const struct obj_data *obj)
{
  if (!obj)
    return FALSE;

  /*
   * Mirror the actual HOLD entry points used by the game:
   * - do_grab() allows objects with ITEM_WEAR_HOLD.
   * - do_offhand() allows ITEM_WEAPON with ITEM_OFFHAND into WEAR_HOLD.
   */
  if (CAN_WEAR((struct obj_data *)obj, ITEM_WEAR_HOLD))
    return TRUE;

  return (GET_OBJ_TYPE((struct obj_data *)obj) == ITEM_WEAPON &&
          OBJ_FLAGGED((struct obj_data *)obj, ITEM_OFFHAND));
}

static int item_can_resolve_to_real_eq_slot(struct char_data *ch, const struct obj_data *obj)
{
  if (!obj)
    return FALSE;

  /* Reuse the actual wear slot resolver used by do_wear(). */
  if (find_eq_pos(ch, (struct obj_data *)obj, NULL) >= 0)
    return TRUE;

  return item_can_resolve_to_hold_slot(obj);
}

static int item_type_is_disallowed_for_enchant(const struct obj_data *obj)
{
  if (!obj)
    return TRUE;

  switch (GET_OBJ_TYPE((struct obj_data *)obj)) {
  case ITEM_SCROLL:
  case ITEM_WAND:
  case ITEM_STAFF:
  case ITEM_POTION:
  case ITEM_CONTAINER:
  case ITEM_TRASH:
  case ITEM_NOTE:
  case ITEM_DRINKCON:
  case ITEM_KEY:
  case ITEM_FOOD:
  case ITEM_MONEY:
  case ITEM_PEN:
  case ITEM_BOAT:
  case ITEM_FOUNTAIN:
    return TRUE;
  default:
    return FALSE;
  }
}

static int item_is_valid_for_enchant(struct char_data *ch, const struct obj_data *obj)
{
  if (!obj)
    return FALSE;
  if (item_type_is_disallowed_for_enchant(obj))
    return FALSE;
  return item_can_resolve_to_real_eq_slot(ch, obj);
}

static int find_free_enchant_affect_slot(const struct obj_data *obj)
{
  int i;

  if (!obj)
    return -1;

  for (i = 0; i < MAX_OBJ_AFFECT; i++) {
    if (obj->affected[i].location == APPLY_NONE)
      return i;
  }

  return -1;
}

static int recipe_is_compatible_with_item(const struct enchant_recipe *recipe, const struct obj_data *obj)
{
  int type;
  int is_armor_like;

  if (!recipe || !obj)
    return FALSE;

  if (recipe->compat_flags & ENCH_COMPAT_ANY_EQUIPPABLE)
    return TRUE;

  type = GET_OBJ_TYPE((struct obj_data *)obj);
  is_armor_like = (type == ITEM_ARMOR || type == ITEM_WORN);

  if ((recipe->compat_flags & ENCH_COMPAT_WEAPON) && type == ITEM_WEAPON)
    return TRUE;
  if ((recipe->compat_flags & ENCH_COMPAT_SHIELD) && type == ITEM_ARMOR &&
      CAN_WEAR((struct obj_data *)obj, ITEM_WEAR_SHIELD))
    return TRUE;
  if ((recipe->compat_flags & ENCH_COMPAT_ARMOR) && is_armor_like && !CAN_WEAR((struct obj_data *)obj, ITEM_WEAR_WIELD))
    return TRUE;

  return FALSE;
}

static int item_disenchant_effect_score(const struct obj_data *obj)
{
  int i, score = 0;
  int enchant_count = 0;

  if (!obj)
    return 0;

  for (i = 0; i < MAX_OBJ_AFFECT; i++) {
    int loc = obj->affected[i].location;
    int mod = obj->affected[i].modifier;
    if (loc == APPLY_NONE || mod == 0)
      continue;
    score += 4;
    score += MIN(12, abs(mod) / 2);
  }

  enchant_count = crafting_get_enchant_count(obj);
  if (enchant_count > 0)
    score += enchant_count * 6;

  if (crafting_is_item_enchanted(obj))
    score += 4;

  if (OBJ_FLAGGED((struct obj_data *)obj, ITEM_MAGIC))
    score += 6;

  return score;
}

static int disenchant_tier_from_score(int score)
{
  if (score >= 28)
    return CRAFT_MAT_TIER_SUPERIOR;
  if (score >= 12)
    return CRAFT_MAT_TIER_GREATER;
  return CRAFT_MAT_TIER_LESSER;
}

static struct obj_data *create_craft_material_item(int discipline, int tier)
{
  struct obj_data *obj;
  const char *tier_name = crafting_material_tier_name(tier);
  char name_buf[MAX_INPUT_LENGTH];
  char short_buf[MAX_INPUT_LENGTH];
  char desc_buf[MAX_INPUT_LENGTH];

  obj = create_obj();
  if (!obj)
    return NULL;

  snprintf(name_buf, sizeof(name_buf), "%s %s material crafting", tier_name, crafting_discipline_name(discipline));
  snprintf(short_buf, sizeof(short_buf), "a %s %s material", tier_name, crafting_discipline_name(discipline));
  snprintf(desc_buf, sizeof(desc_buf), "A %s %s material has been left here.", tier_name, crafting_discipline_name(discipline));

  obj->item_number = NOTHING;
  obj->name = strdup(name_buf);
  obj->short_description = strdup(short_buf);
  obj->description = strdup(desc_buf);
  obj->obj_flags.type_flag = ITEM_OTHER;
  SET_BIT_AR(obj->obj_flags.wear_flags, ITEM_WEAR_TAKE);
  SET_BIT_AR(GET_OBJ_EXTRA(obj), ITEM_CRAFT_MATERIAL);
  GET_OBJ_VAL(obj, 0) = discipline;
  GET_OBJ_VAL(obj, 1) = tier;
  GET_OBJ_VAL(obj, 2) = 0;
  GET_OBJ_VAL(obj, 3) = 0;
  obj->obj_flags.weight = 1;
  obj->obj_flags.cost = 10 * tier;

  return obj;
}

static int get_scroll_spells(const struct obj_data *obj, int out[], int max_out)
{
  const char *payload;
  int i, count = 0;

  if (!obj || GET_OBJ_TYPE((struct obj_data *)obj) != ITEM_SCROLL)
    return 0;

  payload = find_exdesc_value(obj, CRAFT_SCROLL_KEY);
  if (payload && *payload)
    return parse_spell_payload(payload, out, max_out);

  for (i = 1; i <= 3 && count < max_out; i++) {
    int spell = GET_OBJ_VAL((struct obj_data *)obj, i);
    if (spell > 0)
      out[count++] = spell;
  }
  return count;
}

static int scroll_payload_matches_payload(const struct obj_data *obj, const int spells[], int count)
{
  int mine[MAX_SCRIBE_SPELLS];
  int i, mine_count;

  mine_count = get_scroll_spells(obj, mine, MAX_SCRIBE_SPELLS);
  if (mine_count != count)
    return FALSE;
  for (i = 0; i < count; i++)
    if (mine[i] != spells[i])
      return FALSE;
  return TRUE;
}

int crafting_scrolls_identical(const struct obj_data *a, const struct obj_data *b)
{
  int sa[MAX_SCRIBE_SPELLS], sb[MAX_SCRIBE_SPELLS];
  int ca, cb, i;

  if (!a || !b || GET_OBJ_TYPE((struct obj_data *)a) != ITEM_SCROLL || GET_OBJ_TYPE((struct obj_data *)b) != ITEM_SCROLL)
    return FALSE;

  ca = get_scroll_spells(a, sa, MAX_SCRIBE_SPELLS);
  cb = get_scroll_spells(b, sb, MAX_SCRIBE_SPELLS);
  if (ca != cb)
    return FALSE;
  for (i = 0; i < ca; i++)
    if (sa[i] != sb[i])
      return FALSE;
  return TRUE;
}

static int get_tome_count(struct obj_data *tome)
{
  const char *raw = find_exdesc_value(tome, CRAFT_TOME_COUNT_KEY);
  int n = raw ? atoi(raw) : 0;
  return MAX(0, n);
}

static void set_tome_count(struct obj_data *tome, int n)
{
  char buf[32];
  snprintf(buf, sizeof(buf), "%d", MAX(0, n));
  set_exdesc_value(tome, CRAFT_TOME_COUNT_KEY, buf);
}

static int get_tome_capacity(struct obj_data *tome)
{
  if (GET_OBJ_VAL(tome, 0) > 0)
    return MIN(1000, GET_OBJ_VAL(tome, 0));
  return 20;
}

static int profession_roll_success(struct char_data *ch, int core, int mastery, int difficulty)
{
  int chance;

  chance = GET_SKILL(ch, core);
  chance += GET_SKILL(ch, mastery) / 2;
  chance -= difficulty;
  chance = MAX(5, MIN(95, chance));

  return rand_number(1, 100) <= chance;
}

static int has_prof_tool(struct char_data *ch, int disc)
{
  struct obj_data *obj;
  for (obj = ch->carrying; obj; obj = obj->next_content)
    if (OBJ_FLAGGED(obj, ITEM_CRAFT_TOOL) && GET_OBJ_VAL(obj, 0) == disc)
      return TRUE;
  return FALSE;
}

static const char *crafting_discipline_name(int disc)
{
  switch (disc) {
  case CRAFT_DISC_SCRIBING:   return "scribing";
  case CRAFT_DISC_ALCHEMY:    return "alchemy";
  case CRAFT_DISC_ENCHANTING: return "enchanting";
  default:                    return "unknown";
  }
}

static const char *crafting_material_tier_name(int tier)
{
  switch (tier) {
  case CRAFT_MAT_TIER_LESSER:   return "lesser";
  case CRAFT_MAT_TIER_GREATER:  return "greater";
  case CRAFT_MAT_TIER_SUPERIOR: return "superior";
  default:                      return "unknown";
  }
}

static int is_matching_material(const struct obj_data *obj, int disc, int min_tier)
{
  if (!obj || !OBJ_FLAGGED((struct obj_data *)obj, ITEM_CRAFT_MATERIAL))
    return FALSE;
  if (GET_OBJ_VAL((struct obj_data *)obj, 0) != disc)
    return FALSE;
  return GET_OBJ_VAL((struct obj_data *)obj, 1) >= min_tier;
}

static int count_prof_materials(struct char_data *ch, int disc, int min_tier)
{
  int count = 0;
  struct obj_data *obj;

  for (obj = ch->carrying; obj; obj = obj->next_content)
    if (is_matching_material(obj, disc, min_tier))
      count++;
  return count;
}

static int consume_prof_materials(struct char_data *ch, int disc, int min_tier, int needed, int preserve_one)
{
  int consumed = 0;
  struct obj_data *obj, *next;

  for (obj = ch->carrying; obj && consumed < needed; obj = next) {
    next = obj->next_content;
    if (!is_matching_material(obj, disc, min_tier))
      continue;
    if (preserve_one && consumed == needed - 1)
      break;
    extract_obj(obj);
    consumed++;
  }

  return consumed;
}

static int get_scroll_difficulty(const int spells[], int count)
{
  int i, difficulty = 15;

  for (i = 0; i < count; i++) {
    int mana = spell_info[spells[i]].mana_max;
    difficulty += 4;
    difficulty += MAX(1, mana / 12);
    if (i > 0)
      difficulty += 8 * i;
  }

  return difficulty;
}

static int can_use_spell_for_scribing(struct char_data *ch, int spell)
{
  if (spell <= 0 || spell > MAX_SPELLS)
    return FALSE;
  return GET_SKILL(ch, spell) > 0;
}

static int can_use_spell_for_brewing(struct char_data *ch, int spell)
{
  if (spell <= 0 || spell > MAX_SPELLS)
    return FALSE;
  if (GET_SKILL(ch, spell) <= 0)
    return FALSE;
  return SPELL_FLAGGED(spell, SPELL_CRAFT_BREWABLE);
}

static int brew_spell_level_for_char(struct char_data *ch, int spell)
{
  int min_level;

  if (!ch || spell <= 0 || spell > MAX_SPELLS)
    return 0;

  min_level = spell_info[spell].min_level[(int)GET_CLASS(ch)];
  if (min_level >= LVL_IMMORT)
    min_level = MAX(1, GET_LEVEL(ch));
  return min_level;
}

static struct obj_data *create_crafted_scroll(struct char_data *ch, const int spells[], int count)
{
  struct obj_data *obj;
  char payload[MAX_CRAFT_PAYLOAD];

  obj = create_obj();
  obj->item_number = NOTHING;
  obj->name = strdup("crafted scroll scroll parchment");
  obj->short_description = strdup("a crafted spellscroll");
  obj->description = strdup("A crafted spellscroll lies here.");
  obj->obj_flags.type_flag = ITEM_SCROLL;
  SET_BIT_AR(obj->obj_flags.wear_flags, ITEM_WEAR_TAKE);
  GET_OBJ_VAL(obj, 0) = MAX(1, GET_LEVEL(ch));
  GET_OBJ_VAL(obj, 1) = (count > 0) ? spells[0] : -1;
  GET_OBJ_VAL(obj, 2) = (count > 1) ? spells[1] : -1;
  GET_OBJ_VAL(obj, 3) = (count > 2) ? spells[2] : -1;
  obj->obj_flags.weight = 1;
  obj->obj_flags.cost = 10 * count;

  build_spell_payload(spells, count, payload, sizeof(payload));
  set_exdesc_value(obj, CRAFT_SCROLL_KEY, payload);
  return obj;
}

int crafting_get_potion_stack(const struct obj_data *obj)
{
  const char *raw;
  int n;

  if (!obj || GET_OBJ_TYPE((struct obj_data *)obj) != ITEM_POTION)
    return 1;

  raw = find_exdesc_value(obj, CRAFT_POTION_STACK_KEY);
  n = raw ? atoi(raw) : 1;
  return MAX(1, MIN(MAX_POTION_STACK, n));
}

void crafting_set_potion_stack(struct obj_data *obj, int count)
{
  char buf[32];
  if (!obj || GET_OBJ_TYPE(obj) != ITEM_POTION)
    return;
  snprintf(buf, sizeof(buf), "%d", MAX(1, MIN(MAX_POTION_STACK, count)));
  set_exdesc_value(obj, CRAFT_POTION_STACK_KEY, buf);
}

int crafting_try_merge_potion_stack(struct obj_data *dest, struct obj_data *src)
{
  int dcount, scount, total;

  if (!dest || !src || dest == src)
    return FALSE;
  if (GET_OBJ_TYPE(dest) != ITEM_POTION || GET_OBJ_TYPE(src) != ITEM_POTION)
    return FALSE;
  if (GET_OBJ_VAL(dest, 0) != GET_OBJ_VAL(src, 0) ||
      GET_OBJ_VAL(dest, 1) != GET_OBJ_VAL(src, 1) ||
      GET_OBJ_VAL(dest, 2) != GET_OBJ_VAL(src, 2) ||
      GET_OBJ_VAL(dest, 3) != GET_OBJ_VAL(src, 3))
    return FALSE;

  dcount = crafting_get_potion_stack(dest);
  scount = crafting_get_potion_stack(src);
  if (dcount >= MAX_POTION_STACK)
    return FALSE;

  total = dcount + scount;
  if (total <= MAX_POTION_STACK) {
    crafting_set_potion_stack(dest, total);
    extract_obj(src);
  } else {
    crafting_set_potion_stack(dest, MAX_POTION_STACK);
    crafting_set_potion_stack(src, total - MAX_POTION_STACK);
  }

  return TRUE;
}

int crafting_can_teach_ability(struct char_data *trainer, int ability_id)
{
  int trainer_flag = 0;
  struct char_data *cand;

  if (!trainer || !IS_NPC(trainer))
    return TRUE;

  switch (ability_id) {
  case SKILL_SCRIBING:
  case SKILL_SCRIBING_MASTERY:
    trainer_flag = MOB_TEACH_SCRIBING;
    break;
  case SKILL_ALCHEMY:
  case SKILL_ALCHEMY_MASTERY:
    trainer_flag = MOB_TEACH_ALCHEMY;
    break;
  case SKILL_ENCHANTING:
  case SKILL_ENCHANTING_MASTERY:
    trainer_flag = MOB_TEACH_ENCHANTING;
    break;
  default:
    return TRUE;
  }

  if (MOB_FLAGGED(trainer, trainer_flag))
    return TRUE;

  for (cand = world[IN_ROOM(trainer)].people; cand; cand = cand->next_in_room) {
    if (!IS_NPC(cand))
      continue;
    if (MOB_FLAGGED(cand, MOB_NOTDEADYET))
      continue;
    if (MOB_FLAGGED(cand, trainer_flag))
      return TRUE;
  }

  return FALSE;
}

int crafting_handle_tome_put(struct char_data *ch, struct obj_data *obj, struct obj_data *cont)
{
  const char *tome_payload;
  char payload[MAX_CRAFT_PAYLOAD];
  int spells[MAX_SCRIBE_SPELLS];
  int count, cur, cap;

  if (!ch || !obj || !cont)
    return FALSE;
  if (!OBJ_FLAGGED(cont, ITEM_SPELLTOME))
    return FALSE;

  if (GET_OBJ_TYPE(obj) != ITEM_SCROLL) {
    send_to_char(ch, "That tome only accepts scrolls.\r\n");
    return TRUE;
  }

  count = get_scroll_spells(obj, spells, MAX_SCRIBE_SPELLS);
  if (count <= 0) {
    send_to_char(ch, "That scroll has no usable spell payload.\r\n");
    return TRUE;
  }

  tome_payload = find_exdesc_value(cont, CRAFT_TOME_KEY);
  if (tome_payload && *tome_payload && !scroll_payload_matches_payload(obj, spells, count)) {
    send_to_char(ch, "That scroll doesn't match the tome's stored sequence.\r\n");
    return TRUE;
  }

  cur = get_tome_count(cont);
  cap = get_tome_capacity(cont);
  if (cur >= cap) {
    send_to_char(ch, "The tome is full.\r\n");
    return TRUE;
  }

  if (!(tome_payload && *tome_payload)) {
    build_spell_payload(spells, count, payload, sizeof(payload));
    set_exdesc_value(cont, CRAFT_TOME_KEY, payload);
  }

  set_tome_count(cont, cur + 1);
  act("You feed $p into $P.", FALSE, ch, obj, cont, TO_CHAR);
  act("$n feeds $p into $P.", TRUE, ch, obj, cont, TO_ROOM);
  extract_obj(obj);
  return TRUE;
}

static void craft_fail_consume(struct char_data *ch, int disc, int needed, int severity)
{
  int saved = FALSE;

  if (severity >= 80 && GET_SKILL(ch, SKILL_STEADY_MIND) > 0) {
    int avoid = MIN(50, GET_SKILL(ch, SKILL_STEADY_MIND) / 2);
    if (rand_number(1, 100) <= avoid)
      severity = 50;
  }

  if (GET_SKILL(ch, SKILL_CAREFUL_HANDS) > 0) {
    int save_chance = MIN(45, GET_SKILL(ch, SKILL_CAREFUL_HANDS) / 2);
    if (rand_number(1, 100) <= save_chance)
      saved = TRUE;
  }

  if (!saved)
    consume_prof_materials(ch, disc, CRAFT_MAT_TIER_LESSER, needed, FALSE);
}

ACMD(do_scribe)
{
  char mode[MAX_INPUT_LENGTH], work[MAX_INPUT_LENGTH], token[MAX_INPUT_LENGTH];
  char *scan;
  int spells[MAX_SCRIBE_SPELLS], count = 0, resolved, difficulty;
  struct obj_data *made;

  two_arguments(argument, mode, work);

  if (!*mode || !str_cmp(mode, "list")) {
    send_to_char(ch, "Scribing guide:\r\n");
    send_to_char(ch, "  Required tool: scribing tool\r\n");
    send_to_char(ch, "  Required materials: 1 scribing material per spell\r\n");
    send_to_char(ch, "  Max spells per scroll: %d\r\n", MAX_SCRIBE_SPELLS);
    send_to_char(ch, "\r\n");
    send_to_char(ch, "Scribing usage:\r\n");
    send_to_char(ch, "  scribe list\r\n");
    send_to_char(ch, "  scribe create <spell1> [spell2] [spell3] [spell4]\r\n");
    send_to_char(ch, "\r\n");
    send_to_char(ch, "Examples:\r\n");
    send_to_char(ch, "  scribe create weaken\r\n");
    send_to_char(ch, "  scribe create blind weaken\r\n");
    send_to_char(ch, "  scribe create fireball fireball fireball weaken\r\n");
    return;
  }

  if (str_cmp(mode, "create")) {
    send_to_char(ch, "Use 'scribe list' or 'scribe create ...'.\r\n");
    return;
  }

  if (GET_SKILL(ch, SKILL_SCRIBING) <= 0) {
    send_to_char(ch, "You have no training in scribing.\r\n");
    return;
  }

  if (!has_prof_tool(ch, CRAFT_DISC_SCRIBING)) {
    send_to_char(ch, "You need a scribing tool first.\r\n");
    return;
  }

  scan = work;
  while (*scan && count < MAX_SCRIBE_SPELLS) {
    scan = one_argument(scan, token);
    if (!*token)
      break;
    resolved = resolve_spell_by_player_input(ch, token, TRUE, FALSE, TRUE, NULL, NULL, 0);
    if (resolved <= 0 || resolved > MAX_SPELLS) {
      send_to_char(ch, "'%s' is not a valid player spell.\r\n", token);
      return;
    }
    if (!can_use_spell_for_scribing(ch, resolved)) {
      send_to_char(ch, "You cannot inscribe %s.\r\n", skill_name(resolved));
      return;
    }
    spells[count++] = resolved;
  }

  while (*scan && isspace((unsigned char)*scan))
    scan++;
  if (*scan) {
    send_to_char(ch, "You may only inscribe up to %d spells.\r\n", MAX_SCRIBE_SPELLS);
    return;
  }

  if (count <= 0) {
    send_to_char(ch, "You must supply at least one spell.\r\n");
    return;
  }

  if (count_prof_materials(ch, CRAFT_DISC_SCRIBING, CRAFT_MAT_TIER_LESSER) < count) {
    send_to_char(ch, "You lack enough scribing materials (%d lesser-tier or better needed).\r\n", count);
    return;
  }

  difficulty = get_scroll_difficulty(spells, count);
  if (!profession_roll_success(ch, SKILL_SCRIBING, SKILL_SCRIBING_MASTERY, difficulty)) {
    craft_fail_consume(ch, CRAFT_DISC_SCRIBING, count, rand_number(1, 100));
    send_to_char(ch, "The scroll chars and burns to ash.\r\n");
    return;
  }

  consume_prof_materials(ch, CRAFT_DISC_SCRIBING, CRAFT_MAT_TIER_LESSER, count, (GET_SKILL(ch, SKILL_EFFICIENT_CRAFTING) > 0 && rand_number(1, 100) <= MIN(35, GET_SKILL(ch, SKILL_EFFICIENT_CRAFTING) / 2)));

  made = create_crafted_scroll(ch, spells, count);
  obj_to_char(made, ch);
  act("You finish inscribing $p.", FALSE, ch, made, 0, TO_CHAR);
  act("$n finishes inscribing $p.", TRUE, ch, made, 0, TO_ROOM);
}

ACMD(do_brew)
{
  char mode[MAX_INPUT_LENGTH];
  char spell_arg[MAX_INPUT_LENGTH];
  char *rest;
  int spellnum;
  int i, found_any = FALSE;
  int difficulty;
  struct obj_data *p;

  rest = one_argument(argument, mode);
  while (*rest && isspace((unsigned char)*rest))
    rest++;

  if (!*mode || !str_cmp(mode, "list")) {
    send_to_char(ch, "Brew guide:\r\n");
    send_to_char(ch, "  Required tool: alchemy tool\r\n");
    send_to_char(ch, "  Required materials: 1 alchemy material per potion\r\n");
    send_to_char(ch, "\r\n");
    send_to_char(ch, "Brew usage:\r\n");
    send_to_char(ch, "  brew list\r\n");
    send_to_char(ch, "  brew create <spellname>\r\n");
    send_to_char(ch, "\r\n");
    send_to_char(ch, "Known brewable spells:\r\n");

    for (i = 1; i <= MAX_SPELLS; i++) {
      if (!can_use_spell_for_brewing(ch, i))
        continue;
      send_to_char(ch, "  %-26s (lvl %d)\r\n", skill_name(i), brew_spell_level_for_char(ch, i));
      found_any = TRUE;
    }

    if (!found_any)
      send_to_char(ch, "  You do not know any spells that can be brewed.\r\n");
    return;
  }

  if (str_cmp(mode, "create")) {
    send_to_char(ch, "Use 'brew list' or 'brew create <spellname>'.\r\n");
    return;
  }

  strlcpy(spell_arg, rest, sizeof(spell_arg));
  if (!*spell_arg) {
    send_to_char(ch, "Brew which spell?\r\n");
    return;
  }

  if (GET_SKILL(ch, SKILL_ALCHEMY) <= 0) {
    send_to_char(ch, "You have no training in alchemy.\r\n");
    return;
  }
  if (!has_prof_tool(ch, CRAFT_DISC_ALCHEMY)) {
    send_to_char(ch, "You need an alchemy tool first.\r\n");
    return;
  }

  spellnum = resolve_spell_by_player_input(ch, spell_arg, TRUE, FALSE, TRUE, NULL, NULL, 0);
  if (spellnum <= 0 || spellnum > MAX_SPELLS) {
    send_to_char(ch, "'%s' is not a valid player spell.\r\n", spell_arg);
    return;
  }
  if (GET_SKILL(ch, spellnum) <= 0) {
    send_to_char(ch, "You do not know %s.\r\n", skill_name(spellnum));
    return;
  }
  if (!SPELL_FLAGGED(spellnum, SPELL_CRAFT_BREWABLE)) {
    send_to_char(ch, "You cannot brew %s.\r\n", skill_name(spellnum));
    return;
  }
  if (count_prof_materials(ch, CRAFT_DISC_ALCHEMY, CRAFT_MAT_TIER_LESSER) < 1) {
    send_to_char(ch, "You need alchemical materials (lesser-tier or better).\r\n");
    return;
  }

  difficulty = 20 + MAX(1, spell_info[spellnum].mana_max / 12);
  /* Hook: apply future alchemy mastery scaling adjustments to difficulty here. */
  if (!profession_roll_success(ch, SKILL_ALCHEMY, SKILL_ALCHEMY_MASTERY, difficulty)) {
    craft_fail_consume(ch, CRAFT_DISC_ALCHEMY, 1, rand_number(1, 100));
    send_to_char(ch, "The mixture erupts and is ruined.\r\n");
    return;
  }

  consume_prof_materials(ch, CRAFT_DISC_ALCHEMY, CRAFT_MAT_TIER_LESSER, 1, (GET_SKILL(ch, SKILL_EFFICIENT_CRAFTING) > 0 && rand_number(1, 100) <= MIN(35, GET_SKILL(ch, SKILL_EFFICIENT_CRAFTING) / 2)));

  p = create_obj();
  p->item_number = NOTHING;
  p->name = strdup("crafted potion vial");
  p->short_description = strdup("a brewed potion");
  p->description = strdup("A brewed potion has been left here.");
  p->obj_flags.type_flag = ITEM_POTION;
  SET_BIT_AR(p->obj_flags.wear_flags, ITEM_WEAR_TAKE);
  GET_OBJ_VAL(p, 0) = MAX(1, GET_LEVEL(ch));
  GET_OBJ_VAL(p, 1) = spellnum;
  GET_OBJ_VAL(p, 2) = -1;
  GET_OBJ_VAL(p, 3) = -1;
  crafting_set_potion_stack(p, 1);
  obj_to_char(p, ch);
  act("You brew $p.", FALSE, ch, p, 0, TO_CHAR);
  act("$n brews $p.", TRUE, ch, p, 0, TO_ROOM);
}

ACMD(do_enchant)
{
  char recipe[MAX_INPUT_LENGTH], item_name[MAX_INPUT_LENGTH];
  int found = -1;
  int difficulty;
  int existing_count, same_recipe_count, scaled_modifier, affect_slot;
  int attempt_number;
  struct obj_data *obj;

  two_arguments(argument, recipe, item_name);
  if (!*recipe || !str_cmp(recipe, "list")) {
    send_to_char(ch, "Enchanting guide:\r\n");
    send_to_char(ch, "  Recipes unlock by enchanting skill (1-100), not by known spells.\r\n");
    send_to_char(ch, "  Materials are obtained via: disenchant <item>\r\n");
    send_to_char(ch, "  Material tiers: lesser, greater, superior\r\n");
    send_to_char(ch, "  Max enchant applications per item: %d\r\n", MAX_ITEM_ENCHANTS);
    send_to_char(ch, "  Higher enchant counts are riskier; failed 4th attempts may destroy the item.\r\n");
    send_to_char(ch, "\r\n");
    send_to_char(ch, "Usage:\r\n");
    send_to_char(ch, "  enchant list\r\n");
    send_to_char(ch, "  enchant <recipe> <item>\r\n");
    send_to_char(ch, "\r\n");
    send_to_char(ch, "Unlocked recipes:\r\n");
    for (found = 0; enchant_recipes[found].name; found++) {
      if (GET_SKILL(ch, SKILL_ENCHANTING) < enchant_recipes[found].min_level)
        continue;
      send_to_char(ch, "  %-12s lvl %-3d mat:%-8s\r\n",
                   enchant_recipes[found].name,
                   enchant_recipes[found].min_level,
                   crafting_material_tier_name(enchant_recipes[found].required_tier));
    }
    return;
  }

  if (!*item_name) {
    send_to_char(ch, "Enchant what item?\r\n");
    return;
  }

  if (GET_SKILL(ch, SKILL_ENCHANTING) <= 0) {
    send_to_char(ch, "You have no training in enchanting.\r\n");
    return;
  }

  if (!has_prof_tool(ch, CRAFT_DISC_ENCHANTING)) {
    send_to_char(ch, "You need an enchanting tool first.\r\n");
    return;
  }

  found = enchant_recipe_index_by_name(recipe);
  if (found < 0) {
    send_to_char(ch, "Unknown enchant recipe. Use 'enchant list'.\r\n");
    return;
  }
  if (GET_SKILL(ch, SKILL_ENCHANTING) < enchant_recipes[found].min_level) {
    send_to_char(ch, "You need enchanting skill %d for %s.\r\n",
                 enchant_recipes[found].min_level, enchant_recipes[found].name);
    return;
  }

  if (!(obj = get_obj_in_list_vis(ch, item_name, NULL, ch->carrying))) {
    send_to_char(ch, "You don't seem to have %s %s.\r\n", AN(item_name), item_name);
    return;
  }

  if (!item_is_valid_for_enchant(ch, obj)) {
    send_to_char(ch, "You can only enchant equippable gear.\r\n");
    return;
  }
  if (!recipe_is_compatible_with_item(&enchant_recipes[found], obj)) {
    send_to_char(ch, "That recipe is not compatible with this item type.\r\n");
    return;
  }

  existing_count = crafting_get_enchant_count(obj);
  if (existing_count >= MAX_ITEM_ENCHANTS) {
    send_to_char(ch, "That item cannot hold any more enchantments.\r\n");
    return;
  }

  affect_slot = find_free_enchant_affect_slot(obj);
  if (affect_slot < 0) {
    send_to_char(ch, "That item has no remaining space for another enchant.\r\n");
    return;
  }

  same_recipe_count = crafting_get_enchant_recipe_count(obj, enchant_recipes[found].name);
  scaled_modifier = scaled_enchant_modifier(enchant_recipes[found].base_modifier, same_recipe_count);
  if (scaled_modifier == 0) {
    send_to_char(ch, "That enchant cannot be applied safely right now.\r\n");
    return;
  }

  if (count_prof_materials(ch, CRAFT_DISC_ENCHANTING, enchant_recipes[found].required_tier) < 1) {
    send_to_char(ch, "You need %s enchanting materials for that recipe.\r\n",
                 crafting_material_tier_name(enchant_recipes[found].required_tier));
    return;
  }

  attempt_number = existing_count + 1;
  difficulty = 20 + enchant_recipes[found].min_level + enchant_attempt_penalty[existing_count];
  if (!profession_roll_success(ch, SKILL_ENCHANTING, SKILL_ENCHANTING_MASTERY, difficulty)) {
    craft_fail_consume(ch, CRAFT_DISC_ENCHANTING, 1, rand_number(1, 100));
    if (attempt_number == MAX_ITEM_ENCHANTS && rand_number(1, 100) <= fourth_enchant_fail_destroy_pct) {
      act("@rArcane energies detonate around $p, reducing it to ash!@n", FALSE, ch, obj, 0, TO_CHAR);
      act("@rArcane energies detonate around $p, reducing it to ash!@n", TRUE, ch, obj, 0, TO_ROOM);
      extract_obj(obj);
      return;
    }
    send_to_char(ch, "The enchantment collapses.\r\n");
    return;
  }

  consume_prof_materials(ch, CRAFT_DISC_ENCHANTING, enchant_recipes[found].required_tier, 1, (GET_SKILL(ch, SKILL_EFFICIENT_CRAFTING) > 0 && rand_number(1, 100) <= MIN(35, GET_SKILL(ch, SKILL_EFFICIENT_CRAFTING) / 2)));

  obj->affected[affect_slot].location = enchant_recipes[found].apply_loc;
  obj->affected[affect_slot].modifier = scaled_modifier;
  append_item_enchant_history(obj, found);
  SET_BIT_AR(GET_OBJ_EXTRA(obj), ITEM_MAGIC);
  act("You complete the enchantment on $p.", FALSE, ch, obj, 0, TO_CHAR);
  act("$n completes an enchantment on $p.", TRUE, ch, obj, 0, TO_ROOM);
}

ACMD(do_disenchant)
{
  char item_name[MAX_INPUT_LENGTH];
  struct obj_data *obj;
  struct obj_data *material;
  int score, tier;

  one_argument(argument, item_name);
  if (!*item_name) {
    send_to_char(ch, "Usage: disenchant <item>\r\n");
    send_to_char(ch, "Disenchant destroys magical/effect-bearing items and yields enchanting materials.\r\n");
    return;
  }

  if (GET_SKILL(ch, SKILL_ENCHANTING) <= 0) {
    send_to_char(ch, "You have no training in enchanting.\r\n");
    return;
  }

  if (!(obj = get_obj_in_list_vis(ch, item_name, NULL, ch->carrying))) {
    send_to_char(ch, "You don't seem to have %s %s.\r\n", AN(item_name), item_name);
    return;
  }

  score = item_disenchant_effect_score(obj);
  if (score <= 0) {
    send_to_char(ch, "That item has no meaningful magical essence to extract.\r\n");
    return;
  }

  tier = disenchant_tier_from_score(score);
  material = create_craft_material_item(CRAFT_DISC_ENCHANTING, tier);
  if (!material) {
    send_to_char(ch, "Disenchanting fails unexpectedly.\r\n");
    return;
  }

  act("You unravel $p into raw arcane essence.", FALSE, ch, obj, 0, TO_CHAR);
  act("$n unravels $p into raw arcane essence.", TRUE, ch, obj, 0, TO_ROOM);
  extract_obj(obj);
  obj_to_char(material, ch);
  send_to_char(ch, "You extract %s enchanting material.\r\n", crafting_material_tier_name(tier));
}

int crafting_try_recite_tome(struct char_data *ch, char *argument)
{
  char arg1[MAX_INPUT_LENGTH], arg2[MAX_INPUT_LENGTH];
  struct obj_data *tome;
  int spells[MAX_SCRIBE_SPELLS];
  const char *payload;
  int count, i, remaining;

  two_arguments(argument, arg1, arg2);
  if (str_cmp(arg1, "tome"))
    return FALSE;

  if (!*arg2) {
    send_to_char(ch, "Recite tome <tome>.\r\n");
    return TRUE;
  }

  tome = get_obj_in_list_vis(ch, arg2, NULL, ch->carrying);
  if (!tome)
    tome = get_obj_in_list_vis(ch, arg2, NULL, world[IN_ROOM(ch)].contents);
  if (!tome || !OBJ_FLAGGED(tome, ITEM_SPELLTOME)) {
    send_to_char(ch, "That is not a spelltome.\r\n");
    return TRUE;
  }

  remaining = get_tome_count(tome);
  payload = find_exdesc_value(tome, CRAFT_TOME_KEY);
  count = parse_spell_payload(payload, spells, MAX_SCRIBE_SPELLS);
  if (remaining <= 0 || count <= 0) {
    send_to_char(ch, "The tome is empty.\r\n");
    return TRUE;
  }

  WAIT_STATE(ch, PULSE_VIOLENCE);
  act("You recite from $p.", FALSE, ch, tome, 0, TO_CHAR);
  act("$n recites from $p.", TRUE, ch, tome, 0, TO_ROOM);
  for (i = 0; i < count; i++)
    call_magic(ch, ch, NULL, spells[i], GET_LEVEL(ch), CAST_SCROLL);

  set_tome_count(tome, remaining - 1);
  return TRUE;
}
