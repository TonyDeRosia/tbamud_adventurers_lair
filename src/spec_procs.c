/**************************************************************************
*  File: spec_procs.c                                      Part of tbaMUD *
*  Usage: Implementation of special procedures for mobiles/objects/rooms. *
*                                                                         *
*  All rights reserved.  See license for complete information.            *
*                                                                         *
*  Copyright (C) 1993, 94 by the Trustees of the Johns Hopkins University *
*  CircleMUD is based on DikuMUD, Copyright (C) 1990, 1991.               *
**************************************************************************/

/* For more examples: 
 * ftp://ftp.circlemud.org/pub/CircleMUD/contrib/snippets/specials */

#include "conf.h"
#include "sysdep.h"
#include "structs.h"
#include "utils.h"
#include "comm.h"
#include "screen.h"
#include "interpreter.h"
#include "handler.h"
#include "db.h"
#include "spells.h"
#include "constants.h"
#include "act.h"
#include "spec_procs.h"
#include "class.h"
#include "tome.h"
#include "fight.h"
#include "modify.h"
#include "classtrack.h"

/*
 * Legacy NPC special-procedure policy:
 *
 * Core reusable services remain implemented here, including guild practice,
 * dump rooms, pet shops, and banks.
 *
 * Stock NPC personality, combat AI, schedules, patrols, reactions, guard
 * behavior, scavenging, theft, spellcasting, and similar world behavior are
 * retired from C and belong in DG Scripts/world data.
 */

#define PRACTICE_CAP 75



static void format_price_gsc(char *out, size_t outsz, long long total_gold)
{
  format_gold_as_currency(out, outsz, total_gold);
}
/* locally defined functions of local (file) scope */
static int compare_spells(const void *x, const void *y);
static void show_known_abilities(struct char_data *ch, bool include_spells, bool include_skills);

/* Special procedures for mobiles. */
static int spell_sort_info[MAX_SKILLS + 1];



static int compare_spells(const void *x, const void *y)
{
  int	a = *(const int *)x,
	b = *(const int *)y;

  return strcmp(spell_info[a].name, spell_info[b].name);
}

void sort_spells(void)
{
  int a;

  /* initialize array, avoiding reserved. */
  for (a = 1; a <= MAX_SKILLS; a++)
    spell_sort_info[a] = a;

  qsort(&spell_sort_info[1], MAX_SKILLS, sizeof(int), compare_spells);
}

static const char *prac_types[] = {
  "spell",
  "skill"
};

#define LEARNED(ch) (MIN(PRACTICE_CAP, get_class_prac_learned_level((int)GET_CLASS(ch))))

#define MINGAIN(ch) (get_class_prac_min_per_prac((int)GET_CLASS(ch)))
#define MAXGAIN(ch) (get_class_prac_max_per_prac((int)GET_CLASS(ch)))
#define SPLSKL(ch) (prac_types[get_class_prac_type((int)GET_CLASS(ch))])

static void show_known_abilities(struct char_data *ch, bool include_spells, bool include_skills)
{
  send_to_char(ch, "You have %d practice session%s remaining.\r\n",
               GET_PRACTICES(ch), GET_PRACTICES(ch) == 1 ? "" : "s");

  if (include_skills)
    show_ability_table_aligned(ch, FALSE, FALSE, NULL);

  if (include_spells)
    show_ability_table_aligned(ch, TRUE, FALSE, NULL);
}

void list_skills(struct char_data *ch)
{
  show_ability_table_aligned(ch, FALSE, FALSE, NULL);
}

void list_spells(struct char_data *ch)
{
  show_ability_table_aligned(ch, TRUE, FALSE, NULL);
}

void list_known_abilities(struct char_data *ch)
{
  show_known_abilities(ch, TRUE, TRUE);
}

static int can_character_practice_ability(struct char_data *ch, int ability_id)
{
  int learned_at;
  int is_reactive_identity;
  int required_level;

  if (!ch || IS_NPC(ch) || !is_valid_class(GET_CLASS(ch)))
    return FALSE;

  if (ability_id < 1 || ability_id > MAX_SKILLS)
    return FALSE;

  /* Stored legacy proficiency/learned levels cannot authorize Study. */
  if (ability_id == SKILL_STUDY && !character_has_ability_access(ch, ability_id))
    return FALSE;

  learned_at = GET_STUDY_LEARN_LEVEL(ch, ability_id);
  is_reactive_identity = (learned_at > 0);

  if (GET_SKILL(ch, ability_id) > 0) {
    if (has_tome_ability(ch, ability_id))
      return TRUE;
    if (is_reactive_identity) {
      required_level = classtrack_get_study_display_level(ch, ability_id, 1);
      return (GET_LEVEL(ch) >= required_level);
    }
    return TRUE;
  }

  required_level = spell_info[ability_id].min_level[(int) GET_CLASS(ch)];
  return (GET_LEVEL(ch) >= required_level);
}

