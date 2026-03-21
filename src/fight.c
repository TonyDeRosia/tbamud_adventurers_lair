/**************************************************************************
*  File: fight.c                                           Part of tbaMUD *
*  Usage: Combat system.                                                  *
*                                                                         *
*  All rights reserved.  See license for complete information.            *
*                                                                         *
*  Copyright (C) 1993, 94 by the Trustees of the Johns Hopkins University *
*  CircleMUD is based on DikuMUD, Copyright (C) 1990, 1991.               *
**************************************************************************/

#include "conf.h"
#include "sysdep.h"
#include "structs.h"
#include "utils.h"
#include "comm.h"
#include "handler.h"
#include "interpreter.h"
#include "db.h"
#include "spells.h"
#include "screen.h"
#include "constants.h"
#include "dg_scripts.h"
#include "act.h"
#include "class.h"
#include "fight.h"
#include "ai_actor.h"
#include "shop.h"
#include "quest.h"
#include "criticalhits.h"

#define PVP_GLORY_COOLDOWN 600 /* seconds */

#define RARE_KILL_MAX_COUNT 10
#define RARE_KILL_BASE_BONUS 5
#define RARE_KILL_STEP_BONUS 2


/* locally defined global variables, used externally */
/* head of l-list of fighting chars */
struct char_data *combat_list = NULL;
/* Weapon attack texts */
struct attack_hit_type attack_hit_text[] =
{
  { "fist", "fists" },    /* 0 */
  {"sting", "stings"},
  {"whip", "whips"},
  {"slash", "slashes"},
  {"bite", "bites"},
  {"bludgeon", "bludgeons"},  /* 5 */
  {"crush", "crushes"},
  {"pound", "pounds"},
  {"claw", "claws"},
  {"maul", "mauls"},
  {"thrash", "thrashes"}, /* 10 */
  {"pierce", "pierces"},
  {"blast", "blasts"},
  {"punch", "punches"},
  {"stab", "stabs"}
};

/* local (file scope only) variables */
static struct char_data *next_combat_list = NULL;
static int pending_damage_type = DAM_NONE;
static bool violence_tick_running = FALSE;

void set_next_damage_type(int damage_type)
{
  pending_damage_type = damage_type;
}

static int take_next_damage_type(void)
{
  int damage_type = pending_damage_type;
  pending_damage_type = DAM_NONE;
  return damage_type;
}

/* local file scope utility functions */
static void perform_group_gain(struct char_data *ch, int base, struct char_data *victim);
static int count_live_mobs_by_vnum(mob_vnum vnum);
static int rare_kill_bonus_for_count(int live_count);
static int rare_kill_bonus_for_victim(struct char_data *victim);
static void dam_message(int dam, struct char_data *ch, struct char_data *victim, int w_type);
static void make_corpse(struct char_data *ch);
static void process_round_effects(void);
static int find_affect_modifier_for_flag(struct char_data *ch, int aff_flag, int fallback);
static int remove_affects_by_flag(struct char_data *ch, int aff_flag);
static int is_shadow_servant_for(struct char_data *owner, struct char_data *mob);
static int count_shadow_servants_for(struct char_data *ch);
static int extracted_shadow_slot(struct char_data *mob);
static int return_extracted_shadow_to_storage(struct char_data *mob);
static void prepare_shadow_servant_for_removal(struct char_data *mob);
static int should_owned_follower_auto_assist(struct char_data *owner, struct char_data *follower);
static void auto_assist_owned_followers(struct char_data *owner);
static void change_alignment(struct char_data *ch, struct char_data *victim);
static void group_gain(struct char_data *ch, struct char_data *victim);
static void solo_gain(struct char_data *ch, struct char_data *victim);
/** @todo refactor this function name */
static char *replace_string(const char *str, const char *weapon_singular, const char *weapon_plural);
static void do_spirit_procs(struct char_data *ch, struct char_data *vict);


static int damage_severity_tier(int dam, struct char_data *victim)
{
  int pct;

  if (dam <= 0 || !victim)
    return 0;

  pct = (dam * 100) / MAX(1, GET_MAX_HIT(victim));

  if (pct <= 4)        return 1;
  else if (pct <= 9)   return 2;
  else if (pct <= 19)  return 3;
  else if (pct <= 29)  return 4;
  else if (pct <= 44)  return 5;
  else if (pct <= 59)  return 6;
  else if (pct <= 74)  return 7;
  else if (pct <= 89)  return 8;
  else                 return 9;
}

static const char *severity_color(int tier)
{
  switch (tier) {
    case 1: case 2: return "\tG";
    case 3: case 4: return "\tW";
    case 5: return "\ty";
    case 6: return "\tY";
    case 7: return "\tR";
    case 8: return "\tM";
    case 9: return "\tR";
    default: return "\tn";
  }
}

static const char *severity_verb_base(int tier)
{
  switch (tier) {
    case 1: return "graze";
    case 2: return "glance";
    case 3: return "hit";
    case 4: return "strike";
    case 5: return "slam";
    case 6: return "crush";
    case 7: return "blast";
    case 8: return "shred";
    case 9: return "pulverize";
    default: return "hit";
  }
}

static int is_shadow_servant_for(struct char_data *owner, struct char_data *mob)
{
  if (!owner || !mob || !IS_NPC(mob) || mob->master != owner)
    return FALSE;
  if (!AFF_FLAGGED(mob, AFF_CHARM) || GET_SUMMON_TIMER(mob) <= 0)
    return FALSE;
  return affected_by_spell(mob, SPELL_CALL_SHADOW_LEGION)
      || affected_by_spell(mob, SPELL_SHADOW_EXTRACTION)
      || affected_by_spell(mob, SPELL_ARISE_GREATER);
}

int is_owned_follower_target(struct char_data *attacker, struct char_data *victim)
{
  if (!attacker || !victim || attacker == victim || !IS_NPC(victim))
    return FALSE;

  if (victim->master != attacker)
    return FALSE;

  if (AFF_FLAGGED(victim, AFF_CHARM))
    return TRUE;

  if (GET_SUMMON_TIMER(victim) > 0)
    return TRUE;

  if (is_shadow_servant_for(attacker, victim))
    return TRUE;

  return FALSE;
}

static int count_shadow_servants_for(struct char_data *ch)
{
  struct follow_type *f;
  int n = 0;
  if (!ch)
    return 0;
  for (f = ch->followers; f; f = f->next)
    if (is_shadow_servant_for(ch, f->follower))
      n++;
  return n;
}

static int should_owned_follower_auto_assist(struct char_data *owner, struct char_data *follower)
{
  if (!owner || !follower || !IS_NPC(follower))
    return FALSE;
  if (follower->master != owner)
    return FALSE;
  if (IN_ROOM(follower) != IN_ROOM(owner))
    return FALSE;
  if (FIGHTING(follower))
    return FALSE;

  /* Owned combat allies are charmed pets/summons and temporary summons. */
  if (!AFF_FLAGGED(follower, AFF_CHARM) &&
      GET_SUMMON_TIMER(follower) <= 0 &&
      !is_shadow_servant_for(owner, follower))
    return FALSE;

  /* Keep existing incapacity checks aligned with combat eligibility. */
  if (GET_POS(follower) < POS_STANDING ||
      AFF_FLAGGED(follower, AFF_STUNNED) ||
      AFF_FLAGGED(follower, AFF_FEARFUL) ||
      AFF_FLAGGED(follower, AFF_ROOTED))
    return FALSE;

  if (!CAN_SEE(follower, owner))
    return FALSE;

  return TRUE;
}

static void auto_assist_owned_followers(struct char_data *owner)
{
  struct follow_type *f;
  int assist_fired = FALSE;

  if (!owner || !FIGHTING(owner))
    return;

  /* Debug trace point: owner/follower eligibility for auto-assist is gated
   * through should_owned_follower_auto_assist(). */
  for (f = owner->followers; f; f = f->next) {
    if (!should_owned_follower_auto_assist(owner, f->follower))
      continue;

    /* Resolve target from the follower's current room via assist helper. */
    do_assist(f->follower, GET_NAME(owner), 0, 0);
    assist_fired = TRUE;
  }

  if (assist_fired && CONFIG_DEBUG_MODE >= NRM && GET_LEVEL(owner) >= LVL_BUILDER) {
    send_to_char(owner,
      "\t1Combat Debug:\r\n"
      "   \t2Summon Auto-aid:\t3fired\tn\r\n");
  }
}

static int extracted_shadow_slot(struct char_data *mob)
{
  struct affected_type *af;

  if (!mob)
    return -1;

  for (af = mob->affected; af; af = af->next) {
    if (af->spell == SPELL_SHADOW_EXTRACTION && af->location == APPLY_NONE && af->modifier > 1)
      return af->modifier - 2;
  }

  return -1;
}

static void remove_follower_link_silently(struct char_data *master, struct char_data *follower)
{
  struct follow_type *node, *prev = NULL;

  if (!master || !follower)
    return;

  for (node = master->followers; node; prev = node, node = node->next) {
    if (node->follower != follower)
      continue;
    if (prev)
      prev->next = node->next;
    else
      master->followers = node->next;
    free(node);
    break;
  }
}

static void prepare_shadow_servant_for_removal(struct char_data *mob)
{
  struct char_data *fighter, *next_fighter;

  if (!mob)
    return;

  GET_SUMMON_TIMER(mob) = 0;
  HUNTING(mob) = NULL;

  if (FIGHTING(mob))
    stop_fighting(mob);

  for (fighter = combat_list; fighter; fighter = next_fighter) {
    next_fighter = fighter->next_fighting;
    if (FIGHTING(fighter) == mob)
      stop_fighting(fighter);
  }

  while (mob->followers) {
    struct follow_type *next = mob->followers->next;
    if (mob->followers->follower)
      mob->followers->follower->master = NULL;
    free(mob->followers);
    mob->followers = next;
  }

  if (mob->master) {
    remove_follower_link_silently(mob->master, mob);
    mob->master = NULL;
  }
  REMOVE_BIT_AR(AFF_FLAGS(mob), AFF_CHARM);
}

static int return_extracted_shadow_to_storage(struct char_data *mob)
{
  struct char_data *owner;
  int slot;

  if (!mob || !IS_NPC(mob))
    return FALSE;

  owner = mob->master;
  slot = extracted_shadow_slot(mob);
  if (!owner || IS_NPC(owner) || slot < 0 || slot >= MAX_SHADOW_ROSTER)
    return FALSE;
  if (!SHADOW_SLOT_OCCUPIED(owner, slot))
    return FALSE;

  SHADOW_SLOT_ACTIVE(owner, slot) = 0;

  if (IN_ROOM(mob) != NOWHERE)
    act("$n dissolves into darkness.", FALSE, mob, 0, 0, TO_ROOM);
  send_to_char(owner, "Your shadow %s is defeated and returns to your shadow storage.\r\n",
               shadow_slot_display_name(owner, slot));

  prepare_shadow_servant_for_removal(mob);
  extract_char(mob);
  save_char(owner);
  return TRUE;
}

static const char *severity_verb_third(int tier)
{
  switch (tier) {
    case 1: return "grazes";
    case 2: return "glances";
    case 3: return "hits";
    case 4: return "strikes";
    case 5: return "slams";
    case 6: return "crushes";
    case 7: return "blasts";
    case 8: return "shreds";
    case 9: return "pulverizes";
    default: return "hits";
  }
}

static const char *severity_impact_wrap_open(int tier)
{
  switch (tier) {
    case 7: return "- ";
    case 8: return "** ";
    case 9: return "***** ";
    default: return "";
  }
}

static const char *severity_impact_wrap_close(int tier)
{
  switch (tier) {
    case 7: return " -";
    case 8: return " **";
    case 9: return " *****";
    default: return "";
  }
}

static int victim_condition_band(const struct char_data *victim)
{
  int pct;

  if (!victim || GET_MAX_HIT(victim) <= 0)
    return 7;

  pct = (GET_HIT(victim) * 100) / GET_MAX_HIT(victim);

  if (pct >= 100) return 0;
  if (pct >= 90)  return 1;
  if (pct >= 75)  return 2;
  if (pct >= 50)  return 3;
  if (pct >= 30)  return 4;
  if (pct >= 15)  return 5;
  return 6;
}

static const char *victim_condition_text(int band)
{
  switch (band) {
    case 0: return "is in excellent condition.";
    case 1: return "has a few scratches.";
    case 2: return "has some small wounds and bruises.";
    case 3: return "has quite a few wounds.";
    case 4: return "has some big nasty wounds and scratches.";
    case 5: return "is gravely injured.";
    default: return "needs a hospital.";
  }
}

