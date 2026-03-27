#ifndef _CRAFTING_H_
#define _CRAFTING_H_

#include <stddef.h>

struct char_data;
struct obj_data;

#define CRAFT_DISC_SCRIBING   1
#define CRAFT_DISC_ALCHEMY    2
#define CRAFT_DISC_ENCHANTING 3

#define CRAFT_MAT_TIER_LESSER   1
#define CRAFT_MAT_TIER_GREATER  2
#define CRAFT_MAT_TIER_SUPERIOR 3
/* Crafting item value layout:
 *  - ITEM_CRAFT_TOOL:     value[0] = discipline (CRAFT_DISC_*)
 *  - ITEM_CRAFT_MATERIAL: value[0] = discipline (CRAFT_DISC_*),
 *                         value[1] = tier (CRAFT_MAT_TIER_*) */

void do_scribe(struct char_data *ch, char *argument, int cmd, int subcmd);
void do_brew(struct char_data *ch, char *argument, int cmd, int subcmd);
void do_codex(struct char_data *ch, char *argument, int cmd, int subcmd);
void do_enchant(struct char_data *ch, char *argument, int cmd, int subcmd);
void do_disenchant(struct char_data *ch, char *argument, int cmd, int subcmd);
void crafting_sync_enchanting_disenchant(struct char_data *ch);
int crafting_is_valid_discipline(int disc);
int crafting_is_valid_material_tier(int tier);
const char *crafting_discipline_name(int disc);
const char *crafting_material_tier_name(int tier);

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
int crafting_get_enchant_overlay_count(const struct obj_data *obj);
int crafting_get_enchant_overlay_entry(const struct obj_data *obj, int index, int *recipe_index, signed char *location, signed char *modifier, int *order);
void crafting_try_migrate_legacy_enchants(struct obj_data *obj);

#endif
