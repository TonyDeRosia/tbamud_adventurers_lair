#ifndef _MOB_BEHAVIOR_H_
#define _MOB_BEHAVIOR_H_

#include "structs.h"

void mob_behavior_reset_fight_state(struct char_data *mob);
void mob_behavior_advance_pulse(void);
void mob_behavior_on_combat_start(struct char_data *mob, struct char_data *opponent);
void mob_behavior_on_combat_end(struct char_data *mob);
void mob_behavior_eval_combat_round(struct char_data *mob);
void mob_behavior_handle_event(struct char_data *mob, int event_type, struct char_data *actor);
int mob_behavior_validate_skill(int skillnum);
void mob_behavior_begin_native_spell_cast(struct char_data *mob, int spellnum);
void mob_behavior_end_native_spell_cast(struct char_data *mob);
int mob_behavior_is_native_spell_cast_active(const struct char_data *mob, int spellnum);

const char *mob_behavior_ability_type_name(int type);
const char *mob_behavior_target_name(int target);
const char *mob_behavior_trigger_mode_name(int mode);
const char *mob_behavior_event_type_name(int type);
const char *mob_behavior_event_action_name(int type);

#endif