static void apply_severity_verb(char *out, size_t outsz, const char *in, int tier)
{
  const char *col = severity_color(tier);
  const char *vb  = severity_verb_base(tier);
  const char *vt  = severity_verb_third(tier);
  const char *pre = severity_impact_wrap_open(tier);
  const char *post = severity_impact_wrap_close(tier);
  char *pos;

  if (!in || !*in) {
    if (outsz) out[0] = '\0';
    return;
  }

  snprintf(out, outsz, "%s", in);

  /* Prefer replacing " hit" (base) first (skills often use "You hit $N"). */
  if ((pos = strstr(out, " hit "))) {
    char tmp[MAX_STRING_LENGTH];
    *pos = '\0';
    snprintf(tmp, sizeof(tmp), "%s %s%s%s%s\tn %s", out, col, pre, vb, post, pos + 5);
    snprintf(out, outsz, "%s", tmp);
    return;
  }
  if ((pos = strstr(out, " hit!"))) {
    char tmp[MAX_STRING_LENGTH];
    *pos = '\0';
    snprintf(tmp, sizeof(tmp), "%s %s%s%s%s\tn!%s", out, col, pre, vb, post, pos + 5);
    snprintf(out, outsz, "%s", tmp);
    return;
  }

  /* Then replace third person " hits" (many spell messages use "X hits $N"). */
  if ((pos = strstr(out, " hits "))) {
    char tmp[MAX_STRING_LENGTH];
    *pos = '\0';
    snprintf(tmp, sizeof(tmp), "%s %s%s%s%s\tn %s", out, col, pre, vt, post, pos + 6);
    snprintf(out, outsz, "%s", tmp);
    return;
  }
  if ((pos = strstr(out, " hits!"))) {
    char tmp[MAX_STRING_LENGTH];
    *pos = '\0';
    snprintf(tmp, sizeof(tmp), "%s %s%s%s%s\tn!%s", out, col, pre, vt, post, pos + 6);
    snprintf(out, outsz, "%s", tmp);
    return;
  }

  /* If we cannot find a generic verb, leave as is. */
}

static int compute_thaco(struct char_data *ch, struct char_data *vict);


#define IS_WEAPON(type) (((type) >= TYPE_HIT) && ((type) < TYPE_SUFFERING))

static int spirit_proc_damage(const struct char_data *ch)
{
  int bonus = 0;
  int dam = 0;

  if (!ch)
    return 0;

  bonus = MIN(GET_LEVEL(ch) / 30, 2);
  dam = dice(1, 3) + bonus;

  return MIN(dam, 5);
}

static void spirit_proc_attack(struct char_data *ch, struct char_data *vict, int attacktype,
                               int proc_chance, int hit_chance)
{
  int dam;

  if (!ch || IN_ROOM(ch) == NOWHERE)
    return;

  if (!vict || IN_ROOM(vict) == NOWHERE)
    return;

  if (DEAD(vict) || IN_ROOM(ch) != IN_ROOM(vict))
    return;

  if (rand_number(1, 100) > proc_chance)
    return;

  if (!CAN_SEE(ch, vict))
    return;

  if (rand_number(1, 100) > hit_chance) {
    damage(ch, vict, 0, attacktype);
    return;
  }

  dam = spirit_proc_damage(ch);
  damage(ch, vict, dam, attacktype);
}

static void do_spirit_procs(struct char_data *ch, struct char_data *vict)
{
  const int proc_chance = 10;
  const int hit_chance = 70;

  if (!ch || IN_ROOM(ch) == NOWHERE)
    return;

  if (!vict || IN_ROOM(vict) == NOWHERE)
    return;

  if (!FIGHTING(ch))
    return;

  if (DEAD(vict) || IN_ROOM(ch) != IN_ROOM(vict))
    return;

  if (!affected_by_spell(ch, SPELL_BEAR_SPIRIT) &&
      !affected_by_spell(ch, SPELL_WOLF_SPIRIT) &&
      !affected_by_spell(ch, SPELL_TIGER_SPIRIT) &&
      !affected_by_spell(ch, SPELL_EAGLE_SPIRIT) &&
      !affected_by_spell(ch, SPELL_DRAGON_SPIRIT))
    return;

  if (affected_by_spell(ch, SPELL_BEAR_SPIRIT))
    spirit_proc_attack(ch, vict, SPELL_BEAR_SPIRIT, proc_chance, hit_chance);
  if (!vict || IN_ROOM(vict) == NOWHERE || DEAD(vict) || IN_ROOM(ch) != IN_ROOM(vict))
    return;
  if (affected_by_spell(ch, SPELL_WOLF_SPIRIT))
    spirit_proc_attack(ch, vict, SPELL_WOLF_SPIRIT, proc_chance, hit_chance);
  if (!vict || IN_ROOM(vict) == NOWHERE || DEAD(vict) || IN_ROOM(ch) != IN_ROOM(vict))
    return;
  if (affected_by_spell(ch, SPELL_TIGER_SPIRIT))
    spirit_proc_attack(ch, vict, SPELL_TIGER_SPIRIT, proc_chance, hit_chance);
  if (!vict || IN_ROOM(vict) == NOWHERE || DEAD(vict) || IN_ROOM(ch) != IN_ROOM(vict))
    return;
  if (affected_by_spell(ch, SPELL_EAGLE_SPIRIT))
    spirit_proc_attack(ch, vict, SPELL_EAGLE_SPIRIT, proc_chance, hit_chance);
  if (!vict || IN_ROOM(vict) == NOWHERE || DEAD(vict) || IN_ROOM(ch) != IN_ROOM(vict))
    return;
  if (affected_by_spell(ch, SPELL_DRAGON_SPIRIT))
    spirit_proc_attack(ch, vict, SPELL_DRAGON_SPIRIT, proc_chance, hit_chance);
}
/* The Fight related routines */
void appear(struct char_data *ch)
{
  if (affected_by_spell(ch, SPELL_INVISIBLE))
    affect_from_char(ch, SPELL_INVISIBLE);

  REMOVE_BIT_AR(AFF_FLAGS(ch), AFF_INVISIBLE);
  REMOVE_BIT_AR(AFF_FLAGS(ch), AFF_HIDE);

  if (GET_LEVEL(ch) < LVL_IMMORT)
    act("$n slowly fades into existence.", FALSE, ch, 0, 0, TO_ROOM);
  else
    act("You feel a strange presence as $n appears, seemingly from nowhere.",
	FALSE, ch, 0, 0, TO_ROOM);
}

int legacy_ac_to_armor(int legacy_ac)
{
  /* Legacy AC was lower-is-better with 100 as "no armor". Convert to
   * the new higher-is-better Armor scale without inflating durability. */
  int armor = (100 - legacy_ac + 2) / 4; /* rounded integer divide by 4 */
  return MAX(0, MIN(200, armor));
}

int compute_armor(struct char_data *ch)
{
  int armor_bonus = 10 + (GET_CON(ch) - 10) + ((GET_STR(ch) - 10) / 2);
  int armor = GET_ARMOR(ch) + armor_bonus;
  return MAX(0, armor);
}

int compute_armor_class(struct char_data *ch)
{
  return compute_armor(ch);
}

int compute_evasion(struct char_data *ch)
{
  int evasion_bonus = 10 + ((GET_DEX(ch) - 10) * 2);
  int evasion = GET_EVASION(ch) + evasion_bonus;
  return MAX(0, evasion);
}

static int compute_defensive_evasion_value(struct char_data *victim, int *level_component)
{
  int level_bonus = 0;

  if (!victim) {
    if (level_component)
      *level_component = 0;
    return 0;
  }

  level_bonus = GET_LEVEL(victim);
  if (level_component)
    *level_component = level_bonus;

  return compute_evasion(victim) + level_bonus;
}

int compute_offensive_hit_value(struct char_data *ch, struct char_data *victim)
{
  int level_bonus = GET_LEVEL(ch);
  int hitroll_bonus = GET_HITROLL(ch);
  int stat_bonus = str_app[STRENGTH_APPLY_INDEX(ch)].tohit;
  int mental_bonus = (GET_INT(ch) - 10) / 4 + (GET_WIS(ch) - 10) / 4;
  int level_gap_bonus = 0;
  int situational_bonus = 0;

  if (victim) {
    int level_gap = GET_LEVEL(ch) - GET_LEVEL(victim);
    if (level_gap > 0)
      level_gap_bonus = (level_gap / 2) + (level_gap / 4);
    else if (level_gap < 0)
      level_gap_bonus = level_gap / 3;
  }

  if (victim && AFF_FLAGGED(ch, AFF_TRUESIGHT) &&
      (AFF_FLAGGED(victim, AFF_INVISIBLE) || AFF_FLAGGED(victim, AFF_HIDE)))
    situational_bonus = 6;

  return 30 + level_bonus + hitroll_bonus + stat_bonus + mental_bonus +
         level_gap_bonus + situational_bonus;
}

int compute_hit_chance_from_values(int offensive_hit, int target_evasion)
{
  int hit_chance = 50 + (offensive_hit - target_evasion);

  return MAX(5, MIN(95, hit_chance));
}

void update_pos(struct char_data *victim)
{
  if ((GET_HIT(victim) > 0) && (GET_POS(victim) > POS_STUNNED))
    return;
  else if (GET_HIT(victim) > 0)
    GET_POS(victim) = POS_STANDING;
  else if (GET_HIT(victim) <= -11)
    GET_POS(victim) = POS_DEAD;
  else if (GET_HIT(victim) <= -6)
    GET_POS(victim) = POS_MORTALLYW;
  else if (GET_HIT(victim) <= -3)
    GET_POS(victim) = POS_INCAP;
  else
    GET_POS(victim) = POS_STUNNED;
}

void check_killer(struct char_data *ch, struct char_data *vict)
{
  if (PLR_FLAGGED(vict, PLR_KILLER) || PLR_FLAGGED(vict, PLR_THIEF))
    return;
  if (PLR_FLAGGED(ch, PLR_KILLER) || IS_NPC(ch) || IS_NPC(vict) || ch == vict)
    return;

  SET_BIT_AR(PLR_FLAGS(ch), PLR_KILLER);
  send_to_char(ch, "If you want to be a PLAYER KILLER, so be it...\r\n");
  mudlog(BRF, MAX(LVL_IMMORT, MAX(GET_INVIS_LEV(ch), GET_INVIS_LEV(vict))), 
    TRUE, "PC Killer bit set on %s for initiating attack on %s at %s.",
    GET_NAME(ch), GET_NAME(vict), world[IN_ROOM(vict)].name);
}

/* start one char fighting another (yes, it is horrible, I know... )  */
void set_fighting(struct char_data *ch, struct char_data *vict)
{
  if (ch == vict)
    return;

  if (FIGHTING(ch)) {
    core_dump();
    return;
  }

  ch->next_fighting = combat_list;
  combat_list = ch;

  if (AFF_FLAGGED(ch, AFF_SLEEP))
    affect_from_char(ch, SPELL_SLEEP);

  FIGHTING(ch) = vict;
  GET_POS(ch) = POS_FIGHTING;

  ai_actor_event_combat_start(ch, vict);

  if (!CONFIG_PK_ALLOWED)
    check_killer(ch, vict);
}

/* remove a char from the list of fighting chars */
void stop_fighting(struct char_data *ch)
{
  struct char_data *temp;

  if (ch == next_combat_list)
    next_combat_list = ch->next_fighting;

  REMOVE_FROM_LIST(ch, combat_list, next_fighting);
  ch->next_fighting = NULL;
  FIGHTING(ch) = NULL;
  GET_POS(ch) = POS_STANDING;
  update_pos(ch);
}

