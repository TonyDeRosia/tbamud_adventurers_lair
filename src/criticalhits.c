#include "conf.h"
#include "sysdep.h"

#include "structs.h"
#include "utils.h"
#include "criticalhits.h"
#include "comm.h"

/*
 * Crit banner formatting
 * mult is percent: 200=2x, 300=3x, 400=4x
 * Uses { color codes per your banner style.
 */
const char *crit_banner_for_mult(int mult)
{
  if (mult >= 400)
    return "{Y#{W#{Y#{W#{Y C{W E{Y N{W S{Y O{W R{Y #{W#{Y#{W#{x";
  if (mult >= 300)
    return "{R#{Y#{R#{Y#{R C{Y E{R N{Y S{R O{Y R{R #{Y#{R#{Y#{x";
  return "{D#{W#{D#{W#{D C{W E{D N{W S{D O{W R{D #{W#{D#{W#{x";
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

/* Crit banner formatting
 * mult is percent: 200=2x, 300=3x, 400=4x
 * Uses { color codes in your style.
 */
void crit_show_banner(struct char_data *ch, struct char_data *victim, int mult)
{
  const char *b;

  if (!ch)
    return;

  b = crit_banner_for_mult(mult);
  send_to_char(ch, "%s\r\n", b);

  if (victim && victim != ch)
    send_to_char(victim, "%s\r\n", b);
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

