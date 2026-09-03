#ifndef _TOME_H_
#define _TOME_H_

#include <time.h>

#define TOME_ABILITY_SLOTS 4
#define TOME_DEFAULT_COOLDOWN_SECONDS (7 * 24 * 60 * 60)
#define TOME_DEFAULT_OFFCLASS_AFFINITY 50

int tome_valid_ability(int ability);
int tome_validate(const struct obj_data *obj, char *why, size_t whylen);
int has_tome_ability(const struct char_data *ch, int ability);
int character_has_ability_access(const struct char_data *ch, int ability);
int get_ability_class_affinity(const struct char_data *ch, int ability);
void tome_format_remaining(time_t until, char *buf, size_t buflen);
ACMD(do_tome);
int tome_study_object(struct char_data *ch, struct obj_data *obj);

#endif