static void make_corpse(struct char_data *ch)
{
  int inv_dropped = 0;
  long long dropped_gold = 0;
  char buf2[MAX_NAME_LENGTH + 64];
  struct obj_data *corpse, *o;
  struct obj_data *money;
  int i, x, y;

  corpse = create_obj();

  corpse->item_number = NOTHING;
  IN_ROOM(corpse) = NOWHERE;
  corpse->name = strdup("corpse");

  snprintf(buf2, sizeof(buf2), "The corpse of %s is lying here.", GET_NAME(ch));
  corpse->description = strdup(buf2);

  snprintf(buf2, sizeof(buf2), "the corpse of %s", GET_NAME(ch));
  corpse->short_description = strdup(buf2);

  GET_OBJ_TYPE(corpse) = ITEM_CONTAINER;
  for(x = y = 0; x < EF_ARRAY_MAX || y < TW_ARRAY_MAX; x++, y++) {
    if (x < EF_ARRAY_MAX)
      GET_OBJ_EXTRA_AR(corpse, x) = 0;
    if (y < TW_ARRAY_MAX)
      corpse->obj_flags.wear_flags[y] = 0;
  }
  SET_BIT_AR(GET_OBJ_WEAR(corpse), ITEM_WEAR_TAKE);
  SET_BIT_AR(GET_OBJ_EXTRA(corpse), ITEM_NODONATE);
  GET_OBJ_VAL(corpse, 0) = 0;	/* You can't store stuff in a corpse */
  GET_OBJ_VAL(corpse, 1) = IS_NPC(ch) ? GET_MOB_VNUM(ch) : NOBODY; /* source mob vnum for shadow extraction */
  GET_OBJ_VAL(corpse, 2) = GET_LEVEL(ch); /* source level snapshot for shadow extraction */
  GET_OBJ_VAL(corpse, 3) = 1;	/* corpse identifier */
  CORPSE_SHADOW_ATTEMPTS(corpse) = 3; /* shadow extraction attempts remaining */
  CORPSE_SHADOW_ATTEMPTS_INIT(corpse) = 1; /* shadow extraction fields initialized */
  GET_OBJ_WEIGHT(corpse) = GET_WEIGHT(ch) + IS_CARRYING_W(ch);
  GET_OBJ_RENT(corpse) = 100000;
  if (IS_NPC(ch))
    GET_OBJ_TIMER(corpse) = CONFIG_MAX_NPC_CORPSE_TIME;
  else
    GET_OBJ_TIMER(corpse) = CONFIG_MAX_PC_CORPSE_TIME;

  /* transfer character's inventory to the corpse */
  corpse->contains = ch->carrying;
  for (o = corpse->contains; o != NULL; o = o->next_content)
    o->in_obj = corpse;
  object_list_new_owner(corpse, NULL);

  /* transfer character's equipment to the corpse */
  for (i = 0; i < NUM_WEARS; i++)
    if (GET_EQ(ch, i)) {
      remove_otrigger(GET_EQ(ch, i), ch);
  if (IS_NPC(ch)) {
      obj_to_obj(unequip_char(ch, i), corpse);
    }

    
  }
/* transfer gold */
  if (!IS_NPC(ch)) {
    long long have = (long long)GET_MONEY(ch);
    dropped_gold = have / 10;
    if (dropped_gold > 0) {
      money = create_money((int)dropped_gold, 0);
      obj_to_obj(money, corpse);
      GET_MONEY(ch) = (long long)GET_MONEY(ch) - dropped_gold;
    }

    /* Death drop summary for items and money. */
    if (inv_dropped > 0 || dropped_gold > 0) {
      send_to_char(ch, "Death penalty: You drop %lld gold and %d inventory item(s) on your corpse.\r\n",
                   dropped_gold, inv_dropped);
    } else {
      send_to_char(ch, "Death penalty: You drop nothing.\r\n");
    }
    } else {
    /* NPC gold drop: roll between gold_min and gold_max and place on corpse. */
    long long gmin = ch->mob_specials.gold_min;
    long long gmax = ch->mob_specials.gold_max;
    if (gmin < 0) gmin = 0;
    if (gmax < gmin) gmax = gmin;
    dropped_gold = (gmax > 0) ? rand_number((int)gmin, (int)gmax) : 0;
    if (dropped_gold > 0) {
      money = create_money((int)dropped_gold, 0);
      obj_to_obj(money, corpse);
    }
  }
ch->carrying = NULL;
  IS_CARRYING_N(ch) = 0;
  IS_CARRYING_W(ch) = 0;

  obj_to_room(corpse, IN_ROOM(ch));
}

/* When ch kills victim */
static void change_alignment(struct char_data *ch, struct char_data *victim)
{
  /* new alignment change algorithm: if you kill a monster with alignment A,
   * you move 1/16th of the way to having alignment -A.  Simple and fast. */
  GET_ALIGNMENT(ch) += (-GET_ALIGNMENT(victim) - GET_ALIGNMENT(ch)) / 16;
}

static int calc_pve_glory(struct char_data *ch, struct char_data *victim)
{
  int diff = abs(GET_LEVEL(victim) - GET_LEVEL(ch));
  int base = rand_number(15, 25);

  if (diff <= 3)
    return base;

  return MAX(0, (base * MAX(0, (8 - diff))) / 5);
}

static int calc_pvp_glory(struct char_data *ch, struct char_data *victim)
{
  int diff = abs(GET_LEVEL(victim) - GET_LEVEL(ch));
  int base = rand_number(800, 1200);

  if (diff <= 3)
    return base;

  return MAX(0, (base * MAX(0, (10 - diff))) / 7);
}

static bool recently_killed_for_glory(struct char_data *killer, struct char_data *victim)
{
  time_t now = time(NULL);

  if (!GET_LAST_PVP_GLORY_TIME(killer))
    return FALSE;

  if (str_cmp(GET_LAST_PVP_GLORY_VICTIM(killer), GET_NAME(victim)))
    return FALSE;

  return (now - GET_LAST_PVP_GLORY_TIME(killer)) < PVP_GLORY_COOLDOWN;
}

static void grant_glory_for_kill(struct char_data *killer, struct char_data *victim)
{
  int glory = 0;

  if (!killer || IS_NPC(killer) || !victim)
    return;

  if (IS_NPC(victim)) {
    glory = calc_pve_glory(killer, victim);
  } else {
    if (recently_killed_for_glory(killer, victim))
      return;

    glory = calc_pvp_glory(killer, victim);
    if (glory > 0) {
      strlcpy(GET_LAST_PVP_GLORY_VICTIM(killer), GET_NAME(victim), sizeof(GET_LAST_PVP_GLORY_VICTIM(killer)));
      GET_LAST_PVP_GLORY_TIME(killer) = time(NULL);
    }
  }

  if (glory <= 0)
    return;

  GET_GLORY(killer) += glory;
  send_to_char(killer, "You gain %s%d%s %sGlory%s.\r\n",
               CBYEL(killer, C_NRM), glory, CCNRM(killer, C_NRM),
               CBCYN(killer, C_NRM), CCNRM(killer, C_NRM));
}

void death_cry(struct char_data *ch)
{
  int door;

  act("\tRYour blood freezes as you hear $n's death cry.\tn", FALSE, ch, 0, 0, TO_ROOM);

  for (door = 0; door < DIR_COUNT; door++)
    if (CAN_GO(ch, door))
      send_to_room(world[IN_ROOM(ch)].dir_option[door]->to_room, "\tRYour blood freezes as you hear someone's death cry.\tn\r\n");
}

void raw_kill(struct char_data * ch, struct char_data * killer)
{
struct char_data *i;

  if (FIGHTING(ch))
    stop_fighting(ch);

  while (ch->affected)
    affect_remove(ch, ch->affected);

  /* To make ordinary commands work in scripts.  welcor*/
  GET_POS(ch) = POS_STANDING;

  if (killer) {
    if (death_mtrigger(ch, killer))
      death_cry(ch);
  } else
    death_cry(ch);

  if (killer) {
    if (killer->group) {
      while ((i = (struct char_data *) simple_list(killer->group->members)) != NULL)
        if(IN_ROOM(i) == IN_ROOM(ch)  || (world[IN_ROOM(i)].zone == world[IN_ROOM(ch)].zone)) {
          autoquest_trigger_check(i, ch, NULL, AQ_MOB_KILL);
          quest_kill_trigger_check(i, ch);
          campaign_kill_trigger_check(i, ch);
        }
    } else
    {
      autoquest_trigger_check(killer, ch, NULL, AQ_MOB_KILL);
      quest_kill_trigger_check(killer, ch);
      campaign_kill_trigger_check(killer, ch);
    }
  }

  /* Alert Group if Applicable */
  if (GROUP(ch))
    send_to_group(ch, GROUP(ch), "%s has died.\r\n", GET_NAME(ch));

  update_pos(ch);

  make_corpse(ch);
  ai_actor_event_corpse(ch, IN_ROOM(ch));

  /* NPCs: original behavior. PCs: do NOT extract (extraction puts them into the menu).
     Instead, respawn them at the mortal start room and keep the descriptor playing. */
  if (IS_NPC(ch)) {
    extract_char(ch);
  } else {
    /* Clear combat state and ensure valid vitals. */
    GET_HIT(ch) = MAX(1, GET_HIT(ch));
    GET_POS(ch) = POS_RESTING;

    if (IN_ROOM(ch) != NOWHERE)
      char_from_room(ch);
    char_to_room(ch, r_mortal_start_room);

    /* Optional: clear target and stop any lingering fighting. */
    if (FIGHTING(ch))
      stop_fighting(ch);

    look_at_room(ch, 0);
    send_to_char(ch, "You have died. Your spirit reforms at the temple.\r\n");
  }
  if (killer) {
    autoquest_trigger_check(killer, NULL, NULL, AQ_MOB_SAVE);
    autoquest_trigger_check(killer, NULL, NULL, AQ_ROOM_CLEAR);
    if (affected_by_spell(killer, SPELL_SHADOW_REGENESIS))
      GET_MANA(killer) = MIN(effective_max_mana(killer), GET_MANA(killer) + (GET_LEVEL(killer) / 2));
    if (GET_SKILL(killer, SKILL_SHADOW_SURGE) > 0 && count_shadow_servants_for(killer) > 0) {
      struct affected_type af;
      int stacks = 0;
      struct affected_type *taf;
      for (taf = killer->affected; taf; taf = taf->next)
        if (taf->spell == SKILL_SHADOW_SURGE && taf->location == APPLY_DAMROLL)
          stacks++;
      if (stacks < 3) {
        new_affect(&af);
        af.spell = SKILL_SHADOW_SURGE;
        af.duration = 2 + (GET_LEVEL(killer) / 10);
        af.location = APPLY_DAMROLL;
        af.modifier = 2;
        affect_join(killer, &af, FALSE, FALSE, TRUE, FALSE);
      }
    }
  }
}

void die(struct char_data * ch, struct char_data * killer)
{
  if (return_extracted_shadow_to_storage(ch))
    return;

  if (killer && killer != ch && !IS_NPC(ch) && !IS_NPC(killer) && GET_LEVEL(killer) < LVL_IMMORT && GET_BOUNTY(ch) > 0) {
    long long reward = GET_BOUNTY(ch);
    char reward_buf[64];

    format_gold_as_currency(reward_buf, sizeof(reward_buf), reward);
    SET_BOUNTY(ch, 0);
    increase_money_gold(killer, reward);
    save_char(ch);
    save_char(killer);
    send_to_char(killer, "You claim %s for the bounty on %s.\r\n", reward_buf, GET_NAME(ch));
  }
    /* Death penalty: lose 1-10% of TNL on a level-based curve. */
  if (!IS_NPC(ch) && GET_LEVEL(ch) < LVL_IMMORT) {
    long long tnl, loss;
    int pct, max_mortal_level;
    long long num, den, den2;

    /* TNL = exp needed for next level, clamped */
    tnl = (long long)level_exp(GET_CLASS(ch), GET_LEVEL(ch) + 1) - (long long)GET_EXP(ch);
    if (tnl < 0)
      tnl = 0;

    max_mortal_level = LVL_IMMORT - 1;

    /* Quadratic curve: level 1 => 1%, top mortal => 10% */
    if (GET_LEVEL(ch) <= 1 || max_mortal_level <= 1) {
      pct = 1;
    } else {
      num = (long long)(GET_LEVEL(ch) - 1);
      den = (long long)(max_mortal_level - 1);
      den2 = den * den;
      pct = 1 + (int)((9LL * num * num + den2 / 2) / den2);
      pct = MIN(10, MAX(1, pct));
    }

    /* ceil(tnl * pct / 100) */
    loss = (tnl * pct + 99) / 100;

    send_to_char(ch, "Death penalty: You lose %lld experience.\r\n", loss);

    /* gain_exp uses int; clamp to int range */
    if (loss > 2147483647LL) loss = 2147483647LL;
    gain_exp(ch, (int)(-loss));
  } else {
    gain_exp(ch, -(GET_EXP(ch) / 2));
  }

  if (!IS_NPC(ch)) {
    REMOVE_BIT_AR(PLR_FLAGS(ch), PLR_KILLER);
    REMOVE_BIT_AR(PLR_FLAGS(ch), PLR_THIEF);
  }

  /*
   * NPC gold payout to the killer (single, authoritative location).
   * Uses gold_min/gold_max if set, otherwise falls back to GET_GOLD(ch).
   * Clears NPC gold so it does not also appear on the corpse.
   */
  if (killer && ch && IS_NPC(ch) && !IS_NPC(killer)) {
    long long gold_gain = 0;
    long long gmin = (long long)ch->mob_specials.gold_min;
    long long gmax = (long long)ch->mob_specials.gold_max;

    if (gmin < 0) gmin = 0;
    if (gmax < 0) gmax = 0;
    if (gmax < gmin) gmax = gmin;

    if (gmin > 0 || gmax > 0) {
      int imin = (gmin > 2147483647LL) ? 2147483647 : (int)gmin;
      int imax = (gmax > 2147483647LL) ? 2147483647 : (int)gmax;
      gold_gain = (imin == imax) ? (long long)imin : (long long)rand_number(imin, imax);
    } else if (GET_GOLD(ch) > 0) {
      gold_gain = (long long)GET_GOLD(ch);
    }

    if (gold_gain > 0) {
      char buf[64];
      format_gold_as_currency(buf, sizeof(buf), gold_gain);
      increase_money_gold(killer, gold_gain);
      send_to_char(killer, "You receive \ty%s\tn from the kill.\r\n", buf);
    }

    /* prevent corpse gold duplication */
    ch->mob_specials.gold_min = 0;
    ch->mob_specials.gold_max = 0;
    SET_GOLD(ch, 0);
  }

  raw_kill(ch, killer);
}

