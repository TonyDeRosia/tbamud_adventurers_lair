/**************************************************************************
*  File: limits.c                                          Part of tbaMUD *
*  Usage: Limits & gain funcs for HMV, exp, hunger/thirst, idle time.     *
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
#include "spells.h"
#include "comm.h"
#include "db.h"
#include "handler.h"
#include "interpreter.h"
#include "dg_scripts.h"
#include "screen.h"
#include "class.h"
#include "fight.h"
#include "mud_event.h"

/* local file scope function prototypes */
static void check_idling(struct char_data *ch);
static struct affected_type *find_affect(struct char_data *ch, int spellnum);
static int best_regen_multiplier(struct char_data *ch);
static int object_regen_multiplier(struct obj_data *obj);
static int find_affect_modifier_for_flag(struct char_data *ch, int aff_flag, int fallback);
static int condition_stage_value(int cond_value);
static int condition_regen_percent(int cond_value);
static int combined_condition_regen_percent(struct char_data *ch);
static bool is_starving(struct char_data *ch);
static bool is_dehydrated(struct char_data *ch);
static void update_starvation_trackers(struct char_data *ch);
static void apply_condition_tick_penalties(struct char_data *ch);

enum condition_penalty_stage {
  COND_STAGE_NORMAL = 1,
  COND_STAGE_HUNGRY = 2,
  COND_STAGE_STARVING = 3,
  COND_STAGE_PROLONGED = 4
};

#define HUNGRY_THRESHOLD 4
#define STARVING_THRESHOLD 0
#define PROLONGED_TICK_THRESHOLD 6
#define STARVING_MOVE_DRAIN_MIN 1
#define STARVING_MOVE_DRAIN_MAX 2
#define DEHYDRATED_MOVE_DRAIN_MIN 2
#define DEHYDRATED_MOVE_DRAIN_MAX 3
#define STARVING_HP_DRAIN_MIN 2
#define STARVING_HP_DRAIN_MAX 4
#define DEHYDRATED_HP_DRAIN_MIN 2
#define DEHYDRATED_HP_DRAIN_MAX 4
#define STARVING_MANA_DRAIN_MIN 0
#define STARVING_MANA_DRAIN_MAX 1
#define DEHYDRATED_MANA_DRAIN_MIN 1
#define DEHYDRATED_MANA_DRAIN_MAX 2


static struct affected_type *find_affect(struct char_data *ch, int spellnum)
{
  struct affected_type *af;

  for (af = ch->affected; af; af = af->next)
    if (af->spell == spellnum)
      return af;

  return NULL;
}

static int object_regen_multiplier(struct obj_data *obj)
{
  int mult;

  if (!obj)
    return 1;

  switch (GET_OBJ_TYPE(obj)) {
  case ITEM_FURNITURE:
  case ITEM_WEAPON:
    mult = GET_OBJ_VAL(obj, 0);
    break;
  default:
    return 1;
  }

  if (mult < 2)
    return 1;

  return mult;
}

static int best_regen_multiplier(struct char_data *ch)
{
  int mult = 1, check;

  if (GET_POS(ch) != POS_SITTING && GET_POS(ch) != POS_RESTING && GET_POS(ch) != POS_SLEEPING)
    return 1;

  check = object_regen_multiplier(SITTING(ch));
  if (check > mult)
    mult = check;

  check = object_regen_multiplier(GET_EQ(ch, WEAR_WIELD));
  if (check > mult)
    mult = check;

  return mult;
}

static int find_affect_modifier_for_flag(struct char_data *ch, int aff_flag, int fallback)
{
  struct affected_type *af;

  if (!ch)
    return fallback;

  for (af = ch->affected; af; af = af->next) {
    if (IS_SET_AR(af->bitvector, aff_flag) && af->modifier != 0)
      return af->modifier;
  }

  return fallback;
}

static int condition_stage_value(int cond_value)
{
  if (cond_value <= STARVING_THRESHOLD)
    return COND_STAGE_STARVING;
  if (cond_value <= HUNGRY_THRESHOLD)
    return COND_STAGE_HUNGRY;
  return COND_STAGE_NORMAL;
}

