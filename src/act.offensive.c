/**************************************************************************
*  File: act.offensive.c                                   Part of tbaMUD *
*  Usage: Player-level commands of an offensive nature.                   *
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
#include "interpreter.h"
#include "control.h"
#include "handler.h"
#include "db.h"
#include "spells.h"
#include "act.h"
#include "fight.h"
#include "mud_event.h"
#include "race.h"

static const char *appraise_level_band(const struct char_data *ch, const struct char_data *vict)
{
  int delta = GET_LEVEL(vict) - GET_LEVEL(ch);

  if (delta <= -15)
    return "trivial";
  if (delta <= -5)
    return "weaker than you";
  if (delta <= 4)
    return "near your level";
  if (delta <= 12)
    return "stronger than you";
  return "far stronger than you";
}

static const char *appraise_condition(const struct char_data *vict)
{
  int pct;

  if (!vict || GET_MAX_HIT(vict) <= 0)
    return "unclear";
  pct = (GET_HIT(vict) * 100) / GET_MAX_HIT(vict);
  if (pct >= 95)
    return "unwounded";
  if (pct >= 75)
    return "lightly hurt";
  if (pct >= 45)
    return "wounded";
  if (pct >= 20)
    return "badly wounded";
  return "near collapse";
}

static const char *appraise_role(const struct char_data *vict)
{
  if (!vict)
    return "unclear";
  if (GET_INT(vict) >= 18 || AFF_FLAGGED(vict, AFF_TRUESIGHT))
    return "spellcaster";
  if (AFF_FLAGGED(vict, AFF_STONESKIN) || AFF_FLAGGED(vict, AFF_BARKSKIN))
    return "armored defender";
  if (GET_DEX(vict) >= 18 || AFF_FLAGGED(vict, AFF_SNEAK))
    return "agile skirmisher";
  if (GET_STR(vict) >= 18)
    return "melee fighter";
  if (AFF_FLAGGED(vict, AFF_REGENERATING) || AFF_FLAGGED(vict, AFF_HOLY_AURA))
    return "support fighter";
  if (IS_NPC(vict) && vict->master)
    return "summoned servant";
  return "unclear";
}

static int appraise_success_score(struct char_data *ch, struct char_data *vict)
{
  int score = GET_SKILL(ch, SKILL_APPRAISE_ENEMY);
  int level_delta = GET_LEVEL(ch) - GET_LEVEL(vict);

  score += level_delta * 2;
  if (AFF_FLAGGED(vict, AFF_HIDE) || AFF_FLAGGED(vict, AFF_INVISIBLE))
    score -= 20;
  if (AFF_FLAGGED(vict, AFF_SNEAK))
    score -= 10;
  if (AFF_FLAGGED(ch, AFF_DETECT_INVIS) || AFF_FLAGGED(ch, AFF_SENSE_LIFE) ||
      AFF_FLAGGED(ch, AFF_TRUESIGHT))
    score += 10;
  return score;
}

static int appraise_is_hasted(struct char_data *vict)
{
  if (!vict)
    return FALSE;

  #ifdef AFF_HASTE
  if (AFF_FLAGGED(vict, AFF_HASTE))
    return TRUE;
  #endif

  #ifdef SPELL_HASTE
  if (affected_by_spell(vict, SPELL_HASTE))
    return TRUE;
  #endif

  return FALSE;
}

static int appraise_is_undead(struct char_data *vict)
{
  if (!vict)
    return FALSE;

  if (GET_RACE(vict) == RACE_VAMPIRE)
    return TRUE;

  if (IS_NPC(vict) && GET_CLASS(vict) == CLASS_UNDEAD)
    return TRUE;

  return FALSE;
}

ACMD(do_assist)
{
  char arg[MAX_INPUT_LENGTH];
  struct char_data *helpee, *opponent;

  if (FIGHTING(ch)) {
    send_to_char(ch, "You're already fighting!  How can you assist someone else?\r\n");
    return;
  }
  one_argument(argument, arg);

  if (!*arg)
    send_to_char(ch, "Whom do you wish to assist?\r\n");
  else if (!(helpee = get_char_vis(ch, arg, NULL, FIND_CHAR_ROOM)))
    send_to_char(ch, "%s", CONFIG_NOPERSON);
  else if (helpee == ch)
    send_to_char(ch, "You can't help yourself any more than this!\r\n");
  else {
    /*
     * Hit the same enemy the person you're helping is.
     */
    if (FIGHTING(helpee))
      opponent = FIGHTING(helpee);
    else
      for (opponent = world[IN_ROOM(ch)].people;
	   opponent && (FIGHTING(opponent) != helpee);
	   opponent = opponent->next_in_room);

    if (!opponent)
      act("But nobody is fighting $M!", FALSE, ch, 0, helpee, TO_CHAR);
    else if (!CAN_SEE(ch, opponent))
      act("You can't see who is fighting $M!", FALSE, ch, 0, helpee, TO_CHAR);
         /* prevent accidental pkill */
    else if (!CONFIG_PK_ALLOWED && !IS_NPC(opponent))
      send_to_char(ch, "You cannot kill other players.\r\n");
    else {
      send_to_char(ch, "You join the fight!\r\n");
      act("$N assists you!", 0, helpee, 0, ch, TO_CHAR);
      act("$n assists $N.", FALSE, ch, 0, helpee, TO_NOTVICT);
      if (IS_NPC(helpee) && !IS_NPC(ch))
      hit(ch, opponent, TYPE_UNDEFINED);
    }
  }
}

ACMD(do_hit)
{
  char arg[MAX_INPUT_LENGTH];
  struct char_data *vict;

 one_argument(argument, arg);

  if (AFF_FLAGGED(ch, AFF_STUNNED)) {
    send_to_char(ch, "You are stunned and cannot attack!\r\n");
    return;
  }

  if (!*arg)
    send_to_char(ch, "Hit who?\r\n");
  else if (!(vict = get_char_vis(ch, arg, NULL, FIND_CHAR_ROOM)))
    send_to_char(ch, "That player is not here.\r\n");
  else if (vict == ch) {
    send_to_char(ch, "You hit yourself...OUCH!.\r\n");
    act("$n hits $mself, and says OUCH!", FALSE, ch, 0, vict, TO_ROOM);
  } else if (!control_attack_allowed(ch, vict))
    send_to_char(ch, "You cannot compel that attack.\r\n");
  else if (is_owned_follower_target(ch, vict))
    send_to_char(ch, "You cannot attack one of your own followers.\r\n");
  else if (AFF_FLAGGED(ch, AFF_CHARM) && (ch->master == vict))
    act("$N is just such a good friend, you simply can't hit $M.", FALSE, ch, 0, vict, TO_CHAR);
  else {
    if (!CONFIG_PK_ALLOWED && !IS_NPC(vict) && !IS_NPC(ch)) 
      check_killer(ch, vict);

    if ((GET_POS(ch) == POS_STANDING) && (vict != FIGHTING(ch))) { 
      if (GET_DEX(ch) > GET_DEX(vict) || (GET_DEX(ch) == GET_DEX(vict) && rand_number(1, 2) == 1))  /* if faster */
        hit(ch, vict, TYPE_UNDEFINED);  /* first */
      else hit(vict, ch, TYPE_UNDEFINED);  /* or the victim is first */
        WAIT_STATE(ch, PULSE_VIOLENCE + 2); 
    } else 
      send_to_char(ch, "You're fighting the best you can!\r\n"); 
  } 
}