static void perform_group_gain(struct char_data *ch, int base,
			     struct char_data *victim)
{
  int share, hap_share, rare_bonus;

  share = MIN(CONFIG_MAX_EXP_GAIN, MAX(1, base));

  if ((IS_HAPPYHOUR) && (IS_HAPPYEXP))
  {
    /* This only reports the correct amount - the calc is done in gain_exp */
    hap_share = share + (int)((float)share * ((float)HAPPY_EXP / (float)(100)));
    share = MIN(CONFIG_MAX_EXP_GAIN, MAX(1, hap_share));
  }
  if (share > 1)
    send_to_char(ch, "You receive your share of \tYexperience\tn -- \ty%d\tn points.\r\n", share);
  else
    send_to_char(ch, "You receive your share of experience -- one measly little point!\r\n");

  rare_bonus = rare_kill_bonus_for_victim(victim);
  if (rare_bonus > 0) {
    share += rare_bonus;
    send_to_char(ch, "You receive \ty%d\tn '\tyrare kill\tn' \tYexperience\tn bonus.\r\n", rare_bonus);
  }

  gain_exp(ch, share);
  change_alignment(ch, victim);
}

static void group_gain(struct char_data *ch, struct char_data *victim)
{
  int tot_members = 0, base, tot_gain;
  struct char_data *k;
  
  while ((k = (struct char_data *) simple_list(GROUP(ch)->members)) != NULL)
    if (IN_ROOM(ch) == IN_ROOM(k))
      tot_members++;

  /* round up to the nearest tot_members */
  tot_gain = (GET_EXP(victim) / 3) + tot_members - 1;

  /* prevent illegal xp creation when killing players */
  if (!IS_NPC(victim))
    tot_gain = MIN(CONFIG_MAX_EXP_LOSS * 2 / 3, tot_gain);

  if (tot_members >= 1)
    base = MAX(1, tot_gain / tot_members);
  else
    base = 0;

  while ((k = (struct char_data *) simple_list(GROUP(ch)->members)) != NULL)
    if (IN_ROOM(k) == IN_ROOM(ch))
      perform_group_gain(k, base, victim);
}

static void solo_gain(struct char_data *ch, struct char_data *victim)
{
  int exp, happy_exp, rare_bonus;

  exp = MIN(CONFIG_MAX_EXP_GAIN, GET_EXP(victim) / 3);

  /* Calculate level-difference bonus */
  if (IS_NPC(ch))
    exp += MAX(0, (exp * MIN(4, (GET_LEVEL(victim) - GET_LEVEL(ch)))) / 8);
  else
    exp += MAX(0, (exp * MIN(8, (GET_LEVEL(victim) - GET_LEVEL(ch)))) / 8);

  exp = MAX(exp, 1);

  if (IS_HAPPYHOUR && IS_HAPPYEXP) {
    happy_exp = exp + (int)((float)exp * ((float)HAPPY_EXP / (float)(100)));
    exp = MAX(happy_exp, 1);
  }

  if (exp > 1)
    send_to_char(ch, "You receive \ty%d\tn \tYexperience\tn points.\r\n", exp);
  else
    send_to_char(ch, "You receive one lousy experience point.\r\n");

  rare_bonus = rare_kill_bonus_for_victim(victim);
  if (rare_bonus > 0) {
    exp += rare_bonus;
    send_to_char(ch, "You receive \ty%d\tn '\tyrare kill\tn' \tYexperience\tn bonus.\r\n", rare_bonus);
  }

  gain_exp(ch, exp);

    change_alignment(ch, victim);
}

static int count_live_mobs_by_vnum(mob_vnum vnum)
{
  int count = 0;
  struct char_data *mob;

  if (vnum == NOBODY)
    return 0;

  for (mob = character_list; mob; mob = mob->next) {
    if (!IS_NPC(mob) || MOB_FLAGGED(mob, MOB_NOTDEADYET))
      continue;

    if (GET_MOB_VNUM(mob) == vnum)
      count++;
  }

  return count;
}

static int rare_kill_bonus_for_count(int live_count)
{
  if (live_count <= 0 || live_count > RARE_KILL_MAX_COUNT)
    return 0;

  return RARE_KILL_BASE_BONUS + ((RARE_KILL_MAX_COUNT - live_count) * RARE_KILL_STEP_BONUS);
}

static int rare_kill_bonus_for_victim(struct char_data *victim)
{
  if (!victim || !IS_NPC(victim))
    return 0;

  return rare_kill_bonus_for_count(count_live_mobs_by_vnum(GET_MOB_VNUM(victim)));
}

static char *replace_string(const char *str, const char *weapon_singular, const char *weapon_plural)
{
  static char buf[256];
  char *cp = buf;

  for (; *str; str++) {
    if (*str == '#') {
      switch (*(++str)) {
      case 'W':
	for (; *weapon_plural; *(cp++) = *(weapon_plural++));
	break;
      case 'w':
	for (; *weapon_singular; *(cp++) = *(weapon_singular++));
	break;
      default:
	*(cp++) = '#';
	break;
      }
    } else
      *(cp++) = *str;

    *cp = 0;
  }				/* For */

  return (buf);
}

/* message for doing damage with a weapon */
static void dam_message(int dam, struct char_data *ch, struct char_data *victim,
		      int w_type)
{
  char *buf;
  int msgnum;

  int pct = 0;
  static struct dam_weapon_type {
    const char *to_room;
    const char *to_char;
    const char *to_victim;
  } dam_weapons[] = {

    /* #w is weapon singular text (used as a noun here: "with your slash"). */

    {
      "$n tries to use $s #w on $N, but misses.",
      "You try to use your #w on $N, but miss.",
      "$n tries to use $s #w on you, but misses."
    },

    {
      "$n 	Ggrazes	n $N with $s #w.",
      "You 	Ggraze	n $N with your #w.",
      "$n 	Ggrazes	n you with $s #w."
    },

    {
      "$n 	Gglances	n $N with $s #w.",
      "You 	Gglance	n $N with your #w.",
      "$n 	Gglances	n you with $s #w."
    },

    {
      "$n 	Whits	n $N with $s #w.",
      "You 	Whit	n $N with your #w.",
      "$n 	Whits	n you with $s #w."
    },

    {
      "$n 	Wstrikes	n $N with $s #w.",
      "You 	Wstrike	n $N with your #w.",
      "$n 	Wstrikes	n you with $s #w."
    },

    {
      "$n 	yslams	n $N with $s #w.",
      "You 	yslam	n $N with your #w.",
      "$n 	yslams	n you with $s #w."
    },

    {
      "$n 	Ycrushes	n $N with $s #w.",
      "You 	Ycrush	n $N with your #w.",
      "$n 	Ycrushes	n you with $s #w."
    },

    {
      "$n \tR- blasts -\tn $N with $s #w!",
      "You \tR- blast -\tn $N with your #w!",
      "$n \tR- blasts -\tn you with $s #w!"
    },

    {
      "$n \tM** shreds **\tn $N with $s #w!",
      "You \tM** shred **\tn $N with your #w!",
      "$n \tM** shreds **\tn you with $s #w!"
    },

    {
      "$n \tR***** pulverizes *****\tn $N with $s #w!!",
      "You \tR***** pulverize *****\tn $N with your #w!!",
      "$n \tR***** pulverizes *****\tn you with $s #w!!"
    }
  };

  w_type -= TYPE_HIT;		/* Change to base of table with text */
  if (dam <= 0) {
    msgnum = 0;
  } else {
    pct = (dam * 100) / MAX(1, GET_MAX_HIT(victim));

    if (pct <= 2)        msgnum = 1;
    else if (pct <= 6)   msgnum = 2;
    else if (pct <= 14)  msgnum = 3;
    else if (pct <= 24)  msgnum = 4;
    else if (pct <= 39)  msgnum = 5;
    else if (pct <= 54)  msgnum = 6;
    else if (pct <= 69)  msgnum = 7;
    else if (pct <= 84)  msgnum = 8;
    else                 msgnum = 9;
  }
/* damage message to onlookers */
  buf = replace_string(dam_weapons[msgnum].to_room,
	  attack_hit_text[w_type].singular, attack_hit_text[w_type].plural);
  act(buf, FALSE, ch, NULL, victim, TO_NOTVICT);

  /* damage message to damager */
  if (GET_LEVEL(ch) >= LVL_IMMORT)
	send_to_char(ch, "(%d) ", dam);
  buf = replace_string(dam_weapons[msgnum].to_char,
	  attack_hit_text[w_type].singular, attack_hit_text[w_type].plural);
  act(buf, FALSE, ch, NULL, victim, TO_CHAR);
  send_to_char(ch, CCNRM(ch, C_CMP));

  /* damage message to damagee */
  if (GET_LEVEL(victim) >= LVL_IMMORT)
    send_to_char(victim, "\tR(%d)", dam);
  buf = replace_string(dam_weapons[msgnum].to_victim,
	  attack_hit_text[w_type].singular, attack_hit_text[w_type].plural);
  act(buf, FALSE, ch, NULL, victim, TO_VICT | TO_SLEEP);
  send_to_char(victim, CCNRM(victim, C_CMP));
}


/*  message for doing damage with a spell or skill. Also used for weapon
 *  damage on miss and death blows. */
int skill_message(int dam, struct char_data *ch, struct char_data *vict,
		      int attacktype)
{
  int i, j, nr;
  struct message_type *msg;

  char wmsg_att[MAX_STRING_LENGTH];
  char wmsg_vic[MAX_STRING_LENGTH];
  char wmsg_room[MAX_STRING_LENGTH];
  struct obj_data *weap = GET_EQ(ch, WEAR_WIELD);

  /* @todo restructure the messages library to a pointer based system as
   * opposed to the current cyclic location system. */
  for (i = 0; i < MAX_MESSAGES; i++) {
    if (fight_messages[i].a_type == attacktype) {      nr = dice(1, fight_messages[i].number_of_attacks);
      for (j = 1, msg = fight_messages[i].msg; (j < nr) && msg; j++)
        msg = msg->next;

      if (!IS_NPC(vict) && (GET_LEVEL(vict) >= LVL_IMPL)) {
        act(msg->god_msg.attacker_msg, FALSE, ch, weap, vict, TO_CHAR);
        act(msg->god_msg.victim_msg, FALSE, ch, weap, vict, TO_VICT);
        act(msg->god_msg.room_msg, FALSE, ch, weap, vict, TO_NOTVICT);
      } else if (dam != 0) {
        /*
         * Don't send redundant color codes for TYPE_SUFFERING & other types
         * of damage without attacker_msg.
         */
        if (GET_POS(vict) == POS_DEAD) {
          if (msg->die_msg.attacker_msg) {
            send_to_char(ch, CCYEL(ch, C_CMP));
            apply_severity_verb(wmsg_att, sizeof(wmsg_att), msg->die_msg.attacker_msg, damage_severity_tier(dam, vict));
            act(wmsg_att, FALSE, ch, weap, vict, TO_CHAR);
            send_to_char(ch, CCNRM(ch, C_CMP));
          }

          send_to_char(vict, CCRED(vict, C_CMP));
          apply_severity_verb(wmsg_vic, sizeof(wmsg_vic), msg->die_msg.victim_msg, damage_severity_tier(dam, vict));
          act(wmsg_vic, FALSE, ch, weap, vict, TO_VICT | TO_SLEEP);
          send_to_char(vict, CCNRM(vict, C_CMP));

          apply_severity_verb(wmsg_room, sizeof(wmsg_room), msg->die_msg.room_msg, damage_severity_tier(dam, vict));
          act(wmsg_room, FALSE, ch, weap, vict, TO_NOTVICT);
        } else {
          if (msg->hit_msg.attacker_msg) {
            send_to_char(ch, CCYEL(ch, C_CMP));
            apply_severity_verb(wmsg_att, sizeof(wmsg_att), msg->hit_msg.attacker_msg, damage_severity_tier(dam, vict));
            act(wmsg_att, FALSE, ch, weap, vict, TO_CHAR);
            send_to_char(ch, CCNRM(ch, C_CMP));
          }

          send_to_char(vict, CCRED(vict, C_CMP));
          apply_severity_verb(wmsg_vic, sizeof(wmsg_vic), msg->hit_msg.victim_msg, damage_severity_tier(dam, vict));
          act(wmsg_vic, FALSE, ch, weap, vict, TO_VICT | TO_SLEEP);
          send_to_char(vict, CCNRM(vict, C_CMP));

          apply_severity_verb(wmsg_room, sizeof(wmsg_room), msg->hit_msg.room_msg, damage_severity_tier(dam, vict));
          act(wmsg_room, FALSE, ch, weap, vict, TO_NOTVICT);
        }
      } else if (ch != vict) {	/* Dam == 0 */
        if (msg->miss_msg.attacker_msg) {
          send_to_char(ch, CCYEL(ch, C_CMP));
          act(msg->miss_msg.attacker_msg, FALSE, ch, weap, vict, TO_CHAR);
          send_to_char(ch, CCNRM(ch, C_CMP));
        }

        send_to_char(vict, CCRED(vict, C_CMP));
        act(msg->miss_msg.victim_msg, FALSE, ch, weap, vict, TO_VICT | TO_SLEEP);
        send_to_char(vict, CCNRM(vict, C_CMP));

        act(msg->miss_msg.room_msg, FALSE, ch, weap, vict, TO_NOTVICT);
      }
      return (1);
    }
  }
  return (0);
}