static int condition_regen_percent(int cond_value)
{
  int stage = condition_stage_value(cond_value);

  if (stage == COND_STAGE_HUNGRY)
    return 50;
  if (stage >= COND_STAGE_STARVING)
    return 0;
  return 100;
}

static int combined_condition_regen_percent(struct char_data *ch)
{
  if (!ch || IS_NPC(ch) || GET_LEVEL(ch) >= LVL_IMMORT)
    return 100;

  int hunger_percent = condition_regen_percent(GET_COND(ch, HUNGER));
  int thirst_percent = condition_regen_percent(GET_COND(ch, THIRST));

  return MIN(hunger_percent, thirst_percent);
}

static bool is_starving(struct char_data *ch)
{
  if (!ch)
    return FALSE;
  if (IS_NPC(ch) || GET_LEVEL(ch) >= LVL_IMMORT)
    return FALSE;

  return (condition_stage_value(GET_COND(ch, HUNGER)) >= COND_STAGE_STARVING);
}

static bool is_dehydrated(struct char_data *ch)
{
  if (!ch)
    return FALSE;
  if (IS_NPC(ch) || GET_LEVEL(ch) >= LVL_IMMORT)
    return FALSE;

  return (condition_stage_value(GET_COND(ch, THIRST)) >= COND_STAGE_STARVING);
}

static void update_starvation_trackers(struct char_data *ch)
{
  if (!ch || IS_NPC(ch))
    return;
  if (GET_LEVEL(ch) >= LVL_IMMORT) {
    ch->char_specials.starving_ticks = 0;
    ch->char_specials.dehydrated_ticks = 0;
    return;
  }

  if (GET_COND(ch, HUNGER) <= STARVING_THRESHOLD)
    ch->char_specials.starving_ticks++;
  else
    ch->char_specials.starving_ticks = 0;

  if (GET_COND(ch, THIRST) <= STARVING_THRESHOLD)
    ch->char_specials.dehydrated_ticks++;
  else
    ch->char_specials.dehydrated_ticks = 0;
}

static void apply_condition_tick_penalties(struct char_data *ch)
{
  int move_drain = 0;
  int hp_drain = 0;
  int mana_drain = 0;
  bool starving = FALSE;
  bool dehydrated = FALSE;
  bool prolonged_starving = FALSE;
  bool prolonged_dehydrated = FALSE;

  if (!ch || IS_NPC(ch) || GET_LEVEL(ch) >= LVL_IMMORT)
    return;

  starving = is_starving(ch);
  dehydrated = is_dehydrated(ch);
  prolonged_starving = starving && (ch->char_specials.starving_ticks >= PROLONGED_TICK_THRESHOLD);
  prolonged_dehydrated = dehydrated && (ch->char_specials.dehydrated_ticks >= PROLONGED_TICK_THRESHOLD);

  if (starving)
    move_drain += rand_number(STARVING_MOVE_DRAIN_MIN, STARVING_MOVE_DRAIN_MAX);
  if (dehydrated)
    move_drain += rand_number(DEHYDRATED_MOVE_DRAIN_MIN, DEHYDRATED_MOVE_DRAIN_MAX);

  if (move_drain > 0)
    GET_MOVE(ch) = MAX(0, GET_MOVE(ch) - move_drain);

  if (prolonged_starving)
    hp_drain += rand_number(STARVING_HP_DRAIN_MIN, STARVING_HP_DRAIN_MAX);
  if (prolonged_dehydrated)
    hp_drain += rand_number(DEHYDRATED_HP_DRAIN_MIN, DEHYDRATED_HP_DRAIN_MAX);
  if (prolonged_starving)
    mana_drain += rand_number(STARVING_MANA_DRAIN_MIN, STARVING_MANA_DRAIN_MAX);
  if (prolonged_dehydrated)
    mana_drain += rand_number(DEHYDRATED_MANA_DRAIN_MIN, DEHYDRATED_MANA_DRAIN_MAX);

  if (hp_drain > 0)
    GET_HIT(ch) = MAX(1, GET_HIT(ch) - hp_drain);
  if (mana_drain > 0)
    GET_MANA(ch) = MAX(0, GET_MANA(ch) - mana_drain);

  if ((starving || dehydrated) && !(prolonged_starving || prolonged_dehydrated)) {
    if (((starving ? ch->char_specials.starving_ticks : ch->char_specials.dehydrated_ticks) % 8) == 1) {
      if (starving && dehydrated)
        send_to_char(ch, "Hunger and thirst leave you drained.\r\n");
      else if (starving)
        send_to_char(ch, "Hunger weakens your body.\r\n");
      else
        send_to_char(ch, "Thirst leaves you drained.\r\n");
    }
  } else if (prolonged_starving || prolonged_dehydrated) {
    if (((prolonged_starving ? ch->char_specials.starving_ticks : ch->char_specials.dehydrated_ticks) % 6) == 0) {
      if (prolonged_starving && prolonged_dehydrated)
        send_to_char(ch, "Starvation and dehydration wrack your body.\r\n");
      else if (prolonged_starving)
        send_to_char(ch, "Starvation gnaws at your flesh.\r\n");
      else
        send_to_char(ch, "Dehydration wracks your body.\r\n");
    }
  }
}

