/**************************************************************************
*  File: spells.c                                          Part of tbaMUD *
*  Usage: Implementation of "manual spells."                              *
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
#include "spells.h"
#include "handler.h"
#include "db.h"
#include "constants.h"
#include "interpreter.h"
#include "dg_scripts.h"
#include "act.h"
#include "fight.h"
#include "criticalhits.h"
#include "mud_event.h"

static int clampi(int v, int lo, int hi)
{
  if (v < lo)
    return lo;
  if (v > hi)
    return hi;
  return v;
}

static int warlock_power(struct char_data *ch)
{
  return GET_INT(ch) + GET_WIS(ch);
}

/* Handle followers when an owner teleports or recalls. */
void handle_followers_after_owner_teleport_or_recall(struct char_data *ch)
{
  struct follow_type *f, *next;

  if (!ch)
    return;

  for (f = ch->followers; f; f = next) {
    struct char_data *follower = f->follower;

    next = f->next;

    if (!follower || follower->master != ch)
      continue;

    if (is_purchased_pet(ch, follower)) {
      room_rnum to_room = IN_ROOM(ch);

      if (IN_ROOM(follower) == to_room)
        continue;

      act("$n disappears.", TRUE, follower, 0, 0, TO_ROOM);
      char_from_room(follower);
      char_to_room(follower, to_room);
      act("$n arrives at $N's side.", TRUE, follower, 0, ch, TO_ROOM);
      look_at_room(follower, 0);
    } else if (!IS_NPC(follower) && AFF_FLAGGED(follower, AFF_CHARM)) {
      break_charm_follower(ch, follower);
    }
  }
}

static int corruption_duration(int level)
{
  if (level >= 100)
    return 8;
  if (level >= 80)
    return 8;
  if (level >= 60)
    return 7;
  if (level >= 40)
    return 6;
  if (level >= 20)
    return 5;
  return 4;
}

static int corruption_damage_per_tick(int level)
{
  int damage = 1 + (level / 10);

  return MIN(damage, 12);
}

/* Special spells appear below. */
ASPELL(spell_create_water)
{
  int water;

  if (ch == NULL || obj == NULL)
    return;
  /* level = MAX(MIN(level, LVL_IMPL), 1);	 - not used */

  if (GET_OBJ_TYPE(obj) == ITEM_DRINKCON) {
    if ((GET_OBJ_VAL(obj, 2) != LIQ_WATER) && (GET_OBJ_VAL(obj, 1) != 0)) {
      name_from_drinkcon(obj);
      GET_OBJ_VAL(obj, 2) = LIQ_SLIME;
      name_to_drinkcon(obj, LIQ_SLIME);
    } else {
      water = MAX(GET_OBJ_VAL(obj, 0) - GET_OBJ_VAL(obj, 1), 0);
      if (water > 0) {
	if (GET_OBJ_VAL(obj, 1) >= 0)
	  name_from_drinkcon(obj);
	GET_OBJ_VAL(obj, 2) = LIQ_WATER;
	GET_OBJ_VAL(obj, 1) += water;
	name_to_drinkcon(obj, LIQ_WATER);
	weight_change_object(obj, water);
	act("$p is filled.", FALSE, ch, obj, 0, TO_CHAR);
      }
    }
  }
}

ASPELL(spell_recall)
{
  if (victim == NULL || IS_NPC(victim))
    return;

  if (ZONE_FLAGGED(GET_ROOM_ZONE(IN_ROOM(victim)), ZONE_NOASTRAL)) {
    send_to_char(ch, "A bright flash prevents your spell from working!");
    return;
  }

  act("$n disappears.", TRUE, victim, 0, 0, TO_ROOM);
  char_from_room(victim);
  char_to_room(victim, r_mortal_start_room);
  act("$n appears in the middle of the room.", TRUE, victim, 0, 0, TO_ROOM);
  look_at_room(victim, 0);
  entry_memory_mtrigger(victim);
  greet_mtrigger(victim, -1);
  greet_memory_mtrigger(victim);
  handle_followers_after_owner_teleport_or_recall(victim);
}

