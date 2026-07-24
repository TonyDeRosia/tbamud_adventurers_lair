#ifndef _LEGACY_BEHAVIOR_H_
#define _LEGACY_BEHAVIOR_H_

/* Read-only description of behavior still owned by the pre-AI systems.
 * Phase 2 may consume these masks; this module never dispatches behavior. */
enum legacy_behavior_domain {
  LBD_SERVICE = 1 << 0, LBD_ROUTINE = 1 << 1, LBD_MOVEMENT = 1 << 2,
  LBD_POSTURE = 1 << 3, LBD_AMBIENT_SPEECH = 1 << 4,
  LBD_COMBAT_INIT = 1 << 5, LBD_COMBAT_TACTICS = 1 << 6,
  LBD_SCAVENGING = 1 << 7, LBD_DOORS = 1 << 8, LBD_SCRIPT = 1 << 9,
  LBD_MEMORY = 1 << 10, LBD_HELPER = 1 << 11, LBD_FLEE = 1 << 12
};
enum legacy_assignment_origin {
  LAO_NONE, LAO_HARDCODED_VNUM, LAO_REGISTERED, LAO_DIRECT_CUSTOM,
  LAO_SHOP_DATA, LAO_QUEST_DATA, LAO_GUILD_FLAG, LAO_DG_PROTOTYPE, LAO_UNKNOWN
};
struct legacy_special_metadata {
  const char *name; SPECIAL(*func); unsigned domains;
  int service, periodic, intercepts_commands, combat_pulse, coexistence_known;
};
const struct legacy_special_metadata *legacy_special_metadata(SPECIAL(*func));
const char *legacy_assignment_origin_name(int origin);
unsigned legacy_ai_domains(const struct char_data *mob);
int legacy_behavior_warning_count(const struct char_data *mob);
void legacy_behavior_summary(const struct char_data *mob, char *out, size_t size, int detailed);
#endif