/* The hit_limit, mana_limit, and move_limit functions are gone.  They added an
 * unnecessary level of complexity to the internal structure, weren't
 * particularly useful, and led to some annoying bugs.  From the players' point
 * of view, the only difference the removal of these functions will make is
 * that a character's age will now only affect the HMV gain per tick, and _not_
 * the HMV maximums. */
/* manapoint gain pr. game hour */
int mana_gain(struct char_data *ch)
{
  int gain;

  if (IS_NPC(ch)) {
    /* Neat and fast */
    gain = GET_LEVEL(ch);
  } else {
    /* Pool scaling keeps the larger level-100 pools practical to recover;
     * affected stats intentionally improve only temporary regeneration. */
    gain = MAX(1, GET_MAX_MANA(ch) / 40) +
           MAX(0, (GET_INT(ch) + GET_WIS(ch) - 16) / 3);

    /* Class calculations */

    /* Skill/Spell calculations */

    /* Position calculations    */
    switch (GET_POS(ch)) {
    case POS_SLEEPING:
      gain *= 2;
      break;
    case POS_RESTING:
      gain += (gain / 2);	/* Divide by 2 */
      break;
    case POS_SITTING:
      gain += (gain / 4);	/* Divide by 4 */
      break;
    }

    if (IS_MAGIC_USER(ch) || IS_CLERIC(ch))
      gain *= 2;

    gain = (gain * combined_condition_regen_percent(ch)) / 100;
  }

  if (!IS_NPC(ch) && (is_starving(ch) || is_dehydrated(ch)))
    return 0;

  if (AFF_FLAGGED(ch, AFF_POISON))
    gain /= 4;

  if (AFF_FLAGGED(ch, AFF_CLARITY))
    gain += MAX(0, find_affect_modifier_for_flag(ch, AFF_CLARITY, 5));

  gain *= best_regen_multiplier(ch);

  return (gain);
}

/* Hitpoint gain pr. game hour */
int hit_gain(struct char_data *ch)
{
  int gain;

  if (IS_NPC(ch)) {
    /* Neat and fast */
    gain = GET_LEVEL(ch);
  } else {

    gain = MAX(1, GET_MAX_HIT(ch) / 50) +
           MAX(0, (GET_CON(ch) - 8) / 2);

    /* Class/Level calculations */
    /* Skill/Spell calculations */
    /* Position calculations    */

    switch (GET_POS(ch)) {
    case POS_SLEEPING:
      gain += (gain / 2);	/* Divide by 2 */
      break;
    case POS_RESTING:
      gain += (gain / 4);	/* Divide by 4 */
      break;
    case POS_SITTING:
      gain += (gain / 8);	/* Divide by 8 */
      break;
    }

    if (IS_MAGIC_USER(ch) || IS_CLERIC(ch))
      gain /= 2;	/* Ouch. */

    gain = (gain * combined_condition_regen_percent(ch)) / 100;
  }

  if (!IS_NPC(ch) && (is_starving(ch) || is_dehydrated(ch)))
    return 0;

  if (AFF_FLAGGED(ch, AFF_POISON))
    gain /= 4;

  gain *= best_regen_multiplier(ch);

  return (gain);
}

