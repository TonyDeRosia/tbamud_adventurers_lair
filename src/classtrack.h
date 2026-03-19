#ifndef _CLASSTRACK_H_
#define _CLASSTRACK_H_

#include <stddef.h>

struct char_data;

enum classtrack_archetype {
  ARCHETYPE_COMBAT = 0,
  ARCHETYPE_ROGUE,
  ARCHETYPE_ARCANE,
  ARCHETYPE_DIVINE,
  ARCHETYPE_NATURE,
  ARCHETYPE_DARK,
  NUM_ARCHETYPES
};

void classtrack_init_new_player(struct char_data *ch);
void classtrack_record_ability_use(struct char_data *ch, int ability, int was_spell);
void classtrack_check_level_checkpoint(struct char_data *ch);
const char *classtrack_display_class_name(struct char_data *ch);
const char *classtrack_display_class_abbrev(struct char_data *ch);
int classtrack_get_ability_archetype(int ability_id);
int classtrack_can_study_ability(struct char_data *ch, int ability_id);
int classtrack_get_study_min_level(int ability_id);
int classtrack_is_study_catalog_ability(int ability_id, int show_spells);
void classtrack_ensure_study_skill(struct char_data *ch);

/* Future study-system integration hook:
 * Best single hook point: skill_spell_ok() in src/spell_parser.c immediately
 * before a learned spell/skill is accepted and practiced.
 * Returns 1 when learning should be allowed, returns 0 when path commitment
 * blocks new off-path study. */
int classtrack_can_study_archetype(struct char_data *ch, int target_archetype,
                                   char *reason, size_t reason_len);

#endif
