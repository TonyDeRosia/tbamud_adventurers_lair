#include "conf.h"
#include "sysdep.h"

#include "classtrack.h"

#include "structs.h"
#include "utils.h"
#include "spells.h"
#include "comm.h"
#include "db.h"
#include "screen.h"

#define CT_SOFT_MIN_SCORE 10
#define CT_SOFT_TOTAL_SCORE_MIN 24
#define CT_SOFT_WEAK_DOMINANCE_MARGIN 8
#define CT_SOFT_DOMINANCE_MARGIN 4
#define CT_FINAL_MIN_SCORE 80
#define CT_FINAL_MARGIN 12
#define CT_FINAL_HYBRID_MARGIN 10
#define CT_BROAD_ACTIVE_SCORE 8
#define CT_COMMIT_LEVEL 50
#define CT_COMMIT_PRIMARY 120
#define CT_COMMIT_MARGIN 30

struct ability_archetype_map {
  int ability_id;
  int archetype;
};

static const struct ability_archetype_map ct_spell_archetype_map[] = {
  { SPELL_ARMOR, ARCHETYPE_DIVINE },
  { SPELL_BLESS, ARCHETYPE_DIVINE },
  { SPELL_CALL_LIGHTNING, ARCHETYPE_NATURE },
  { SPELL_CURE_CRITIC, ARCHETYPE_DIVINE },
  { SPELL_CURE_LIGHT, ARCHETYPE_DIVINE },
  { SPELL_DISPEL_EVIL, ARCHETYPE_DIVINE },
  { SPELL_EARTHQUAKE, ARCHETYPE_NATURE },
  { SPELL_ENERGY_DRAIN, ARCHETYPE_DARK },
  { SPELL_FIREBALL, ARCHETYPE_ARCANE },
  { SPELL_HARM, ARCHETYPE_DARK },
  { SPELL_HEAL, ARCHETYPE_DIVINE },
  { SPELL_LIGHTNING_BOLT, ARCHETYPE_ARCANE },
  { SPELL_MAGIC_MISSILE, ARCHETYPE_ARCANE },
  { SPELL_POISON, ARCHETYPE_DARK },
  { SPELL_SANCTUARY, ARCHETYPE_DIVINE },
  { SPELL_STRENGTH, ARCHETYPE_COMBAT },
  { SPELL_WORD_OF_RECALL, ARCHETYPE_DIVINE },
  { SPELL_ANIMATE_DEAD, ARCHETYPE_DARK },
  { SPELL_DARKNESS, ARCHETYPE_DARK },
  { SPELL_BEAR_SPIRIT, ARCHETYPE_NATURE },
  { SPELL_WOLF_SPIRIT, ARCHETYPE_NATURE },
  { SPELL_ARCANE_WARD, ARCHETYPE_ARCANE },
  { SPELL_IRONSKIN, ARCHETYPE_COMBAT },
  { SPELL_DIVINE_BULWARK, ARCHETYPE_DIVINE },
  { SPELL_DARK_AEGIS, ARCHETYPE_DARK },
  { SPELL_PLAGUE_BOLT, ARCHETYPE_DARK },
  { SPELL_DEVOUR_SOUL, ARCHETYPE_DARK },
  { SPELL_FIREBOLT, ARCHETYPE_ARCANE },
  { SPELL_FLAME_ARROW, ARCHETYPE_ARCANE },
  { SPELL_FROSTBITE, ARCHETYPE_ARCANE },
  { SPELL_VOLTAIC_BOLT, ARCHETYPE_ARCANE },
  { SPELL_ACID_BLAST, ARCHETYPE_ARCANE },
  { SPELL_SHADOW_BOLT, ARCHETYPE_DARK },
  { SPELL_VAMPIRIC_TOUCH, ARCHETYPE_DARK },
  { SPELL_STONE_SKIN, ARCHETYPE_COMBAT },
  { SPELL_BARKSKIN, ARCHETYPE_NATURE },
  { SPELL_GIANT_STRENGTH, ARCHETYPE_COMBAT },
  { SPELL_GREATER_HEAL, ARCHETYPE_DIVINE },
  { SPELL_CLEANSE, ARCHETYPE_DIVINE },
  { SPELL_CONSECRATE, ARCHETYPE_DIVINE },
  { SPELL_ICE_STORM, ARCHETYPE_ARCANE },
  { SPELL_BLIZZARD, ARCHETYPE_ARCANE },
  { SPELL_FIREBALL_GREATER, ARCHETYPE_ARCANE },
  { SPELL_CALL_WOLVES, ARCHETYPE_NATURE },
  { SPELL_CALL_BEARS, ARCHETYPE_NATURE },
  { SPELL_ANIMATE_DEAD_GREATER, ARCHETYPE_DARK },
  { SPELL_METEOR, ARCHETYPE_ARCANE },
  { SPELL_METEOR_SWARM, ARCHETYPE_ARCANE },
  { SPELL_HELLFIRE, ARCHETYPE_DARK },
  { SPELL_CELESTIAL_SMITE, ARCHETYPE_DIVINE },
  { SPELL_HAMMER_OF_GOD, ARCHETYPE_DIVINE },
  { SPELL_DEATH_KNELL, ARCHETYPE_DARK },
  { SPELL_UNHOLY_WORD, ARCHETYPE_DARK },
  { SPELL_HOLY_WORD, ARCHETYPE_DIVINE },
  { SPELL_FINGER_OF_DEATH, ARCHETYPE_DARK },
  { SPELL_WAIL_OF_THE_BANSHEE, ARCHETYPE_DARK },
  { SPELL_BLACK_LANCE, ARCHETYPE_DARK },
  { SPELL_NEGATIVE_BURST, ARCHETYPE_DARK },
  { SPELL_CALL_SHADOW_LEGION, ARCHETYPE_DARK },
  { SPELL_SHADOW_REGENESIS, ARCHETYPE_DARK },
  { 0, -1 }
};

