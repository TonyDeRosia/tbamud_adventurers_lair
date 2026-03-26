#include "conf.h"
#include "sysdep.h"

#include "crafting.h"
#include "structs.h"
#include "utils.h"
#include "comm.h"
#include "handler.h"
#include "interpreter.h"
#include "spells.h"
#include "db.h"

#define CRAFT_SCROLL_KEY "__craft_scroll_spells"
#define CRAFT_TOME_KEY "__craft_tome_spells"
#define CRAFT_TOME_COUNT_KEY "__craft_tome_count"
#define CRAFT_POTION_STACK_KEY "__craft_potion_stack"
#define MAX_SCRIBE_SPELLS 4
#define MAX_CRAFT_PAYLOAD 256
#define MAX_POTION_STACK 100

struct enchant_recipe {
  const char *name;
  int apply_loc;
  int modifier;
  int min_level;
};

static const struct enchant_recipe enchant_recipes[] = {
  {"sturdy", APPLY_AC, -5, 1},
  {"accurate", APPLY_HITROLL, 1, 10},
  {"keen", APPLY_DAMROLL, 1, 15},
  {NULL, 0, 0, 0}
};

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

static struct obj_data *find_prof_material(struct char_data *ch, int disc)
{
  struct obj_data *obj;
  for (obj = ch->carrying; obj; obj = obj->next_content)
    if (OBJ_FLAGGED(obj, ITEM_CRAFT_MATERIAL) && GET_OBJ_VAL(obj, 0) == disc)
      return obj;
  return NULL;
}

static int consume_prof_materials(struct char_data *ch, int disc, int needed, int preserve_one)
{
  int consumed = 0;
  struct obj_data *obj, *next;

  for (obj = ch->carrying; obj && consumed < needed; obj = next) {
    next = obj->next_content;
    if (!OBJ_FLAGGED(obj, ITEM_CRAFT_MATERIAL) || GET_OBJ_VAL(obj, 0) != disc)
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
    consume_prof_materials(ch, disc, needed, FALSE);
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

  {
    int have = 0;
    struct obj_data *obj;
    for (obj = ch->carrying; obj; obj = obj->next_content)
      if (OBJ_FLAGGED(obj, ITEM_CRAFT_MATERIAL) && GET_OBJ_VAL(obj, 0) == CRAFT_DISC_SCRIBING)
        have++;
    if (have < count) {
      send_to_char(ch, "You lack enough scribing materials (%d needed).\r\n", count);
      return;
    }
  }

  difficulty = get_scroll_difficulty(spells, count);
  if (!profession_roll_success(ch, SKILL_SCRIBING, SKILL_SCRIBING_MASTERY, difficulty)) {
    craft_fail_consume(ch, CRAFT_DISC_SCRIBING, count, rand_number(1, 100));
    send_to_char(ch, "The scroll chars and burns to ash.\r\n");
    return;
  }

  consume_prof_materials(ch, CRAFT_DISC_SCRIBING, count, (GET_SKILL(ch, SKILL_EFFICIENT_CRAFTING) > 0 && rand_number(1, 100) <= MIN(35, GET_SKILL(ch, SKILL_EFFICIENT_CRAFTING) / 2)));

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
  if (!find_prof_material(ch, CRAFT_DISC_ALCHEMY)) {
    send_to_char(ch, "You need alchemical materials.\r\n");
    return;
  }

  difficulty = 20 + MAX(1, spell_info[spellnum].mana_max / 12);
  /* Hook: apply future alchemy mastery scaling adjustments to difficulty here. */
  if (!profession_roll_success(ch, SKILL_ALCHEMY, SKILL_ALCHEMY_MASTERY, difficulty)) {
    craft_fail_consume(ch, CRAFT_DISC_ALCHEMY, 1, rand_number(1, 100));
    send_to_char(ch, "The mixture erupts and is ruined.\r\n");
    return;
  }

  consume_prof_materials(ch, CRAFT_DISC_ALCHEMY, 1, (GET_SKILL(ch, SKILL_EFFICIENT_CRAFTING) > 0 && rand_number(1, 100) <= MIN(35, GET_SKILL(ch, SKILL_EFFICIENT_CRAFTING) / 2)));

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
  int i, found = -1;
  int difficulty;
  struct obj_data *obj;

  two_arguments(argument, recipe, item_name);
  if (!*recipe || !str_cmp(recipe, "list")) {
    send_to_char(ch, "Available enchant recipes:\r\n");
    for (i = 0; enchant_recipes[i].name; i++)
      send_to_char(ch, "  %-10s (lvl %d)\r\n", enchant_recipes[i].name, enchant_recipes[i].min_level);
    send_to_char(ch, "Usage: enchant <recipe> <item>\r\n");
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

  for (i = 0; enchant_recipes[i].name; i++)
    if (!str_cmp(recipe, enchant_recipes[i].name))
      found = i;
  if (found < 0) {
    send_to_char(ch, "Unknown enchant recipe. Use 'enchant list'.\r\n");
    return;
  }

  if (!(obj = get_obj_in_list_vis(ch, item_name, NULL, ch->carrying))) {
    send_to_char(ch, "You don't seem to have %s %s.\r\n", AN(item_name), item_name);
    return;
  }

  if (!find_prof_material(ch, CRAFT_DISC_ENCHANTING)) {
    send_to_char(ch, "You need enchanting materials.\r\n");
    return;
  }

  difficulty = 20 + enchant_recipes[found].min_level;
  if (!profession_roll_success(ch, SKILL_ENCHANTING, SKILL_ENCHANTING_MASTERY, difficulty)) {
    craft_fail_consume(ch, CRAFT_DISC_ENCHANTING, 1, rand_number(1, 100));
    send_to_char(ch, "The enchantment collapses.\r\n");
    return;
  }

  consume_prof_materials(ch, CRAFT_DISC_ENCHANTING, 1, (GET_SKILL(ch, SKILL_EFFICIENT_CRAFTING) > 0 && rand_number(1, 100) <= MIN(35, GET_SKILL(ch, SKILL_EFFICIENT_CRAFTING) / 2)));

  obj->affected[0].location = enchant_recipes[found].apply_loc;
  obj->affected[0].modifier += enchant_recipes[found].modifier;
  obj->obj_flags.bitvector[0] = obj->obj_flags.bitvector[0];
  SET_BIT_AR(GET_OBJ_EXTRA(obj), ITEM_MAGIC);
  act("You complete the enchantment on $p.", FALSE, ch, obj, 0, TO_CHAR);
  act("$n completes an enchantment on $p.", TRUE, ch, obj, 0, TO_ROOM);
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