ASPELL(spell_teleport)
{
  room_rnum to_room;

  if (victim == NULL || IS_NPC(victim))
    return;

  if (ZONE_FLAGGED(GET_ROOM_ZONE(IN_ROOM(victim)), ZONE_NOASTRAL)) {
    send_to_char(ch, "A bright flash prevents your spell from working!");
    return;
  }

  do {
    to_room = rand_number(0, top_of_world);
  } while (ROOM_FLAGGED(to_room, ROOM_PRIVATE) || ROOM_FLAGGED(to_room, ROOM_DEATH) ||
           ROOM_FLAGGED(to_room, ROOM_GODROOM) || ZONE_FLAGGED(GET_ROOM_ZONE(to_room), ZONE_CLOSED) ||
           ZONE_FLAGGED(GET_ROOM_ZONE(to_room), ZONE_NOASTRAL));

  act("$n slowly fades out of existence and is gone.",
      FALSE, victim, 0, 0, TO_ROOM);
  char_from_room(victim);
  char_to_room(victim, to_room);
  act("$n slowly fades into existence.", FALSE, victim, 0, 0, TO_ROOM);
  look_at_room(victim, 0);
  entry_memory_mtrigger(victim);
  greet_mtrigger(victim, -1);
  greet_memory_mtrigger(victim);
  handle_followers_after_owner_teleport_or_recall(victim);
}

#define SUMMON_FAIL "You failed.\r\n"
ASPELL(spell_summon)
{
  if (ch == NULL || victim == NULL)
    return;

  if (GET_LEVEL(victim) > MIN(LVL_IMMORT - 1, level + 3)) {
    send_to_char(ch, "%s", SUMMON_FAIL);
    return;
  }

  if (ZONE_FLAGGED(GET_ROOM_ZONE(IN_ROOM(victim)), ZONE_NOASTRAL) ||
      ZONE_FLAGGED(GET_ROOM_ZONE(IN_ROOM(ch)), ZONE_NOASTRAL)) {
    send_to_char(ch, "A bright flash prevents your spell from working!");
    return;
  }

  if (!CONFIG_PK_ALLOWED) {
    if (MOB_FLAGGED(victim, MOB_AGGRESSIVE)) {
      act("As the words escape your lips and $N travels\r\n"
	  "through time and space towards you, you realize that $E is\r\n"
	  "aggressive and might harm you, so you wisely send $M back.",
	  FALSE, ch, 0, victim, TO_CHAR);
      return;
    }
    if (!IS_NPC(victim) && !PRF_FLAGGED(victim, PRF_SUMMONABLE) &&
	!PLR_FLAGGED(victim, PLR_KILLER)) {
      send_to_char(victim, "%s just tried to summon you to: %s.\r\n"
	      "This failed because you have summon protection on.\r\n"
	      "Type NOSUMMON to allow other players to summon you.\r\n",
	      GET_NAME(ch), world[IN_ROOM(ch)].name);

      send_to_char(ch, "You failed because %s has summon protection on.\r\n", GET_NAME(victim));
      mudlog(BRF, MAX(LVL_IMMORT, MAX(GET_INVIS_LEV(ch), GET_INVIS_LEV(victim))), TRUE, 
        "%s failed summoning %s to %s.", GET_NAME(ch), GET_NAME(victim), world[IN_ROOM(ch)].name);
      return;
    }
  }

  if (MOB_FLAGGED(victim, MOB_NOSUMMON) ||
      (IS_NPC(victim) && mag_savingthrow(victim, SAVING_SPELL, 0))) {
    send_to_char(ch, "%s", SUMMON_FAIL);
    return;
  }

  act("$n disappears suddenly.", TRUE, victim, 0, 0, TO_ROOM);

  char_from_room(victim);
  char_to_room(victim, IN_ROOM(ch));

  act("$n arrives suddenly.", TRUE, victim, 0, 0, TO_ROOM);
  act("$n has summoned you!", FALSE, ch, 0, victim, TO_VICT);
  look_at_room(victim, 0);
  entry_memory_mtrigger(victim);
  greet_mtrigger(victim, -1);
  greet_memory_mtrigger(victim);
}