ACMD(do_kill)
{
  char arg[MAX_INPUT_LENGTH];
  struct char_data *vict;

  if (GET_LEVEL(ch) < LVL_GRGOD || IS_NPC(ch) || !PRF_FLAGGED(ch, PRF_NOHASSLE)) {
    do_hit(ch, argument, cmd, subcmd);
    return;
  }
  one_argument(argument, arg);

  if (AFF_FLAGGED(ch, AFF_STUNNED)) {
    send_to_char(ch, "You are stunned and cannot attack!\r\n");
    return;
  }

  if (!*arg) {
    send_to_char(ch, "Kill who?\r\n");
  } else {
    if (!(vict = get_char_vis(ch, arg, NULL, FIND_CHAR_ROOM)))
      send_to_char(ch, "That player is not here.\r\n");
    else if (ch == vict)
      send_to_char(ch, "Your mother would be so sad.. :(\r\n");
    else if (is_owned_follower_target(ch, vict))
      send_to_char(ch, "You cannot attack one of your own followers.\r\n");
    else {
      act("You chop $M to pieces!  Ah!  The blood!", FALSE, ch, 0, vict, TO_CHAR);
      act("$N chops you to pieces!", FALSE, vict, 0, ch, TO_CHAR);
      act("$n brutally slays $N!", FALSE, ch, 0, vict, TO_NOTVICT);
      raw_kill(vict, ch);
    }
  }
}

static int physical_skill_target_ok(struct char_data *ch, struct char_data *vict)
{
  return ch && vict && ch != vict && control_attack_allowed(ch, vict) && IN_ROOM(ch) != NOWHERE &&
    IN_ROOM(ch) == IN_ROOM(vict) && GET_POS(ch) >= POS_FIGHTING &&
    !ROOM_FLAGGED(IN_ROOM(ch), ROOM_PEACEFUL);
}

/* Generated parts carry structured provenance.  Never infer remains identity
 * from display text, since builders may create ordinary decorative objects. */
static int is_embalmable_remains(const struct obj_data *obj)
{
  int valid_part_mask = BODY_PART_HEAD | BODY_PART_TORSO |
    BODY_PART_ARM_LEFT | BODY_PART_ARM_RIGHT | BODY_PART_LEG_LEFT |
    BODY_PART_LEG_RIGHT | BODY_PART_FORELEG_LEFT | BODY_PART_FORELEG_RIGHT |
    BODY_PART_HINDLEG_LEFT | BODY_PART_HINDLEG_RIGHT | BODY_PART_TAIL |
    BODY_PART_WING_LEFT | BODY_PART_WING_RIGHT | BODY_PART_HORN |
    BODY_PART_TENTACLE_1 | BODY_PART_TENTACLE_2;

  if (!obj)
    return FALSE;
  if (IS_CORPSE(obj))
    return TRUE;
  if (!IS_GENUINE_REMAINS(obj) || !REMAINS_ANATOMY_INITIALIZED(obj) ||
      REMAINS_PROFILE(obj) < BODY_PROFILE_NONE ||
      REMAINS_PROFILE(obj) >= NUM_BODY_PROFILES ||
      !obj->remains_source_name || !*obj->remains_source_name)
    return FALSE;
  if (!REMAINS_SOURCE_IS_PLAYER(obj) && CORPSE_SOURCE_VNUM(obj) == NOBODY)
    return FALSE;
  if ((REMAINS_PART(obj) & valid_part_mask) != REMAINS_PART(obj) ||
      (REMAINS_PART(obj) & (REMAINS_PART(obj) - 1)) != 0)
    return FALSE;
  return REMAINS_SOURCE_IS_PLAYER(obj) == 0 || REMAINS_SOURCE_IS_PLAYER(obj) == 1;
}

enum combat_skill_result perform_backstab(struct char_data *ch,
    struct char_data *vict, int proficiency, int improve)
{
  int percent, success, move_cost;

  if (!physical_skill_target_ok(ch, vict) || GET_POS(ch) < POS_STANDING ||
      !GET_EQ(ch, WEAR_WIELD) ||
      GET_OBJ_VAL(GET_EQ(ch, WEAR_WIELD), 3) != TYPE_PIERCE - TYPE_HIT ||
      FIGHTING(vict) || is_owned_follower_target(ch, vict))
    return COMBAT_SKILL_NOT_ATTEMPTED;

  /* Player Backstab attempts pay the configured physical-skill cost from MOVE.
   * DG-scripted NPC backstabs call this helper with improve == FALSE and keep
   * their existing resource behavior. */
  move_cost = MAX(0, spell_info[SKILL_BACKSTAB].mana_min);
  if (improve && GET_LEVEL(ch) < LVL_IMMORT && GET_MOVE(ch) < move_cost) {
    send_to_char(ch, "You are too exhausted to attempt a backstab.\r\n");
    return COMBAT_SKILL_NOT_ATTEMPTED;
  }
  if (improve && GET_LEVEL(ch) < LVL_IMMORT && move_cost > 0)
    GET_MOVE(ch) = MAX(0, GET_MOVE(ch) - move_cost);

  /* Backstab is an aggressive action and breaks voluntary concealment. */
  if (AFF_FLAGGED(ch, AFF_INVISIBLE) || AFF_FLAGGED(ch, AFF_HIDE))
    appear(ch);
  if (AFF_FLAGGED(ch, AFF_SNEAK))
    affect_from_char(ch, SKILL_SNEAK);
  if (AFF_FLAGGED(ch, AFF_SKULK))
    affect_from_char(ch, SKILL_SKULK);
  if (MOB_FLAGGED(vict, MOB_AWARE) && AWAKE(vict)) {
    act("You notice $N lunging at you!", FALSE, vict, 0, ch, TO_CHAR);
    act("$e notices you lunging at $m!", FALSE, vict, 0, ch, TO_VICT);
    act("$n notices $N lunging at $m!", FALSE, vict, 0, ch, TO_NOTVICT);

    /* Spotting the ambush turns it into a real fight, not a one-off swing. */
    if (!FIGHTING(vict) && GET_POS(vict) > POS_STUNNED)
      set_fighting(vict, ch);
    if (!FIGHTING(ch) && GET_POS(ch) > POS_STUNNED)
      set_fighting(ch, vict);

    hit(vict, ch, TYPE_UNDEFINED);

    if (improve)
      improve_ability_from_use(ch, SKILL_BACKSTAB, FALSE);
    WAIT_STATE(ch, 2 * PULSE_VIOLENCE);
    return COMBAT_SKILL_ATTEMPTED;
  }
  percent = rand_number(1, 101);
  success = !AWAKE(vict) || percent <= proficiency;
  if (success)
    dual_skill_attack(ch, vict, SKILL_BACKSTAB);
  else
    damage(ch, vict, 0, SKILL_BACKSTAB);
  if (improve)
    improve_ability_from_use(ch, SKILL_BACKSTAB, success);
  WAIT_STATE(ch, 2 * PULSE_VIOLENCE);
  return COMBAT_SKILL_ATTEMPTED;
}

ACMD(do_backstab)
{
  char buf[MAX_INPUT_LENGTH];
  struct char_data *vict;

  if (IS_NPC(ch) || !GET_SKILL(ch, SKILL_BACKSTAB)) {
    send_to_char(ch, "You have no idea how to do that.\r\n");
    return;
  }
  one_argument(argument, buf);
  if (!(vict = get_char_vis(ch, buf, NULL, FIND_CHAR_ROOM))) {
    send_to_char(ch, "Backstab who?\r\n");
    return;
  }
  if (vict == ch) {
    send_to_char(ch, "How can you sneak up on yourself?\r\n");
    return;
  }
  if (is_owned_follower_target(ch, vict)) {
    send_to_char(ch, "You cannot attack one of your own followers.\r\n");
    return;
  }
  if (!GET_EQ(ch, WEAR_WIELD)) {
    send_to_char(ch, "You need to wield a weapon to make it a success.\r\n");
    return;
  }
  if (GET_OBJ_VAL(GET_EQ(ch, WEAR_WIELD), 3) != TYPE_PIERCE - TYPE_HIT) {
    send_to_char(ch, "Only piercing weapons can be used for backstabbing.\r\n");
    return;
  }
  if (FIGHTING(ch)) {
    send_to_char(ch, "You cannot backstab while fighting.\r\n");
    return;
  }
  if (FIGHTING(vict)) {
    send_to_char(ch, "You can't backstab a fighting person -- they're too alert!\r\n");
    return;
  }
  perform_backstab(ch, vict, GET_SKILL(ch, SKILL_BACKSTAB), TRUE);
}

