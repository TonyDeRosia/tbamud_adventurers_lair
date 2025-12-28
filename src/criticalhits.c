#include "conf.h"
#include "sysdep.h"

#include "structs.h"
#include "utils.h"
#include "criticalhits.h"

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
  return (GET_DEX(ch) / 2);
}

int crit_base_spell(const struct char_data *ch)
{
  return (((GET_INT(ch) + GET_WIS(ch)) / 4) + 10);
}

int crit_base_heal(const struct char_data *ch)
{
  return (((GET_INT(ch) + GET_CHA(ch)) / 4) + 5);
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
  return clamp_mult(200 + GET_MELEE_CRIT_MULT(ch));
}

int crit_mult_spell(const struct char_data *ch)
{
  return clamp_mult(200 + GET_SPELL_CRIT_MULT(ch));
}

int crit_mult_heal(const struct char_data *ch)
{
  return clamp_mult(200 + GET_HEAL_CRIT_MULT(ch));
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
