/**
 * @file class.h
 * Header file for class specific functions and variables.
 *
 * Part of the core tbaMUD source code distribution, which is a derivative
 * of, and continuation of, CircleMUD.
 *
 * All rights reserved. See license for complete information.
 * Copyright (C) 1993, 94 by the Trustees of the Johns Hopkins University
 * CircleMUD is based on DikuMUD, Copyright (C) 1990, 1991.
 */

#ifndef _CLASS_H_
#define _CLASS_H_

#include <stdbool.h>

#include "structs.h"   /* byte, bitvector_t, NUM_CLASSES, struct types */

/* Forward declarations (structs.h already has these, but keeping them is fine) */
struct char_data;
struct obj_data;

struct pc_class_definition {
  const char *name;
  const char *abbrev;
  const char *archetype_abbrev;
  char select_key;
  bool selectable;
  int prac_learned_level;
  int prac_max_per_prac;
  int prac_min_per_prac;
  int prac_type;
};

/* Functions available through class.c */
int backstab_mult(int level);
void do_start(struct char_data *ch);
void ensure_class_abilities(struct char_data *ch);
bitvector_t find_class_bitvector(const char *arg);
int invalid_class(struct char_data *ch, struct obj_data *obj);
int is_valid_class(int class_num);
int num_pc_classes(void);
int level_exp(int chclass, int level);
int parse_class(char arg);
void roll_real_abils(struct char_data *ch);
byte saving_throws(int class_num, int type, int level);
int thaco(int class_num, int level);
const char *title_female(int chclass, int level);
const char *title_male(int chclass, int level);
const char *class_name(int class_id);
const char *class_abbrev(int class_id);
const char *get_archetype_abbrev(struct char_data *ch);
const char *get_class_display_abbrev(struct char_data *ch);
const char *class_menu(void);

int get_class_prac_learned_level(int class_num);
int get_class_prac_max_per_prac(int class_num);
int get_class_prac_min_per_prac(int class_num);
int get_class_prac_type(int class_num);

/* Global variables */
extern const struct pc_class_definition pc_classes[];
extern const char *class_abbrevs[];
extern const char *pc_class_types[];
extern struct guild_info_type guild_info[];

#endif /* _CLASS_H_ */