ACMD(do_circle)
{
  char arg[MAX_INPUT_LENGTH];
  struct char_data *vict;
  int success;

  if (IS_NPC(ch) || !GET_SKILL(ch, SKILL_CIRCLE)) {
    send_to_char(ch, "You have no idea how to do that.\r\n");
    return;
  }
  if (spell_on_cooldown(ch, SKILL_CIRCLE)) {
    send_to_char(ch, "You need a moment to regain your footing.\r\n");
    return;
  }
  one_argument(argument, arg);
  vict = FIGHTING(ch);
  if (!vict) {
    send_to_char(ch, "You can only circle an opponent you are fighting.\r\n");
    return;
  }
  if (*arg && !(vict = get_char_vis(ch, arg, NULL, FIND_CHAR_ROOM))) {
    send_to_char(ch, "Circle whom?\r\n");
    return;
  }
  if (vict != FIGHTING(ch)) {
    send_to_char(ch, "You must circle the opponent you are fighting.\r\n");
    return;
  }
  success = rand_number(1, 100) <= GET_SKILL(ch, SKILL_CIRCLE);
  set_spell_cooldown(ch, SKILL_CIRCLE, 2);
  WAIT_STATE(ch, PULSE_VIOLENCE);
  if (!success) {
    act("You fail to find an opening around $N.", FALSE, ch, 0, vict, TO_CHAR);
    damage(ch, vict, 0, SKILL_CIRCLE);
  } else {
    act("You circle behind $N and strike!", FALSE, ch, 0, vict, TO_CHAR);
    if (GET_EQ(ch, WEAR_WIELD) && GET_EQ(ch, WEAR_HOLD))
      dual_skill_attack(ch, vict, SKILL_CIRCLE);
    else if (GET_EQ(ch, WEAR_WIELD))
      hit(ch, vict, SKILL_CIRCLE);
    else {
      /* A Mystic's unarmed Circle is intentionally a pair of hand strikes. */
      hit(ch, vict, TYPE_UNDEFINED);
      if (IN_ROOM(ch) == IN_ROOM(vict))
        hit(ch, vict, TYPE_UNDEFINED);
    }
  }
  improve_ability_from_use(ch, SKILL_CIRCLE, success);
}

ACMD(do_vanish)
{
  struct char_data *vict;
  struct affected_type af;
  int success;

  if (IS_NPC(ch) || !GET_SKILL(ch, SKILL_VANISH)) {
    send_to_char(ch, "You have no idea how to do that.\r\n");
    return;
  }
  if (!(vict = FIGHTING(ch))) {
    send_to_char(ch, "You are not fighting anyone.\r\n");
    return;
  }
  if (spell_on_cooldown(ch, SKILL_VANISH)) {
    send_to_char(ch, "You cannot vanish again so soon.\r\n");
    return;
  }
  set_spell_cooldown(ch, SKILL_VANISH, 3);
  success = rand_number(1, 100) <= GET_SKILL(ch, SKILL_VANISH) +
      (GET_DEX(ch) - GET_DEX(vict));
  WAIT_STATE(ch, PULSE_VIOLENCE);
  if (!success) {
    act("You fail to vanish from $N's sight.", FALSE, ch, 0, vict, TO_CHAR);
    improve_ability_from_use(ch, SKILL_VANISH, FALSE);
    return;
  }
  {
    struct char_data *fighter, *next_fighter;
    for (fighter = combat_list; fighter; fighter = next_fighter) {
      next_fighter = fighter->next_fighting;
      if (FIGHTING(fighter) == ch)
        stop_fighting(fighter);
    }
  }
  stop_fighting(ch);
  if (AFF_FLAGGED(ch, AFF_SKULK))
    affect_from_char(ch, SKILL_SKULK);
  if (AFF_FLAGGED(ch, AFF_SNEAK))
    affect_from_char(ch, SKILL_SNEAK);
  new_affect(&af);
  af.spell = SKILL_SNEAK;
  af.duration = GET_LEVEL(ch);
  SET_BIT_AR(af.bitvector, AFF_SNEAK);
  affect_to_char(ch, &af);
  act("You vanish from $N's attention and slip into the shadows.", FALSE, ch, 0, vict, TO_CHAR);
  act("$n slips from $N's attention and vanishes into the shadows.", FALSE, ch, 0, vict, TO_NOTVICT);
  improve_ability_from_use(ch, SKILL_VANISH, TRUE);
}

ACMD(do_nerve_pinch)
{
  char arg[MAX_INPUT_LENGTH];
  struct char_data *vict;
  struct affected_type af;
  int success, duration, cooldown, damage_amount, failure_chance, attack_score, defense_score;
  struct mud_event_data *event;

  if (IS_NPC(ch) || !GET_SKILL(ch, SKILL_NERVE_PINCH)) {
    send_to_char(ch, "You have no idea how to do that.\r\n");
    return;
  }
  if (spell_on_cooldown(ch, SKILL_NERVE_PINCH)) {
    send_to_char(ch, "Your hands have not yet recovered for another nerve pinch.\r\n");
    return;
  }
  one_argument(argument, arg);
  vict = FIGHTING(ch);
  if (*arg && !(vict = get_char_vis(ch, arg, NULL, FIND_CHAR_ROOM))) {
    send_to_char(ch, "Nerve pinch whom?\r\n");
    return;
  }
  if (!vict || vict != FIGHTING(ch)) {
    send_to_char(ch, "You must use nerve pinch on the opponent you are fighting.\r\n");
    return;
  }
  cooldown = GET_SKILL(ch, SKILL_NERVE_PINCH) >= 75 ? 3 :
      (GET_SKILL(ch, SKILL_NERVE_PINCH) >= 40 ? 4 : 5);
  set_spell_cooldown(ch, SKILL_NERVE_PINCH, cooldown);
  attack_score = GET_SKILL(ch, SKILL_NERVE_PINCH) + (GET_DEX(ch) - 10) +
      (GET_WIS(ch) - 10) + (GET_INT(ch) - 10) / 2 + GET_LEVEL(ch) / 5;
  defense_score = (GET_DEX(vict) - 10) + (GET_CON(vict) - 10) +
      (GET_WIS(vict) - 10) / 2 + (GET_INT(vict) - 10) / 2 + GET_LEVEL(vict) / 5;
  success = rand_number(1, 100) <= MAX(15, MIN(90, attack_score - defense_score + 50));
  WAIT_STATE(ch, PULSE_VIOLENCE);
  if (!success) {
    act("You miss $N's nerve cluster.", FALSE, ch, 0, vict, TO_CHAR);
    damage(ch, vict, 0, SKILL_NERVE_PINCH);
    improve_ability_from_use(ch, SKILL_NERVE_PINCH, FALSE);
    return;
  }
  damage_amount = MAX(1, GET_LEVEL(ch) / 2 + GET_SKILL(ch, SKILL_NERVE_PINCH) / 8);
  damage(ch, vict, damage_amount, SKILL_NERVE_PINCH);
  duration = 5 + GET_SKILL(ch, SKILL_NERVE_PINCH) / 10 +
      (GET_DEX(ch) - 10) / 4 + (GET_WIS(ch) - 10) / 4 + (GET_INT(ch) - 10) / 6 -
      GET_LEVEL(vict) / 10 - (GET_CON(vict) - 10) / 4 -
      (GET_WIS(vict) - 10) / 6 - (GET_INT(vict) - 10) / 6;
  duration = MAX(5, MIN(20, duration));
  failure_chance = 25 + GET_SKILL(ch, SKILL_NERVE_PINCH) / 2 +
      (GET_WIS(ch) - 10) / 3 + (GET_INT(ch) - 10) / 4 - GET_LEVEL(vict) / 8 -
      (GET_CON(vict) - 10) / 4 - (GET_WIS(vict) - 10) / 6;
  failure_chance = MAX(25, MIN(65, failure_chance));
  affect_from_char(vict, SKILL_NERVE_PINCH);
  event = char_has_mud_event(vict, eSKL_NERVE_DISRUPTION);
  if (event && event->pEvent)
    event_cancel(event->pEvent);
  new_affect(&af);
  af.spell = SKILL_NERVE_PINCH;
  af.duration = -1;
  af.modifier = failure_chance;
  SET_BIT_AR(af.bitvector, AFF_NERVE_DISRUPTION);
  affect_to_char(vict, &af);
  NEW_EVENT(eSKL_NERVE_DISRUPTION, vict, NULL, duration * PASSES_PER_SEC);
  act("You strike $N's nerves, disrupting $S concentration!", FALSE, ch, 0, vict, TO_CHAR);
  act("$n strikes your nerves; your concentration wavers!", FALSE, ch, 0, vict, TO_VICT);
  improve_ability_from_use(ch, SKILL_NERVE_PINCH, TRUE);
}