/* Used by the locate object spell to check the alias list on objects */
static int isname_obj(char *search, char *list)
{
  char *found_in_list; /* But could be something like 'ring' in 'shimmering.' */
  char searchname[128];
  char namelist[MAX_STRING_LENGTH];
  int found_pos = -1;
  int found_name=0; /* found the name we're looking for */
  int match = 1;
  int i;

  /* Force to lowercase for string comparisons */
  sprintf(searchname, "%s", search);
  for (i = 0; searchname[i]; i++)
    searchname[i] = LOWER(searchname[i]);

  sprintf(namelist, "%s", list);
  for (i = 0; namelist[i]; i++)
    namelist[i] = LOWER(namelist[i]);

  /* see if searchname exists any place within namelist */
  found_in_list = strstr(namelist, searchname);
  if (!found_in_list) {
    return 0;
  }

  /* Found the name in the list, now see if it's a valid hit. The following
   * avoids substrings (like ring in shimmering) is it at beginning of
   * namelist? */
  for (i = 0; searchname[i]; i++)
    if (searchname[i] != namelist[i])
      match = 0;

  if (match) /* It was found at the start of the namelist string. */
    found_name = 1;
  else { /* It is embedded inside namelist. Is it preceded by a space? */
    found_pos = found_in_list - namelist;
    if (namelist[found_pos-1] == ' ')
      found_name = 1;
  }

  if (found_name)
    return 1;
  else
    return 0;
}

ASPELL(spell_locate_object)
{
  struct obj_data *i;
  char name[MAX_INPUT_LENGTH];
  int j;

  if (!obj) {
    send_to_char(ch, "You sense nothing.\r\n");
    return;
  }

  /*  added a global var to catch 2nd arg. */
  sprintf(name, "%s", cast_arg2);

  j = GET_LEVEL(ch) / 2;  /* # items to show = twice char's level */

  for (i = object_list; i && (j > 0); i = i->next) {
    if (!isname_obj(name, i->name))
      continue;

  send_to_char(ch, "%c%s", UPPER(*i->short_description), i->short_description + 1);

    if (i->carried_by)
      send_to_char(ch, " is being carried by %s.\r\n", PERS(i->carried_by, ch));
    else if (IN_ROOM(i) != NOWHERE)
      send_to_char(ch, " is in %s.\r\n", world[IN_ROOM(i)].name);
    else if (i->in_obj)
      send_to_char(ch, " is in %s.\r\n", i->in_obj->short_description);
    else if (i->worn_by)
      send_to_char(ch, " is being worn by %s.\r\n", PERS(i->worn_by, ch));
    else
      send_to_char(ch, "'s location is uncertain.\r\n");

    j--;
  }
}

ASPELL(spell_charm)
{
  struct affected_type af;

  if (victim == NULL || ch == NULL)
    return;

  if (victim == ch)
    send_to_char(ch, "You like yourself even better!\r\n");
  else if (!IS_NPC(victim) && !PRF_FLAGGED(victim, PRF_SUMMONABLE))
    send_to_char(ch, "You fail because SUMMON protection is on!\r\n");
  else if (AFF_FLAGGED(victim, AFF_SANCTUARY))
    send_to_char(ch, "Your victim is protected by sanctuary!\r\n");
  else if (MOB_FLAGGED(victim, MOB_NOCHARM))
    send_to_char(ch, "Your victim resists!\r\n");
  else if (AFF_FLAGGED(ch, AFF_CHARM))
    send_to_char(ch, "You can't have any followers of your own!\r\n");
  else if (AFF_FLAGGED(victim, AFF_CHARM) || level < GET_LEVEL(victim))
    send_to_char(ch, "You fail.\r\n");
  /* player charming another player - no legal reason for this */
  else if (!CONFIG_PK_ALLOWED && !IS_NPC(victim))
    send_to_char(ch, "You fail - shouldn't be doing it anyway.\r\n");
  else if (circle_follow(victim, ch))
    send_to_char(ch, "Sorry, following in circles is not allowed.\r\n");
  else if (mag_savingthrow(victim, SAVING_PARA, 0))
    send_to_char(ch, "Your victim resists!\r\n");
  else {
    if (victim->master)
      stop_follower(victim);

    add_follower(victim, ch);

    new_affect(&af);
    af.spell = SPELL_CHARM;
    af.duration = 24 * 2;
    if (GET_CHA(ch))
      af.duration *= GET_CHA(ch);
    if (GET_INT(victim))
      af.duration /= GET_INT(victim);
    SET_BIT_AR(af.bitvector, AFF_CHARM);
    affect_to_char(victim, &af);

    act("Isn't $n just such a nice fellow?", FALSE, ch, 0, victim, TO_VICT);
    if (IS_NPC(victim))
      REMOVE_BIT_AR(MOB_FLAGS(victim), MOB_SPEC);
  }
}

