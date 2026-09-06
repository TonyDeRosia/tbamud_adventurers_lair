/**
* @file fight.h
* Fighting and violence functions and variables.
* 
* Part of the core tbaMUD source code distribution, which is a derivative
* of, and continuation of, CircleMUD.
*                                                                        
* All rights reserved.  See license for complete information.                                                                
* Copyright (C) 1993, 94 by the Trustees of the Johns Hopkins University 
* CircleMUD is based on DikuMUD, Copyright (C) 1990, 1991.
*
*/
#ifndef _FIGHT_H_
#define _FIGHT_H_

/* Structures and defines */
/* Attacktypes with grammar */
struct attack_hit_type {
   const char *singular;
   const char *plural;
};

/* Functions available in fight.c */
void appear(struct char_data *ch);
void check_killer(struct char_data *ch, struct char_data *vict);
int legacy_ac_to_armor(int legacy_ac);
int compute_armor(struct char_data *ch);
int compute_armor_class(struct char_data *ch);
int compute_evasion(struct char_data *ch);
int compute_offensive_hit_value(struct char_data *ch, struct char_data *victim);
int compute_hit_chance_from_values(int offensive_hit, int target_evasion);
/* SCORE benchmark: visible, awake, equal-level defender, DEX 20, no applies. */
#define STANDARD_DEFENDER_EVASION 30
int compute_reference_hit_chance(struct char_data *ch);
int apply_armor_mitigation(int damage, int armor);
int armor_mitigation_basis_points(int armor);
void get_player_unarmed_profile(int level, int *dice_num, int *dice_size, int *level_bonus);
int unarmed_proficiency_bonus(int unarmed_component, int skill);
int unarmed_expected_average_x100(int level, int skill);
int mob_kill_base_xp_for_levels(int attacker_level, int victim_level);
int is_owned_follower_target(struct char_data *attacker, struct char_data *victim);
int damage(struct char_data *ch, struct char_data *victim, int dam, int attacktype);
void death_cry(struct char_data *ch);
void die(struct char_data * ch, struct char_data * killer);
void hit(struct char_data *ch, struct char_data *victim, int type);
void dual_skill_attack(struct char_data *ch, struct char_data *victim, int type);
void perform_violence(void);
void set_next_damage_type(int damage_type);
void raw_kill(struct char_data * ch, struct char_data * killer);
void  set_fighting(struct char_data *ch, struct char_data *victim);
int skill_message(int dam, struct char_data *ch, struct char_data *vict,
          int attacktype);
void  stop_fighting(struct char_data *ch);
bool corpse_has_remaining_part(const struct obj_data *corpse, int part);
struct obj_data *sever_corpse_part(struct obj_data *corpse, int part);
int body_profile_parts(int profile);
const char *body_profile_name(int profile);
int resolve_body_profile(const struct char_data *ch);


/* Global variables */
extern struct attack_hit_type attack_hit_text[];
extern struct char_data *combat_list;

#endif /* _FIGHT_H_*/
