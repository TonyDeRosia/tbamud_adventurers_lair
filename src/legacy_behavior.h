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

#ifndef MOB_BEHAVIOR_DOMAIN_COUNT
#define MOB_BEHAVIOR_DOMAIN_COUNT 13
#endif
#define MOB_BEHAVIOR_EDITABLE_DOMAIN_COUNT 9

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


/* Phase 2A conservative mobile-pulse arbitration.  The bit values intentionally
 * match legacy_behavior_domain for domains already audited in Phase 1. */
typedef unsigned MobBehaviorDomainMask;

enum mob_behavior_owner {
  MOB_BEHAVIOR_OWNER_COMPATIBILITY = 0,
  MOB_BEHAVIOR_OWNER_LEGACY,
  MOB_BEHAVIOR_OWNER_AI,
  MOB_BEHAVIOR_OWNER_DISABLED
};

enum mob_behavior_compatibility_mode {
  MOB_BEHAVIOR_COMPAT_LEGACY_PRESERVING = 0
};

struct mob_behavior_action_result {
  MobBehaviorDomainMask domains_acted;
  MobBehaviorDomainMask domains_blocked;
  int moved, changed_posture, ambient_message, reactive_message;
  int initiated_combat, assisted_ally, memory_retaliated;
  int object_interaction, attempted_flee, consumed_pulse;
  int returned_true, unknown_special, false_after_acting, claimed_old_pulse;
  char reason[96];
};

struct mob_behavior_pulse_context {
  struct char_data *mob;
  int compatibility_mode;
  int arbitration_enabled;
  enum mob_behavior_owner configured_owner[MOB_BEHAVIOR_DOMAIN_COUNT];
  enum mob_behavior_owner effective_owner[MOB_BEHAVIOR_DOMAIN_COUNT];
  MobBehaviorDomainMask legacy_special_domains, ai_configured_domains, legacy_tail_domains;
  MobBehaviorDomainMask unavailable_to_ai, unavailable_to_legacy_tail;
  MobBehaviorDomainMask unknown_special_locks;
  struct mob_behavior_action_result special_result, ai_result, legacy_tail_result;
  char lock_reason[MOB_BEHAVIOR_DOMAIN_COUNT][96];
  char compatibility_short_circuit[96];
};

#define MOB_BEHAVIOR_PHASE2A_DOMAINS (LBD_ROUTINE|LBD_MOVEMENT|LBD_POSTURE|LBD_AMBIENT_SPEECH|LBD_COMBAT_INIT|LBD_MEMORY|LBD_HELPER|LBD_SCAVENGING|LBD_FLEE)

const char *mob_behavior_owner_name(int owner);
const char *mob_behavior_domain_name(unsigned domain);
const char *mob_behavior_domain_token(unsigned domain);
unsigned mob_behavior_editable_domain(unsigned index);
unsigned mob_behavior_domain_from_token(const char *token);
int mob_behavior_owner_from_token(const char *token, enum mob_behavior_owner *owner);
int mob_behavior_domain_index(unsigned domain);
void mob_behavior_context_init(struct mob_behavior_pulse_context *ctx, struct char_data *mob, int enabled);
int mob_behavior_context_has_explicit_owner(const struct mob_behavior_pulse_context *ctx);
int mob_behavior_domain_available_to_ai(const struct mob_behavior_pulse_context *ctx, unsigned domain);
int mob_behavior_domain_available_to_legacy_tail(const struct mob_behavior_pulse_context *ctx, unsigned domain);
void mob_behavior_mark_action(struct mob_behavior_action_result *result, unsigned domain);
void mob_behavior_record_recent_pulse(struct char_data *mob, const struct mob_behavior_pulse_context *ctx);
void mob_behavior_recent_pulse_report(const struct char_data *mob, char *out, size_t size);
int mob_behavior_mayor_ai_ownership_supported(const struct char_data *mob, unsigned domain, char *why, size_t why_size);
#endif