ASPELL(spell_corruption)
{
  struct affected_type af;
  int caster_level = GET_LEVEL(ch);

  if (victim == NULL || ch == NULL)
    return;

  new_affect(&af);
  af.spell = SPELL_CORRUPTION;
  af.duration = corruption_duration(caster_level);
  af.modifier = corruption_damage_per_tick(caster_level);
  af.location = APPLY_NONE;

  affect_join(victim, &af, FALSE, FALSE, FALSE, FALSE);

  act("You envelop $N in a wave of corrupting energy.", FALSE, ch, 0, victim, TO_CHAR);
  act("$n envelopes $N in a wave of corrupting energy.", TRUE, ch, 0, victim, TO_ROOM);

  if (ch != victim)
  /* 0 damage prints like a miss. Make the initial hit at least 1. */
  damage(ch, victim, (af.modifier < 1 ? 1 : af.modifier), SPELL_CORRUPTION);

}

ASPELL(spell_plague_bolt)
{
  struct affected_type af;
  int power;
  int dam;
  int mult = 200;
  int pen;
  int dur_ticks;

  if (victim == NULL || ch == NULL)
    return;

  act("You fling a \tGplague bolt\tn at $N!\tn", FALSE, ch, 0, victim, TO_CHAR);
  act("$n flings a \tGplague bolt\tn at you!\tn", FALSE, ch, 0, victim, TO_VICT);
  act("$n flings a \tGplague bolt\tn at $N!\tn", TRUE, ch, 0, victim, TO_ROOM);

  power = warlock_power(ch);
  dam = dice(3, 6) + (level / 2) + (power / 5);
  if (mag_savingthrow(victim, SAVING_SPELL, 0))
    dam = (dam * 75) / 100;

  if (crit_check_spell(ch, &mult)) {
    dam = (dam * mult) / 100;
    crit_show_banner(ch, victim, mult);
  }

  if (dam < 1)
    dam = 1;

  if (damage(ch, victim, dam, SPELL_PLAGUE_BOLT) == -1)
    return;

  pen = clampi(1 + MAX(0, power - 20) / 10, 1, 4);
  dur_ticks = clampi(2 + MAX(0, power - 20) / 12, 2, 6);

  new_affect(&af);
  af.spell = SPELL_PLAGUE_BOLT;
  af.duration = dur_ticks;
  af.modifier = -pen;
  af.location = APPLY_STR;
  SET_BIT_AR(af.bitvector, AFF_POISON);
  affect_join(victim, &af, FALSE, FALSE, FALSE, FALSE);

  act("\tGSickness\tn spreads through $N's body!\tn", FALSE, ch, 0, victim, TO_CHAR);
  act("\tGSickness\tn spreads through your body!\tn", FALSE, ch, 0, victim, TO_VICT);
  act("\tGSickness\tn spreads through $N's body!\tn", TRUE, ch, 0, victim, TO_ROOM);
}