static const struct ability_archetype_map ct_skill_archetype_map[] = {
  { SKILL_BACKSTAB, ARCHETYPE_ROGUE },
  { SKILL_BASH, ARCHETYPE_COMBAT },
  { SKILL_HIDE, ARCHETYPE_ROGUE },
  { SKILL_KICK, ARCHETYPE_COMBAT },
  { SKILL_PICK_LOCK, ARCHETYPE_ROGUE },
  { SKILL_WHIRLWIND, ARCHETYPE_COMBAT },
  { SKILL_RESCUE, ARCHETYPE_COMBAT },
  { SKILL_SNEAK, ARCHETYPE_ROGUE },
  { SKILL_STEAL, ARCHETYPE_ROGUE },
  { SKILL_TRACK, ARCHETYPE_NATURE },
  { SKILL_BANDAGE, ARCHETYPE_DIVINE },
  { SKILL_DUAL_WIELD, ARCHETYPE_COMBAT },
  { SKILL_RECALL, ARCHETYPE_DIVINE },
  { SKILL_UNDEAD_COMMAND, ARCHETYPE_DARK },
  { SKILL_SHADOW_COMMANDER, ARCHETYPE_DARK },
  { SKILL_PREDATORS_ADVANCE, ARCHETYPE_NATURE },
  { SKILL_RELENTLESS_HUNT, ARCHETYPE_NATURE },
  { SKILL_CHAIN_ASSASSAULT, ARCHETYPE_ROGUE },
  { SKILL_KILL_WINDOW, ARCHETYPE_ROGUE },
  { SKILL_APPRAISE_ENEMY, ARCHETYPE_ROGUE },
  { SKILL_STUDY, ARCHETYPE_ARCANE },
  { 0, -1 }
};

static const int ct_study_compatibility[NUM_ARCHETYPES][NUM_ARCHETYPES] = {
  /* From: Combat, Rogue, Arcane, Divine, Nature, Dark */
  { 1, 1, 1, 1, 1, 1 }, /* Combat */
  { 1, 1, 1, 0, 1, 0 }, /* Rogue */
  { 1, 1, 1, 0, 0, 1 }, /* Arcane */
  { 1, 0, 0, 1, 1, 0 }, /* Divine */
  { 1, 1, 0, 1, 1, 0 }, /* Nature */
  { 1, 0, 1, 0, 0, 1 }  /* Dark */
};

static const char *const ct_soft_titles[NUM_ARCHETYPES][6] = {
  { "Warrior", "Fighter", "Knight", "Berserker", "Barbarian", "Champion" },
  { "Rogue", "Thief", "Assassin", "Stalker", "Trickster", "Trickster" },
  { "Mage", "Wizard", "Sorcerer", "Arcanist", "Elementalist", "Elementalist" },
  { "Cleric", "Priest", "Templar", "Crusader", "Crusader", "Crusader" },
  { "Druid", "Ranger", "Shaman", "Warden", "Warden", "Warden" },
  { "Warlock", "Necromancer", "Occultist", "Reaper", "Reaper", "Reaper" }
};