/* This function returns the following codes:
 *	< 0	Victim died.
 *	= 0	No damage.
 *	> 0	How much damage done. */

/* dual wield offhand system */
#define OFFHAND_DAMAGE_PCT 60

static int g_offhand_attack = 0;

static int can_offhand_attack(struct char_data *ch)
{
  struct obj_data *prim = GET_EQ(ch, WEAR_WIELD);
  struct obj_data *off  = GET_EQ(ch, WEAR_HOLD);

  if (!ch) return 0;
  if (!prim || !off) return 0;
  if (GET_OBJ_TYPE(prim) != ITEM_WEAPON) return 0;
  if (GET_OBJ_TYPE(off)  != ITEM_WEAPON) return 0;

  if (!GET_SKILL(ch, SKILL_DUAL_WIELD)) return 0;
  if (OBJ_FLAGGED(prim, ITEM_TWO_HANDER)) return 0;
  if (!OBJ_FLAGGED(off, ITEM_OFFHAND)) return 0;
  if (GET_EQ(ch, WEAR_SHIELD)) return 0;
  if (GET_OBJ_WEIGHT(off) > GET_OBJ_WEIGHT(prim)) return 0;

  return 1;
}

static int find_affect_modifier_for_flag(struct char_data *ch, int aff_flag, int fallback)
{
  struct affected_type *af;

  if (!ch)
    return fallback;

  for (af = ch->affected; af; af = af->next)
    if (IS_SET_AR(af->bitvector, aff_flag))
      return af->modifier;

  return fallback;
}

static int remove_affects_by_flag(struct char_data *ch, int aff_flag)
{
  struct affected_type *af, *next;
  int removed = 0;

  if (!ch)
    return 0;

  for (af = ch->affected; af; af = next) {
    next = af->next;
    if (IS_SET_AR(af->bitvector, aff_flag)) {
      affect_remove(ch, af);
      removed = 1;
    }
  }

  if (AFF_FLAGGED(ch, aff_flag)) {
    REMOVE_BIT_AR(AFF_FLAGS(ch), aff_flag);
    removed = 1;
  }

  return removed;
}

static void do_offhand_attack(struct char_data *ch, struct char_data *victim)
{
  struct obj_data *prim = GET_EQ(ch, WEAR_WIELD);
  struct obj_data *off  = GET_EQ(ch, WEAR_HOLD);

  if (!can_offhand_attack(ch)) return;
  if (!victim) return;

  ch->equipment[WEAR_WIELD] = off;
  g_offhand_attack = 1;
  hit(ch, victim, TYPE_UNDEFINED);
  g_offhand_attack = 0;
  ch->equipment[WEAR_WIELD] = prim;
}

