#ifndef _AI_ACTOR_H_
#define _AI_ACTOR_H_

#include <stdint.h>

#define AI_MEM_MAX 12
#define AI_EVENT_RING_MAX 12
#define AI_INTENT_KEYWORDS_MAX 192
#define AI_ACTOR_PERSONALITIES 12
#define AI_DIALOGUE_CATEGORIES 20
#define AI_DIALOGUE_MAX_LINES 8
#define AI_DIALOGUE_LINE_MAX 240
#define AI_SOCIAL_COOLDOWN_MIN 1
#define AI_SOCIAL_COOLDOWN_MAX 300
#define AI_THREAT_STEP_MAX 10
#define AI_TARGET_WEIGHTS 8
#define AI_HELP_EVENT_MAX 16
#define AI_HELP_EVENT_RESPONDERS 10
#define AI_SCHEDULE_MAX 16
#define AI_PATROL_MAX 8
#define AI_PATROL_WAYPOINT_MAX 16
#define AI_DAY_MASK_ALL 0x7f

enum mob_ai_profile_mode { MOB_AI_INFERRED = 0, MOB_AI_CUSTOM, MOB_AI_INFERRED_OVERRIDES };
enum mob_ai_personality { AI_TRAIT_AGGRESSION, AI_TRAIT_BRAVERY, AI_TRAIT_SOCIABILITY, AI_TRAIT_CURIOSITY, AI_TRAIT_DISCIPLINE, AI_TRAIT_HONESTY, AI_TRAIT_GREED, AI_TRAIT_COMPASSION, AI_TRAIT_LOYALTY, AI_TRAIT_PATIENCE, AI_TRAIT_SUSPICION, AI_TRAIT_PRIDE };
enum ai_social_style { AI_SOCIAL_SILENT, AI_SOCIAL_RESERVED, AI_SOCIAL_POLITE, AI_SOCIAL_FRIENDLY, AI_SOCIAL_TALKATIVE, AI_SOCIAL_BOASTFUL, AI_SOCIAL_RUDE, AI_SOCIAL_HOSTILE, AI_SOCIAL_EXTORTING, AI_SOCIAL_PREACHER, AI_SOCIAL_GOSSIP };
enum ai_dialogue_category { AI_DIALOGUE_GREETING, AI_DIALOGUE_FRIENDLY, AI_DIALOGUE_SUSPICIOUS, AI_DIALOGUE_HOSTILE, AI_DIALOGUE_AMBIENT_SPEECH, AI_DIALOGUE_AMBIENT_EMOTE, AI_DIALOGUE_FAREWELL, AI_DIALOGUE_WARNING, AI_DIALOGUE_CHALLENGE, AI_DIALOGUE_THREAT, AI_DIALOGUE_CALL_HELP, AI_DIALOGUE_FEAR, AI_DIALOGUE_SCHEDULE_DEPARTURE, AI_DIALOGUE_SCHEDULE_ARRIVAL, AI_DIALOGUE_WORK, AI_DIALOGUE_GUARD, AI_DIALOGUE_PATROL, AI_DIALOGUE_SLEEP, AI_DIALOGUE_WAKE, AI_DIALOGUE_SCHEDULE_FAILURE };
enum ai_threat_response { AI_THREAT_OBSERVE, AI_THREAT_WARN, AI_THREAT_CHALLENGE, AI_THREAT_CALL_HELP, AI_THREAT_ASSIST, AI_THREAT_FOLLOW, AI_THREAT_ARREST, AI_THREAT_ATTACK, AI_THREAT_FLEE, AI_THREAT_SURRENDER, AI_THREAT_IGNORE, AI_THREAT_RESPONSE_MAX };
enum ai_combat_style { AI_COMBAT_PASSIVE, AI_COMBAT_DEFENSIVE, AI_COMBAT_BALANCED, AI_COMBAT_AGGRESSIVE, AI_COMBAT_PROTECTOR, AI_COMBAT_COWARDLY, AI_COMBAT_FANATICAL, AI_COMBAT_OPPORTUNIST, AI_COMBAT_CONTROLLER, AI_COMBAT_BOSS, AI_COMBAT_STYLE_MAX };
enum ai_target_weight { AI_TARGET_CURRENT_ATTACKER, AI_TARGET_TRUSTED_ATTACKER, AI_TARGET_GROUP_ATTACKER, AI_TARGET_KNOWN_HOSTILE, AI_TARGET_LOW_HEALTH, AI_TARGET_PLAYER, AI_TARGET_NPC, AI_TARGET_PREVIOUS };
struct ai_threat_step { int type, minimum_severity, cooldown, max_repetitions, advance_on_failure; };
enum mob_ai_config_movement { AI_MOVE_STATIONARY, AI_MOVE_RANDOM, AI_MOVE_PATROL, AI_MOVE_SCHEDULED, AI_MOVE_GUARD_ROOM, AI_MOVE_RETURN_HOME };
enum ai_schedule_activity { AI_SCHEDULE_REMAIN, AI_SCHEDULE_TRAVEL, AI_SCHEDULE_PATROL, AI_SCHEDULE_IDLE_SOCIAL, AI_SCHEDULE_GUARD, AI_SCHEDULE_WORK, AI_SCHEDULE_SLEEP, AI_SCHEDULE_REST, AI_SCHEDULE_RETURN_HOME, AI_SCHEDULE_ACTIVITY_MAX };
enum ai_schedule_destination { AI_DEST_CURRENT_ROOM, AI_DEST_ROOM_VNUM, AI_DEST_HOME, AI_DEST_WORK, AI_DEST_SLEEP, AI_DEST_GUARD, AI_DEST_FALLBACK, AI_DEST_PATROL, AI_DEST_SPAWN, AI_DESTINATION_MAX };
enum ai_schedule_action { AI_SCHEDULE_ACTION_NONE, AI_SCHEDULE_ACTION_SPEAK, AI_SCHEDULE_ACTION_EMOTE, AI_SCHEDULE_ACTION_SIT, AI_SCHEDULE_ACTION_REST, AI_SCHEDULE_ACTION_SLEEP, AI_SCHEDULE_ACTION_STAND, AI_SCHEDULE_ACTION_WAKE, AI_SCHEDULE_ACTION_BEGIN_PATROL, AI_SCHEDULE_ACTION_GUARD, AI_SCHEDULE_ACTION_MAX };
enum ai_schedule_interrupt { AI_INTERRUPT_IGNORE, AI_INTERRUPT_PAUSE_RESUME, AI_INTERRUPT_RESTART, AI_INTERRUPT_SKIP, AI_INTERRUPT_ABORT, AI_INTERRUPT_FALLBACK, AI_INTERRUPT_MAX };
enum ai_schedule_failure { AI_FAILURE_WAIT_RETRY, AI_FAILURE_SKIP, AI_FAILURE_RESTART, AI_FAILURE_FALLBACK, AI_FAILURE_ABORT, AI_FAILURE_DISABLE_UNTIL_CHANGE, AI_FAILURE_MAX };
enum ai_schedule_state { AI_SCHED_INACTIVE, AI_SCHED_SELECTED, AI_SCHED_PREPARING_DEPARTURE, AI_SCHED_TRAVELING, AI_SCHED_ARRIVED, AI_SCHED_ACTIVE, AI_SCHED_WAITING_WAYPOINT, AI_SCHED_INTERRUPTED, AI_SCHED_RESUMING, AI_SCHED_FAILED, AI_SCHED_COMPLETED, AI_SCHED_ABORTED, AI_SCHED_RUNTIME_DISABLED };
enum ai_schedule_interrupt_reason { AI_SCHEDULE_INTERRUPT_NONE, AI_SCHEDULE_INTERRUPT_MINOR_EVENT, AI_SCHEDULE_INTERRUPT_MAJOR_THREAT, AI_SCHEDULE_INTERRUPT_COMBAT, AI_SCHEDULE_INTERRUPT_COMBAT_FLEE, AI_SCHEDULE_INTERRUPT_FORCED_MOVEMENT, AI_SCHEDULE_INTERRUPT_SCRIPTED_MOVEMENT, AI_SCHEDULE_INTERRUPT_FOLLOWER_MOVEMENT, AI_SCHEDULE_INTERRUPT_ADMIN_TRANSFER, AI_SCHEDULE_INTERRUPT_TELEPORT_SUMMON, AI_SCHEDULE_INTERRUPT_CHARM_CONTROL, AI_SCHEDULE_INTERRUPT_INCAPACITATED, AI_SCHEDULE_INTERRUPT_INVALID_DESTINATION, AI_SCHEDULE_INTERRUPT_INVALID_ROUTE, AI_SCHEDULE_INTERRUPT_MOVEMENT_FAILURE, AI_SCHEDULE_INTERRUPT_TRAVEL_TIMEOUT, AI_SCHEDULE_INTERRUPT_SCHEDULE_REPLACED, AI_SCHEDULE_INTERRUPT_UNKNOWN_DISPLACEMENT };
enum ai_schedule_resume_result { AI_SCHEDULE_RESUME_VALID, AI_SCHEDULE_RESUME_ENTRY_EXPIRED, AI_SCHEDULE_RESUME_ENTRY_REPLACED, AI_SCHEDULE_RESUME_DESTINATION_INVALID, AI_SCHEDULE_RESUME_ROUTE_INVALID, AI_SCHEDULE_RESUME_POSITION_INVALID, AI_SCHEDULE_RESUME_CONTROL_CONFLICT, AI_SCHEDULE_RESUME_MOVEMENT_RESTRICTED };
enum ai_schedule_result { AI_SCHEDULE_INACTIVE, AI_SCHEDULE_ALLOW_WANDER, AI_SCHEDULE_BLOCK_WANDER, AI_SCHEDULE_MAJOR_ACTION };
enum ai_patrol_loop { AI_PATROL_LOOP, AI_PATROL_PINGPONG, AI_PATROL_ONCE, AI_PATROL_RANDOM, AI_PATROL_LOOP_MAX };
struct ai_schedule_entry { int id, enabled, start_hour, end_hour, day_mask, priority, activity, destination, destination_value, arrival_action, departure_action, interruption_policy, failure_policy, max_travel_time, max_attempts, wait_duration, route_id; };
struct ai_patrol_waypoint { int room_vnum, wait_duration, arrival_action; };
struct ai_patrol_route { int id, enabled, loop_mode, failure_policy, waypoint_count; char label[32]; struct ai_patrol_waypoint waypoints[AI_PATROL_WAYPOINT_MAX]; };
struct mob_ai_config {
  int mode, role, movement, social;
  unsigned long override_mask;
  int personality[AI_ACTOR_PERSONALITIES];
  int home_room_vnum, work_room_vnum, guard_room_vnum, sleep_room_vnum, fallback_room_vnum, schedule_enabled, resume_after_interrupt, default_failure_policy, schedule_count, patrol_count, next_schedule_id, next_patrol_id;
  struct ai_schedule_entry schedules[AI_SCHEDULE_MAX];
  struct ai_patrol_route patrols[AI_PATROL_MAX];
  int roam_radius, pursuit_distance, movement_delay;
  int greeting_enabled, ambient_speech_enabled, ambient_emotes_enabled, whisper_enabled;
  int respond_strangers, respond_trusted, respond_feared, respond_hostile;
  int speech_cooldown, room_speech_cooldown, emote_cooldown;
  int dialogue_count[AI_DIALOGUE_CATEGORIES];
  char *dialogue[AI_DIALOGUE_CATEGORIES][AI_DIALOGUE_MAX_LINES];
  int flee_hp_percent, surrender_hp_percent, assist_enabled, call_help_enabled, hunt_enabled, return_home, stay_zone;
  int notice_entry, notice_departure, notice_speech, notice_whispers, notice_emotes, notice_combat, notice_self_attack, notice_ally_attack, notice_corpses, notice_drops, notice_gifts, notice_crimes;
  int hearing_sensitivity, observation_sensitivity, suspicion_threshold, recognition_confidence;
  int memory_enabled, memory_max_actors, memory_ordinary_duration, memory_important_duration, trust_gain, trust_loss, fear_gain, fear_decay, hostility_gain, hostility_decay, familiarity_gain, familiarity_decay, forgiveness;
  int remember_attacks, remember_assistance, remember_crimes, remember_gifts, remember_insults, remember_conversations, remember_threats, remember_last_room, remember_deaths;
  int threat_enabled[AI_THREAT_RESPONSE_MAX], threat_cooldown, calm_reset_time, repeated_event_window, threat_step_count;
  struct ai_threat_step threat_steps[10];
  int combat_style, combat_enabled, may_initiate, may_assist, may_call_help, may_flee;
  int protect_trusted, protect_group, protect_same_role, protect_same_prototype;
  int avoid_incapacitated, retaliate_self, retaliate_ally, retaliate_hostile, switch_targets;
  int assist_severity, target_switch_threshold, max_allies, max_responders, combat_cooldown;
  int target_weight[AI_TARGET_WEIGHTS];
};

