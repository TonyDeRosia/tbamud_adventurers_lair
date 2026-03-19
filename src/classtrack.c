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
#define CT_SOFT_DOMINANCE_MARGIN 4
#define CT_FINAL_MIN_SCORE 80
#define CT_FINAL_MARGIN 12
#define CT_COMMIT_LEVEL 50
#define CT_COMMIT_PRIMARY 120
#define CT_COMMIT_MARGIN 30

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

static int classtrack_name_has(const char *name, const char *needle)
{
  return (name && *name && needle && *needle && strstr(name, needle) != NULL);
}

static int classtrack_pick_archetype_for_spell(int ability)
{
  const char *name;

  if (ability <= 0 || ability > TOP_SPELL_DEFINE)
    return ARCHETYPE_ARCANE;

  name = spell_info[ability].name;
  if (!name)
    return ARCHETYPE_ARCANE;

  if (classtrack_name_has(name, "heal") || classtrack_name_has(name, "holy") ||
      classtrack_name_has(name, "bless") || classtrack_name_has(name, "sanct") ||
      classtrack_name_has(name, "cleric") || classtrack_name_has(name, "divine"))
    return ARCHETYPE_DIVINE;

  if (classtrack_name_has(name, "dead") || classtrack_name_has(name, "death") ||
      classtrack_name_has(name, "dark") || classtrack_name_has(name, "shadow") ||
      classtrack_name_has(name, "unholy") || classtrack_name_has(name, "necro") ||
      classtrack_name_has(name, "plague") || classtrack_name_has(name, "miasma"))
    return ARCHETYPE_DARK;

  if (classtrack_name_has(name, "wolf") || classtrack_name_has(name, "bear") ||
      classtrack_name_has(name, "bark") || classtrack_name_has(name, "storm") ||
      classtrack_name_has(name, "nature") || classtrack_name_has(name, "thunder") ||
      classtrack_name_has(name, "lightning") || classtrack_name_has(name, "druid"))
    return ARCHETYPE_NATURE;

  if (classtrack_name_has(name, "fire") || classtrack_name_has(name, "frost") ||
      classtrack_name_has(name, "ice") || classtrack_name_has(name, "arcane") ||
      classtrack_name_has(name, "magic") || classtrack_name_has(name, "meteor") ||
      classtrack_name_has(name, "bolt") || classtrack_name_has(name, "spell") ||
      classtrack_name_has(name, "mana"))
    return ARCHETYPE_ARCANE;

  if (classtrack_name_has(name, "ward") || classtrack_name_has(name, "armor") ||
      classtrack_name_has(name, "skin") || classtrack_name_has(name, "strength"))
    return ARCHETYPE_COMBAT;

  return ARCHETYPE_ARCANE;
}

static int classtrack_pick_archetype_for_skill(int ability)
{
  switch (ability) {
    case SKILL_BACKSTAB:
    case SKILL_HIDE:
    case SKILL_PICK_LOCK:
    case SKILL_STEAL:
    case SKILL_SNEAK:
    case SKILL_APPRAISE_ENEMY:
      return ARCHETYPE_ROGUE;

    case SKILL_TRACK:
      return ARCHETYPE_NATURE;

    case SKILL_RECALL:
      return ARCHETYPE_DIVINE;

    default:
      return ARCHETYPE_COMBAT;
  }
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

static int classtrack_is_compatible_pair(int a, int b)
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

  classtrack_get_top2(ch, &primary, &secondary);
  pscore = GET_ARCHETYPE_SCORE(ch, primary);
  sscore = GET_ARCHETYPE_SCORE(ch, secondary);

  if (pscore < CT_SOFT_MIN_SCORE)
    return "Adventurer";

  if ((pscore - sscore) < CT_SOFT_DOMINANCE_MARGIN)
    return "Adventurer";

  tier = classtrack_clamp_title_tier(GET_LEVEL(ch));
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

  if ((pscore - sscore) <= CT_FINAL_MARGIN && classtrack_is_compatible_pair(primary, secondary)) {
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
  int gain = 1;
  int i, broad_count = 0;

  if (!ch || IS_NPC(ch) || GET_CLASS_LOCKED(ch))
    return;

  if (ability <= 0 || ability > MAX_SKILLS)
    return;

  if (was_spell)
    archetype = classtrack_pick_archetype_for_spell(ability);
  else
    archetype = classtrack_pick_archetype_for_skill(ability);

  if (archetype < 0 || archetype >= NUM_ARCHETYPES)
    return;

  for (i = 0; i < NUM_ARCHETYPES; i++) {
    if (GET_ARCHETYPE_SCORE(ch, i) >= CT_SOFT_MIN_SCORE)
      broad_count++;
  }

  /* Anti-hoarding scaffold: broad investment reduces future cross-path gains. */
  if (broad_count >= 3 && GET_ARCHETYPE_SCORE(ch, archetype) < CT_FINAL_MIN_SCORE)
    gain = 1;
  else if (was_spell)
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