ACMD(do_embalm)
{
  char arg[MAX_INPUT_LENGTH], remainder[MAX_INPUT_LENGTH], custom[MAX_INPUT_LENGTH];
  struct obj_data *obj;

  if (IS_NPC(ch) || !GET_SKILL(ch, SKILL_EMBALM)) {
    send_to_char(ch, "You have no idea how to do that.\r\n");
    return;
  }
  half_chop(argument, arg, remainder);
  if (!*arg) {
    send_to_char(ch, "Embalm what?\r\n");
    return;
  }
  obj = get_obj_in_list_vis(ch, arg, NULL, ch->carrying);
  if (!obj)
    obj = get_obj_in_list_vis(ch, arg, NULL, world[IN_ROOM(ch)].contents);
  if (!is_embalmable_remains(obj)) {
    send_to_char(ch, "You can only embalm a corpse or corpse-derived trophy.\r\n");
    return;
  }
  if (GET_OBJ_TIMER(obj) < 0) {
    send_to_char(ch, "That trophy has already been embalmed.\r\n");
    return;
  }
  if (*remainder) {
    half_chop(remainder, arg, custom);
    if (!is_abbrev(arg, "as")) {
      send_to_char(ch, "To name it, use: embalm <corpse> as <presentation>.\r\n");
      return;
    }
    delete_doubledollar(custom);
    if (!*custom || strlen(custom) > 80 || strpbrk(custom, "\r\n\033")) {
      send_to_char(ch, "Use a single, plain presentation name of at most 80 characters.\r\n");
      return;
    }
    /* Player corpses are dynamic objects.  Their short description is owned
     * by this instance, so replacing it does not edit an object prototype. */
    free(obj->short_description);
    obj->short_description = strdup(custom);
  }
  /* Corpse decay only runs for positive timers; -1 is a durable internal
   * embalmed marker and does not alter its contents, values, or flags. */
  GET_OBJ_TIMER(obj) = -1;
  act("You embalm $p, preserving it as a lasting trophy.", FALSE, ch, obj, 0, TO_CHAR);
  act("$n carefully embalms $p.", FALSE, ch, obj, 0, TO_ROOM);
  improve_ability_from_use(ch, SKILL_EMBALM, TRUE);
}

ACMD(do_decapitate)
{
  char arg[MAX_INPUT_LENGTH];
  struct obj_data *corpse, *head;
  int chance;

  if (IS_NPC(ch) || !GET_SKILL(ch, SKILL_DECAPITATE)) {
    send_to_char(ch, "You have no idea how to do that.\r\n");
    return;
  }
  if (spell_on_cooldown(ch, SKILL_DECAPITATE)) {
    send_to_char(ch, "You need a moment before attempting another decapitation.\r\n");
    return;
  }
  one_argument(argument, arg);
  if (!*arg) {
    send_to_char(ch, "Decapitate what?\r\n");
    return;
  }
  corpse = get_obj_in_list_vis(ch, arg, NULL, world[IN_ROOM(ch)].contents);
  if (!corpse || !IS_CORPSE(corpse)) {
    send_to_char(ch, "You can only decapitate a genuine corpse.\r\n");
    return;
  }
  if (!REMAINS_ANATOMY_INITIALIZED(corpse)) {
    send_to_char(ch, "That old corpse has no reliable anatomy to harvest.\r\n");
    return;
  }
  if (!corpse_has_remaining_part(corpse, BODY_PART_HEAD)) {
    if (REMAINS_PROFILE(corpse) == BODY_PROFILE_NONE)
      send_to_char(ch, "That creature has no severable head.\r\n");
    else
      send_to_char(ch, "That corpse no longer has a head to take.\r\n");
    return;
  }

  chance = GET_SKILL(ch, SKILL_DECAPITATE) + (GET_DEX(ch) - 10) * 2 +
      (GET_STR(ch) - 10) - GET_OBJ_VAL(corpse, 2) / 2;
  chance = MAX(15, MIN(90, chance));
  set_spell_cooldown(ch, SKILL_DECAPITATE, 2);
  WAIT_STATE(ch, PULSE_VIOLENCE);
  if (rand_number(1, 100) > chance) {
    act("You fail to find a clean cut on $p.", FALSE, ch, corpse, 0, TO_CHAR);
    act("$n fails to find a clean cut on $p.", FALSE, ch, corpse, 0, TO_ROOM);
    improve_ability_from_use(ch, SKILL_DECAPITATE, FALSE);
    return;
  }

  head = sever_corpse_part(corpse, BODY_PART_HEAD);
  if (!head) {
    send_to_char(ch, "The corpse yields no head.\r\n");
    return;
  }
  obj_to_room(head, IN_ROOM(ch));
  act("You decapitate $p.", FALSE, ch, corpse, 0, TO_CHAR);
  act("$n decapitates $p.", FALSE, ch, corpse, 0, TO_ROOM);
  improve_ability_from_use(ch, SKILL_DECAPITATE, TRUE);
}

static int is_ordered_attack_command(const char *message, char *verb, char *target)
{
  char local[MAX_INPUT_LENGTH];

  if (!message || !*message)
    return FALSE;

  strlcpy(local, message, sizeof(local));
  half_chop(local, verb, target);
  if (!*verb || !*target)
    return FALSE;

  return (is_abbrev(verb, "hit") || is_abbrev(verb, "kill"));
}