/* Builder-facing, read-only explanation of the currently audited runtime.
 * The report is deliberately not a validator that repairs configuration. */
int ai_actor_compatibility_warning_count(const struct char_data *mob);
void ai_actor_compatibility_report(const struct char_data *mob, char *out, size_t size,
                                   int detailed);
#define AI_OVERRIDE_ROLE (1UL << 0)
#define AI_OVERRIDE_MOVEMENT (1UL << 1)
#define AI_OVERRIDE_SOCIAL (1UL << 2)
#define AI_OVERRIDE_TRAITS (1UL << 3)

#define AI_PROFILE_INCONSISTENT (1 << 0)

enum ai_disposition_flags {
  AI_DISP_ATTACKED_ME = (1 << 0),
  AI_DISP_HELPED_ME = (1 << 1),
  AI_DISP_ANNOYED_ME = (1 << 2),
  AI_DISP_DISRESPECT = (1 << 3),
  AI_DISP_THREATENED = (1 << 4),
  AI_DISP_FRIENDLY = (1 << 5)
};

enum ai_actor_intent {
  AI_INTENT_NONE = 0,
  AI_INTENT_GREET,
  AI_INTENT_ASK_SERVICE,
  AI_INTENT_DIRECTIONS,
  AI_INTENT_THREAT,
  AI_INTENT_INSULT,
  AI_INTENT_PRAISE,
  AI_INTENT_CONFUSION,
  AI_INTENT_EMOTE_DANCE,
  AI_INTENT_EMOTE_SPIT,
  AI_INTENT_EMOTE_HUG,
  AI_INTENT_EMOTE_WAVE,
  AI_INTENT_BUY_WEAPON,
  AI_INTENT_BUY_ARMOR,
  AI_INTENT_BUY_FOOD,
  AI_INTENT_HEAL,
  AI_INTENT_BANK,
  AI_INTENT_INN,
  AI_INTENT_TRAIN,
  AI_INTENT_RUMOR,
  AI_INTENT_QUEST,
  AI_INTENT_SMALLTALK,
  AI_INTENT_GIBBERISH
};

