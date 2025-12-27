#include "conf.h"
#include "sysdep.h"

#include "structs.h"
#include "utils.h"
#include "race.h"
#include "handler.h"   /* affect_total */

#include <ctype.h>
#include <stdlib.h>

const char *pc_race_types[] = {
  "Undefined",
  "Human",
  "Elf",
  "Dwarf",
  "Orc",
  "Halfling",
  "Troll",
  "Goblin",
  "Werewolf",
  "Satyr",
  "Minotaur",
  "Vampire",
  "\n"
};

const char *race_menu =
"\r\n"
"Please choose a race:\r\n"
" 1) Human       (h)\r\n"
" 2) Elf         (e)\r\n"
" 3) Dwarf       (d)\r\n"
" 4) Orc         (o)\r\n"
" 5) Halfling    (l)\r\n"
" 6) Troll       (t)\r\n"
" 7) Goblin      (g)\r\n"
" 8) Werewolf    (w)\r\n"
" 9) Satyr       (s)\r\n"
"10) Minotaur    (m)\r\n"
"11) Vampire     (v)\r\n"
"Enter race (number or letter): ";

int parse_race(const char *arg)
{
  int n;

  if (!arg || !*arg)
    return RACE_UNDEFINED;

  while (*arg && isspace((unsigned char)*arg))
    arg++;

  if (!*arg)
    return RACE_UNDEFINED;

  if (isdigit((unsigned char)arg[0])) {
    n = atoi(arg);
    if (n >= RACE_HUMAN && n < NUM_RACES)
      return n;
    return RACE_UNDEFINED;
  }

  switch (tolower((unsigned char)arg[0])) {
    case 'h': return RACE_HUMAN;
    case 'e': return RACE_ELF;
    case 'd': return RACE_DWARF;
    case 'o': return RACE_ORC;
    case 'l': return RACE_HALFLING;
    case 't': return RACE_TROLL;
    case 'g': return RACE_GOBLIN;
    case 'w': return RACE_WEREWOLF;
    case 's': return RACE_SATYR;
    case 'm': return RACE_MINOTAUR;
    case 'v': return RACE_VAMPIRE;
    default:  return RACE_UNDEFINED;
  }
}

static void clamp_abils(struct char_data *ch)
{
  if (ch->real_abils.str < 3) ch->real_abils.str = 3;
  if (ch->real_abils.dex < 3) ch->real_abils.dex = 3;
  if (ch->real_abils.con < 3) ch->real_abils.con = 3;
  if (ch->real_abils.intel < 3) ch->real_abils.intel = 3;
  if (ch->real_abils.wis < 3) ch->real_abils.wis = 3;
  if (ch->real_abils.cha < 3) ch->real_abils.cha = 3;

  if (ch->real_abils.str > 25) ch->real_abils.str = 25;
  if (ch->real_abils.dex > 25) ch->real_abils.dex = 25;
  if (ch->real_abils.con > 25) ch->real_abils.con = 25;
  if (ch->real_abils.intel > 25) ch->real_abils.intel = 25;
  if (ch->real_abils.wis > 25) ch->real_abils.wis = 25;
  if (ch->real_abils.cha > 25) ch->real_abils.cha = 25;
}

/*
  Apply racial bonuses once at creation time.
  Do not call this on login or load.
*/
void apply_racial_bonuses(struct char_data *ch)
{
  if (!ch)
    return;

  switch (GET_RACE(ch)) {
    case RACE_HUMAN:
      break;

    case RACE_ELF:
      ch->real_abils.intel += 1;
      ch->real_abils.wis   += 1;
      ch->real_abils.con   -= 1;
      break;

    case RACE_DWARF:
      ch->real_abils.con   += 1;
      ch->real_abils.dex   -= 1;
      ch->real_abils.cha   -= 1;
      GET_MAX_HIT(ch) += 5;
      GET_HIT(ch)     += 5;
      GET_AC(ch)      -= 1;
      break;

    case RACE_ORC:
      ch->real_abils.str   += 1;
      ch->real_abils.dex   += 1;
      ch->real_abils.intel -= 1;
      ch->real_abils.wis   -= 1;
      break;

    case RACE_HALFLING:
      ch->real_abils.dex   += 1;
      ch->real_abils.cha   += 1;
      ch->real_abils.str   -= 1;
      break;

    case RACE_TROLL:
      ch->real_abils.con   += 1;
      ch->real_abils.str   += 1;
      ch->real_abils.intel -= 1;
      ch->real_abils.wis   -= 1;
      break;

    case RACE_GOBLIN:
      ch->real_abils.dex   += 1;
      ch->real_abils.intel += 1;
      ch->real_abils.cha   -= 2;
      break;

    case RACE_WEREWOLF:
      ch->real_abils.str   += 2;
      ch->real_abils.cha   -= 1;
      ch->real_abils.intel -= 1;
      break;

    case RACE_SATYR:
      ch->real_abils.cha   += 1;
      ch->real_abils.dex   += 1;
      ch->real_abils.wis   -= 2;
      break;

    case RACE_MINOTAUR:
      ch->real_abils.str   += 2;
      ch->real_abils.dex   -= 1;
      ch->real_abils.intel -= 1;
      break;

    case RACE_VAMPIRE:
      ch->real_abils.intel += 1;
      ch->real_abils.cha   += 1;
      ch->real_abils.con   -= 1;
      break;

    default:
      break;
  }

  clamp_abils(ch);
  affect_total(ch);
}