static int execute_ordered_attack(struct char_data *issuer, struct char_data *follower,
                                  const char *target_name)
{
  struct char_data *victim = NULL;
  char victim_arg[MAX_INPUT_LENGTH];

  if (!issuer || !follower || !target_name || !*target_name)
    return FALSE;

  if (IN_ROOM(follower) != IN_ROOM(issuer)) {
    send_to_char(issuer, "You can't order %s to attack from another room.\r\n",
      GET_NAME(follower));
    return TRUE;
  }

  strlcpy(victim_arg, target_name, sizeof(victim_arg));
  victim = get_char_vis(follower, victim_arg, NULL, FIND_CHAR_ROOM);
  if (!victim) {
    send_to_char(issuer, "%s cannot find '%s' here.\r\n", GET_NAME(follower), target_name);
    return TRUE;
  }

  if (victim == follower) {
    send_to_char(issuer, "%s refuses to attack itself.\r\n", GET_NAME(follower));
    return TRUE;
  }

  if (is_owned_follower_target(follower, victim)) {
    send_to_char(issuer, "%s refuses to attack its own allies.\r\n", GET_NAME(follower));
    return TRUE;
  }

  if (GET_POS(follower) < POS_STANDING) {
    send_to_char(issuer, "%s is unable to fight right now.\r\n", GET_NAME(follower));
    return TRUE;
  }

  if (FIGHTING(follower) && FIGHTING(follower) != victim)
    stop_fighting(follower);

  if (!FIGHTING(follower))
    set_fighting(follower, victim);
  if (!FIGHTING(victim))
    set_fighting(victim, follower);

  hit(follower, victim, TYPE_UNDEFINED);
  return TRUE;
}

ACMD(do_order)
{
  char name[MAX_INPUT_LENGTH], message[MAX_INPUT_LENGTH];
  char command_verb[MAX_INPUT_LENGTH], target[MAX_INPUT_LENGTH];
  int order_all = FALSE;
  int ordered_attack = FALSE;
  bool found = FALSE;
  struct char_data *vict = NULL;
  struct follow_type *k;

  if (control_order(ch, argument)) return;
  half_chop(argument, name, message);
  ordered_attack = is_ordered_attack_command(message, command_verb, target);

  order_all = (is_abbrev(name, "followers") || is_abbrev(name, "all"));

  if (!*name || !*message)
    send_to_char(ch, "Order who to do what?\r\n");
  else if (!order_all && !(vict = get_char_vis(ch, name, NULL, FIND_CHAR_ROOM))) {
    for (k = ch->followers; k; k = k->next) {
      if (!k->follower || IN_ROOM(k->follower) != IN_ROOM(ch))
        continue;
      if ((k->follower->master == ch) &&
          (isname(name, GET_NAME(k->follower)) ||
           (k->follower->player.short_descr &&
            isname(name, k->follower->player.short_descr)))) {
        vict = k->follower;
        break;
      }
    }
  }

  if (!order_all && !vict)
    send_to_char(ch, "That person isn't here.\r\n");
  else if (!order_all && ch == vict)
    send_to_char(ch, "You obviously suffer from skitzofrenia.\r\n");
  else {
    if (AFF_FLAGGED(ch, AFF_CHARM)) {
      send_to_char(ch, "Your superior would not aprove of you giving orders.\r\n");
      return;
    }
    if (vict) {
      char buf[MAX_STRING_LENGTH];

      snprintf(buf, sizeof(buf), "$N orders you to '%s'", message);
      act(buf, FALSE, vict, 0, ch, TO_CHAR);
      act("$n gives $N an order.", FALSE, ch, 0, vict, TO_ROOM);

      if ((vict->master != ch) || !AFF_FLAGGED(vict, AFF_CHARM))
        act("$n has an indifferent look.", FALSE, vict, 0, 0, TO_ROOM);
      else {
        if (ordered_attack)
          execute_ordered_attack(ch, vict, target);
        else
          command_interpreter(vict, message);

        send_to_char(ch, "%s", CONFIG_OK);
      }
    } else {			/* This is order "followers"/"all" */
      char buf[MAX_STRING_LENGTH];

      snprintf(buf, sizeof(buf), "$n issues the order '%s'.", message);
      act(buf, FALSE, ch, 0, 0, TO_ROOM);

      for (k = ch->followers; k; k = k->next) {
        if (IN_ROOM(ch) == IN_ROOM(k->follower))
          if (AFF_FLAGGED(k->follower, AFF_CHARM)) {
            found = TRUE;
            if (ordered_attack)
              execute_ordered_attack(ch, k->follower, target);
            else
              command_interpreter(k->follower, message);
          }
      }
      if (found)
        send_to_char(ch, "%s", CONFIG_OK);
      else
        send_to_char(ch, "Nobody here is a loyal subject of yours!\r\n");
    }
  }
}

ACMD(do_flee)
{
  int i, attempt, loss;
  int web_penalty = 0;
  struct char_data *was_fighting;
  struct affected_type *af;

  if (AFF_FLAGGED(ch, AFF_ROOTED)) {
    send_to_char(ch, "You are rooted and cannot flee!\r\n");
    return;
  }
  if (room_has_effect(&world[IN_ROOM(ch)], ROOM_EFFECT_GRAVITY_WELL)) {
    send_to_char(ch, "The gravity well pins you in place; you cannot flee!\r\n");
    return;
  }

  if (AFF_FLAGGED(ch, AFF_WEBBED)) {
    for (af = ch->affected; af; af = af->next) {
      if (IS_SET_AR(af->bitvector, AFF_WEBBED) && af->modifier < web_penalty)
        web_penalty = af->modifier;
    }
    if (web_penalty <= -4) {
      send_to_char(ch, "You are tightly webbed and cannot flee!\r\n");
      return;
    }
  }

  if (GET_POS(ch) < POS_FIGHTING) {
    send_to_char(ch, "You are in pretty bad shape, unable to flee!\r\n");
    return;
  }

  for (i = 0; i < 6; i++) {
    attempt = rand_number(0, DIR_COUNT - 1); /* Select a random direction */
    if (CAN_GO(ch, attempt) &&
	!ROOM_FLAGGED(EXIT(ch, attempt)->to_room, ROOM_DEATH)) {
      act("$n panics, and attempts to flee!", TRUE, ch, 0, 0, TO_ROOM);
      was_fighting = FIGHTING(ch);
      if (do_simple_move(ch, attempt, TRUE)) {
	send_to_char(ch, "You flee head over heels.\r\n");
        if (was_fighting && !IS_NPC(ch)) {
	  loss = GET_MAX_HIT(was_fighting) - GET_HIT(was_fighting);
	  loss *= GET_LEVEL(was_fighting);
	  gain_exp(ch, -loss);
        }
      if (FIGHTING(ch)) 
        stop_fighting(ch); 
      if (was_fighting && ch == FIGHTING(was_fighting))
        stop_fighting(was_fighting); 
      } else {
	act("$n tries to flee, but can't!", TRUE, ch, 0, 0, TO_ROOM);
      }
      return;
    }
  }
  send_to_char(ch, "PANIC!  You couldn't escape!\r\n");
}

enum combat_skill_result perform_bash(struct char_data *ch,
    struct char_data *vict, int proficiency, int improve, int consume_move)
{
  int percent, success;

  if (!physical_skill_target_ok(ch, vict) || AFF_FLAGGED(ch, AFF_STUNNED) ||
      !GET_EQ(ch, WEAR_WIELD) || is_owned_follower_target(ch, vict) ||
      MOB_FLAGGED(vict, MOB_NOKILL))
    return COMBAT_SKILL_NOT_ATTEMPTED;
  if (consume_move) {
    if (GET_MOVE(ch) < 15)
      return COMBAT_SKILL_NOT_ATTEMPTED;
    GET_MOVE(ch) = MAX(0, GET_MOVE(ch) - 15);
  }
  percent = rand_number(1, 101);
  if (MOB_FLAGGED(vict, MOB_NOBASH))
    percent = 101;
  success = percent <= proficiency;
  if (!success) {
    damage(ch, vict, 0, SKILL_BASH);
    GET_POS(ch) = POS_SITTING;
  } else if (damage(ch, vict, 1, SKILL_BASH) > 0) {
    WAIT_STATE(vict, PULSE_VIOLENCE);
    if (IN_ROOM(ch) == IN_ROOM(vict))
      GET_POS(vict) = POS_SITTING;
  }
  if (improve)
    improve_ability_from_use(ch, SKILL_BASH, success);
  WAIT_STATE(ch, PULSE_VIOLENCE * 2);
  return COMBAT_SKILL_ATTEMPTED;
}