/* move gain pr. game hour */
int move_gain(struct char_data *ch)
{
  int gain;

  if (IS_NPC(ch)) {
    /* Neat and fast */
    gain = GET_LEVEL(ch);
  } else {
    gain = MAX(1, GET_MAX_MOVE(ch) / 40) +
           MAX(0, (GET_CON(ch) + GET_DEX(ch) - 16) / 4);

    /* Class/Level calculations */
    /* Skill/Spell calculations */
    /* Position calculations    */
    switch (GET_POS(ch)) {
    case POS_SLEEPING:
      gain += (gain / 2);	/* Divide by 2 */
      break;
    case POS_RESTING:
      gain += (gain / 4);	/* Divide by 4 */
      break;
    case POS_SITTING:
      gain += (gain / 8);	/* Divide by 8 */
      break;
    }

    gain = (gain * combined_condition_regen_percent(ch)) / 100;
  }

  if (!IS_NPC(ch) && (is_starving(ch) || is_dehydrated(ch)))
    return 0;

  if (AFF_FLAGGED(ch, AFF_POISON))
    gain /= 4;

  gain *= best_regen_multiplier(ch);

  return (gain);
}

void set_title(struct char_data *ch, char *title)
{
  if (GET_TITLE(ch) != NULL)
    free(GET_TITLE(ch));

  if (title == NULL) {
    if (!IS_NPC(ch) && *GET_SOFT_CLASS_TITLE(ch)) {
      char dynamic_title[MAX_TITLE_LENGTH + 1];
      snprintf(dynamic_title, sizeof(dynamic_title), "the %s", GET_SOFT_CLASS_TITLE(ch));
      GET_TITLE(ch) = strdup(dynamic_title);
    } else {
      GET_TITLE(ch) = strdup(GET_SEX(ch) == SEX_FEMALE ?
        title_female(GET_CLASS(ch), GET_LEVEL(ch)) :
        title_male(GET_CLASS(ch), GET_LEVEL(ch)));
    }
  } else {
    if (strlen(title) > MAX_TITLE_LENGTH)
      title[MAX_TITLE_LENGTH] = '\0';

    GET_TITLE(ch) = strdup(title);
  }
}

void run_autowiz(void)
{
#if defined(CIRCLE_UNIX) || defined(CIRCLE_WINDOWS)
  if (CONFIG_USE_AUTOWIZ) {
    size_t res;
    char buf[256];

#if defined(CIRCLE_UNIX)
    res = snprintf(buf, sizeof(buf), "nice ../bin/autowiz %d %s %d %s %d &",
	CONFIG_MIN_WIZLIST_LEV, WIZLIST_FILE, LVL_IMMORT, IMMLIST_FILE, (int) getpid());
#elif defined(CIRCLE_WINDOWS)
    res = snprintf(buf, sizeof(buf), "autowiz %d %s %d %s",
	CONFIG_MIN_WIZLIST_LEV, WIZLIST_FILE, LVL_IMMORT, IMMLIST_FILE);
#endif /* CIRCLE_WINDOWS */

    /* Abusing signed -> unsigned conversion to avoid '-1' check. */
    if (res < sizeof(buf)) {
      mudlog(CMP, LVL_IMMORT, FALSE, "Initiating autowiz.");
      reboot_wizlists();
      int rval = system(buf);
      if(rval != 0)
        mudlog(BRF, LVL_IMMORT, TRUE, "Warning: autowiz failed with return value %d", rval);
    } else
      log("Cannot run autowiz: command-line doesn't fit in buffer.");
  }
#endif /* CIRCLE_UNIX || CIRCLE_WINDOWS */
}