static int classtrack_clamp_title_tier(int level)
{
  int tier = (level / 20);
  if (tier < 0)
    tier = 0;
  if (tier > 5)
    tier = 5;
  return tier;
}

static int classtrack_lookup_mapped_archetype(int ability,
                                              const struct ability_archetype_map *map)
{
  int i;

  for (i = 0; map[i].ability_id > 0; i++) {
    if (map[i].ability_id == ability)
      return map[i].archetype;
  }
  return -1;
}

static int classtrack_pick_archetype_for_ability(int ability, int was_spell)
{
  if (was_spell)
    return classtrack_lookup_mapped_archetype(ability, ct_spell_archetype_map);
  return classtrack_lookup_mapped_archetype(ability, ct_skill_archetype_map);
}

static int classtrack_min_study_level_for_ability(int ability_id)
{
  int i;
  int min_level = LVL_IMPL + 1;

  if (ability_id <= 0 || ability_id > TOP_SPELL_DEFINE)
    return -1;

  for (i = 0; i < NUM_CLASSES; i++) {
    int lvl = spell_info[ability_id].min_level[i];
    if (lvl > 0 && lvl < min_level)
      min_level = lvl;
  }

  if (min_level > LVL_IMMORT)
    return -1;

  return min_level;
}

static int classtrack_total_score(struct char_data *ch)
{
  int i;
  int total = 0;

  for (i = 0; i < NUM_ARCHETYPES; i++)
    total += MAX(0, GET_ARCHETYPE_SCORE(ch, i));

  return total;
}

static void classtrack_get_top2(struct char_data *ch, int *primary, int *secondary)
{
  int i;
  int top = ARCHETYPE_COMBAT;
  int second = ARCHETYPE_ROGUE;

  for (i = 0; i < NUM_ARCHETYPES; i++) {
    if (GET_ARCHETYPE_SCORE(ch, i) > GET_ARCHETYPE_SCORE(ch, top)) {
      second = top;
      top = i;
    } else if (i != top && GET_ARCHETYPE_SCORE(ch, i) > GET_ARCHETYPE_SCORE(ch, second)) {
      second = i;
    }
  }

  *primary = top;
  *secondary = second;
}

static int classtrack_get_primary(struct char_data *ch)
{
  int primary, secondary;
  classtrack_get_top2(ch, &primary, &secondary);
  return primary;
}

static int classtrack_is_compatible_pair(int a, int b)
{
  if (a < 0 || a >= NUM_ARCHETYPES || b < 0 || b >= NUM_ARCHETYPES)
    return 0;

  return ct_study_compatibility[a][b];
}

static int classtrack_is_hybrid_pair(int a, int b)
{
  if ((a == ARCHETYPE_COMBAT && b == ARCHETYPE_DIVINE) ||
      (a == ARCHETYPE_DIVINE && b == ARCHETYPE_COMBAT))
    return 1;
  if ((a == ARCHETYPE_ROGUE && b == ARCHETYPE_NATURE) ||
      (a == ARCHETYPE_NATURE && b == ARCHETYPE_ROGUE))
    return 1;
  if ((a == ARCHETYPE_ARCANE && b == ARCHETYPE_DARK) ||
      (a == ARCHETYPE_DARK && b == ARCHETYPE_ARCANE))
    return 1;
  if ((a == ARCHETYPE_COMBAT && b == ARCHETYPE_ARCANE) ||
      (a == ARCHETYPE_ARCANE && b == ARCHETYPE_COMBAT))
    return 1;
  if ((a == ARCHETYPE_ROGUE && b == ARCHETYPE_ARCANE) ||
      (a == ARCHETYPE_ARCANE && b == ARCHETYPE_ROGUE))
    return 1;
  if ((a == ARCHETYPE_COMBAT && b == ARCHETYPE_DARK) ||
      (a == ARCHETYPE_DARK && b == ARCHETYPE_COMBAT))
    return 1;

  return 0;
}

static int classtrack_is_opposed_pair(int a, int b)
{
  if ((a == ARCHETYPE_DIVINE && b == ARCHETYPE_DARK) ||
      (a == ARCHETYPE_DARK && b == ARCHETYPE_DIVINE))
    return 1;

  if ((a == ARCHETYPE_NATURE && b == ARCHETYPE_DARK) ||
      (a == ARCHETYPE_DARK && b == ARCHETYPE_NATURE))
    return 1;

  return 0;
}