SPECIAL(guild)
{
  int skill_num, percent;

  if (IS_NPC(ch) || !CMD_IS("practice"))
    return (FALSE);

  skip_spaces(&argument);

  if (!*argument) {
    list_known_abilities(ch);
    return (TRUE);
  }
  if (GET_PRACTICES(ch) <= 0) {
    send_to_char(ch, "You do not seem to be able to practice now.\r\n");
    return (TRUE);
  }

  {
    char ambiguity[MAX_STRING_LENGTH];

    skill_num = resolve_spell_by_player_input(ch, argument, FALSE, TRUE, FALSE,
        NULL, ambiguity, sizeof(ambiguity));
    if (skill_num == -2) {
      send_to_char(ch, "Ambiguous %s name. Did you mean: %s?\r\n",
          SPLSKL(ch), ambiguity);
      return (TRUE);
    }
  }

  if (!can_character_practice_ability(ch, skill_num)) {
    send_to_char(ch, "You do not know of that %s.\r\n", SPLSKL(ch));
    return (TRUE);
  }
  if (GET_SKILL(ch, skill_num) >= LEARNED(ch)) {
    send_to_char(ch, "You are already learned in that area.\r\n");
    return (TRUE);
  }
  send_to_char(ch, "You practice for a while...\r\n");
  GET_PRACTICES(ch)--;

  percent = GET_SKILL(ch, skill_num);
  percent += MIN(MAXGAIN(ch), MAX(MINGAIN(ch), int_app[GET_INT(ch)].learn));

  SET_SKILL(ch, skill_num, MIN(LEARNED(ch), percent));

  
  /* practice cap */
  if (GET_SKILL(ch, skill_num) > PRACTICE_CAP)
    SET_SKILL(ch, skill_num, PRACTICE_CAP);
if (GET_SKILL(ch, skill_num) >= LEARNED(ch))
    send_to_char(ch, "You are now learned in that area.\r\n");

  return (TRUE);
}

SPECIAL(dump)
{
  struct obj_data *k;
  int value = 0;

  for (k = world[IN_ROOM(ch)].contents; k; k = world[IN_ROOM(ch)].contents) {
    act("$p vanishes in a puff of smoke!", FALSE, 0, k, 0, TO_ROOM);
    extract_obj(k);
  }

  if (!CMD_IS("drop"))
    return (FALSE);

  do_drop(ch, argument, cmd, SCMD_DROP);

  for (k = world[IN_ROOM(ch)].contents; k; k = world[IN_ROOM(ch)].contents) {
    act("$p vanishes in a puff of smoke!", FALSE, 0, k, 0, TO_ROOM);
    value += MAX(1, MIN(50, GET_OBJ_COST(k) / 10));
    extract_obj(k);
  }

  if (value) {
    send_to_char(ch, "You are awarded for outstanding performance.\r\n");
    act("$n has been awarded for being a good citizen.", TRUE, ch, 0, 0, TO_ROOM);

    if (GET_LEVEL(ch) < 3)
      gain_exp(ch, value);
    else
      increase_gold(ch, value);
  }
  return (TRUE);
}

/* General special procedures for mobiles. */

/* Quite lethal to low-level characters. */
/* Special procedures for mobiles. */
 

static int pet_shop_price(struct char_data *pet)
{
  int price = GET_LEVEL(pet) * 300;

  if (GET_PET_PRICE(pet) > 0)
    return GET_PET_PRICE(pet);

  if (GET_MOB_RNUM(pet) != NOBODY) {
    struct char_data *proto = &mob_proto[GET_MOB_RNUM(pet)];

    if (GET_PET_PRICE(proto) > 0)
      price = GET_PET_PRICE(proto);
  }

  return price;
}