int final_positive_xp_gain(int raw_gain)
{
  long long modified = raw_gain;

  if (modified <= 0)
    return 0;
  if (IS_HAPPYHOUR && IS_HAPPYEXP)
    modified += (modified * HAPPY_EXP) / 100;
  return (int)MIN((long long)CONFIG_MAX_EXP_GAIN, modified);
}

void gain_exp(struct char_data *ch, int gain)
{
  int is_altered = FALSE;
  int num_levels = 0;
  int max_mortal_level = LVL_IMMORT - 1;
  bool hit_mortal_cap = FALSE;
  int glory_awarded = 0;

  if (!IS_NPC(ch) && ((GET_LEVEL(ch) < 1 || GET_LEVEL(ch) >= LVL_IMMORT)))
    return;

  if (IS_NPC(ch)) {
    GET_EXP(ch) += gain;
    return;
  }
  if (gain > 0) {
    gain = final_positive_xp_gain(gain);
    GET_EXP(ch) += gain;
    while (GET_LEVEL(ch) < max_mortal_level &&
        GET_EXP(ch) >= level_exp(GET_CLASS(ch), GET_LEVEL(ch) + 1)) {
      GET_LEVEL(ch) += 1;
      num_levels++;
      advance_level(ch);
      glory_awarded += rand_number(50, 75);
      is_altered = TRUE;
    }

    if (GET_LEVEL(ch) >= max_mortal_level) {
      int immort_exp = level_exp(GET_CLASS(ch), LVL_IMMORT);

      if (immort_exp > 0 && GET_EXP(ch) >= immort_exp) {
        GET_EXP(ch) = immort_exp - 1;
        hit_mortal_cap = TRUE;
      }
    }

    if (is_altered) {
      mudlog(BRF, MAX(LVL_IMMORT, GET_INVIS_LEV(ch)), TRUE, "%s advanced %d level%s to level %d.\r\n",
                GET_NAME(ch), num_levels, num_levels == 1 ? "" : "s", GET_LEVEL(ch));
      if (num_levels == 1)
        send_to_char(ch, "%sYou rise to level %d!%s\r\n", CCYEL(ch, C_NRM), GET_LEVEL(ch), CCNRM(ch, C_NRM));
      else
        send_to_char(ch, "%sYou rise %d levels to level %d!%s\r\n", CCYEL(ch, C_NRM), num_levels, GET_LEVEL(ch), CCNRM(ch, C_NRM));
      if (glory_awarded > 0) {
        GET_GLORY(ch) += glory_awarded;
        send_to_char(ch, "You gain %s%d%s %sGlory%s for advancing.\r\n",
                     CBYEL(ch, C_NRM), glory_awarded, CCNRM(ch, C_NRM),
                     CBCYN(ch, C_NRM), CCNRM(ch, C_NRM));
      }
      set_title(ch, NULL);
      if (GET_LEVEL(ch) >= LVL_IMMORT && !PLR_FLAGGED(ch, PLR_NOWIZLIST))
        run_autowiz();
    }

    if (hit_mortal_cap)
      send_to_char(ch, "You have reached the mortal level cap. Immortality requires immortal approval.\r\n");
  } else if (gain < 0) {
    gain = MAX(-CONFIG_MAX_EXP_LOSS, gain);     /* Cap max exp lost per death */
    GET_EXP(ch) += gain;
    if (GET_EXP(ch) < 0)
      GET_EXP(ch) = 0;
  }
  if (GET_LEVEL(ch) >= LVL_IMMORT && !PLR_FLAGGED(ch, PLR_NOWIZLIST))
    run_autowiz();
  }

