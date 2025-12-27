#ifndef _RACE_H_
#define _RACE_H_

#include "structs.h"

#define RACE_UNDEFINED 0
#define RACE_HUMAN     1
#define RACE_ELF       2
#define RACE_DWARF     3
#define RACE_ORC       4
#define RACE_HALFLING  5
#define RACE_TROLL     6
#define RACE_GOBLIN    7
#define RACE_WEREWOLF  8
#define RACE_SATYR     9
#define RACE_MINOTAUR 10
#define RACE_VAMPIRE  11
#define NUM_RACES     12

extern const char *pc_race_types[];
extern const char *race_menu;

int parse_race(const char *arg);
void apply_racial_bonuses(struct char_data *ch);

#endif /* _RACE_H_ */