#define PET_PRICE(pet) (pet_shop_price(pet))
SPECIAL(pet_shops)
{
  char buf[MAX_STRING_LENGTH], pet_name[256];
  room_rnum pet_room;
  struct char_data *pet;

  /* Gross. */
  pet_room = IN_ROOM(ch) + 1;

  if (CMD_IS("list")) {
    send_to_char(ch, "Available pets are:\r\n");
    for (pet = world[pet_room].people; pet; pet = pet->next_in_room) {
      /* No, you can't have the Implementor as a pet if he's in there. */
      if (!IS_NPC(pet))
        continue;
      { long long _pc = (long long)PET_PRICE(pet); char _pb[64]; format_price_gsc(_pb, sizeof(_pb), _pc); send_to_char(ch, "%8s - %s\r\n", _pb, GET_NAME(pet)); }
    }
    return (TRUE);
  } else if (CMD_IS("buy") || CMD_IS("adopt")) {

    two_arguments(argument, buf, pet_name);

    if (CMD_IS("buy")) {
      send_to_char(ch, "Use ADOPT <pet> [name] for pet purchases.\r\n");
      return TRUE;
    }

    if (!(pet = get_char_room(buf, NULL, pet_room)) || !IS_NPC(pet)) {
      send_to_char(ch, "There is no such pet!\r\n");
      return (TRUE);
    }
    if ((long long)GET_MONEY(ch) < (long long)PET_PRICE(pet)) {
      char _pb[64]; format_price_gsc(_pb, sizeof(_pb), (long long)PET_PRICE(pet)); send_to_char(ch, "You do not have enough money. Cost is %s.\r\n", _pb);
      return (TRUE);
    }
    GET_MONEY(ch) = (long long)GET_MONEY(ch) - (long long)PET_PRICE(pet); if (GET_MONEY(ch) < 0) GET_MONEY(ch) = 0;

    pet = read_mobile(GET_MOB_RNUM(pet), REAL);
    GET_EXP(pet) = 0;
    SET_BIT_AR(AFF_FLAGS(pet), AFF_CHARM);

    if (*pet_name) {
      snprintf(buf, sizeof(buf), "%s %s", pet->player.name, pet_name);
      /* free(pet->player.name); don't free the prototype! */
      pet->player.name = strdup(buf);

      snprintf(buf, sizeof(buf), "%sA small sign on a chain around the neck says 'My name is %s'\r\n",
	      pet->player.description, pet_name);
      /* free(pet->player.description); don't free the prototype! */
      pet->player.description = strdup(buf);
    }
    char_to_room(pet, IN_ROOM(ch));
    add_follower(pet, ch);

    /* Be certain that pets can't get/carry/use/wield/wear items */
    IS_CARRYING_W(pet) = 1000;
    IS_CARRYING_N(pet) = 100;

    send_to_char(ch, "May you enjoy your pet.\r\n");
    act("$n buys $N as a pet.", FALSE, ch, 0, pet, TO_ROOM);

    return (TRUE);
  }

  /* All commands except list and adopt */
  return (FALSE);
}

/* Special procedures for objects. */
SPECIAL(bank)
{
  int amount;

  if (CMD_IS("balance")) {
    if (GET_BANK_GOLD(ch) > 0)
      send_to_char(ch, "Your current balance is %lld coins.\r\n", ((long long)GET_BANK_GOLD(ch)));
    else
      send_to_char(ch, "You currently have no money deposited.\r\n");
    return (TRUE);
  } else if (CMD_IS("deposit")) {
    if ((amount = atoi(argument)) <= 0) {
      send_to_char(ch, "How much do you want to deposit?\r\n");
      return (TRUE);
    }
    if (GET_GOLD(ch) < amount) {
      send_to_char(ch, "You don't have that many coins!\r\n");
      return (TRUE);
    }
    decrease_gold(ch, amount);
	increase_bank(ch, amount);
    send_to_char(ch, "You deposit %d coins.\r\n", amount);
    act("$n makes a bank transaction.", TRUE, ch, 0, FALSE, TO_ROOM);
    return (TRUE);
  } else if (CMD_IS("withdraw")) {
    if ((amount = atoi(argument)) <= 0) {
      send_to_char(ch, "How much do you want to withdraw?\r\n");
      return (TRUE);
    }
    if (GET_BANK_GOLD(ch) < amount) {
      send_to_char(ch, "You don't have that many coins deposited!\r\n");
      return (TRUE);
    }
    increase_gold(ch, amount);
	decrease_bank(ch, amount);
    send_to_char(ch, "You withdraw %d coins.\r\n", amount);
    act("$n makes a bank transaction.", TRUE, ch, 0, FALSE, TO_ROOM);
    return (TRUE);
  } else
    return (FALSE);
}