int damage(struct char_data *ch, struct char_data *victim, int dam, int attacktype)
{
  /* OFFHAND DAMAGE SCALE */
  if (g_offhand_attack) {
    dam = (dam * OFFHAND_DAMAGE_PCT) / 100;
  }

  int damage_type = take_next_damage_type();
  int old_hit = 0;
  int old_band = 0;
  long local_gold = 0, happy_gold = 0;
  char local_buf[256];
  struct char_data *tmp_char;
  struct obj_data *corpse_obj;

  if (GET_POS(victim) <= POS_DEAD) {
    /* This is "normal"-ish now with delayed extraction. -gg 3/15/2001 */
    if (PLR_FLAGGED(victim, PLR_NOTDEADYET) || MOB_FLAGGED(victim, MOB_NOTDEADYET))
      return (-1);

    log("SYSERR: Attempt to damage corpse '%s' in room #%d by '%s'.",
        GET_NAME(victim), GET_ROOM_VNUM(IN_ROOM(victim)), GET_NAME(ch));
    die(victim, ch);
    return (-1);			/* -je, 7/7/92 */
  }

  if (ch && victim != ch && is_owned_follower_target(ch, victim)) {
    if (!IS_NPC(ch))
      send_to_char(ch, "You cannot attack one of your own followers.\r\n");
    return (0);
  }

  /* peaceful rooms */
  if (ch->nr != real_mobile(DG_CASTER_PROXY) &&
      ch != victim && ROOM_FLAGGED(IN_ROOM(ch), ROOM_PEACEFUL)) {
    send_to_char(ch, "This room just has such a peaceful, easy feeling...\r\n");
    return (0);
  }

  /* shopkeeper and MOB_NOKILL protection */
  if (!ok_damage_shopkeeper(ch, victim) || MOB_FLAGGED(victim, MOB_NOKILL)) {
    send_to_char(ch, "This mob is protected.\r\n");
    return (0);
  }

  /* You can't damage an immortal! */
  if (!IS_NPC(victim) && ((GET_LEVEL(victim) >= LVL_IMMORT) && PRF_FLAGGED(victim, PRF_NOHASSLE)))
    dam = 0;

  dam = damage_mtrigger(ch, victim, dam, attacktype);
  if (dam == -1) {
  	return (0);
  }

  if (dam > 0 && IS_WEAPON(attacktype) && victim != ch && AFF_FLAGGED(victim, AFF_PHASE) && rand_number(1, 100) <= 30) {
    act("$N phases out of alignment and your attack passes through harmlessly!", FALSE, ch, 0, victim, TO_CHAR);
    act("$n's attack passes through you as you phase out of alignment!", FALSE, ch, 0, victim, TO_VICT);
    act("$n's attack passes right through $N!", FALSE, ch, 0, victim, TO_NOTVICT);
    dam = 0;
  }
  if (dam > 0 && IS_WEAPON(attacktype) && victim != ch && AFF_FLAGGED(victim, AFF_MIRROR_IMAGE) && rand_number(1, 100) <= 20) {
    act("Your strike shatters one of $N's mirror images!", FALSE, ch, 0, victim, TO_CHAR);
    act("$n strikes one of your mirror images, which shatters!", FALSE, ch, 0, victim, TO_VICT);
    act("$n strikes one of $N's mirror images, which shatters!", FALSE, ch, 0, victim, TO_NOTVICT);
    dam = 0;
  }
  if (dam > 0 && IS_WEAPON(attacktype) && victim != ch &&
      GET_SKILL(victim, SKILL_MONARCH_REFLEXES) > 0 &&
      !affected_by_spell(victim, SKILL_MONARCH_REFLEXES) &&
      rand_number(1, 100) <= 10) {
    struct affected_type af;
    new_affect(&af);
    af.spell = SKILL_MONARCH_REFLEXES;
    af.duration = 100;
    af.location = APPLY_NONE;
    af.modifier = 1;
    affect_join(victim, &af, FALSE, FALSE, FALSE, FALSE);
    act("You evade the opening strike with monarch reflexes!", FALSE, victim, 0, ch, TO_CHAR);
    act("$N slips past your opening strike with terrifying reflexes!", FALSE, ch, 0, victim, TO_CHAR);
    dam = 0;
  }

  if (!IS_NPC(ch) && ch != victim)
    ai_actor_record_room_crime(NULL, ch, MEM_WANTED);

  if (victim != ch) {
    /* Start the attacker fighting the victim */
    if (GET_POS(ch) > POS_STUNNED && (FIGHTING(ch) == NULL))
      set_fighting(ch, victim);

    /* Start the victim fighting the attacker */
    if (GET_POS(victim) > POS_STUNNED && (FIGHTING(victim) == NULL)) {
      set_fighting(victim, ch);
      if (MOB_FLAGGED(victim, MOB_MEMORY) && !IS_NPC(ch))
	remember(victim, ch);
      if (!IS_NPC(ch))
        ai_actor_record_damage(victim, ch, dam);
    }
  }

  /* If you attack a pet, it hates your guts */
  if (victim->master == ch)
    stop_follower(victim);

  /* If the attacker is invisible, he becomes visible */
  if (AFF_FLAGGED(ch, AFF_INVISIBLE) || AFF_FLAGGED(ch, AFF_HIDE))
    appear(ch);

  /* Cut damage in half if victim has sanct, to a minimum 1 */
  if (AFF_FLAGGED(victim, AFF_SANCTUARY) && dam >= 2)
    dam /= 2;

  if (dam > 0 && AFF_FLAGGED(victim, AFF_MARKED)) {
    if (affected_by_spell(victim, SPELL_EXECUTION_MARK) &&
        GET_HIT(victim) * 100 <= GET_MAX_HIT(victim) * 25)
      dam = (dam * 14) / 10;
    else
      dam = (dam * 6) / 5;
  }

  if (dam > 0 && ch && ch != victim && affected_by_spell(ch, SPELL_SHADOW_STEP)) {
    dam = (dam * 110) / 100;
    affect_from_char(ch, SPELL_SHADOW_STEP);
  }
  if (dam > 0 && ch && ch != victim && affected_by_spell(ch, SPELL_SHADOW_EXCHANGE)) {
    affect_from_char(ch, SPELL_SHADOW_EXCHANGE);
  }
  if (dam > 0 && ch && ch != victim && affected_by_spell(ch, SPELL_TOTAL_OCCULTATION)) {
    dam = (dam * 120) / 100;
    affect_from_char(ch, SPELL_TOTAL_OCCULTATION);
  }

  if (dam > 0 && ch && ch != victim && GET_SKILL(ch, SKILL_PREDATORS_ADVANCE) > 0 &&
      GET_HIT(victim) * 100 <= GET_MAX_HIT(victim) * 30)
    dam = (dam * 110) / 100;

  if (dam > 0 && ch && ch != victim && GET_SKILL(ch, SKILL_KILL_WINDOW) > 0 &&
      GET_HIT(victim) * 100 <= GET_MAX_HIT(victim) * 25)
    dam += 4;

  if (dam > 0 && ch && ch != victim && affected_by_spell(ch, SPELL_ASSASSINS_INTENT) &&
      (AFF_FLAGGED(victim, AFF_BLIND) || AFF_FLAGGED(victim, AFF_BLINDED_MAGICAL) ||
       AFF_FLAGGED(victim, AFF_ROOTED) || AFF_FLAGGED(victim, AFF_STUNNED) ||
       AFF_FLAGGED(victim, AFF_MARKED)))
    dam = (dam * 110) / 100;

  if (dam > 0 && ch && ch != victim && affected_by_spell(ch, SPELL_HUNTERS_INSTINCT) &&
      GET_LEVEL(victim) > GET_LEVEL(ch))
    dam = (dam * 110) / 100;

  if (dam > 0 && ch && ch != victim && affected_by_spell(ch, SPELL_DOMINION_OF_SHADOWS) &&
      damage_type == DAM_SHADOW)
    dam = (dam * 105) / 100;

  if (dam > 0 && ch && ch != victim && attacktype > 0 && attacktype <= MAX_SPELLS &&
      GET_SKILL(ch, SKILL_CHAIN_ASSASSAULT) > 0 &&
      affected_by_spell(ch, SKILL_CHAIN_ASSASSAULT) &&
      !IS_SET(spell_info[attacktype].targets, TAR_IGNORE) &&
      IS_SET(spell_info[attacktype].targets, TAR_CHAR_ROOM)) {
    dam = (dam * 110) / 100;
    affect_from_char(ch, SKILL_CHAIN_ASSASSAULT);
  }

  if (dam > 0 && ch && victim &&
      GET_SKILL(ch, SKILL_OVERLORD_PRESENCE) > 0 &&
      IS_NPC(victim) && GET_LEVEL(ch) >= GET_LEVEL(victim) + 10)
    dam += 2;

  if (dam > 0 && ch && IS_NPC(ch) && ch->master && is_shadow_servant_for(ch->master, ch)) {
    if (GET_SKILL(ch->master, SKILL_SHADOW_COMMANDER) > 0)
      dam = (dam * 110) / 100;
    if (affected_by_spell(ch->master, SPELL_DOMINION_OF_SHADOWS))
      dam = (dam * 110) / 100;
    if (room_has_effect(&world[IN_ROOM(ch)], ROOM_EFFECT_SHADOW_DOMAIN))
      dam = (dam * 110) / 100;
    if (GET_SKILL(ch->master, SKILL_LEGION_MASTERY) > 0 &&
        count_shadow_servants_for(ch->master) >= 3)
      dam = (dam * 105) / 100;
  }
  if (dam > 0 && ch && IS_NPC(ch) && ch->master &&
      GET_SKILL(ch->master, SKILL_UNDEAD_COMMAND) > 0 &&
      GET_CLASS(ch) == CLASS_UNDEAD) {
    dam = (dam * 110) / 100;
  }

  if (dam > 0 && ch && ch != victim && affected_by_spell(ch, SPELL_TIME_STOP)) {
    struct char_data *tch;
    affect_from_char(ch, SPELL_TIME_STOP);
    for (tch = world[IN_ROOM(ch)].people; tch; tch = tch->next_in_room) {
      if (tch == ch)
        continue;
      GET_WAIT_STATE(tch) = 0;
    }
    send_to_char(ch, "Your assault shatters the frozen moment.\r\n");
    act("Time lurches back into motion around $n.", TRUE, ch, 0, 0, TO_ROOM);
  }

  if (dam > 0 && AFF_FLAGGED(victim, AFF_STONESKIN) && IS_WEAPON(attacktype)) {
    dam = MAX(1, dam - 5);
  }

  if (dam > 0 && IS_WEAPON(attacktype)) {
    int armor = compute_armor(victim);
    dam = (dam * 100) / (100 + armor);
  }

  if (dam > 0) {
    if (affected_by_spell(victim, SPELL_BODY_OF_EFFULGENT_BERYL) &&
        attacktype > 0 && IS_WEAPON(attacktype)) {
      dam = (dam * 75) / 100;
    }
    if (affected_by_spell(victim, SPELL_CRYSTAL_BODY) &&
        (damage_type == DAM_FIRE || damage_type == DAM_COLD ||
         damage_type == DAM_LIGHTNING || damage_type == DAM_ACID)) {
      dam = (dam * 75) / 100;
    }
    if (affected_by_spell(victim, SPELL_PANTHEON) &&
        (damage_type == DAM_HOLY || damage_type == DAM_SHADOW || damage_type == DAM_ARCANE))
      dam = (dam * 75) / 100;
    if (AFF_FLAGGED(victim, AFF_ELEMENTAL_WARD_FIRE) && damage_type == DAM_FIRE)
      dam = (dam * 6) / 10;
    else if (AFF_FLAGGED(victim, AFF_ELEMENTAL_WARD_COLD) && damage_type == DAM_COLD)
      dam = (dam * 6) / 10;
    else if (AFF_FLAGGED(victim, AFF_ELEMENTAL_WARD_LIGHTNING) && damage_type == DAM_LIGHTNING)
      dam = (dam * 6) / 10;
    else if (AFF_FLAGGED(victim, AFF_ELEMENTAL_WARD_ACID) && damage_type == DAM_ACID)
      dam = (dam * 6) / 10;
    if (affected_by_spell(victim, SPELL_SHADOW_ARMOR) &&
        (damage_type == DAM_SHADOW || damage_type == DAM_FORCE))
      dam = (dam * 8) / 10;
  }

  if (dam > 0 && AFF_FLAGGED(victim, AFF_WARDED) &&
      attacktype > 0 && attacktype <= MAX_SPELLS)
    dam = (dam * 7) / 10;

  if (dam > 0 && damage_type == DAM_LIGHTNING && AFF_FLAGGED(victim, AFF_STATIC)) {
    dam = (dam * 125) / 100;
    if (remove_affects_by_flag(victim, AFF_STATIC))
      send_to_char(victim, "The static charge around you dissipates.\r\n");
  }

  if (dam > 0 && ch && affected_by_spell(ch, SPELL_DESPAIR_AURA) &&
      GET_SKILL(ch, SKILL_DREAD_DOMINION) > 0 &&
      (damage_type == DAM_SHADOW || damage_type == DAM_NECROTIC))
    dam = (dam * 105) / 100;


  /* Melee crits (only weapon attacks) */
  if (dam > 0 && ch && victim && ch != victim && IS_WEAPON(attacktype)) {
    if (GET_SKILL(ch, SKILL_CHAIN_ASSASSAULT) > 0) {
      struct affected_type af;
      new_affect(&af);
      af.spell = SKILL_CHAIN_ASSASSAULT;
      af.duration = 1;
      af.location = APPLY_NONE;
      af.modifier = 1;
      affect_join(ch, &af, FALSE, FALSE, FALSE, FALSE);
    }
    int mult = 200;
    if (crit_check_melee(ch, &mult)) {
      dam = (dam * mult) / 100;
      crit_show_banner(ch, victim, mult);
    }
  }

  /* Check for PK if this is not a PK MUD */
  if (!CONFIG_PK_ALLOWED) {
    check_killer(ch, victim);
    if (PLR_FLAGGED(ch, PLR_KILLER) && (ch != victim))
      dam = 0;
  }

  /* Set the maximum damage per round and subtract the hit points */
  dam = MAX(MIN(dam, 1000), 0);
  if (dam > 0 && AFF_FLAGGED(victim, AFF_SHIELDED)) {
    struct affected_type *af;
    for (af = victim->affected; af; af = af->next) {
      if (IS_SET_AR(af->bitvector, AFF_SHIELDED)) {
        int absorbed = MIN(dam, MAX(0, af->modifier));
        af->modifier = MAX(0, af->modifier - absorbed);
        dam -= absorbed;
        if (absorbed > 0) {
          send_to_char(victim, "Your protective shield absorbs %d damage.\r\n", absorbed);
          if (ch && ch != victim)
            send_to_char(ch, "%s's shield absorbs part of your attack.\r\n", GET_NAME(victim));
        }
        if (af->modifier <= 0) {
          act("Your protective shield shatters!", FALSE, victim, 0, 0, TO_CHAR);
          act("$n's protective shield shatters!", FALSE, victim, 0, 0, TO_ROOM);
          affect_remove(victim, af);
        }
        break;
      }
    }
  }

  if (dam > 0 && AFF_FLAGGED(victim, AFF_DEATH_WARD) && GET_HIT(victim) - dam < 1) {
    struct affected_type *af;
    for (af = victim->affected; af; af = af->next) {
      if (IS_SET_AR(af->bitvector, AFF_DEATH_WARD)) {
        affect_remove(victim, af);
        break;
      }
    }
    send_to_char(victim, "A death ward flares and saves you from a killing blow!\r\n");
    act("A death ward flares around $n, snatching $m from death!", TRUE, victim, 0, 0, TO_ROOM);
    dam = MAX(0, GET_HIT(victim) - 1);
  }

  if (dam > 0 && affected_by_spell(victim, SPELL_UNDYING_WILL) && GET_HIT(victim) - dam < 1) {
    struct affected_type af;
    affect_from_char(victim, SPELL_UNDYING_WILL);
    GET_HIT(victim) = 1;
    GET_MANA(victim) = MIN(GET_MAX_MANA(victim), GET_MANA(victim) + (GET_LEVEL(victim) * 2));
    new_affect(&af);
    af.spell = SPELL_UNDYING_WILL;
    af.duration = 1;
    af.location = APPLY_NONE;
    SET_BIT_AR(af.bitvector, AFF_STUNNED);
    affect_join(victim, &af, FALSE, FALSE, FALSE, FALSE);
    send_to_char(victim, "Your undying will drags you back from the brink!\r\n");
    act("$n refuses death through sheer will!", TRUE, victim, 0, 0, TO_ROOM);
    dam = 0;
  }

  old_hit = GET_HIT(victim);
  old_band = victim_condition_band(victim);
  GET_HIT(victim) -= dam;

  /* Gain exp for the hit */
  if (ch != victim)

  update_pos(victim);

  /* skill_message sends a message from the messages file in lib/misc.
   * dam_message just sends a generic "You hit $n extremely hard.".
   * skill_message is preferable to dam_message because it is more
   * descriptive.
   *
   * If we are _not_ attacking with a weapon (i.e. a spell), always use
   * skill_message. If we are attacking with a weapon: If this is a miss or a
   * death blow, send a skill_message if one exists; if not, default to a
   * dam_message. Otherwise, always send a dam_message. */
  if (!IS_WEAPON(attacktype)) {
    /* Always add a short severity verb line for spells, skills, DoTs. */
    int shown = skill_message(dam, ch, victim, attacktype);
    if (!shown && dam > 0 && IN_ROOM(victim) != NOWHERE) {
      static const char *const v3[] = {
        "misses", "grazes", "glances", "hits", "strikes", "slams", "crushes", "blasts", "shreds", "pulverizes"
      };
      static const char *const v3_past[] = {
        "missed", "grazed", "glanced", "hit", "struck", "slammed", "crushed", "blasted", "shredded", "pulverized"
      };
      int tier = damage_severity_tier(dam, victim);
      const char *col = severity_color(tier);
      const char *pre = severity_impact_wrap_open(tier);
      const char *post = severity_impact_wrap_close(tier);
      if (tier < 0) tier = 0;
      if (tier > 9) tier = 9;
      if (ch) {
        char to_char[160], to_vict[160], to_room[160];
        snprintf(to_char, sizeof(to_char), "Your magic %s%s%s%s\tn $N.", col, pre, v3[tier], post);
        snprintf(to_vict, sizeof(to_vict), "$n's magic %s%s%s%s\tn you.", col, pre, v3[tier], post);
        snprintf(to_room, sizeof(to_room), "$n's magic %s%s%s%s\tn $N.", col, pre, v3[tier], post);
        act(to_room, FALSE, ch, NULL, victim, TO_NOTVICT);
        act(to_char, FALSE, ch, NULL, victim, TO_CHAR);
        act(to_vict, FALSE, ch, NULL, victim, TO_VICT | TO_SLEEP);
      } else {
        send_to_char(victim, "Lingering magic %s%s%s%s\tn you.\r\n", col, pre, v3[tier], post);
        {
          char roommsg[160];
          snprintf(roommsg, sizeof(roommsg), "$n is %s%s%s%s\tn by lingering magic.", col, pre, v3_past[tier], post);
          act(roommsg, TRUE, victim, 0, 0, TO_ROOM);
        }
      }
    } else if (!shown) {
      /* If a spell has no messages and did 0 damage, still show something. */
      if (ch) {
        act("Your magic misses $N.", FALSE, ch, NULL, victim, TO_CHAR);
        act("$n's magic misses you.", FALSE, ch, NULL, victim, TO_VICT | TO_SLEEP);
        act("$n's magic misses $N.", FALSE, ch, NULL, victim, TO_NOTVICT);
      } else {
        send_to_char(victim, "You suffer.\r\n");
        act("$n suffers.", TRUE, victim, 0, 0, TO_ROOM);
      }
    }
  } else {
    if (GET_POS(victim) == POS_DEAD || dam == 0) {
      if (!skill_message(dam, ch, victim, attacktype))
	dam_message(dam, ch, victim, attacktype);
    } else {
      dam_message(dam, ch, victim, attacktype);
    }
  }
  if (ch && victim && ch != victim && GET_POS(victim) > POS_DEAD && dam > 0) {
    int new_band = victim_condition_band(victim);
    int tier = damage_severity_tier(dam, victim);

    if ((new_band > old_band && tier >= 4) || tier >= 7) {
      send_to_char(ch, "\tC%s %s\tn\r\n", PERS(victim, ch), victim_condition_text(new_band));
      if (GET_LEVEL(ch) >= LVL_IMMORT && CONFIG_DEBUG_MODE >= NRM)
        send_to_char(ch, "\tD(cond: %d%% -> %d%%)\tn\r\n",
                     (old_hit * 100) / MAX(1, GET_MAX_HIT(victim)),
                     (GET_HIT(victim) * 100) / MAX(1, GET_MAX_HIT(victim)));
    }
  }


  /* Use send_to_char -- act() doesn't send message if you are DEAD. */
  switch (GET_POS(victim)) {
  case POS_MORTALLYW:
    act("$n is mortally wounded, and will die soon, if not aided.", TRUE, victim, 0, 0, TO_ROOM);
    send_to_char(victim, "You are mortally wounded, and will die soon, if not aided.\r\n");
    break;
  case POS_INCAP:
    act("$n is incapacitated and will slowly die, if not aided.", TRUE, victim, 0, 0, TO_ROOM);
    send_to_char(victim, "You are incapacitated and will slowly die, if not aided.\r\n");
    break;
  case POS_STUNNED:
    act("$n is stunned, but will probably regain consciousness again.", TRUE, victim, 0, 0, TO_ROOM);
    send_to_char(victim, "You're stunned, but will probably regain consciousness again.\r\n");
    break;
  case POS_DEAD:
    act("$n is dead!  \tYR.I.P.\tn", FALSE, victim, 0, 0, TO_ROOM);
    send_to_char(victim, "You are dead!  Sorry...\r\n");
    break;

  default:			/* >= POSITION SLEEPING */
    if (dam > (GET_MAX_HIT(victim) / 4))
      send_to_char(victim, "That really did HURT!\r\n");

    if (GET_HIT(victim) < (GET_MAX_HIT(victim) / 4)) {
      send_to_char(victim, "%sYou wish that your wounds would stop BLEEDING so much!%s\r\n",
		CCRED(victim, C_SPR), CCNRM(victim, C_SPR));
      if (ch != victim && MOB_FLAGGED(victim, MOB_WIMPY) && !MOB_FLAGGED(victim, MOB_HELPER))
	do_flee(victim, NULL, 0, 0);
    }
    if (!IS_NPC(victim) && GET_WIMP_LEV(victim) && (victim != ch) &&
	GET_HIT(victim) < GET_WIMP_LEV(victim) && GET_HIT(victim) > 0) {
      send_to_char(victim, "You wimp out, and attempt to flee!\r\n");
      do_flee(victim, NULL, 0, 0);
    }
    break;
  }

  /* Help out poor linkless people who are attacked */
  if (!IS_NPC(victim) && !(victim->desc) && GET_POS(victim) > POS_STUNNED) {
    do_flee(victim, NULL, 0, 0);
    if (!FIGHTING(victim)) {
      act("$n is rescued by divine forces.", FALSE, victim, 0, 0, TO_ROOM);
      GET_WAS_IN(victim) = IN_ROOM(victim);
      char_from_room(victim);
      char_to_room(victim, 0);
    }
  }

  /* stop someone from fighting if they're stunned or worse */
  if (GET_POS(victim) <= POS_STUNNED && FIGHTING(victim) != NULL)
    stop_fighting(victim);

  /* Uh oh.  Victim died. */
  if (GET_POS(victim) == POS_DEAD) {
    if (ch != victim && ch && !IS_NPC(ch) &&
        GET_SKILL(ch, SKILL_TACTICAL_SPELL_MEMORY) > 0 &&
        attacktype > 0 && attacktype <= MAX_SPELLS) {
      int mana_refund = MAX(1, GET_LEVEL(ch) / 4);
      GET_MANA(ch) = MIN(effective_max_mana(ch), GET_MANA(ch) + mana_refund);
      send_to_char(ch, "You retain tactical spell memory and recover %d mana.\r\n", mana_refund);
    }

    if (ch != victim && (IS_NPC(victim) || victim->desc)) {
      if (GROUP(ch))
	group_gain(ch, victim);
      else
        solo_gain(ch, victim);
    }

    if (!IS_NPC(victim)) {
      mudlog(BRF, MAX(LVL_IMMORT, MAX(GET_INVIS_LEV(ch), GET_INVIS_LEV(victim))),
        TRUE, "%s killed by %s at %s", GET_NAME(victim), GET_NAME(ch), world[IN_ROOM(victim)].name);
      if (MOB_FLAGGED(ch, MOB_MEMORY))
        forget(ch, victim);
    }
    if (ch != victim)
      grant_glory_for_kill(ch, victim);
    /* Cant determine GET_GOLD on corpse, so do now and store */
    if (IS_NPC(victim)) {
      if ((IS_HAPPYHOUR) && (IS_HAPPYGOLD))
      {
        happy_gold = (long)(GET_GOLD(victim) * (((float)(HAPPY_GOLD))/(float)100));
        happy_gold = MAX(0, happy_gold);
        increase_gold(victim, happy_gold);
      }
      local_gold = GET_GOLD(victim);
      sprintf(local_buf,"%ld", (long)local_gold);
    }

    die(victim, ch);
    if (GROUP(ch) && (local_gold > 0) && PRF_FLAGGED(ch, PRF_AUTOSPLIT) ) {
      generic_find("corpse", FIND_OBJ_ROOM, ch, &tmp_char, &corpse_obj);
      if (corpse_obj) {
        do_get(ch, "all.coin corpse", 0, 0);
        do_split(ch, local_buf, 0, 0);
      }
      /* need to remove the gold from the corpse */
    } else if (!IS_NPC(ch) && (ch != victim) && PRF_FLAGGED(ch, PRF_AUTOGOLD)) {
      do_get(ch, "all.coin corpse", 0, 0);
    }
    if (!IS_NPC(ch) && (ch != victim) && PRF_FLAGGED(ch, PRF_AUTOLOOT)) {
      do_get(ch, "all corpse", 0, 0);
    }
    if (IS_NPC(victim) && !IS_NPC(ch) && PRF_FLAGGED(ch, PRF_AUTOSAC)) {
      do_sac(ch,"corpse",0,0);
    }
    return (-1);
  }
  return (dam);
}