void gain_exp_regardless(struct char_data *ch, int gain, int max_level)
{
  int is_altered = FALSE;
  int num_levels = 0;
  int level_cap = MIN(max_level, LVL_IMPL);
  int glory_awarded = 0;

  if ((IS_HAPPYHOUR) && (IS_HAPPYEXP))
    gain += (int)((float)gain * ((float)HAPPY_EXP / (float)(100)));

  GET_EXP(ch) += gain;
  if (GET_EXP(ch) < 0)
    GET_EXP(ch) = 0;

  if (!IS_NPC(ch)) {
    while (GET_LEVEL(ch) < level_cap &&
        GET_EXP(ch) >= level_exp(GET_CLASS(ch), GET_LEVEL(ch) + 1)) {
      GET_LEVEL(ch) += 1;
      num_levels++;
      advance_level(ch);
      glory_awarded += rand_number(50, 75);
      is_altered = TRUE;
    }

    if (is_altered) {
      mudlog(BRF, MAX(LVL_IMMORT, GET_INVIS_LEV(ch)), TRUE, "%s advanced %d level%s to level %d.",
		GET_NAME(ch), num_levels, num_levels == 1 ? "" : "s", GET_LEVEL(ch));
      if (num_levels == 1)
        send_to_char(ch, "%sYou rise to level %d!%s\r\n", CCYEL(ch, C_NRM), GET_LEVEL(ch), CCNRM(ch, C_NRM));
      else
        send_to_char(ch, "%sYou rise %d levels to level %d!%s\r\n", CCYEL(ch, C_NRM), num_levels, GET_LEVEL(ch), CCNRM(ch, C_NRM));
      if (glory_awarded > 0) {
        GET_GLORY(ch) += glory_awarded;
        send_to_char(ch, "You gain %s%d%s %sGlory%s for advancing.\r\n",
                     CBYEL(ch, C_NRM), glory_awarded, CCNRM(ch, C_NRM),
                     CBCYN(ch, C_NRM), CCNRM(ch, C_NRM));
      }
      set_title(ch, NULL);
    }
  }
  if (GET_LEVEL(ch) >= LVL_IMMORT && !PLR_FLAGGED(ch, PLR_NOWIZLIST))
    run_autowiz();
}

void gain_condition(struct char_data *ch, int condition, int value)
{
  bool intoxicated;

  if (IS_NPC(ch) || GET_COND(ch, condition) == -1)	/* No change */
    return;
  if (GET_LEVEL(ch) >= LVL_IMMORT &&
      (condition == HUNGER || condition == THIRST))
    return;

  intoxicated = (GET_COND(ch, DRUNK) > 0);

  GET_COND(ch, condition) += value;

  GET_COND(ch, condition) = MAX(0, GET_COND(ch, condition));
  GET_COND(ch, condition) = MIN(24, GET_COND(ch, condition));

  if (GET_COND(ch, condition) || PLR_FLAGGED(ch, PLR_WRITING))
    return;

  switch (condition) {
  case HUNGER:
    send_to_char(ch, "You are hungry.\r\n");
    break;
  case THIRST:
    send_to_char(ch, "You are thirsty.\r\n");
    break;
  case DRUNK:
    if (intoxicated)
      send_to_char(ch, "You are now sober.\r\n");
    break;
  default:
    break;
  }

}

static void check_idling(struct char_data *ch)
{
  if (ch->char_specials.timer > CONFIG_IDLE_VOID) {
    if (GET_WAS_IN(ch) == NOWHERE && IN_ROOM(ch) != NOWHERE) {
      GET_WAS_IN(ch) = IN_ROOM(ch);
      if (FIGHTING(ch)) {
	stop_fighting(FIGHTING(ch));
	stop_fighting(ch);
      }
      act("$n disappears into the void.", TRUE, ch, 0, 0, TO_ROOM);
      send_to_char(ch, "You have been idle, and are pulled into a void.\r\n");
      save_char(ch);
      Crash_crashsave(ch);
      char_from_room(ch);
      char_to_room(ch, 1);
    } else if (ch->char_specials.timer > CONFIG_IDLE_RENT_TIME) {
      if (IN_ROOM(ch) != NOWHERE)
	char_from_room(ch);
      char_to_room(ch, 3);
      if (ch->desc) {
	STATE(ch->desc) = CON_DISCONNECT;
	/*
	 * For the 'if (d->character)' test in close_socket().
	 * -gg 3/1/98 (Happy anniversary.)
	 */
	ch->desc->character = NULL;
	ch->desc = NULL;
      }
      if (CONFIG_FREE_RENT)
	Crash_rentsave(ch, 0);
      else
	Crash_idlesave(ch);
      mudlog(CMP, MAX(LVL_GOD, GET_INVIS_LEV(ch)), TRUE, "%s force-rented and extracted (idle).", GET_NAME(ch));
      add_llog_entry(ch, LAST_IDLEOUT);
      extract_char(ch);
    }
  }
}