ACMD(do_bash)
{
  if (AFF_FLAGGED(ch, AFF_STUNNED)) {
    send_to_char(ch, "You are stunned and cannot do that.\r\n");
    return;
  }
  int move_cost = 15;
  if (GET_MOVE(ch) < move_cost) {
    send_to_char(ch, "You are too exhausted to bash.\r\n");
    return;
  }
  GET_MOVE(ch) = MAX(0, GET_MOVE(ch) - move_cost);
  char arg[MAX_INPUT_LENGTH];
  struct char_data *vict;
  one_argument(argument, arg);
  if (IS_NPC(ch) || !GET_SKILL(ch, SKILL_BASH)) {
    send_to_char(ch, "You have no idea how.\r\n");
    return;
  }
  if (ROOM_FLAGGED(IN_ROOM(ch), ROOM_PEACEFUL)) {
    send_to_char(ch, "This room just has such a peaceful, easy feeling...\r\n");
    return;
  }
  if (!GET_EQ(ch, WEAR_WIELD)) {
    send_to_char(ch, "You need to wield a weapon to make it a success.\r\n");
    return;
  }
  if (!(vict = get_char_vis(ch, arg, NULL, FIND_CHAR_ROOM))) {
    if (FIGHTING(ch) && IN_ROOM(ch) == IN_ROOM(FIGHTING(ch)))
      vict = FIGHTING(ch);
    else {
      send_to_char(ch, "Bash who?\r\n");
      return;
    }
  }
  if (vict == ch) {
    send_to_char(ch, "Aren't we funny today...\r\n");
    return;
  }
  if (is_owned_follower_target(ch, vict)) {
    send_to_char(ch, "You cannot attack one of your own followers.\r\n");
    return;
  }
  if (MOB_FLAGGED(vict, MOB_NOKILL)) {
    send_to_char(ch, "This mob is protected.\r\n");
    return;
  }
  perform_bash(ch, vict, GET_SKILL(ch, SKILL_BASH), TRUE, FALSE);
}

ACMD(do_rescue)
{
  char arg[MAX_INPUT_LENGTH];
  struct char_data *vict, *tmp_ch;
  int percent, prob;

  if (IS_NPC(ch) || !GET_SKILL(ch, SKILL_RESCUE)) {
    send_to_char(ch, "You have no idea how to do that.\r\n");
    return;
  }

  one_argument(argument, arg);

  if (!(vict = get_char_vis(ch, arg, NULL, FIND_CHAR_ROOM))) {
    send_to_char(ch, "Whom do you want to rescue?\r\n");
    return;
  }
  if (vict == ch) {
    send_to_char(ch, "What about fleeing instead?\r\n");
    return;
  }
  if (FIGHTING(ch) == vict) {
    send_to_char(ch, "How can you rescue someone you are trying to kill?\r\n");
    return;
  }
  for (tmp_ch = world[IN_ROOM(ch)].people; tmp_ch &&
       (FIGHTING(tmp_ch) != vict); tmp_ch = tmp_ch->next_in_room);

  if ((FIGHTING(vict) != NULL) && (FIGHTING(ch) == FIGHTING(vict)) && (tmp_ch == NULL)) {
     tmp_ch = FIGHTING(vict);
     if (FIGHTING(tmp_ch) == ch) {
     send_to_char(ch, "You have already rescued %s from %s.\r\n", GET_NAME(vict), GET_NAME(FIGHTING(ch)));
     return;
  }
  }

  if (!tmp_ch) {
    act("But nobody is fighting $M!", FALSE, ch, 0, vict, TO_CHAR);
    return;
  }
  percent = rand_number(1, 101);	/* 101% is a complete failure */
  prob = GET_SKILL(ch, SKILL_RESCUE);

  if (percent > prob) {
    send_to_char(ch, "You fail the rescue!\r\n");
    return;
  }
  send_to_char(ch, "Banzai!  To the rescue...\r\n");
  act("You are rescued by $N, you are confused!", FALSE, vict, 0, ch, TO_CHAR);
  act("$n heroically rescues $N!", FALSE, ch, 0, vict, TO_NOTVICT);

  if (FIGHTING(vict) == tmp_ch)
    stop_fighting(vict);
  if (FIGHTING(tmp_ch))
    stop_fighting(tmp_ch);
  if (FIGHTING(ch))
    stop_fighting(ch);

  set_fighting(ch, tmp_ch);
  set_fighting(tmp_ch, ch);

  WAIT_STATE(vict, 2 * PULSE_VIOLENCE);
}

EVENTFUNC(event_whirlwind)
{
  struct char_data *ch, *tch;
  struct mud_event_data *pMudEvent;
  struct list_data *room_list;
  int count;
	
  /* This is just a dummy check, but we'll do it anyway */
  if (event_obj == NULL)
    return 0;
	  
  /* For the sake of simplicity, we will place the event data in easily
   * referenced pointers */  
  pMudEvent = (struct mud_event_data *) event_obj;
  ch = (struct char_data *) pMudEvent->pStruct;    
  
  /* Spend moves each whirlwind tick */
  const int WHIRL_TICK_COST = 12;
  if (!ch || GET_MOVE(ch) < WHIRL_TICK_COST) {
    if (ch) send_to_char(ch, "You are too exhausted to keep spinning.\r\n");
    return 0;
  }
  GET_MOVE(ch) = MAX(0, GET_MOVE(ch) - WHIRL_TICK_COST);

  /* When using a list, we have to make sure to allocate the list as it
   * uses dynamic memory */
  room_list = create_list();
  
  /* We search through the "next_in_room", and grab all NPCs and add them
   * to our list */
  for (tch = world[IN_ROOM(ch)].people; tch; tch = tch->next_in_room)  
    if (IS_NPC(tch) && !is_owned_follower_target(ch, tch))
      add_to_list(tch, room_list);
      
  /* If our list is empty or has "0" entries, we free it from memory and
   * close off our event */    
  if (room_list->iSize == 0) {
    free_list(room_list);
    send_to_char(ch, "There is no one in the room to whirlwind!\r\n");
    return 0;
  }
  
  /* We spit out some ugly colour, making use of the new colour options,
   * to let the player know they are performing their whirlwind strike */
  send_to_char(ch, "\t[f313]You deliver a vicious \t[f014]\t[b451]WHIRLWIND!!!\tn\r\n");
  
  /* Lets grab some a random NPC from the list, and hit() them up */
  for (count = dice(1, 4); count > 0; count--) {
    tch = random_from_list(room_list);

    /* Target may be gone or dead by the time we pick it */
    if (!tch)
      continue;
    if (IN_ROOM(tch) != IN_ROOM(ch))
      continue;
    if (GET_POS(tch) <= POS_DEAD)
      continue;

    hit(ch, tch, TYPE_UNDEFINED);

    /* If we got extracted or moved, stop safely */
    if (IN_ROOM(ch) == NOWHERE)
      break;
  }
/* Now that our attack is done, let's free out list */
  free_list(room_list);
  
  /* The "return" of the event function is the time until the event is called
   * again. If we return 0, then the event is freed and removed from the list, but
   * any other numerical response will be the delay until the next call */
  if (GET_SKILL(ch, SKILL_WHIRLWIND) < rand_number(1, 101)) {
    send_to_char(ch, "You stop spinning.\r\n");
    return 0;
  } else
    return 1.5 * PASSES_PER_SEC;
}

