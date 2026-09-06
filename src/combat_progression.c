/**************************************************************************
*  File: combat_progression.c                              Adventurer's Lair *
*  Usage: Shared stat + proficiency probability framework.                 *
**************************************************************************/

#include "conf.h"
#include "sysdep.h"

#include "structs.h"
#include "utils.h"
#include "class.h"
#include "combat_progression.h"

/*
 * Return a 0..1000 rating with diminishing returns as effective stats rise.
 * The breakpoints deliberately match the current mortal effective-stat range.
 */
int combat_progression_stat_rating(int stat_value)
{
  stat_value = MAX(0, MIN(30, stat_value));

  if (stat_value <= 10)
    return 0;

  if (stat_value <= 15)
    return (stat_value - 10) * 70;                    /* 15 => 350 */

  if (stat_value <= 20)
    return 350 + ((stat_value - 15) * 60);            /* 20 => 650 */

  if (stat_value <= 25)
    return 650 + ((stat_value - 20) * 45);            /* 25 => 875 */

  return 875 + ((stat_value - 25) * 25);              /* 30 => 1000 */
}

int combat_progression_weighted_stat_rating(int primary_value,
                                             int secondary_value,
                                             int tertiary_value)
{
  int primary = combat_progression_stat_rating(primary_value);
  int secondary = combat_progression_stat_rating(secondary_value);
  int tertiary = combat_progression_stat_rating(tertiary_value);

  return (primary * COMBAT_PROGRESSION_PRIMARY_WEIGHT +
          secondary * COMBAT_PROGRESSION_SECONDARY_WEIGHT +
          tertiary * COMBAT_PROGRESSION_TERTIARY_WEIGHT + 50) / 100;
}

int combat_progression_chance_basis_points_from_values(int proficiency,
                                                        int primary_value,
                                                        int secondary_value,
                                                        int tertiary_value,
                                                        int stage_percent)
{
  int weighted_rating;
  int training_basis_points;
  int stat_multiplier_per_thousand;
  int chance_basis_points;

  if (proficiency <= 0 || stage_percent <= 0)
    return 0;

  proficiency = MAX(1, MIN(100, proficiency));
  stage_percent = MAX(1, MIN(100, stage_percent));

  /*
   * Training is the foundation:
   *   5.00% + (0.75% * proficiency)
   * giving 5.75% at proficiency 1 and 80.00% at proficiency 100
   * before stats are applied.
   */
  training_basis_points = 500 + (75 * proficiency);

  /*
   * Stats multiply training instead of replacing it.
   * Weighted stat rating 0..1000 maps to a 0.750x..1.200x multiplier.
   */
  weighted_rating = combat_progression_weighted_stat_rating(primary_value,
                                                             secondary_value,
                                                             tertiary_value);
  stat_multiplier_per_thousand =
      750 + ((450 * weighted_rating + 500) / 1000);

  chance_basis_points =
      (training_basis_points * stat_multiplier_per_thousand + 500) / 1000;

  /* Later chain stages use 100 / 70 / 50 percent of the calculated chance. */
  chance_basis_points =
      (chance_basis_points * stage_percent + 50) / 100;

  /*
   * Known abilities always retain at least a 5% possibility in V1.
   * Cap at 95% so even mastery never becomes mathematically automatic.
   */
  return MAX(500, MIN(9500, chance_basis_points));
}

int combat_progression_chance_basis_points(struct char_data *ch,
                                           int proficiency,
                                           int primary_stat,
                                           int secondary_stat,
                                           int tertiary_stat,
                                           int stage_percent)
{
  if (!ch)
    return 0;

  return combat_progression_chance_basis_points_from_values(
      proficiency,
      get_class_stat_value(ch, primary_stat),
      get_class_stat_value(ch, secondary_stat),
      get_class_stat_value(ch, tertiary_stat),
      stage_percent);
}

int combat_progression_class_chance_basis_points(struct char_data *ch,
                                                 int proficiency,
                                                 int stage_percent)
{
  int class_num;

  if (!ch)
    return 0;

  class_num = GET_CLASS(ch);
  if (!is_valid_class(class_num))
    return 0;

  return combat_progression_chance_basis_points(
      ch,
      proficiency,
      get_class_primary_stat(class_num),
      get_class_secondary_stat(class_num),
      get_class_tertiary_stat(class_num),
      stage_percent);
}

static void combat_progression_physical_multiattack_stats(
    int class_num,
    int *primary_stat,
    int *secondary_stat,
    int *tertiary_stat)
{
  /* Safe physical fallback for rare tome/cross-class access. */
  *primary_stat = CLASS_STAT_STR;
  *secondary_stat = CLASS_STAT_DEX;
  *tertiary_stat = CLASS_STAT_CON;

  switch (class_num) {
    case CLASS_WARRIOR:
      *primary_stat = CLASS_STAT_STR;
      *secondary_stat = CLASS_STAT_DEX;
      *tertiary_stat = CLASS_STAT_CON;
      break;

    case CLASS_THIEF:
      *primary_stat = CLASS_STAT_DEX;
      *secondary_stat = CLASS_STAT_STR;
      *tertiary_stat = CLASS_STAT_INT;
      break;

    case CLASS_PALADIN:
      *primary_stat = CLASS_STAT_STR;
      *secondary_stat = CLASS_STAT_CON;
      *tertiary_stat = CLASS_STAT_WIS;
      break;

    case CLASS_BARD:
      *primary_stat = CLASS_STAT_DEX;
      *secondary_stat = CLASS_STAT_CHA;
      *tertiary_stat = CLASS_STAT_INT;
      break;

    case CLASS_MYSTIC:
      *primary_stat = CLASS_STAT_DEX;
      *secondary_stat = CLASS_STAT_WIS;
      *tertiary_stat = CLASS_STAT_CON;
      break;

    default:
      break;
  }
}

int combat_progression_physical_multiattack_chance_basis_points(
    struct char_data *ch,
    int proficiency,
    int stage_percent)
{
  int primary_stat;
  int secondary_stat;
  int tertiary_stat;

  if (!ch || IS_NPC(ch))
    return 0;

  combat_progression_physical_multiattack_stats(
      GET_CLASS(ch), &primary_stat, &secondary_stat, &tertiary_stat);

  return combat_progression_chance_basis_points(
      ch,
      proficiency,
      primary_stat,
      secondary_stat,
      tertiary_stat,
      stage_percent);
}

bool combat_progression_physical_multiattack_roll(struct char_data *ch,
                                                  int proficiency,
                                                  int stage_percent)
{
  int chance = combat_progression_physical_multiattack_chance_basis_points(
      ch, proficiency, stage_percent);

  return chance > 0 &&
         rand_number(1, COMBAT_PROGRESSION_CHANCE_SCALE) <= chance;
}

bool combat_progression_roll(struct char_data *ch,
                             int proficiency,
                             int primary_stat,
                             int secondary_stat,
                             int tertiary_stat,
                             int stage_percent)
{
  int chance = combat_progression_chance_basis_points(
      ch, proficiency, primary_stat, secondary_stat, tertiary_stat, stage_percent);

  return chance > 0 && rand_number(1, COMBAT_PROGRESSION_CHANCE_SCALE) <= chance;
}

bool combat_progression_class_roll(struct char_data *ch,
                                   int proficiency,
                                   int stage_percent)
{
  int chance = combat_progression_class_chance_basis_points(
      ch, proficiency, stage_percent);

  return chance > 0 && rand_number(1, COMBAT_PROGRESSION_CHANCE_SCALE) <= chance;
}