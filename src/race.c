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

void clamp_base_stats(struct char_data *ch)
{
  if (!ch)
    return;

  ch->real_abils.str   = MIN(MAX(ch->real_abils.str,   BASE_STAT_MIN), BASE_STAT_CAP);
  ch->real_abils.dex   = MIN(MAX(ch->real_abils.dex,   BASE_STAT_MIN), BASE_STAT_CAP);
  ch->real_abils.con   = MIN(MAX(ch->real_abils.con,   BASE_STAT_MIN), BASE_STAT_CAP);
  ch->real_abils.intel = MIN(MAX(ch->real_abils.intel, BASE_STAT_MIN), BASE_STAT_CAP);
  ch->real_abils.wis   = MIN(MAX(ch->real_abils.wis,   BASE_STAT_MIN), BASE_STAT_CAP);
  ch->real_abils.cha   = MIN(MAX(ch->real_abils.cha,   BASE_STAT_MIN), BASE_STAT_CAP);
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

  /* Racial adjustments can push stats over/under creation caps; clamp once */
  /* they're finalized so what the player selected is what gets saved.      */
  clamp_base_stats(ch);
  affect_total(ch);
}


/*
  Return the racial ability bonus for a given race.
  abil: 0 Str, 1 Dex, 2 Con, 3 Int, 4 Wis, 5 Cha
*/
int race_abil_bonus(int race, int abil)
{
  if (abil < 0 || abil > 5)
    return 0;

  switch (race) {
    case RACE_HUMAN:
      return 0;

    case RACE_ELF:
      switch (abil) {
        case 2: return -1; /* Con */
        case 3: return +1; /* Int */
        case 4: return +1; /* Wis */
        default: return 0;
      }

    case RACE_DWARF:
      switch (abil) {
        case 1: return -1; /* Dex */
        case 2: return +1; /* Con */
        case 5: return -1; /* Cha */
        default: return 0;
      }

    case RACE_ORC:
      switch (abil) {
        case 0: return +1; /* Str */
        case 1: return +1; /* Dex */
        case 3: return -1; /* Int */
        case 4: return -1; /* Wis */
        default: return 0;
      }

    case RACE_HALFLING:
      switch (abil) {
        case 0: return -1; /* Str */
        case 1: return +1; /* Dex */
        case 5: return +1; /* Cha */
        default: return 0;
      }

    case RACE_TROLL:
      switch (abil) {
        case 0: return +1; /* Str */
        case 2: return +1; /* Con */
        case 3: return -1; /* Int */
        case 4: return -1; /* Wis */
        default: return 0;
      }

    case RACE_GOBLIN:
      switch (abil) {
        case 1: return +1; /* Dex */
        case 3: return +1; /* Int */
        case 5: return -2; /* Cha */
        default: return 0;
      }

    case RACE_WEREWOLF:
      switch (abil) {
        case 0: return +2; /* Str */
        case 3: return -1; /* Int */
        case 5: return -1; /* Cha */
        default: return 0;
      }

    case RACE_SATYR:
      switch (abil) {
        case 1: return +1; /* Dex */
        case 4: return -2; /* Wis */
        case 5: return +1; /* Cha */
        default: return 0;
      }

    case RACE_MINOTAUR:
      switch (abil) {
        case 0: return +2; /* Str */
        case 1: return -1; /* Dex */
        case 3: return -1; /* Int */
        default: return 0;
      }

    case RACE_VAMPIRE:
      switch (abil) {
        case 2: return -1; /* Con */
        case 3: return +1; /* Int */
        case 5: return +1; /* Cha */
        default: return 0;
      }

    default:
      return 0;
  }
}