/* Update PCs, NPCs, and objects */
void point_update(void)
{
  struct char_data *i, *next_char;
  struct obj_data *j, *next_thing, *jj, *next_thing2;

  /* characters */
  for (i = character_list; i; i = next_char) {
    next_char = i->next;

    if (!IS_NPC(i) && i->char_specials.food_sated_ticks > 0)
      i->char_specials.food_sated_ticks--;
    else
      gain_condition(i, HUNGER, -1);
    gain_condition(i, DRUNK, -1);
    gain_condition(i, THIRST, -1);
    update_starvation_trackers(i);

    if (GET_POS(i) >= POS_STUNNED) {
      struct affected_type *corruption = NULL;

      GET_HIT(i) = MIN(GET_HIT(i) + hit_gain(i), GET_MAX_HIT(i));
      GET_MANA(i) = MIN(GET_MANA(i) + mana_gain(i), effective_max_mana(i));
      GET_MOVE(i) = MIN(GET_MOVE(i) + move_gain(i), effective_max_move(i));
      apply_condition_tick_penalties(i);
      if (AFF_FLAGGED(i, AFF_POISON))
        if (damage(i, i, 2, SPELL_POISON) == -1)
          continue;     /* Oops, they died. -gg 6/24/98 */
      corruption = find_affect(i, SPELL_CORRUPTION);
      if (corruption) {
        send_to_char(i, "Corrupting energy gnaws at you.\\r\\n");
        if (damage(i, i, (corruption->modifier < 1 ? 1 : corruption->modifier), SPELL_CORRUPTION) == -1)
          continue;
      }
      if (GET_POS(i) <= POS_STUNNED)
        update_pos(i);
    } else if (GET_POS(i) == POS_INCAP) {
      if (damage(i, i, 1, TYPE_SUFFERING) == -1)
	continue;
    } else if (GET_POS(i) == POS_MORTALLYW) {
      if (damage(i, i, 2, TYPE_SUFFERING) == -1)
	continue;
    }
    if (!IS_NPC(i)) {
      update_char_objects(i);
      (i->char_specials.timer)++;
      if (GET_LEVEL(i) < CONFIG_IDLE_MAX_LEVEL)
	check_idling(i);
    }
  }

  /* objects */
  for (j = object_list; j; j = next_thing) {
    next_thing = j->next;	/* Next in object list */

    /* If this is a corpse */
    if (IS_CORPSE(j)) {
      /* timer count down */
      if (GET_OBJ_TIMER(j) > 0)
	GET_OBJ_TIMER(j)--;

      if (!GET_OBJ_TIMER(j)) {

	if (j->carried_by)
	  act("$p decays in your hands.", FALSE, j->carried_by, j, 0, TO_CHAR);
	else if ((IN_ROOM(j) != NOWHERE) && (world[IN_ROOM(j)].people)) {
	  act("A quivering horde of maggots consumes $p.",
	      TRUE, world[IN_ROOM(j)].people, j, 0, TO_ROOM);
	  act("A quivering horde of maggots consumes $p.",
	      TRUE, world[IN_ROOM(j)].people, j, 0, TO_CHAR);
	}
	for (jj = j->contains; jj; jj = next_thing2) {
	  next_thing2 = jj->next_content;	/* Next in inventory */
	  obj_from_obj(jj);

	  if (j->in_obj)
	    obj_to_obj(jj, j->in_obj);
	  else if (j->carried_by)
	    obj_to_room(jj, IN_ROOM(j->carried_by));
	  else if (IN_ROOM(j) != NOWHERE)
	    obj_to_room(jj, IN_ROOM(j));
	  else
	    core_dump();
	}
	extract_obj(j);
      }
    }
    /* If the timer is set, count it down and at 0, try the trigger
     * note to .rej hand-patchers: make this last in your point-update() */
    else if (GET_OBJ_TIMER(j)>0) {
      GET_OBJ_TIMER(j)--;
      if (!GET_OBJ_TIMER(j)) {
        if (GET_OBJ_VNUM(j) == OBJVNUM_SPELL_PORTAL && IN_ROOM(j) != NOWHERE && world[IN_ROOM(j)].people) {
          act("The shimmering portal collapses.", TRUE, world[IN_ROOM(j)].people, 0, 0, TO_ROOM);
          act("The shimmering portal collapses.", TRUE, world[IN_ROOM(j)].people, 0, 0, TO_CHAR);
        }
        timer_otrigger(j);
      }
    }
  }

  /* Take 1 from the happy-hour tick counter, and end happy-hour if zero */
       if (HAPPY_TIME > 1)  HAPPY_TIME--;
  else if (HAPPY_TIME == 1)   /* Last tick - set everything back to zero */
  {
    HAPPY_QP = 0;
    HAPPY_EXP = 0;
    HAPPY_GOLD = 0;
    HAPPY_TIME = 0;
   game_info("Happy hour has ended!");
  }
}


