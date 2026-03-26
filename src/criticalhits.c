#include "conf.h"
#include "sysdep.h"

#include "structs.h"
#include "utils.h"
#include "criticalhits.h"
#include "comm.h"
#include "fight.h"

/*
 * Crit banner formatting
 * mult is percent: 200=2x, 300=3x, 400=4x
 * Uses { color codes per your banner style.
 */
const char *crit_banner_for_mult(int mult)
{
  if (mult >= 400)
    return "\tY#\tW#\tY#\tW#\tY C\tW E\tY N\tW S\tY O\tW R\tY #\tW#\tY#\tW#\tn";
  if (mult >= 300)
    return "\tR#\tY#\tR#\tY#\tR C\tY E\tR N\tY S\tR O\tY R\tR #\tY#\tR#\tY#\tn";
  return "\tD#\tW#\tD#\tW#\tD C\tW E\tD N\tW S\tD O\tW R\tD #\tW#\tD#\tW#\tn";
}


static int clamp_percent(int v)
{
  if (v < 0) return 0;
  if (v > 100) return 100;
  return v;
}

static int clamp_mult(int v)
{
  if (v < 100) return 100;
  return v;
}

int crit_base_melee(const struct char_data *ch)
{
  int dex_for_combat = combat_effective_stat(ch, APPLY_DEX);
  return (dex_for_combat / 2) + MAX(0, (dex_for_combat - 10) / 3);
}

int crit_base_spell(const struct char_data *ch)
{
  int int_for_combat = combat_effective_stat(ch, APPLY_INT);
  int wis_for_combat = combat_effective_stat(ch, APPLY_WIS);
  return (((int_for_combat + wis_for_combat) / 4) + 10);
}

int crit_base_heal(const struct char_data *ch)
{
  int int_for_combat = combat_effective_stat(ch, APPLY_INT);
  int cha_for_combat = combat_effective_stat(ch, APPLY_CHA);
  return (((int_for_combat + cha_for_combat) / 4) + 5);
}

int crit_total_melee(const struct char_data *ch)
{
  return clamp_percent(crit_base_melee(ch) + GET_MELEE_CRIT(ch));
}

int crit_total_spell(const struct char_data *ch)
{
  return clamp_percent(crit_base_spell(ch) + GET_SPELL_CRIT(ch));
}

int crit_total_heal(const struct char_data *ch)
{
  return clamp_percent(crit_base_heal(ch) + GET_HEAL_CRIT(ch));
}

int crit_mult_melee(const struct char_data *ch)
{
  int str_for_combat = combat_effective_stat(ch, APPLY_STR);
  int dex_for_combat = combat_effective_stat(ch, APPLY_DEX);
  int stat_mult_bonus = MAX(0, str_for_combat - 10) + MAX(0, (dex_for_combat - 10) / 2);
  return clamp_mult(150 + GET_MELEE_CRIT_MULT(ch) + stat_mult_bonus);
}

int crit_mult_spell(const struct char_data *ch)
{
  int int_for_combat = combat_effective_stat(ch, APPLY_INT);
  int wis_for_combat = combat_effective_stat(ch, APPLY_WIS);
  int stat_mult_bonus = MAX(0, int_for_combat - 10) + MAX(0, (wis_for_combat - 10) / 2);
  return clamp_mult(150 + GET_SPELL_CRIT_MULT(ch) + stat_mult_bonus);
}

int crit_mult_heal(const struct char_data *ch)
{
  int int_for_combat = combat_effective_stat(ch, APPLY_INT);
  int cha_for_combat = combat_effective_stat(ch, APPLY_CHA);
  int stat_mult_bonus = MAX(0, int_for_combat - 10) + MAX(0, (cha_for_combat - 10) / 2);
  return clamp_mult(150 + GET_HEAL_CRIT_MULT(ch) + stat_mult_bonus);
}

int roll_melee_crit(struct char_data *ch, int diceroll)
{
  if (diceroll == 20)
    return TRUE;

  if (diceroll < 18)
    return FALSE;

  return (rand_number(1, 100) <= crit_total_melee(ch));
}

int roll_spell_crit(struct char_data *ch)
{
  return (rand_number(1, 100) <= crit_total_spell(ch));
}

int roll_heal_crit(struct char_data *ch)
{
  return (rand_number(1, 100) <= crit_total_heal(ch));
}

/* Crit banner formatting
 * mult is percent: 200=2x, 300=3x, 400=4x
 * Uses { color codes in your style.
 */
void crit_show_banner(struct char_data *ch, struct char_data *victim, int mult)
{
  /* Inline crit presentation is handled in the main damage line.
   * Keep this function as a no-op to avoid duplicate or confusing output. */
  (void)ch;
  (void)victim;
  (void)mult;
}


/* These use the points fields added by your crit system: ch->points.melee_crit, etc.
 * They return 1 if a crit happens and set *mult to 200/300/400.
 *
 * Tier logic:
 * - base crit chance is the field value (0..100 assumed)
 * - if a crit happens, tier roll upgrades to 3x/4x at smaller odds
 */

int crit_check_melee(struct char_data *ch, int *mult)
{
  int chance;

  if (!ch || !mult)
    return 0;

  chance = crit_total_melee(ch);
  if (chance <= 0)
    return 0;

  if (rand_number(1, 100) > chance)
    return 0;

  *mult = crit_mult_melee(ch);
  return 1;
}


int crit_check_spell(struct char_data *ch, int *mult)
{
  int chance;

  if (!ch || !mult)
    return 0;

  chance = crit_total_spell(ch);
  if (chance <= 0)
    return 0;

  if (rand_number(1, 100) > chance)
    return 0;

  *mult = crit_mult_spell(ch);
  return 1;
}


int crit_check_heal(struct char_data *ch, int *mult)
{
  int chance;

  if (!ch || !mult)
    return 0;

  chance = crit_total_heal(ch);
  if (chance <= 0)
    return 0;

  if (rand_number(1, 100) > chance)
    return 0;

  *mult = crit_mult_heal(ch);
  return 1;
}
