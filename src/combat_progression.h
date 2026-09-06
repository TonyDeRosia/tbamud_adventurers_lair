#ifndef _COMBAT_PROGRESSION_H_
#define _COMBAT_PROGRESSION_H_

#include <stdbool.h>

struct char_data;

/* Public scale: 10000 basis points = 100.00%. */
#define COMBAT_PROGRESSION_CHANCE_SCALE 10000

/* V1 class-profile weighting. */
#define COMBAT_PROGRESSION_PRIMARY_WEIGHT   60
#define COMBAT_PROGRESSION_SECONDARY_WEIGHT 25
#define COMBAT_PROGRESSION_TERTIARY_WEIGHT  15

/* Common chain-stage modifiers. */
#define COMBAT_PROGRESSION_STAGE_FULL    100
#define COMBAT_PROGRESSION_STAGE_SECOND   70
#define COMBAT_PROGRESSION_STAGE_THIRD    50

/*
 * Convert an effective stat into a 0..1000 diminishing-return rating.
 *
 * 10 -> 0
 * 15 -> 350
 * 20 -> 650
 * 25 -> 875
 * 30 -> 1000
 *
 * Stats below 10 do not penalize below zero in V1. Effective stats above
 * 30 are treated as 30, matching the current EFFECTIVE_STAT_CAP.
 */
int combat_progression_stat_rating(int stat_value);

/* Combine three already-resolved stat values using the V1 60/25/15 profile. */
int combat_progression_weighted_stat_rating(int primary_value,
                                             int secondary_value,
                                             int tertiary_value);

/*
 * Pure deterministic probability helper.
 *
 * proficiency: 0..100
 * stat values: effective stat values, normally 0..30
 * stage_percent: 100 for a full proc check, 70 / 50 for later chain stages
 *
 * Returns basis points in the range 0..9500.
 * Unknown abilities (proficiency <= 0) return 0.
 */
int combat_progression_chance_basis_points_from_values(int proficiency,
                                                        int primary_value,
                                                        int secondary_value,
                                                        int tertiary_value,
                                                        int stage_percent);

/*
 * Resolve an explicit stat profile from a character. This is the primary
 * ability-facing helper: individual skills/spells can specify the stat profile
 * that fits their class/archetype identity.
 */
int combat_progression_chance_basis_points(struct char_data *ch,
                                           int proficiency,
                                           int primary_stat,
                                           int secondary_stat,
                                           int tertiary_stat,
                                           int stage_percent);

/* Resolve the character's default class primary/secondary/tertiary profile. */
int combat_progression_class_chance_basis_points(struct char_data *ch,
                                                 int proficiency,
                                                 int stage_percent);

/*
 * Physical multiattack profile used by Double / Triple / Fourth Attack.
 * The class mapping preserves class identity rather than treating DEX as a
 * universal attack stat. Cross-class/tome access falls back to STR/DEX/CON.
 */
int combat_progression_physical_multiattack_chance_basis_points(
    struct char_data *ch,
    int proficiency,
    int stage_percent);

bool combat_progression_physical_multiattack_roll(struct char_data *ch,
                                                  int proficiency,
                                                  int stage_percent);

/* Random roll counterparts. */
bool combat_progression_roll(struct char_data *ch,
                             int proficiency,
                             int primary_stat,
                             int secondary_stat,
                             int tertiary_stat,
                             int stage_percent);

bool combat_progression_class_roll(struct char_data *ch,
                                   int proficiency,
                                   int stage_percent);

#endif /* _COMBAT_PROGRESSION_H_ */