long long increase_money_gold(struct char_data *ch, long long amt)
{
  long long curr = GET_MONEY(ch);
  long long updated = curr + amt;

  if (amt < 0) {
    if (amt < LLONG_MIN - curr)
      updated = 0LL;
    else if (updated < 0)
      updated = 0LL;
  } else {
    if (amt > LLONG_MAX - curr || updated > MAX_MONEY)
      updated = MAX_MONEY;
  }

  GET_MONEY(ch) = updated;

  if (GET_MONEY(ch) == MAX_MONEY)
    send_to_char(ch, "%sYou have reached the maximum currency!\r\n%sSpend or bank it before gaining more.\r\n", QBRED, QNRM);

  return GET_MONEY(ch);
}

long long increase_bank_gold(struct char_data *ch, long long amt)
{
  long long curr;
  long long updated;
  if (IS_NPC(ch)) return 0;

  curr = GET_BANK_MONEY(ch);
  updated = curr + amt;

  if (amt < 0) {
    if (amt < LLONG_MIN - curr)
      updated = 0LL;
    else if (updated < 0)
      updated = 0LL;
  } else {
    if (amt > LLONG_MAX - curr || updated > MAX_BANK)
      updated = MAX_BANK;
  }

  GET_BANK_MONEY(ch) = updated;

  if (GET_BANK_MONEY(ch) == MAX_BANK)
    send_to_char(ch, "%sYou have reached the maximum bank balance!\r\n%sWithdraw before depositing more.\r\n", QBRED, QNRM);

  return GET_BANK_MONEY(ch);
}


int increase_diamonds(struct char_data *ch, int amt)
{
  if (amt == 0) return GET_DIAMONDS(ch);
  if (amt < 0) GET_DIAMONDS(ch) = MAX(0, GET_DIAMONDS(ch) + amt);
  else {
    if (GET_DIAMONDS(ch) > 2000000000 - amt) GET_DIAMONDS(ch) = 2000000000;
    else GET_DIAMONDS(ch) += amt;
  }
  return GET_DIAMONDS(ch);
}

int decrease_diamonds(struct char_data *ch, int deduction)
{
  return increase_diamonds(ch, -deduction);
}

/* Legacy API: arguments are in gold units (authoritative currency unit). */
int increase_gold(struct char_data *ch, int amt_gold)
{
  return (int)increase_money_gold(ch, (long long)amt_gold);
}

int decrease_gold(struct char_data *ch, int deduction_gold)
{
  return increase_gold(ch, -deduction_gold);
}

int increase_bank(struct char_data *ch, int amt_gold)
{
  return (int)increase_bank_gold(ch, (long long)amt_gold);
}

int decrease_bank(struct char_data *ch, int deduction_gold)
{
  return increase_bank(ch, -deduction_gold);
}