static const char *classtrack_soft_title(struct char_data *ch)
{
  int primary, secondary;
  int pscore, sscore;
  int tier;
  int total_score;

  classtrack_get_top2(ch, &primary, &secondary);
  pscore = GET_ARCHETYPE_SCORE(ch, primary);
  sscore = GET_ARCHETYPE_SCORE(ch, secondary);
  total_score = classtrack_total_score(ch);

  if (total_score < CT_SOFT_TOTAL_SCORE_MIN || pscore < CT_SOFT_MIN_SCORE)
    return "Adventurer";

  if ((pscore - sscore) < CT_SOFT_DOMINANCE_MARGIN)
    return ct_soft_titles[primary][0];

  tier = classtrack_clamp_title_tier(GET_LEVEL(ch));
  if ((pscore - sscore) < CT_SOFT_WEAK_DOMINANCE_MARGIN && tier > 2)
    tier = 2;
  return ct_soft_titles[primary][tier];
}

static const char *classtrack_final_title(struct char_data *ch)
{
  int primary, secondary;
  int pscore, sscore;

  classtrack_get_top2(ch, &primary, &secondary);
  pscore = GET_ARCHETYPE_SCORE(ch, primary);
  sscore = GET_ARCHETYPE_SCORE(ch, secondary);

  if (pscore < CT_FINAL_MIN_SCORE)
    return "Adventurer";

  if ((pscore - sscore) <= CT_FINAL_HYBRID_MARGIN && classtrack_is_hybrid_pair(primary, secondary)) {
    if ((primary == ARCHETYPE_COMBAT && secondary == ARCHETYPE_DIVINE) ||
        (primary == ARCHETYPE_DIVINE && secondary == ARCHETYPE_COMBAT))
      return "Paladin";
    if ((primary == ARCHETYPE_ROGUE && secondary == ARCHETYPE_NATURE) ||
        (primary == ARCHETYPE_NATURE && secondary == ARCHETYPE_ROGUE))
      return "Ranger";
    if ((primary == ARCHETYPE_ARCANE && secondary == ARCHETYPE_DARK) ||
        (primary == ARCHETYPE_DARK && secondary == ARCHETYPE_ARCANE))
      return "Warlock";
    if ((primary == ARCHETYPE_COMBAT && secondary == ARCHETYPE_ARCANE) ||
        (primary == ARCHETYPE_ARCANE && secondary == ARCHETYPE_COMBAT))
      return "Spellblade";
    if ((primary == ARCHETYPE_ROGUE && secondary == ARCHETYPE_ARCANE) ||
        (primary == ARCHETYPE_ARCANE && secondary == ARCHETYPE_ROGUE))
      return "Shadowblade";
    if ((primary == ARCHETYPE_COMBAT && secondary == ARCHETYPE_DARK) ||
        (primary == ARCHETYPE_DARK && secondary == ARCHETYPE_COMBAT))
      return "Death Knight";
  }

  if ((pscore - sscore) < CT_FINAL_MARGIN)
    return ct_soft_titles[primary][0];

  return ct_soft_titles[primary][5];
}

static void classtrack_set_title(struct char_data *ch, const char *title)
{
  if (!ch || IS_NPC(ch) || !title)
    return;

  strlcpy(GET_SOFT_CLASS_TITLE(ch), title, sizeof(ch->player_specials->saved.soft_class_title));
}

void classtrack_init_new_player(struct char_data *ch)
{
  int i;

  if (!ch || IS_NPC(ch))
    return;

  for (i = 0; i < NUM_ARCHETYPES; i++)
    GET_ARCHETYPE_SCORE(ch, i) = 0;

  GET_CLASS_LOCKED(ch) = 0;
  classtrack_set_title(ch, "Adventurer");
}