ASPELL(spell_enfeeblement)
{
  struct affected_type af;
  struct mud_event_data *event;
  int power;
  int stat_pen;
  int dur_sec;

  if (victim == NULL || ch == NULL)
    return;

  act("You whisper a \tDcruel hex\tn and sap $N's strength.\tn", FALSE, ch, 0, victim, TO_CHAR);
  act("$n whispers a \tDcruel hex\tn and your limbs go weak.\tn", FALSE, ch, 0, victim, TO_VICT);
  act("$n whispers a \tDcruel hex\tn and $N's limbs go weak.\tn", TRUE, ch, 0, victim, TO_ROOM);

  if (mag_savingthrow(victim, SAVING_SPELL, 0)) {
    act("$N shakes off your \tDhex\tn.\tn", FALSE, ch, 0, victim, TO_CHAR);
    act("You shake off $n's \tDhex\tn.\tn", FALSE, ch, 0, victim, TO_VICT);
    act("$N shakes off $n's \tDhex\tn.\tn", TRUE, ch, 0, victim, TO_ROOM);
    return;
  }

  power = warlock_power(ch);
  stat_pen = clampi(1 + MAX(0, power - 20) / 8, 1, 6);
  dur_sec = 30 + MIN(60, MAX(0, power - 20) * 2);
  dur_sec = clampi(dur_sec, 30, 90);

  if (affected_by_spell(victim, SPELL_ENFEEBLEMENT))
    affect_from_char(victim, SPELL_ENFEEBLEMENT);

  new_affect(&af);
  af.spell = SPELL_ENFEEBLEMENT;
  af.duration = -1;
  af.modifier = -stat_pen;
  af.location = APPLY_STR;
  affect_join(victim, &af, FALSE, FALSE, FALSE, FALSE);

  af.location = APPLY_DEX;
  affect_join(victim, &af, FALSE, FALSE, FALSE, FALSE);

  event = char_has_mud_event(victim, eSPL_ENFEEBLEMENT);
  if (event && event->pEvent)
    event_cancel(event->pEvent);

  NEW_EVENT(eSPL_ENFEEBLEMENT, victim, NULL, dur_sec * PASSES_PER_SEC);
}

ASPELL(spell_devour_soul)
{
  struct affected_type af;
  int power;
  int dam;
  int mult = 200;
  int pct;
  int mana_d;
  int move_d;
  int hr_pen;
  int sv_pen;
  int dur_ticks;

  if (victim == NULL || ch == NULL)
    return;

  act("You reach out with \tDcold jaws\tn to \tDevour\tn $N's soul!\tn", FALSE, ch, 0, victim, TO_CHAR);
  act("$n reaches out with \tDcold jaws\tn to \tDevour\tn your soul!\tn", FALSE, ch, 0, victim, TO_VICT);
  act("$n reaches out with \tDcold jaws\tn to \tDevour\tn $N's soul!\tn", TRUE, ch, 0, victim, TO_ROOM);

  power = warlock_power(ch);
  dam = dice(8, 15) + (level * 3) + (power * 3 / 2);
  pct = clampi(1 + MAX(0, power - 20) / 2, 1, 15);

  if (mag_savingthrow(victim, SAVING_SPELL, 0)) {
    dam = (dam * 75) / 100;
    pct = MAX(1, pct / 2);
  }

  if (crit_check_spell(ch, &mult)) {
    dam = (dam * mult) / 100;
    crit_show_banner(ch, victim, mult);
  }

  if (damage(ch, victim, dam, SPELL_DEVOUR_SOUL) == -1)
    return;

  mana_d = MAX(1, (GET_MAX_MANA(victim) * pct) / 100);
  move_d = MAX(1, (GET_MAX_MOVE(victim) * pct) / 100);
  mana_d = MIN(mana_d, GET_MANA(victim));
  move_d = MIN(move_d, GET_MOVE(victim));

  GET_MANA(victim) = MAX(0, GET_MANA(victim) - mana_d);
  GET_MOVE(victim) = MAX(0, GET_MOVE(victim) - move_d);
  GET_MANA(ch) = MIN(GET_MAX_MANA(ch), GET_MANA(ch) + mana_d);
  GET_MOVE(ch) = MIN(GET_MAX_MOVE(ch), GET_MOVE(ch) + move_d);

  act("You drink in $N's essence, restoring your power.\tn", FALSE, ch, 0, victim, TO_CHAR);
  act("You feel your power ripped away as $n feeds on you!\tn", FALSE, ch, 0, victim, TO_VICT);
  act("$n feeds on $N's essence!\tn", TRUE, ch, 0, victim, TO_ROOM);

  hr_pen = clampi(1 + MAX(0, power - 20) / 8, 1, 6);
  sv_pen = clampi(1 + MAX(0, power - 20) / 6, 1, 8);
  dur_ticks = clampi(2 + MAX(0, power - 20) / 12, 2, 6);

  new_affect(&af);
  af.spell = SPELL_DEVOUR_SOUL;
  af.duration = dur_ticks;
  af.modifier = -hr_pen;
  af.location = APPLY_HITROLL;
  affect_join(victim, &af, FALSE, FALSE, FALSE, FALSE);

  af.modifier = sv_pen;
  af.location = APPLY_SAVING_SPELL;
  affect_join(victim, &af, FALSE, FALSE, FALSE, FALSE);
}

