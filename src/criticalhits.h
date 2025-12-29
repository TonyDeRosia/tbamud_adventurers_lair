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

const char *crit_banner_for_mult(int mult);

/* Crit banner helpers (mult is percent: 200=2x, 300=3x, 400=4x) */
const char *crit_banner_for_mult(int mult);
void crit_show_banner(struct char_data *ch, struct char_data *victim, int mult);

/* Simple crit checks wired at damage/mag_damage/mag_points */
int crit_check_melee(struct char_data *ch, int *mult);
int crit_check_spell(struct char_data *ch, int *mult);
int crit_check_heal(struct char_data *ch, int *mult);

#endif
