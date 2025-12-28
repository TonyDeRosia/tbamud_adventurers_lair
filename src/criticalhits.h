#ifndef CRITICALHITS_H
#define CRITICALHITS_H

#include "structs.h"

int crit_base_melee(const struct char_data *ch);
int crit_base_spell(const struct char_data *ch);
int crit_base_heal(const struct char_data *ch);

int crit_total_melee(const struct char_data *ch);
int crit_total_spell(const struct char_data *ch);
int crit_total_heal(const struct char_data *ch);

int crit_mult_melee(const struct char_data *ch);
int crit_mult_spell(const struct char_data *ch);
int crit_mult_heal(const struct char_data *ch);

int roll_melee_crit(struct char_data *ch, int diceroll);
int roll_spell_crit(struct char_data *ch);
int roll_heal_crit(struct char_data *ch);

#endif