enum ai_topic_target {
  TARGET_NONE = 0,
  TARGET_INN,
  TARGET_BANK,
  TARGET_TEMPLE,
  TARGET_MARKET,
  TARGET_ARMORY,
  TARGET_BAKERY,
  TARGET_TRAINER,
  TARGET_HEAL
};

enum ai_actor_topic_flags {
  AI_TOPIC_MIDGAARD = (1 << 0),
  AI_TOPIC_TEMPLE = (1 << 1),
  AI_TOPIC_MARKET = (1 << 2),
  AI_TOPIC_INN = (1 << 3),
  AI_TOPIC_BANK = (1 << 4),
  AI_TOPIC_ALLEY = (1 << 5),
  AI_TOPIC_WILDERNESS = (1 << 6),
  AI_TOPIC_DUNGEON = (1 << 7),
  AI_TOPIC_SEWER = (1 << 8),
  AI_TOPIC_CASTLE = (1 << 9)
};

/* Optional compile-time debug logging. */
#ifndef AI_ACTOR_DEBUG
#define AI_ACTOR_DEBUG 0
#endif

#ifndef AI_ACTOR_DEBUG_SPEECH
#define AI_ACTOR_DEBUG_SPEECH 0
#endif

struct ai_actor_brain;

enum ai_event_type {
  AI_EVENT_PLAYER_SAY = 0,
  AI_EVENT_PLAYER_EMOTE,
  AI_EVENT_PLAYER_ENTER,
  AI_EVENT_PLAYER_LEAVE,
  AI_EVENT_COMBAT_START,
  AI_EVENT_COMBAT_END
};