void classtrack_record_ability_use(struct char_data *ch, int ability, int was_spell)
{
  int archetype;
  int gain = 2;
  int i, broad_count = 0;
  int dominant;

  if (!ch || IS_NPC(ch) || GET_CLASS_LOCKED(ch))
    return;

  if (ability <= 0 || ability > MAX_SKILLS)
    return;

  archetype = classtrack_pick_archetype_for_ability(ability, was_spell);

  /* Neutral fallback for unmapped abilities: no score gain (prevents arcane bias). */
  if (archetype < 0 || archetype >= NUM_ARCHETYPES)
    return;

  dominant = classtrack_get_primary(ch);

  for (i = 0; i < NUM_ARCHETYPES; i++) {
    if (GET_ARCHETYPE_SCORE(ch, i) >= CT_BROAD_ACTIVE_SCORE)
      broad_count++;
  }

  if (GET_ARCHETYPE_SCORE(ch, dominant) > 0 && archetype == dominant) {
    gain = 3;
  } else if (GET_ARCHETYPE_SCORE(ch, dominant) > 0) {
    gain = 1;
  }

  if (broad_count >= 3 && archetype != dominant)
    gain = 1;
  if (broad_count >= 4 && archetype == dominant)
    gain = 2;

  GET_ARCHETYPE_SCORE(ch, archetype) += gain;
}

void classtrack_check_level_checkpoint(struct char_data *ch)
{
  const char *title;

  if (!ch || IS_NPC(ch) || GET_CLASS_LOCKED(ch))
    return;

  if (GET_LEVEL(ch) >= 100) {
    title = classtrack_final_title(ch);
    classtrack_set_title(ch, title);
    GET_CLASS_LOCKED(ch) = 1;
    send_to_char(ch, "\r\nYour path is now permanent. You are %s%s%s.\r\n",
                 CCYEL(ch, C_NRM), title, CCNRM(ch, C_NRM));
    return;
  }

  if (GET_LEVEL(ch) < 10 || (GET_LEVEL(ch) % 10) != 0)
    return;

  title = classtrack_soft_title(ch);
  classtrack_set_title(ch, title);
  send_to_char(ch, "\r\nYour path sharpens. You are now known as %s%s%s.\r\n",
               CCYEL(ch, C_NRM), title, CCNRM(ch, C_NRM));
}

int classtrack_can_study_archetype(struct char_data *ch, int target_archetype,
                                   char *reason, size_t reason_len)
{
  int primary, secondary;
  int pscore, sscore;

  if (!ch || IS_NPC(ch))
    return 1;

  if (target_archetype < 0 || target_archetype >= NUM_ARCHETYPES)
    return 1;

  classtrack_get_top2(ch, &primary, &secondary);
  pscore = GET_ARCHETYPE_SCORE(ch, primary);
  sscore = GET_ARCHETYPE_SCORE(ch, secondary);

  if (GET_LEVEL(ch) < CT_COMMIT_LEVEL || pscore < CT_COMMIT_PRIMARY ||
      (pscore - sscore) < CT_COMMIT_MARGIN)
    return 1;

  if (target_archetype == primary || target_archetype == secondary)
    return 1;

  if (classtrack_is_compatible_pair(primary, target_archetype))
    return 1;

  if (classtrack_is_opposed_pair(primary, target_archetype)) {
    if (reason && reason_len > 0)
      strlcpy(reason, "You have gone too far down your current path to learn that kind of power.", reason_len);
    return 0;
  }

  if (reason && reason_len > 0)
    strlcpy(reason, "You have gone too far down your current path to learn that kind of power.", reason_len);
  return 0;
}

const char *classtrack_display_class_name(struct char_data *ch)
{
  if (!ch || IS_NPC(ch))
    return "Adventurer";

  if (!GET_CLASS_LOCKED(ch))
    return "Adventurer";

  if (*GET_SOFT_CLASS_TITLE(ch))
    return GET_SOFT_CLASS_TITLE(ch);

  return "Adventurer";
}

const char *classtrack_display_class_abbrev(struct char_data *ch)
{
  static char abbrev[4];
  const char *name;
  int i, j;

  if (!ch || IS_NPC(ch))
    return "Adv";

  if (!GET_CLASS_LOCKED(ch))
    return "Adv";

  name = classtrack_display_class_name(ch);
  for (i = 0, j = 0; name[i] != '\0' && j < 3; i++) {
    if (isalpha((unsigned char)name[i]))
      abbrev[j++] = UPPER(name[i]);
  }

  if (j == 0)
    return "Adv";

  abbrev[j] = '\0';
  return abbrev;
}

int classtrack_get_ability_archetype(int ability_id)
{
  int archetype;

  if (ability_id <= 0 || ability_id > TOP_SPELL_DEFINE)
    return -1;

  archetype = classtrack_lookup_mapped_archetype(ability_id, ct_spell_archetype_map);
  if (archetype >= 0)
    return archetype;

  return classtrack_lookup_mapped_archetype(ability_id, ct_skill_archetype_map);
}