/* Calculate the THAC0 of the attacker. 'victim' currently isn't used but you
 * could use it for special cases like weapons that hit evil creatures easier
 * or a weapon that always misses attacking an animal. */
static int compute_thaco(struct char_data *ch, struct char_data *victim)
{
  int calc_thaco;
  int situational_hit_bonus = 0;
  int level_gap_bonus = 0;

  if (!IS_NPC(ch))
    calc_thaco = thaco(GET_CLASS(ch), GET_LEVEL(ch));
  else		/* THAC0 for monsters is set in the HitRoll */
    calc_thaco = 20;
  calc_thaco -= str_app[STRENGTH_APPLY_INDEX(ch)].tohit;
  calc_thaco -= GET_HITROLL(ch);
  calc_thaco -= (int) ((GET_INT(ch) - 13) / 1.5);	/* Intelligence helps! */
  calc_thaco -= (int) ((GET_WIS(ch) - 13) / 1.5);	/* So does wisdom */
  if (victim)
    level_gap_bonus = (GET_LEVEL(ch) - GET_LEVEL(victim)) / 3;
  calc_thaco -= level_gap_bonus;
  if (victim && AFF_FLAGGED(ch, AFF_TRUESIGHT) &&
      (AFF_FLAGGED(victim, AFF_INVISIBLE) || AFF_FLAGGED(victim, AFF_HIDE)))
    situational_hit_bonus = 10;
  calc_thaco -= situational_hit_bonus;
  return MAX(1, calc_thaco);
}

void hit(struct char_data *ch, struct char_data *victim, int type)
{
  struct obj_data *wielded = GET_EQ(ch, WEAR_WIELD);
  int w_type, attacker_hit, defender_evasion, hit_chance, dam;
  int thaco_legacy, victim_ac_legacy, diceroll_legacy;
  int attacker_level, victim_level, hitroll_bonus, stat_bonus, mental_bonus;
  int level_gap_bonus, situational_bonus, defender_level_bonus;
  int final_hit_roll;

  /* Check that the attacker and victim exist */
  if (!ch || !victim) return;
  if (is_owned_follower_target(ch, victim)) {
    if (!IS_NPC(ch))
      send_to_char(ch, "You cannot attack one of your own followers.\r\n");
    return;
  }

  /* check if the character has a fight trigger */
  fight_mtrigger(ch);

  /* Do some sanity checking, in case someone flees, etc. */
  if (IN_ROOM(ch) != IN_ROOM(victim)) {
    if (FIGHTING(ch) && FIGHTING(ch) == victim)
      stop_fighting(ch);
    return;
  }

  /* Find the weapon type (for display purposes only) */
  if (wielded && GET_OBJ_TYPE(wielded) == ITEM_WEAPON)
    w_type = GET_OBJ_VAL(wielded, 3) + TYPE_HIT;
  else {
    if (IS_NPC(ch) && ch->mob_specials.attack_type != 0)
      w_type = ch->mob_specials.attack_type + TYPE_HIT;
    else
      w_type = TYPE_HIT;
  }

  attacker_level = GET_LEVEL(ch);
  victim_level = GET_LEVEL(victim);
  hitroll_bonus = GET_HITROLL(ch);
  stat_bonus = str_app[STRENGTH_APPLY_INDEX(ch)].tohit;
  mental_bonus = (GET_INT(ch) - 10) / 4 + (GET_WIS(ch) - 10) / 4;
  if (attacker_level > victim_level)
    level_gap_bonus = ((attacker_level - victim_level) / 2) + ((attacker_level - victim_level) / 4);
  else if (attacker_level < victim_level)
    level_gap_bonus = (attacker_level - victim_level) / 3;
  else
    level_gap_bonus = 0;
  situational_bonus = (AFF_FLAGGED(ch, AFF_TRUESIGHT) &&
      (AFF_FLAGGED(victim, AFF_INVISIBLE) || AFF_FLAGGED(victim, AFF_HIDE))) ? 6 : 0;

  attacker_hit = 30 + attacker_level + hitroll_bonus + stat_bonus +
                 mental_bonus + level_gap_bonus + situational_bonus;
  defender_evasion = compute_defensive_evasion_value(victim, &defender_level_bonus);
  hit_chance = compute_hit_chance_from_values(attacker_hit, defender_evasion);
  final_hit_roll = rand_number(1, 100);
  thaco_legacy = compute_thaco(ch, victim);
  victim_ac_legacy = compute_armor_class(victim) / 10;
  victim_ac_legacy = MAX(-10, MIN(10, victim_ac_legacy));
  diceroll_legacy = rand_number(1, 20);

  /* report for debugging if necessary */
  if (CONFIG_DEBUG_MODE >= NRM && GET_LEVEL(ch) >= LVL_BUILDER)
    send_to_char(ch,
      "\t1Combat Debug:\r\n"
      "   \t2Attacker Lvl:\t3%d\r\n"
      "   \t2Victim Lvl:\t3%d\r\n"
      "   \t2Accuracy Value:\t3%d\r\n"
      "   \t2Evasion Value:\t3%d\r\n"
      "   \t2Hitroll Contribution:\t3%d\r\n"
      "   \t2Stat Contribution:\t3%d (STR) + %d (INT/WIS)\r\n"
      "   \t2Level-gap Contribution:\t3%d\r\n"
      "   \t2Defender Level Contribution:\t3%d\r\n"
      "   \t2Final Hit Chance:\t3%d%%\r\n"
      "   \t2Final Hit Roll:\t3%d\r\n"
      "   \t2Legacy THAC0 Check:\t3THAC0 %d vs AC %d on d20(%d)\tn\r\n",
      attacker_level, victim_level, attacker_hit, defender_evasion, hitroll_bonus,
      stat_bonus, mental_bonus, level_gap_bonus, defender_level_bonus,
      hit_chance, final_hit_roll, thaco_legacy, victim_ac_legacy, diceroll_legacy);

  dam = (!AWAKE(victim) || final_hit_roll <= hit_chance);

  if (!dam)
    /* the attacker missed the victim */
    damage(ch, victim, 0, type == SKILL_BACKSTAB ? SKILL_BACKSTAB : w_type);
  else {
    /* okay, we know the guy has been hit.  now calculate damage.
     * Start with the damage bonuses: the damroll and strength apply */
    dam = str_app[STRENGTH_APPLY_INDEX(ch)].todam;
    dam += GET_DAMROLL(ch);

    {
    int unarmed_base_roll = 0;
    int unarmed_level_scaling_bonus = 0;
    int level_gap_damage_bonus = 0;

    /* Maybe holding arrow? */
    if (wielded && GET_OBJ_TYPE(wielded) == ITEM_WEAPON) {
      /* Add weapon-based damage if a weapon is being wielded */
      dam += dice(GET_OBJ_VAL(wielded, 1), GET_OBJ_VAL(wielded, 2));
    } else {
      /* If no weapon, add bare hand damage instead */
      if (IS_NPC(ch)) {
        unarmed_base_roll = dice(ch->mob_specials.damnodice, ch->mob_specials.damsizedice);
        dam += unarmed_base_roll;
      } else {
        unarmed_base_roll = dice(MIN(4, 1 + (GET_LEVEL(ch) / 30)),
                                 MIN(7, 2 + (GET_LEVEL(ch) / 20)));
        unarmed_level_scaling_bonus = MAX(0, GET_LEVEL(ch) / 25);
        dam += unarmed_base_roll + unarmed_level_scaling_bonus;
      }
    }

    if (victim && GET_LEVEL(ch) > GET_LEVEL(victim)) {
      level_gap_damage_bonus = MAX(1, (GET_LEVEL(ch) - GET_LEVEL(victim)) / 6);
      dam += level_gap_damage_bonus;
    }

    if (CONFIG_DEBUG_MODE >= NRM && GET_LEVEL(ch) >= LVL_BUILDER)
      send_to_char(ch,
        "\t1Combat Debug:\r\n"
        "   \t2Unarmed Base Damage Roll:\t3%d\r\n"
        "   \t2Unarmed Level Scaling Bonus:\t3%d\r\n"
        "   \t2Level-gap Damage Bonus:\t3%d\tn\r\n",
        unarmed_base_roll, unarmed_level_scaling_bonus, level_gap_damage_bonus);
    }

    /* Include a damage multiplier if victim isn't ready to fight:
     * Position sitting  1.33 x normal
     * Position resting  1.66 x normal
     * Position sleeping 2.00 x normal
     * Position stunned  2.33 x normal
     * Position incap    2.66 x normal
     * Position mortally 3.00 x normal
     * Note, this is a hack because it depends on the particular
     * values of the POSITION_XXX constants. */
    if (GET_POS(victim) < POS_FIGHTING)
      dam *= 1 + (POS_FIGHTING - GET_POS(victim)) / 3;

    /* at least 1 hp damage min per hit */
    dam = MAX(1, dam);

    if (type == SKILL_BACKSTAB)
      damage(ch, victim, dam * backstab_mult(GET_LEVEL(ch)), SKILL_BACKSTAB);
    else
      damage(ch, victim, dam, w_type);
  }

  /* check if the victim has a hitprcnt trigger */
  hitprcnt_mtrigger(victim);
}

