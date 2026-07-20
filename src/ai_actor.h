#ifndef _AI_ACTOR_H_
#define _AI_ACTOR_H_

#include <stdint.h>

#define AI_MEM_MAX 12
#define AI_EVENT_RING_MAX 12
#define AI_INTENT_KEYWORDS_MAX 192
#define AI_ACTOR_PERSONALITIES 12
#define AI_DIALOGUE_CATEGORIES 7
#define AI_DIALOGUE_MAX_LINES 8
#define AI_DIALOGUE_LINE_MAX 240
#define AI_SOCIAL_COOLDOWN_MIN 1
#define AI_SOCIAL_COOLDOWN_MAX 300

enum mob_ai_profile_mode { MOB_AI_INFERRED = 0, MOB_AI_CUSTOM, MOB_AI_INFERRED_OVERRIDES };
enum mob_ai_personality { AI_TRAIT_AGGRESSION, AI_TRAIT_BRAVERY, AI_TRAIT_SOCIABILITY, AI_TRAIT_CURIOSITY, AI_TRAIT_DISCIPLINE, AI_TRAIT_HONESTY, AI_TRAIT_GREED, AI_TRAIT_COMPASSION, AI_TRAIT_LOYALTY, AI_TRAIT_PATIENCE, AI_TRAIT_SUSPICION, AI_TRAIT_PRIDE };
enum ai_social_style { AI_SOCIAL_SILENT, AI_SOCIAL_RESERVED, AI_SOCIAL_POLITE, AI_SOCIAL_FRIENDLY, AI_SOCIAL_TALKATIVE, AI_SOCIAL_BOASTFUL, AI_SOCIAL_RUDE, AI_SOCIAL_HOSTILE, AI_SOCIAL_EXTORTING, AI_SOCIAL_PREACHER, AI_SOCIAL_GOSSIP };
enum ai_dialogue_category { AI_DIALOGUE_GREETING, AI_DIALOGUE_FRIENDLY, AI_DIALOGUE_SUSPICIOUS, AI_DIALOGUE_HOSTILE, AI_DIALOGUE_AMBIENT_SPEECH, AI_DIALOGUE_AMBIENT_EMOTE, AI_DIALOGUE_FAREWELL };
enum mob_ai_config_movement { AI_MOVE_STATIONARY, AI_MOVE_RANDOM, AI_MOVE_PATROL, AI_MOVE_SCHEDULED, AI_MOVE_GUARD_ROOM, AI_MOVE_RETURN_HOME };
struct mob_ai_config {
  int mode, role, movement, social;
  unsigned long override_mask;
  int personality[AI_ACTOR_PERSONALITIES];
  int home_room_vnum, work_room_vnum, guard_room_vnum, roam_radius, pursuit_distance, movement_delay;
  int greeting_enabled, ambient_speech_enabled, ambient_emotes_enabled, whisper_enabled;
  int respond_strangers, respond_trusted, respond_feared, respond_hostile;
  int speech_cooldown, room_speech_cooldown, emote_cooldown;
  int dialogue_count[AI_DIALOGUE_CATEGORIES];
  char *dialogue[AI_DIALOGUE_CATEGORIES][AI_DIALOGUE_MAX_LINES];
  int flee_hp_percent, surrender_hp_percent, assist_enabled, call_help_enabled, hunt_enabled, return_home, stay_zone;
};
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
  float belief_confidence;
  int belief_last_room;
  int belief_last_direction;
  time_t belief_updated_at;
  float belief_hostility;
  float belief_familiarity;
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
};

uint32_t ai_actor_compute_signature(struct char_data *mob);
void ai_actor_build_profile(struct char_data *mob, int full_reset);
void ai_actor_rebuild_profile(struct char_data *mob);
void ai_actor_refresh_profile(struct char_data *mob, int force);
void ai_actor_refresh_live_mobs_by_vnum(mob_vnum vnum);

void ai_actor_init(struct char_data *mob);
void ai_actor_free(struct char_data *mob);
int ai_actor_tick(struct char_data *mob, time_t now);
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
void ai_actor_schedule_reaction_speech(struct char_data *mob, struct char_data *target, const char *msg);
struct mob_ai_config *mob_ai_config_new(void);
struct mob_ai_config *mob_ai_config_copy(const struct mob_ai_config *from);
void mob_ai_config_free(struct mob_ai_config *config);
void mob_ai_config_validate(struct mob_ai_config *config);
const char *ai_social_style_name(int style);
const char *ai_dialogue_category_name(int category);
int mob_ai_dialogue_set(struct mob_ai_config *config, int category, int index, const char *line);
int mob_ai_dialogue_delete(struct mob_ai_config *config, int category, int index);
int mob_ai_dialogue_move(struct mob_ai_config *config, int category, int from, int to);
/* Deterministic social response modifier; every personality trait contributes. */
int ai_actor_personality_response_modifier(const int personality[AI_ACTOR_PERSONALITIES]);

#endif