/* The "Whirlwind" skill is designed to provide a basic understanding of the
 * mud event and list systems. */
ACMD(do_whirlwind)
{
  
  if (IS_NPC(ch) || !GET_SKILL(ch, SKILL_WHIRLWIND)) {
    send_to_char(ch, "You have no idea how.\r\n");
    return;
  }
  
  if ROOM_FLAGGED(IN_ROOM(ch), ROOM_PEACEFUL) {
    send_to_char(ch, "This room just has such a peaceful, easy feeling...\r\n");
    return;
  }

  if (GET_POS(ch) < POS_FIGHTING) {
    send_to_char(ch, "You must be on your feet to perform a whirlwind.\r\n");
    return;    
  }

  /* First thing we do is check to make sure the character is not in the middle
   * of a whirl wind attack.
   * 
   * "char_had_mud_event() will sift through the character's event list to see if
   * an event of type "eWHIRLWIND" currently exists. */
  if (char_has_mud_event(ch, eWHIRLWIND)) {
    send_to_char(ch, "You are already attempting that!\r\n");
    return;   
  }

  send_to_char(ch, "You begin to spin rapidly in circles.\r\n");
  act("$n begins to rapidly spin in a circle!", FALSE, ch, 0, 0, TO_ROOM);
  
  /* NEW_EVENT() will add a new mud event to the event list of the character.
   * This function below adds a new event of "eWHIRLWIND", to "ch", and passes "NULL" as
   * additional data. The event will be called in "3 * PASSES_PER_SEC" or 3 seconds */
  NEW_EVENT(eWHIRLWIND, ch, NULL, 3 * PASSES_PER_SEC);
  WAIT_STATE(ch, PULSE_VIOLENCE * 3);
}

enum combat_skill_result perform_kick(struct char_data *ch,
    struct char_data *vict, int proficiency, int improve, int consume_move)
{
  int percent, success;

  if (!physical_skill_target_ok(ch, vict) || AFF_FLAGGED(ch, AFF_STUNNED) ||
      is_owned_follower_target(ch, vict))
    return COMBAT_SKILL_NOT_ATTEMPTED;
  if (consume_move) {
    if (GET_MOVE(ch) < 10)
      return COMBAT_SKILL_NOT_ATTEMPTED;
    GET_MOVE(ch) = MAX(0, GET_MOVE(ch) - 10);
  }
  percent = ((compute_armor(vict) / 10) * 2) + rand_number(1, 101);
  success = percent <= proficiency;
  damage(ch, vict, success ? GET_LEVEL(ch) / 2 : 0, SKILL_KICK);
  if (improve)
    improve_ability_from_use(ch, SKILL_KICK, success);
  WAIT_STATE(ch, PULSE_VIOLENCE * 3);
  return COMBAT_SKILL_ATTEMPTED;
}

ACMD(do_kick)
{
  if (AFF_FLAGGED(ch, AFF_STUNNED)) {
    send_to_char(ch, "You are stunned and cannot do that.\r\n");
    return;
  }
  int move_cost = 10;
  if (GET_MOVE(ch) < move_cost) {
    send_to_char(ch, "You are too exhausted to kick.\r\n");
    return;
  }
  GET_MOVE(ch) = MAX(0, GET_MOVE(ch) - move_cost);
  char arg[MAX_INPUT_LENGTH];
  struct char_data *vict;
  if (IS_NPC(ch) || !GET_SKILL(ch, SKILL_KICK)) {
    send_to_char(ch, "You have no idea how.\r\n");
    return;
  }
  one_argument(argument, arg);
  if (!(vict = get_char_vis(ch, arg, NULL, FIND_CHAR_ROOM))) {
    if (FIGHTING(ch) && IN_ROOM(ch) == IN_ROOM(FIGHTING(ch)))
      vict = FIGHTING(ch);
    else {
      send_to_char(ch, "Kick who?\r\n");
      return;
    }
  }
  if (vict == ch) {
    send_to_char(ch, "Aren't we funny today...\r\n");
    return;
  }
  if (is_owned_follower_target(ch, vict)) {
    send_to_char(ch, "You cannot attack one of your own followers.\r\n");
    return;
  }
  perform_kick(ch, vict, GET_SKILL(ch, SKILL_KICK), TRUE, FALSE);
}