enum ai_actor_role {
  ROLE_UNKNOWN = 0,
  ROLE_GUARD,
  ROLE_MERCHANT,
  ROLE_BANDIT,
  ROLE_BEAST,
  ROLE_UNDEAD,
  ROLE_SPIRIT,
  ROLE_CULTIST,
  ROLE_CIVILIAN,
  ROLE_BOSS
};

enum ai_actor_persona {
  AI_PERSONA_NEUTRAL = 0,
  AI_PERSONA_GUARD,
  AI_PERSONA_CONSTABLE,
  AI_PERSONA_MERCHANT,
  AI_PERSONA_INNKEEPER,
  AI_PERSONA_BANDIT,
  AI_PERSONA_INSTRUCTOR,
  AI_PERSONA_CULTIST,
  AI_PERSONA_BEAST
};

enum ai_actor_movement {
  MOVE_SENTINEL = 0,
  MOVE_PATROL,
  MOVE_WANDER_RADIUS,
  MOVE_ROAM_INTEREST
};

enum ai_actor_aggression {
  AGG_PEACEFUL = 0,
  AGG_RETALIATE,
  AGG_TERRITORIAL,
  AGG_OPPORTUNISTIC,
  AGG_CRIME_HUNTER,
  AGG_AMBUSH
};