ASPELL(spell_identify)
{
  int i, found;
  size_t len;

  if (obj) {
    char bitbuf[MAX_STRING_LENGTH];

    sprinttype(GET_OBJ_TYPE(obj), item_types, bitbuf, sizeof(bitbuf));
    send_to_char(ch, "You feel informed:\r\nObject '%s', Item type: %s\r\n", obj->short_description, bitbuf);

    if (GET_OBJ_AFFECT(obj)) {
      sprintbitarray(GET_OBJ_AFFECT(obj), affected_bits, AF_ARRAY_MAX, bitbuf);
      send_to_char(ch, "Item will give you following abilities:  %s\r\n", bitbuf);
    }

    sprintbitarray(GET_OBJ_EXTRA(obj), extra_bits, EF_ARRAY_MAX, bitbuf);
    send_to_char(ch, "Item is: %s\r\n", bitbuf);

    send_to_char(ch, "Weight: %d, Value: %d, Rent: %d, Min. level: %d\r\n",
                     GET_OBJ_WEIGHT(obj), GET_OBJ_COST(obj), GET_OBJ_RENT(obj), GET_OBJ_LEVEL(obj));

    switch (GET_OBJ_TYPE(obj)) {
    case ITEM_SCROLL:
    case ITEM_POTION:
      len = i = 0;

      if (GET_OBJ_VAL(obj, 1) >= 1) {
	i = snprintf(bitbuf + len, sizeof(bitbuf) - len, " %s", skill_name(GET_OBJ_VAL(obj, 1)));
        if (i >= 0)
          len += i;
      }

      if (GET_OBJ_VAL(obj, 2) >= 1 && len < sizeof(bitbuf)) {
	i = snprintf(bitbuf + len, sizeof(bitbuf) - len, " %s", skill_name(GET_OBJ_VAL(obj, 2)));
        if (i >= 0)
          len += i;
      }

      if (GET_OBJ_VAL(obj, 3) >= 1 && len < sizeof(bitbuf)) {
	snprintf(bitbuf + len, sizeof(bitbuf) - len, " %s", skill_name(GET_OBJ_VAL(obj, 3)));
      }

      send_to_char(ch, "This %s casts: %s\r\n", item_types[(int) GET_OBJ_TYPE(obj)], bitbuf);
      break;
    case ITEM_WAND:
    case ITEM_STAFF:
      send_to_char(ch, "This %s casts: %s\r\nIt has %d maximum charge%s and %d remaining.\r\n",
		item_types[(int) GET_OBJ_TYPE(obj)], skill_name(GET_OBJ_VAL(obj, 3)),
		GET_OBJ_VAL(obj, 1), GET_OBJ_VAL(obj, 1) == 1 ? "" : "s", GET_OBJ_VAL(obj, 2));
      break;
    case ITEM_WEAPON:
      send_to_char(ch, "Damage Dice is '%dD%d' for an average per-round damage of %.1f.\r\n",
		GET_OBJ_VAL(obj, 1), GET_OBJ_VAL(obj, 2), ((GET_OBJ_VAL(obj, 2) + 1) / 2.0) * GET_OBJ_VAL(obj, 1));
      break;
    case ITEM_ARMOR:
      send_to_char(ch, "AC-apply is %d\r\n", GET_OBJ_VAL(obj, 0));
      break;
    }
    found = FALSE;
    for (i = 0; i < MAX_OBJ_AFFECT; i++) {
      if ((obj->affected[i].location != APPLY_NONE) &&
	  (obj->affected[i].modifier != 0)) {
	if (!found) {
	  send_to_char(ch, "Can affect you as :\r\n");
	  found = TRUE;
	}
	sprinttype(obj->affected[i].location, apply_types, bitbuf, sizeof(bitbuf));
	send_to_char(ch, "   Affects: %s By %d\r\n", bitbuf, obj->affected[i].modifier);
      }
    }
  } else if (victim) {		/* victim */
    send_to_char(ch, "Name: %s\r\n", GET_NAME(victim));
    if (!IS_NPC(victim))
      send_to_char(ch, "%s is %d years, %d months, %d days and %d hours old.\r\n",
	      GET_NAME(victim), age(victim)->year, age(victim)->month,
	      age(victim)->day, age(victim)->hours);
    send_to_char(ch, "Height %d cm, Weight %d pounds\r\n", GET_HEIGHT(victim), GET_WEIGHT(victim));
    send_to_char(ch, "Level: %d, Hits: %d, Mana: %d\r\n", GET_LEVEL(victim), GET_HIT(victim), GET_MANA(victim));
    send_to_char(ch, "AC: %d, Hitroll: %d, Damroll: %d\r\n", compute_armor_class(victim), GET_HITROLL(victim), GET_DAMROLL(victim));
    send_to_char(ch, "Str: %d/%d, Int: %d, Wis: %d, Dex: %d, Con: %d, Cha: %d\r\n",
	GET_STR(victim), GET_ADD(victim), GET_INT(victim),
	GET_WIS(victim), GET_DEX(victim), GET_CON(victim), GET_CHA(victim));
  }
}