int classtrack_can_study_ability(struct char_data *ch, int ability_id)
{
  int required_level;
  int archetype;

  if (!ch || IS_NPC(ch))
    return 0;

  if (ability_id <= 0 || ability_id > TOP_SPELL_DEFINE || ability_id > MAX_SKILLS)
    return 0;

  if (!spell_info[ability_id].name || !*spell_info[ability_id].name ||
      !str_cmp(spell_info[ability_id].name, "!UNUSED!"))
    return 0;

  if (GET_SKILL(ch, ability_id) > 0)
    return 0;

  required_level = classtrack_min_study_level_for_ability(ability_id);
  if (required_level > 0 && GET_LEVEL(ch) < required_level)
    return 0;

  archetype = classtrack_get_ability_archetype(ability_id);
  if (archetype < 0 || archetype >= NUM_ARCHETYPES)
    return 0;

  return classtrack_can_study_archetype(ch, archetype, NULL, 0);
}

int classtrack_get_study_min_level(int ability_id)
{
  return classtrack_min_study_level_for_ability(ability_id);
}

int classtrack_is_study_catalog_ability(int ability_id, int show_spells)
{
  int required_level;
  int archetype;

  if (ability_id <= 0 || ability_id > TOP_SPELL_DEFINE || ability_id > MAX_SKILLS)
    return 0;

  if (!spell_info[ability_id].name || !*spell_info[ability_id].name ||
      !str_cmp(spell_info[ability_id].name, "!UNUSED!"))
    return 0;

  if (show_spells) {
    if (ability_id > MAX_SPELLS)
      return 0;
  } else {
    if (ability_id <= MAX_SPELLS)
      return 0;
  }

  if (ability_id == SKILL_STUDY)
    return 0;

  required_level = classtrack_min_study_level_for_ability(ability_id);
  if (required_level <= 0)
    return 0;

  archetype = classtrack_get_ability_archetype(ability_id);
  if (archetype < 0 || archetype >= NUM_ARCHETYPES)
    return 0;

  return 1;
}

void classtrack_ensure_study_skill(struct char_data *ch)
{
  if (!ch || IS_NPC(ch))
    return;

  if (GET_SKILL(ch, SKILL_STUDY) <= 0)
    SET_SKILL(ch, SKILL_STUDY, 1);

  if (!GET_CLASS_LOCKED(ch)) {
    if (GET_SKILL(ch, SPELL_MAGIC_MISSILE) > 0 && GET_STUDY_LEARN_LEVEL(ch, SPELL_MAGIC_MISSILE) == 0)
      SET_STUDY_LEARN_LEVEL(ch, SPELL_MAGIC_MISSILE, 1);
    if (GET_SKILL(ch, SKILL_RECALL) > 0 && GET_STUDY_LEARN_LEVEL(ch, SKILL_RECALL) == 0)
      SET_STUDY_LEARN_LEVEL(ch, SKILL_RECALL, 1);
    if (GET_SKILL(ch, SKILL_STUDY) > 0 && GET_STUDY_LEARN_LEVEL(ch, SKILL_STUDY) == 0)
      SET_STUDY_LEARN_LEVEL(ch, SKILL_STUDY, 1);
  }
}

void classtrack_record_study_learn_level(struct char_data *ch, int ability_id, int learned_level)
{
  int capped_level;

  if (!ch || IS_NPC(ch))
    return;

  if (ability_id <= 0 || ability_id > MAX_SKILLS)
    return;

  capped_level = MAX(1, MIN(learned_level, 255));
  SET_STUDY_LEARN_LEVEL(ch, ability_id, capped_level);
}

int classtrack_get_study_display_level(struct char_data *ch, int ability_id, int fallback_level)
{
  int learned_at;

  if (!ch || IS_NPC(ch))
    return fallback_level;

  if (ability_id <= 0 || ability_id > MAX_SKILLS)
    return fallback_level;

  if (GET_CLASS_LOCKED(ch))
    return fallback_level;

  if (GET_SKILL(ch, ability_id) <= 0)
    return fallback_level;

  if (ability_id == SPELL_MAGIC_MISSILE || ability_id == SKILL_RECALL || ability_id == SKILL_STUDY)
    return 1;

  learned_at = GET_STUDY_LEARN_LEVEL(ch, ability_id);
  if (learned_at > 0)
    return learned_at;

  return GET_LEVEL(ch);
}