enum ai_actor_social {
  SOC_SILENT = 0,
  SOC_WARNING,
  SOC_TALKATIVE,
  SOC_EXTORT
};

enum ai_actor_morale {
  MORALE_BRAVE = 0,
  MORALE_NORMAL,
  MORALE_COWARD
};

#define MEM_WANTED      (1 << 0)
#define MEM_ATTACKED_ME (1 << 1)
#define MEM_HELPED_ME   (1 << 2)
#define MEM_STOLE       (1 << 3)
#define MEM_FLED_FROM   (1 << 4)
#define MEM_ASSAULT     (1 << 5)
#define MEM_MURDER      (1 << 6)
#define MEM_TRESPASS    (1 << 7)

enum {
  AI_OPP_NONE = 0,
  AI_OPP_PREF_WOUNDED = (1 << 0),
  AI_OPP_PREF_ALONE = (1 << 1)
};

struct ai_actor_profile {
  int role;
  int mode;
  int movement;
  int aggression;
  int social;
  int personality[AI_ACTOR_PERSONALITIES];
  int greeting_enabled, ambient_speech_enabled, ambient_emotes_enabled, whisper_enabled;
  int respond_strangers, respond_trusted, respond_feared, respond_hostile;
  int emote_cooldown_secs;
  int dialogue_count[AI_DIALOGUE_CATEGORIES];
  char *dialogue[AI_DIALOGUE_CATEGORIES][AI_DIALOGUE_MAX_LINES];
  int morale;
  int home_room_vnum;
  int roam_radius;
  int flee_hp_percent;
  int surrender_hp_percent;
  int talk_cooldown_secs;
  int room_talk_cooldown_secs;
  int hunt_enabled;
  int arrest_enabled;
  int trade_enabled;
  int assist_enabled;
  int call_help_enabled;
  int target_alignment_pref;
  int opportunistic_pref;
  uint32_t signature;
  uint32_t profile_flags;
  char matched_keywords[AI_INTENT_KEYWORDS_MAX];
  int style;
  int initialized;
  int notice_entry, notice_departure, notice_speech, notice_whispers, notice_emotes, notice_combat, notice_self_attack, notice_ally_attack, notice_corpses, notice_drops, notice_gifts, notice_crimes;
  int hearing_sensitivity, observation_sensitivity, suspicion_threshold, recognition_confidence;
  int memory_enabled, memory_max_actors, memory_ordinary_duration, memory_important_duration, trust_gain, trust_loss, fear_gain, fear_decay, hostility_gain, hostility_decay, familiarity_gain, familiarity_decay, forgiveness;
  int remember_attacks, remember_assistance, remember_crimes, remember_gifts, remember_insults, remember_conversations, remember_threats, remember_last_room, remember_deaths;
  int threat_enabled[AI_THREAT_RESPONSE_MAX], threat_cooldown, calm_reset_time, repeated_event_window, threat_step_count;
  struct ai_threat_step threat_steps[10];
  int combat_style, combat_enabled, may_initiate, may_assist, may_call_help, may_flee;
  int protect_trusted, protect_group, protect_same_role, protect_same_prototype;
  int avoid_incapacitated, retaliate_self, retaliate_ally, retaliate_hostile, switch_targets;
  int assist_severity, target_switch_threshold, max_allies, max_responders, combat_cooldown;
  int target_weight[AI_TARGET_WEIGHTS];
};