ACMD(do_appraise_enemy)
{
  char arg[MAX_INPUT_LENGTH];
  struct char_data *vict;
  int roll, score;
  int move_cost = 8;
  int quality = 0;

  if (IS_NPC(ch) || !GET_SKILL(ch, SKILL_APPRAISE_ENEMY)) {
    send_to_char(ch, "You have no idea how to read an opponent like that.\r\n");
    return;
  }
  if (spell_on_cooldown(ch, SKILL_APPRAISE_ENEMY)) {
    send_to_char(ch, "You need a moment before appraising another foe.\r\n");
    return;
  }
  if (GET_MOVE(ch) < move_cost) {
    send_to_char(ch, "You are too exhausted to appraise anyone right now.\r\n");
    return;
  }

  one_argument(argument, arg);
  if (!*arg) {
    send_to_char(ch, "Appraise whom?\r\n");
    return;
  }

  vict = get_char_vis(ch, arg, NULL, FIND_CHAR_ROOM);
  if (!vict) {
    send_to_char(ch, "You cannot get a clean read on anyone by that name here.\r\n");
    return;
  }
  if (vict == ch) {
    send_to_char(ch, "You already know your own strengths and flaws.\r\n");
    return;
  }

  GET_MOVE(ch) = MAX(0, GET_MOVE(ch) - move_cost);
  set_spell_cooldown(ch, SKILL_APPRAISE_ENEMY, 2);
  WAIT_STATE(ch, PULSE_VIOLENCE * 2);

  act("You study $N with a predator's eye, appraising strengths and weaknesses.", FALSE, ch, 0, vict, TO_CHAR);
  act("$n studies $N with a cold, measuring stare.", FALSE, ch, 0, vict, TO_NOTVICT);

  score = appraise_success_score(ch, vict);
  roll = rand_number(1, 101);
  if (roll <= score - 25)
    quality = 3; /* excellent */
  else if (roll <= score)
    quality = 2; /* strong success */
  else if (roll <= score + 20)
    quality = 1; /* basic success */
  else
    quality = 0; /* failure */

  if (quality == 0) {
    send_to_char(ch, "Your appraisal of %s is uncertain.\r\n", PERS(vict, ch));
    improve_ability_from_use(ch, SKILL_APPRAISE_ENEMY, 0);
    return;
  }

  send_to_char(ch, "You appraise %s:\r\n", PERS(vict, ch));
  send_to_char(ch, "Power: %s.\r\n", appraise_level_band(ch, vict));
  send_to_char(ch, "Condition: %s.\r\n", appraise_condition(vict));
  send_to_char(ch, "Role: %s.\r\n", appraise_role(vict));

  if (AFF_FLAGGED(vict, AFF_POISON)) send_to_char(ch, "Flag: poisoned.\r\n");
  if (AFF_FLAGGED(vict, AFF_BURNING)) send_to_char(ch, "Flag: burning.\r\n");
  if (AFF_FLAGGED(vict, AFF_ROOTED) || AFF_FLAGGED(vict, AFF_WEBBED)) send_to_char(ch, "Flag: rooted.\r\n");
  if (AFF_FLAGGED(vict, AFF_STUNNED)) send_to_char(ch, "Flag: stunned.\r\n");
  if (appraise_is_hasted(vict)) send_to_char(ch, "Flag: hasted.\r\n");
  if (AFF_FLAGGED(vict, AFF_ADRENALINE)) send_to_char(ch, "Flag: adrenaline surge.\r\n");
  if (AFF_FLAGGED(vict, AFF_FROZEN) || AFF_FLAGGED(vict, AFF_TIME_SNARE)) send_to_char(ch, "Flag: slowed.\r\n");
  if (AFF_FLAGGED(vict, AFF_WARDED) || AFF_FLAGGED(vict, AFF_SANCTUARY)) send_to_char(ch, "Flag: protected by magic.\r\n");
  if (AFF_FLAGGED(vict, AFF_SHIELDED)) send_to_char(ch, "Flag: shielded.\r\n");
  if (AFF_FLAGGED(vict, AFF_HIDE) || AFF_FLAGGED(vict, AFF_INVISIBLE)) send_to_char(ch, "Flag: hidden by shadow.\r\n");
  if (appraise_is_undead(vict)) send_to_char(ch, "Flag: undead.\r\n");
  if (IS_NPC(vict) && vict->master) send_to_char(ch, "Flag: summoned.\r\n");

  if (quality >= 2) {
    int phys = GET_DAMROLL(vict) + GET_HITROLL(vict);
    int def = compute_armor(vict);
    int mag = GET_INT(vict) + GET_WIS(vict);

    send_to_char(ch, "Threat (offense): %s.\r\n",
        phys < 8 ? "low" : phys < 18 ? "moderate" : phys < 28 ? "high" : "severe");
    send_to_char(ch, "Threat (defense): %s.\r\n",
        def < 10 ? "fragile" : def < 30 ? "steady" : def < 55 ? "durable" : "extremely durable");
    send_to_char(ch, "Threat (magic): %s.\r\n",
        mag < 18 ? "none" : mag < 26 ? "minor" : mag < 34 ? "notable" : mag < 42 ? "dangerous" : "overwhelming");

    if (AFF_FLAGGED(vict, AFF_STONESKIN)) send_to_char(ch, "Ward: stoneskin.\r\n");
    if (AFF_FLAGGED(vict, AFF_BARKSKIN)) send_to_char(ch, "Ward: barkskin.\r\n");
    if (AFF_FLAGGED(vict, AFF_MIRROR_IMAGE)) send_to_char(ch, "Ward: mirror effects.\r\n");
    if (AFF_FLAGGED(vict, AFF_ELEMENTAL_WARD_FIRE) || AFF_FLAGGED(vict, AFF_ELEMENTAL_WARD_COLD) ||
        AFF_FLAGGED(vict, AFF_ELEMENTAL_WARD_LIGHTNING) || AFF_FLAGGED(vict, AFF_ELEMENTAL_WARD_ACID))
      send_to_char(ch, "Ward: elemental wards.\r\n");
    if (AFF_FLAGGED(vict, AFF_DEATH_WARD)) send_to_char(ch, "Ward: death ward.\r\n");
    if (AFF_FLAGGED(vict, AFF_TRUESIGHT)) send_to_char(ch, "Ward: truesight.\r\n");
    if (AFF_FLAGGED(vict, AFF_BLOODLUST)) send_to_char(ch, "Ward: bloodlust.\r\n");
    if (affected_by_spell(vict, SPELL_SHADOW_ARMOR)) send_to_char(ch, "Ward: shadow armor.\r\n");
  }

  if (quality >= 3) {
    if (AFF_FLAGGED(vict, AFF_ELEMENTAL_WARD_FIRE)) send_to_char(ch, "Resistance: resistant to fire.\r\n");
    if (AFF_FLAGGED(vict, AFF_ELEMENTAL_WARD_COLD)) send_to_char(ch, "Resistance: resistant to cold.\r\n");
    if (AFF_FLAGGED(vict, AFF_ELEMENTAL_WARD_LIGHTNING)) send_to_char(ch, "Resistance: resistant to lightning.\r\n");
    if (AFF_FLAGGED(vict, AFF_ELEMENTAL_WARD_ACID)) send_to_char(ch, "Resistance: resistant to acid.\r\n");
    if (AFF_FLAGGED(vict, AFF_DEATH_WARD)) send_to_char(ch, "Resistance: resistant to death magic.\r\n");
    if (MOB_FLAGGED(vict, MOB_NOBASH) || AFF_FLAGGED(vict, AFF_ROOTED)) send_to_char(ch, "Resistance: difficult to restrain.\r\n");
    if (MOB_FLAGGED(vict, MOB_AWARE)) send_to_char(ch, "Resistance: difficult to frighten.\r\n");

    if (compute_armor(vict) < 20) send_to_char(ch, "Weakness: lightly armored.\r\n");
    if (AFF_FLAGGED(vict, AFF_ARCANE_LEAK) || AFF_FLAGGED(vict, AFF_HEXED)) send_to_char(ch, "Weakness: vulnerable to disruption.\r\n");
    if (GET_HIT(vict) < (GET_MAX_HIT(vict) / 2)) send_to_char(ch, "Weakness: already weakened.\r\n");
    if (AFF_FLAGGED(vict, AFF_FEARFUL) || AFF_FLAGGED(vict, AFF_STUNNED)) send_to_char(ch, "Weakness: unstable under pressure.\r\n");

    if (GET_DAMROLL(vict) > GET_INT(vict))
      send_to_char(ch, "Dominance: physically dominant.\r\n");
    else if (GET_INT(vict) > GET_DAMROLL(vict) + 4)
      send_to_char(ch, "Dominance: magically dominant.\r\n");
    else
      send_to_char(ch, "Dominance: balanced.\r\n");
    if (GET_MOVE(vict) < (GET_MAX_MOVE(vict) / 4))
      send_to_char(ch, "Posture: exhausted.\r\n");
    if (AFF_FLAGGED(vict, AFF_HIDE) || AFF_FLAGGED(vict, AFF_INVISIBLE))
      send_to_char(ch, "Posture: hiding true power.\r\n");
  }

  improve_ability_from_use(ch, SKILL_APPRAISE_ENEMY, 1);
}

ACMD(do_bandage)
{
  char arg[MAX_INPUT_LENGTH];
  struct char_data * vict;
  int percent, prob;

  if (!GET_SKILL(ch, SKILL_BANDAGE))
  {
    send_to_char(ch, "You are unskilled in the art of bandaging.\r\n");
    return;
  }

  if (GET_POS(ch) != POS_STANDING) {
    send_to_char(ch, "You are not in a proper position for that!\r\n");
    return;
  }

  one_argument(argument, arg);

  if (!(vict = get_char_vis(ch, arg, NULL, FIND_CHAR_ROOM))) {
    send_to_char(ch, "Who do you want to bandage?\r\n");
    return;
  }

  if (GET_HIT(vict) >= 0) {
    send_to_char(ch, "You can only bandage someone who is close to death.\r\n");
    return;
  }

  WAIT_STATE(ch, PULSE_VIOLENCE * 2);

  percent = rand_number(1, 101);        /* 101% is a complete failure */
  prob = GET_SKILL(ch, SKILL_BANDAGE);

  if (percent <= prob) {
    act("Your attempt to bandage fails.", FALSE, ch, 0, 0, TO_CHAR);
    act("$n tries to bandage $N, but fails miserably.", TRUE, ch, 
      0, vict, TO_NOTVICT);
    damage(vict, vict, 2, TYPE_SUFFERING);
    return;
  }

  act("You successfully bandage $N.", FALSE, ch, 0, vict, TO_CHAR);
  act("$n bandages $N, who looks a bit better now.", TRUE, ch, 0, 
    vict, TO_NOTVICT);
  act("Someone bandages you, and you feel a bit better now.",
         FALSE, ch, 0, vict, TO_VICT);
  GET_HIT(vict) = 0;
}