/* Cannot use this spell on an equipped object or it will mess up the wielding
 * character's hit/dam totals. */
ASPELL(spell_enchant_weapon)
{
  int i;

  if (ch == NULL || obj == NULL)
    return;

  /* Either already enchanted or not a weapon. */
  if (GET_OBJ_TYPE(obj) != ITEM_WEAPON || OBJ_FLAGGED(obj, ITEM_MAGIC))
    return;

  /* Make sure no other affections. */
  for (i = 0; i < MAX_OBJ_AFFECT; i++)
    if (obj->affected[i].location != APPLY_NONE)
      return;

  SET_BIT_AR(GET_OBJ_EXTRA(obj), ITEM_MAGIC);

  obj->affected[0].location = APPLY_HITROLL;
  obj->affected[0].modifier = 1 + (level >= 18);

  obj->affected[1].location = APPLY_DAMROLL;
  obj->affected[1].modifier = 1 + (level >= 20);

  if (IS_GOOD(ch)) {
    SET_BIT_AR(GET_OBJ_EXTRA(obj), ITEM_ANTI_EVIL);
    act("$p glows blue.", FALSE, ch, obj, 0, TO_CHAR);
  } else if (IS_EVIL(ch)) {
    SET_BIT_AR(GET_OBJ_EXTRA(obj), ITEM_ANTI_GOOD);
    act("$p glows red.", FALSE, ch, obj, 0, TO_CHAR);
  } else
    act("$p glows yellow.", FALSE, ch, obj, 0, TO_CHAR);
}

ASPELL(spell_detect_poison)
{
  if (victim) {
    if (victim == ch) {
      if (AFF_FLAGGED(victim, AFF_POISON))
        send_to_char(ch, "You can sense poison in your blood.\r\n");
      else
        send_to_char(ch, "You feel healthy.\r\n");
    } else {
      if (AFF_FLAGGED(victim, AFF_POISON))
        act("You sense that $E is poisoned.", FALSE, ch, 0, victim, TO_CHAR);
      else
        act("You sense that $E is healthy.", FALSE, ch, 0, victim, TO_CHAR);
    }
  }

  if (obj) {
    switch (GET_OBJ_TYPE(obj)) {
    case ITEM_DRINKCON:
    case ITEM_FOUNTAIN:
    case ITEM_FOOD:
      if (GET_OBJ_VAL(obj, 3))
	act("You sense that $p has been contaminated.",FALSE,ch,obj,0,TO_CHAR);
      else
	act("You sense that $p is safe for consumption.", FALSE, ch, obj, 0,
	    TO_CHAR);
      break;
    default:
      send_to_char(ch, "You sense that it should not be consumed.\r\n");
    }
  }
}