struct ai_help_event {
  unsigned long id;
  long source_id, target_id, victim_id;
  int room_vnum, maximum_responders, responder_count, call_help_emitted, relayed;
  long responders[AI_HELP_EVENT_RESPONDERS];
  time_t created_at, expires_at;
};

struct ai_actor_recent_event {
  int type;
  long actor_idnum;
  int room_vnum;
  time_t when;
  char text[48];
};

struct ai_actor_memory_entry {
  long idnum;
  char key_name[24];
  int attitude;
  time_t last_seen_time;
  time_t last_interaction_time;
  int disposition_flags;
  int hostility;
  int trust;
  int fear;
  time_t last_update;
  int last_room_vnum;
  int flags;
  time_t last_reaction;
  int last_intent;
  time_t last_reply_time;
  int last_topic;
  time_t last_topic_time;
  char last_topic_key[32];
  float belief_confidence; /* identity confidence, bounded 0..100 */
  int belief_last_room;
  int belief_last_direction;
  time_t belief_updated_at;
  float belief_hostility;
  float belief_familiarity;
  int familiarity;
  int identity_confidence, hostility_confidence, crime_confidence, room_confidence;
  int threat_step, threat_repetitions, threat_severity;
  time_t threat_last_action, threat_last_event, threat_last_warning, threat_last_challenge;
  int threat_help_called;
};

struct ai_actor_state {
  time_t next_tick;
  time_t last_spoke;
  time_t last_room_spoke;
  int last_room_vnum_spoke;
  unsigned long pending_speech_fire_pulse;
  long pending_speech_target_idnum;
  char pending_speech[256];
  struct ai_actor_memory_entry mem[AI_MEM_MAX];
  int mem_count;
  long current_target_idnum;
  time_t target_last_seen;
  time_t next_signature_check;
  time_t last_action_time;
  time_t last_talk_time;
  time_t last_emote_time;
  int last_room_vnum;
  int talk_cooldown_pulses;
  int intent_cooldown_pulses;
  int event_ring_start;
  int event_ring_count;
  int social_spam_count;
  long pending_target_idnum;
  enum ai_event_type pending_event_type;
  char pending_event_text[64];
  time_t pending_event_time;
  int cached_zone;
  uint32_t local_topic_mask;
  int role_scores[ROLE_BOSS + 1];
  char last_pool_name[48];
  char last_speak_reason[32];
  int last_speech_hash;
  int last_emote_hash;
  int recent_speech_hashes[5];
  int recent_emote_hashes[5];
  struct ai_actor_recent_event recent_events[AI_EVENT_RING_MAX];
  struct ai_actor_brain *brain;
  time_t last_combat_action, last_target_switch, last_flee_attempt;
  long last_selected_target_idnum, last_help_event_id;
  /* Bounded combat-cycle bookkeeping: IDs, never live character pointers. */
  unsigned long combat_event_id;
  long combat_opponent_id, last_combat_opponent_id, last_switch_from_id, last_switch_to_id;
  unsigned long last_assist_event_id, last_help_heard_event_id, last_help_answered_event_id;
  int combat_active, combat_end_recorded, combat_end_reason;
  time_t combat_started_at;
  int active_schedule_id, previous_schedule_id, schedule_destination_vnum, schedule_route_id, schedule_waypoint, patrol_direction, schedule_failures, schedule_attempts, schedule_interrupted, schedule_reason, expected_room_vnum, schedule_state, schedule_skipped_id, last_arrival_action, last_departure_action, schedule_wander_suppressed;
  int schedule_departure_done, schedule_arrival_done, schedule_failure_emitted, schedule_failure_applied, schedule_disabled_id, schedule_activation_day, schedule_activation_start, schedule_activation_end;
  int resume_schedule_id, resume_route_id, resume_waypoint, resume_direction, resume_state, resume_destination_vnum, resume_departure_done, resume_arrival_done;
  time_t schedule_started_at, last_schedule_eval, last_schedule_move, schedule_wait_until, schedule_retry_at;
};