static void process_round_effects(void)
{
  struct char_data *i, *next_char;
  room_rnum room;

  for (i = character_list; i; i = next_char) {
    next_char = i->next;
    if (DEAD(i))
      continue;

    if (!IS_NPC(i))
      GET_HP_LAST_ROUND(i) = GET_HIT(i);

    if (!FIGHTING(i) && affected_by_spell(i, SKILL_MONARCH_REFLEXES))
      affect_from_char(i, SKILL_MONARCH_REFLEXES);

    tick_spell_cooldowns(i);

    if (AFF_FLAGGED(i, AFF_BURNING)) {
      set_next_damage_type(DAM_FIRE);
      if (damage(i, i, 5, TYPE_SUFFERING) == -1)
        continue;
    }

    if (affected_by_spell(i, SPELL_MIASMA)) {
      set_next_damage_type(DAM_POISON);
      if (damage(i, i, 5, TYPE_SUFFERING) == -1)
        continue;
    }

    if (affected_by_spell(i, SPELL_TOXIC_CLOUD)) {
      set_next_damage_type(DAM_POISON);
      if (damage(i, i, 6, TYPE_SUFFERING) == -1)
        continue;
      if (rand_number(1, 100) <= 30 && !AFF_FLAGGED(i, AFF_POISON))
        call_magic(i, i, NULL, SPELL_POISON, GET_LEVEL(i), CAST_SPELL);
    }

    if (affected_by_spell(i, SPELL_HELL_FLAME)) {
      set_next_damage_type(DAM_FIRE);
      if (damage(i, i, (GET_LEVEL(i) * 2) + dice(2, MAX(1, GET_LEVEL(i) / 4)), SPELL_HELL_FLAME) == -1)
        continue;
    }

    if (affected_by_spell(i, SPELL_NAPALM)) {
      set_next_damage_type(DAM_FIRE);
      if (damage(i, i, (GET_LEVEL(i) * 2) + dice(2, MAX(1, GET_LEVEL(i) / 4)), SPELL_NAPALM) == -1)
        continue;
      affect_from_char(i, SPELL_NAPALM);
    }

    if (AFF_FLAGGED(i, AFF_ARCANE_LEAK))
      GET_MANA(i) = MAX(0, GET_MANA(i) - 10);

    if (AFF_FLAGGED(i, AFF_BLOODLUST))
      GET_HIT(i) = MAX(1, GET_HIT(i) - 5);

    if (AFF_FLAGGED(i, AFF_REGENERATING)) {
      int regen = find_affect_modifier_for_flag(i, AFF_REGENERATING, 0);
      if (regen > 0)
        GET_HIT(i) = MIN(GET_MAX_HIT(i), GET_HIT(i) + regen);
    }

    if (affected_by_spell(i, SPELL_SOVEREIGNS_STEP))
      GET_MOVE(i) = MIN(GET_MAX_MOVE(i), GET_MOVE(i) + 10);

    if (FIGHTING(i) && AFF_FLAGGED(FIGHTING(i), AFF_MARKED)) {
      GET_MOVE(i) = MIN(GET_MAX_MOVE(i), GET_MOVE(i) + MAX(1, GET_MAX_MOVE(i) / 10));
      if (GET_SKILL(i, SKILL_RELENTLESS_HUNT) > 0)
        GET_MOVE(i) = MIN(GET_MAX_MOVE(i), GET_MOVE(i) + 5);
    }

    if (AFF_FLAGGED(i, AFF_TIME_SNARE))
      WAIT_STATE(i, PULSE_VIOLENCE);

    if (AFF_FLAGGED(i, AFF_FEARFUL) && FIGHTING(i)) {
      int fear_penalty = find_affect_modifier_for_flag(i, AFF_FEARFUL, 0);
      if (fear_penalty <= -10 && rand_number(1, 100) <= 60)
        do_flee(i, NULL, 0, 0);
      else if (affected_by_spell(i, SPELL_CRY_OF_THE_BANSHEE) && rand_number(1, 100) <= 40)
        do_flee(i, NULL, 0, 0);
    }

    if (affected_by_spell(i, SPELL_KINGS_COMMAND))
      remove_affects_by_flag(i, AFF_FEARFUL);

    if ((AFF_FLAGGED(i, AFF_FEARFUL) || AFF_FLAGGED(i, AFF_ROOTED) || AFF_FLAGGED(i, AFF_STUNNED))) {
      struct char_data *src;
      for (src = world[IN_ROOM(i)].people; src; src = src->next_in_room) {
        if (GET_SKILL(src, SKILL_SOVEREIGN_PRESSURE) <= 0)
          continue;
        if (FIGHTING(src) != i && FIGHTING(i) != src)
          continue;
        {
          struct affected_type af;
          new_affect(&af);
          af.spell = SKILL_SOVEREIGN_PRESSURE;
          af.duration = 1;
          af.location = APPLY_SAVING_SPELL;
          af.modifier = 1;
          affect_join(i, &af, FALSE, FALSE, FALSE, FALSE);
        }
        break;
      }
    }

    if (!IS_NPC(i) && GET_SKILL(i, SKILL_LEGION_MASTERY) > 0 &&
        count_shadow_servants_for(i) >= 3) {
      struct affected_type af;
      new_affect(&af);
      af.spell = SKILL_LEGION_MASTERY;
      af.duration = 1;
      af.location = APPLY_SAVING_SPELL;
      af.modifier = -2;
      affect_join(i, &af, FALSE, FALSE, FALSE, FALSE);
    }

    if (IS_NPC(i) && i->master && is_shadow_servant_for(i->master, i) &&
        GET_SKILL(i->master, SKILL_SHADOW_COMMANDER) > 0) {
      struct affected_type af;
      new_affect(&af);
      af.spell = SKILL_SHADOW_COMMANDER;
      af.duration = 1;
      af.location = APPLY_HITROLL;
      af.modifier = 2;
      affect_join(i, &af, FALSE, FALSE, FALSE, FALSE);
      new_affect(&af);
      af.spell = SKILL_SHADOW_COMMANDER;
      af.duration = 1;
      af.location = APPLY_DAMROLL;
      af.modifier = 2;
      affect_join(i, &af, FALSE, FALSE, FALSE, FALSE);
      GET_HIT(i) = MIN(GET_MAX_HIT(i), GET_HIT(i) + MAX(1, GET_MAX_HIT(i) / 10));
    }

    if (IS_NPC(i) && i->master && GET_SKILL(i->master, SKILL_LEGION_MASTERY) > 0 &&
        count_shadow_servants_for(i->master) >= 3 && AFF_FLAGGED(i, AFF_CHARM)) {
      struct affected_type af;
      new_affect(&af);
      af.spell = SKILL_LEGION_MASTERY;
      af.duration = 1;
      af.location = APPLY_HITROLL;
      af.modifier = 1;
      affect_join(i, &af, FALSE, FALSE, FALSE, FALSE);
      new_affect(&af);
      af.spell = SKILL_LEGION_MASTERY;
      af.duration = 1;
      af.location = APPLY_DAMROLL;
      af.modifier = 1;
      affect_join(i, &af, FALSE, FALSE, FALSE, FALSE);
    }

    if (affected_by_spell(i, SPELL_DESPAIR_AURA)) {
      struct char_data *tch;
      if (GET_SKILL(i, SKILL_DREAD_DOMINION) > 0) {
        struct affected_type af;
        new_affect(&af);
        af.spell = SKILL_DREAD_DOMINION;
        af.duration = 1;
        af.location = APPLY_SAVING_SPELL;
        af.modifier = 2;
        affect_join(i, &af, FALSE, FALSE, FALSE, FALSE);
        new_affect(&af);
        af.spell = SKILL_DREAD_DOMINION;
        af.duration = 1;
        af.location = APPLY_AC;
        af.modifier = 2;
        affect_join(i, &af, FALSE, FALSE, FALSE, FALSE);
      }
      for (tch = world[IN_ROOM(i)].people; tch; tch = tch->next_in_room) {
        struct affected_type af;
        if (tch == i)
          continue;
        if (!IS_NPC(tch) && !IS_NPC(i) && !CONFIG_PK_ALLOWED)
          continue;
        if (!IS_NPC(i) && IS_NPC(tch) && AFF_FLAGGED(tch, AFF_CHARM))
          continue;
        new_affect(&af);
        af.spell = SKILL_DREAD_DOMINION;
        af.duration = 1;
        af.location = APPLY_HITROLL;
        af.modifier = -4;
        affect_join(tch, &af, FALSE, FALSE, FALSE, FALSE);
        new_affect(&af);
        af.spell = SKILL_DREAD_DOMINION;
        af.duration = 1;
        af.location = APPLY_SAVING_SPELL;
        af.modifier = -2;
        affect_join(tch, &af, FALSE, FALSE, FALSE, FALSE);
        if (FIGHTING(tch) && GET_LEVEL(tch) < GET_LEVEL(i) - 8 && rand_number(1, 100) <= 20)
          do_flee(tch, NULL, 0, 0);
      }
    }

    if (IS_NPC(i) && GET_SUMMON_TIMER(i) > 0) {
      GET_SUMMON_TIMER(i)--;
      if (GET_SUMMON_TIMER(i) <= 0) {
        struct char_data *owner = i->master;
        int slot = extracted_shadow_slot(i);
        if (owner && !IS_NPC(owner) && slot >= 0 && slot < MAX_SHADOW_ROSTER &&
            SHADOW_SLOT_OCCUPIED(owner, slot)) {
          SHADOW_SLOT_ACTIVE(owner, slot) = 0;
          save_char(owner);
        }
        act("$n flickers and vanishes as the summoning fades.", FALSE, i, 0, 0, TO_ROOM);
        prepare_shadow_servant_for_removal(i);
        extract_char(i);
      }
    }
  }

  for (room = 0; room <= top_of_world; room++)
    room_tick_effects(&world[room]);
}

/* control the fights going on.  Called every 2 seconds from comm.c. */
void perform_violence(void)
{
  struct char_data *ch, *tch;

  if (!violence_tick_running) {
    violence_tick_running = TRUE;
    process_round_effects();
    violence_tick_running = FALSE;
  }

  for (ch = combat_list; ch; ch = next_combat_list) {
    next_combat_list = ch->next_fighting;

    if (FIGHTING(ch) == NULL || IN_ROOM(ch) != IN_ROOM(FIGHTING(ch))) {
      stop_fighting(ch);
      continue;
    }

    if (IS_NPC(ch)) {
      if (GET_MOB_WAIT(ch) > 0) {
        GET_MOB_WAIT(ch) -= PULSE_VIOLENCE;
        continue;
      }
      GET_MOB_WAIT(ch) = 0;
      if (GET_POS(ch) < POS_FIGHTING) {
        GET_POS(ch) = POS_FIGHTING;
        act("$n scrambles to $s feet!", TRUE, ch, 0, 0, TO_ROOM);
      }
    }

    if (GET_POS(ch) < POS_FIGHTING) {
      send_to_char(ch, "You can't fight while sitting!!\r\n");
      continue;
    }

    if (AFF_FLAGGED(ch, AFF_STUNNED)) {
      send_to_char(ch, "You are stunned and cannot act this round!\r\n");
      continue;
    }

    if (affected_by_spell(ch, SPELL_CONFUSION) && rand_number(1, 100) <= 33) {
      send_to_char(ch, "You falter in confusion and lose your action this round!\r\n");
      continue;
    }

    if (GROUP(ch) && GROUP(ch)->members && GROUP(ch)->members->iSize) {
      struct iterator_data Iterator;

      tch = (struct char_data *) merge_iterator(&Iterator, GROUP(ch)->members);
      for (; tch ; tch = next_in_list(&Iterator)) {
        if (tch == ch)
          continue;
        if (!IS_NPC(tch) && !PRF_FLAGGED(tch, PRF_AUTOASSIST))
          continue;
        if (IN_ROOM(ch) != IN_ROOM(tch))
          continue;
        if (FIGHTING(tch))
          continue;
        if (GET_POS(tch) != POS_STANDING)
          continue;
        if (!CAN_SEE(tch, ch))
          continue;
      
        do_assist(tch, GET_NAME(ch), 0, 0);				  
      }
    }

    auto_assist_owned_followers(ch);

    hit(ch, FIGHTING(ch), TYPE_UNDEFINED);
    
    do_offhand_attack(ch, FIGHTING(ch));
    if (FIGHTING(ch))
      do_spirit_procs(ch, FIGHTING(ch));

    if (MOB_FLAGGED(ch, MOB_SPEC) && GET_MOB_SPEC(ch) && !MOB_FLAGGED(ch, MOB_NOTDEADYET)) {
      char actbuf[MAX_INPUT_LENGTH] = "";
      (GET_MOB_SPEC(ch)) (ch, ch, 0, actbuf);
    }
  }
}
