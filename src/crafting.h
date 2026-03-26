#ifndef _CRAFTING_H_
#define _CRAFTING_H_

#include <stddef.h>

struct char_data;
struct obj_data;

#define CRAFT_DISC_SCRIBING   1
#define CRAFT_DISC_ALCHEMY    2
#define CRAFT_DISC_ENCHANTING 3

void do_scribe(struct char_data *ch, char *argument, int cmd, int subcmd);
void do_brew(struct char_data *ch, char *argument, int cmd, int subcmd);
void do_enchant(struct char_data *ch, char *argument, int cmd, int subcmd);

int crafting_try_recite_tome(struct char_data *ch, char *argument);
int crafting_handle_tome_put(struct char_data *ch, struct obj_data *obj, struct obj_data *cont);
int crafting_can_teach_ability(struct char_data *trainer, int ability_id);
int crafting_scrolls_identical(const struct obj_data *a, const struct obj_data *b);

int crafting_get_potion_stack(const struct obj_data *obj);
void crafting_set_potion_stack(struct obj_data *obj, int count);
int crafting_try_merge_potion_stack(struct obj_data *dest, struct obj_data *src);

int crafting_get_enchant_count(const struct obj_data *obj);
int crafting_get_enchant_recipe_count(const struct obj_data *obj, const char *recipe_name);
int crafting_is_item_enchanted(const struct obj_data *obj);
void crafting_build_enchant_tag(const struct obj_data *obj, char *out, size_t outsz);
void crafting_build_enchant_recipe_summary(const struct obj_data *obj, char *out, size_t outsz);

#endif