uint32_t ai_actor_compute_signature(struct char_data *mob);
void ai_actor_build_profile(struct char_data *mob, int full_reset);
void ai_actor_rebuild_profile(struct char_data *mob);
void ai_actor_refresh_profile(struct char_data *mob, int force);
void ai_actor_refresh_live_mobs_by_vnum(mob_vnum vnum);

void ai_actor_init(struct char_data *mob);
void ai_actor_free(struct char_data *mob);
int ai_actor_tick(struct char_data *mob, time_t now);
/* Pure schedule helpers use the canonical in-game time_info calendar. */
int ai_schedule_time_matches(int start, int end, int hour);
int ai_schedule_day_matches(int mask, int day);
int ai_schedule_select(const struct mob_ai_config *c, int day, int hour);
int ai_schedule_entries_overlap(const struct ai_schedule_entry *a, const struct ai_schedule_entry *b);
int ai_schedule_interruption_is_minor(int reason);
int ai_schedule_entry_activation_signature(const struct ai_schedule_entry *e, int day);
int ai_schedule_entry_is_suppressed_for_window(const struct ai_actor_state *s, const struct ai_schedule_entry *e, int day);
int ai_schedule_retry_ready(const struct ai_actor_state *s, time_t now);
int ai_schedule_travel_timed_out(const struct ai_actor_state *s, const struct ai_schedule_entry *e, time_t now);
int ai_schedule_should_block_wandering(const struct ai_actor_state *s);
int ai_patrol_advance(const struct ai_patrol_route *route, int index, int direction, int *next_direction);
int ai_schedule_add(struct mob_ai_config *c, const struct ai_schedule_entry *entry);
int ai_schedule_delete(struct mob_ai_config *c, int index);
int ai_schedule_move(struct mob_ai_config *c, int from, int to);
int ai_schedule_duplicate(struct mob_ai_config *c, int index);
int ai_patrol_add(struct mob_ai_config *c, const struct ai_patrol_route *route);
int ai_patrol_delete(struct mob_ai_config *c, int index);
int ai_patrol_move(struct mob_ai_config *c, int from, int to);
int ai_patrol_duplicate(struct mob_ai_config *c, int index);
int ai_patrol_waypoint_add(struct ai_patrol_route *route, const struct ai_patrol_waypoint *waypoint);
int ai_patrol_waypoint_delete(struct ai_patrol_route *route, int index);
int ai_patrol_waypoint_duplicate(struct ai_patrol_route *route, int index);
int ai_patrol_waypoint_move(struct ai_patrol_route *route, int from, int to);
void ai_actor_schedule_preview(const struct mob_ai_config *c, int day, int hour, char *out, size_t size);
void ai_actor_schedule_validate(const struct mob_ai_config *c, char *out, size_t size);
void ai_actor_patrol_preview(const struct mob_ai_config *c, int route_id, char *out, size_t size);
void ai_actor_schedule_interrupt(struct char_data *mob, int reason, long actor_id, time_t now);
void ai_actor_schedule_show_state(struct char_data *viewer, const struct char_data *mob);
void ai_actor_record_damage(struct char_data *mob, struct char_data *actor, int dam);
void ai_actor_record_help(struct char_data *mob, struct char_data *actor, int amount);
void ai_actor_record_crime(struct char_data *mob, struct char_data *criminal, int flags);
void ai_actor_record_room_crime(struct char_data *witness, struct char_data *criminal, int flags);
void ai_actor_event_enter(struct char_data *actor, room_rnum room);
enum ai_actor_persona get_actor_persona(struct char_data *ch);
void ai_actor_event_leave(struct char_data *actor, room_rnum room);
void ai_actor_event_say(struct char_data *actor, const char *msg);
void ai_actor_event_whisper(struct char_data *actor, struct char_data *target, const char *msg);
void ai_actor_event_emote(struct char_data *actor, const char *msg);
void ai_actor_event_combat_start(struct char_data *attacker, struct char_data *victim);
void ai_actor_on_room_event(struct char_data *mob, enum ai_event_type type, struct char_data *actor, const char *text);
void ai_actor_event_corpse(struct char_data *dead, room_rnum room);
void ai_actor_event_drop(struct char_data *actor, struct obj_data *obj);
void ai_actor_event_give(struct char_data *actor, struct char_data *to, struct obj_data *obj);
void ai_actor_event_attack(struct char_data *attacker, struct char_data *victim, int damage);
void ai_actor_event_combat_end(struct char_data *actor, struct char_data *opponent, int reason);
void ai_actor_event_defeat(struct char_data *actor, struct char_data *opponent);
void ai_actor_event_fled(struct char_data *actor, struct char_data *opponent, int actor_fled);
void ai_actor_combat_preview(const struct mob_ai_config *config, char *out, size_t out_size);
void ai_actor_combat_validate(const struct mob_ai_config *config, char *out, size_t out_size);
int ai_actor_target_score(struct char_data *mob, struct char_data *candidate);
int ai_actor_is_local_ally(struct char_data *mob, struct char_data *other, const char **reason);
int ai_actor_should_flee(struct char_data *mob);
const char *ai_actor_target_weight_name(int index);
int ai_threat_step_edit(struct mob_ai_config *config, int index, const struct ai_threat_step *step);
int ai_threat_step_move(struct mob_ai_config *config, int from, int to);
int ai_help_event_admit(struct ai_help_event *event, long responder_id, time_t now);
void ai_actor_event_crime(struct char_data *criminal, int flags);
enum ai_relationship { AI_REL_UNKNOWN, AI_REL_FAMILIAR, AI_REL_TRUSTED, AI_REL_FEARED, AI_REL_HOSTILE, AI_REL_TRUSTED_FEARED, AI_REL_HOSTILE_FEARED };
enum ai_relationship ai_actor_relationship(const struct ai_actor_memory_entry *memory);
void ai_actor_schedule_reaction_speech(struct char_data *mob, struct char_data *target, const char *msg);
struct mob_ai_config *mob_ai_config_new(void);
struct mob_ai_config *mob_ai_config_copy(const struct mob_ai_config *from);
void mob_ai_config_free(struct mob_ai_config *config);
void mob_ai_config_validate(struct mob_ai_config *config);
const char *ai_social_style_name(int style);
const char *ai_dialogue_category_name(int category);
const char *ai_actor_config_role_name(int role);
const char *ai_actor_config_movement_name(int movement);
const char *ai_actor_config_role_summary(int role);
const char *ai_actor_config_movement_summary(int movement);
int mob_ai_dialogue_set(struct mob_ai_config *config, int category, int index, const char *line);
int mob_ai_dialogue_delete(struct mob_ai_config *config, int category, int index);
int mob_ai_dialogue_move(struct mob_ai_config *config, int category, int from, int to);
/* Deterministic social response modifier; every personality trait contributes. */
int ai_actor_personality_response_modifier(const int personality[AI_ACTOR_PERSONALITIES]);
/* Pure threat-policy helpers, deliberately deterministic for regression tests. */
int ai_threat_response_available(int type);
int ai_threat_response_targeted(int type);
int ai_threat_step_valid(const struct ai_threat_step *step, const int enabled[AI_THREAT_RESPONSE_MAX]);
int ai_threat_severity(int event_base, const struct ai_actor_memory_entry *memory, const int personality[AI_ACTOR_PERSONALITIES], int confidence, int injured_nonhostile);
int ai_threat_choose_step(const struct ai_actor_profile *profile, const struct ai_actor_memory_entry *memory, time_t now);

#endif
