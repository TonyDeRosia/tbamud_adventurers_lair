#include "conf.h"
#include "sysdep.h"

#include <ctype.h>
#include <math.h>

#include "structs.h"
#include "utils.h"
#include "db.h"
#include "comm.h"
#include "handler.h"
#include "class.h"
#include "graph.h"
#include "fight.h"
#include "shop.h"
#include "spells.h"
#include "act.h"
#include "ai_actor.h"
#include "ai_actor_brain.h"

#define AI_HOSTILE_ATTACK_THRESHOLD 12
#define AI_ROOM_IDLE_SKIP_SECS 12
#define AI_BFS_MAX_DEPTH 6
#define AI_SIGNATURE_CHECK_SECS 10
#define AI_TARGET_REACTION_COOLDOWN_SECS 18
#define AI_EVENT_IGNORE_MSG_SECS 4
#define AI_PER_PLAYER_REPLY_COOLDOWN_SECS 6
#define AI_INTENT_THRESHOLD 25
#define AI_INTENT_COOLDOWN_MIN 6
#define AI_INTENT_COOLDOWN_MAX 12
#define AI_TALK_COOLDOWN_MIN 12
#define AI_TALK_COOLDOWN_MAX 20
#define AI_ROLE_AMBIGUOUS_MARGIN 3
#define AI_TOPIC_MEMORY_WINDOW_SECS 30
#define AI_TOPIC_CONTINUITY_WINDOW_SECS 60
#define AI_BFS_QUEUE_MAX 256
#define AI_ROOM_PLAYER_SPEECH_GRACE_SECS 8
#define AI_NPC_CONVO_MAX_LINES 6
#define AI_NPC_CONVO_LINE_GAP_SECS 10
#define AI_NPC_CONVO_TOPIC_MIN_SECS 20
#define AI_NPC_CONVO_TOPIC_MAX_SECS 30
#define AI_NPC_CONVO_START_EMPTY_SECS 60
#define AI_NPC_CONVO_START_WITH_PLAYERS_SECS 180
#define AI_PLAYER_ARB_CACHE_MAX 128
#define AI_PLAYER_ARB_TTL_SECS 6
#define AI_ATTENTION_THRESHOLD 0.25f
#define AI_GOAL_STACK_MAX 4
#define AI_THREAT_TABLE_MAX 16
#define AI_ZONE_HEATMAP_MAX 256
#define AI_ZONE_ALERT_MAX 64
#define AI_MOOD_SPRING_K 0.15f
#define AI_MOOD_DAMPING 0.75f
#define AI_SESSION_READ_MAX 16
#define AI_MIN_ROLE_FITNESS 10
#define AI_SERVICE_CACHE_MAX_ZONES 256
#define AI_SERVICE_CANDIDATES_MAX 192
#define AI_SERVICE_CACHE_TTL_SECS 120
#define AI_ROUTE_MAX_STEPS 6
#define AI_WORKING_MEM_MAX 8
#define AI_RECENT_CORE_HASH_MAX 12
#define AI_RECENT_REPLY_HASH_MAX 6
#define AI_REQ_MAX 3
#define AI_ZONE_ITEMS_MAX 512
#define AI_ZONE_ROOMS_MAX 512
#define AI_ZONE_SHOPS_MAX 64
#define AI_ZONE_KEYWORDS_MAX 1024
#define AI_ZONE_CACHE_MAX 64

enum ai_speech_domain {
  DOMAIN_NONE = 0,
  DOMAIN_SERVICES,
  DOMAIN_SHOPPING,
  DOMAIN_DIRECTIONS,
  DOMAIN_RUMOR,
  DOMAIN_QUEST,
  DOMAIN_LAW,
  DOMAIN_SOCIAL,
  DOMAIN_PERSONAL,
};

enum ai_imtb_personality {
  IMTB_STOIC = 0,
  IMTB_FRIENDLY,
  IMTB_GRUFF,
  IMTB_WITTY,
  IMTB_SUSPICIOUS,
  IMTB_BOOKISH,
  IMTB_ZEALOUS,
  IMTB_EERIE,
  IMTB_MAX
};

struct ai_imtb_profile {
  const char *tag;
  int warmth;
  int verbosity;
  int curiosity;
  int certainty;
  int humor;
  int formality;
};

struct ai_working_mem_entry {
  long player_idnum;
  char text[64];
  int intent;
  int topic_target;
  int speech_act;
  int domain;
  time_t when;
  int answered;
  int event_id;
};

struct ai_world_facts {
  char service_name[48];
  char route_snippet[32];
  char zone_status[64];
  char example_item[32];
  int service_found;
  int confidence;
};

struct ai_reply_context {
  const char *current_text;
  int intent;
  int speech_act;
  int domain;
  int topic_target;
  int requested_targets[AI_REQ_MAX];
  int requested_count;
  int primary_topic_target;
  const struct ai_working_mem_entry *callback_hint;
  int callback_hint_score;
  char callback_prefix[80];
  struct ai_world_facts facts;
  int confidence;
  int personality;
  const struct ai_imtb_profile *profile;
  const char *chosen_core;
  int from_template;
  int event_id;
};

struct ai_zone_item_ref { obj_vnum vnum; int type; int wear; int extra_flags; char name[40]; };
struct ai_zone_room_ref { int room_vnum; char name[48]; };
struct ai_zone_shop_ref { int room_vnum; int shop_nr; int sells_mask; };

struct ai_zone_knowledge {
  int zone;
  int built;
  time_t built_at;
  struct ai_zone_shop_ref shops[AI_ZONE_SHOPS_MAX];
  int shop_count;
  struct ai_zone_item_ref items[AI_ZONE_ITEMS_MAX];
  int item_count;
  struct ai_zone_room_ref rooms[AI_ZONE_ROOMS_MAX];
  int room_count;
};

enum ai_time_bucket {
  AI_TIME_DAY = 0,
  AI_TIME_DUSK,
  AI_TIME_NIGHT,
  AI_TIME_DAWN
};

enum ai_player_archetype {
  AI_ARCH_UNKNOWN = 0,
  AI_ARCH_TROUBLEMAKER,
  AI_ARCH_ERRATIC,
  AI_ARCH_TRANSACTOR,
  AI_ARCH_EXPLORER,
  AI_ARCH_SOCIALIZER,
  AI_ARCH_SILENT
};

enum ai_conv_arc_state {
  AI_ARC_STRANGER = 0,
  AI_ARC_ACKNOWLEDGED,
  AI_ARC_ENGAGED,
  AI_ARC_RAPPORT,
  AI_ARC_COLD
};

enum ai_reply_goal {
  GOAL_INFORM = 0,
  GOAL_DEFLECT,
  GOAL_WARN,
  GOAL_CONNECT,
  GOAL_DISMISS,
  GOAL_CLARIFY,
  GOAL_SERVE
};

enum ai_reply_stance {
  STANCE_OPEN = 0,
  STANCE_NEUTRAL,
  STANCE_GUARDED,
  STANCE_HOSTILE,
  STANCE_WARM,
  STANCE_AMUSED
};

struct ai_reply_intention {
  int topic;
  enum ai_reply_goal goal;
  enum ai_reply_stance stance;
  int be_specific;
  int be_brief;
};

struct ai_context_vector {
  int safe_room;
  int lawful_or_city;
  int dark;
  int indoors;
  int visible_pc_count;
  int visible_mob_count;
  int guard_present;
  int authority_present;
  int open_exit_count;
  int recent_violence;
  enum ai_time_bucket time_bucket;
  float zone_danger;
  float zone_alert;
  float alignment_delta;
};

struct ai_session_read_entry {
  long player_idnum;
  time_t last_exchange_time;
  time_t last_reply_time;
  time_t cooldown_until;
  int exchange_count;
  int speech_count;
  int emote_count;
  int greet_count;
  int question_count;
  int hostile_count;
  int social_count;
  int transact_count;
  int story_count;
  int topic_jump_count;
  int last_speech_act;
  int non_hostile_streak;
  float aggression;
  float curiosity;
  float social;
  float erratic;
  float first_impression;
  int first_impression_set;
  enum ai_player_archetype archetype;
  enum ai_conv_arc_state arc;
  int rapport_rumor_used;
  int last_topic_target;
  int last_domain;
  time_t last_turn_time;
  unsigned int last_player_utter_hash;
  struct ai_working_mem_entry working_mem[AI_WORKING_MEM_MAX];
  int working_mem_head;
  int working_mem_count;
  int working_mem_next_id;
  unsigned int recent_reply_hashes[AI_RECENT_REPLY_HASH_MAX];
  int recent_reply_head;
  int recent_reply_count;
};

enum ai_conversation_topic {
  AI_CONV_TOPIC_UNKNOWN = 0,
  AI_CONV_TOPIC_WEATHER,
  AI_CONV_TOPIC_SMALLTALK,
  AI_CONV_TOPIC_DIRECTIONS,
  AI_CONV_TOPIC_SHOP,
  AI_CONV_TOPIC_INN,
  AI_CONV_TOPIC_BANK,
  AI_CONV_TOPIC_HELP,
  AI_CONV_TOPIC_THREAT,
  AI_CONV_TOPIC_CRIME,
  AI_CONV_TOPIC_PATROL,
  AI_CONV_TOPIC_RUMOR
};

enum ai_action_type {
  AI_ACTION_SPEAK = 0,
  AI_ACTION_SPEAK_WARN,
  AI_ACTION_SPEAK_DEFLECT,
  AI_ACTION_IGNORE,
  AI_ACTION_CALL_HELP,
  AI_ACTION_FLEE,
  AI_ACTION_OBSERVE,
  AI_ACTION_EMOTE_REACT,
  AI_ACTION_COUNT
};

enum ai_goal_type {
  AI_GOAL_NONE = 0,
  AI_GOAL_MAINTAIN_POST,
  AI_GOAL_PURSUE_OFFENDER,
  AI_GOAL_ESCORT,
  AI_GOAL_SELL,
  AI_GOAL_GREET_TRAVELERS,
  AI_GOAL_MONITOR_SUSPECT,
  AI_GOAL_SEEK_SAFETY,
  AI_GOAL_AMBUSH,
  AI_GOAL_REGROUP,
  AI_GOAL_RECRUIT,
  AI_GOAL_IDLE_WANDER
};

struct ai_goal_entry {
  enum ai_goal_type type;
  float priority;
  time_t committed_until;
  time_t expires_at;
  long target_idnum;
};

struct ai_threat_entry {
  long player_idnum;
  float threat;
  time_t last_damage_time;
  time_t last_heal_time;
  time_t updated_at;
};

struct ai_zone_heat {
  int zone_rnum;
  float danger;
  float profit;
  time_t updated_at;
};

struct ai_zone_alert {
  int zone_rnum;
  float alert_level;
  long target_idnum;
  time_t raised_at;
  time_t expires_at;
};

struct ai_conv_actor_state {
  struct char_data *mob;
  int voice_profile_ready;
  struct ai_voice_profile {
    int vocab_tier;
    int rhythm;
    int hedge_style;
    int topic_lean;
    int tic_index;
    int opener_index;
    int closer_index;
    int intensity;
    int mbti_ei;
    int mbti_sn;
    int mbti_tf;
    int mbti_jp;
  } voice_profile;
  int current_topic;
  long partner_id;
  long last_speaker_id;
  time_t last_line_time;
  time_t topic_expires_at;
  int depth_counter;
  time_t updated_at;
  struct ai_goal_entry goals[AI_GOAL_STACK_MAX];
  int goal_count;
  float mood_current;
  float mood_target;
  float mood_velocity;
  time_t mood_last_tick;
  struct ai_threat_entry threats[AI_THREAT_TABLE_MAX];
  struct ai_session_read_entry session_reads[AI_SESSION_READ_MAX];
  unsigned int recent_core_hashes[AI_RECENT_CORE_HASH_MAX];
  int recent_core_head;
  int recent_core_count;
  int personality;
  int personality_ready;
  unsigned int recent_lead_hashes[6];
  int recent_lead_head;
  int recent_lead_count;
  int tone_clipped;
  int tone_no_extras;
  int tone_day_trade;
  int tone_night_watch;
};

struct ai_conv_room_state {
  room_rnum room;
  struct char_data *speaker_a;
  struct char_data *speaker_b;
  int topic;
  int active;
  int line_count;
  long last_speaker_id;
  time_t last_line_time;
  time_t topic_expires_at;
  time_t last_start_time;
  time_t last_player_speech_time;
  time_t last_violence_time;
};

#define AI_CONV_ACTOR_STATE_MAX 512
#define AI_CONV_ROOM_STATE_MAX 256
static struct ai_conv_actor_state ai_conv_actor_states[AI_CONV_ACTOR_STATE_MAX];
static struct ai_conv_room_state ai_conv_room_states[AI_CONV_ROOM_STATE_MAX];

static int ai_debug = AI_ACTOR_DEBUG;

struct ai_service_candidate {
  int service_type;
  room_vnum room_vnum;
  int shop_nr;
  obj_vnum sample_item_vnum;
  int has_weapon;
  int has_armor;
  int has_food;
};

struct ai_service_zone_cache {
  int in_use;
  int zone_rnum;
  time_t last_built;
  unsigned long build_version;
  struct ai_service_candidate candidates[AI_SERVICE_CANDIDATES_MAX];
  int candidate_count;
};

static struct ai_service_zone_cache ai_service_zone_cache[AI_SERVICE_CACHE_MAX_ZONES];
static unsigned long ai_service_cache_build_version = 0;
static struct ai_zone_knowledge ai_zone_cache[AI_ZONE_CACHE_MAX];
static struct ai_zone_knowledge *ai_zone_get_cache(int zone_rnum);
static void ai_zone_build_cache(struct ai_zone_knowledge *zk);
static void ai_detect_requested_targets(const char *text, int *targets, int *count);
static int ai_is_question_shape(const char *text);
static int ai_pick_stance_prefix(int role, int style, unsigned long seed, const char **out_prefix);
static const char *ai_pick_question_mirror_clause(int topic_target, int role, int style, unsigned long seed);
static const char *ai_role_because_clause(int role, int style);
static int ai_line_has_min_service_usefulness(const char *line);
static void ai_append_clause(char *buf, size_t bufsz, const char *clause);

struct ai_synonym_group;
struct ai_word_tier_entry {
  const char *concept;
  const char *tier0;
  const char *tier1;
  const char *tier2;
  const char *tier3;
};

struct ai_phrase_entry {
  const char *tag;
  int tier;
  int rhythm;
  const char *text;
};

static const struct ai_imtb_profile ai_imtb_profiles[IMTB_MAX] = {
  {"stoic", -1, 0, 0, 1, 0, 2},
  {"friendly", 2, 1, 2, 0, 0, 0},
  {"gruff", -2, 0, 0, -1, 0, 0},
  {"witty", 1, 1, 1, 0, 2, 1},
  {"suspicious", -1, 0, 0, -1, 0, 1},
  {"bookish", 0, 2, 1, 1, 0, 2},
  {"zealous", 0, 1, 1, 1, 0, 2},
  {"eerie", -1, 1, 0, -1, 0, 1}
};

static const char *const ai_imtb_leadin[IMTB_MAX][8] = {
  {"Listen.", "Noted.", "Very well.", "Let's keep this plain.", "Straight answer:", NULL},
  {"Hey there.", "Glad you asked.", "Happy to help.", "Good to see you.", "All right, friend.", NULL},
  {"Yeah.", "Make it quick.", "Fine.", "Spit it out.", "Keep it short.", NULL},
  {"Well now.", "If you insist.", "Let us make this interesting.", "Right then.", "Here's the lively version.", NULL},
  {"Careful.", "Watch who you ask.", "Keep your voice down.", "Quiet now.", "Stay sharp.", NULL},
  {"If memory serves,", "By common practice,", "According to what I know,", "From what the records show,", "Let me order this clearly:", NULL},
  {"By oath,", "By the light,", "By shadow,", "The omen is this:", "By ritual measure,", NULL},
  {"Hush.", "The air remembers.", "Listen to the quiet,", "The veil stirs.", "Hear the old hush,", NULL}
};


static const char *const ai_role_leadin_guard[] = {
  "On watch:", "By my duty,", NULL
};
static const char *const ai_role_leadin_merchant[] = {
  "On wares and prices:", "For fair stock,", NULL
};
static const char *const ai_role_leadin_innkeeper[] = {
  "By the hearth,", "For room and stew,", NULL
};
static const char *const ai_role_leadin_cultist[] = {
  "By ritual sign,", "Read the omen:", NULL
};
static const char *const ai_role_leadin_spirit[] = {
  "From beyond the veil,", "Through the old currents,", NULL
};
static const char *const ai_role_leadin_bandit[] = {
  "Keep walking,", "About that purse,", NULL
};

static const char *const ai_imtb_uncertainty[IMTB_MAX][4] = {
  {"Unclear.", "Not certain.", "Hard to say.", NULL},
  {"I think", "Might be", "As best I can tell", NULL},
  {"Dunno.", "Not my lane.", "Could be.", NULL},
  {"Maybe.", "Could be.", "Depends who you ask.", NULL},
  {"Not sure.", "Could be a trap.", "I cannot confirm.", NULL},
  {"Possibly.", "I cannot confirm.", "Records are thin.", NULL},
  {"Perhaps.", "By oath, uncertain.", "I cannot swear it.", NULL},
  {"Mist hides it.", "Hard to see.", "The signs are unclear.", NULL}
};

static const char *const ai_imtb_followup[IMTB_MAX][4] = {
  {"Do you need anything else?", "Is that enough?", NULL, NULL},
  {"Want help with anything else?", "Need another pointer?", NULL, NULL},
  {"Anything else?", "You done?", NULL, NULL},
  {"Want the longer version?", "Shall I add details?", NULL, NULL},
  {"Who sent you?", "Need anything else, carefully?", NULL, NULL},
  {"Would you like a more precise route?", "Shall I cross-check another lead?", NULL, NULL},
  {"Do you seek more guidance?", "Shall I name another place?", NULL, NULL},
  {"Do you still seek an answer?", "Shall we follow another thread?", NULL, NULL}
};

static struct char_data *ai_find_player_by_idnum_room(struct char_data *mob, long idnum);
static const char *ai_pick_phrase(const char *const *pool);
static const char *ai_pool_pick(const char *const *pool);
static int ai_role_can_give_directions(int role);
static int ai_role_can_answer_intent(int role, int style, int intent);
static const char *ai_role_redirect_line(int role, int style, int topic_target);
static int ai_text_has_sub_ci(const char *hay, const char *needle);
static int ai_role_priority_score(struct char_data *mob);
static void ai_state_refresh_local_topics(struct char_data *mob);
static struct ai_conv_actor_state *ai_conv_actor_state_get(struct char_data *mob, int create);
static struct ai_conv_room_state *ai_conv_room_state_get(room_rnum room, int create);
static void ai_conv_actor_reset(struct char_data *mob, time_t now);
static void ai_conv_room_end(struct ai_conv_room_state *room_st, time_t now);
static int ai_conv_topic_from_intent(int intent);
static int ai_conv_topic_for_pair(struct char_data *a, struct char_data *b);
static int ai_conv_room_has_player(room_rnum room);
static const char *ai_conv_line_for_topic(struct char_data *speaker, int topic);
static int ai_conv_emit_line(struct ai_conv_room_state *room_st, struct char_data *speaker, struct char_data *partner, time_t now);
static int ai_conv_try_progress(struct char_data *mob, time_t now);
static int ai_conv_try_start(struct char_data *mob, time_t now);
static int ai_player_speech_classify(const char *text, int *out_confidence, int *out_is_weather);
static int ai_intent_from_player_class(int speech_class);
static int ai_detect_emote_kind(const char *text);
static int ai_detect_topic_target_from_text(const char *text);
static int ai_actor_room_response_slot(struct char_data *mob, struct char_data *actor, enum ai_event_type type, int intent, int confidence, const char *normalized);
static int ai_actor_ensure_ready(struct char_data *mob);
static unsigned long ai_hash_mix(unsigned long h, unsigned long v);
static unsigned long ai_hash_text_stable(const char *text);
static int ai_imtb_pick_personality(struct char_data *mob, int role, int style);
static const struct ai_imtb_profile *ai_imtb_profile_get(int personality);
static const char *ai_imtb_pick_leadin(struct ai_conv_actor_state *st, const struct ai_reply_context *rctx, int role, int style, unsigned long seed, int *out_suppressed);
static const char *ai_imtb_pick_uncertainty(const struct ai_reply_context *rctx, int role, int style, unsigned long seed, int *out_used);
static const char *ai_imtb_pick_followup(const struct ai_reply_context *rctx, int role, int style, unsigned long seed);
static unsigned long ai_conv_seed(struct char_data *mob, int intent, unsigned int counter);
static int ai_hash_ring_has(const unsigned int *ring, int count, int head, int max, unsigned int v);
static void ai_hash_ring_push(unsigned int *ring, int *head, int *count, int max, unsigned int v);
static int ai_template_pick_index(const int *ids, int count, unsigned long seed, int avoid_id, int avoid_prev, int avoid_recent1, int avoid_recent2);
static struct ai_conv_reply_state *ai_conv_reply_state_get(struct char_data *mob, int create);
static int ai_is_weather_smalltalk(const char *text);
static const char *ai_synonym_pick(const struct ai_synonym_group *groups, const char *token, unsigned long seed, int slot);
static void ai_template_expand(const char *tpl, const struct ai_synonym_group *groups, unsigned long seed, char *out, size_t outsz);
static const char *ai_template_reply_for_intent(struct char_data *mob, int intent, const char *text, int avoid_template_id, int *out_template_id);
static void ai_voice_profile_derive(struct char_data *mob, struct ai_voice_profile *out);
static void ai_traits_to_mbti(const struct ai_actor_traits *t, struct ai_voice_profile *vp, int role);
static const struct ai_voice_profile *ai_voice_profile_get(struct char_data *mob);
static const char *ai_word(const char *concept, int tier);
static const char *ai_word_sn(const char *concept, int vocab_tier, int sn);
static const char *ai_phrase(const char *tag, int tier, int rhythm, unsigned long seed, int slot);
static const char *ai_mbti_string(const struct ai_voice_profile *vp);
static void ai_mbti_compound_modifier(const struct ai_voice_profile *vp, int speech_act, int *out_add_followup_question, int *out_add_topic_lean, int *out_suppress_opener, int *out_use_emotional_color, unsigned long seed);
static int ai_line_is_role_legal(const char *line, int role, int style);
static struct ai_reply_intention ai_form_intention(struct char_data *mob, int speech_act, int speech_class, int suspicion_bucket, int arc_state, const struct ai_context_vector *ctx, const struct ai_session_read_entry *sr, struct ai_actor_memory_entry *e, time_t now);
static const char *ai_select_content_for_intention(struct char_data *mob, const struct ai_reply_intention *in, const struct ai_reply_context *rctx, struct ai_session_read_entry *sr, int *out_from_template);
static void
ai_voice_assemble(struct char_data *mob, const struct ai_voice_profile *vp, const struct ai_reply_intention *in, const struct ai_reply_context *rctx, unsigned long seed, char *out, size_t outsz);
static float ai_event_salience(enum ai_event_type type, int role, const char *text);
static float ai_event_recency(struct char_data *mob, enum ai_event_type type, struct char_data *actor, time_t now);
static float ai_attention_score(struct char_data *mob, enum ai_event_type type, struct char_data *actor, const char *text, time_t now);
static float ai_belief_confidence_now(struct ai_actor_memory_entry *e, time_t now);
static float ai_belief_value_decay(float base, time_t updated_at, time_t now, float half_life);
static struct ai_goal_entry *ai_goal_active(struct char_data *mob, time_t now);
static int ai_goal_interferes(enum ai_goal_type a, enum ai_goal_type b);
static void ai_goal_push(struct char_data *mob, enum ai_goal_type type, float priority, int commit_secs, int expire_secs, long target_idnum);
static float ai_mood_drift(struct char_data *mob, time_t now);
static void ai_mood_spring_update(struct char_data *mob, float dt);
static float ai_heatmap_danger(int zone_rnum, time_t now);
static float ai_heatmap_profit(int zone_rnum);
static void ai_heatmap_update_danger(int zone_rnum, float incident_weight);
static void ai_heatmap_decay_tick(time_t now);
static void ai_alert_raise(int zone_rnum, float level, long target_idnum, int duration_secs);
static float ai_alert_level(int zone_rnum, time_t now);
static void ai_dbg_evt(struct char_data *mob, const char *tag, enum ai_event_type type, struct char_data *actor, const char *msg);
static void ai_alert_decay_tick(time_t now);
static enum ai_time_bucket ai_time_bucket_now(void);
static const char *ai_time_bucket_name(enum ai_time_bucket b);
static const char *ai_arch_name(enum ai_player_archetype a);
static const char *ai_arc_name(enum ai_conv_arc_state a);
static int ai_is_hostile_intent(int intent);
static int ai_is_social_intent(int intent);
static int ai_is_transact_intent(int intent);
static int ai_is_story_intent(int intent);
static int ai_is_question_intent(int intent);
static void ai_context_vector_build(struct char_data *mob, struct char_data *actor, time_t now, struct ai_context_vector *out);
static struct ai_session_read_entry *ai_session_read_get(struct ai_conv_actor_state *st, long player_id, int create, time_t now);
static void ai_session_read_update(struct ai_session_read_entry *sr, enum ai_event_type type, int intent, const char *normalized, time_t now);
static void ai_session_read_apply_impression(struct ai_session_read_entry *sr, struct ai_actor_memory_entry *e, struct ai_conv_actor_state *conv_st);
static void ai_session_read_update_arc(struct ai_session_read_entry *sr, struct char_data *mob, const struct ai_context_vector *ctx);
static float ai_session_cooldown_penalty(struct ai_session_read_entry *sr, enum ai_action_type action, time_t now);
static float ai_session_arc_action_bias(const struct ai_session_read_entry *sr, enum ai_action_type action);
static float ai_session_arch_action_bias(const struct ai_session_read_entry *sr, enum ai_action_type action);
static float ai_context_action_bias(struct char_data *mob, const struct ai_context_vector *ctx, enum ai_action_type action, int intent);
static float ai_context_suspicion_bias(struct char_data *mob, const struct ai_context_vector *ctx);
static float ai_suspicion_score(struct char_data *mob, struct char_data *actor, struct ai_actor_memory_entry *e, time_t now, int speech_act);
static float ai_utility_score(struct char_data *mob, enum ai_action_type action, struct char_data *actor, int speech_act, int intent, time_t now, float attention_score, int is_emote_event, float suspicion);
static int ai_find_closest_service(struct char_data *mob, struct char_data *player, int service_type, int *out_room_vnum, int *out_item_vnum);
static struct ai_service_zone_cache *ai_service_cache_get_zone(struct char_data *mob, int force_rebuild);
static void ai_service_cache_add_candidate(struct ai_service_zone_cache *zc, int service_type, room_vnum room, int shop_nr, obj_vnum item_vnum, int has_weapon, int has_armor, int has_food);
static void ai_service_cache_build_for_zone(int zone_rnum, struct ai_service_zone_cache *zc);
static int ai_zone_candidate_matches(const struct ai_service_candidate *c, int service_type);
static int ai_find_route_to_room(room_rnum start, room_rnum target, int max_depth, int *out_first_dir, int *out_distance);
static int ai_classify_domain(const char *normalized, int intent, int speech_act);
static void ai_working_mem_push(struct ai_working_mem_entry *mem, int *head, int *count, int *next_id, long player_idnum, const char *text, int intent, int topic_target, int speech_act, int domain, time_t now, int *out_event_id);
static void ai_working_mem_mark_answered(struct ai_working_mem_entry *mem, int count, int head, int event_id);
static const struct ai_working_mem_entry *ai_relevance_link(const struct ai_working_mem_entry *mem, int count, int head, long player_idnum, const char *current_text, int current_intent, int current_topic, int current_domain, time_t now, int *out_best_score);
static void ai_resolve_world_facts(struct char_data *mob, struct char_data *actor, int topic_target, int domain, const struct ai_context_vector *ctx, struct ai_world_facts *out);
static void ai_slot_replace(const char *in, const struct ai_world_facts *f, char *out, size_t outsz);
static void ai_sanitize_unresolved_tokens(const char *in, char *out, size_t outsz);
static const char *ai_epistemic_line(int confidence, int role, int topic_target);


enum ai_player_speech_class {
  AI_SPEECH_UNKNOWN = 0,
  AI_SPEECH_GREET,
  AI_SPEECH_WEATHER,
  AI_SPEECH_SMALLTALK,
  AI_SPEECH_DIRECTIONS,
  AI_SPEECH_SHOP,
  AI_SPEECH_INN,
  AI_SPEECH_BANK,
  AI_SPEECH_HELP,
  AI_SPEECH_THREAT,
  AI_SPEECH_OPINION,
  AI_SPEECH_COMPLIMENT,
  AI_SPEECH_ROMANCE,
  AI_SPEECH_FEELING
};

enum ai_emote_kind {
  AI_EMOTE_OTHER = 0,
  AI_EMOTE_DANCE,
  AI_EMOTE_HIGHFIVE,
  AI_EMOTE_HUG,
  AI_EMOTE_GLARE
};

struct ai_player_arb_entry {
  room_rnum room;
  long actor_id;
  long mob_id;
  enum ai_event_type type;
  unsigned long text_hash;
  time_t created_at;
  struct char_data *responder1;
  struct char_data *responder2;
  int responder1_template_id;
  int responder2_template_id;
};

static struct ai_player_arb_entry ai_player_arb_cache[AI_PLAYER_ARB_CACHE_MAX];
static struct ai_zone_heat ai_zone_heatmap[AI_ZONE_HEATMAP_MAX];
static struct ai_zone_alert ai_zone_alerts[AI_ZONE_ALERT_MAX];
static time_t ai_last_minute_decay_tick = 0;

#define AI_CONV_REPLY_STATE_MAX 512
#define AI_TEMPLATE_BUFFER_MAX 256

struct ai_conv_reply_state {
  struct char_data *mob;
  unsigned int counter;
  int last_intent;
  int last_template_ids[3];
  time_t updated_at;
};

enum ai_template_intent {
  AI_TEMPLATE_INTENT_SMALLTALK = 1,
  AI_TEMPLATE_INTENT_WEATHER = 2
};

struct ai_synonym_group {
  const char *token;
  const char *const *words;
};

struct ai_reply_template {
  int id;
  int role;
  int intent;
  const char *text;
};

static struct ai_conv_reply_state ai_conv_reply_states[AI_CONV_REPLY_STATE_MAX];



static float ai_clampf(float v, float lo, float hi)
{
  if (v < lo) return lo;
  if (v > hi) return hi;
  return v;
}

static int ai_role_is_suspicious_watcher(int role)
{
  return (role == ROLE_GUARD || role == ROLE_BOSS || role == ROLE_CULTIST || role == ROLE_MERCHANT);
}

static float ai_belief_value_decay(float base, time_t updated_at, time_t now, float half_life)
{
  float age;

  if (updated_at <= 0 || now <= updated_at)
    return ai_clampf(base, 0.0f, 1.0f);

  age = (float)(now - updated_at);
  return ai_clampf(base * expf(-0.693f * age / half_life), 0.0f, 1.0f);
}

static float ai_belief_confidence_now(struct ai_actor_memory_entry *e, time_t now)
{
  if (!e)
    return 0.0f;
  return ai_belief_value_decay(e->belief_confidence, e->belief_updated_at, now, 180.0f);
}

static float ai_event_salience(enum ai_event_type type, int role, const char *text)
{
  switch (type) {
    case AI_EVENT_PLAYER_SAY:
      switch (role) {
        case ROLE_GUARD: return 0.9f;
        case ROLE_MERCHANT: return 0.7f;
        case ROLE_BANDIT: return 0.8f;
        case ROLE_BEAST: return 0.3f;
        case ROLE_UNDEAD: return 0.4f;
        case ROLE_SPIRIT: return 0.6f;
        case ROLE_CIVILIAN: return 0.7f;
        default: return 0.6f;
      }
    case AI_EVENT_PLAYER_EMOTE:
      return 0.5f + ((text && (ai_text_has_sub_ci(text, "threat") || ai_text_has_sub_ci(text, "attack") || ai_text_has_sub_ci(text, "kill"))) ? 0.2f : 0.0f);
    case AI_EVENT_PLAYER_ENTER:
      if (role == ROLE_GUARD) return 0.8f;
      if (role == ROLE_BANDIT) return 0.9f;
      if (role == ROLE_MERCHANT) return 0.7f;
      if (role == ROLE_BEAST) return 0.85f;
      return 0.5f;
    case AI_EVENT_COMBAT_START:
      return 1.0f;
    default:
      return 0.4f;
  }
}

static float ai_event_recency(struct char_data *mob, enum ai_event_type type, struct char_data *actor, time_t now)
{
  int i;
  int recent_count = 0;
  long actor_id = (actor && !IS_NPC(actor)) ? GET_IDNUM(actor) : 0;

  if (!mob || !mob->ai_state)
    return 0.0f;

  for (i = 0; i < mob->ai_state->event_ring_count; i++) {
    int idx = (mob->ai_state->event_ring_start + i) % AI_EVENT_RING_MAX;
    struct ai_actor_recent_event *ev = &mob->ai_state->recent_events[idx];
    if (ev->type != type || ev->actor_idnum != actor_id)
      continue;
    if ((now - ev->when) <= 30)
      recent_count++;
  }

  return ai_clampf((float)recent_count * 0.25f, 0.0f, 1.0f);
}

static float ai_attention_score(struct char_data *mob, enum ai_event_type type, struct char_data *actor, const char *text, time_t now)
{
  float salience;
  float novelty;

  if (!mob || !mob->ai_prof)
    return 0.0f;

  salience = ai_event_salience(type, mob->ai_prof->role, text);
  novelty = 1.0f - ai_event_recency(mob, type, actor, now);
  return ai_clampf(salience * novelty * 1.0f, 0.0f, 1.0f);
}

static const char *ai_action_name(enum ai_action_type action)
{
  static const char *names[] = {"SPEAK","SPEAK_WARN","SPEAK_DEFLECT","IGNORE","CALL_HELP","FLEE","OBSERVE","EMOTE_REACT"};
  if (action < 0 || action >= AI_ACTION_COUNT) return "UNKNOWN";
  return names[action];
}

static const char *ai_goal_name(enum ai_goal_type goal)
{
  switch (goal) {
    case AI_GOAL_MAINTAIN_POST: return "MAINTAIN_POST";
    case AI_GOAL_PURSUE_OFFENDER: return "PURSUE_OFFENDER";
    case AI_GOAL_ESCORT: return "ESCORT";
    case AI_GOAL_SELL: return "SELL";
    case AI_GOAL_GREET_TRAVELERS: return "GREET_TRAVELERS";
    case AI_GOAL_MONITOR_SUSPECT: return "MONITOR_SUSPECT";
    case AI_GOAL_SEEK_SAFETY: return "SEEK_SAFETY";
    case AI_GOAL_AMBUSH: return "AMBUSH";
    case AI_GOAL_REGROUP: return "REGROUP";
    case AI_GOAL_RECRUIT: return "RECRUIT";
    case AI_GOAL_IDLE_WANDER: return "IDLE_WANDER";
    default: return "NONE";
  }
}


static const char *ai_reply_goal_name(enum ai_reply_goal goal)
{
  switch (goal) {
    case GOAL_INFORM: return "INFORM";
    case GOAL_DEFLECT: return "DEFLECT";
    case GOAL_WARN: return "WARN";
    case GOAL_CONNECT: return "CONNECT";
    case GOAL_DISMISS: return "DISMISS";
    case GOAL_CLARIFY: return "CLARIFY";
    case GOAL_SERVE: return "SERVE";
    default: return "UNKNOWN";
  }
}
static const char *ai_role_name_local(int role)
{
  switch (role) {
    case ROLE_GUARD: return "GUARD";
    case ROLE_MERCHANT: return "MERCHANT";
    case ROLE_BANDIT: return "BANDIT";
    case ROLE_BEAST: return "BEAST";
    case ROLE_UNDEAD: return "UNDEAD";
    case ROLE_SPIRIT: return "SPIRIT";
    case ROLE_CULTIST: return "CULTIST";
    case ROLE_BOSS: return "COMMANDER";
    case ROLE_CIVILIAN: return "GENERIC";
    default: return "GENERIC";
  }
}

static const char *ai_event_reason_name(enum ai_event_type type)
{
  switch (type) {
    case AI_EVENT_PLAYER_SAY: return "PLAYER_SAY";
    case AI_EVENT_PLAYER_EMOTE: return "PLAYER_EMOTE";
    case AI_EVENT_PLAYER_ENTER: return "ARRIVAL";
    case AI_EVENT_COMBAT_START: return "COMBAT_TAUNT";
    default: return "AMBIENT";
  }
}

static void ai_debug_log(const char *fmt, ...)
{
  va_list args;
  char buf[MAX_STRING_LENGTH];

  if (!ai_debug)
    return;

  va_start(args, fmt);
  vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);
  log("AI_ACTOR: %s", buf);
}

static void ai_dbg_evt(struct char_data *mob, const char *tag, enum ai_event_type type, struct char_data *actor, const char *msg)
{
  int role = (mob && mob->ai_prof) ? mob->ai_prof->role : ROLE_UNKNOWN;
  int style = (mob && mob->ai_prof) ? mob->ai_prof->style : 0;
  room_rnum room = (mob && IN_ROOM(mob) != NOWHERE) ? IN_ROOM(mob) : NOWHERE;
  int room_id = (room != NOWHERE) ? GET_ROOM_VNUM(room) : NOWHERE;

  if (!ai_debug)
    return;

  ai_debug_log("AI_EVT_TRACE tag=%s event=%s mob_vnum=%d mob=%s role=%s style=%d room=%d actor_id=%ld actor=%s msg=%s",
               tag ? tag : "-", ai_event_reason_name(type), mob ? GET_MOB_VNUM(mob) : -1,
               (mob && GET_NAME(mob)) ? GET_NAME(mob) : "(null)", ai_role_name_local(role), style, room_id,
               actor ? GET_IDNUM(actor) : 0L, (actor && GET_NAME(actor)) ? GET_NAME(actor) : "(null)",
               msg ? msg : "");
}

static int ai_actor_ensure_ready(struct char_data *mob)
{
  if (!mob || !IS_NPC(mob) || !MOB_FLAGGED(mob, MOB_AI_ACTOR) || IN_ROOM(mob) == NOWHERE)
    return 0;

  if (!mob->ai_state || !mob->ai_state->brain)
    ai_actor_init(mob);

  if (!mob->ai_state || !mob->ai_prof) {
    if (ai_debug)
      ai_debug_log("AI_READY_FAIL vnum=%d state=%d prof=%d", GET_MOB_VNUM(mob), mob->ai_state ? 1 : 0, mob->ai_prof ? 1 : 0);
    return 0;
  }

  return 1;
}

static void ai_extract_text(char *out, size_t outsz, struct char_data *mob)
{
  const char *parts[4];
  char tmp[MAX_STRING_LENGTH];
  size_t i, j = 0;
  int p;

  if (!out || outsz == 0) return;
  out[0] = '\0';
  if (!mob) return;

  parts[0] = mob->player.name ? mob->player.name : "";
  parts[1] = mob->player.short_descr ? mob->player.short_descr : "";
  parts[2] = mob->player.long_descr ? mob->player.long_descr : "";
  parts[3] = mob->player.description ? mob->player.description : "";

  snprintf(tmp, sizeof(tmp), "%.*s %.*s %.*s %.*s",
           (int)sizeof(tmp) / 4 - 2, parts[0],
           (int)sizeof(tmp) / 4 - 2, parts[1],
           (int)sizeof(tmp) / 4 - 2, parts[2],
           (int)sizeof(tmp) / 4 - 2, parts[3]);

  for (i = 0; tmp[i] != '\0' && j + 1 < outsz; i++) {
    unsigned char c = (unsigned char)tmp[i];
    if (isalnum(c))
      out[j++] = (char)tolower(c);
    else if (j > 0 && out[j - 1] != ' ')
      out[j++] = ' ';
  }
  if (j > 0 && out[j - 1] == ' ')
    j--;
  out[j] = '\0';

  for (p = (int)j - 1; p >= 0; p--) {
    if (out[p] == ' ')
      out[p] = '\0';
    else
      break;
  }
}

static int ai_text_has(const char *hay, const char *needle)
{
  char token[128];
  size_t nlen;
  const char *pos;

  if (!hay || !needle || !*hay || !*needle)
    return FALSE;

  nlen = strlen(needle);
  if (nlen + 3 > sizeof(token))
    return FALSE;

  snprintf(token, sizeof(token), " %s ", needle);
  pos = hay;
  if (!strncmp(hay, needle, nlen) && (hay[nlen] == '\0' || hay[nlen] == ' '))
    return TRUE;

  while ((pos = strstr(pos, token + 1)) != NULL) {
    if ((pos == hay || *(pos - 1) == ' ') &&
        (pos[nlen] == '\0' || pos[nlen] == ' '))
      return TRUE;
    pos++;
  }

  return FALSE;
}

static int ai_role_weight_from_keywords(const char *text, const char *const *words)
{
  int score = 0, i;
  for (i = 0; words[i]; i++) {
    if (ai_text_has(text, words[i]))
      score += 3;
  }
  return score;
}

static struct ai_actor_memory_entry *ai_mem_get_or_create(struct char_data *mob, long idnum)
{
  struct ai_actor_state *st;
  int i, evict = 0;
  int evict_score = 999999;

  if (!mob || !mob->ai_state || idnum <= 0)
    return NULL;

  st = mob->ai_state;

  for (i = 0; i < st->mem_count; i++) {
    if (st->mem[i].idnum == idnum)
      return &st->mem[i];
  }

  if (st->mem_count < AI_MEM_MAX) {
    memset(&st->mem[st->mem_count], 0, sizeof(st->mem[st->mem_count]));
    st->mem[st->mem_count].idnum = idnum;
    st->mem[st->mem_count].attitude = 0;
    st->mem[st->mem_count].last_seen_time = time(0);
    st->mem[st->mem_count].last_interaction_time = time(0);
    st->mem[st->mem_count].last_update = time(0);
    st->mem[st->mem_count].belief_confidence = 0.5f;
    st->mem[st->mem_count].belief_last_room = NOWHERE;
    st->mem[st->mem_count].belief_last_direction = -1;
    st->mem[st->mem_count].belief_updated_at = time(0);
    return &st->mem[st->mem_count++];
  }

  for (i = 0; i < AI_MEM_MAX; i++) {
    int score = abs(st->mem[i].hostility) + abs(st->mem[i].trust) + abs(st->mem[i].fear) + ((st->mem[i].flags != 0) ? 5 : 0);
    if (score < evict_score) {
      evict = i;
      evict_score = score;
    }
  }

  memset(&st->mem[evict], 0, sizeof(st->mem[evict]));
  st->mem[evict].idnum = idnum;
  st->mem[evict].attitude = 0;
  st->mem[evict].last_seen_time = time(0);
  st->mem[evict].last_interaction_time = time(0);
  st->mem[evict].last_update = time(0);
  st->mem[evict].belief_confidence = 0.5f;
  st->mem[evict].belief_last_room = NOWHERE;
  st->mem[evict].belief_last_direction = -1;
  st->mem[evict].belief_updated_at = time(0);
  return &st->mem[evict];
}

static int ai_room_crowd_count(room_rnum room)
{
  struct char_data *ch;
  int n = 0;
  if (room == NOWHERE) return 0;
  for (ch = world[room].people; ch; ch = ch->next_in_room)
    n++;
  return n;
}

static int ai_conv_room_has_player(room_rnum room)
{
  struct char_data *ch;

  if (room == NOWHERE)
    return FALSE;

  for (ch = world[room].people; ch; ch = ch->next_in_room) {
    if (!IS_NPC(ch))
      return TRUE;
  }
  return FALSE;
}

static struct ai_conv_actor_state *ai_conv_actor_state_get(struct char_data *mob, int create)
{
  int i;
  int oldest = 0;

  if (!mob)
    return NULL;

  for (i = 0; i < AI_CONV_ACTOR_STATE_MAX; i++) {
    if (ai_conv_actor_states[i].mob == mob)
      return &ai_conv_actor_states[i];
  }

  if (!create)
    return NULL;

  for (i = 0; i < AI_CONV_ACTOR_STATE_MAX; i++) {
    if (!ai_conv_actor_states[i].mob) {
      memset(&ai_conv_actor_states[i], 0, sizeof(ai_conv_actor_states[i]));
      ai_conv_actor_states[i].mob = mob;
      return &ai_conv_actor_states[i];
    }
    if (ai_conv_actor_states[i].updated_at < ai_conv_actor_states[oldest].updated_at)
      oldest = i;
  }

  memset(&ai_conv_actor_states[oldest], 0, sizeof(ai_conv_actor_states[oldest]));
  ai_conv_actor_states[oldest].mob = mob;
  return &ai_conv_actor_states[oldest];
}

static struct ai_conv_room_state *ai_conv_room_state_get(room_rnum room, int create)
{
  static int initialized = FALSE;
  int i;
  int oldest = 0;

  if (!initialized) {
    for (i = 0; i < AI_CONV_ROOM_STATE_MAX; i++)
      ai_conv_room_states[i].room = NOWHERE;
    initialized = TRUE;
  }

  if (room == NOWHERE)
    return NULL;

  for (i = 0; i < AI_CONV_ROOM_STATE_MAX; i++) {
    if (ai_conv_room_states[i].room == room)
      return &ai_conv_room_states[i];
  }

  if (!create)
    return NULL;

  for (i = 0; i < AI_CONV_ROOM_STATE_MAX; i++) {
    if (ai_conv_room_states[i].room == NOWHERE) {
      memset(&ai_conv_room_states[i], 0, sizeof(ai_conv_room_states[i]));
      ai_conv_room_states[i].room = room;
      return &ai_conv_room_states[i];
    }
    if (ai_conv_room_states[i].last_line_time < ai_conv_room_states[oldest].last_line_time)
      oldest = i;
  }

  memset(&ai_conv_room_states[oldest], 0, sizeof(ai_conv_room_states[oldest]));
  ai_conv_room_states[oldest].room = room;
  return &ai_conv_room_states[oldest];
}

static void ai_conv_actor_reset(struct char_data *mob, time_t now)
{
  struct ai_conv_actor_state *st = ai_conv_actor_state_get(mob, 0);

  if (!st)
    return;

  st->current_topic = AI_CONV_TOPIC_UNKNOWN;
  st->partner_id = 0;
  st->last_speaker_id = 0;
  st->last_line_time = 0;
  st->topic_expires_at = 0;
  st->depth_counter = 0;
  st->updated_at = now;
}

static void ai_conv_room_end(struct ai_conv_room_state *room_st, time_t now)
{
  if (!room_st)
    return;

  if (room_st->speaker_a)
    ai_conv_actor_reset(room_st->speaker_a, now);
  if (room_st->speaker_b)
    ai_conv_actor_reset(room_st->speaker_b, now);

  room_st->speaker_a = NULL;
  room_st->speaker_b = NULL;
  room_st->topic = AI_CONV_TOPIC_UNKNOWN;
  room_st->active = FALSE;
  room_st->line_count = 0;
  room_st->last_speaker_id = 0;
  room_st->topic_expires_at = 0;
}

static enum ai_time_bucket ai_time_bucket_now(void)
{
  if (time_info.hours >= 6 && time_info.hours <= 8)
    return AI_TIME_DAWN;
  if (time_info.hours >= 9 && time_info.hours <= 17)
    return AI_TIME_DAY;
  if (time_info.hours >= 18 && time_info.hours <= 20)
    return AI_TIME_DUSK;
  return AI_TIME_NIGHT;
}

static const char *ai_time_bucket_name(enum ai_time_bucket b)
{
  switch (b) {
    case AI_TIME_DAY: return "day";
    case AI_TIME_DUSK: return "dusk";
    case AI_TIME_NIGHT: return "night";
    case AI_TIME_DAWN: return "dawn";
    default: return "day";
  }
}

static const char *ai_arch_name(enum ai_player_archetype a)
{
  switch (a) {
    case AI_ARCH_TROUBLEMAKER: return "trouble";
    case AI_ARCH_ERRATIC: return "erratic";
    case AI_ARCH_TRANSACTOR: return "transact";
    case AI_ARCH_EXPLORER: return "explore";
    case AI_ARCH_SOCIALIZER: return "social";
    case AI_ARCH_SILENT: return "silent";
    default: return "unknown";
  }
}

static const char *ai_arc_name(enum ai_conv_arc_state a)
{
  switch (a) {
    case AI_ARC_STRANGER: return "stranger";
    case AI_ARC_ACKNOWLEDGED: return "ack";
    case AI_ARC_ENGAGED: return "engaged";
    case AI_ARC_RAPPORT: return "rapport";
    case AI_ARC_COLD: return "cold";
    default: return "stranger";
  }
}

static int ai_is_hostile_intent(int intent)
{
  return intent == AI_INTENT_THREAT || intent == AI_INTENT_INSULT || intent == AI_INTENT_EMOTE_SPIT;
}

static int ai_is_social_intent(int intent)
{
  return intent == AI_INTENT_GREET || intent == AI_INTENT_SMALLTALK || intent == AI_INTENT_PRAISE || intent >= AI_INTENT_EMOTE_DANCE;
}

static int ai_is_transact_intent(int intent)
{
  return intent == AI_INTENT_ASK_SERVICE || intent == AI_INTENT_BUY_ARMOR || intent == AI_INTENT_BUY_WEAPON || intent == AI_INTENT_BUY_FOOD || intent == AI_INTENT_BANK || intent == AI_INTENT_INN;
}

static int ai_is_story_intent(int intent)
{
  return intent == AI_INTENT_RUMOR || intent == AI_INTENT_QUEST || intent == AI_INTENT_CONFUSION;
}

static int ai_is_question_intent(int intent)
{
  return intent == AI_INTENT_DIRECTIONS || intent == AI_INTENT_BANK || intent == AI_INTENT_INN || intent == AI_INTENT_HEAL || intent == AI_INTENT_QUEST || intent == AI_INTENT_ASK_SERVICE || intent == AI_INTENT_RUMOR || intent == AI_INTENT_CONFUSION;
}

static void ai_context_vector_build(struct char_data *mob, struct char_data *actor, time_t now, struct ai_context_vector *out)
{
  struct char_data *it;
  int d;
  int zone;

  if (!mob || IN_ROOM(mob) == NOWHERE || !out)
    return;

  memset(out, 0, sizeof(*out));
  zone = world[IN_ROOM(mob)].zone;
  out->safe_room = ROOM_FLAGGED(IN_ROOM(mob), ROOM_PEACEFUL) || ROOM_FLAGGED(IN_ROOM(mob), ROOM_NOMOB);
  out->lawful_or_city = (SECT(IN_ROOM(mob)) == SECT_CITY) || ROOM_FLAGGED(IN_ROOM(mob), ROOM_PEACEFUL);
  out->dark = ROOM_FLAGGED(IN_ROOM(mob), ROOM_DARK);
  out->indoors = ROOM_FLAGGED(IN_ROOM(mob), ROOM_INDOORS);
  out->time_bucket = ai_time_bucket_now();
  out->zone_danger = ai_heatmap_danger(zone, now);
  out->zone_alert = ai_alert_level(zone, now);

  for (d = 0; d < NUM_OF_DIRS; d++)
    if (CAN_GO(mob, d))
      out->open_exit_count++;

  for (it = world[IN_ROOM(mob)].people; it; it = it->next_in_room) {
    if (!CAN_SEE(mob, it))
      continue;
    if (IS_NPC(it)) {
      out->visible_mob_count++;
      if (it->ai_prof && it->ai_prof->role == ROLE_GUARD)
        out->guard_present = TRUE;
      if (it->ai_prof && (it->ai_prof->role == ROLE_GUARD || it->ai_prof->role == ROLE_BOSS))
        out->authority_present = TRUE;
    } else
      out->visible_pc_count++;
  }

  if (actor)
    out->alignment_delta = ai_clampf((float)abs(GET_ALIGNMENT(mob) - GET_ALIGNMENT(actor)) / 1000.0f, 0.0f, 1.0f);
}

static struct ai_session_read_entry *ai_session_read_get(struct ai_conv_actor_state *st, long player_id, int create, time_t now)
{
  int i, oldest = 0;
  if (!st || player_id <= 0)
    return NULL;
  for (i = 0; i < AI_SESSION_READ_MAX; i++)
    if (st->session_reads[i].player_idnum == player_id)
      return &st->session_reads[i];
  if (!create)
    return NULL;
  for (i = 0; i < AI_SESSION_READ_MAX; i++) {
    if (st->session_reads[i].player_idnum == 0) {
      memset(&st->session_reads[i], 0, sizeof(st->session_reads[i]));
      st->session_reads[i].player_idnum = player_id;
      st->session_reads[i].arc = AI_ARC_STRANGER;
      st->session_reads[i].archetype = AI_ARCH_UNKNOWN;
      st->session_reads[i].last_exchange_time = now;
      st->session_reads[i].last_speech_act = AI_INTENT_NONE;
      return &st->session_reads[i];
    }
    if (st->session_reads[i].last_exchange_time < st->session_reads[oldest].last_exchange_time)
      oldest = i;
  }
  memset(&st->session_reads[oldest], 0, sizeof(st->session_reads[oldest]));
  st->session_reads[oldest].player_idnum = player_id;
  st->session_reads[oldest].arc = AI_ARC_STRANGER;
  st->session_reads[oldest].archetype = AI_ARCH_UNKNOWN;
  st->session_reads[oldest].last_exchange_time = now;
  st->session_reads[oldest].last_speech_act = AI_INTENT_NONE;
  return &st->session_reads[oldest];
}

static void ai_session_read_update(struct ai_session_read_entry *sr, enum ai_event_type type, int intent, const char *normalized, time_t now)
{
  float denom;
  if (!sr)
    return;
  sr->exchange_count++;
  sr->last_exchange_time = now;
  if (type == AI_EVENT_PLAYER_SAY)
    sr->speech_count++;
  if (type == AI_EVENT_PLAYER_EMOTE)
    sr->emote_count++;
  if (intent == AI_INTENT_GREET || intent == AI_INTENT_PRAISE)
    sr->greet_count++;
  if (ai_is_question_intent(intent) || (normalized && strchr(normalized, '?')))
    sr->question_count++;
  if (ai_is_hostile_intent(intent))
    sr->hostile_count++;
  if (ai_is_social_intent(intent))
    sr->social_count++;
  if (ai_is_transact_intent(intent))
    sr->transact_count++;
  if (ai_is_story_intent(intent))
    sr->story_count++;
  if (sr->last_speech_act != AI_INTENT_NONE && sr->last_speech_act != intent)
    sr->topic_jump_count++;
  sr->last_speech_act = intent;

  if (!sr->first_impression_set) {
    float imp = 0.0f;
    if (intent == AI_INTENT_GREET || intent == AI_INTENT_PRAISE || (type == AI_EVENT_PLAYER_EMOTE && !ai_is_hostile_intent(intent))) imp = 0.45f;
    else if (ai_is_question_intent(intent)) imp = 0.05f;
    else if (ai_is_hostile_intent(intent)) imp = -0.55f;
    sr->first_impression = ai_clampf(imp, -1.0f, 1.0f);
    sr->first_impression_set = TRUE;
  }

  denom = (float)MAX(1, sr->exchange_count);
  sr->aggression = ai_clampf((float)sr->hostile_count / denom, 0.0f, 1.0f);
  sr->curiosity = ai_clampf((float)(sr->question_count + sr->story_count) / (denom * 1.2f), 0.0f, 1.0f);
  sr->social = ai_clampf((float)(sr->social_count + sr->greet_count) / (denom * 1.3f), 0.0f, 1.0f);
  sr->erratic = ai_clampf((float)sr->topic_jump_count / (denom * 0.9f), 0.0f, 1.0f);

  if (sr->exchange_count >= 3 && ((sr->exchange_count == 3) || (sr->exchange_count % 5 == 0))) {
    if (sr->speech_count == 0 && sr->emote_count > 0)
      sr->archetype = AI_ARCH_SILENT;
    else if (sr->aggression > 0.45f)
      sr->archetype = AI_ARCH_TROUBLEMAKER;
    else if (sr->transact_count >= sr->social_count && sr->transact_count >= sr->story_count)
      sr->archetype = AI_ARCH_TRANSACTOR;
    else if (sr->curiosity > 0.55f && sr->story_count >= sr->question_count / 2)
      sr->archetype = AI_ARCH_EXPLORER;
    else if (sr->social > 0.5f)
      sr->archetype = AI_ARCH_SOCIALIZER;
    else if (sr->erratic > 0.55f)
      sr->archetype = AI_ARCH_ERRATIC;
    else
      sr->archetype = AI_ARCH_UNKNOWN;
  }

  if (ai_is_hostile_intent(intent))
    sr->non_hostile_streak = 0;
  else
    sr->non_hostile_streak++;
}

static void ai_session_read_apply_impression(struct ai_session_read_entry *sr, struct ai_actor_memory_entry *e, struct ai_conv_actor_state *conv_st)
{
  float decay;
  if (!sr || !e || !sr->first_impression_set)
    return;
  decay = ai_clampf(1.0f - ((float)MIN(sr->exchange_count, 10) / 10.0f), 0.0f, 1.0f);
  if (sr->first_impression > 0.0f) {
    e->belief_familiarity = ai_clampf(e->belief_familiarity + (0.08f * sr->first_impression * decay), 0.0f, 1.0f);
    if (conv_st)
      conv_st->mood_target = ai_clampf(conv_st->mood_target + (0.06f * sr->first_impression * decay), -1.0f, 1.0f);
  } else if (sr->first_impression < 0.0f) {
    e->belief_hostility = ai_clampf(e->belief_hostility + (0.10f * -sr->first_impression * decay), 0.0f, 1.0f);
  }
}

static void ai_session_read_update_arc(struct ai_session_read_entry *sr, struct char_data *mob, const struct ai_context_vector *ctx)
{
  int rapport_req = 4;
  if (!sr || !mob || !mob->ai_prof)
    return;
  if (mob->ai_prof->role == ROLE_GUARD)
    rapport_req = 6;

  if (ai_is_hostile_intent(sr->last_speech_act) || sr->aggression > 0.7f)
    sr->arc = AI_ARC_COLD;
  else if (sr->arc == AI_ARC_STRANGER && sr->exchange_count > 0)
    sr->arc = AI_ARC_ACKNOWLEDGED;
  else if (sr->arc == AI_ARC_ACKNOWLEDGED && sr->exchange_count >= 2 && sr->aggression < 0.35f && (sr->question_count > 0 || sr->social_count > 0))
    sr->arc = AI_ARC_ENGAGED;
  else if (sr->arc == AI_ARC_ENGAGED && sr->exchange_count >= rapport_req && sr->aggression < 0.25f && (sr->story_count > 0 || sr->social_count > 2) && sr->archetype != AI_ARCH_TRANSACTOR)
    sr->arc = AI_ARC_RAPPORT;
  else if (sr->arc == AI_ARC_COLD && sr->non_hostile_streak >= 3)
    sr->arc = AI_ARC_ACKNOWLEDGED;

  if (mob->ai_prof->role == ROLE_BANDIT) {
    if (sr->arc > AI_ARC_ENGAGED)
      sr->arc = AI_ARC_ENGAGED;
    if (sr->archetype == AI_ARCH_TROUBLEMAKER || sr->archetype == AI_ARCH_TRANSACTOR)
      sr->arc = MIN(sr->arc, AI_ARC_ACKNOWLEDGED);
  }
  if (mob->ai_prof->role == ROLE_CULTIST && !(sr->archetype == AI_ARCH_EXPLORER && sr->curiosity > 0.55f) && sr->arc == AI_ARC_RAPPORT)
    sr->arc = AI_ARC_ENGAGED;
  if (mob->ai_prof->role == ROLE_MERCHANT && ctx && ctx->zone_alert > 0.65f && sr->arc == AI_ARC_RAPPORT)
    sr->arc = AI_ARC_ENGAGED;
}

static void ai_working_mem_push(struct ai_working_mem_entry *mem, int *head, int *count, int *next_id, long player_idnum, const char *text, int intent, int topic_target, int speech_act, int domain, time_t now, int *out_event_id)
{
  int slot;

  if (!mem || !head || !count || !next_id)
    return;

  slot = (*head + *count) % AI_WORKING_MEM_MAX;
  if (*count >= AI_WORKING_MEM_MAX) {
    slot = *head;
    *head = (*head + 1) % AI_WORKING_MEM_MAX;
  } else {
    (*count)++;
  }

  memset(&mem[slot], 0, sizeof(mem[slot]));
  mem[slot].player_idnum = player_idnum;
  snprintf(mem[slot].text, sizeof(mem[slot].text), "%.*s", (int)sizeof(mem[slot].text) - 1, text ? text : "");
  mem[slot].intent = intent;
  mem[slot].topic_target = topic_target;
  mem[slot].speech_act = speech_act;
  mem[slot].domain = domain;
  mem[slot].when = now;
  mem[slot].answered = 0;
  mem[slot].event_id = (*next_id)++;

  if (out_event_id)
    *out_event_id = mem[slot].event_id;
}

static void ai_working_mem_mark_answered(struct ai_working_mem_entry *mem, int count, int head, int event_id)
{
  int i;

  if (!mem || event_id < 0)
    return;

  for (i = 0; i < count; i++) {
    int idx = (head + i) % AI_WORKING_MEM_MAX;
    if (mem[idx].event_id == event_id) {
      mem[idx].answered = 1;
      return;
    }
  }
}

static int ai_intent_family(int intent)
{
  if (intent == AI_INTENT_DIRECTIONS || intent == AI_INTENT_BANK || intent == AI_INTENT_INN || intent == AI_INTENT_HEAL || intent == AI_INTENT_TRAIN)
    return 1;
  if (intent == AI_INTENT_BUY_WEAPON || intent == AI_INTENT_BUY_ARMOR || intent == AI_INTENT_BUY_FOOD || intent == AI_INTENT_ASK_SERVICE)
    return 2;
  if (intent == AI_INTENT_RUMOR || intent == AI_INTENT_QUEST)
    return 3;
  if (intent == AI_INTENT_GREET || intent == AI_INTENT_SMALLTALK)
    return 4;
  return intent;
}

static int ai_overlap_bonus(const char *a, const char *b)
{
  char aw[16][24];
  int an = 0;
  int overlap = 0;
  char buf[256];
  char *tok;
  int i;

  snprintf(buf, sizeof(buf), "%.*s", (int)sizeof(buf) - 1, a ? a : "");
  tok = strtok(buf, " ");
  while (tok && an < 16) {
    if ((int)strlen(tok) >= 4) {
      snprintf(aw[an], sizeof(aw[an]), "%.*s", (int)sizeof(aw[an]) - 1, tok);
      for (i = 0; aw[an][i]; i++)
        aw[an][i] = LOWER(aw[an][i]);
      an++;
    }
    tok = strtok(NULL, " ");
  }

  snprintf(buf, sizeof(buf), "%.*s", (int)sizeof(buf) - 1, b ? b : "");
  tok = strtok(buf, " ");
  while (tok) {
    char lw[24];
    int len = (int)strlen(tok);
    if (len >= 4) {
      snprintf(lw, sizeof(lw), "%.*s", (int)sizeof(lw) - 1, tok);
      for (i = 0; lw[i]; i++)
        lw[i] = LOWER(lw[i]);
      for (i = 0; i < an; i++) {
        if (!strcmp(lw, aw[i])) {
          overlap++;
          break;
        }
      }
    }
    tok = strtok(NULL, " ");
  }

  return (overlap >= 2) ? 8 : 0;
}

static const struct ai_working_mem_entry *ai_relevance_link(const struct ai_working_mem_entry *mem, int count, int head, long player_idnum, const char *current_text, int current_intent, int current_topic, int current_domain, time_t now, int *out_best_score)
{
  int i;
  int best_score = -999;
  int best_idx = -1;
  int answered_best_score = -999;
  int answered_best_idx = -1;

  if (out_best_score)
    *out_best_score = -999;
  if (!mem || count <= 0)
    return NULL;

  for (i = 0; i < count; i++) {
    int idx = (head + i) % AI_WORKING_MEM_MAX;
    const struct ai_working_mem_entry *e = &mem[idx];
    int score = 0;
    int age_pen = (int)MAX(0, (now - e->when) / 30);

    if (player_idnum > 0 && e->player_idnum != player_idnum)
      continue;
    if (current_topic != 0 && e->topic_target == current_topic)
      score += 15;
    if (current_domain != DOMAIN_NONE && e->domain == current_domain)
      score += 10;
    if (ai_intent_family(e->intent) == ai_intent_family(current_intent))
      score += 8;
    if (!e->answered)
      score += 5;
    score -= age_pen;
    score += ai_overlap_bonus(e->text, current_text);

    if (e->answered) {
      if (score > answered_best_score) {
        answered_best_score = score;
        answered_best_idx = idx;
      }
      continue;
    }

    if (score > best_score) {
      best_score = score;
      best_idx = idx;
    }
  }

  if (out_best_score)
    *out_best_score = best_score;
  if (best_idx >= 0 && best_score >= 10)
    return &mem[best_idx];
  if (best_idx < 0 && answered_best_idx >= 0 && answered_best_score >= 10)
    return &mem[answered_best_idx];
  return NULL;
}

static float ai_session_cooldown_penalty(struct ai_session_read_entry *sr, enum ai_action_type action, time_t now)
{
  if (!sr || now >= sr->cooldown_until)
    return 0.0f;
  if (action == AI_ACTION_SPEAK || action == AI_ACTION_SPEAK_WARN)
    return -1.5f;
  return 0.0f;
}

static float ai_session_arc_action_bias(const struct ai_session_read_entry *sr, enum ai_action_type action)
{
  if (!sr)
    return 0.0f;
  switch (sr->arc) {
    case AI_ARC_STRANGER:
      if (action == AI_ACTION_SPEAK) return -0.15f;
      break;
    case AI_ARC_ACKNOWLEDGED:
      if (action == AI_ACTION_SPEAK) return 0.05f;
      break;
    case AI_ARC_ENGAGED:
      if (action == AI_ACTION_SPEAK) return 0.15f;
      break;
    case AI_ARC_RAPPORT:
      if (action == AI_ACTION_SPEAK) return 0.2f;
      break;
    case AI_ARC_COLD:
      if (action == AI_ACTION_SPEAK_WARN || action == AI_ACTION_SPEAK_DEFLECT) return 0.25f;
      if (action == AI_ACTION_SPEAK) return -0.25f;
      break;
    default: break;
  }
  return 0.0f;
}

static float ai_session_arch_action_bias(const struct ai_session_read_entry *sr, enum ai_action_type action)
{
  if (!sr)
    return 0.0f;
  if (sr->archetype == AI_ARCH_TROUBLEMAKER) {
    if (action == AI_ACTION_OBSERVE || action == AI_ACTION_SPEAK_WARN) return 0.3f;
    if (action == AI_ACTION_SPEAK) return -0.2f;
  } else if (sr->archetype == AI_ARCH_TRANSACTOR) {
    if (action == AI_ACTION_SPEAK) return 0.12f;
    if (action == AI_ACTION_EMOTE_REACT) return -0.15f;
  } else if (sr->archetype == AI_ARCH_EXPLORER) {
    if (action == AI_ACTION_SPEAK) return 0.15f;
  } else if (sr->archetype == AI_ARCH_SOCIALIZER) {
    if (action == AI_ACTION_SPEAK) return 0.18f;
  } else if (sr->archetype == AI_ARCH_ERRATIC) {
    if (action == AI_ACTION_SPEAK_DEFLECT) return 0.22f;
  } else if (sr->archetype == AI_ARCH_SILENT) {
    if (action == AI_ACTION_EMOTE_REACT) return 0.2f;
  }
  return 0.0f;
}

static float ai_context_action_bias(struct char_data *mob, const struct ai_context_vector *ctx, enum ai_action_type action, int intent)
{
  float bias = 0.0f;
  if (!mob || !ctx || !mob->ai_prof)
    return 0.0f;
  if (ctx->safe_room && (action == AI_ACTION_SPEAK_WARN || action == AI_ACTION_CALL_HELP || action == AI_ACTION_FLEE))
    bias -= 0.15f;
  if (ctx->recent_violence && (action == AI_ACTION_SPEAK_WARN || action == AI_ACTION_FLEE))
    bias += 0.22f;
  if (ctx->visible_pc_count >= 4 && mob->ai_prof->role == ROLE_MERCHANT && action == AI_ACTION_SPEAK)
    bias += 0.12f;
  if (ctx->open_exit_count <= 1 && ctx->zone_danger > 0.5f && mob->ai_prof->role == ROLE_CIVILIAN) {
    if (action == AI_ACTION_FLEE || action == AI_ACTION_SPEAK_DEFLECT)
      bias += 0.18f;
  }
  if (ctx->time_bucket == AI_TIME_NIGHT) {
    if (mob->ai_prof->role == ROLE_GUARD && action == AI_ACTION_OBSERVE) bias += 0.18f;
    if (mob->ai_prof->role == ROLE_BANDIT && (action == AI_ACTION_OBSERVE || action == AI_ACTION_SPEAK_WARN)) bias += 0.15f;
  }
  if (ctx->time_bucket == AI_TIME_DAY && mob->ai_prof->role == ROLE_MERCHANT && action == AI_ACTION_SPEAK && ai_is_transact_intent(intent))
    bias += 0.12f;
  return bias;
}

static float ai_context_suspicion_bias(struct char_data *mob, const struct ai_context_vector *ctx)
{
  float b = 0.0f;
  if (!mob || !ctx || !mob->ai_prof)
    return 0.0f;
  if (mob->ai_prof->role != ROLE_GUARD && mob->ai_prof->role != ROLE_CULTIST)
    return 0.0f;
  if (ctx->time_bucket == AI_TIME_NIGHT) b += 0.08f;
  if (!ctx->lawful_or_city) b += 0.06f;
  if (ctx->recent_violence) b += 0.09f;
  b += ctx->alignment_delta * 0.10f;
  return b;
}

static void ai_state_push_event(struct char_data *mob, enum ai_event_type type, struct char_data *actor, const char *text)
{
  struct ai_actor_state *st;
  struct ai_actor_recent_event *ev;
  int idx;

  if (!mob || !mob->ai_state) return;
  st = mob->ai_state;
  idx = (st->event_ring_start + st->event_ring_count) % AI_EVENT_RING_MAX;
  if (st->event_ring_count == AI_EVENT_RING_MAX) {
    st->event_ring_start = (st->event_ring_start + 1) % AI_EVENT_RING_MAX;
    idx = (st->event_ring_start + st->event_ring_count - 1) % AI_EVENT_RING_MAX;
  } else st->event_ring_count++;

  ev = &st->recent_events[idx];
  memset(ev, 0, sizeof(*ev));
  ev->type = type;
  ev->actor_idnum = (actor && !IS_NPC(actor)) ? GET_IDNUM(actor) : 0;
  ev->room_vnum = (IN_ROOM(mob) == NOWHERE) ? NOWHERE : GET_ROOM_VNUM(IN_ROOM(mob));
  ev->when = time(0);
  if (text && *text)
    strlcpy(ev->text, text, sizeof(ev->text));

  st->pending_target_idnum = ev->actor_idnum;
  st->pending_event_type = type;
  st->pending_event_time = ev->when;
  if (ev->text[0]) strlcpy(st->pending_event_text, ev->text, sizeof(st->pending_event_text));
  else st->pending_event_text[0] = '\0';

  if (type == AI_EVENT_PLAYER_EMOTE)
    st->social_spam_count = MIN(8, st->social_spam_count + 1);
}

static int ai_state_cooldown_left_pulses(time_t last, int cd_secs)
{
  int left;
  if (last <= 0 || cd_secs <= 0) return 0;
  left = cd_secs - (int)(time(0) - last);
  if (left <= 0) return 0;
  return left * PASSES_PER_SEC;
}

static void ai_mem_decay(struct char_data *mob, time_t now)
{
  struct ai_actor_state *st;
  int i;

  if (!mob || !mob->ai_state)
    return;

  st = mob->ai_state;

  for (i = 0; i < st->mem_count; i++) {
    struct ai_actor_memory_entry *e = &st->mem[i];
    int steps;

    if (e->idnum <= 0)
      continue;

    steps = (int)((now - e->last_update) / 120);
    if (steps <= 0)
      continue;
    if (steps > 5)
      steps = 5;

    while (steps-- > 0) {
      if (e->hostility > 0) e->hostility--;
      else if (e->hostility < 0) e->hostility++;
      if (e->trust > 0) e->trust--;
      else if (e->trust < 0) e->trust++;
      if (e->fear > 0) e->fear--;
      else if (e->fear < 0) e->fear++;
      if (e->attitude > 0) e->attitude--;
      else if (e->attitude < 0) e->attitude++;
    }

    if (e->hostility == 0 && e->trust == 0 && e->fear == 0 && (now - e->last_update) > 600) {
      memset(e, 0, sizeof(*e));
      continue;
    }
    e->last_update = now;
  }
}

static int ai_mob_has_shop_data(struct char_data *mob)
{
  if (!mob || GET_MOB_RNUM(mob) == NOBODY)
    return FALSE;
  return (mob_index[GET_MOB_RNUM(mob)].func == shop_keeper);
}

static int ai_can_speak_now(struct char_data *mob, time_t now)
{
  struct ai_actor_profile *pf;
  struct ai_actor_state *st;
  int room_num_vnum;

  if (!mob || !mob->ai_prof || !mob->ai_state)
    return FALSE;
  pf = mob->ai_prof;
  st = mob->ai_state;

  room_num_vnum = (IN_ROOM(mob) != NOWHERE) ? GET_ROOM_VNUM(IN_ROOM(mob)) : -1;

  if ((now - st->last_spoke) < pf->talk_cooldown_secs)
    return FALSE;
  if (st->last_room_vnum_spoke == room_num_vnum && (now - st->last_room_spoke) < pf->room_talk_cooldown_secs)
    return FALSE;
  return TRUE;
}

static void ai_say(struct char_data *mob, const char *msg, time_t now)
{
  char saybuf[256];

  if (!mob || !msg || !*msg)
    return;

  if (!strncmp(msg, "$n ", 3)) {
    snprintf(saybuf, sizeof(saybuf), "%.*s", (int)sizeof(saybuf) - 1, msg + 3);
    do_echo(mob, saybuf, 0, SCMD_EMOTE);
  } else {
    snprintf(saybuf, sizeof(saybuf), "%.*s", (int)sizeof(saybuf) - 1, msg);
    do_say(mob, saybuf, 0, 0);
  }

#if AI_ACTOR_DEBUG_SPEECH
  log("AI_ACTOR_SPEECH: vnum=%d name=%s role=%s pool=%s reason=%s target=%ld",
      GET_MOB_VNUM(mob), GET_NAME(mob), ai_role_name_local(mob->ai_prof ? mob->ai_prof->role : ROLE_UNKNOWN),
      (mob->ai_state && mob->ai_state->last_pool_name[0]) ? mob->ai_state->last_pool_name : "POOL_NONE",
      (mob->ai_state && mob->ai_state->last_speak_reason[0]) ? mob->ai_state->last_speak_reason : "AMBIENT",
      (mob->ai_state ? mob->ai_state->pending_speech_target_idnum : 0));
#endif

  if (mob->ai_state) {
    mob->ai_state->last_spoke = now;
    mob->ai_state->last_talk_time = now;
    mob->ai_state->last_action_time = now;
    mob->ai_state->talk_cooldown_pulses = ai_state_cooldown_left_pulses(now, rand_number(AI_TALK_COOLDOWN_MIN, AI_TALK_COOLDOWN_MAX));
    mob->ai_state->intent_cooldown_pulses = ai_state_cooldown_left_pulses(now, rand_number(AI_INTENT_COOLDOWN_MIN, AI_INTENT_COOLDOWN_MAX));
    mob->ai_state->last_room_spoke = now;
    mob->ai_state->last_room_vnum_spoke = (IN_ROOM(mob) != NOWHERE) ? GET_ROOM_VNUM(IN_ROOM(mob)) : -1;
  }
}

void ai_actor_schedule_reaction_speech(struct char_data *mob, struct char_data *target, const char *msg)
{
  struct ai_actor_state *st;

  if (!mob || !msg || !*msg || !mob->ai_state)
    return;

  st = mob->ai_state;
  snprintf(st->pending_speech, sizeof(st->pending_speech), "%.*s", (int)sizeof(st->pending_speech) - 1, msg);
  st->pending_speech_target_idnum =
      (target && !IS_NPC(target)) ? GET_IDNUM(target) : 0;
  st->pending_speech_fire_pulse = pulse + 1;
}

static int ai_try_emit_pending_reaction_speech(struct char_data *mob, time_t now)
{
  struct ai_actor_state *st;
  struct char_data *target = NULL;

  if (!mob || !mob->ai_state)
    return FALSE;

  st = mob->ai_state;

  if (!st->pending_speech[0] || st->pending_speech_fire_pulse == 0)
    return FALSE;

  if (pulse < st->pending_speech_fire_pulse)
    return FALSE;

  if (IN_ROOM(mob) == NOWHERE) {
    st->pending_speech[0] = '\0';
    st->pending_speech_target_idnum = 0;
    st->pending_speech_fire_pulse = 0;
    return FALSE;
  }

  if (st->pending_speech_target_idnum > 0) {
    target = ai_find_player_by_idnum_room(mob, st->pending_speech_target_idnum);
    if (!target) {
      st->pending_speech[0] = '\0';
      st->pending_speech_target_idnum = 0;
      st->pending_speech_fire_pulse = 0;
      return FALSE;
    }
  }

  if (ai_can_speak_now(mob, now))
    ai_say(mob, st->pending_speech, now);

  st->pending_speech[0] = '\0';
  st->pending_speech_target_idnum = 0;
  st->pending_speech_fire_pulse = 0;
  return TRUE;
}

static int ai_within_radius_home(struct char_data *mob, room_rnum room, int max_depth)
{
  room_rnum q_room[256];
  int q_depth[256];
  int head = 0, tail = 0, visited = 0;
  int mark = ROOM_BFS_MARK;
  room_rnum home;
  int i;

  if (!mob || !mob->ai_prof || room == NOWHERE)
    return TRUE;
  home = real_room(mob->ai_prof->home_room_vnum);
  if (home == NOWHERE)
    return TRUE;
  if (home == room)
    return TRUE;

  q_room[tail] = home;
  q_depth[tail++] = 0;
  SET_BIT_AR(ROOM_FLAGS(home), mark);

  while (head < tail && tail < 255) {
    room_rnum cur = q_room[head];
    int depth = q_depth[head++];
    int dir;

    if (depth >= max_depth)
      continue;

    for (dir = 0; dir < DIR_COUNT; dir++) {
      room_rnum to;
      if (!world[cur].dir_option[dir])
        continue;
      to = world[cur].dir_option[dir]->to_room;
      if (to == NOWHERE)
        continue;
      if (ROOM_FLAGGED(to, ROOM_NOMOB) || ROOM_FLAGGED(to, ROOM_DEATH))
        continue;
      if (IS_SET_AR(ROOM_FLAGS(to), mark))
        continue;
      SET_BIT_AR(ROOM_FLAGS(to), mark);
      visited++;
      if (to == room) {
        for (i = 0; i <= top_of_world; i++)
          REMOVE_BIT_AR(ROOM_FLAGS(i), mark);
        return TRUE;
      }
      q_room[tail] = to;
      q_depth[tail++] = depth + 1;
      if (visited >= 200)
        break;
    }
  }

  for (i = 0; i <= top_of_world; i++)
    REMOVE_BIT_AR(ROOM_FLAGS(i), mark);

  return FALSE;
}

static int ai_move_random_biased(struct char_data *mob)
{
  int tries;
  int door;

  if (!mob || IN_ROOM(mob) == NOWHERE)
    return FALSE;

  for (tries = 0; tries < 4; tries++) {
    door = rand_number(0, DIR_COUNT - 1);
    if (!CAN_GO(mob, door))
      continue;
    if (ROOM_FLAGGED(EXIT(mob, door)->to_room, ROOM_NOMOB) || ROOM_FLAGGED(EXIT(mob, door)->to_room, ROOM_DEATH))
      continue;
    if (MOB_FLAGGED(mob, MOB_STAY_ZONE) && world[EXIT(mob, door)->to_room].zone != world[IN_ROOM(mob)].zone)
      continue;
    if (mob->ai_prof && mob->ai_prof->roam_radius > 0 &&
        !ai_within_radius_home(mob, EXIT(mob, door)->to_room, mob->ai_prof->roam_radius))
      continue;

    perform_move(mob, door, 1);
    return TRUE;
  }

  return FALSE;
}


static uint32_t ai_fnv1a32_update(uint32_t hash, const char *s)
{
  const unsigned char *p = (const unsigned char *)(s ? s : "");

  while (*p) {
    hash ^= (uint32_t)(*p++);
    hash *= 16777619u;
  }

  return hash;
}

uint32_t ai_actor_compute_signature(struct char_data *mob)
{
  uint32_t hash = 2166136261u;
  char vbuf[32];

  if (!mob)
    return 0;

  snprintf(vbuf, sizeof(vbuf), "%d", GET_MOB_VNUM(mob));
  hash = ai_fnv1a32_update(hash, vbuf);
  hash = ai_fnv1a32_update(hash, "|");
  hash = ai_fnv1a32_update(hash, mob->player.name ? mob->player.name : "");
  hash = ai_fnv1a32_update(hash, "|");
  hash = ai_fnv1a32_update(hash, mob->player.short_descr ? mob->player.short_descr : "");
  hash = ai_fnv1a32_update(hash, "|");
  hash = ai_fnv1a32_update(hash, mob->player.long_descr ? mob->player.long_descr : "");
  hash = ai_fnv1a32_update(hash, "|");
  hash = ai_fnv1a32_update(hash, mob->player.description ? mob->player.description : "");

  return hash;
}

static int ai_role_from_name(const char *value)
{
  if (!value || !*value)
    return ROLE_UNKNOWN;
  if (!str_cmp(value, "guard") || !str_cmp(value, "constable") || !str_cmp(value, "watch")) return ROLE_GUARD;
  if (!str_cmp(value, "merchant") || !str_cmp(value, "innkeeper") || !str_cmp(value, "vendor")) return ROLE_MERCHANT;
  if (!str_cmp(value, "bandit") || !str_cmp(value, "raider")) return ROLE_BANDIT;
  if (!str_cmp(value, "beast") || !str_cmp(value, "animal")) return ROLE_BEAST;
  if (!str_cmp(value, "undead")) return ROLE_UNDEAD;
  if (!str_cmp(value, "spirit") || !str_cmp(value, "ghost")) return ROLE_SPIRIT;
  if (!str_cmp(value, "cultist")) return ROLE_CULTIST;
  if (!str_cmp(value, "commander") || !str_cmp(value, "boss")) return ROLE_BOSS;
  if (!str_cmp(value, "civilian")) return ROLE_CIVILIAN;
  return ROLE_UNKNOWN;
}

static int ai_temperament_from_name(const char *value)
{
  if (!value || !*value)
    return AGG_RETALIATE;
  if (!str_cmp(value, "calm")) return AGG_PEACEFUL;
  if (!str_cmp(value, "neutral")) return AGG_RETALIATE;
  if (!str_cmp(value, "aggressive")) return AGG_OPPORTUNISTIC;
  if (!str_cmp(value, "cowardly")) return AGG_TERRITORIAL;
  return AGG_RETALIATE;
}

static int ai_morale_from_temperament(const char *value)
{
  if (!value || !*value)
    return MORALE_NORMAL;
  if (!str_cmp(value, "cowardly"))
    return MORALE_COWARD;
  if (!str_cmp(value, "aggressive"))
    return MORALE_BRAVE;
  return MORALE_NORMAL;
}

static void ai_parse_override_tag(const char *tag, char *key, size_t keysz, char *val, size_t valsz)
{
  const char *eq;

  if (!tag || !*tag) {
    key[0] = '\0';
    val[0] = '\0';
    return;
  }

  eq = strchr(tag, '=');
  if (!eq) {
    key[0] = '\0';
    val[0] = '\0';
    return;
  }

  snprintf(key, keysz, "%.*s", (int)(eq - tag), tag);
  snprintf(val, valsz, "%s", eq + 1);
}

static void ai_extract_description_without_tags(const char *src, char *dst, size_t dstsz)
{
  size_t i = 0, j = 0;
  int in_tag = FALSE;

  if (!dst || dstsz == 0)
    return;
  dst[0] = '\0';
  if (!src)
    return;

  while (src[i] && j + 1 < dstsz) {
    if (!in_tag && src[i] == '[' && !strncasecmp(src + i, "[AI_", 4)) {
      in_tag = TRUE;
      i++;
      continue;
    }
    if (in_tag) {
      if (src[i] == ']')
        in_tag = FALSE;
      i++;
      continue;
    }
    dst[j++] = src[i++];
  }
  dst[j] = '\0';
}

static void ai_apply_overrides_from_description(const char *desc, struct ai_actor_profile *pf)
{
  const char *d;
  const char *p;
  char token[128], key[64], val[64];

  if (!pf || !desc)
    return;

  d = desc;
  p = d;
  while ((p = strstr(p, "[AI_")) != NULL) {
    const char *end = strchr(p, ']');
    int role;
    if (!end)
      break;
    snprintf(token, sizeof(token), "%.*s", (int)(end - (p + 1)), p + 1);
    ai_parse_override_tag(token, key, sizeof(key), val, sizeof(val));

    if (!str_cmp(key, "AI_ROLE")) {
      role = ai_role_from_name(val);
      if (role != ROLE_UNKNOWN)
        pf->role = role;
    } else if (!str_cmp(key, "AI_TEMPERAMENT")) {
      pf->aggression = ai_temperament_from_name(val);
      pf->morale = ai_morale_from_temperament(val);
    } else if (!str_cmp(key, "AI_MODE")) {
      pf->mode = MAX(0, MIN(3, atoi(val)));
    } else if (!str_cmp(key, "AI_ROAM")) {
      pf->roam_radius = MAX(0, MIN(10, atoi(val)));
    }

    p = end + 1;
  }
}

static void ai_actor_apply_role_setup(struct char_data *mob, const char *text)
{
  if (!mob || !mob->ai_prof)
    return;

  switch (mob->ai_prof->role) {
    case ROLE_GUARD:
      mob->ai_prof->movement = MOB_FLAGGED(mob, MOB_SENTINEL) ? MOVE_SENTINEL : MOVE_PATROL;
      mob->ai_prof->aggression = AGG_CRIME_HUNTER;
      mob->ai_prof->arrest_enabled = TRUE;
      mob->ai_prof->assist_enabled = TRUE;
      mob->ai_prof->call_help_enabled = TRUE;
      mob->ai_prof->roam_radius = 3;
      mob->ai_prof->flee_hp_percent = 10;
      break;
    case ROLE_MERCHANT:
      mob->ai_prof->movement = MOB_FLAGGED(mob, MOB_SENTINEL) ? MOVE_SENTINEL : MOVE_ROAM_INTEREST;
      mob->ai_prof->aggression = AGG_PEACEFUL;
      mob->ai_prof->trade_enabled = TRUE;
      mob->ai_prof->call_help_enabled = TRUE;
      mob->ai_prof->social = SOC_TALKATIVE;
      mob->ai_prof->roam_radius = 2;
      mob->ai_prof->style = (ai_text_has(text, "inn") || ai_text_has(text, "ale") || ai_text_has(text, "tavern")) ? 1 : 0;
      break;
    case ROLE_BANDIT:
      mob->ai_prof->movement = MOVE_WANDER_RADIUS;
      mob->ai_prof->aggression = ai_text_has(text, "ambush") ? AGG_AMBUSH : AGG_OPPORTUNISTIC;
      mob->ai_prof->social = SOC_EXTORT;
      mob->ai_prof->hunt_enabled = TRUE;
      mob->ai_prof->roam_radius = 5;
      mob->ai_prof->flee_hp_percent = 25;
      mob->ai_prof->opportunistic_pref = AI_OPP_PREF_ALONE | AI_OPP_PREF_WOUNDED;
      break;
    case ROLE_BEAST:
      mob->ai_prof->movement = MOVE_WANDER_RADIUS;
      mob->ai_prof->aggression = AGG_TERRITORIAL;
      mob->ai_prof->social = SOC_SILENT;
      mob->ai_prof->roam_radius = 4;
      mob->ai_prof->flee_hp_percent = 15;
      break;
    case ROLE_UNDEAD:
      mob->ai_prof->movement = MOVE_WANDER_RADIUS;
      mob->ai_prof->aggression = AGG_TERRITORIAL;
      mob->ai_prof->morale = MORALE_BRAVE;
      mob->ai_prof->flee_hp_percent = 0;
      mob->ai_prof->social = SOC_SILENT;
      break;
    case ROLE_SPIRIT:
      mob->ai_prof->movement = MOVE_ROAM_INTEREST;
      mob->ai_prof->aggression = AGG_RETALIATE;
      mob->ai_prof->whisper_enabled = TRUE;
      mob->ai_prof->hunt_enabled = FALSE;
      mob->ai_prof->roam_radius = 3;
      break;
    case ROLE_CULTIST:
      mob->ai_prof->movement = MOVE_PATROL;
      mob->ai_prof->aggression = AGG_OPPORTUNISTIC;
      mob->ai_prof->call_help_enabled = TRUE;
      mob->ai_prof->hunt_enabled = TRUE;
      break;
    case ROLE_BOSS:
      mob->ai_prof->movement = MOB_FLAGGED(mob, MOB_SENTINEL) ? MOVE_SENTINEL : MOVE_PATROL;
      mob->ai_prof->aggression = AGG_RETALIATE;
      mob->ai_prof->morale = MORALE_BRAVE;
      mob->ai_prof->flee_hp_percent = 0;
      mob->ai_prof->social = SOC_WARNING;
      break;
    default:
      break;
  }
}

void ai_actor_refresh_profile(struct char_data *mob, int force)
{
  /*
   * AI role keyword table:
   * guard: guard captain watch patrol sentry warden sheriff constable knight paladin
   * merchant: merchant shop shopkeeper vendor trader peddler innkeeper bartender banker inn tavern ale room rooms wares
   * bandit: bandit thief brigand outlaw cutpurse pirate raider mugger highwayman assassin
   * beast: wolf bear boar spider rat beast serpent drake lion tiger bat hound panther
   * undead: skeleton zombie wight lich undead ghoul revenant corpse vampire
   * spirit: spirit ghost wraith apparition ethereal phantom shade whisper haunting
   * cultist: cult acolyte zealot fanatic heretic summoner devotee ritual
   * boss/commander: king queen lord commander champion ancient elder arch high dread captain marshal
   */
  static const char *const guard_kw[] = { "guard", "captain", "watch", "patrol", "sentry", "warden", "sheriff", "constable", "knight", "paladin", NULL };
  static const char *const merchant_kw[] = { "merchant", "shop", "shopkeeper", "vendor", "trader", "peddler", "innkeeper", "bartender", "banker", "inn", "tavern", "ale", "room", "rooms", "wares", NULL };
  static const char *const bandit_kw[] = { "bandit", "thief", "brigand", "outlaw", "cutpurse", "pirate", "raider", "mugger", "highwayman", "assassin", NULL };
  static const char *const beast_kw[] = { "wolf", "bear", "boar", "spider", "rat", "beast", "serpent", "drake", "lion", "tiger", "bat", "hound", "panther", NULL };
  static const char *const undead_kw[] = { "skeleton", "zombie", "wight", "lich", "undead", "ghoul", "revenant", "corpse", "vampire", NULL };
  static const char *const spirit_kw[] = { "spirit", "ghost", "wraith", "apparition", "ethereal", "phantom", "shade", "whisper", "haunting", NULL };
  static const char *const cult_kw[] = { "cult", "acolyte", "zealot", "fanatic", "heretic", "summoner", "devotee", "ritual", NULL };
  static const char *const boss_kw[] = { "king", "queen", "lord", "commander", "champion", "ancient", "elder", "arch", "high", "dread", "marshal", NULL };
  int score[ROLE_BOSS + 1];
  char text[MAX_STRING_LENGTH];
  int best_role = ROLE_UNKNOWN;
  int best_score = -9999;
  int i;
  int zone_lvl = 0;
  char clean_desc[MAX_STRING_LENGTH];
  char raw_desc[MAX_STRING_LENGTH];

  if (!mob || !IS_NPC(mob) || !MOB_FLAGGED(mob, MOB_AI_ACTOR))
    return;

  if (!mob->ai_prof)
    CREATE(mob->ai_prof, struct ai_actor_profile, 1);
  if (!mob->ai_state)
    CREATE(mob->ai_state, struct ai_actor_state, 1);
  if (!mob->ai_prof || !mob->ai_state)
    return;

  if (!force && mob->ai_prof->initialized)
    return;

  memset(mob->ai_prof, 0, sizeof(*mob->ai_prof));
  ai_actor_brain_free(mob);
  memset(mob->ai_state, 0, sizeof(*mob->ai_state));
  ai_actor_brain_init(mob);

  raw_desc[0] = '\0';
  if (mob->player.description)
    snprintf(raw_desc, sizeof(raw_desc), "%s", mob->player.description);

  if (mob->player.description) {
    ai_extract_description_without_tags(mob->player.description, clean_desc, sizeof(clean_desc));
    if (strcmp(clean_desc, mob->player.description)) {
      free(mob->player.description);
      mob->player.description = strdup(clean_desc);
    }
  }

  mob->ai_prof->role = ROLE_UNKNOWN;
  mob->ai_prof->mode = 1;
  mob->ai_prof->movement = MOVE_WANDER_RADIUS;
  mob->ai_prof->aggression = AGG_RETALIATE;
  mob->ai_prof->social = SOC_WARNING;
  mob->ai_prof->morale = MORALE_NORMAL;
  mob->ai_prof->home_room_vnum = (IN_ROOM(mob) == NOWHERE) ? NOWHERE : GET_ROOM_VNUM(IN_ROOM(mob));
  mob->ai_prof->roam_radius = 3;
  mob->ai_prof->talk_cooldown_secs = 16;
  mob->ai_prof->room_talk_cooldown_secs = 28;
  mob->ai_prof->flee_hp_percent = 20;
  mob->ai_prof->surrender_hp_percent = 15;
  mob->ai_state->cached_zone = (IN_ROOM(mob) == NOWHERE) ? NOWHERE : world[IN_ROOM(mob)].zone;
  ai_state_refresh_local_topics(mob);
  mob->ai_state->next_tick = time(0) + rand_number(1, 3);
  mob->ai_state->next_signature_check = 0;

  ai_extract_text(text, sizeof(text), mob);
  memset(score, 0, sizeof(score));

  score[ROLE_GUARD] += ai_role_weight_from_keywords(text, guard_kw);
  score[ROLE_MERCHANT] += ai_role_weight_from_keywords(text, merchant_kw);
  score[ROLE_BANDIT] += ai_role_weight_from_keywords(text, bandit_kw);
  score[ROLE_BEAST] += ai_role_weight_from_keywords(text, beast_kw);
  score[ROLE_UNDEAD] += ai_role_weight_from_keywords(text, undead_kw);
  score[ROLE_SPIRIT] += ai_role_weight_from_keywords(text, spirit_kw);
  score[ROLE_CULTIST] += ai_role_weight_from_keywords(text, cult_kw);
  score[ROLE_BOSS] += ai_role_weight_from_keywords(text, boss_kw);

  /*
   * Role scoring rules (small + reversible):
   * 1) Primary truth = mob text (name/short/long/description) via keyword weights.
   * 2) Secondary truth = mob design flags/spec/shop signals.
   * 3) Tertiary room/zone context can only nudge weak/ambiguous scores.
   * 4) Confidence gate: if (top - second) < AI_ROLE_AMBIGUOUS_MARGIN, use generic role.
   */
  if (MOB_FLAGGED(mob, MOB_AGGRESSIVE)) {
    if (score[ROLE_BEAST] >= score[ROLE_BANDIT])
      score[ROLE_BEAST] += 4;
    else
      score[ROLE_BANDIT] += 4;
  }
  if (MOB_FLAGGED(mob, MOB_SENTINEL))
    score[ROLE_GUARD] += 5;
  if (MOB_FLAGGED(mob, MOB_HELPER))
    score[ROLE_GUARD] += 2;
  if (MOB_FLAGGED(mob, MOB_SCAVENGER)) {
    score[ROLE_BANDIT] += 1;
    score[ROLE_BEAST] += 2;
  }
  if (ai_mob_has_shop_data(mob))
    score[ROLE_MERCHANT] += 10;
  if (mob_index[GET_MOB_RNUM(mob)].func == shop_keeper)
    score[ROLE_MERCHANT] += 6;

  if (IN_ROOM(mob) != NOWHERE) {
    zone_lvl = (zone_table[world[IN_ROOM(mob)].zone].min_level + zone_table[world[IN_ROOM(mob)].zone].max_level) / 2;
    if (zone_lvl >= 80 && best_score <= 6)
      score[ROLE_UNDEAD] += 1;
    if (zone_lvl >= 110 && best_score <= 6)
      score[ROLE_BOSS] += 1;
  }

  {
    int second_score = -9999;
    for (i = ROLE_UNKNOWN; i <= ROLE_BOSS; i++) {
      if (score[i] > best_score) {
        second_score = best_score;
        best_score = score[i];
        best_role = i;
      } else if (score[i] > second_score) {
        second_score = score[i];
      }
      if (mob->ai_state)
        mob->ai_state->role_scores[i] = score[i];
    }

    if (best_score <= 0 || (best_score - second_score) < AI_ROLE_AMBIGUOUS_MARGIN)
      best_role = ROLE_CIVILIAN;
  }

  mob->ai_prof->role = best_role;
  if (best_role == ROLE_UNKNOWN || best_role == ROLE_CIVILIAN) {
    mob->ai_prof->movement = MOB_FLAGGED(mob, MOB_SENTINEL) ? MOVE_SENTINEL : MOVE_PATROL;
    mob->ai_prof->aggression = MOB_FLAGGED(mob, MOB_AGGRESSIVE) ? AGG_OPPORTUNISTIC : AGG_RETALIATE;
    mob->ai_prof->social = SOC_SILENT;
    mob->ai_prof->morale = MORALE_NORMAL;
    mob->ai_prof->style = 0;
    mob->ai_prof->talk_cooldown_secs = 30;
    mob->ai_prof->room_talk_cooldown_secs = 45;
    if (MOB_FLAGGED(mob, MOB_STAY_ZONE) || MOB_FLAGGED(mob, MOB_SENTINEL))
      mob->ai_prof->aggression = AGG_TERRITORIAL;
    if (ai_text_has(text, "fang") || ai_text_has(text, "claw") || ai_text_has(text, "beast")) {
      mob->ai_prof->style = 1;
      if (mob->ai_prof->aggression == AGG_RETALIATE) mob->ai_prof->aggression = AGG_TERRITORIAL;
    }
    if (ai_text_has(text, "ethereal") || ai_text_has(text, "undead") || ai_text_has(text, "wraith"))
      mob->ai_prof->style = 2;
  }
  ai_actor_apply_role_setup(mob, text);

  ai_apply_overrides_from_description(raw_desc, mob->ai_prof);
  ai_actor_apply_role_setup(mob, text);

  mob->ai_prof->mode = MAX(0, MIN(3, mob->ai_prof->mode));
  mob->ai_prof->roam_radius = MAX(0, MIN(10, mob->ai_prof->roam_radius));
  mob->ai_prof->talk_cooldown_secs = MAX(8, MIN(45, mob->ai_prof->talk_cooldown_secs));
  mob->ai_prof->room_talk_cooldown_secs = MAX(12, MIN(60, mob->ai_prof->room_talk_cooldown_secs));

  if (mob->ai_prof->morale == MORALE_COWARD)
    mob->ai_prof->flee_hp_percent = MAX(mob->ai_prof->flee_hp_percent, 35);
  else if (mob->ai_prof->morale == MORALE_BRAVE)
    mob->ai_prof->flee_hp_percent = 0;

  if (mob->ai_prof->role == ROLE_BANDIT)
    mob->ai_prof->flee_hp_percent = MAX(mob->ai_prof->flee_hp_percent, 25);

  mob->ai_prof->surrender_hp_percent = MAX(0, mob->ai_prof->flee_hp_percent - 5);
  if (mob->ai_prof->surrender_hp_percent > mob->ai_prof->flee_hp_percent)
    mob->ai_prof->surrender_hp_percent = mob->ai_prof->flee_hp_percent;

  {
    char matched_keywords[AI_INTENT_KEYWORDS_MAX];
    int r1 = ROLE_CIVILIAN, r2 = ROLE_CIVILIAN, r3 = ROLE_CIVILIAN;
    int rs1 = -999, rs2 = -999, rs3 = -999;

    for (i = ROLE_UNKNOWN; i <= ROLE_BOSS; i++) {
      int v = score[i];
      if (v > rs1) { rs3 = rs2; r3 = r2; rs2 = rs1; r2 = r1; rs1 = v; r1 = i; }
      else if (v > rs2) { rs3 = rs2; r3 = r2; rs2 = v; r2 = i; }
      else if (v > rs3) { rs3 = v; r3 = i; }
    }

    snprintf(matched_keywords, sizeof(matched_keywords),
             "top3=%s=%d %s=%d %s=%d pool=%s reason=%s",
             ai_role_name_local(r1), rs1,
             ai_role_name_local(r2), rs2,
             ai_role_name_local(r3), rs3,
             (mob->ai_state && mob->ai_state->last_pool_name[0]) ? mob->ai_state->last_pool_name : "POOL_NONE",
             (mob->ai_state && mob->ai_state->last_speak_reason[0]) ? mob->ai_state->last_speak_reason : "NONE");
    matched_keywords[sizeof(matched_keywords) - 1] = '\0';
    strlcpy(mob->ai_prof->matched_keywords, matched_keywords, sizeof(mob->ai_prof->matched_keywords));
  }
  mob->ai_prof->profile_flags = 0;
  if ((ai_text_has(text, "fire") || ai_text_has(text, "flame")) && (ai_text_has(text, "ice") || ai_text_has(text, "frost")))
    mob->ai_prof->profile_flags |= AI_PROFILE_INCONSISTENT;

  mob->ai_prof->signature = ai_actor_compute_signature(mob);
  mob->ai_prof->initialized = TRUE;
}

void ai_actor_build_profile(struct char_data *mob, int full_reset)
{
  ai_actor_refresh_profile(mob, full_reset);
}

void ai_actor_rebuild_profile(struct char_data *mob)
{
  int before_role;
  uint32_t before_sig;

  if (!mob || !IS_NPC(mob) || !MOB_FLAGGED(mob, MOB_AI_ACTOR))
    return;

  if (!mob->ai_prof)
    CREATE(mob->ai_prof, struct ai_actor_profile, 1);
  if (!mob->ai_state)
    CREATE(mob->ai_state, struct ai_actor_state, 1);
  if (!mob->ai_prof || !mob->ai_state)
    return;

  before_role = mob->ai_prof->role;
  before_sig = mob->ai_prof->signature;
  ai_actor_refresh_profile(mob, TRUE);
  ai_debug_log("profile refresh vnum=%d sig=%u->%u role=%d->%d", GET_MOB_VNUM(mob),
               (unsigned int)before_sig, (unsigned int)mob->ai_prof->signature,
               before_role, mob->ai_prof->role);
}

void ai_actor_refresh_live_mobs_by_vnum(mob_vnum vnum)
{
  struct char_data *mob;

  for (mob = character_list; mob; mob = mob->next) {
    if (!IS_NPC(mob) || !MOB_FLAGGED(mob, MOB_AI_ACTOR))
      continue;
    if (GET_MOB_VNUM(mob) != vnum)
      continue;
    ai_actor_rebuild_profile(mob);
  }
}

void ai_actor_init(struct char_data *mob)
{
  struct ai_conv_actor_state *conv_st;
  if (!mob || !IS_NPC(mob) || !MOB_FLAGGED(mob, MOB_AI_ACTOR))
    return;

  ai_actor_refresh_profile(mob, TRUE);
  conv_st = ai_conv_actor_state_get(mob, 1);
  if (conv_st) {
    conv_st->mood_current = 0.0f;
    conv_st->mood_target = 0.0f;
    conv_st->mood_velocity = 0.0f;
    conv_st->mood_last_tick = time(0);
    conv_st->goal_count = 0;
    switch (mob->ai_prof->role) {
      case ROLE_GUARD: ai_goal_push(mob, AI_GOAL_MAINTAIN_POST, 0.8f, 60, 0, 0); break;
      case ROLE_MERCHANT:
        if (mob->ai_prof->style == 1) ai_goal_push(mob, AI_GOAL_GREET_TRAVELERS, 0.75f, 10, 0, 0);
        else ai_goal_push(mob, AI_GOAL_SELL, 0.7f, 30, 0, 0);
        break;
      case ROLE_BANDIT: ai_goal_push(mob, AI_GOAL_AMBUSH, 0.7f, 90, 0, 0); break;
      case ROLE_CIVILIAN: ai_goal_push(mob, AI_GOAL_IDLE_WANDER, 0.5f, 15, 0, 0); break;
      case ROLE_CULTIST: ai_goal_push(mob, AI_GOAL_MONITOR_SUSPECT, 0.6f, 20, 0, 0); break;
      default: break;
    }
  }
  ai_debug_log("init %s role=%d move=%d aggr=%d morale=%d", GET_NAME(mob), mob->ai_prof->role,
               mob->ai_prof->movement, mob->ai_prof->aggression, mob->ai_prof->morale);
}

void ai_actor_free(struct char_data *mob)
{
  if (!mob)
    return;
  if (mob->ai_prof) {
    free(mob->ai_prof);
    mob->ai_prof = NULL;
  }
  if (mob->ai_state) {
    ai_actor_brain_free(mob);
    free(mob->ai_state);
    mob->ai_state = NULL;
  }
}


#if 0
static int ai_find_hostile_target_in_room(struct char_data *mob)
{
  struct char_data *vict;

  if (!mob || IN_ROOM(mob) == NOWHERE || !mob->ai_state)
    return 0;

  for (vict = world[IN_ROOM(mob)].people; vict; vict = vict->next_in_room) {
    int i;
    if (IS_NPC(vict) || PRF_FLAGGED(vict, PRF_NOHASSLE) || !CAN_SEE(mob, vict))
      continue;
    for (i = 0; i < mob->ai_state->mem_count; i++) {
      struct ai_actor_memory_entry *e = &mob->ai_state->mem[i];
      if (e->idnum == GET_IDNUM(vict) && (e->flags & MEM_WANTED || e->hostility >= AI_HOSTILE_ATTACK_THRESHOLD)) {
        return GET_IDNUM(vict);
      }
    }
  }
  return 0;
}
#endif

static struct char_data *ai_find_player_by_idnum_room(struct char_data *mob, long idnum)
{
  struct char_data *vict;
  if (!mob || idnum <= 0 || IN_ROOM(mob) == NOWHERE)
    return NULL;
  for (vict = world[IN_ROOM(mob)].people; vict; vict = vict->next_in_room) {
    if (!IS_NPC(vict) && GET_IDNUM(vict) == idnum)
      return vict;
  }
  return NULL;
}

static int ai_try_flee_or_surrender(struct char_data *mob, time_t now)
{
  int hp_pct;

  if (!mob || !mob->ai_prof || !FIGHTING(mob))
    return FALSE;

  if (GET_MAX_HIT(mob) <= 0)
    return FALSE;

  hp_pct = (GET_HIT(mob) * 100) / GET_MAX_HIT(mob);
  if (hp_pct <= mob->ai_prof->flee_hp_percent && mob->ai_prof->flee_hp_percent > 0) {
    int moved = ai_move_random_biased(mob);
    if (moved && FIGHTING(mob))
      stop_fighting(mob);
    if (moved) {
      ai_debug_log("%s flees at %d%% hp", GET_NAME(mob), hp_pct);
      return TRUE;
    }
  }

  if (hp_pct <= mob->ai_prof->surrender_hp_percent && mob->ai_prof->surrender_hp_percent > 0) {
    if (ai_can_speak_now(mob, now))
      ai_say(mob, "$n lowers $s weapon and backs away in surrender.", now);
    if (FIGHTING(mob))
      stop_fighting(mob);
    return TRUE;
  }

  return FALSE;
}

/*
 * Intent scoring rules summary:
 * - Evaluate lightweight context once per tick window.
 * - Score intents using role/temperament bases + recent event boosts + disposition.
 * - Apply hard gates: peaceful rooms, cooldowns, visibility, and special/script ownership.
 * - Execute only the top intent above threshold, then apply intent/talk cooldowns.
 */
static const char *const role_guard_greet[] = {"{GREET}. Keep things lawful on this {PLACE}.", "{GREET}. Keep the {QUIET}.", "Morning. Move smart and stay {QUIET}.", "Eyes open. No trouble today.", "You are safe if you act right.", "Keep your blade sheathed in town.", "Report crimes and keep walking.", "Stay civil and we get along.", "The square is watched. Behave.", "Mind the law and you'll do fine.", "Need directions? Ask plainly.", "Order first, comfort second.", NULL};
static const char *const role_guard_service[] = {"Need a direction? I can point you to the inn, bank, or market.", "For rooms and rest, head to the inn. For trade, market stalls.", "The law office keeps records; the bank is east from here.", "Travelers rest at the inn. Keep your coin close on the road.", "Need help finding a healer? I can point the way.", "If you're lost, follow the main road to the square fountain.", "Merchants trade nearby; keep business clean and legal.", "Ask clearly and I'll give directions, not discounts.", NULL};
static const char *const role_merchant_greet[] = {"{GREET}, traveler. Browse my wares.", "Fresh stock and fair measures this {TIME}.", "Take your {TIME}; prices stay {GOOD}.", "Looking to buy or sell?", "Careful hands, quality goods.", "Coin talks, and I listen.", "Best rates in this quarter.", "See anything you fancy?", "Trade straight, leave happy.", "Step closer and have a look.", "Fine goods, no tricks.", "I can help you outfit your journey.", NULL};
static const char *const role_merchant_service[] = {"I buy and sell. Show me what you carry.", "Need wares? I've got supplies and tools.", "Trade window's open; let's do business.", "If you need kit for the road, I can sort you out.", "Sell loot, buy provisions, move fast.", "My stock rotates often; check the shelves.", "I can price your goods fairly.", "If you seek rest, the inn is across the square.", NULL};
static const char *const role_innkeeper_greet[] = {"{GREET} in. Warm beds and hot stew.", "Evening, friend. Rest and room at this {PLACE}.", "Boots off, worries down, hearth stays {GOOD}.", "Need a quiet room tonight?", "The fire's hot and the ale's fresh.", "Travel's hard; rest here.", "I've got blankets, broth, and a bed.", "Come in out of the weather.", "A calm table and a softer mattress await.", "Sit, breathe, and settle in.", "You're welcome so long as you keep it civil.", "Long road behind you? I've got rest for that.", NULL};
static const char *const role_innkeeper_service[] = {"I can offer a room, a meal, and a place to rest.", "Need an inn room? I can set you up.", "Rest your wounds by the hearth and take a bed upstairs.", "Food's hot, beds are clean, and noise stays low.", "You can rent a room or just sit and recover.", "If you need healing rest, this is your best stop.", "Stay the night and start fresh at dawn.", "No shop haggling here; comfort's what I sell.", NULL};
static const char *const role_bandit_greet[] = {"You're new. Keep coin visible on this {ROAD}.", "The {ROAD}'s {BAD}. Pay attention.", "Nice purse. Shame if {TROUBLE} found it.", "Walk light and don't stare.", "You look like trouble worth weighing.", "Eyes down, pockets up.", "I've seen richer folk go missing.", "Careful where you step, friend.", "This lane charges tolls in silver.", "Keep moving and maybe we smile.", "You breathe easy for someone in my street.", "Hope you can afford local manners.", NULL};
static const char *const role_bandit_service[] = {"I don't sell wares; I tax passage.", "Service? Pay coin and I might answer.", "Inn and bank are for soft hands, not mine.", "You want directions, buy them.", "Trade's for merchants. I deal in leverage.", "Need rest? Don't sleep where I can see you.", "You're asking a lot for free.", "I can help you keep your purse by not taking it.", NULL};
static const char *const role_beast_greet[] = {"$n snorts and watches you warily.", "$n paces in a tense circle.", "$n rumbles a low warning growl.", "$n flicks ears and studies your movement.", "$n stamps once and bares its teeth.", "$n huffs and keeps distance.", "$n lets out a rough bark.", "$n watches your hands, unblinking.", "$n prowls a step closer, then stops.", "$n growls but does not lunge.", "$n shakes its mane and sniffs the air.", "$n tracks you with predator focus.", NULL};
static const char *const role_beast_service[] = {"$n growls, offering no help.", "$n bares fangs; there is no service here.", "$n huffs and ignores your request.", "$n paws the ground in refusal.", "$n answers with a warning snarl.", "$n circles, uninterested in trade.", "$n stares as if you are prey.", "$n snaps the air and turns away.", NULL};
static const char *const role_undead_greet[] = {"$n hisses, voice dry as dust.", "$n's hollow eyes fix on you.", "$n rasps a death-cold greeting.", "$n drifts forward with a graveyard hush.", "$n clicks bone against bone.", "$n whispers from behind dead lips.", "$n exhales a chill moan.", "$n studies you like a future corpse.", "$n sways, then stills.", "$n's jaw cracks in a rotten grin.", "$n croaks in sepulchral tones.", "$n stares without blinking.", NULL};
static const char *const role_undead_service[] = {"$n hisses: no comfort for the living.", "$n rasps: only graves offer rest here.", "$n gives no aid, only cold silence.", "$n mutters of rot instead of trade.", "$n will not guide the breathing.", "$n offers hunger, not healing.", "$n's answer is a funeral whisper.", "$n turns away with a hiss.", NULL};
static const char *const role_spirit_greet[] = {"$n whispers through the air around you.", "$n shimmers and nods faintly.", "$n's voice drifts like wind in glass.", "$n circles you in a pale glow.", "$n murmurs from nowhere and everywhere.", "$n bows with spectral grace.", "$n flickers, then steadies.", "$n hums a thin haunting note.", "$n greets you in a breath-cold whisper.", "$n glides nearby without footsteps.", "$n watches with distant calm.", "$n ripples like moonlight on water.", NULL};
static const char *const role_spirit_service[] = {"$n whispers: I keep no wares, only echoes.", "$n murmurs: rest is for flesh, not fog.", "$n cannot trade coin, only omens.", "$n offers guidance in riddles, not rooms.", "$n says the bank means nothing to the dead.", "$n drifts, refusing worldly service.", "$n whispers directions like a dream.", "$n sighs: seek living hands for living needs.", NULL};
static const char *const role_unknown_idle[] = {"$n watches quietly.", "$n studies the room in silence.", NULL};

static const char *const role_guard_emote[] = {"Keep it orderly.", "Public antics are fine; keep it decent.", "Seen worse. Carry on.", "No laws broken yet.", "You have spirit. Keep control.", "Stay respectful and we're good.", "Enjoy yourself, just keep peace.", "That's enough show for now.", "Move along and stay civil.", "The square isn't a stage, but fine.", "Keep hands to yourself.", "Don't test my patience.", NULL};
static const char *const role_merchant_emote[] = {"Good energy brings good trade.", "A lively crowd helps business.", "Try not to knock the wares.", "If you dance, dance clear of my stall.", "Friendly folk spend well.", "Keep it cheerful, keep it moving.", "No stains on the goods, please.", "You bring attention; I like that.", "A wave and a smile sell more than shouting.", "Spirited crowd today.", "Mind the shelves while you celebrate.", "Thanks for brightening the square.", NULL};
static const char *const role_innkeeper_emote[] = {"Easy now, keep the common room calm.", "Dance if you like, just no broken chairs.", "Warm mood, warm hearth.", "Friendly gestures are welcome here.", "Mind the mugs while you celebrate.", "You're welcome to be merry, not messy.", "A wave to the room goes a long way.", "Hugs are fine, fights are not.", "Keep voices kind and I'll keep serving.", "Joy's good for the house.", "Don't spit in my inn.", "Respect the place and stay as long as you like.", NULL};
static const char *const role_bandit_emote[] = {"Cute. Keep your purse while you perform.", "Dance all you want; I count your coin.", "Wave less, watch more.", "Hugging in this district gets expensive.", "Spit again and pay in blood.", "You entertain. I evaluate.", "Big moves make easy targets.", "I prefer fear to applause.", "You got nerve; maybe too much.", "Keep the show short.", "You trying to impress me?", "One wrong step and I collect.", NULL};
static const char *const role_beast_emote[] = {"$n growls at the motion.", "$n huffs and backs a half-step.", "$n snaps at the air.", "$n's hackles rise.", "$n paws hard at the ground.", "$n watches your gestures with suspicion.", "$n circles and snorts.", "$n emits a warning rumble.", "$n bares teeth briefly.", "$n shakes off with a low growl.", "$n tracks you, tense and alert.", "$n stalks in a tight arc.", NULL};
static const char *const role_undead_emote[] = {"$n hisses at your display.", "$n rattles in contempt.", "$n whispers a curse.", "$n's stare chills the air.", "$n croaks a hollow warning.", "$n's jaw clacks in disapproval.", "$n drifts closer, hostile.", "$n emits a grave-cold moan.", "$n watches with corpse-still malice.", "$n rasps at your insolence.", "$n's fingers twitch like dead roots.", "$n leans in with a hiss.", NULL};
static const char *const role_spirit_emote[] = {"$n whispers around your motion.", "$n flickers in pale interest.", "$n swirls as if in a slow dance.", "$n hums a spectral reply.", "$n drifts back from your gesture.", "$n's glow dims in disapproval.", "$n curls into mist and returns.", "$n answers with a haunted murmur.", "$n shivers through the air.", "$n ripples with soft emotion.", "$n whispers from behind you.", "$n lingers, uncertain.", NULL};

static const char *const emote_dance_guard[] = {"Keep that energy outside the post.", NULL};
static const char *const emote_dance_innkeeper[] = {"Ha. Good to see some life in here.", NULL};
static const char *const emote_dance_merchant[] = {"Good spirits. Good for business.", NULL};
static const char *const emote_dance_bandit[] = {"Draw less attention to yourself.", NULL};
static const char *const emote_dance_commander[] = {"Enough. Keep order in here.", NULL};
static const char *const emote_dance_cultist[] = {"Joy is a distraction from purpose.", NULL};
static const char *const emote_dance_spirit_emote[] = {"$n gives a soft, ethereal laugh.", NULL};

static const char *const emote_affection_innkeeper[] = {"Friendly sort, aren't you.", NULL};
static const char *const emote_affection_guard[] = {"Mind your distance on post.", NULL};
static const char *const emote_affection_bandit[] = {"Watch the hands.", NULL};
static const char *const emote_affection_civilian[] = {"Oh. Well, hello to you too.", NULL};

static const char *const emote_glare_guard[] = {"Something the matter, citizen?", NULL};
static const char *const emote_glare_bandit[] = {"Keep walking.", NULL};
static const char *const emote_glare_spirit[] = {"Your gaze cannot touch what I am.", NULL};
static const char *const emote_glare_commander[] = {"State your grievance or move on.", NULL};

static const char *const role_rare_guard[] = {"I know every alley here. Ask and I'll map your path.", NULL};
static const char *const role_rare_merchant[] = {"For you? A rumor free with every fair trade.", NULL};
static const char *const role_rare_innkeeper[] = {"Old travelers say this hearth blesses honest sleepers.", NULL};
static const char *const role_rare_bandit[] = {"Pay once and I might remember your face kindly.", NULL};
static const char *const role_rare_beast[] = {"$n lets out a strangely melodic growl.", NULL};
static const char *const role_rare_undead[] = {"$n whispers your name as if from a crypt.", NULL};
static const char *const role_rare_spirit[] = {"$n murmurs of doors hidden between moonbeams.", NULL};

static int ai_pool_roll_percent(void)
{
  return rand_number(1, 100);
}

static const char *ai_snapshot_touch_role_pool(struct char_data *mob, int role, const char **out_pool)
{
  const char *picked = NULL;

  if (out_pool)
    *out_pool = "POOL_NONE";
  if (!mob || !mob->ai_state)
    return NULL;
  if (((pulse + GET_MOB_VNUM(mob)) % 23) != 0)
    return NULL;

  switch (role) {
    case ROLE_BANDIT:
      picked = ai_pool_pick(role_bandit_service);
      if (out_pool) *out_pool = "POOL_BANDIT_SERVICE";
      break;
    case ROLE_BEAST:
      if (ai_pool_roll_percent() <= 5) {
        picked = ai_pool_pick(role_rare_beast);
        if (out_pool) *out_pool = "POOL_RARE_BEAST";
      } else {
        picked = ai_pool_pick(role_beast_greet);
        if (out_pool) *out_pool = "POOL_BEAST_GREET";
      }
      (void)ai_pool_pick(role_beast_service);
      break;
    case ROLE_UNDEAD:
      if (ai_pool_roll_percent() <= 5) {
        picked = ai_pool_pick(role_rare_undead);
        if (out_pool) *out_pool = "POOL_RARE_UNDEAD";
      } else {
        picked = ai_pool_pick(role_undead_greet);
        if (out_pool) *out_pool = "POOL_UNDEAD_GREET";
      }
      (void)ai_pool_pick(role_undead_service);
      break;
    case ROLE_SPIRIT:
      if (ai_pool_roll_percent() <= 5) {
        picked = ai_pool_pick(role_rare_spirit);
        if (out_pool) *out_pool = "POOL_RARE_SPIRIT";
      } else {
        picked = ai_pool_pick(role_spirit_greet);
        if (out_pool) *out_pool = "POOL_SPIRIT_GREET";
      }
      (void)ai_pool_pick(role_spirit_service);
      break;
    default:
      break;
  }

  return picked;
}


static void ai_refresh_score_snapshot_text(struct char_data *mob)
{
  int i, r1 = ROLE_CIVILIAN, r2 = ROLE_CIVILIAN, r3 = ROLE_CIVILIAN;
  int rs1 = -999, rs2 = -999, rs3 = -999;
  char buf[AI_INTENT_KEYWORDS_MAX];
  const char *touch_pool = NULL;
  const char *touch_pick = NULL;

  if (!mob || !mob->ai_prof || !mob->ai_state)
    return;

  for (i = ROLE_UNKNOWN; i <= ROLE_BOSS; i++) {
    int v = mob->ai_state->role_scores[i];
    if (v > rs1) { rs3 = rs2; r3 = r2; rs2 = rs1; r2 = r1; rs1 = v; r1 = i; }
    else if (v > rs2) { rs3 = rs2; r3 = r2; rs2 = v; r2 = i; }
    else if (v > rs3) { rs3 = v; r3 = i; }
  }

  snprintf(buf, sizeof(buf), "top3=%s=%d %s=%d %s=%d pool=%s reason=%s",
           ai_role_name_local(r1), rs1,
           ai_role_name_local(r2), rs2,
           ai_role_name_local(r3), rs3,
           mob->ai_state->last_pool_name[0] ? mob->ai_state->last_pool_name : "POOL_NONE",
           mob->ai_state->last_speak_reason[0] ? mob->ai_state->last_speak_reason : "NONE");

  touch_pick = ai_snapshot_touch_role_pool(mob, r1, &touch_pool);
  if (touch_pick && touch_pool && *touch_pool) {
    snprintf(mob->ai_state->last_pool_name, sizeof(mob->ai_state->last_pool_name), "%s", touch_pool);
    snprintf(mob->ai_state->last_speak_reason, sizeof(mob->ai_state->last_speak_reason), "SNAPSHOT_TOUCH");
  }

  strlcpy(mob->ai_prof->matched_keywords, buf, sizeof(mob->ai_prof->matched_keywords));
}

static void ai_set_last_speech_meta(struct char_data *mob, const char *pool, const char *reason)
{
  if (!mob || !mob->ai_state)
    return;
  strlcpy(mob->ai_state->last_pool_name, pool ? pool : "POOL_NONE", sizeof(mob->ai_state->last_pool_name));
  strlcpy(mob->ai_state->last_speak_reason, reason ? reason : "AMBIENT", sizeof(mob->ai_state->last_speak_reason));
  ai_refresh_score_snapshot_text(mob);
}

static unsigned long ai_hash_mix(unsigned long h, unsigned long v)
{
  h ^= v + 0x9e3779b9UL + (h << 6) + (h >> 2);
  return h;
}

static unsigned long ai_hash_text_stable(const char *text)
{
  const unsigned char *p = (const unsigned char *)text;
  unsigned long h = 2166136261UL;

  if (!text)
    return h;

  while (*p) {
    h ^= (unsigned long)(*p++);
    h *= 16777619UL;
  }

  return h;
}


static const struct ai_imtb_profile *ai_imtb_profile_get(int personality)
{
  if (personality < 0 || personality >= IMTB_MAX)
    personality = IMTB_STOIC;
  return &ai_imtb_profiles[personality];
}

static int ai_imtb_pick_personality(struct char_data *mob, int role, int style)
{
  struct ai_conv_actor_state *st;
  char seedbuf[256];
  unsigned long h;
  int p;

  if (!mob)
    return IMTB_STOIC;

  st = ai_conv_actor_state_get(mob, 1);
  if (st && st->personality_ready)
    return st->personality;

  snprintf(seedbuf, sizeof(seedbuf), "%d|%s|%s|%d|%d|%d",
           GET_MOB_VNUM(mob),
           GET_NAME(mob) ? GET_NAME(mob) : "",
           (mob->player.long_descr && *mob->player.long_descr) ? mob->player.long_descr : "",
           role,
           GET_ALIGNMENT(mob),
           style);
  h = ai_hash_text_stable(seedbuf);
  p = (int)(h % (unsigned long)IMTB_MAX);

  if ((role == ROLE_SPIRIT || role == ROLE_UNDEAD) && p != IMTB_EERIE) {
    if ((h % 100UL) < 70UL)
      p = IMTB_EERIE;
  }
  if (role == ROLE_CULTIST) {
    p = ((h % 2UL) == 0UL) ? IMTB_ZEALOUS : IMTB_SUSPICIOUS;
  }
  if (role == ROLE_MERCHANT) {
    if (p == IMTB_EERIE)
      p = ((h % 2UL) == 0UL) ? IMTB_FRIENDLY : IMTB_BOOKISH;
  }

  if (st) {
    st->personality = p;
    st->personality_ready = 1;
  }

  if (ai_debug)
    ai_debug_log("AI_IMTB_ASSIGN vnum=%d role=%s personality=%s", GET_MOB_VNUM(mob), ai_role_name_local(role), ai_imtb_profiles[p].tag);

  return p;
}

static const char *ai_imtb_pick_fragment(const char *const *pool, unsigned long seed)
{
  int n = 0;
  if (!pool)
    return "";
  while (pool[n])
    n++;
  if (n <= 0)
    return "";
  return pool[seed % (unsigned long)n];
}

static const char *ai_imtb_pick_leadin(struct ai_conv_actor_state *st, const struct ai_reply_context *rctx, int role, int style, unsigned long seed, int *out_suppressed)
{
  const char *frag;
  unsigned int h;
  int personality;
  unsigned long mix;
  int multi_topic;
  int spoke_recently;

  if (out_suppressed)
    *out_suppressed = 0;
  if (!rctx)
    return "";

  multi_topic = (rctx->requested_count > 1);
  spoke_recently = (st && st->last_line_time > 0 && (time(0) - st->last_line_time) <= 2);

  if (multi_topic || spoke_recently) {
    if (out_suppressed)
      *out_suppressed = 1;
    return "";
  }

  personality = (rctx->personality >= 0 && rctx->personality < IMTB_MAX) ? rctx->personality : IMTB_STOIC;
  mix = ai_hash_mix(seed, (unsigned long)(personality * 41 + 7));

  if (role == ROLE_GUARD)
    frag = ai_imtb_pick_fragment(ai_role_leadin_guard, ai_hash_mix(mix, 3));
  else if (role == ROLE_MERCHANT && style == 1)
    frag = ai_imtb_pick_fragment(ai_role_leadin_innkeeper, ai_hash_mix(mix, 5));
  else if (role == ROLE_MERCHANT)
    frag = ai_imtb_pick_fragment(ai_role_leadin_merchant, ai_hash_mix(mix, 7));
  else if (role == ROLE_CULTIST)
    frag = ai_imtb_pick_fragment(ai_role_leadin_cultist, ai_hash_mix(mix, 11));
  else if (role == ROLE_SPIRIT)
    frag = ai_imtb_pick_fragment(ai_role_leadin_spirit, ai_hash_mix(mix, 13));
  else if (role == ROLE_BANDIT)
    frag = ai_imtb_pick_fragment(ai_role_leadin_bandit, ai_hash_mix(mix, 17));
  else
    frag = "";

  if (!frag || !*frag)
    frag = ai_imtb_pick_fragment(ai_imtb_leadin[personality], mix);
  if (!frag || !*frag)
    return "";

  h = (unsigned int)ai_hash_text_stable(frag);
  if (st && ai_hash_ring_has(st->recent_lead_hashes, st->recent_lead_count, st->recent_lead_head, 6, h)) {
    frag = ai_imtb_pick_fragment(ai_imtb_leadin[personality], ai_hash_mix(mix, 19));
    if (!frag || !*frag)
      return "";
    h = (unsigned int)ai_hash_text_stable(frag);
    if (ai_hash_ring_has(st->recent_lead_hashes, st->recent_lead_count, st->recent_lead_head, 6, h))
      return "";
  }

  if (!ai_line_is_role_legal(frag, role, style)) {
    frag = ai_imtb_pick_fragment(ai_imtb_leadin[personality], ai_hash_mix(mix, 53));
    if (!frag || !*frag || !ai_line_is_role_legal(frag, role, style)) {
      if (out_suppressed)
        *out_suppressed = 1;
      return "";
    }
  }

  if (st)
    ai_hash_ring_push(st->recent_lead_hashes, &st->recent_lead_head, &st->recent_lead_count, 6, h);

  return frag;
}

static const char *ai_imtb_pick_uncertainty(const struct ai_reply_context *rctx, int role, int style, unsigned long seed, int *out_used)
{
  const char *frag;
  int personality;

  if (out_used)
    *out_used = 0;
  if (!rctx || rctx->confidence > 2)
    return "";

  personality = (rctx->personality >= 0 && rctx->personality < IMTB_MAX) ? rctx->personality : IMTB_STOIC;
  frag = ai_imtb_pick_fragment(ai_imtb_uncertainty[personality], ai_hash_mix(seed, 73));
  if (!frag || !*frag)
    return "";
  if (!ai_line_is_role_legal(frag, role, style))
    return "";
  if (out_used)
    *out_used = 1;
  return frag;
}

static const char *ai_imtb_pick_followup(const struct ai_reply_context *rctx, int role, int style, unsigned long seed)
{
  int personality;
  const struct ai_imtb_profile *profile;
  const char *frag;
  int allow = 0;

  if (!rctx)
    return "";
  personality = (rctx->personality >= 0 && rctx->personality < IMTB_MAX) ? rctx->personality : IMTB_STOIC;
  profile = ai_imtb_profile_get(personality);

  if ((rctx->domain == DOMAIN_SOCIAL || (rctx->domain == DOMAIN_NONE && rctx->speech_act == AI_INTENT_SMALLTALK)) && profile->curiosity > 0)
    allow = 1;
  if (rctx->confidence <= 1 && rctx->requested_count <= 1)
    allow = 1;
  if (!allow)
    return "";

  frag = ai_imtb_pick_fragment(ai_imtb_followup[personality], ai_hash_mix(seed, 97));
  if (!frag || !*frag)
    return "";
  if (!ai_line_is_role_legal(frag, role, style))
    return "";
  return frag;
}

static int ai_hash_ring_has(const unsigned int *ring, int count, int head, int max, unsigned int v)
{
  int i;
  for (i = 0; i < count; i++) {
    int idx = (head + i) % max;
    if (ring[idx] == v)
      return TRUE;
  }
  return FALSE;
}

static void ai_hash_ring_push(unsigned int *ring, int *head, int *count, int max, unsigned int v)
{
  int slot;

  if (!ring || !head || !count || max <= 0)
    return;

  slot = (*head + *count) % max;
  ring[slot] = v;
  if (*count < max) {
    (*count)++;
  } else {
    *head = (*head + 1) % max;
  }
}

static struct ai_conv_reply_state *ai_conv_reply_state_get(struct char_data *mob, int create)
{
  int i;
  int oldest = 0;

  if (!mob)
    return NULL;

  for (i = 0; i < AI_CONV_REPLY_STATE_MAX; i++) {
    if (ai_conv_reply_states[i].mob == mob)
      return &ai_conv_reply_states[i];
    if (!ai_conv_reply_states[i].mob && oldest == 0)
      oldest = i;
    else if (ai_conv_reply_states[oldest].updated_at > ai_conv_reply_states[i].updated_at)
      oldest = i;
  }

  if (!create)
    return NULL;

  memset(&ai_conv_reply_states[oldest], 0, sizeof(ai_conv_reply_states[oldest]));
  ai_conv_reply_states[oldest].mob = mob;
  ai_conv_reply_states[oldest].last_intent = AI_INTENT_NONE;
  ai_conv_reply_states[oldest].last_template_ids[0] = -1;
  ai_conv_reply_states[oldest].last_template_ids[1] = -1;
  ai_conv_reply_states[oldest].last_template_ids[2] = -1;
  ai_conv_reply_states[oldest].updated_at = time(0);
  return &ai_conv_reply_states[oldest];
}

static int ai_is_weather_smalltalk(const char *text)
{
  if (!text)
    return FALSE;
  return ai_text_has_sub_ci(text, "weather") || ai_text_has_sub_ci(text, "rain") || ai_text_has_sub_ci(text, "sun") ||
         ai_text_has_sub_ci(text, "storm") || ai_text_has_sub_ci(text, "wind") || ai_text_has_sub_ci(text, "snow") ||
         ai_text_has_sub_ci(text, "fog") || ai_text_has_sub_ci(text, "nice day") || ai_text_has_sub_ci(text, "cloud");
}

static unsigned long ai_conv_seed(struct char_data *mob, int intent, unsigned int counter)
{
  unsigned long h = 1469598103UL;
  int role = (mob && mob->ai_prof) ? mob->ai_prof->role : ROLE_UNKNOWN;
  int zone = (mob && IN_ROOM(mob) != NOWHERE) ? world[IN_ROOM(mob)].zone : -1;

  h = ai_hash_mix(h, (unsigned long)((mob && IS_NPC(mob)) ? GET_MOB_VNUM(mob) : 0));
  h = ai_hash_mix(h, (unsigned long)role);
  h = ai_hash_mix(h, (unsigned long)zone);
  h = ai_hash_mix(h, (unsigned long)intent);
  h = ai_hash_mix(h, (unsigned long)counter);
  return h;
}


static const struct ai_word_tier_entry ai_word_tiers[] = {
  {"AFFIRM", "yeah", "yes", "indeed", "verily"},
  {"DENY", "nope", "no", "I cannot", "I shall not"},
  {"UNCERTAIN", "dunno", "not sure", "I am unsure", "I cannot say with certainty"},
  {"WARN", "watch out", "be careful", "exercise care", "heed well"},
  {"GREET", "hey", "hello", "greetings", "well met, traveler"},
  {"FAREWELL", "bye", "take care", "safe travels", "may your road be clear"},
  {"THANKS", "ta", "thanks", "I thank you", "my gratitude"},
  {"TROUBLE", "mess", "trouble", "difficulty", "upheaval"},
  {"QUIET", "still", "quiet", "calm", "serene"},
  {"DANGEROUS", "bad", "risky", "dangerous", "perilous"},
  {"PLACE", "spot", "place", "location", "locale"},
  {"PEOPLE", "folks", "people", "individuals", "souls"},
  {"TIME", "a while", "some time", "a period", "an age"},
  {"ROAD", "road", "path", "route", "thoroughfare"},
  {"WORK", "job", "work", "duty", "charge"},
  {"UNKNOWN", "weird", "odd", "unusual", "peculiar"},
  {"GOOD", "fine", "good", "excellent", "admirable"},
  {"BAD", "rough", "bad", "poor", "lamentable"},
  {"WANT", "want", "would like", "prefer", "desire"},
  {"THINK", "reckon", "think", "believe", "surmise"},
  {NULL, NULL, NULL, NULL, NULL}
};

static const struct ai_phrase_entry ai_phrases[] = {
  {"OPENER",0,0,"Right."},{"OPENER",0,0,"Well."},{"OPENER",0,0,"Look."},{"OPENER",0,0,"So."},
  {"OPENER",1,1,"Truth is,"},{"OPENER",1,1,"Now then,"},{"OPENER",1,1,"As it stands,"},{"OPENER",1,1,"By my count,"},
  {"OPENER",2,2,"I should say,"},{"OPENER",2,2,"For what it is worth,"},{"OPENER",2,2,"As matters presently stand,"},{"OPENER",2,2,"To be precise,"},
  {"OPENER",3,3,"In the turning of things,"},{"OPENER",3,3,"Among all matters,"},{"OPENER",3,3,"By the lights above,"},{"OPENER",3,3,"In the broader pattern,"},
  {"CLOSER",0,-1,"that's it."},{"CLOSER",0,-1,"simple as."},{"CLOSER",0,-1,"end of."},{"CLOSER",0,-1,"no more."},
  {"CLOSER",1,-1,"that's how it stands."},{"CLOSER",1,-1,"make of it what you will."},{"CLOSER",1,-1,"that's the shape of it."},{"CLOSER",1,-1,"that's all."},
  {"CLOSER",2,-1,"I trust that answers your question."},{"CLOSER",2,-1,"that should be sufficient."},{"CLOSER",2,-1,"those are the facts."},{"CLOSER",2,-1,"thus it is resolved."},
  {"CLOSER",3,-1,"so the currents run."},{"CLOSER",3,-1,"as it has ever been."},{"CLOSER",3,-1,"so the pattern suggests."},{"CLOSER",3,-1,"such is the turning."},
  {"HEDGE_UNCERTAIN",0,-1,"I think,"},{"HEDGE_UNCERTAIN",1,-1,"maybe,"},{"HEDGE_UNCERTAIN",2,-1,"could be,"},{"HEDGE_UNCERTAIN",3,-1,"as best I can tell,"},
  {"HEDGE_EVASIVE",0,-1,"hard to say,"},{"HEDGE_EVASIVE",1,-1,"who can tell,"},{"HEDGE_EVASIVE",2,-1,"some would argue,"},{"HEDGE_EVASIVE",3,-1,"the matter is obscured,"},
  {"HEDGE_CONFIDENT",0,-1,"no doubt,"},{"HEDGE_CONFIDENT",1,-1,"without question,"},{"HEDGE_CONFIDENT",2,-1,"mark my words,"},{"HEDGE_CONFIDENT",3,-1,"I am certain,"},
  {"TOPIC_DUTY",0,-1,"the watch holds"},{"TOPIC_DUTY",1,-1,"order must be kept"},{"TOPIC_DUTY",2,-1,"this post is not idle"},{"TOPIC_DUTY",3,-1,"duty does not rest"},
  {"TOPIC_TRADE",0,-1,"coin moves slowly"},{"TOPIC_TRADE",1,-1,"the market breathes"},{"TOPIC_TRADE",2,-1,"stock runs thin"},{"TOPIC_TRADE",3,-1,"prices shift by the hour"},
  {"TOPIC_COMFORT",0,-1,"the hearth stays warm"},{"TOPIC_COMFORT",1,-1,"a meal helps most things"},{"TOPIC_COMFORT",2,-1,"rest when you can"},{"TOPIC_COMFORT",3,-1,"comfort is not weakness"},
  {"TOPIC_DANGER",0,-1,"the roads are not safe"},{"TOPIC_DANGER",1,-1,"trouble gathers"},{"TOPIC_DANGER",2,-1,"something moves in the east"},{"TOPIC_DANGER",3,-1,"stay armed if you travel"},
  {"TOPIC_MYSTERY",0,-1,"not everything has a name"},{"TOPIC_MYSTERY",1,-1,"some doors stay shut"},{"TOPIC_MYSTERY",2,-1,"the pattern is not clear"},{"TOPIC_MYSTERY",3,-1,"ask no more of that"},
  {"FEEL_POSITIVE",0,-1,"I am glad to hear it."},{"FEEL_POSITIVE",1,-1,"That is welcome news."},{"FEEL_POSITIVE",2,-1,"It eases my mind."},{"FEEL_POSITIVE",3,-1,"Good to know."},
  {"FEEL_NEGATIVE",0,-1,"That does not sit well."},{"FEEL_NEGATIVE",1,-1,"I worry at that."},{"FEEL_NEGATIVE",2,-1,"It grieves me to say."},{"FEEL_NEGATIVE",3,-1,"That is troubling."},
  {NULL,0,0,NULL}
};

static const char *const ai_followup_greet[] = {"And yourself? Road treating you well?","You look like you have traveled far. All well?","How long have you been in these parts?","First time through here, or do you know the area?",NULL};
static const char *const ai_followup_status[] = {"Did you come from the east road or the west?","Have you seen anything unusual on your way in?","Any trouble where you came from?",NULL};
static const char *const ai_followup_story[] = {"You have the look of someone with a tale of their own.","Have you heard something similar on your travels?",NULL};
static const char *const ai_followup_general[] = {"What brings you to ask?","Curious question. What is your interest?","I wonder what led you to that.",NULL};

static const char *ai_followup_pick(int speech_act, unsigned long seed)
{
  const char *const *pool = ai_followup_general;
  int n = 0;
  if (speech_act == AI_INTENT_GREET || speech_act == AI_INTENT_SMALLTALK) pool = ai_followup_greet;
  else if (speech_act == AI_INTENT_CONFUSION) pool = ai_followup_story;
  else if (speech_act == AI_INTENT_DIRECTIONS) pool = ai_followup_status;
  while (pool[n]) n++;
  if (n <= 0) return "";
  return pool[(seed + (unsigned long)speech_act) % (unsigned long)n];
}

static const char *ai_word(const char *concept, int tier)
{
  int i;
  int t = (tier < 0 || tier > 3) ? 1 : tier;
  for (i = 0; ai_word_tiers[i].concept; i++) {
    if (!strcmp(ai_word_tiers[i].concept, concept)) {
      if (t == 0) return ai_word_tiers[i].tier0;
      if (t == 1) return ai_word_tiers[i].tier1;
      if (t == 2) return ai_word_tiers[i].tier2;
      return ai_word_tiers[i].tier3;
    }
  }
  return concept ? concept : "";
}

static const char *ai_word_sn(const char *concept, int vocab_tier, int sn)
{
  int t = vocab_tier;
  if (sn == 0 && t > 1) t = 1;
  if (sn == 1 && t < 2) t = 2;
  return ai_word(concept, t);
}

static const char *ai_phrase(const char *tag, int tier, int rhythm, unsigned long seed, int slot)
{
  int idxs[64];
  int any[64];
  int i, n = 0, m = 0;
  for (i = 0; ai_phrases[i].tag; i++) {
    if (strcmp(ai_phrases[i].tag, tag) || ai_phrases[i].tier != tier)
      continue;
    if (ai_phrases[i].rhythm == rhythm)
      idxs[n++] = i;
    if (ai_phrases[i].rhythm == -1)
      any[m++] = i;
  }
  if (n > 0)
    return ai_phrases[idxs[(seed + (unsigned long)(slot * 13)) % (unsigned long)n]].text;
  if (m > 0)
    return ai_phrases[any[(seed + (unsigned long)(slot * 17)) % (unsigned long)m]].text;
  return "";
}

static const char *ai_mbti_string(const struct ai_voice_profile *vp)
{
  static char mbti[5];
  if (!vp) return "ISTJ";
  mbti[0] = vp->mbti_ei ? 'E' : 'I';
  mbti[1] = vp->mbti_sn ? 'N' : 'S';
  mbti[2] = vp->mbti_tf ? 'F' : 'T';
  mbti[3] = vp->mbti_jp ? 'P' : 'J';
  mbti[4] = '\0';
  return mbti;
}

static void ai_traits_to_mbti(const struct ai_actor_traits *t, struct ai_voice_profile *vp, int role)
{
  int score;
  int greed, curiosity, empathy, aggression, discipline, superstition;
  if (!t || !vp)
    return;

  greed = t->greed / 10;
  curiosity = t->curiosity / 10;
  empathy = t->empathy / 10;
  aggression = t->aggression / 10;
  discipline = t->discipline / 10;
  superstition = t->superstition / 10;

  score = empathy + curiosity - discipline;
  vp->mbti_ei = (score > 0) ? 1 : 0;
  score = curiosity + superstition - discipline;
  vp->mbti_sn = (score > 0) ? 1 : 0;
  score = empathy - aggression - discipline;
  vp->mbti_tf = (score > 0) ? 1 : 0;
  score = curiosity + greed - discipline;
  vp->mbti_jp = (score > 0) ? 1 : 0;

  if (role == ROLE_BEAST) {
    vp->mbti_ei = 0; vp->mbti_sn = 0; vp->mbti_tf = 0; vp->mbti_jp = 0;
  } else if (role == ROLE_SPIRIT) {
    vp->mbti_sn = 1;
    vp->mbti_jp = (discipline >= 9) ? 0 : 1;
  } else if (role == ROLE_CULTIST) {
    vp->mbti_sn = 1;
    vp->mbti_ei = (empathy + curiosity >= 18) ? 1 : 0;
  } else if (role == ROLE_GUARD) {
    vp->mbti_jp = 0;
  } else if (role == ROLE_BOSS) {
    vp->mbti_jp = 0;
    vp->mbti_tf = (empathy >= 8 && aggression <= 3) ? 1 : 0;
  }
}

static void ai_voice_profile_derive(struct char_data *mob, struct ai_voice_profile *out)
{
  unsigned long seed;
  int role;
  int zone = (mob && IN_ROOM(mob) != NOWHERE) ? world[IN_ROOM(mob)].zone : 0;
  const struct ai_actor_traits def = {50,50,50,50,50,50,50};
  const struct ai_actor_traits *t = &def;

  if (!mob || !out || !mob->ai_prof)
    return;

  role = mob->ai_prof->role;
  if (mob->ai_state && mob->ai_state->brain)
    t = &mob->ai_state->brain->traits;

  seed = ai_hash_mix(ai_hash_mix((unsigned long)GET_MOB_VNUM(mob), (unsigned long)role), (unsigned long)zone);
  out->vocab_tier = MAX(0, MIN(3, ((t->discipline / 10) + (t->empathy / 10)) / 4));
  out->rhythm = ((t->discipline / 10) > 5) ? 0 : (((t->curiosity / 10) > 5) ? 3 : 1);
  out->hedge_style = ((t->superstition / 10) > 5) ? 2 : (((t->discipline / 10) > 7) ? 3 : 1);
  out->topic_lean = (seed >> 12) % 5;
  out->tic_index = (seed >> 16) % 8;
  out->opener_index = (seed >> 20) % 6;
  out->closer_index = (seed >> 24) % 6;
  out->intensity = MAX(0, MIN(2, (t->aggression / 10) / 4));
  ai_traits_to_mbti(t, out, role);

  if (role == ROLE_BEAST) { out->vocab_tier = 0; out->rhythm = (out->rhythm % 2) ? 2 : 0; out->intensity = out->intensity ? 1 : 0; }
  if (role == ROLE_UNDEAD) { out->vocab_tier = 1 + (out->vocab_tier % 2); out->rhythm = (out->rhythm % 2) ? 2 : 0; out->hedge_style = (out->hedge_style % 2) ? 3 : 0; out->mbti_ei=0; out->mbti_jp=0; }
  if (role == ROLE_SPIRIT) { out->vocab_tier = 2 + (out->vocab_tier % 2); out->rhythm = (out->rhythm % 2) ? 3 : 1; }
  if (role == ROLE_GUARD) { if (out->vocab_tier > 2) out->vocab_tier = 2; out->hedge_style = (out->hedge_style % 2) ? 3 : 0; }
  if (role == ROLE_CULTIST) { out->hedge_style = 2; out->topic_lean = 4; }
  if (role == ROLE_BOSS) { out->hedge_style = (out->hedge_style % 2) ? 3 : 0; if (out->vocab_tier == 0) out->vocab_tier = 1; }
  if (role == ROLE_MERCHANT) { out->mbti_ei = 1; }
  if (role == ROLE_BANDIT) { out->mbti_tf=0; out->mbti_jp=1; }
}

static const struct ai_voice_profile *ai_voice_profile_get(struct char_data *mob)
{
  struct ai_conv_actor_state *st = ai_conv_actor_state_get(mob, 1);
  if (!st) return NULL;
  if (!st->voice_profile_ready) {
    ai_voice_profile_derive(mob, &st->voice_profile);
    st->voice_profile_ready = TRUE;
  }
  return &st->voice_profile;
}

static void ai_mbti_compound_modifier(const struct ai_voice_profile *vp, int speech_act, int *out_add_followup_question, int *out_add_topic_lean, int *out_suppress_opener, int *out_use_emotional_color, unsigned long seed)
{
  const char *m = ai_mbti_string(vp);
  (void)seed;
  *out_add_followup_question = vp && vp->mbti_ei;
  *out_add_topic_lean = vp && vp->mbti_ei;
  *out_suppress_opener = vp && !vp->mbti_ei;
  *out_use_emotional_color = vp && vp->mbti_tf;
  if (!strcmp(m, "ENFP")) { *out_add_followup_question = (speech_act==AI_INTENT_GREET||speech_act==AI_INTENT_SMALLTALK); *out_add_topic_lean=1; *out_suppress_opener=0; *out_use_emotional_color=1; }
  else if (!strcmp(m, "ISTJ")) { *out_add_followup_question=0; *out_add_topic_lean=0; *out_suppress_opener=1; *out_use_emotional_color=0; }
  else if (!strcmp(m, "INTJ")) { *out_add_followup_question=0; *out_add_topic_lean=(speech_act==AI_INTENT_SMALLTALK||speech_act==AI_INTENT_CONFUSION); *out_suppress_opener=1; *out_use_emotional_color=0; }
  else if (!strcmp(m, "ESFP")) { *out_add_followup_question=((seed>>2)%10)<7; *out_add_topic_lean=1; *out_use_emotional_color=1; }
  else if (!strcmp(m, "ENTP")) { *out_add_followup_question=1; *out_add_topic_lean=1; *out_use_emotional_color=0; }
  else if (!strcmp(m, "ISFP")) { *out_suppress_opener=1; *out_use_emotional_color=(speech_act==AI_INTENT_PRAISE||speech_act==AI_INTENT_INSULT); }
}

static void ai_voice_apply_tokens(const struct ai_voice_profile *vp, const char *in, char *out, size_t outsz)
{
  size_t oi = 0;
  size_t i = 0;
  while (in && in[i] && oi + 1 < outsz) {
    if (in[i] == '{') {
      char tok[32];
      size_t ti = 0;
      i++;
      while (in[i] && in[i] != '}' && ti + 1 < sizeof(tok)) tok[ti++] = in[i++];
      tok[ti] = '\0';
      if (in[i] == '}') {
        const char *w = ai_word_sn(tok, vp ? vp->vocab_tier : 1, vp ? vp->mbti_sn : 0);
        size_t k = 0;
        while (w && w[k] && oi + 1 < outsz) out[oi++] = w[k++];
        i++;
        continue;
      }
    }
    out[oi++] = in[i++];
  }
  out[oi] = '\0';
}


static int ai_line_is_role_legal(const char *line, int role, int style)
{
  int innkeeper = (role == ROLE_MERCHANT && style == 1);
  if (!line || !*line)
    return FALSE;

  if ((ai_text_has_sub_ci(line, "patrol") || ai_text_has_sub_ci(line, "post") || ai_text_has_sub_ci(line, "watch") || ai_text_has_sub_ci(line, "garrison") || ai_text_has_sub_ci(line, "duty") || ai_text_has_sub_ci(line, "formation") || ai_text_has_sub_ci(line, "orders"))
      && !(role == ROLE_GUARD || role == ROLE_BOSS))
    return FALSE;

  if ((ai_text_has_sub_ci(line, "hearth") || ai_text_has_sub_ci(line, "stew") || ai_text_has_sub_ci(line, "ale") || ai_text_has_sub_ci(line, "room") || ai_text_has_sub_ci(line, "bed") || ai_text_has_sub_ci(line, "warm yourself") || ai_text_has_sub_ci(line, "meal helps") || ai_text_has_sub_ci(line, "rest here"))
      && !innkeeper)
    return FALSE;

  if ((ai_text_has_sub_ci(line, "stock") || ai_text_has_sub_ci(line, "prices") || ai_text_has_sub_ci(line, "bargain") || ai_text_has_sub_ci(line, "wares"))
      && role != ROLE_MERCHANT)
    return FALSE;

  if (ai_text_has_sub_ci(line, "coin purse") && !(role == ROLE_MERCHANT || role == ROLE_BANDIT))
    return FALSE;

  if ((ai_text_has_sub_ci(line, "purse") || ai_text_has_sub_ci(line, "pay up") || ai_text_has_sub_ci(line, "mark") || ai_text_has_sub_ci(line, "easy coin") || ai_text_has_sub_ci(line, "keep walking"))
      && role != ROLE_BANDIT)
    return FALSE;

  if ((ai_text_has_sub_ci(line, "ritual") || ai_text_has_sub_ci(line, "omen") || ai_text_has_sub_ci(line, "pattern") || ai_text_has_sub_ci(line, "veil") || ai_text_has_sub_ci(line, "currents") || ai_text_has_sub_ci(line, "beyond"))
      && !(role == ROLE_CULTIST || role == ROLE_SPIRIT))
    return FALSE;

  return TRUE;
}

static struct ai_reply_intention ai_form_intention(struct char_data *mob, int speech_act, int speech_class, int suspicion_bucket, int arc_state, const struct ai_context_vector *ctx, const struct ai_session_read_entry *sr, struct ai_actor_memory_entry *e, time_t now)
{
  struct ai_reply_intention in;
  int role = (mob && mob->ai_prof) ? mob->ai_prof->role : ROLE_UNKNOWN;
  int style = (mob && mob->ai_prof) ? mob->ai_prof->style : 0;
  int role_fit = ai_role_can_answer_intent(role, style, speech_act) ? 20 : 0;

  (void)now;
  in.topic = speech_act;
  in.goal = GOAL_DEFLECT;
  in.stance = STANCE_NEUTRAL;
  in.be_specific = 0;
  in.be_brief = 0;

  if (suspicion_bucket >= 2)
    in.topic = AI_INTENT_QUEST;

  if (speech_act == AI_INTENT_CONFUSION || speech_act == AI_INTENT_GIBBERISH || speech_class == AI_SPEECH_UNKNOWN)
    in.goal = GOAL_CLARIFY;
  else if (speech_act == AI_INTENT_ASK_SERVICE || speech_act == AI_INTENT_BANK || speech_act == AI_INTENT_INN || speech_act == AI_INTENT_DIRECTIONS)
    in.goal = (role_fit >= AI_MIN_ROLE_FITNESS) ? GOAL_SERVE : GOAL_DEFLECT;
  else if (speech_act == AI_INTENT_GREET || speech_act == AI_INTENT_SMALLTALK)
    in.goal = GOAL_CONNECT;
  else if (speech_act == AI_INTENT_INSULT || speech_act == AI_INTENT_THREAT)
    in.goal = (role == ROLE_GUARD || role == ROLE_BOSS) ? GOAL_WARN : GOAL_DISMISS;
  else
    in.goal = (role_fit >= AI_MIN_ROLE_FITNESS) ? GOAL_INFORM : GOAL_DEFLECT;

  if (suspicion_bucket >= 2)
    in.goal = GOAL_WARN;
  if (arc_state == AI_ARC_COLD && in.goal != GOAL_CLARIFY)
    in.goal = (role == ROLE_GUARD || role == ROLE_BOSS) ? GOAL_DEFLECT : GOAL_DISMISS;

  switch (arc_state) {
    case AI_ARC_ENGAGED: in.stance = STANCE_OPEN; break;
    case AI_ARC_RAPPORT: in.stance = STANCE_WARM; break;
    case AI_ARC_COLD: in.stance = STANCE_GUARDED; break;
    default: in.stance = STANCE_NEUTRAL; break;
  }
  if (e && e->belief_hostility > 0.75f)
    in.stance = STANCE_HOSTILE;
  if (role == ROLE_GUARD && in.stance == STANCE_WARM)
    in.stance = STANCE_OPEN;
  if (role == ROLE_CULTIST)
    in.stance = STANCE_GUARDED;
  if (role == ROLE_SPIRIT && in.stance == STANCE_HOSTILE)
    in.stance = STANCE_GUARDED;
  if (role == ROLE_SPIRIT && in.stance == STANCE_WARM)
    in.stance = STANCE_OPEN;
  if (role == ROLE_BANDIT && arc_state == AI_ARC_STRANGER && in.stance == STANCE_OPEN)
    in.stance = STANCE_NEUTRAL;
  if (sr && sr->archetype == AI_ARCH_TROUBLEMAKER && (role == ROLE_GUARD || role == ROLE_MERCHANT) && in.stance < STANCE_GUARDED)
    in.stance = STANCE_GUARDED;

  in.be_specific = (arc_state >= AI_ARC_ENGAGED) || (e && e->belief_confidence > 0.5f);
  in.be_brief = (in.stance == STANCE_GUARDED || in.stance == STANCE_HOSTILE || arc_state == AI_ARC_STRANGER || arc_state == AI_ARC_COLD || (ctx && ctx->recent_violence));

  return in;
}

static const char *ai_select_content_for_intention(struct char_data *mob, const struct ai_reply_intention *in, const struct ai_reply_context *rctx, struct ai_session_read_entry *sr, int *out_from_template)
{
  static char best[256];
  const char *cands[48];
  int score[48];
  int n = 0;
  int i;
  int role = (mob && mob->ai_prof) ? mob->ai_prof->role : ROLE_UNKNOWN;
  int style = (mob && mob->ai_prof) ? mob->ai_prof->style : 0;
  int role_fit = ai_role_can_answer_intent(role, style, in ? in->topic : AI_INTENT_NONE) ? 20 : 0;
  const struct ai_voice_profile *vp = ai_voice_profile_get(mob);
  static const char *const checkin_guard[] = {"All good on this post. What do you need?", "Quiet watch so far. You?", "Holding steady. Need directions?", NULL};
  static const char *const checkin_merch[] = {"Busy enough. Looking for wares?", "All good. Need supplies?", "Keeping trade moving. What can I get you?", NULL};
  static const char *const checkin_inn[] = {"House is steady. Need food or a room?", "All right here. Want stew or a bed?", "Hearth is warm. How can I help?", NULL};
  static const char *const story_inn[] = {"Travelers say the north road is calmer this week.", "Heard a caravan made good time through the pass.", NULL};
  static const char *const story_cult[] = {"Some whisper the moon looked wrong above the ridge.", "A sign appeared at dawn, then vanished.", NULL};
  static const char *const story_spirit[] = {"Old stones remember footsteps from long ago.", "I have heard echoes from the western gate.", NULL};
  static const char *const service_lead_guard[] = {"Try", "Head to", "Best place is", "Go see", NULL};
  static const char *const service_lead_merch[] = {"You want", "Look for", "Best bet is", "Try", NULL};
  static const char *const service_lead_inn[] = {"If it is food", "If it is lodging", "For supplies", "Try", NULL};
  static const char *const route_joiners[] = {"from here", "straight off", "near the square", "not far", NULL};
  static const char *const clarify_pool[] = {
    "What do you mean?",
    "Can you ask that another way?",
    "Could you be more specific?",
    "Would you rephrase that?",
    "What are you asking exactly?",
    "Can you clarify your point?",
    "Could you make that clearer?",
    "What part do you want answered?",
    NULL
  };
  static const char *const deflect_guard[] = {"State your business and keep it short.", "I have watch duty to finish.", "Ask one clear question and stand easy.", "I can spare a moment, not a lecture.", "If you need help, ask plainly and briefly.", "I keep this post, so keep to the point.", "I've orders to hold this watch.", "Speak direct; my patrol is due.", "I answer clear needs, not wandering chatter.", "Keep your request concise.", "Use plain words and we can proceed.", "I can help, but not for long.", NULL};
  static const char *const deflect_inn[] = {"If you need rest, ask for a room.", "I can pour ale, not debate rumors.", "Ask for stew, room, or bed and we'll be square.", "This hearth serves comfort, not long disputes.", "Keep it simple: meal, bed, or directions.", "I can warm you up, not chase side talk.", "If you're staying, ask what you need.", "I keep rooms and rest, not gossip chains.", "Tell me if you need a bed or a bowl.", "Let's keep this to house business.", "I can help you settle in, not speculate.", "Best keep it to inn matters.", NULL};
  static const char *const deflect_merch[] = {"Ask about wares, prices, or stock.", "Trade only for now.", "If it isn't about a bargain, keep moving.", "Name the item and we can deal.", "I can quote prices, not idle theories.", "Let's keep this on wares.", "If you want stock details, ask directly.", "I handle coin, not side stories.", "Pick a good and we'll talk numbers.", "Bargain talk only today.", "I can help with price and quality.", "State the purchase and we'll proceed.", NULL};
  static const char *const deflect_bandit[] = {"Keep walking, mark.", "Not your concern.", "Pay up or move on.", "Easy coin doesn't answer questions.", "Wrong road for chatter.", "Purse first, talk later.", "You're asking too much for free.", "Keep your head down and pass through.", "No free guidance in my stretch.", "Step light and keep walking.", "Ask less, pay more.", "Pick a direction and go.", NULL};
  static const char *const deflect_cult[] = {"The veil does not answer every voice.", "Not all patterns are for open telling.", "That omen is not for this moment.", "The currents stay unread for strangers.", "Some signs are kept within ritual.", "Beyond words for now.", "The pattern remains closed.", "Not every question crosses the veil.", "This thread is not yours to pull.", "The rite keeps that answer veiled.", "I will not open that omen.", "Ask another current.", NULL};
  static const char *const warn_guard[] = {"Stand easy and keep lawful.", "Mind your tone and hold position.", "Keep order or move along.", "Last courtesy: stay civil.", "This post stays calm by rule.", "Choose respect and keep to the law.", "Drop the heat and speak plainly.", "Watch your words and keep the peace.", "Take one step back and steady yourself.", "Do not test the watch.", "Hold your temper and comply.", "Keep hands clear and trouble ends here.", NULL};
  static const char *const warn_inn[] = {"Keep it civil under this roof.", "No brawls near my hearth.", "Lower your voice or leave the room.", "You can rest here, not rage here.", "Respect the house and calm down.", "No threats over my ale and stew.", "Settle down or take the road.", "This bed and board stays peaceful.", "Quiet it, or I close your tab.", "You're welcome warm, not wild.", "Keep your temper from my common room.", "Ease up before I turn you out.", NULL};
  static const char *const warn_merch[] = {"Keep this market lawful.", "No threats near my wares.", "Mind your hands around my stock.", "Talk straight or walk.", "Keep your edge down and your coin honest.", "No trouble at my stall.", "Bargain fair, or leave.", "Watch it, or this deal is done.", "Keep calm and we'll keep trading.", "You want prices, not problems.", "Don't test me in the market.", "One more push and you're gone.", NULL};
  static const char *const warn_bandit[] = {"Careful, mark.", "Push again and you pay up.", "Keep your voice low and your purse ready.", "Test me and lose easy coin.", "One wrong move and this road gets expensive.", "Don't make me collect early.", "You walk because I allow it.", "Stay polite and keep moving.", "I warned you once.", "Easy now, unless you want trouble.", "Pay attention and stay in line.", "Keep walking while you still can.", NULL};
  static const char *const warn_cult[] = {"Do not mock the ritual.", "Keep your voice soft before the omen.", "The pattern bites back when provoked.", "Step lightly near the veil.", "Show restraint and the current stays calm.", "Disrespect the rite and face consequence.", "Let this warning settle.", "Do not force what lies beyond.", "Keep still; the sign is watching.", "Temper yourself before the pattern turns.", "The veil closes on reckless tongues.", "Take heed and lower your intent.", NULL};
  static const char *const dismiss_pool[] = {"Keep it brief.", "Not today.", "Move along.", NULL};
  static const char *const connect_guard[] = {"Well met. Keep to the law.", "Good day. Stay alert at this post.", "You're clear to pass; keep it lawful.", "Welcome through. Keep the peace.", "Morning. Report trouble, avoid it otherwise.", "Steady step and you'll do fine here.", "Road looks calm; keep it that way.", "You're safe while you stay civil.", "All quiet so far; let's keep watch tight.", "Need help, ask plain and respectful.", "Duty's steady and the gate is open.", "Walk with purpose and keep clear of trouble.", NULL};
  static const char *const connect_inn[] = {"Welcome in. Warm yourself by the hearth.", "Good evening. Rest here and breathe easy.", "Come in from the road; stew is hot.", "Beds are ready if you need real sleep.", "Set your pack down and take a seat.", "Ale's fresh and the room is calm.", "Long road? I've got bed and broth.", "Take the edge off by the fire.", "You're welcome while you keep it civil.", "A warm table waits if you're staying.", "House is open, hearth is bright.", "Stay the night and start fresh at dawn.", NULL};
  static const char *const connect_merch[] = {"Greetings. Looking for wares?", "Hello there; stock is fresh today.", "Fair prices and honest measures right here.", "See something useful? Ask and I'll quote.", "Trade's steady; let's make a clean bargain.", "Need provisions? I've got quality stock.", "Take your time and compare the wares.", "Coin and clarity make good business.", "If you need kit, I can sort it.", "Market's open; what's your price range?", "Ask for an item and I'll run numbers.", "Good day. Let's trade straight.", NULL};
  static const char *const connect_bandit[] = {"Yeah? Keep it short.", "You talk fast for this road.", "Step careful and maybe we get along.", "You look like a mark worth sparing today.", "No sudden moves and we're fine.", "Walk light and keep your purse close.", "You made it this far; don't waste it.", "Talk plain and keep moving.", "Coin buys manners around here.", "You seem sharp enough to listen.", "I'm in a decent mood; don't test it.", "Say your bit and keep walking.", NULL};
  static const char *const connect_cult[] = {"The veil stirs; speak your thread.", "I hear a pattern in your voice.", "The current is calm enough to listen.", "An omen opens; ask with care.", "Beyond the noise, I can hear you.", "The rite is quiet; proceed.", "A sign crosses the air. Continue.", "You stand at the edge of meaning; speak.", "The pattern leans closer.", "I attend, if your words are steady.", "The veil allows a moment.", "Your voice enters the current.", NULL};
  static const char *const serve_shop[] = {"I can show wares, prices, and stock.", "Tell me the item and I'll quote it.", "Need provisions? I can set a fair bargain.", "I keep practical stock for the road.", "Ask by item type and I'll narrow options.", "If you sell, I can price your goods.", "Let's compare quality and numbers.", "State your budget and I can guide you.", "I can handle both buying and selling.", "Point to the wares and we'll bargain.", "Trade with me and keep it straightforward.", "Name your need and we'll do business.", NULL};
  static const char *const serve_inn[] = {"I can offer room, bed, stew, and ale.", "Need rest? I have clean rooms tonight.", "Sit by the hearth and warm yourself first.", "A hot meal and a bed are ready.", "If you're hurt, rest here and recover.", "You can take a room and sleep safely.", "I serve comfort: bed, broth, and calm.", "Tell me if you need food or a room.", "Stay here, eat well, and start fresh.", "I can set you up with a quiet bed.", "Warm ale and soft blankets are available.", "Rest here as long as you keep house peace.", NULL};
  static const char *const serve_bank[] = {"The bank is east from here.", "Head to the vault counter in town.", "Bankers keep ledgers near the central square.", "Follow the main street to the bank doors.", "The vault hall is by the market road.", "Ask a clerk at the bank counter.", "Take the city route toward the vault district.", "You want the bank office near town center.", "Go to the vault house and ask inside.", "The banking hall is a short walk east.", "Find the ledger desk at the bank.", "Bank services are handled at the vault post.", NULL};
  static const char *const serve_dir[] = {"Head north, then east at the square.", "Follow the main road and ask at the post.", "Keep to the central path, then turn by the sign.", "Take the broad street until the crossroads.", "Go straight, then turn where the lantern hangs.", "Use the market lane and continue past the gate.", "Stay on the marked road and you'll find it.", "Walk the main route; the next post can guide you.", "Take the road with foot traffic and keep east.", "Follow signs along the square and turn once.", "Go by the busy lane, then look for the marker.", "Take the shortest path through the central street.", NULL};
  static const char *const inform_guard[] = {"Watch duty is steady and the streets are mostly calm.", "Patrol reports are clean this watch.", "Orders are clear: keep the post secure.", "The garrison is rotating patrols on schedule.", "We've had noise, but no breach of order.", "Guard traffic is normal and lawful.", "The watch is alert and holding lines.", "Status is stable; keep your dealings legal.", "Post discipline is intact this shift.", "We are scanning crossings and side roads.", "The gate remains open under routine checks.", "Command says keep calm and keep watch.", NULL};
  static const char *const inform_inn[] = {"House is steady: warm hearth, full stew, clean beds.", "Business is good and the common room stays calm.", "Rooms are turning over and ale is flowing well.", "Travelers are resting, and the fire is strong.", "Beds are mostly taken, but we manage comfort.", "Kitchen is busy and spirits are better by night.", "The inn is quiet enough for real sleep.", "We keep a warm room and a fair tab.", "Stew's on, hearth's bright, and service is smooth.", "Guests are settled and trouble is low.", "Most folk want rest and hot meals today.", "Status is simple: comfort, food, and calm.", NULL};
  static const char *const inform_merch[] = {"Trade is steady and stock is moving.", "Prices are stable and bargains are fair.", "Market traffic is solid this day.", "My wares are fresh and supply looks good.", "Coin flow is healthy and demand is clear.", "Caravan arrivals improved stock quality.", "Sales are up on practical goods.", "I can still bargain on bulk purchases.", "The market is active without chaos.", "Inventory is balanced across common needs.", "Business is brisk and honest today.", "Status: good stock, fair prices, clean deals.", NULL};
  static const char *const inform_bandit[] = {"Road's busy, but I keep my mark list short.", "Easy coin is thinner today, so I stay selective.", "Things are tense; folk guard their purse tighter.", "I've seen richer marks farther east.", "This stretch pays if you move smart.", "Status is simple: watch, weigh, and collect.", "Too many eyes right now, so I stay cool.", "Coin still moves, just slower than dawn.", "I read the road before I act.", "If trouble grows, I change lanes fast.", "The best marks travel when they feel safe.", "Today is caution first, profit second.", NULL};
  static const char *const inform_cult[] = {"The omen is quiet, but the pattern keeps turning.", "Ritual signs are subtle today.", "The veil feels thin near dusk.", "Current lines are steady, not silent.", "Beyond the noise, the pattern is coherent.", "The rite remains in preparation.", "A mild omen passed without rupture.", "The veil answers in fragments this hour.", "Currents align, though meaning is partial.", "The pattern holds, awaiting a stronger sign.", "We observe and record each omen.", "Status is calm, but never still.", NULL};
  static const char *const mbti_numbers[] = {"first", "second", "third", "two", "three", "step", "steps", "count", "measured", "precise", NULL};
  static const char *const mbti_patterns[] = {"pattern", "omen", "signal", "trend", "possible", "maybe", "beyond", "currents", NULL};
  static const char *const mbti_logic[] = {"because", "therefore", "if", "then", "so", "evidence", "reason", "rule", NULL};
  static const char *const mbti_values[] = {"kind", "care", "respect", "welcome", "warm", "peace", "fair", "comfort", NULL};

  if (out_from_template)
    *out_from_template = 0;
  best[0] = '\0';
  if (!in)
    return "What do you mean?";

  if (in->goal == GOAL_CLARIFY) {
    snprintf(best, sizeof(best), "%.*s", (int)sizeof(best) - 1, ai_pick_phrase(clarify_pool));
    return best;
  }

  {
    enum ai_reply_goal effective_goal = in->goal;

    if ((effective_goal == GOAL_INFORM || effective_goal == GOAL_SERVE) && role_fit < AI_MIN_ROLE_FITNESS)
      effective_goal = GOAL_DEFLECT;
    if (rctx && (rctx->domain == DOMAIN_SHOPPING || rctx->domain == DOMAIN_SERVICES || rctx->domain == DOMAIN_DIRECTIONS)) {
      if (effective_goal == GOAL_INFORM)
        effective_goal = GOAL_SERVE;
    }

    if (effective_goal == GOAL_CLARIFY) {
      for (i = 0; clarify_pool[i] && n < 48; i++) cands[n++] = clarify_pool[i];
    } else if (effective_goal == GOAL_DEFLECT) {
      const char *const *pool = (role == ROLE_GUARD || role == ROLE_BOSS) ? deflect_guard :
                                ((role == ROLE_MERCHANT && style == 1) ? deflect_inn :
                                (role == ROLE_MERCHANT ? deflect_merch :
                                (role == ROLE_BANDIT ? deflect_bandit : deflect_cult)));
      for (i = 0; pool[i] && n < 48; i++) cands[n++] = pool[i];
    } else if (effective_goal == GOAL_WARN) {
      const char *const *pool = (role == ROLE_GUARD || role == ROLE_BOSS) ? warn_guard :
                                ((role == ROLE_MERCHANT && style == 1) ? warn_inn :
                                (role == ROLE_MERCHANT ? warn_merch :
                                (role == ROLE_BANDIT ? warn_bandit : warn_cult)));
      for (i = 0; pool[i] && n < 48; i++) cands[n++] = pool[i];
    } else if (effective_goal == GOAL_DISMISS) {
      for (i = 0; dismiss_pool[i] && n < 48; i++) cands[n++] = dismiss_pool[i];
    } else if (effective_goal == GOAL_CONNECT) {
      const char *const *pool = (role == ROLE_GUARD || role == ROLE_BOSS) ? connect_guard :
                                ((role == ROLE_MERCHANT && style == 1) ? connect_inn :
                                (role == ROLE_MERCHANT ? connect_merch :
                                (role == ROLE_BANDIT ? connect_bandit : connect_cult)));
      for (i = 0; pool[i] && n < 48; i++) cands[n++] = pool[i];
    } else if (effective_goal == GOAL_SERVE) {
      const char *const *pool = (in->topic == AI_INTENT_INN) ? serve_inn :
                                ((in->topic == AI_INTENT_BANK) ? serve_bank :
                                ((in->topic == AI_INTENT_DIRECTIONS) ? serve_dir : serve_shop));
      for (i = 0; pool[i] && n < 48; i++) cands[n++] = pool[i];
      if (rctx && rctx->facts.confidence == 3 && n < 48) {
        const char *lead = (role == ROLE_GUARD || role == ROLE_BOSS) ? ai_pick_phrase(service_lead_guard) : ((role == ROLE_MERCHANT && style == 1) ? ai_pick_phrase(service_lead_inn) : ai_pick_phrase(service_lead_merch));
        const char *join = ai_pick_phrase(route_joiners);
        snprintf(best, sizeof(best), "%s %s %s %s.", lead ? lead : "Try", rctx->facts.service_name[0] ? rctx->facts.service_name : "that place", join ? join : "nearby", rctx->facts.route_snippet[0] ? rctx->facts.route_snippet : "ask around");
        cands[n++] = best;
      }
    } else if (effective_goal == GOAL_INFORM && rctx && (rctx->domain == DOMAIN_SOCIAL || in->topic == AI_INTENT_SMALLTALK) &&
               !(rctx->domain == DOMAIN_SHOPPING || rctx->domain == DOMAIN_SERVICES || rctx->domain == DOMAIN_DIRECTIONS)) {
      const char *const *pool = (role == ROLE_GUARD || role == ROLE_BOSS) ? checkin_guard : ((role == ROLE_MERCHANT && style == 1) ? checkin_inn : checkin_merch);
      for (i = 0; pool[i] && n < 48; i++) cands[n++] = pool[i];
    } else if (effective_goal == GOAL_INFORM && in->topic == AI_INTENT_RUMOR) {
      const char *const *pool = (role == ROLE_MERCHANT && style == 1) ? story_inn : ((role == ROLE_CULTIST) ? story_cult : story_spirit);
      for (i = 0; pool[i] && n < 48; i++) cands[n++] = pool[i];
    } else {
      const char *const *pool = (role == ROLE_GUARD || role == ROLE_BOSS) ? inform_guard :
                                ((role == ROLE_MERCHANT && style == 1) ? inform_inn :
                                (role == ROLE_MERCHANT ? inform_merch :
                                (role == ROLE_BANDIT ? inform_bandit : inform_cult)));
      for (i = 0; pool[i] && n < 48; i++) cands[n++] = pool[i];
    }

    if (effective_goal == GOAL_INFORM || effective_goal == GOAL_SERVE) {
    const char *tpl = ai_template_reply_for_intent(mob, in->topic, (rctx && rctx->current_text) ? rctx->current_text : "", -1, NULL);
    if (tpl && n < 48)
      cands[n++] = tpl;
    }
  }

  if (n == 0) {
    const char *fallback = ai_role_redirect_line(role, style, TARGET_NONE);
    return fallback ? fallback : "I'd rather not discuss that.";
  }

  for (i = 0; i < n; i++) {
    int j;
    const char *cand = cands[i];
    score[i] = 0;
    if (!cand || !*cand || !ai_line_is_role_legal(cand, role, style)) {
      score[i] = -999;
      continue;
    }
    {
      unsigned int h = (unsigned int)ai_hash_text_stable(cand);
      int rej = FALSE;
      if (sr && ai_hash_ring_has(sr->recent_reply_hashes, sr->recent_reply_count, sr->recent_reply_head, AI_RECENT_REPLY_HASH_MAX, h))
        rej = TRUE;
      else {
        struct ai_conv_actor_state *conv_st = ai_conv_actor_state_get(mob, 0);
        if (conv_st && ai_hash_ring_has(conv_st->recent_core_hashes, conv_st->recent_core_count, conv_st->recent_core_head, AI_RECENT_CORE_HASH_MAX, h))
          rej = TRUE;
      }
      if (rej) {
        score[i] = -998;
        if (ai_debug)
          ai_debug_log("AI_REPEAT_REJECT vnum=%d hash=%u cand=%s", GET_MOB_VNUM(mob), h, cand);
        continue;
      }
    }
    if (in->be_brief)
      score[i] += (strlen(cand) <= 72) ? 5 : -3;
    if (in->stance == STANCE_WARM && (ai_text_has_sub_ci(cand, "welcome") || ai_text_has_sub_ci(cand, "warm") || ai_text_has_sub_ci(cand, "rest")))
      score[i] += 4;
    if ((in->stance == STANCE_HOSTILE || in->stance == STANCE_GUARDED) && (ai_text_has_sub_ci(cand, "watch") || ai_text_has_sub_ci(cand, "brief") || ai_text_has_sub_ci(cand, "move along")))
      score[i] += 4;
    if (rctx && rctx->current_text && *rctx->current_text && ai_text_has_sub_ci(rctx->current_text, "food") && ai_text_has_sub_ci(cand, "stew"))
      score[i] += 3;
    if (rctx) {
      if (rctx->domain == DOMAIN_SHOPPING && (ai_text_has_sub_ci(cand, "buy") || ai_text_has_sub_ci(cand, "wares") || ai_text_has_sub_ci(cand, "stock")))
        score[i] += 3;
      if (rctx->domain == DOMAIN_SERVICES && (ai_text_has_sub_ci(cand, "inn") || ai_text_has_sub_ci(cand, "bank") || ai_text_has_sub_ci(cand, "service")))
        score[i] += 3;
      if (rctx->domain == DOMAIN_DIRECTIONS && (ai_text_has_sub_ci(cand, "head") || ai_text_has_sub_ci(cand, "road") || ai_text_has_sub_ci(cand, "route")))
        score[i] += 3;
    }
    if (vp) {
      if (strchr(cand, '?')) score[i] += vp->mbti_ei ? 1 : -1;
      if (vp->mbti_sn > 0)
        for (j = 0; mbti_patterns[j]; j++) if (ai_text_has_sub_ci(cand, mbti_patterns[j])) { score[i] += 1; break; }
      if (vp->mbti_sn <= 0)
        for (j = 0; mbti_numbers[j]; j++) if (ai_text_has_sub_ci(cand, mbti_numbers[j])) { score[i] += 1; break; }
      if (vp->mbti_tf > 0)
        for (j = 0; mbti_values[j]; j++) if (ai_text_has_sub_ci(cand, mbti_values[j])) { score[i] += 1; break; }
      if (vp->mbti_tf <= 0)
        for (j = 0; mbti_logic[j]; j++) if (ai_text_has_sub_ci(cand, mbti_logic[j])) { score[i] += 1; break; }
      if (vp->mbti_jp > 0 && (ai_text_has_sub_ci(cand, "always") || ai_text_has_sub_ci(cand, "clear") || ai_text_has_sub_ci(cand, "steady")))
        score[i] += 1;
      if (vp->mbti_jp <= 0 && (ai_text_has_sub_ci(cand, "maybe") || ai_text_has_sub_ci(cand, "might") || ai_text_has_sub_ci(cand, "perhaps")))
        score[i] += 1;
    }
  }

  {
    int bi = -1;
    int bs = -1000;
    int oldest_ok = -1;
    for (i = 0; i < n; i++) {
      if (oldest_ok < 0 && score[i] > -999)
        oldest_ok = i;
      if (score[i] > bs) {
        bs = score[i];
        bi = i;
      }
    }
    if ((bi < 0 || bs <= -999) && oldest_ok >= 0)
      bi = oldest_ok;
    if (bi >= 0 && score[bi] > -999) {
      unsigned int h = (unsigned int)ai_hash_text_stable(cands[bi]);
      struct ai_conv_actor_state *conv_st = ai_conv_actor_state_get(mob, 1);
      if (conv_st)
        ai_hash_ring_push(conv_st->recent_core_hashes, &conv_st->recent_core_head, &conv_st->recent_core_count, AI_RECENT_CORE_HASH_MAX, h);
      if (sr)
        ai_hash_ring_push(sr->recent_reply_hashes, &sr->recent_reply_head, &sr->recent_reply_count, AI_RECENT_REPLY_HASH_MAX, h);
      if (ai_debug)
        ai_debug_log("AI_REPEAT_CHOOSE vnum=%d hash=%u cand=%s", GET_MOB_VNUM(mob), h, cands[bi]);
      snprintf(best, sizeof(best), "%.*s", (int)sizeof(best) - 1, cands[bi]);
      return best;
    }
  }

  {
    const char *fallback = ai_role_redirect_line(role, style, TARGET_NONE);
    if (!fallback) fallback = "I'd rather not discuss that.";
    snprintf(best, sizeof(best), "%.*s", (int)sizeof(best) - 1, fallback);
  }
  return best;
}

static void ai_voice_assemble(struct char_data *mob, const struct ai_voice_profile *vp, const struct ai_reply_intention *in, const struct ai_reply_context *rctx, unsigned long seed, char *out, size_t outsz)
{
  char core[256], slotted[256], work[512], topic_tag[24], buf[128];
  int role = (mob && mob->ai_prof) ? mob->ai_prof->role : ROLE_UNKNOWN;
  int style = (mob && mob->ai_prof) ? mob->ai_prof->style : 0;
  int rhythm;
  int use_emotional = 0;
  int recent_violence = 0;
  int add_q = 0, add_topic = 0, suppress_opener = 0;
  int allow_followup = 0;
  int personality = (rctx ? rctx->personality : IMTB_STOIC);
  const struct ai_imtb_profile *iprofile = ai_imtb_profile_get(personality);
  const char *plead = "";
  const char *punc = "";
  const char *pfollow = "";
  int psuppressed = 0;
  int punc_used = 0;
  int multi_topic = (rctx && rctx->requested_count > 1);
  int service_domain = (rctx && (rctx->domain == DOMAIN_SHOPPING || rctx->domain == DOMAIN_SERVICES || rctx->domain == DOMAIN_DIRECTIONS));
  int engaged_arc = (in && (in->stance == STANCE_OPEN || in->stance == STANCE_WARM));
  struct ai_conv_actor_state *st = ai_conv_actor_state_get(mob, 0);
  const char *closer = "";
  const char *topic = "";
  size_t len;
  int vocab_tier = vp ? vp->vocab_tier : 1;
  int vp_rhythm = vp ? vp->rhythm : 1;
  int vp_closer_index = vp ? vp->closer_index : 1;
  int vp_tic_index = vp ? vp->tic_index : 0;
  int vp_topic_lean = vp ? vp->topic_lean : 0;

  (void)seed;
  if (!out || outsz == 0)
    return;

  ai_slot_replace((rctx && rctx->chosen_core) ? rctx->chosen_core : "", rctx ? &rctx->facts : NULL, slotted, sizeof(slotted));
  ai_voice_apply_tokens(vp, slotted, core, sizeof(core));
  if (!ai_line_is_role_legal(core, role, style)) {
    ai_voice_apply_tokens(vp, slotted, core, sizeof(core));
    if (!ai_line_is_role_legal(core, role, style))
      core[0] = "\0"[0];
  }

  plead = ai_imtb_pick_leadin(st, rctx, role, style, ai_hash_mix(seed, 111), &psuppressed);
  if (psuppressed && ai_debug)
    ai_debug_log("AI_IMTB_SUPPRESS vnum=%d role=%s fragment=leadin", GET_MOB_VNUM(mob), ai_role_name_local(role));

  if (service_domain)
    allow_followup = 0;

  if (in && in->goal == GOAL_CLARIFY) {
    snprintf(out, outsz, "%.*s", (int)outsz - 1, core);
    len = strlen(out);
    if (len == 0 || out[len - 1] != '?')
      snprintf(out, outsz, "What do you mean?");
    return;
  }

  ai_mbti_compound_modifier(vp, rctx ? rctx->speech_act : AI_INTENT_NONE, &add_q, &add_topic, &suppress_opener, &use_emotional, seed);

  rhythm = vp ? vp->rhythm : 1;
  if (st && st->tone_clipped)
    rhythm = 0;
  if (iprofile && iprofile->verbosity <= 0 && rhythm > 1)
    rhythm = 1;
  if (in && in->be_brief && rhythm > 1)
    rhythm = 1;

  closer = ai_phrase("CLOSER", vocab_tier, rhythm, seed, vp_closer_index);
  if (!ai_line_is_role_legal(closer, role, style))
    closer = ai_phrase("CLOSER", vocab_tier, rhythm, ai_hash_mix(seed, 13), vp_closer_index + 1);
  if (!ai_line_is_role_legal(closer, role, style))
    closer = "";

  snprintf(topic_tag, sizeof(topic_tag), "TOPIC_%s", (vp_topic_lean==0)?"DUTY":(vp_topic_lean==1)?"TRADE":(vp_topic_lean==2)?"COMFORT":(vp_topic_lean==3)?"DANGER":"MYSTERY");
  topic = ai_phrase(topic_tag, vocab_tier, rhythm, seed, 5);
  if (!ai_line_is_role_legal(topic, role, style))
    topic = ai_phrase(topic_tag, 1, rhythm, ai_hash_mix(seed, 29), 1);
  if (!ai_line_is_role_legal(topic, role, style))
    topic = "";

  if (st && st->mob && IN_ROOM(st->mob) != NOWHERE) {
    struct ai_conv_room_state *room_st = ai_conv_room_state_get(IN_ROOM(st->mob), 0);
    if (room_st && room_st->last_violence_time > 0 && (time(0) - room_st->last_violence_time) <= 60)
      recent_violence = 1;
  }

  if (in && in->goal == GOAL_SERVE && role == ROLE_MERCHANT && style == 1)
    use_emotional = 1;

  if (plead && *plead)
    snprintf(work, sizeof(work), "%s %s", plead, core);
  else
    snprintf(work, sizeof(work), "%s", core);

  if (service_domain && !multi_topic && rctx && rctx->confidence <= 2) {
    punc = ai_imtb_pick_uncertainty(rctx, role, style, ai_hash_mix(seed, 137), &punc_used);
    if (punc_used && punc && *punc) {
      char oldwork[512];
      snprintf(oldwork, sizeof(oldwork), "%s", work);
      snprintf(work, sizeof(work), "%s %s", punc, oldwork);
      if (ai_debug && service_domain)
        ai_debug_log("AI_IMTB_UNCERTAIN vnum=%d role=%s domain=%d", GET_MOB_VNUM(mob), ai_role_name_local(role), rctx->domain);
    }
  }

  if (rhythm >= 1 && closer && *closer)
    snprintf(work + strlen(work), sizeof(work) - strlen(work), " %s", closer);

  if (rhythm >= 3 && topic && *topic)
    snprintf(work + strlen(work), sizeof(work) - strlen(work), " %s.", topic);

  if (vp && vp->mbti_ei && engaged_arc && !recent_violence && !(st && st->tone_no_extras) &&
      ((rhythm == 2) || (rhythm >= 3)))
    allow_followup = 1;

  if (allow_followup)
    snprintf(work + strlen(work), sizeof(work) - strlen(work), " %s", ai_followup_pick(rctx ? rctx->speech_act : AI_INTENT_NONE, seed));

  pfollow = ai_imtb_pick_followup(rctx, role, style, ai_hash_mix(seed, 149));
  if (pfollow && *pfollow && !service_domain && !multi_topic)
    snprintf(work + strlen(work), sizeof(work) - strlen(work), " %s", pfollow);

  if (use_emotional) {
    const char *feel = ((rctx ? rctx->speech_act : AI_INTENT_NONE)==AI_INTENT_PRAISE||(rctx ? rctx->speech_act : AI_INTENT_NONE)==AI_INTENT_GREET||(rctx ? rctx->speech_act : AI_INTENT_NONE)==AI_INTENT_SMALLTALK) ? ai_phrase("FEEL_POSITIVE", vocab_tier, rhythm, seed, 7) : ai_phrase("FEEL_NEGATIVE", vocab_tier, rhythm, seed, 8);
    snprintf(buf, sizeof(buf), " %s", feel);
    snprintf(work + strlen(work), sizeof(work) - strlen(work), "%s", buf);
  }

  len = strlen(work);
  if (len > 180) {
    size_t cut = 180;
    while (cut > 0 && work[cut] != '.' && work[cut] != '?' && work[cut] != '!')
      cut--;
    if (cut == 0)
      cut = 180;
    work[cut] = '\0';
  }

  if (rctx && rctx->callback_prefix[0] && ai_line_is_role_legal(rctx->callback_prefix, role, style)) {
    size_t used;
    snprintf(out, outsz, "%.*s", (int)outsz - 1, rctx->callback_prefix);
    used = strlen(out);
    if (used < outsz - 1)
      snprintf(out + used, outsz - used, "%.*s", (int)(outsz - used - 1), work);
    out[outsz - 1] = '\0';
  } else {
    snprintf(out, outsz, "%.*s", (int)outsz - 1, work);
  }
  if (ai_debug)
    ai_debug_log("VOICE vnum=%d role=%s tier=%d rhythm=%d tic=%d mbti=%s out=%s", GET_MOB_VNUM(mob), ai_role_name_local(role), vocab_tier, vp_rhythm, vp_tic_index, ai_mbti_string(vp), out);
}


static const char *const syn_greeting[] = {"Greetings", "Hello", "Well met", "Good day", NULL};
static const char *const syn_notice[] = {"friend", "traveler", "neighbor", "folk", NULL};
static const char *const syn_tone_guard[] = {"Keep the peace", "Stay alert", "Walk steady", NULL};
static const char *const syn_tone_inn[] = {"Rest easy", "Warm yourself", "Take a calm breath", NULL};
static const char *const syn_tone_merch[] = {"Trade fair", "Keep your coin close", "Mind the market", NULL};
static const char *const syn_tone_bandit[] = {"Watch your purse", "Keep your wits", "Step careful", NULL};
static const char *const syn_weather[] = {"weather", "skies", "wind", "clouds", NULL};
static const char *const syn_weather_state[] = {"shifts quickly", "turns by the hour", "never stays still", NULL};
static const char *const syn_weather_action[] = {"Carry a cloak", "Plan your road", "Keep dry if you can", NULL};

static const struct ai_synonym_group ai_synonyms_common[] = {
  {"GREETING", syn_greeting},
  {"NOTICE", syn_notice},
  {"WEATHER", syn_weather},
  {"WSTATE", syn_weather_state},
  {"WACTION", syn_weather_action},
  {NULL, NULL}
};

static const struct ai_synonym_group ai_synonyms_guard[] = {
  {"TONE", syn_tone_guard},
  {NULL, NULL}
};

static const struct ai_synonym_group ai_synonyms_inn[] = {
  {"TONE", syn_tone_inn},
  {NULL, NULL}
};

static const struct ai_synonym_group ai_synonyms_merchant[] = {
  {"TONE", syn_tone_merch},
  {NULL, NULL}
};

static const struct ai_synonym_group ai_synonyms_bandit[] = {
  {"TONE", syn_tone_bandit},
  {NULL, NULL}
};

static const struct ai_reply_template ai_reply_templates[] = {
  {1001, ROLE_GUARD, AI_TEMPLATE_INTENT_SMALLTALK, "{GREETING}, {NOTICE}. {TONE}."},
  {1002, ROLE_GUARD, AI_TEMPLATE_INTENT_SMALLTALK, "{GREETING}. {TONE}, and mind the crossings."},
  {1003, ROLE_GUARD, AI_TEMPLATE_INTENT_WEATHER, "The {WEATHER} {WSTATE}; {WACTION}."},
  {1004, ROLE_GUARD, AI_TEMPLATE_INTENT_WEATHER, "Around here the {WEATHER} {WSTATE}. {TONE}."},
  {1101, ROLE_MERCHANT, AI_TEMPLATE_INTENT_SMALLTALK, "{GREETING}, {NOTICE}. {TONE}."},
  {1102, ROLE_MERCHANT, AI_TEMPLATE_INTENT_SMALLTALK, "{GREETING}. {TONE}, and may business be steady."},
  {1103, ROLE_MERCHANT, AI_TEMPLATE_INTENT_WEATHER, "When the {WEATHER} {WSTATE}, carts slow down. {WACTION}."},
  {1104, ROLE_MERCHANT, AI_TEMPLATE_INTENT_WEATHER, "The {WEATHER} {WSTATE}; {TONE}."},
  {1201, ROLE_BANDIT, AI_TEMPLATE_INTENT_SMALLTALK, "{GREETING}. {TONE}."},
  {1202, ROLE_BANDIT, AI_TEMPLATE_INTENT_SMALLTALK, "{NOTICE}, {TONE} out here."},
  {1203, ROLE_BANDIT, AI_TEMPLATE_INTENT_WEATHER, "The {WEATHER} {WSTATE}. {TONE}."},
  {1204, ROLE_BANDIT, AI_TEMPLATE_INTENT_WEATHER, "Bad {WEATHER}? Good cover. {WACTION}."},
  {1301, ROLE_BOSS, AI_TEMPLATE_INTENT_SMALLTALK, "{GREETING}. Keep formation and keep focus."},
  {1302, ROLE_BOSS, AI_TEMPLATE_INTENT_WEATHER, "If the {WEATHER} {WSTATE}, adjust your route accordingly."},
  {1401, ROLE_CIVILIAN, AI_TEMPLATE_INTENT_SMALLTALK, "{GREETING}. Hope your road stays kind."},
  {1402, ROLE_CIVILIAN, AI_TEMPLATE_INTENT_WEATHER, "Seems the {WEATHER} {WSTATE} today."},
  {-1, 0, 0, NULL}
};

static const char *ai_synonym_pick(const struct ai_synonym_group *groups, const char *token, unsigned long seed, int slot)
{
  int i;

  if (!token)
    return "";

  if (groups) {
    for (i = 0; groups[i].token; i++) {
      if (!strcmp(groups[i].token, token)) {
        int n = 0;
        while (groups[i].words && groups[i].words[n]) n++;
        if (n <= 0)
          break;
        return groups[i].words[(seed + (unsigned long)(slot * 13)) % (unsigned long)n];
      }
    }
  }

  for (i = 0; ai_synonyms_common[i].token; i++) {
    if (!strcmp(ai_synonyms_common[i].token, token)) {
      int n = 0;
      while (ai_synonyms_common[i].words && ai_synonyms_common[i].words[n]) n++;
      if (n <= 0)
        break;
      return ai_synonyms_common[i].words[(seed + (unsigned long)(slot * 17)) % (unsigned long)n];
    }
  }

  return "";
}

static void ai_template_expand(const char *tpl, const struct ai_synonym_group *groups, unsigned long seed, char *out, size_t outsz)
{
  size_t oi = 0;
  size_t i = 0;
  int slot = 0;

  if (!out || outsz == 0)
    return;
  out[0] = '\0';
  if (!tpl)
    return;

  while (tpl[i] != '\0' && oi + 1 < outsz) {
    if (tpl[i] == '{') {
      char token[32];
      size_t ti = 0;
      const char *word;
      i++;
      while (tpl[i] && tpl[i] != '}' && ti + 1 < sizeof(token))
        token[ti++] = tpl[i++];
      token[ti] = '\0';
      if (tpl[i] == '}')
        i++;
      word = ai_synonym_pick(groups, token, seed, slot++);
      if (word) {
        size_t wi;
        for (wi = 0; word[wi] != '\0' && oi + 1 < outsz; wi++)
          out[oi++] = word[wi];
      }
      continue;
    }
    out[oi++] = tpl[i++];
  }
  out[oi] = '\0';
}

static int ai_template_pick_index(const int *ids, int count, unsigned long seed, int avoid_id, int avoid_prev, int avoid_recent1, int avoid_recent2)
{
  int start;
  int pass;
  int i;

  if (!ids || count <= 0)
    return -1;

  start = (int)(seed % (unsigned long)count);
  for (pass = 0; pass < 2; pass++) {
    for (i = 0; i < count; i++) {
      int idx = (start + i) % count;
      int id = ids[idx];
      if (pass == 0) {
        if (id == avoid_prev || id == avoid_id || id == avoid_recent1 || id == avoid_recent2)
          continue;
      } else {
        if (id == avoid_prev)
          continue;
      }
      return idx;
    }
  }

  return start;
}

static const char *ai_template_reply_for_intent(struct char_data *mob, int intent, const char *text, int avoid_template_id, int *out_template_id)
{
  static char out[AI_TEMPLATE_BUFFER_MAX];
  struct ai_conv_reply_state *st;
  int role;
  int mapped_intent = 0;
  int candidate_idx[16];
  int candidate_count = 0;
  int chosen_slot;
  int i;
  unsigned long seed;
  const struct ai_reply_template *t;
  const struct ai_synonym_group *role_groups = NULL;

  if (!mob || !mob->ai_prof)
    return NULL;

  if (intent == AI_INTENT_SMALLTALK)
    mapped_intent = ai_is_weather_smalltalk(text) ? AI_TEMPLATE_INTENT_WEATHER : AI_TEMPLATE_INTENT_SMALLTALK;
  else
    return NULL;

  role = mob->ai_prof->role;
  if (role == ROLE_MERCHANT && mob->ai_prof->style == 1)
    role_groups = ai_synonyms_inn;
  else if (role == ROLE_GUARD)
    role_groups = ai_synonyms_guard;
  else if (role == ROLE_MERCHANT)
    role_groups = ai_synonyms_merchant;
  else if (role == ROLE_BANDIT)
    role_groups = ai_synonyms_bandit;

  for (i = 0; ai_reply_templates[i].id >= 0; i++) {
    if (ai_reply_templates[i].intent != mapped_intent)
      continue;
    if (ai_reply_templates[i].role != role && ai_reply_templates[i].role != ROLE_CIVILIAN)
      continue;
    if (candidate_count < (int)(sizeof(candidate_idx) / sizeof(candidate_idx[0])))
      candidate_idx[candidate_count++] = i;
  }

  if (candidate_count <= 0)
    return NULL;

  st = ai_conv_reply_state_get(mob, 1);
  if (!st)
    return NULL;

  st->counter++;
  seed = ai_conv_seed(mob, mapped_intent, st->counter);
  seed = ai_hash_mix(seed, ai_hash_text_stable(text));

  {
    int ids[16];
    for (i = 0; i < candidate_count; i++)
      ids[i] = ai_reply_templates[candidate_idx[i]].id;
    chosen_slot = ai_template_pick_index(ids, candidate_count, seed, avoid_template_id,
                                         st->last_template_ids[0], st->last_template_ids[1], st->last_template_ids[2]);
    if (chosen_slot < 0)
      return NULL;
  }

  t = &ai_reply_templates[candidate_idx[chosen_slot]];
  ai_template_expand(t->text, role_groups, seed, out, sizeof(out));
  if (!out[0])
    return NULL;

  st->last_template_ids[2] = st->last_template_ids[1];
  st->last_template_ids[1] = st->last_template_ids[0];
  st->last_template_ids[0] = t->id;
  st->last_intent = mapped_intent;
  st->updated_at = time(0);
  if (out_template_id)
    *out_template_id = t->id;
  return out;
}

static const char *ai_pick_weighted_phrase(const char *const *normal_pool, const char *const *rare_pool)
{
  if (rare_pool && rand_number(1, 100) <= 5)
    return ai_pick_phrase(rare_pool);
  return ai_pick_phrase(normal_pool);
}

static int ai_is_gibberish(const char *text)
{
  int total = 0, letters = 0, vowels = 0, non_alnum = 0;
  int one_letter_tokens = 0;
  int token_len = 0;
  const unsigned char *p;

  if (!text)
    return FALSE;

  for (p = (const unsigned char *)text; *p; p++) {
    if (!isspace(*p))
      total++;
    if (isalpha(*p)) {
      char c = (char)tolower(*p);
      letters++;
      if (c == 'a' || c == 'e' || c == 'i' || c == 'o' || c == 'u')
        vowels++;
      token_len++;
    } else {
      if (!isalnum(*p) && !isspace(*p))
        non_alnum++;
      if (token_len == 1)
        one_letter_tokens++;
      token_len = 0;
    }
  }
  if (token_len == 1)
    one_letter_tokens++;

  if (total < 6)
    return FALSE;
  if (letters * 100 < total * 35)
    return TRUE;
  if (letters >= 6 && vowels * 100 < letters * 12)
    return TRUE;
  if (non_alnum * 100 > total * 45)
    return TRUE;
  if (one_letter_tokens >= 3)
    return TRUE;

  return FALSE;
}

static int ai_room_name_matches(room_rnum r, const char *const *needles)
{
  int i;
  if (r == NOWHERE || !needles)
    return FALSE;

  for (i = 0; needles[i]; i++) {
    if (ai_text_has_sub_ci(world[r].name, needles[i]))
      return TRUE;
  }
  return FALSE;
}

static int ai_bfs_find_target_room(room_rnum start, int max_depth, const char *const *needles,
                                   room_rnum *out_room, int *out_first_dir, int *out_depth)
{
  room_rnum q_room[AI_BFS_QUEUE_MAX];
  int q_depth[AI_BFS_QUEUE_MAX];
  int q_first_dir[AI_BFS_QUEUE_MAX];
  room_rnum visited[AI_BFS_QUEUE_MAX];
  int head = 0, tail = 0, vcount = 0;
  int i;

  if (start == NOWHERE || !needles)
    return FALSE;

  q_room[tail] = start;
  q_depth[tail] = 0;
  q_first_dir[tail] = -1;
  tail++;
  visited[vcount++] = start;

  while (head < tail && tail < AI_BFS_QUEUE_MAX) {
    room_rnum cur = q_room[head];
    int depth = q_depth[head];
    int first = q_first_dir[head];
    int dir;
    head++;

    if (depth > 0 && ai_room_name_matches(cur, needles)) {
      if (out_room)
        *out_room = cur;
      if (out_first_dir)
        *out_first_dir = first;
      if (out_depth)
        *out_depth = depth;
      return TRUE;
    }
    if (depth >= max_depth)
      continue;

    for (dir = 0; dir < DIR_COUNT; dir++) {
      room_rnum to;
      int seen = FALSE;
      if (!world[cur].dir_option[dir])
        continue;
      to = world[cur].dir_option[dir]->to_room;
      if (to == NOWHERE)
        continue;
      for (i = 0; i < vcount; i++) {
        if (visited[i] == to) {
          seen = TRUE;
          break;
        }
      }
      if (seen)
        continue;
      visited[vcount++] = to;
      if (vcount >= AI_BFS_QUEUE_MAX)
        break;

      q_room[tail] = to;
      q_depth[tail] = depth + 1;
      q_first_dir[tail] = (depth == 0) ? dir : first;
      tail++;
      if (tail >= AI_BFS_QUEUE_MAX)
        break;
    }
  }

  return FALSE;
}

static const char *ai_build_route_text(int first_dir, char *out, size_t outsz)
{
  const char *dname = NULL;
  if (!out || outsz == 0)
    return NULL;
  switch (first_dir) {
    case NORTH: dname = "north"; break;
    case EAST: dname = "east"; break;
    case SOUTH: dname = "south"; break;
    case WEST: dname = "west"; break;
    case UP: dname = "up"; break;
    case DOWN: dname = "down"; break;
#ifdef CONFIG_DIAGONAL_DIRS
    case NORTHWEST: dname = "northwest"; break;
    case NORTHEAST: dname = "northeast"; break;
    case SOUTHWEST: dname = "southwest"; break;
    case SOUTHEAST: dname = "southeast"; break;
#endif
    default: return NULL;
  }
  snprintf(out, outsz, "Go %s.", dname);
  return out;
}

static int ai_detect_topic_target_from_text(const char *text)
{
  if (!text || !*text)
    return TARGET_NONE;
  if (ai_text_has_sub_ci(text, "armory") || ai_text_has_sub_ci(text, "weapon") || ai_text_has_sub_ci(text, "sword") || ai_text_has_sub_ci(text, "dagger") || ai_text_has_sub_ci(text, "axe") || ai_text_has_sub_ci(text, "bow") || ai_text_has_sub_ci(text, "mace") || ai_text_has_sub_ci(text, "staff"))
    return TARGET_ARMORY;
  if (ai_text_has_sub_ci(text, "inn") || ai_text_has_sub_ci(text, "room") || ai_text_has_sub_ci(text, "rent") || ai_text_has_sub_ci(text, "sleep") || ai_text_has_sub_ci(text, "rest"))
    return TARGET_INN;
  if (ai_text_has_sub_ci(text, "bank") || ai_text_has_sub_ci(text, "vault") || ai_text_has_sub_ci(text, "deposit") || ai_text_has_sub_ci(text, "withdraw") || ai_text_has_sub_ci(text, "exchange"))
    return TARGET_BANK;
  if (ai_text_has_sub_ci(text, "temple") || ai_text_has_sub_ci(text, "shrine"))
    return TARGET_TEMPLE;
  if (ai_text_has_sub_ci(text, "heal") || ai_text_has_sub_ci(text, "healer") || ai_text_has_sub_ci(text, "cleric") || ai_text_has_sub_ci(text, "cure"))
    return TARGET_HEAL;
  if (ai_text_has_sub_ci(text, "market") || ai_text_has_sub_ci(text, "square") || ai_text_has_sub_ci(text, "bazaar"))
    return TARGET_MARKET;
  if (ai_text_has_sub_ci(text, "food") || ai_text_has_sub_ci(text, "bakery") || ai_text_has_sub_ci(text, "tavern") || ai_text_has_sub_ci(text, "drink") || ai_text_has_sub_ci(text, "hungry") || ai_text_has_sub_ci(text, "eat"))
    return TARGET_BAKERY;
  if (ai_text_has_sub_ci(text, "train") || ai_text_has_sub_ci(text, "guild") || ai_text_has_sub_ci(text, "trainer") || ai_text_has_sub_ci(text, "practice") || ai_text_has_sub_ci(text, "training"))
    return TARGET_TRAINER;
  return TARGET_NONE;
}

static const char *const *ai_needles_for_target(int target)
{
  static const char *const needles_armory[] = {"armory", "weapon", NULL};
  static const char *const needles_inn[] = {"inn", "tavern", NULL};
  static const char *const needles_bank[] = {"bank", "atm", "vault", NULL};
  static const char *const needles_temple[] = {"temple", "shrine", NULL};
  static const char *const needles_market[] = {"market", "square", "bazaar", NULL};
  static const char *const needles_bakery[] = {"bakery", "food", NULL};
  static const char *const needles_trainer[] = {"guild", "training", "practice", NULL};

  switch (target) {
    case TARGET_ARMORY: return needles_armory;
    case TARGET_INN: return needles_inn;
    case TARGET_BANK: return needles_bank;
    case TARGET_TEMPLE:
    case TARGET_HEAL: return needles_temple;
    case TARGET_MARKET: return needles_market;
    case TARGET_BAKERY: return needles_bakery;
    case TARGET_TRAINER: return needles_trainer;
    default: return NULL;
  }
}

static const char *ai_topic_key_name(int target)
{
  switch (target) {
    case TARGET_INN: return "INN";
    case TARGET_BANK: return "BANK";
    case TARGET_TEMPLE: return "TEMPLE";
    case TARGET_MARKET: return "MARKET";
    case TARGET_ARMORY: return "ARMORY";
    case TARGET_BAKERY: return "BAKERY";
    case TARGET_TRAINER: return "TRAINER";
    case TARGET_HEAL: return "HEAL";
    default: return "NONE";
  }
}

static struct ai_zone_knowledge *ai_zone_get_cache(int zone_rnum)
{
  int i;
  struct ai_zone_knowledge *slot = NULL;

  if (zone_rnum < 0)
    return NULL;

  for (i = 0; i < AI_ZONE_CACHE_MAX; i++) {
    if (ai_zone_cache[i].built && ai_zone_cache[i].zone == zone_rnum)
      return &ai_zone_cache[i];
    if (!slot && !ai_zone_cache[i].built)
      slot = &ai_zone_cache[i];
  }

  if (!slot)
    slot = &ai_zone_cache[zone_rnum % AI_ZONE_CACHE_MAX];

  if (!slot->built || slot->zone != zone_rnum) {
    memset(slot, 0, sizeof(*slot));
    slot->zone = zone_rnum;
    ai_zone_build_cache(slot);
  }

  return slot;
}

static void ai_zone_build_cache(struct ai_zone_knowledge *zk)
{
  int i, j;
  room_vnum low, high;

  if (!zk || zk->zone < 0 || zk->zone > top_of_zone_table)
    return;

  low = zone_table[zk->zone].bot;
  high = zone_table[zk->zone].top;

  zk->shop_count = 0;
  zk->room_count = 0;
  zk->item_count = 0;

  for (i = 0; i <= top_shop && zk->shop_count < AI_ZONE_SHOPS_MAX; i++) {
    for (j = 0; SHOP_ROOM(i, j) != NOWHERE; j++) {
      room_rnum rr = real_room(SHOP_ROOM(i, j));
      int sells_mask = 0;
      int k;
      if (rr == NOWHERE || world[rr].zone != zk->zone)
        continue;
      for (k = 0; SHOP_PRODUCT(i, k) != NOTHING; k++) {
        obj_rnum ornum = real_object(SHOP_PRODUCT(i, k));
        if (ornum == NOTHING)
          continue;
        if (GET_OBJ_TYPE(&obj_proto[ornum]) == ITEM_WEAPON)
          sells_mask |= 1;
        if (GET_OBJ_TYPE(&obj_proto[ornum]) == ITEM_ARMOR)
          sells_mask |= 2;
        if (GET_OBJ_TYPE(&obj_proto[ornum]) == ITEM_FOOD || GET_OBJ_TYPE(&obj_proto[ornum]) == ITEM_DRINKCON)
          sells_mask |= 4;
      }
      zk->shops[zk->shop_count].room_vnum = SHOP_ROOM(i, j);
      zk->shops[zk->shop_count].shop_nr = i;
      zk->shops[zk->shop_count].sells_mask = sells_mask;
      zk->shop_count++;
      break;
    }
  }

  for (i = 0; i <= top_of_world && zk->room_count < AI_ZONE_ROOMS_MAX; i++) {
    if (world[i].zone != zk->zone)
      continue;
    zk->rooms[zk->room_count].room_vnum = world[i].number;
    snprintf(zk->rooms[zk->room_count].name, sizeof(zk->rooms[zk->room_count].name), "%.*s", (int)sizeof(zk->rooms[zk->room_count].name)-1, world[i].name ? world[i].name : "room");
    zk->room_count++;
  }

  for (i = 0; i <= top_of_objt && zk->item_count < AI_ZONE_ITEMS_MAX; i++) {
    obj_vnum v = obj_index[i].vnum;
    const char *n;
    size_t tlen = 0;
    if (v < low || v > high)
      continue;
    zk->items[zk->item_count].vnum = v;
    zk->items[zk->item_count].type = GET_OBJ_TYPE(&obj_proto[i]);
    zk->items[zk->item_count].wear = obj_proto[i].obj_flags.wear_flags[0];
    zk->items[zk->item_count].extra_flags = obj_proto[i].obj_flags.extra_flags[0];
    n = obj_proto[i].name ? obj_proto[i].name : "item";
    while (n[tlen] && !isspace((unsigned char)n[tlen]) && tlen + 1 < sizeof(zk->items[zk->item_count].name)) {
      zk->items[zk->item_count].name[tlen] = n[tlen];
      tlen++;
    }
    zk->items[zk->item_count].name[tlen] = '\0';
    zk->item_count++;
    if ((v - low) > 5000)
      break;
  }

  zk->built = 1;
  zk->built_at = time(0);
  if (ai_debug)
    ai_debug_log("AI_ZONE_CACHE_BUILD zone=%d shops=%d rooms=%d items=%d", zk->zone, zk->shop_count, zk->room_count, zk->item_count);
}

static void ai_detect_requested_targets(const char *text, int *targets, int *count)
{
  int n = 0;
  if (!targets || !count)
    return;
  *count = 0;
  if (!text)
    return;
#define AI_REQ_PUSH(t) do { int _x=(t),_i,_f=0; for(_i=0;_i<n;_i++) if(targets[_i]==_x) _f=1; if(!_f && n<AI_REQ_MAX) targets[n++]=_x; } while(0)
  if (ai_text_has_sub_ci(text, "weapon") || ai_text_has_sub_ci(text, "sword") || ai_text_has_sub_ci(text, "axe") || ai_text_has_sub_ci(text, "mace") || ai_text_has_sub_ci(text, "dagger") || ai_text_has_sub_ci(text, "bow") || ai_text_has_sub_ci(text, "staff") || ai_text_has_sub_ci(text, "spear")) AI_REQ_PUSH(TARGET_ARMORY);
  if (ai_text_has_sub_ci(text, "armor") || ai_text_has_sub_ci(text, "shield") || ai_text_has_sub_ci(text, "helm") || ai_text_has_sub_ci(text, "breastplate")) AI_REQ_PUSH(TARGET_ARMORY);
  if (ai_text_has_sub_ci(text, "hungry") || ai_text_has_sub_ci(text, "starving") || ai_text_has_sub_ci(text, "food") || ai_text_has_sub_ci(text, "eat") || ai_text_has_sub_ci(text, "meal") || ai_text_has_sub_ci(text, "ration") || ai_text_has_sub_ci(text, "bread") || ai_text_has_sub_ci(text, "stew")) AI_REQ_PUSH(TARGET_BAKERY);
  if (ai_text_has_sub_ci(text, "heal") || ai_text_has_sub_ci(text, "healer") || ai_text_has_sub_ci(text, "temple") || ai_text_has_sub_ci(text, "potion") || ai_text_has_sub_ci(text, "bandage") || ai_text_has_sub_ci(text, "wounded")) AI_REQ_PUSH(TARGET_HEAL);
  if (ai_text_has_sub_ci(text, "inn") || ai_text_has_sub_ci(text, "lodging") || ai_text_has_sub_ci(text, "sleep") || ai_text_has_sub_ci(text, "bed") || ai_text_has_sub_ci(text, "bedroll") || ai_text_has_sub_ci(text, "tent") || ai_text_has_sub_ci(text, "camping")) AI_REQ_PUSH(TARGET_INN);
  if (ai_text_has_sub_ci(text, "bank") || ai_text_has_sub_ci(text, "deposit") || ai_text_has_sub_ci(text, "withdraw") || ai_text_has_sub_ci(text, "atm") || ai_text_has_sub_ci(text, "teller")) AI_REQ_PUSH(TARGET_BANK);
  if (ai_text_has_sub_ci(text, "train") || ai_text_has_sub_ci(text, "trainer") || ai_text_has_sub_ci(text, "practice") || ai_text_has_sub_ci(text, "guild") || ai_text_has_sub_ci(text, "instructor") || ai_text_has_sub_ci(text, "master")) AI_REQ_PUSH(TARGET_TRAINER);
  if (ai_text_has_sub_ci(text, "market") || ai_text_has_sub_ci(text, "supplies") || ai_text_has_sub_ci(text, "supply") || ai_text_has_sub_ci(text, "gear") || ai_text_has_sub_ci(text, "equipment") || ai_text_has_sub_ci(text, "pack") || ai_text_has_sub_ci(text, "rope") || ai_text_has_sub_ci(text, "torch") || ai_text_has_sub_ci(text, "oil") || ai_text_has_sub_ci(text, "kit")) AI_REQ_PUSH(TARGET_MARKET);
#undef AI_REQ_PUSH
  *count = n;
}

static int ai_is_question_shape(const char *text)
{
  if (!text)
    return FALSE;
  return (strchr(text, '?') != NULL || ai_text_has_sub_ci(text, "where") || !strncasecmp(text, "where", 5) || !strncasecmp(text, "how", 3) || !strncasecmp(text, "can i", 5) || !strncasecmp(text, "what", 4));
}

static int ai_pick_stance_prefix(int role, int style, unsigned long seed, const char **out_prefix)
{
  static const char *const stance_guard[] = {"Listen,", "Right,", "Easy,", "On duty,", NULL};
  static const char *const stance_constable[] = {"Steady,", "Right then,", "Let's be clear,", "I'll help,", NULL};
  static const char *const stance_merchant[] = {"Well then,", "Friend,", "Right,", "Coinwise,", NULL};
  static const char *const stance_innkeeper[] = {"Welcome,", "Easy now,", "You're safe here,", "Traveller,", NULL};
  static const char *const stance_bandit[] = {"Heh,", "Careful,", "Keep it short,", "Well now,", NULL};
  static const char *const stance_cultist[] = {"Hush,", "Attend,", "Listen close,", "So it turns,", NULL};
  static const char *const stance_spirit[] = {"Wanderer,", "From afar,", "In echoes,", "Hollow one,", NULL};
  static const char *const stance_commander[] = {"Report,", "Concise,", "Understood,", "Proceed,", NULL};
  static const char *const stance_undead[] = {"...", "Speak.", "Few words.", "Go on.", NULL};
  const char *const *pool = NULL;
  int count = 0, pick;

  if (!out_prefix)
    return FALSE;
  *out_prefix = NULL;

  if (role == ROLE_GUARD) pool = (style == 2) ? stance_constable : stance_guard;
  else if (role == ROLE_MERCHANT && style == 1) pool = stance_innkeeper;
  else if (role == ROLE_MERCHANT) pool = stance_merchant;
  else if (role == ROLE_BANDIT) pool = stance_bandit;
  else if (role == ROLE_CULTIST) pool = stance_cultist;
  else if (role == ROLE_SPIRIT) pool = stance_spirit;
  else if (role == ROLE_BOSS) pool = stance_commander;
  else if (role == ROLE_UNDEAD) pool = stance_undead;

  if (!pool)
    return FALSE;
  while (pool[count]) count++;
  if (count <= 0)
    return FALSE;
  pick = (int)(seed % (unsigned long)count);
  *out_prefix = pool[pick];
  return TRUE;
}

static const char *ai_pick_question_mirror_clause(int topic_target, int role, int style, unsigned long seed)
{
  const char *line = NULL;
  (void)seed;
  if (topic_target == TARGET_BAKERY)
    line = "Looking for a meal?";
  else if (topic_target == TARGET_ARMORY)
    line = "Need steel?";
  else if (topic_target == TARGET_HEAL || topic_target == TARGET_TEMPLE)
    line = "Hurt bad?";
  else if (topic_target == TARGET_MARKET)
    line = "Packing up?";
  else if (topic_target == TARGET_TRAINER)
    line = "Want instruction?";
  else if (topic_target == TARGET_INN)
    line = "Need a bed?";
  else if (topic_target == TARGET_NONE)
    line = "Lost?";
  if (!line)
    return NULL;
  if (role == ROLE_CULTIST) {
    if (topic_target == TARGET_NONE) return "Lost in the veil?";
    return "You seek that path?";
  }
  if (role == ROLE_SPIRIT) {
    if (topic_target == TARGET_NONE) return "Lost among echoes?";
    return "You seek among the living?";
  }
  if (role == ROLE_BANDIT && topic_target == TARGET_ARMORY)
    return "Need steel, mark?";
  if (role == ROLE_MERCHANT && style == 1 && topic_target == TARGET_BAKERY)
    return "Looking for a hot meal?";
  return line;
}

static const char *ai_role_because_clause(int role, int style)
{
  if (role == ROLE_GUARD && style == 2)
    return "Keeps things orderly.";
  if (role == ROLE_GUARD)
    return "Keeps you out of trouble.";
  if (role == ROLE_MERCHANT && style == 1)
    return "Warms you up.";
  if (role == ROLE_MERCHANT)
    return "Saves you coin.";
  if (role == ROLE_BANDIT)
    return "If you're lucky.";
  if (role == ROLE_CULTIST)
    return "If you dare.";
  if (role == ROLE_SPIRIT)
    return "If the living will sell to you.";
  if (role == ROLE_BOSS)
    return "Good logistics.";
  return NULL;
}

static int ai_line_has_min_service_usefulness(const char *line)
{
  if (!line || !*line)
    return FALSE;
  if (ai_text_has_sub_ci(line, "inn") || ai_text_has_sub_ci(line, "market") || ai_text_has_sub_ci(line, "temple") || ai_text_has_sub_ci(line, "armory") || ai_text_has_sub_ci(line, "bank") || ai_text_has_sub_ci(line, "trainer") || ai_text_has_sub_ci(line, "instructor"))
    return TRUE;
  if (ai_text_has_sub_ci(line, "north") || ai_text_has_sub_ci(line, "south") || ai_text_has_sub_ci(line, "east") || ai_text_has_sub_ci(line, "west") || ai_text_has_sub_ci(line, "down the road") || ai_text_has_sub_ci(line, "near the square") || ai_text_has_sub_ci(line, "head "))
    return TRUE;
  if (ai_text_has_sub_ci(line, "not sure") || ai_text_has_sub_ci(line, "cannot say") || ai_text_has_sub_ci(line, "can't say") || ai_text_has_sub_ci(line, "haven't seen"))
    return TRUE;
  return FALSE;
}

static void ai_append_clause(char *buf, size_t bufsz, const char *clause)
{
  size_t len;
  if (!buf || bufsz == 0 || !clause || !*clause)
    return;
  len = strlen(buf);
  if (len >= bufsz - 1)
    return;
  if (len > 0 && buf[len - 1] != ' ' && buf[len - 1] != '.' && buf[len - 1] != '!' && buf[len - 1] != '?')
    snprintf(buf + len, bufsz - len, ". %s", clause);
  else if (len > 0 && buf[len - 1] != ' ')
    snprintf(buf + len, bufsz - len, " %s", clause);
  else
    snprintf(buf + len, bufsz - len, "%s", clause);
}

static void ai_service_cache_add_candidate(struct ai_service_zone_cache *zc, int service_type, room_vnum room, int shop_nr, obj_vnum item_vnum, int has_weapon, int has_armor, int has_food)
{
  int i;
  struct ai_service_candidate *c;

  if (!zc || room <= 0)
    return;

  for (i = 0; i < zc->candidate_count; i++) {
    c = &zc->candidates[i];
    if (c->service_type == service_type && c->room_vnum == room) {
      if (c->shop_nr < 0 && shop_nr >= 0)
        c->shop_nr = shop_nr;
      if (c->sample_item_vnum < 0 && item_vnum >= 0)
        c->sample_item_vnum = item_vnum;
      c->has_weapon = c->has_weapon || has_weapon;
      c->has_armor = c->has_armor || has_armor;
      c->has_food = c->has_food || has_food;
      return;
    }
  }

  if (zc->candidate_count >= AI_SERVICE_CANDIDATES_MAX)
    return;

  c = &zc->candidates[zc->candidate_count++];
  c->service_type = service_type;
  c->room_vnum = room;
  c->shop_nr = shop_nr;
  c->sample_item_vnum = item_vnum;
  c->has_weapon = has_weapon;
  c->has_armor = has_armor;
  c->has_food = has_food;
}

static void ai_service_cache_build_for_zone(int zone_rnum, struct ai_service_zone_cache *zc)
{
  int i, j;
  int counts[TARGET_HEAL + 1];
  memset(counts, 0, sizeof(counts));

  if (!zc || zone_rnum < 0 || zone_rnum > top_of_zone_table)
    return;

  zc->candidate_count = 0;
  zc->zone_rnum = zone_rnum;
  zc->last_built = time(0);
  zc->build_version = ++ai_service_cache_build_version;

  for (i = 0; i <= top_shop; i++) {
    int has_weapon = FALSE, has_armor = FALSE, has_food = FALSE;
    obj_vnum weapon_item = -1, armor_item = -1, food_item = -1;

    for (j = 0; SHOP_PRODUCT(i, j) != NOTHING; j++) {
      obj_rnum ornum = real_object(SHOP_PRODUCT(i, j));
      if (ornum == NOTHING)
        continue;
      if (GET_OBJ_TYPE(&obj_proto[ornum]) == ITEM_WEAPON) {
        has_weapon = TRUE;
        if (weapon_item < 0)
          weapon_item = SHOP_PRODUCT(i, j);
      } else if (GET_OBJ_TYPE(&obj_proto[ornum]) == ITEM_ARMOR) {
        has_armor = TRUE;
        if (armor_item < 0)
          armor_item = SHOP_PRODUCT(i, j);
      } else if (GET_OBJ_TYPE(&obj_proto[ornum]) == ITEM_FOOD || GET_OBJ_TYPE(&obj_proto[ornum]) == ITEM_DRINKCON) {
        has_food = TRUE;
        if (food_item < 0)
          food_item = SHOP_PRODUCT(i, j);
      }
    }

    for (j = 0; SHOP_ROOM(i, j) != NOWHERE; j++) {
      room_rnum rr = real_room(SHOP_ROOM(i, j));
      if (rr == NOWHERE || world[rr].zone != zone_rnum)
        continue;
      if (has_weapon || has_armor || has_food)
        ai_service_cache_add_candidate(zc, TARGET_MARKET, SHOP_ROOM(i, j), i, (weapon_item >= 0 ? weapon_item : (armor_item >= 0 ? armor_item : food_item)), has_weapon, has_armor, has_food);
      if (has_weapon || has_armor)
        ai_service_cache_add_candidate(zc, TARGET_ARMORY, SHOP_ROOM(i, j), i, (weapon_item >= 0 ? weapon_item : armor_item), has_weapon, has_armor, has_food);
      if (has_food)
        ai_service_cache_add_candidate(zc, TARGET_BAKERY, SHOP_ROOM(i, j), i, food_item, has_weapon, has_armor, has_food);
    }
  }

  for (i = 0; i <= top_of_world; i++) {
    if (world[i].zone != zone_rnum)
      continue;

    if (ai_text_has_sub_ci(world[i].name, "bank") || ai_text_has_sub_ci(world[i].name, "vault") || ai_text_has_sub_ci(world[i].name, "atm"))
      ai_service_cache_add_candidate(zc, TARGET_BANK, world[i].number, -1, -1, FALSE, FALSE, FALSE);
    if (ai_text_has_sub_ci(world[i].name, "inn") || ai_text_has_sub_ci(world[i].name, "tavern") || ai_text_has_sub_ci(world[i].name, "reception"))
      ai_service_cache_add_candidate(zc, TARGET_INN, world[i].number, -1, -1, FALSE, FALSE, FALSE);
    if (ai_text_has_sub_ci(world[i].name, "healer") || ai_text_has_sub_ci(world[i].name, "temple") || ai_text_has_sub_ci(world[i].name, "shrine")) {
      ai_service_cache_add_candidate(zc, TARGET_HEAL, world[i].number, -1, -1, FALSE, FALSE, FALSE);
      ai_service_cache_add_candidate(zc, TARGET_TEMPLE, world[i].number, -1, -1, FALSE, FALSE, FALSE);
    }
    if (ai_text_has_sub_ci(world[i].name, "guild") || ai_text_has_sub_ci(world[i].name, "training") || ai_text_has_sub_ci(world[i].name, "practice"))
      ai_service_cache_add_candidate(zc, TARGET_TRAINER, world[i].number, -1, -1, FALSE, FALSE, FALSE);
  }

  if (ai_debug)
    ai_debug_log("AI_SERVICE_CACHE_BUILD zone=%d version=%lu candidates=%d", zone_rnum, zc->build_version, zc->candidate_count);
  for (i = 0; i < zc->candidate_count; i++) {
    if (zc->candidates[i].service_type >= TARGET_NONE && zc->candidates[i].service_type <= TARGET_HEAL)
      counts[zc->candidates[i].service_type]++;
  }
  if (ai_debug) {
    ai_debug_log("AI_SERVICE_COUNTS zone=%d armory=%d bank=%d inn=%d heal=%d trainer=%d bakery=%d market=%d temple=%d",
      zone_rnum, counts[TARGET_ARMORY], counts[TARGET_BANK], counts[TARGET_INN], counts[TARGET_HEAL], counts[TARGET_TRAINER], counts[TARGET_BAKERY], counts[TARGET_MARKET], counts[TARGET_TEMPLE]);
  }
}

static struct ai_service_zone_cache *ai_service_cache_get_zone(struct char_data *mob, int force_rebuild)
{
  int zone_rnum;
  int i;
  struct ai_service_zone_cache *slot = NULL;
  time_t now = time(0);

  if (!mob || IN_ROOM(mob) == NOWHERE)
    return NULL;

  zone_rnum = world[IN_ROOM(mob)].zone;
  for (i = 0; i < AI_SERVICE_CACHE_MAX_ZONES; i++) {
    if (ai_service_zone_cache[i].in_use && ai_service_zone_cache[i].zone_rnum == zone_rnum) {
      slot = &ai_service_zone_cache[i];
      break;
    }
    if (!slot && !ai_service_zone_cache[i].in_use)
      slot = &ai_service_zone_cache[i];
  }

  if (!slot)
    slot = &ai_service_zone_cache[zone_rnum % AI_SERVICE_CACHE_MAX_ZONES];

  if (!slot->in_use || force_rebuild || (now - slot->last_built) > AI_SERVICE_CACHE_TTL_SECS) {
    if (ai_debug)
      ai_debug_log("AI_SERVICE_CACHE_REBUILD zone=%d reason=%s", zone_rnum,
        (!slot->in_use ? "miss" : (force_rebuild ? "forced" : "ttl")));
    memset(slot, 0, sizeof(*slot));
    slot->in_use = TRUE;
    ai_service_cache_build_for_zone(zone_rnum, slot);
  }

  return slot;
}

static int ai_zone_candidate_matches(const struct ai_service_candidate *c, int service_type)
{
  if (!c)
    return FALSE;
  if (c->service_type == service_type)
    return TRUE;
  if (service_type == TARGET_ARMORY && c->service_type == TARGET_MARKET)
    return c->has_weapon || c->has_armor;
  if (service_type == TARGET_BAKERY && c->service_type == TARGET_MARKET)
    return c->has_food;
  return FALSE;
}

static int ai_find_route_to_room(room_rnum start, room_rnum target, int max_depth, int *out_first_dir, int *out_distance)
{
  const char *needles[2];
  room_rnum found = NOWHERE;
  int first_dir = -1;
  int depth = 0;

  if (start == NOWHERE || target == NOWHERE || !VALID_ROOM_RNUM(target))
    return FALSE;

  needles[0] = world[target].name;
  needles[1] = NULL;
  if (!ai_bfs_find_target_room(start, max_depth, needles, &found, &first_dir, &depth))
    return FALSE;
  if (found != target)
    return FALSE;

  if (out_first_dir)
    *out_first_dir = first_dir;
  if (out_distance)
    *out_distance = depth;
  return TRUE;
}

static int ai_find_closest_service(struct char_data *mob, struct char_data *player, int service_type, int *out_room_vnum, int *out_item_vnum)
{
  struct ai_service_zone_cache *zc;
  int i;
  int best_dist = 9999;
  room_vnum best_room = NOWHERE;
  obj_vnum best_item = -1;

  if (!mob || IN_ROOM(mob) == NOWHERE)
    return FALSE;

  zc = ai_service_cache_get_zone(mob, FALSE);
  if (!zc)
    return FALSE;

  for (i = 0; i < zc->candidate_count; i++) {
    const struct ai_service_candidate *c = &zc->candidates[i];
    room_rnum rr;
    int dist = 0;
    int first_dir = -1;
    obj_vnum item_choice = c->sample_item_vnum;

    if (!ai_zone_candidate_matches(c, service_type))
      continue;

    rr = real_room(c->room_vnum);
    if (rr == NOWHERE)
      continue;

    if (!ai_find_route_to_room(IN_ROOM(mob), rr, AI_BFS_MAX_DEPTH, &first_dir, &dist))
      continue;

    if (c->shop_nr >= 0 && player) {
      int j;
      item_choice = -1;
      for (j = 0; SHOP_PRODUCT(c->shop_nr, j) != NOTHING; j++) {
        obj_rnum ornum = real_object(SHOP_PRODUCT(c->shop_nr, j));
        struct obj_data *o;
        if (ornum == NOTHING)
          continue;
        o = &obj_proto[ornum];
        if (service_type == TARGET_ARMORY && GET_OBJ_TYPE(o) != ITEM_WEAPON && GET_OBJ_TYPE(o) != ITEM_ARMOR)
          continue;
        if (service_type == TARGET_BAKERY && GET_OBJ_TYPE(o) != ITEM_FOOD && GET_OBJ_TYPE(o) != ITEM_DRINKCON)
          continue;
        if (invalid_align(player, o) || invalid_class(player, o))
          continue;
        item_choice = SHOP_PRODUCT(c->shop_nr, j);
        break;
      }
    }

    if (dist < best_dist) {
      best_dist = dist;
      best_room = c->room_vnum;
      best_item = item_choice;
    }
  }

  if (best_room == NOWHERE)
    return FALSE;

  if (out_room_vnum)
    *out_room_vnum = best_room;
  if (out_item_vnum)
    *out_item_vnum = best_item;

  if (ai_debug)
    ai_debug_log("AI_SERVICE_SELECT zone=%d service=%s room=%d dist=%d item=%d", world[IN_ROOM(mob)].zone, ai_topic_key_name(service_type), best_room, best_dist, best_item);

  return TRUE;
}

static int ai_classify_domain(const char *normalized, int intent, int speech_act)
{
  int scores[DOMAIN_PERSONAL + 1];
  int i, best = DOMAIN_NONE, bestv = 0;

  static const char *const shopping[] = {"buy","sword","weapon","armor","dagger","shield","axe","mace","bow","equipment","gear","purchase","shop","sell",NULL};
  static const char *const services[] = {"bank","inn","heal","healer","train","temple","rest","room","deposit","withdraw",NULL};
  static const char *const directions[] = {"where","how do i get","which way","find","locate","nearest","closest",NULL};
  static const char *const rumor[] = {"heard","rumor","story","news","word","whisper","legend","tale",NULL};
  static const char *const quest[] = {"quest","mission","work","job","task","bounty","help","need someone",NULL};
  static const char *const law[] = {"guard","crime","arrest","law","constable","trouble","stolen","thief",NULL};
  static const char *const social[] = {"hello","hi","greet","how are","whats up","good day","how goes",NULL};
  static const char *const personal[] = {"name","age","where from","who are you","origin","tell me about yourself",NULL};
  const char *const *pools[] = {NULL, services, shopping, directions, rumor, quest, law, social, personal};

  memset(scores, 0, sizeof(scores));
  for (i = DOMAIN_SERVICES; i <= DOMAIN_PERSONAL; i++) {
    int j;
    for (j = 0; pools[i] && pools[i][j]; j++) {
      if (normalized && ai_text_has_sub_ci(normalized, pools[i][j]))
        scores[i] += 3;
    }
  }

  if (intent == AI_INTENT_BUY_WEAPON || intent == AI_INTENT_BUY_ARMOR || intent == AI_INTENT_BUY_FOOD)
    scores[DOMAIN_SHOPPING] += 6;
  if (normalized && (ai_text_has_sub_ci(normalized, "hungry") || ai_text_has_sub_ci(normalized, "food") || ai_text_has_sub_ci(normalized, "eat") ||
      ai_text_has_sub_ci(normalized, "meal") || ai_text_has_sub_ci(normalized, "ration") || ai_text_has_sub_ci(normalized, "bread") || ai_text_has_sub_ci(normalized, "stew"))) {
    scores[DOMAIN_SERVICES] += 24;
    scores[DOMAIN_SHOPPING] += 10;
  }
  if (intent == AI_INTENT_BANK || intent == AI_INTENT_INN || intent == AI_INTENT_HEAL || intent == AI_INTENT_TRAIN || intent == AI_INTENT_ASK_SERVICE)
    scores[DOMAIN_SERVICES] += 6;
  if (intent == AI_INTENT_DIRECTIONS)
    scores[DOMAIN_DIRECTIONS] += 6;
  if (intent == AI_INTENT_RUMOR)
    scores[DOMAIN_RUMOR] += 6;
  if (intent == AI_INTENT_QUEST)
    scores[DOMAIN_QUEST] += 6;
  if (speech_act == AI_INTENT_GREET || speech_act == AI_INTENT_SMALLTALK)
    scores[DOMAIN_SOCIAL] += 2;

  for (i = DOMAIN_SERVICES; i <= DOMAIN_PERSONAL; i++) {
    if (scores[i] > bestv) {
      bestv = scores[i];
      best = i;
    }
  }
  return best;
}

static void ai_resolve_world_facts(struct char_data *mob, struct char_data *actor, int topic_target, int domain, const struct ai_context_vector *ctx, struct ai_world_facts *out)
{
  int service_type = TARGET_NONE;
  int room_v = NOWHERE;
  int item_v = -1;
  room_rnum rr;
  int first_dir = -1;
  int dist = 0;
  char route_buf[128];
  const char *route = "";
  struct ai_zone_knowledge *zk = NULL;

  if (!out)
    return;
  memset(out, 0, sizeof(*out));

  if (ctx) {
    if (ctx->zone_danger > 0.7f)
      snprintf(out->zone_status, sizeof(out->zone_status), "%s", "Trouble has been reported nearby.");
    else if (ctx->zone_alert > 0.6f)
      snprintf(out->zone_status, sizeof(out->zone_status), "%s", "The watch is on alert.");
    else if (ctx->zone_danger < 0.2f && ctx->zone_alert < 0.2f)
      snprintf(out->zone_status, sizeof(out->zone_status), "%s", "The area has been quiet.");
    else
      snprintf(out->zone_status, sizeof(out->zone_status), "%s", "Things seem normal enough.");
  }

  if (mob && IN_ROOM(mob) != NOWHERE)
    zk = ai_zone_get_cache(world[IN_ROOM(mob)].zone);

  if (!(domain == DOMAIN_SHOPPING || domain == DOMAIN_SERVICES || domain == DOMAIN_DIRECTIONS)) {
    out->confidence = 0;
    return;
  }

  if (topic_target != TARGET_NONE)
    service_type = topic_target;
  else if (domain == DOMAIN_SERVICES)
    service_type = TARGET_INN;
  else if (domain == DOMAIN_SHOPPING)
    service_type = TARGET_ARMORY;

  if (service_type == TARGET_NONE || !mob) {
    out->confidence = 0;
    return;
  }

  if (!ai_find_closest_service(mob, actor, service_type, &room_v, &item_v)) {
    out->confidence = 0;
    return;
  }

  rr = real_room(room_v);
  if (rr != NOWHERE)
    snprintf(out->service_name, sizeof(out->service_name), "%.*s", (int)sizeof(out->service_name) - 1, world[rr].name ? world[rr].name : "an armory near the market");

  if (item_v >= 0) {
    obj_rnum ornum = real_object(item_v);
    if (ornum != NOTHING)
      snprintf(out->example_item, sizeof(out->example_item), "%.*s", (int)sizeof(out->example_item) - 1, obj_proto[ornum].short_description ? obj_proto[ornum].short_description : "goods");
  }


  if (!out->example_item[0] && zk && zk->item_count > 0) {
    int zi;
    for (zi = 0; zi < zk->item_count; zi++) {
      if ((service_type == TARGET_ARMORY && (zk->items[zi].type == ITEM_WEAPON || zk->items[zi].type == ITEM_ARMOR)) ||
          (service_type == TARGET_BAKERY && (zk->items[zi].type == ITEM_FOOD || zk->items[zi].type == ITEM_DRINKCON))) {
        snprintf(out->example_item, sizeof(out->example_item), "%.*s", (int)sizeof(out->example_item)-1, zk->items[zi].name);
        break;
      }
    }
  }
  if (rr != NOWHERE && ai_find_route_to_room(IN_ROOM(mob), rr, AI_BFS_MAX_DEPTH, &first_dir, &dist)) {
    int maxp = (int)sizeof(out->route_snippet) - 1;
    ai_build_route_text(first_dir, route_buf, sizeof(route_buf));
    route = route_buf;
    if (!route)
      route = "";
    snprintf(out->route_snippet, sizeof(out->route_snippet), "%.*s", maxp, route);
    out->route_snippet[sizeof(out->route_snippet) - 1] = '\0';
    out->confidence = 3;
    out->service_found = 1;
  } else {
    snprintf(out->service_name, sizeof(out->service_name), "%s", "an armory near the market");
    snprintf(out->route_snippet, sizeof(out->route_snippet), "%s", "ask near the market square");
    out->confidence = 2;
  }
}

static void ai_slot_replace(const char *in, const struct ai_world_facts *f, char *out, size_t outsz)
{
  size_t i = 0, oi = 0;

  if (!out || outsz == 0)
    return;
  out[0] = 0;

  while (in && in[i] && oi + 1 < outsz) {
    const char *rep = NULL;
    size_t ri = 0;
    if (!strncmp(&in[i], "{SERVICE}", 9)) {
      rep = (f && f->service_name[0]) ? f->service_name : "somewhere nearby";
      i += 9;
    } else if (!strncmp(&in[i], "{ROUTE}", 7)) {
      rep = (f && f->route_snippet[0]) ? f->route_snippet : "ask around";
      i += 7;
    } else if (!strncmp(&in[i], "{ITEM}", 6)) {
      rep = (f && f->example_item[0]) ? f->example_item : "";
      i += 6;
    } else if (!strncmp(&in[i], "{ZONE}", 6)) {
      rep = (f && f->zone_status[0]) ? f->zone_status : "";
      i += 6;
    }

    if (rep) {
      while (rep[ri] && oi + 1 < outsz)
        out[oi++] = rep[ri++];
      continue;
    }

    out[oi++] = in[i++];
  }
  out[oi] = 0;
}

static void ai_sanitize_unresolved_tokens(const char *in, char *out, size_t outsz)
{
  size_t i = 0, oi = 0;
  int removed = 0;
  int prev_space = 1;

  if (!out || outsz == 0)
    return;
  out[0] = '\0';
  if (!in)
    return;

  while (in[i] && oi + 1 < outsz) {
    if (in[i] == '{') {
      size_t j = i + 1;
      int valid = 1;
      while (in[j] && in[j] != '}') {
        unsigned char c = (unsigned char)in[j];
        if (!(isupper(c) || c == '_' || isdigit(c))) {
          valid = 0;
          break;
        }
        j++;
      }
      if (valid && in[j] == '}' && j > i + 1) {
        size_t tlen = j - i - 1;
        const char *tok = in + i + 1;
        int keep = ((tlen == 7 && !strncmp(tok, "SERVICE", 7)) ||
                    (tlen == 5 && !strncmp(tok, "ROUTE", 5)) ||
                    (tlen == 4 && !strncmp(tok, "ITEM", 4)) ||
                    (tlen == 4 && !strncmp(tok, "ZONE", 4)));
        if (!keep) {
          removed = 1;
          i = j + 1;
          continue;
        }
      }
    }

    if (isspace((unsigned char)in[i])) {
      if (!prev_space && oi + 1 < outsz)
        out[oi++] = ' ';
      prev_space = 1;
      i++;
      continue;
    }

    out[oi++] = in[i++];
    prev_space = 0;
  }

  while (oi > 0 && out[oi - 1] == ' ')
    oi--;
  out[oi] = '\0';

  while (out[0] == ' ' || out[0] == '.' || out[0] == ',') {
    size_t k;
    for (k = 0; out[k]; k++)
      out[k] = out[k + 1];
    if (out[0] != ' ')
      break;
  }

  if (removed && ai_debug)
    ai_debug_log("AI_SANITIZE_TOKENS original='%s' sanitized='%s'", in, out);
}

static const char *ai_epistemic_line(int confidence, int role, int topic_target)
{
  (void)topic_target;
  if (confidence == 3)
    return NULL;
  if (confidence == 2) {
    if (role == ROLE_GUARD) return "There is one nearby but I cannot give you a clean route from here. Ask at the north post.";
    if (role == ROLE_MERCHANT) return "I know there is one nearby but I cannot point you there directly.";
    return "Should be near the market square though I cannot say exactly.";
  }
  if (confidence == 1)
    return "Somewhere in this district but I do not know the exact location.";
  if (role == ROLE_GUARD) return "I do not know. Ask at the garrison or find a constable.";
  if (role == ROLE_MERCHANT) return "Outside my area. Try asking a guard.";
  if (role == ROLE_BANDIT) return "No idea. Not my concern.";
  if (role == ROLE_CULTIST) return "That knowledge is not mine to give.";
  if (role == ROLE_SPIRIT) return "Ask the living. They keep such records.";
  return "I cannot help with that.";
}

static const char *ai_role_redirect_line(int role, int style, int topic_target)
{
  (void)topic_target;
  if (role == ROLE_GUARD)
    return "Ask at the market or the inn. Say what you need.";
  if (role == ROLE_MERCHANT && style != 1)
    return "Not my trade. Try the market stalls.";
  if (role == ROLE_MERCHANT && style == 1)
    return "Ask the guards or merchants outside.";
  if (role == ROLE_BOSS)
    return "Find a guard post and ask plainly.";
  if (role == ROLE_CIVILIAN)
    return "I'm not sure. Maybe ask a guard.";
  return NULL;
}

static int ai_role_can_answer_intent(int role, int style, int intent)
{
  int innkeeper = (role == ROLE_MERCHANT && style == 1);
  if (intent == AI_INTENT_NONE)
    return FALSE;

  if (role == ROLE_BEAST || role == ROLE_UNDEAD || role == ROLE_SPIRIT || role == ROLE_CULTIST)
    return (intent >= AI_INTENT_EMOTE_DANCE && intent <= AI_INTENT_EMOTE_WAVE) || intent == AI_INTENT_GREET || intent == AI_INTENT_THREAT || intent == AI_INTENT_INSULT || intent == AI_INTENT_EMOTE_SPIT;

  if (role == ROLE_BANDIT)
    return (intent == AI_INTENT_GREET || intent == AI_INTENT_INSULT || intent == AI_INTENT_THREAT || intent == AI_INTENT_EMOTE_SPIT || intent >= AI_INTENT_EMOTE_DANCE || intent == AI_INTENT_SMALLTALK);

  if (role == ROLE_GUARD)
    return intent == AI_INTENT_DIRECTIONS || intent == AI_INTENT_BANK || intent == AI_INTENT_INN || intent == AI_INTENT_HEAL || intent == AI_INTENT_QUEST || intent == AI_INTENT_SMALLTALK || intent == AI_INTENT_GIBBERISH || intent == AI_INTENT_GREET || intent == AI_INTENT_CONFUSION || intent == AI_INTENT_ASK_SERVICE;

  if (role == ROLE_MERCHANT && innkeeper)
    return intent == AI_INTENT_DIRECTIONS || intent == AI_INTENT_INN || intent == AI_INTENT_BUY_FOOD || intent == AI_INTENT_RUMOR || intent == AI_INTENT_SMALLTALK || intent == AI_INTENT_GIBBERISH || intent == AI_INTENT_GREET || intent == AI_INTENT_CONFUSION || intent == AI_INTENT_ASK_SERVICE;

  if (role == ROLE_MERCHANT)
    return intent == AI_INTENT_DIRECTIONS || intent == AI_INTENT_BUY_WEAPON || intent == AI_INTENT_BUY_ARMOR || intent == AI_INTENT_BUY_FOOD || intent == AI_INTENT_SMALLTALK || intent == AI_INTENT_GIBBERISH || intent == AI_INTENT_GREET || intent == AI_INTENT_CONFUSION || intent == AI_INTENT_ASK_SERVICE;

  if (role == ROLE_BOSS)
    return intent == AI_INTENT_DIRECTIONS || intent == AI_INTENT_QUEST || intent == AI_INTENT_SMALLTALK || intent == AI_INTENT_GIBBERISH || intent == AI_INTENT_GREET;

  if (role == ROLE_CIVILIAN || role == ROLE_UNKNOWN)
    return intent == AI_INTENT_DIRECTIONS || intent == AI_INTENT_RUMOR || intent == AI_INTENT_SMALLTALK || intent == AI_INTENT_GIBBERISH || intent == AI_INTENT_GREET || intent == AI_INTENT_CONFUSION;

  return FALSE;
}

static int ai_role_can_give_directions(int role)
{
  return (role == ROLE_GUARD || role == ROLE_MERCHANT || role == ROLE_BOSS || role == ROLE_CIVILIAN || role == ROLE_UNKNOWN);
}

static int ai_service_type_from_intent_topic(int intent, int topic)
{
  if (intent == AI_INTENT_BUY_WEAPON || intent == AI_INTENT_BUY_ARMOR)
    return TARGET_ARMORY;
  if (intent == AI_INTENT_BUY_FOOD)
    return TARGET_BAKERY;
  if (intent == AI_INTENT_BANK)
    return TARGET_BANK;
  if (intent == AI_INTENT_INN)
    return TARGET_INN;
  if (intent == AI_INTENT_HEAL)
    return TARGET_HEAL;
  if (intent == AI_INTENT_TRAIN)
    return TARGET_TRAINER;
  if (topic != TARGET_NONE)
    return topic;
  return TARGET_MARKET;
}

static const char *ai_service_direction_line(struct char_data *mob, struct char_data *player, int intent, int topic)
{
  static char line[220];
  int room_v = NOWHERE;
  int item_v = -1;
  room_rnum rr;
  int first_dir = -1;
  int dist = 0;
  char route[128];
  char route_trim[96];
  const char *item_name = NULL;
  const char *dest_name = NULL;
  int service_type = ai_service_type_from_intent_topic(intent, topic);

  if (!mob || IN_ROOM(mob) == NOWHERE)
    return NULL;

  if (!ai_find_closest_service(mob, player, service_type, &room_v, &item_v))
    return NULL;

  rr = real_room(room_v);
  if (rr == NOWHERE)
    return NULL;

  if (!ai_find_route_to_room(IN_ROOM(mob), rr, AI_BFS_MAX_DEPTH, &first_dir, &dist))
    return NULL;

  ai_build_route_text(first_dir, route, sizeof(route));
  {
    int maxp = (int)sizeof(route_trim) - 1;
    if (!route[0])
      route[0] = '\0';
    snprintf(route_trim, sizeof(route_trim), "%.*s", maxp, route);
    route_trim[sizeof(route_trim) - 1] = '\0';
  }
  dest_name = world[rr].name;

  if (item_v >= 0) {
    obj_rnum ornum = real_object(item_v);
    if (ornum != NOTHING)
      item_name = obj_proto[ornum].short_description;
  }

  if (item_name && *item_name)
    snprintf(line, sizeof(line), "%s: %s They usually stock %s.", dest_name, route_trim, item_name);
  else
    snprintf(line, sizeof(line), "%s: %s", dest_name, route_trim);

  if (ai_debug)
    ai_debug_log("AI_SERVICE_ROUTE intent=%d topic=%s room=%d route=%s", intent, ai_topic_key_name(service_type), room_v, route_trim);

  return line;
}

static const char *ai_direction_line(struct char_data *mob, int target_topic)
{
  static char line[160];
  room_rnum found = NOWHERE;
  int first_dir = -1;
  const char *const *needles;
  char route[64];
  int role;
  int style;

  if (!mob || !mob->ai_prof || !mob->ai_state)
    return NULL;
  role = mob->ai_prof->role;
  style = mob->ai_prof->style;

  if (!ai_role_can_give_directions(role))
    return NULL;

  if (target_topic == TARGET_NONE)
    target_topic = TARGET_MARKET;

  needles = ai_needles_for_target(target_topic);
  if (needles && IN_ROOM(mob) != NOWHERE && ai_bfs_find_target_room(IN_ROOM(mob), AI_BFS_MAX_DEPTH, needles, &found, &first_dir, NULL)) {
    ai_build_route_text(first_dir, route, sizeof(route));
    if (role == ROLE_GUARD)
      snprintf(line, sizeof(line), "%s Keep your eyes open.", route);
    else if (role == ROLE_MERCHANT && style == 1)
      snprintf(line, sizeof(line), "%s Warm beds once you arrive.", route);
    else if (role == ROLE_MERCHANT)
      snprintf(line, sizeof(line), "%s You'll see the stalls.", route);
    else if (role == ROLE_BOSS)
      snprintf(line, sizeof(line), "%s Stay alert and move with purpose.", route);
    else
      snprintf(line, sizeof(line), "%s That's the best way I know.", route);
    return line;
  }

  if (mob->ai_state->local_topic_mask & AI_TOPIC_MARKET)
    return "Head to the market roads and ask again there.";
  if (mob->ai_state->local_topic_mask & AI_TOPIC_MIDGAARD)
    return "Follow the main roads toward the city square.";
  return "Keep to the main road and ask a guard post.";
}

const char *ai_line_for_intent(struct char_data *mob, struct ai_actor_memory_entry *e, int intent, int attitude, const char *text, enum ai_action_type action, int avoid_template_id, int *out_template_id, const char **out_pool, const char **out_reason)
{
  static char line[300];
  static char voiced[300];
  int innkeeper;
  int role;
  int style;
  int topic = TARGET_NONE;
  int skip_voice = FALSE;
  const char *dir_line = NULL;
  const char *core = NULL;
  const struct ai_voice_profile *vp = NULL;
  unsigned long seed;
  int emote_kind = AI_EMOTE_OTHER;
  int suppress_default_emote = FALSE;

  static const char *const gib_guard[] = {"Slow down and say that clearly.", "I did not catch that. Ask again plain.", NULL};
  static const char *const gib_merch[] = {"I cannot parse that. Ask for wares plainly.", "Try that again with clear words.", NULL};
  static const char *const gib_inn[] = {"Easy now. Ask for room or meal plain.", "I missed that. Say it slow.", NULL};
  static const char *const gib_civ[] = {"I do not understand. Maybe ask a guard.", "Could you say that another way?", NULL};

  static const char *const weather_guard[] = {"The {QUIET} never holds. {WARN}, {PEOPLE}.", "The {ROAD} turns by the hour; {WARN}.", NULL};
  static const char *const weather_inn[] = {"If rain comes, the hearth stays warm at this {PLACE}.", "Bad weather fills my rooms before {TIME}.", NULL};
  static const char *const weather_merch[] = {"The weather shifts prices as much as caravans.", "Dry {ROAD} means better stock by dusk.", NULL};
  static const char *const smalltalk_guard[] = {"Stay {GOOD} and keep {QUIET} on the {ROAD}.", "{QUIET} streets are {GOOD} streets.", NULL};
  static const char *const smalltalk_inn[] = {"A {QUIET} table helps {PEOPLE} breathe easier.", "Long {ROAD}s make short talks welcome.", NULL};
  static const char *const smalltalk_merch[] = {"Steady crowds make steady {WORK}.", "{GOOD} mood, {GOOD} market.", NULL};
  static const char *const personal_guard[] = {"Keep your mind on the {ROAD} and your heart steady.", "That's personal. Keep choices respectful and lawful.", NULL};
  static const char *const personal_inn[] = {"Hearts are complicated; I serve comfort, not gossip.", "Feelings are yours to weigh, but kindness helps.", NULL};
  static const char *const personal_merch[] = {"I trade in goods, not hearts, friend.", "That's personal coin to spend carefully.", NULL};
  static const char *const personal_bandit[] = {"Romance gets {PEOPLE} careless.", "Ask someone softer for heart-talk.", NULL};

  if (!mob || !mob->ai_prof)
    return NULL;
  if (out_pool) *out_pool = "POOL_NONE";
  if (out_reason) *out_reason = "AMBIENT";

  role = mob->ai_prof->role;
  style = mob->ai_prof->style;
  innkeeper = (role == ROLE_MERCHANT && style == 1);
  seed = ai_hash_mix(ai_conv_seed(mob, intent, 0), ai_hash_text_stable(text ? text : ""));
  emote_kind = ai_detect_emote_kind(text);

  if (ai_text_has_sub_ci(text, "love") || ai_text_has_sub_ci(text, "crush") || ai_text_has_sub_ci(text, "romance") || ai_text_has_sub_ci(text, "date") || ai_text_has_sub_ci(text, "pretty") ||
      ai_text_has_sub_ci(text, "weird") || ai_text_has_sub_ci(text, "feel") || ai_text_has_sub_ci(text, "feeling") || ai_text_has_sub_ci(text, "what do you think") || ai_text_has_sub_ci(text, "do you think i") || ai_text_has_sub_ci(text, "am i")) {
    if (out_pool) *out_pool = "POOL_PERSONAL_SMALLTALK";
    if (role == ROLE_GUARD) core = ai_pick_phrase(personal_guard);
    else if (role == ROLE_MERCHANT && innkeeper) core = ai_pick_phrase(personal_inn);
    else if (role == ROLE_MERCHANT) core = ai_pick_phrase(personal_merch);
    else if (role == ROLE_BANDIT) core = ai_pick_phrase(personal_bandit);
    else if (role == ROLE_BOSS) core = "Keep your focus where it matters.";
    else core = "That's personal. Best asked of a close friend.";
    goto finalize;
  }

  if (!ai_role_can_answer_intent(role, style, intent)) {
    core = ai_role_redirect_line(role, style, TARGET_NONE);
    skip_voice = TRUE;
    goto finalize;
  }

  if (intent == AI_INTENT_DIRECTIONS || intent == AI_INTENT_BUY_WEAPON || intent == AI_INTENT_BUY_ARMOR || intent == AI_INTENT_BUY_FOOD ||
      intent == AI_INTENT_INN || intent == AI_INTENT_BANK || intent == AI_INTENT_HEAL || intent == AI_INTENT_TRAIN) {
    topic = ai_detect_topic_target_from_text(text);
    if (topic == TARGET_NONE && e && (time(0) - e->last_topic_time) <= AI_TOPIC_MEMORY_WINDOW_SECS)
      topic = e->last_topic;
    dir_line = ai_service_direction_line(mob, NULL, intent, topic);
    if (!dir_line && topic != TARGET_NONE)
      dir_line = ai_direction_line(mob, topic);
  }

  if (e && topic != TARGET_NONE) {
    e->last_topic = topic;
    e->last_topic_time = time(0);
    snprintf(e->last_topic_key, sizeof(e->last_topic_key), "%s", ai_topic_key_name(topic));
  }

  if (intent == AI_INTENT_SMALLTALK && ai_is_weather_smalltalk(text)) {
    const char *tpl_line = ai_template_reply_for_intent(mob, intent, text, avoid_template_id, out_template_id);
    if (tpl_line && *tpl_line) {
      if (out_pool) *out_pool = "POOL_TEMPLATE_WEATHER";
      core = tpl_line;
      skip_voice = TRUE;
      goto finalize;
    }
    if (out_pool) *out_pool = "POOL_WEATHER";
    if (role == ROLE_GUARD) core = ai_pick_phrase(weather_guard);
    else if (role == ROLE_MERCHANT && innkeeper) core = ai_pick_phrase(weather_inn);
    else if (role == ROLE_MERCHANT) core = ai_pick_phrase(weather_merch);
    else core = "The {QUIET} never holds. {WARN}, {PEOPLE}.";
    goto finalize;
  }

  if (intent == AI_INTENT_SMALLTALK) {
    const char *tpl_line = ai_template_reply_for_intent(mob, intent, text, avoid_template_id, out_template_id);
    if (tpl_line && *tpl_line) {
      if (out_pool) *out_pool = "POOL_TEMPLATE_SMALLTALK";
      core = tpl_line;
      skip_voice = TRUE;
      goto finalize;
    }
    if (out_pool) *out_pool = "POOL_SMALLTALK";
    if (role == ROLE_GUARD) core = ai_pick_phrase(smalltalk_guard);
    else if (role == ROLE_MERCHANT && innkeeper) core = ai_pick_phrase(smalltalk_inn);
    else if (role == ROLE_MERCHANT) core = ai_pick_phrase(smalltalk_merch);
    else if (role == ROLE_BANDIT) core = ai_pick_weighted_phrase(role_bandit_greet, role_rare_bandit);
    else if (role == ROLE_BOSS) core = "Stay sharp. This {WORK} does not sleep.";
    else if (role == ROLE_CIVILIAN || role == ROLE_UNKNOWN) core = "{GREET}.";
    goto finalize;
  }

  if (role == ROLE_GUARD) {
    if (intent == AI_INTENT_GREET) core = ai_pick_weighted_phrase(role_guard_greet, role_rare_guard);
    else if (intent == AI_INTENT_GIBBERISH) core = ai_pick_phrase(gib_guard);
    else if (intent == AI_INTENT_DIRECTIONS || intent == AI_INTENT_HEAL || intent == AI_INTENT_BANK || intent == AI_INTENT_INN || intent == AI_INTENT_QUEST) {
      core = dir_line ? dir_line : ai_pick_phrase(role_guard_service);
      skip_voice = (dir_line != NULL);
    }
  } else if (role == ROLE_MERCHANT) {
    if (innkeeper) {
      if (intent == AI_INTENT_GREET || intent == AI_INTENT_SMALLTALK) core = ai_pick_weighted_phrase(role_innkeeper_greet, role_rare_innkeeper);
      else if (intent == AI_INTENT_GIBBERISH) core = ai_pick_phrase(gib_inn);
      else if (intent == AI_INTENT_INN || intent == AI_INTENT_BUY_FOOD || intent == AI_INTENT_DIRECTIONS || intent == AI_INTENT_RUMOR) {
        core = dir_line ? dir_line : ai_pick_phrase(role_innkeeper_service);
        skip_voice = (dir_line != NULL);
      }
    } else {
      if (intent == AI_INTENT_GREET || intent == AI_INTENT_SMALLTALK) core = ai_pick_weighted_phrase(role_merchant_greet, role_rare_merchant);
      else if (intent == AI_INTENT_GIBBERISH) core = ai_pick_phrase(gib_merch);
      else if (intent == AI_INTENT_BUY_WEAPON || intent == AI_INTENT_BUY_ARMOR || intent == AI_INTENT_BUY_FOOD || intent == AI_INTENT_DIRECTIONS) {
        core = dir_line ? dir_line : ai_pick_phrase(role_merchant_service);
        skip_voice = (dir_line != NULL);
      }
    }
  } else if (role == ROLE_BOSS) {
    if (intent == AI_INTENT_GREET || intent == AI_INTENT_SMALLTALK) core = "Stay sharp. This {WORK} does not sleep.";
    else if (intent == AI_INTENT_GIBBERISH) core = "Collect yourself and speak plainly.";
    else if (intent == AI_INTENT_DIRECTIONS || intent == AI_INTENT_QUEST) {
      core = dir_line ? dir_line : "Find a guard post and ask plainly.";
      skip_voice = (dir_line != NULL);
    }
  } else if (role == ROLE_CIVILIAN || role == ROLE_UNKNOWN) {
    if (intent == AI_INTENT_GREET || intent == AI_INTENT_SMALLTALK) core = "{GREET}.";
    else if (intent == AI_INTENT_GIBBERISH) core = ai_pick_phrase(gib_civ);
    else if (intent == AI_INTENT_DIRECTIONS || intent == AI_INTENT_RUMOR) {
      core = dir_line ? dir_line : "I'm not sure. Maybe ask a guard.";
      skip_voice = (dir_line != NULL);
    }
  } else if (role == ROLE_BANDIT) {
    if (intent == AI_INTENT_GREET || intent == AI_INTENT_SMALLTALK) core = ai_pick_weighted_phrase(role_bandit_greet, role_rare_bandit);
  } else if (role == ROLE_BEAST || role == ROLE_UNDEAD || role == ROLE_SPIRIT || role == ROLE_CULTIST) {
    if (intent == AI_INTENT_GIBBERISH) core = NULL;
  }

  if (!core && intent == AI_INTENT_PRAISE)
    core = "{THANKS}. Keep to the {GOOD} {WORK}.";
  if (!core && (intent == AI_INTENT_INSULT || intent == AI_INTENT_EMOTE_SPIT || intent == AI_INTENT_THREAT))
    core = (attitude < -20) ? "Last warning. Respect the law or leave." : "Mind your tongue and keep the {QUIET}.";
  if (!core && intent >= AI_INTENT_EMOTE_DANCE && (action == AI_ACTION_SPEAK || action == AI_ACTION_SPEAK_WARN)) {
    if (emote_kind == AI_EMOTE_DANCE) {
      if (role == ROLE_GUARD) core = ai_pick_phrase(emote_dance_guard);
      else if (role == ROLE_MERCHANT && innkeeper) core = ai_pick_phrase(emote_dance_innkeeper);
      else if (role == ROLE_MERCHANT) core = ai_pick_phrase(emote_dance_merchant);
      else if (role == ROLE_BANDIT) core = ai_pick_phrase(emote_dance_bandit);
      else if (role == ROLE_BOSS) core = ai_pick_phrase(emote_dance_commander);
      else if (role == ROLE_CULTIST) core = ai_pick_phrase(emote_dance_cultist);
      else if (role == ROLE_SPIRIT) {
        core = ai_pick_phrase(emote_dance_spirit_emote);
        skip_voice = TRUE;
      }
      else if (role == ROLE_BEAST || role == ROLE_UNDEAD)
        suppress_default_emote = TRUE;
      if (core && out_pool) *out_pool = "POOL_EMOTE_ROLE_DANCE";
    } else if (emote_kind == AI_EMOTE_HIGHFIVE || emote_kind == AI_EMOTE_HUG) {
      if (role == ROLE_MERCHANT && innkeeper) core = ai_pick_phrase(emote_affection_innkeeper);
      else if (role == ROLE_GUARD) core = ai_pick_phrase(emote_affection_guard);
      else if (role == ROLE_BANDIT) core = ai_pick_phrase(emote_affection_bandit);
      else if (role == ROLE_CIVILIAN) core = ai_pick_phrase(emote_affection_civilian);
      if (core && out_pool) *out_pool = "POOL_EMOTE_ROLE_AFFECTION";
    } else if (emote_kind == AI_EMOTE_GLARE) {
      if (role == ROLE_GUARD) core = ai_pick_phrase(emote_glare_guard);
      else if (role == ROLE_BANDIT) core = ai_pick_phrase(emote_glare_bandit);
      else if (role == ROLE_SPIRIT) core = ai_pick_phrase(emote_glare_spirit);
      else if (role == ROLE_BOSS) core = ai_pick_phrase(emote_glare_commander);
      if (core && out_pool) *out_pool = "POOL_EMOTE_ROLE_GLARE";
    }
  }

  if (!core && !suppress_default_emote && intent >= AI_INTENT_EMOTE_DANCE) {
    if (role == ROLE_GUARD) core = ai_pick_phrase(role_guard_emote);
    else if (role == ROLE_MERCHANT && innkeeper) core = ai_pick_phrase(role_innkeeper_emote);
    else if (role == ROLE_MERCHANT) core = ai_pick_phrase(role_merchant_emote);
    else if (role == ROLE_BANDIT) core = ai_pick_phrase(role_bandit_emote);
    else if (role == ROLE_BEAST) core = ai_pick_phrase(role_beast_emote);
    else if (role == ROLE_UNDEAD) core = ai_pick_phrase(role_undead_emote);
    else if (role == ROLE_SPIRIT) core = ai_pick_phrase(role_spirit_emote);
    else if (role == ROLE_CULTIST) core = (intent == AI_INTENT_EMOTE_SPIT) ? "Blasphemy has a price." : "Ritual, not revelry.";
  }

  if (!core && intent == AI_INTENT_ASK_SERVICE) {
    core = ai_role_redirect_line(role, style, TARGET_NONE);
    skip_voice = TRUE;
  }

  if (core && !ai_line_is_role_legal(core, role, style))
    core = NULL;

  if (!core && intent == AI_INTENT_CONFUSION) {
    core = "What do you mean?";
  }
  if (!core) {
    core = "I am not sure I follow. Ask me in another way.";
  }

finalize:
  if (skip_voice || !core)
    return core;
  vp = ai_voice_profile_get(mob);
  { struct ai_reply_context lrctx; int p; memset(&lrctx, 0, sizeof(lrctx)); lrctx.speech_act = intent; p = ai_imtb_pick_personality(mob, role, style); lrctx.personality = p; lrctx.profile = ai_imtb_profile_get(p); lrctx.chosen_core = core; ai_voice_assemble(mob, vp, NULL, &lrctx, seed, voiced, sizeof(voiced)); }
  snprintf(line, sizeof(line), "%.*s", (int)sizeof(line) - 1, voiced);
  return line;
}

static int ai_actor_choose_intent(struct char_data *mob, struct char_data **out_target, const char **out_line, int *do_emote, int *do_warn)
{
  struct ai_actor_state *st = mob->ai_state;
  struct ai_actor_profile *pf = mob->ai_prof;
  struct ai_actor_memory_entry *e = NULL;
  struct char_data *target = NULL;
  int crowd = ai_room_crowd_count(IN_ROOM(mob));
  int score_greet = 0, score_social = 0, score_say = 0, score_warn = 0, score_idle = 0, score_flee = 0;

  *out_target = NULL; *out_line = NULL; *do_emote = FALSE; *do_warn = FALSE;
  if (!st || !pf || IN_ROOM(mob) == NOWHERE) return 0;
  if (st->intent_cooldown_pulses > 0) return 0;
  if (st->pending_event_time <= 0 || (time(0) - st->pending_event_time) > 20)
    st->pending_event_type = AI_EVENT_PLAYER_LEAVE;

  if (st->pending_target_idnum > 0)
    target = ai_find_player_by_idnum_room(mob, st->pending_target_idnum);
  if (target)
    e = ai_mem_get_or_create(mob, GET_IDNUM(target));

  score_idle = (pf->role == ROLE_UNKNOWN || pf->role == ROLE_CIVILIAN) ? 20 : 6;
  score_greet = (st->pending_event_type == AI_EVENT_PLAYER_ENTER) ? 26 : 0;
  score_social = (st->pending_event_type == AI_EVENT_PLAYER_EMOTE) ? 24 : 0;
  score_say = (st->pending_event_type == AI_EVENT_PLAYER_SAY) ? 22 : 0;
  score_warn = (st->social_spam_count >= 3) ? 26 : 0;
  score_flee = (pf->morale == MORALE_COWARD || pf->role == ROLE_MERCHANT || pf->role == ROLE_UNKNOWN || pf->role == ROLE_CIVILIAN) ? 8 : 0;

  if (e) {
    score_social += e->attitude / 8;
    score_say += e->attitude / 10;
    if (e->disposition_flags & AI_DISP_ANNOYED_ME) score_warn += 8;
    if (e->disposition_flags & AI_DISP_ATTACKED_ME) score_warn += 12;
  }
  if (crowd >= 6) score_warn += 4;
  if (ROOM_FLAGGED(IN_ROOM(mob), ROOM_PEACEFUL) || ROOM_FLAGGED(IN_ROOM(mob), ROOM_NOMOB)) score_flee = 0;
  if (GET_MAX_HIT(mob) > 0 && (GET_HIT(mob) * 100 / GET_MAX_HIT(mob)) < 30) score_flee += 16;

  if (score_flee >= AI_INTENT_THRESHOLD && !MOB_FLAGGED(mob, MOB_SENTINEL) && GET_POS(mob) == POS_STANDING) {
    *out_line = NULL;
    return 6;
  }
  if (score_warn >= AI_INTENT_THRESHOLD && ai_can_speak_now(mob, time(0))) {
    *out_line = "Enough. Keep order in here.";
    *out_target = target;
    *do_warn = TRUE;
    return 4;
  }
  if (score_social >= AI_INTENT_THRESHOLD && ai_can_speak_now(mob, time(0))) {
    *out_line = (e && e->attitude < -10) ? "Mind yourself." : "Noted.";
    *out_target = target;
    return 2;
  }
  if (score_say >= AI_INTENT_THRESHOLD && ai_can_speak_now(mob, time(0))) {
    *out_target = target;
    if (pf->role == ROLE_GUARD) *out_line = "State your business and keep calm.";
    else if (pf->role == ROLE_MERCHANT) *out_line = ai_pick_phrase(role_merchant_greet);
    else if (pf->role == ROLE_BANDIT) *out_line = ai_pick_phrase(role_bandit_greet);
    else *out_line = (pf->role == ROLE_CIVILIAN || pf->role == ROLE_UNKNOWN) ? NULL : "...";
    return 3;
  }
  if (score_greet >= AI_INTENT_THRESHOLD && ai_can_speak_now(mob, time(0))) {
    *out_target = target;
    if (pf->role == ROLE_GUARD) *out_line = ai_pick_phrase(role_guard_greet);
    else if (pf->role == ROLE_MERCHANT && pf->style == 1) *out_line = ai_pick_phrase(role_innkeeper_greet);
    else if (pf->role == ROLE_MERCHANT) *out_line = ai_pick_phrase(role_merchant_greet);
    else if (pf->role == ROLE_BANDIT) *out_line = ai_pick_phrase(role_bandit_greet);
    else *out_line = (pf->role == ROLE_CIVILIAN || pf->role == ROLE_UNKNOWN) ? NULL : "Well met.";
    return 1;
  }
  if (score_idle >= AI_INTENT_THRESHOLD && !rand_number(0, 14)) {
    *out_line = ai_pick_phrase(role_unknown_idle);
    *do_emote = TRUE;
    return 5;
  }
  return 0;
}

int ai_actor_tick(struct char_data *mob, time_t now)
{
  struct ai_actor_profile *pf;
  struct ai_actor_state *st;
  struct char_data *target = NULL;
  const char *line = NULL;
  int do_emote = FALSE, do_warn = FALSE;
  int intent = AI_INTENT_NONE;
  int has_external_logic;

  if (!mob || !IS_NPC(mob) || !MOB_FLAGGED(mob, MOB_AI_ACTOR) || AFF_FLAGGED(mob, AFF_CHARM))
    return FALSE;
  if (!CONFIG_AI_ACTOR_ENABLED)
    return FALSE;

  if (!mob->ai_prof || !mob->ai_state || !mob->ai_prof->initialized)
    ai_actor_init(mob);
  if (!mob->ai_prof || !mob->ai_state)
    return FALSE;

  pf = mob->ai_prof;
  st = mob->ai_state;
  has_external_logic = (MOB_FLAGGED(mob, MOB_SPEC) || mob->proto_script || mob->script);

  if (st->next_signature_check <= now) {
    uint32_t sig = ai_actor_compute_signature(mob);
    if (sig != pf->signature)
      ai_actor_rebuild_profile(mob);
    st->next_signature_check = now + AI_SIGNATURE_CHECK_SECS + rand_number(0, 3);
    pf = mob->ai_prof;
    st = mob->ai_state;
  }

  if (st->next_tick > now)
    return FALSE;
  ai_state_refresh_local_topics(mob);
  st->next_tick = now + rand_number(2, 4);

  if (st->talk_cooldown_pulses > 0) st->talk_cooldown_pulses = MAX(0, st->talk_cooldown_pulses - (int)(2 * PASSES_PER_SEC));
  if (st->intent_cooldown_pulses > 0) st->intent_cooldown_pulses = MAX(0, st->intent_cooldown_pulses - (int)(2 * PASSES_PER_SEC));
  if (st->social_spam_count > 0 && !rand_number(0, 2)) st->social_spam_count--;

  ai_mem_decay(mob, now);
  ai_mood_spring_update(mob, 1.0f);

  if (ai_last_minute_decay_tick == 0 || (now - ai_last_minute_decay_tick) >= 60) {
    ai_heatmap_decay_tick(now);
    ai_alert_decay_tick(now);
    ai_last_minute_decay_tick = now;
  }

  if (ai_try_emit_pending_reaction_speech(mob, now))
    return TRUE;

  if (ai_conv_try_progress(mob, now))
    return TRUE;

  if (FIGHTING(mob)) {
    if (ai_try_flee_or_surrender(mob, now))
      return TRUE;
    return FALSE;
  }
  if (IN_ROOM(mob) == NOWHERE)
    return FALSE;

  if (ai_conv_try_start(mob, now))
    return TRUE;

  intent = ai_actor_choose_intent(mob, &target, &line, &do_emote, &do_warn);
  if (intent <= 0)
    return FALSE;

  if (intent == 6) {
    if (!has_external_logic && ai_move_random_biased(mob)) {
      st->last_action_time = now;
      st->intent_cooldown_pulses = rand_number(AI_INTENT_COOLDOWN_MIN, AI_INTENT_COOLDOWN_MAX) * PASSES_PER_SEC;
      return TRUE;
    }
    return FALSE;
  }

  if (line && *line) {
    const char *reason = (st->pending_event_type == AI_EVENT_PLAYER_ENTER) ? "ARRIVAL" : "AMBIENT";
    const char *pool = (pf->role == ROLE_CIVILIAN || pf->role == ROLE_UNKNOWN) ? "POOL_GENERIC_AMBIENT" : "POOL_ROLE_AMBIENT";
    ai_set_last_speech_meta(mob, pool, reason);
    if (do_emote)
      do_echo(mob, (char *)(line[0] == '$' ? line + 3 : line), 0, SCMD_EMOTE);
    else if (ai_can_speak_now(mob, now) && st->talk_cooldown_pulses <= 0)
      ai_say(mob, line, now);
    else
      return FALSE;

    st->last_action_time = now;
    if (do_emote) st->last_emote_time = now;
    if (!do_emote) st->last_talk_time = now;
    st->intent_cooldown_pulses = rand_number(AI_INTENT_COOLDOWN_MIN, AI_INTENT_COOLDOWN_MAX) * PASSES_PER_SEC;
    if (!do_emote)
      st->talk_cooldown_pulses = rand_number(AI_TALK_COOLDOWN_MIN, AI_TALK_COOLDOWN_MAX) * PASSES_PER_SEC;
    st->pending_target_idnum = 0;
    st->pending_event_text[0] = '\0';
    st->pending_event_type = AI_EVENT_PLAYER_LEAVE;
    return TRUE;
  }

  return do_warn;
}


void ai_actor_record_damage(struct char_data *mob, struct char_data *actor, int dam)
{
  struct ai_actor_memory_entry *e;

  if (!mob || !actor || !IS_NPC(mob) || IS_NPC(actor) || !MOB_FLAGGED(mob, MOB_AI_ACTOR))
    return;
  if (!mob->ai_prof || !mob->ai_state)
    ai_actor_init(mob);

  e = ai_mem_get_or_create(mob, GET_IDNUM(actor));
  if (!e)
    return;

  e->hostility += MAX(1, dam / 10);
  e->flags |= MEM_ATTACKED_ME;
  e->disposition_flags |= AI_DISP_ATTACKED_ME;
  e->attitude = MAX(-100, e->attitude - MAX(4, dam / 8));
  e->last_update = time(0);
  e->last_seen_time = e->last_update;
  e->last_interaction_time = e->last_update;
  e->last_room_vnum = (IN_ROOM(mob) == NOWHERE) ? NOWHERE : GET_ROOM_VNUM(IN_ROOM(mob));
  e->belief_hostility = ai_clampf(e->belief_hostility + 0.25f, 0.0f, 1.0f);
  e->belief_updated_at = e->last_update;
  ai_actor_brain_on_attacked(mob, actor, dam);
}

void ai_actor_record_help(struct char_data *mob, struct char_data *actor, int amount)
{
  struct ai_actor_memory_entry *e;

  if (!mob || !actor || !IS_NPC(mob) || IS_NPC(actor) || !MOB_FLAGGED(mob, MOB_AI_ACTOR))
    return;
  if (!mob->ai_prof || !mob->ai_state)
    ai_actor_init(mob);

  e = ai_mem_get_or_create(mob, GET_IDNUM(actor));
  if (!e)
    return;

  e->trust += MAX(1, amount / 5);
  e->flags |= MEM_HELPED_ME;
  e->disposition_flags |= AI_DISP_HELPED_ME;
  e->attitude = MIN(100, e->attitude + MAX(3, amount / 4));
  e->last_update = time(0);
  e->last_room_vnum = (IN_ROOM(mob) == NOWHERE) ? NOWHERE : GET_ROOM_VNUM(IN_ROOM(mob));
  e->belief_familiarity = ai_clampf(e->belief_familiarity + 0.08f, 0.0f, 1.0f);
  e->belief_hostility = ai_clampf(e->belief_hostility - 0.03f, 0.0f, 1.0f);
  e->belief_updated_at = e->last_update;
}

void ai_actor_record_crime(struct char_data *mob, struct char_data *criminal, int flags)
{
  struct ai_actor_memory_entry *e;

  if (!mob || !criminal || !IS_NPC(mob) || IS_NPC(criminal) || !MOB_FLAGGED(mob, MOB_AI_ACTOR))
    return;
  if (!mob->ai_prof || !mob->ai_state)
    ai_actor_init(mob);

  e = ai_mem_get_or_create(mob, GET_IDNUM(criminal));
  if (!e)
    return;

  e->hostility += 4;
  e->flags |= (MEM_WANTED | flags);
  e->last_update = time(0);
  e->last_room_vnum = (IN_ROOM(mob) == NOWHERE) ? NOWHERE : GET_ROOM_VNUM(IN_ROOM(mob));
}

void ai_actor_record_room_crime(struct char_data *witness, struct char_data *criminal, int flags)
{
  struct char_data *mob;
  room_rnum room = NOWHERE;

  if (!criminal || IS_NPC(criminal) || IN_ROOM(criminal) == NOWHERE)
    return;

  room = IN_ROOM(criminal);
  if (witness && IN_ROOM(witness) != NOWHERE)
    room = IN_ROOM(witness);

  for (mob = world[room].people; mob; mob = mob->next_in_room) {
    if (!IS_NPC(mob) || !MOB_FLAGGED(mob, MOB_AI_ACTOR))
      continue;
    if (!mob->ai_prof || !mob->ai_prof->initialized)
      ai_actor_init(mob);
    if (!mob->ai_prof || mob->ai_prof->role != ROLE_GUARD)
      continue;
    if (!CAN_SEE(mob, criminal))
      continue;
    ai_actor_record_crime(mob, criminal, flags);
  }
}


static int ai_text_has_sub_ci(const char *hay, const char *needle)
{
  size_t nlen;
  const char *p;

  if (!hay || !needle || !*hay || !*needle)
    return FALSE;

  nlen = strlen(needle);
  for (p = hay; *p; p++) {
    if (!strncasecmp(p, needle, nlen))
      return TRUE;
  }
  return FALSE;
}

static void ai_state_refresh_local_topics(struct char_data *mob)
{
  struct ai_actor_state *st;
  char room_name[MAX_INPUT_LENGTH * 2];
  char zone_name[MAX_INPUT_LENGTH * 2];
  char combo[(MAX_INPUT_LENGTH * 4) + 8];
  int zone;

  if (!mob || !mob->ai_state) return;
  st = mob->ai_state;
  st->local_topic_mask = 0;

  if (IN_ROOM(mob) == NOWHERE)
    return;

  zone = world[IN_ROOM(mob)].zone;
  room_name[0] = '\0';
  zone_name[0] = '\0';
  combo[0] = '\0';

  if (world[IN_ROOM(mob)].name)
    snprintf(room_name, sizeof(room_name), "%s", world[IN_ROOM(mob)].name);
  if (zone >= 0 && zone <= top_of_zone_table && zone_table[zone].name)
    snprintf(zone_name, sizeof(zone_name), "%s", zone_table[zone].name);

  snprintf(combo, sizeof(combo), "%s %s", room_name, zone_name);

  if (ai_text_has_sub_ci(combo, "midgaard")) st->local_topic_mask |= AI_TOPIC_MIDGAARD;
  if (ai_text_has_sub_ci(combo, "temple")) st->local_topic_mask |= AI_TOPIC_TEMPLE;
  if (ai_text_has_sub_ci(combo, "market") || ai_text_has_sub_ci(combo, "bazaar") || ai_text_has_sub_ci(combo, "square")) st->local_topic_mask |= AI_TOPIC_MARKET;
  if (ai_text_has_sub_ci(combo, "inn") || ai_text_has_sub_ci(combo, "tavern")) st->local_topic_mask |= AI_TOPIC_INN;
  if (ai_text_has_sub_ci(combo, "bank") || ai_text_has_sub_ci(combo, "vault") || ai_text_has_sub_ci(combo, "exchange")) st->local_topic_mask |= AI_TOPIC_BANK;
  if (ai_text_has_sub_ci(combo, "alley") || ai_text_has_sub_ci(combo, "backstreet")) st->local_topic_mask |= AI_TOPIC_ALLEY;
  if (ai_text_has_sub_ci(combo, "wild") || ai_text_has_sub_ci(combo, "forest") || ai_text_has_sub_ci(combo, "field") || ai_text_has_sub_ci(combo, "plains")) st->local_topic_mask |= AI_TOPIC_WILDERNESS;
  if (ai_text_has_sub_ci(combo, "dungeon") || ai_text_has_sub_ci(combo, "crypt") || ai_text_has_sub_ci(combo, "cavern")) st->local_topic_mask |= AI_TOPIC_DUNGEON;
  if (ai_text_has_sub_ci(combo, "sewer") || ai_text_has_sub_ci(combo, "drain")) st->local_topic_mask |= AI_TOPIC_SEWER;
  if (ai_text_has_sub_ci(combo, "castle") || ai_text_has_sub_ci(combo, "keep") || ai_text_has_sub_ci(combo, "fort")) st->local_topic_mask |= AI_TOPIC_CASTLE;

  if (st->local_topic_mask == 0)
    st->local_topic_mask = AI_TOPIC_MIDGAARD;
}

static int ai_conv_topic_from_intent(int intent)
{
  if (intent == AI_INTENT_DIRECTIONS)
    return AI_CONV_TOPIC_DIRECTIONS;
  if (intent == AI_INTENT_BANK)
    return AI_CONV_TOPIC_BANK;
  if (intent == AI_INTENT_INN)
    return AI_CONV_TOPIC_INN;
  if (intent == AI_INTENT_QUEST || intent == AI_INTENT_ASK_SERVICE)
    return AI_CONV_TOPIC_HELP;
  if (intent == AI_INTENT_THREAT || intent == AI_INTENT_INSULT || intent == AI_INTENT_EMOTE_SPIT)
    return AI_CONV_TOPIC_THREAT;
  if (intent == AI_INTENT_RUMOR)
    return AI_CONV_TOPIC_RUMOR;
  if (intent == AI_INTENT_SMALLTALK || intent == AI_INTENT_GREET)
    return AI_CONV_TOPIC_SMALLTALK;
  return AI_CONV_TOPIC_UNKNOWN;
}

static int ai_conv_topic_for_pair(struct char_data *a, struct char_data *b)
{
  int ra, rb, sa, sb;

  if (!a || !b || !a->ai_prof || !b->ai_prof)
    return AI_CONV_TOPIC_UNKNOWN;

  ra = a->ai_prof->role;
  rb = b->ai_prof->role;
  sa = ai_role_priority_score(a);
  sb = ai_role_priority_score(b);

  if ((ra == ROLE_GUARD && rb == ROLE_GUARD))
    return (sa + sb > 90) ? AI_CONV_TOPIC_PATROL : AI_CONV_TOPIC_CRIME;
  if ((ra == ROLE_GUARD && rb == ROLE_BANDIT) || (ra == ROLE_BANDIT && rb == ROLE_GUARD))
    return (sa > sb) ? AI_CONV_TOPIC_CRIME : AI_CONV_TOPIC_THREAT;
  if ((ra == ROLE_MERCHANT && a->ai_prof->style != 1 && rb == ROLE_CIVILIAN) ||
      (rb == ROLE_MERCHANT && b->ai_prof->style != 1 && ra == ROLE_CIVILIAN))
    return AI_CONV_TOPIC_SHOP;
  if ((ra == ROLE_MERCHANT && a->ai_prof->style == 1 && rb == ROLE_CIVILIAN) ||
      (rb == ROLE_MERCHANT && b->ai_prof->style == 1 && ra == ROLE_CIVILIAN))
    return AI_CONV_TOPIC_INN;
  if ((ra == ROLE_MERCHANT && ai_mob_has_shop_data(a) && rb == ROLE_CIVILIAN) ||
      (rb == ROLE_MERCHANT && ai_mob_has_shop_data(b) && ra == ROLE_CIVILIAN))
    return AI_CONV_TOPIC_BANK;
  if (ra == ROLE_UNDEAD || rb == ROLE_UNDEAD || ra == ROLE_SPIRIT || rb == ROLE_SPIRIT)
    return (rand_number(0, 1) == 0) ? AI_CONV_TOPIC_RUMOR : AI_CONV_TOPIC_SMALLTALK;
  if (ra == ROLE_BEAST || rb == ROLE_BEAST)
    return AI_CONV_TOPIC_UNKNOWN;
  if (ra == ROLE_GUARD || rb == ROLE_GUARD)
    return AI_CONV_TOPIC_PATROL;
  if (ra == ROLE_MERCHANT || rb == ROLE_MERCHANT)
    return AI_CONV_TOPIC_SHOP;
  return AI_CONV_TOPIC_SMALLTALK;
}

static const char *ai_conv_line_for_topic(struct char_data *speaker, int topic)
{
  static const char *const weather_lines[] = {"Looks like weather's turning again.", "Air feels different today.", NULL};
  static const char *const smalltalk_lines[] = {"Quiet stretch, for now.", "Seen many travelers today?", NULL};
  static const char *const directions_lines[] = {"Main road still fastest through town.", "Square is easiest landmark to follow.", NULL};
  static const char *const shop_lines[] = {"Coin's moving slowly today.", "Stock's better when caravans arrive on time.", NULL};
  static const char *const inn_lines[] = {"Beds fill fast on wet nights.", "Stew's gone before moonrise most nights.", NULL};
  static const char *const bank_lines[] = {"Vault runners were busy this morning.", "People trust locked iron more than luck.", NULL};
  static const char *const help_lines[] = {"Most folk just need clear directions.", "Half the work is calming people down.", NULL};
  static const char *const threat_lines[] = {"Tension's high. Keep your eyes open.", "Trouble starts when tempers do.", NULL};
  static const char *const crime_lines[] = {"Petty theft's up near the alleys.", "Keep reports clear and names exact.", NULL};
  static const char *const patrol_lines[] = {"Routes look calm this watch.", "Keep patrol turns tight and visible.", NULL};
  static const char *const rumor_lines[] = {"Heard whispers from the old road again.", "Rumors travel faster than caravans.", NULL};

  if (speaker && speaker->ai_prof && speaker->ai_prof->role == ROLE_BEAST)
    return ai_pick_phrase(role_beast_emote);

  switch (topic) {
    case AI_CONV_TOPIC_WEATHER: return ai_pick_phrase(weather_lines);
    case AI_CONV_TOPIC_SMALLTALK: return ai_pick_phrase(smalltalk_lines);
    case AI_CONV_TOPIC_DIRECTIONS: return ai_pick_phrase(directions_lines);
    case AI_CONV_TOPIC_SHOP: return ai_pick_phrase(shop_lines);
    case AI_CONV_TOPIC_INN: return ai_pick_phrase(inn_lines);
    case AI_CONV_TOPIC_BANK: return ai_pick_phrase(bank_lines);
    case AI_CONV_TOPIC_HELP: return ai_pick_phrase(help_lines);
    case AI_CONV_TOPIC_THREAT: return ai_pick_phrase(threat_lines);
    case AI_CONV_TOPIC_CRIME: return ai_pick_phrase(crime_lines);
    case AI_CONV_TOPIC_PATROL: return ai_pick_phrase(patrol_lines);
    case AI_CONV_TOPIC_RUMOR: return ai_pick_phrase(rumor_lines);
    default: return ai_pick_phrase(smalltalk_lines);
  }
}

static int ai_conv_emit_line(struct ai_conv_room_state *room_st, struct char_data *speaker, struct char_data *partner, time_t now)
{
  struct ai_conv_actor_state *sst, *pst;
  const char *line;

  if (!room_st || !speaker || !partner)
    return FALSE;

  line = ai_conv_line_for_topic(speaker, room_st->topic);
  if (!line || !*line)
    return FALSE;
  if (!ai_can_speak_now(speaker, now))
    return FALSE;

  ai_set_last_speech_meta(speaker, "POOL_NPC_CONVERSATION", "AMBIENT");
  ai_say(speaker, line, now);

  room_st->line_count++;
  room_st->last_speaker_id = GET_MOB_VNUM(speaker);
  room_st->last_line_time = now;

  sst = ai_conv_actor_state_get(speaker, 1);
  pst = ai_conv_actor_state_get(partner, 1);
  if (sst) {
    sst->current_topic = room_st->topic;
    sst->partner_id = GET_MOB_VNUM(partner);
    sst->last_speaker_id = room_st->last_speaker_id;
    sst->last_line_time = now;
    sst->topic_expires_at = room_st->topic_expires_at;
    sst->depth_counter = room_st->line_count;
    sst->updated_at = now;
  }
  if (pst) {
    pst->current_topic = room_st->topic;
    pst->partner_id = GET_MOB_VNUM(speaker);
    pst->last_speaker_id = room_st->last_speaker_id;
    pst->last_line_time = now;
    pst->topic_expires_at = room_st->topic_expires_at;
    pst->depth_counter = room_st->line_count;
    pst->updated_at = now;
  }

  return TRUE;
}

static int ai_conv_try_progress(struct char_data *mob, time_t now)
{
  struct ai_conv_room_state *room_st;
  struct char_data *speaker, *partner;

  if (!mob || IN_ROOM(mob) == NOWHERE)
    return FALSE;

  room_st = ai_conv_room_state_get(IN_ROOM(mob), 0);
  if (!room_st || !room_st->active)
    return FALSE;

  if (!room_st->speaker_a || !room_st->speaker_b || room_st->speaker_a == room_st->speaker_b) {
    ai_conv_room_end(room_st, now);
    return FALSE;
  }

  if (room_st->line_count >= AI_NPC_CONVO_MAX_LINES || now >= room_st->topic_expires_at) {
    ai_conv_room_end(room_st, now);
    return FALSE;
  }

  if ((now - room_st->last_line_time) < AI_NPC_CONVO_LINE_GAP_SECS)
    return FALSE;
  if ((now - room_st->last_player_speech_time) < AI_ROOM_PLAYER_SPEECH_GRACE_SECS)
    return FALSE;

  speaker = (room_st->last_speaker_id == GET_MOB_VNUM(room_st->speaker_a)) ? room_st->speaker_b : room_st->speaker_a;
  partner = (speaker == room_st->speaker_a) ? room_st->speaker_b : room_st->speaker_a;

  if (IN_ROOM(speaker) != IN_ROOM(partner) || FIGHTING(speaker) || FIGHTING(partner) || GET_POS(speaker) <= POS_SLEEPING || GET_POS(partner) <= POS_SLEEPING) {
    ai_conv_room_end(room_st, now);
    return FALSE;
  }

  if (!ai_conv_emit_line(room_st, speaker, partner, now))
    return FALSE;

  if (room_st->line_count >= AI_NPC_CONVO_MAX_LINES || now >= room_st->topic_expires_at)
    ai_conv_room_end(room_st, now);

  return TRUE;
}

static int ai_conv_try_start(struct char_data *mob, time_t now)
{
  struct ai_conv_room_state *room_st;
  struct ai_conv_actor_state *self_state;
  struct char_data *it, *best = NULL;
  int best_score = -9999;
  int min_start_gap;

  if (!mob || !mob->ai_prof || IN_ROOM(mob) == NOWHERE)
    return FALSE;
  if (ROOM_FLAGGED(IN_ROOM(mob), ROOM_NOMOB) || ROOM_FLAGGED(IN_ROOM(mob), ROOM_PEACEFUL))
    return FALSE;
  if (FIGHTING(mob) || GET_POS(mob) <= POS_SLEEPING || !ai_can_speak_now(mob, now))
    return FALSE;

  room_st = ai_conv_room_state_get(IN_ROOM(mob), 1);
  self_state = ai_conv_actor_state_get(mob, 1);
  if (!room_st || !self_state)
    return FALSE;

  if (room_st->active)
    return FALSE;

  min_start_gap = ai_conv_room_has_player(IN_ROOM(mob)) ? AI_NPC_CONVO_START_WITH_PLAYERS_SECS : AI_NPC_CONVO_START_EMPTY_SECS;
  if ((now - room_st->last_start_time) < min_start_gap)
    return FALSE;
  if ((now - room_st->last_player_speech_time) < AI_ROOM_PLAYER_SPEECH_GRACE_SECS)
    return FALSE;

  if (self_state->partner_id != 0 && now < self_state->topic_expires_at)
    return FALSE;

  for (it = world[IN_ROOM(mob)].people; it; it = it->next_in_room) {
    struct ai_conv_actor_state *other_state;
    int topic;
    int score;

    if (it == mob || !IS_NPC(it) || !MOB_FLAGGED(it, MOB_AI_ACTOR) || !it->ai_prof || !it->ai_state)
      continue;
    if (FIGHTING(it) || GET_POS(it) <= POS_SLEEPING || !ai_can_speak_now(it, now))
      continue;

    other_state = ai_conv_actor_state_get(it, 1);
    if (!other_state)
      continue;
    if (other_state->partner_id != 0 && now < other_state->topic_expires_at)
      continue;

    topic = ai_conv_topic_for_pair(mob, it);
    if (topic == AI_CONV_TOPIC_UNKNOWN)
      continue;

    score = ai_role_priority_score(mob) + ai_role_priority_score(it);
    if (topic == AI_CONV_TOPIC_PATROL || topic == AI_CONV_TOPIC_CRIME)
      score += 8;
    if (topic == AI_CONV_TOPIC_THREAT)
      score += 5;

    if (score > best_score) {
      best_score = score;
      best = it;
    }
  }

  if (!best)
    return FALSE;

  room_st->speaker_a = mob;
  room_st->speaker_b = best;
  room_st->topic = ai_conv_topic_for_pair(mob, best);
  room_st->active = TRUE;
  room_st->line_count = 0;
  room_st->last_line_time = now - AI_NPC_CONVO_LINE_GAP_SECS;
  room_st->topic_expires_at = now + rand_number(AI_NPC_CONVO_TOPIC_MIN_SECS, AI_NPC_CONVO_TOPIC_MAX_SECS);
  room_st->last_start_time = now;

  return ai_conv_emit_line(room_st, mob, best, now);
}

static void ai_normalize_text(const char *src, char *dst, size_t dstsz)
{
  size_t i, j = 0;

  if (!dst || dstsz == 0)
    return;
  dst[0] = '\0';
  if (!src)
    return;

  for (i = 0; src[i] && j + 1 < dstsz; i++) {
    unsigned char c = (unsigned char)src[i];

    if (isalnum(c))
      dst[j++] = (char)tolower(c);
    else if (j > 0 && dst[j - 1] != ' ')
      dst[j++] = ' ';
  }
  if (j > 0 && dst[j - 1] == ' ')
    j--;
  dst[j] = '\0';
}

static int ai_player_speech_classify(const char *text, int *out_confidence, int *out_is_weather)
{
  int scores[AI_SPEECH_FEELING + 1];
  int best = AI_SPEECH_UNKNOWN;
  int i;
  int is_hunger_request;

  memset(scores, 0, sizeof(scores));
  if (out_confidence) *out_confidence = 0;
  if (out_is_weather) *out_is_weather = FALSE;
  if (!text || !*text)
    return AI_SPEECH_UNKNOWN;

  if (ai_text_has_sub_ci(text, "hello") || ai_text_has_sub_ci(text, "hi") || ai_text_has_sub_ci(text, "hey") ||
      ai_text_has_sub_ci(text, "greetings") || ai_text_has_sub_ci(text, "yo") || ai_text_has_sub_ci(text, "sup") ||
      ai_text_has_sub_ci(text, "good morning") || ai_text_has_sub_ci(text, "good afternoon") || ai_text_has_sub_ci(text, "good evening"))
    scores[AI_SPEECH_GREET] += 10;

  if (ai_text_has_sub_ci(text, "weather") || ai_text_has_sub_ci(text, "rain") || ai_text_has_sub_ci(text, "sun") || ai_text_has_sub_ci(text, "storm") ||
      ai_text_has_sub_ci(text, "wind") || ai_text_has_sub_ci(text, "snow") || ai_text_has_sub_ci(text, "fog") || ai_text_has_sub_ci(text, "nice day")) {
    scores[AI_SPEECH_WEATHER] += 12;
    if (out_is_weather) *out_is_weather = TRUE;
  }

  is_hunger_request = (ai_text_has_sub_ci(text, "hungry") || ai_text_has_sub_ci(text, "starving") || ai_text_has_sub_ci(text, "need food") ||
                       ai_text_has_sub_ci(text, "need something to eat") || ai_text_has_sub_ci(text, "something to eat"));

  if (!is_hunger_request && (ai_text_has_sub_ci(text, "how are you") || ai_text_has_sub_ci(text, "what s up") || ai_text_has_sub_ci(text, "whats up") || ai_text_has_sub_ci(text, "what's up") ||
      ai_text_has_sub_ci(text, "how goes") || ai_text_has_sub_ci(text, "how is it going") || ai_text_has_sub_ci(text, "hows it going") || ai_text_has_sub_ci(text, "how's it going") ||
      ai_text_has_sub_ci(text, "hows your day") || ai_text_has_sub_ci(text, "how's your day") || ai_text_has_sub_ci(text, "anything new") || ai_text_has_sub_ci(text, "what's new") || ai_text_has_sub_ci(text, "whats new") ||
      ai_text_has_sub_ci(text, "you alright") || ai_text_has_sub_ci(text, "all good") || ai_text_has_sub_ci(text, "how are things") || ai_text_has_sub_ci(text, "how goes it") || ai_text_has_sub_ci(text, "hows things") ||
      ai_text_has_sub_ci(text, "what are you up to") || ai_text_has_sub_ci(text, "what are you doing") || ai_text_has_sub_ci(text, "tell me about")))
    scores[AI_SPEECH_SMALLTALK] += 12;
  if (!is_hunger_request && (ai_text_has_sub_ci(text, "chat") || ai_text_has_sub_ci(text, "talk") || ai_text_has_sub_ci(text, "bored") || ai_text_has_sub_ci(text, "day going")))
    scores[AI_SPEECH_SMALLTALK] += 4;

  if (is_hunger_request) {
    scores[AI_SPEECH_SHOP] += 18;
    scores[AI_SPEECH_INN] += 8;
  }

  if (ai_text_has_sub_ci(text, "where") || ai_text_has_sub_ci(text, "which way") || ai_text_has_sub_ci(text, "how do i get") ||
      ai_text_has_sub_ci(text, "directions") || ai_text_has_sub_ci(text, "find") || ai_text_has_sub_ci(text, "locate") || ai_text_has_sub_ci(text, "path"))
    scores[AI_SPEECH_DIRECTIONS] += 10;
  if (ai_text_has_sub_ci(text, "where should i go") || ai_text_has_sub_ci(text, "where do i go") || ai_text_has_sub_ci(text, "where to go"))
    scores[AI_SPEECH_DIRECTIONS] += 14;
  if (ai_text_has_sub_ci(text, "where is the bank") || ai_text_has_sub_ci(text, "where's the bank") || ai_text_has_sub_ci(text, "where is bank"))
    scores[AI_SPEECH_BANK] += 16;

  if (ai_text_has_sub_ci(text, "buy") || ai_text_has_sub_ci(text, "sell") || ai_text_has_sub_ci(text, "wares") || ai_text_has_sub_ci(text, "shop") ||
      ai_text_has_sub_ci(text, "price") || ai_text_has_sub_ci(text, "trade") || ai_text_has_sub_ci(text, "discount"))
    scores[AI_SPEECH_SHOP] += 10;
  if (ai_text_has_sub_ci(text, "inn") || ai_text_has_sub_ci(text, "room") || ai_text_has_sub_ci(text, "rest") || ai_text_has_sub_ci(text, "sleep") || ai_text_has_sub_ci(text, "rent"))
    scores[AI_SPEECH_INN] += 11;
  if (ai_text_has_sub_ci(text, "bank") || ai_text_has_sub_ci(text, "deposit") || ai_text_has_sub_ci(text, "withdraw") || ai_text_has_sub_ci(text, "vault"))
    scores[AI_SPEECH_BANK] += 11;
  if (ai_text_has_sub_ci(text, "help") || ai_text_has_sub_ci(text, "quest") || ai_text_has_sub_ci(text, "job") || ai_text_has_sub_ci(text, "task") ||
      ai_text_has_sub_ci(text, "mission") || ai_text_has_sub_ci(text, "advice"))
    scores[AI_SPEECH_HELP] += 10;

  if (ai_text_has_sub_ci(text, "die") || ai_text_has_sub_ci(text, "kill") || ai_text_has_sub_ci(text, "attack") || ai_text_has_sub_ci(text, "fight") ||
      ai_text_has_sub_ci(text, "rob") || ai_text_has_sub_ci(text, "mug") || ai_text_has_sub_ci(text, "threat"))
    scores[AI_SPEECH_THREAT] += 14;

  if (ai_text_has_sub_ci(text, "what do you think") || ai_text_has_sub_ci(text, "opinion") || ai_text_has_sub_ci(text, "am i") ||
      ai_text_has_sub_ci(text, "do you think i") || ai_text_has_sub_ci(text, "weird") || ai_text_has_sub_ci(text, "pretty"))
    scores[AI_SPEECH_OPINION] += 11;
  if (ai_text_has_sub_ci(text, "you are nice") || ai_text_has_sub_ci(text, "you re nice") || ai_text_has_sub_ci(text, "good work") ||
      ai_text_has_sub_ci(text, "thanks") || ai_text_has_sub_ci(text, "appreciate"))
    scores[AI_SPEECH_COMPLIMENT] += 10;
  if (ai_text_has_sub_ci(text, "love") || ai_text_has_sub_ci(text, "crush") || ai_text_has_sub_ci(text, "date") || ai_text_has_sub_ci(text, "romance") ||
      ai_text_has_sub_ci(text, "kiss") || ai_text_has_sub_ci(text, "marry") || ai_text_has_sub_ci(text, "heart"))
    scores[AI_SPEECH_ROMANCE] += 13;
  if (ai_text_has_sub_ci(text, "feel") || ai_text_has_sub_ci(text, "feeling") || ai_text_has_sub_ci(text, "happy") || ai_text_has_sub_ci(text, "sad") ||
      ai_text_has_sub_ci(text, "afraid") || ai_text_has_sub_ci(text, "angry") || ai_text_has_sub_ci(text, "lonely"))
    scores[AI_SPEECH_FEELING] += 10;

  for (i = 1; i <= AI_SPEECH_FEELING; i++) {
    if (scores[i] > scores[best])
      best = i;
  }

  if (best == AI_SPEECH_UNKNOWN && (ai_text_has_sub_ci(text, "what") || ai_text_has_sub_ci(text, "hmm") || ai_text_has_sub_ci(text, "uh")))
    scores[AI_SPEECH_SMALLTALK] += 6;

  for (i = 1; i <= AI_SPEECH_FEELING; i++) {
    if (scores[i] > scores[best])
      best = i;
  }

  if (out_confidence)
    *out_confidence = scores[best];

  return best;
}

static int ai_intent_from_player_class(int speech_class)
{
  switch (speech_class) {
    case AI_SPEECH_GREET: return AI_INTENT_GREET;
    case AI_SPEECH_WEATHER: return AI_INTENT_SMALLTALK;
    case AI_SPEECH_SMALLTALK: return AI_INTENT_SMALLTALK;
    case AI_SPEECH_DIRECTIONS: return AI_INTENT_DIRECTIONS;
    case AI_SPEECH_SHOP: return AI_INTENT_ASK_SERVICE;
    case AI_SPEECH_INN: return AI_INTENT_INN;
    case AI_SPEECH_BANK: return AI_INTENT_BANK;
    case AI_SPEECH_HELP: return AI_INTENT_QUEST;
    case AI_SPEECH_THREAT: return AI_INTENT_THREAT;
    case AI_SPEECH_COMPLIMENT: return AI_INTENT_PRAISE;
    case AI_SPEECH_OPINION: return AI_INTENT_CONFUSION;
    case AI_SPEECH_ROMANCE: return AI_INTENT_CONFUSION;
    case AI_SPEECH_FEELING: return AI_INTENT_CONFUSION;
    default: return AI_INTENT_CONFUSION;
  }
}

static int ai_detect_emote_kind(const char *text)
{
  if (!text || !*text)
    return AI_EMOTE_OTHER;

  if (ai_text_has_sub_ci(text, "dance"))
    return AI_EMOTE_DANCE;
  if (ai_text_has_sub_ci(text, "high five") || ai_text_has_sub_ci(text, "highfive") || ai_text_has_sub_ci(text, "high-five"))
    return AI_EMOTE_HIGHFIVE;
  if (ai_text_has_sub_ci(text, "hug"))
    return AI_EMOTE_HUG;
  if (ai_text_has_sub_ci(text, "glare") || ai_text_has_sub_ci(text, "glares") || ai_text_has_sub_ci(text, "glared"))
    return AI_EMOTE_GLARE;

  return AI_EMOTE_OTHER;
}

static int ai_detect_intent(enum ai_event_type type, const char *text)
{
  int emote_kind;
  int speech_class;

  if (type == AI_EVENT_PLAYER_EMOTE) {
    emote_kind = ai_detect_emote_kind(text);
    if (emote_kind == AI_EMOTE_DANCE) return AI_INTENT_EMOTE_DANCE;
    if (emote_kind == AI_EMOTE_HIGHFIVE || emote_kind == AI_EMOTE_HUG) return AI_INTENT_EMOTE_HUG;
    if (emote_kind == AI_EMOTE_GLARE) return AI_INTENT_INSULT;

    if (ai_text_has_sub_ci(text, "spit")) return AI_INTENT_EMOTE_SPIT;
    if (ai_text_has_sub_ci(text, "sing")) return AI_INTENT_EMOTE_WAVE;
    if (ai_text_has_sub_ci(text, "wave")) return AI_INTENT_EMOTE_WAVE;
    return AI_INTENT_NONE;
  }

  if (type != AI_EVENT_PLAYER_SAY)
    return AI_INTENT_NONE;

  if (ai_is_gibberish(text))
    return AI_INTENT_GIBBERISH;

  speech_class = ai_player_speech_classify(text, NULL, NULL);
  return ai_intent_from_player_class(speech_class);
}

static int ai_role_priority_score(struct char_data *mob)
{
  if (!mob || !mob->ai_prof) return 0;
  if (mob->ai_prof->role == ROLE_GUARD) return 300;
  if (mob->ai_prof->role == ROLE_MERCHANT && mob->ai_prof->style == 1) return 280;
  if (mob->ai_prof->role == ROLE_MERCHANT) return 260;
  if (mob->ai_prof->role == ROLE_BOSS) return 245;
  if (mob->ai_prof->role == ROLE_CIVILIAN) return 220;
  if (mob->ai_prof->role == ROLE_BANDIT) return 180;
  if (mob->ai_prof->role == ROLE_BEAST) return 120;
  if (mob->ai_prof->role == ROLE_UNDEAD) return 110;
  if (mob->ai_prof->role == ROLE_SPIRIT) return 100;
  return 10;
}

static int ai_event_fit_bonus(struct char_data *mob, enum ai_event_type type, int intent)
{
  int role;
  int style;

  if (!mob || !mob->ai_prof)
    return 0;

  role = mob->ai_prof->role;
  style = mob->ai_prof->style;

  if (!ai_role_can_answer_intent(role, style, intent))
    return -1000;

  if (type == AI_EVENT_PLAYER_SAY && intent == AI_INTENT_GIBBERISH) {
    if (role == ROLE_GUARD) return 320;
    if (role == ROLE_MERCHANT && style == 1) return 305;
    if (role == ROLE_MERCHANT) return 290;
    if (role == ROLE_BOSS) return 275;
    if (role == ROLE_CIVILIAN) return 260;
  }

  if (intent == AI_INTENT_BUY_WEAPON || intent == AI_INTENT_BUY_ARMOR) {
    if (role == ROLE_MERCHANT && style != 1) return 330;
    if (role == ROLE_MERCHANT && style == 1) return -200;
    return -400;
  }

  if (intent == AI_INTENT_INN || intent == AI_INTENT_BUY_FOOD) {
    if (role == ROLE_MERCHANT && style == 1) return 325;
    if (role == ROLE_MERCHANT) return 260;
  }

  if (intent == AI_INTENT_HEAL) {
    if (role == ROLE_GUARD) return 290;
    if (role == ROLE_CIVILIAN) return 260;
    if (role == ROLE_BEAST) return -900;
  }

  if (type == AI_EVENT_PLAYER_SAY && intent == AI_INTENT_DIRECTIONS) {
    if (!ai_role_can_give_directions(role))
      return -500;
    if (role == ROLE_GUARD) return 280;
    if (role == ROLE_MERCHANT && style == 1) return 265;
    if (role == ROLE_BOSS) return 255;
    if (role == ROLE_MERCHANT) return 245;
    if (role == ROLE_CIVILIAN) return 230;
  }

  if (type == AI_EVENT_PLAYER_SAY && intent == AI_INTENT_GREET) {
    if (role == ROLE_MERCHANT && style == 1) return 50;
    if (role == ROLE_MERCHANT) return 35;
    if (role == ROLE_GUARD) return 25;
    if (role == ROLE_BANDIT) return 20;
  }

  return ai_role_priority_score(mob);
}

static unsigned long ai_text_hash_simple(const char *text)
{
  unsigned long h = 2166136261u;
  size_t i;

  if (!text)
    return 0;

  for (i = 0; text[i]; i++) {
    h ^= (unsigned long)(unsigned char)text[i];
    h *= 16777619u;
  }
  return h;
}

static struct ai_player_arb_entry *ai_player_arb_lookup(room_rnum room, long actor_id, long mob_id, enum ai_event_type type, unsigned long text_hash, time_t now)
{
  int i;

  for (i = 0; i < AI_PLAYER_ARB_CACHE_MAX; i++) {
    struct ai_player_arb_entry *e = &ai_player_arb_cache[i];
    if (e->created_at > 0 && (now - e->created_at) > AI_PLAYER_ARB_TTL_SECS)
      memset(e, 0, sizeof(*e));
    if (e->created_at <= 0)
      continue;
    if (e->room == room && e->actor_id == actor_id && e->mob_id == mob_id && e->type == type && e->text_hash == text_hash)
      return e;
  }

  return NULL;
}

static struct ai_player_arb_entry *ai_player_arb_get_or_create(room_rnum room, long actor_id, long mob_id, enum ai_event_type type, unsigned long text_hash, time_t now)
{
  struct ai_player_arb_entry *found;
  int i;
  int oldest = 0;

  found = ai_player_arb_lookup(room, actor_id, mob_id, type, text_hash, now);
  if (found)
    return found;

  for (i = 0; i < AI_PLAYER_ARB_CACHE_MAX; i++) {
    if (ai_player_arb_cache[i].created_at <= 0) {
      oldest = i;
      break;
    }
    if (ai_player_arb_cache[i].created_at < ai_player_arb_cache[oldest].created_at)
      oldest = i;
  }

  memset(&ai_player_arb_cache[oldest], 0, sizeof(ai_player_arb_cache[oldest]));
  ai_player_arb_cache[oldest].room = room;
  ai_player_arb_cache[oldest].actor_id = actor_id;
  ai_player_arb_cache[oldest].mob_id = mob_id;
  ai_player_arb_cache[oldest].type = type;
  ai_player_arb_cache[oldest].text_hash = text_hash;
  ai_player_arb_cache[oldest].created_at = now;
  return &ai_player_arb_cache[oldest];
}

static int ai_actor_room_response_slot(struct char_data *mob, struct char_data *actor, enum ai_event_type type, int intent, int confidence, const char *normalized)
{
  struct char_data *it;
  struct char_data *top1 = NULL, *top2 = NULL;
  struct ai_player_arb_entry *arb;
  int best1 = -999999, best2 = -999999;
  time_t now = time(0);
  unsigned long text_hash;

  if (!mob || !actor || IN_ROOM(mob) == NOWHERE || IN_ROOM(actor) != IN_ROOM(mob))
    return FALSE;

  text_hash = ai_text_hash_simple(normalized);
  arb = ai_player_arb_get_or_create(IN_ROOM(mob), GET_IDNUM(actor), GET_IDNUM(mob), type, text_hash, now);
  if (!arb)
    return FALSE;

  if (arb->responder1 || arb->responder2)
    return (mob == arb->responder1 || mob == arb->responder2);

  for (it = world[IN_ROOM(mob)].people; it; it = it->next_in_room) {
    int pri;
    int role_score;
    int dist = (IN_ROOM(it) == IN_ROOM(actor)) ? 0 : 1;

    if (!IS_NPC(it) || !MOB_FLAGGED(it, MOB_AI_ACTOR) || !it->ai_prof || !it->ai_state)
      continue;

    {
      struct ai_actor_memory_entry *it_e = ai_mem_get_or_create(it, GET_IDNUM(actor));
      time_t it_last_reply = it_e ? it_e->last_reply_time : 0;
      if ((now - it_last_reply) < AI_PER_PLAYER_REPLY_COOLDOWN_SECS)
        continue;
    }

    pri = ai_event_fit_bonus(it, type, intent);
    role_score = ai_role_priority_score(it);
    pri += confidence * 100;
    pri += role_score;
    pri -= (dist * 2);

    if (pri > best1 || (pri == best1 && (!top1 || GET_MOB_VNUM(it) < GET_MOB_VNUM(top1)))) {
      best2 = best1;
      top2 = top1;
      best1 = pri;
      top1 = it;
    } else if (pri > best2 || (pri == best2 && (!top2 || GET_MOB_VNUM(it) < GET_MOB_VNUM(top2)))) {
      best2 = pri;
      top2 = it;
    }
  }

  arb->responder1 = top1;
  arb->responder2 = top2;
  return (mob == arb->responder1 || mob == arb->responder2);
}



static const char *ai_pick_phrase(const char *const *pool)
{
  return ai_pool_pick(pool);
}

static const char *ai_pool_pick(const char *const *pool)
{
  int n = 0;
  int i;

  if (!pool)
    return NULL;

  while (pool[n]) n++;

  if (!n)
    return NULL;

  i = rand_number(0, n - 1);
  return pool[i];
}


#if 0
static int ai_actor_peaceful_room(room_rnum room)
{
  if (room == NOWHERE)
    return FALSE;
  return ROOM_FLAGGED(room, ROOM_PEACEFUL) || ROOM_FLAGGED(room, ROOM_NOMOB) || ROOM_FLAGGED(room, ROOM_NOMAGIC);
}

static int ai_actor_target_cooldown_ok(struct char_data *mob, struct char_data *actor, time_t now)
{
  int i;
  struct ai_actor_memory_entry *e;

  if (!mob || !mob->ai_state || !actor || IS_NPC(actor))
    return TRUE;

  for (i = 0; i < mob->ai_state->mem_count; i++) {
    if (mob->ai_state->mem[i].idnum == GET_IDNUM(actor)) {
      e = &mob->ai_state->mem[i];
      return (now - e->last_reaction) >= AI_TARGET_REACTION_COOLDOWN_SECS;
    }
  }

  e = ai_mem_get_or_create(mob, GET_IDNUM(actor));
  if (!e)
    return TRUE;
  return (now - e->last_reaction) >= AI_TARGET_REACTION_COOLDOWN_SECS;
}

static void ai_actor_mark_target_reaction(struct char_data *mob, struct char_data *actor, time_t now)
{
  struct ai_actor_memory_entry *e;

  if (!mob || !mob->ai_state || !actor || IS_NPC(actor))
    return;
  e = ai_mem_get_or_create(mob, GET_IDNUM(actor));
  if (!e)
    return;
  e->last_reaction = now;
}
#endif


static int ai_goal_interferes(enum ai_goal_type a, enum ai_goal_type b)
{
  if (a == AI_GOAL_IDLE_WANDER && b != AI_GOAL_NONE) return TRUE;
  if (b == AI_GOAL_IDLE_WANDER && a != AI_GOAL_NONE) return TRUE;
  if ((a == AI_GOAL_MAINTAIN_POST && (b == AI_GOAL_PURSUE_OFFENDER || b == AI_GOAL_ESCORT)) ||
      (b == AI_GOAL_MAINTAIN_POST && (a == AI_GOAL_PURSUE_OFFENDER || a == AI_GOAL_ESCORT))) return TRUE;
  if ((a == AI_GOAL_SELL && b == AI_GOAL_SEEK_SAFETY) || (b == AI_GOAL_SELL && a == AI_GOAL_SEEK_SAFETY)) return TRUE;
  if ((a == AI_GOAL_AMBUSH && b == AI_GOAL_REGROUP) || (b == AI_GOAL_AMBUSH && a == AI_GOAL_REGROUP)) return TRUE;
  return FALSE;
}

static struct ai_goal_entry *ai_goal_active(struct char_data *mob, time_t now)
{
  struct ai_conv_actor_state *st = ai_conv_actor_state_get(mob, 1);
  int i, best = -1;

  if (!st)
    return NULL;

  for (i = 0; i < st->goal_count; i++) {
    if (st->goals[i].expires_at > 0 && now > st->goals[i].expires_at)
      continue;
    if (best < 0 || st->goals[i].priority > st->goals[best].priority)
      best = i;
  }
  if (best < 0)
    return NULL;
  return &st->goals[best];
}

static void ai_goal_push(struct char_data *mob, enum ai_goal_type type, float priority, int commit_secs, int expire_secs, long target_idnum)
{
  struct ai_conv_actor_state *st = ai_conv_actor_state_get(mob, 1);
  struct ai_goal_entry *active;
  int idx;

  if (!st || type == AI_GOAL_NONE)
    return;

  active = ai_goal_active(mob, time(0));
  if (active && active->committed_until > time(0) && ai_goal_interferes(active->type, type)) {
    if (priority <= active->priority * 1.5f)
      return;
  }

  if (st->goal_count < AI_GOAL_STACK_MAX) idx = st->goal_count++;
  else idx = AI_GOAL_STACK_MAX - 1;

  st->goals[idx].type = type;
  st->goals[idx].priority = priority;
  st->goals[idx].committed_until = time(0) + MAX(0, commit_secs);
  st->goals[idx].expires_at = (expire_secs > 0) ? (time(0) + expire_secs) : 0;
  st->goals[idx].target_idnum = target_idnum;
}

static void ai_mood_spring_update(struct char_data *mob, float dt)
{
  struct ai_conv_actor_state *st = ai_conv_actor_state_get(mob, 1);
  float force;

  if (!st)
    return;

  force = AI_MOOD_SPRING_K * (st->mood_target - st->mood_current) * MAX(0.1f, dt);
  st->mood_velocity += force;
  st->mood_velocity *= AI_MOOD_DAMPING;
  st->mood_current += st->mood_velocity;
  st->mood_current = ai_clampf(st->mood_current, -1.0f, 1.0f);
  st->mood_target += (0.0f - st->mood_target) * 0.01f;
  st->mood_target = ai_clampf(st->mood_target, -1.0f, 1.0f);
}

static float ai_mood_drift(struct char_data *mob, time_t now)
{
  float twopi = 6.2831853f;
  int vnum = GET_MOB_VNUM(mob);
  int period_a = 7200 + (vnum % 1800);
  int period_b = 21600 + (vnum % 3600);
  int period_c = 86400;
  int phase_a = (vnum * 17) % period_a;
  int phase_b = (vnum * 31) % period_b;
  int phase_c = (vnum * 7) % period_c;

  return 0.15f * sinf(twopi * ((float)(now + phase_a) / (float)period_a)) +
         0.10f * sinf(twopi * ((float)(now + phase_b) / (float)period_b)) +
         0.08f * sinf(twopi * ((float)(now + phase_c) / (float)period_c));
}

static int ai_heatmap_slot(int zone_rnum)
{
  int i, free_idx = -1;
  for (i = 0; i < AI_ZONE_HEATMAP_MAX; i++) {
    if (ai_zone_heatmap[i].zone_rnum == zone_rnum) return i;
    if (free_idx < 0 && ai_zone_heatmap[i].zone_rnum == 0 && ai_zone_heatmap[i].updated_at == 0) free_idx = i;
  }
  return (free_idx >= 0) ? free_idx : (zone_rnum % AI_ZONE_HEATMAP_MAX);
}

static void ai_heatmap_update_danger(int zone_rnum, float incident_weight)
{
  int idx;
  if (zone_rnum < 0) return;
  idx = ai_heatmap_slot(zone_rnum);
  ai_zone_heatmap[idx].zone_rnum = zone_rnum;
  ai_zone_heatmap[idx].danger = MAX(ai_zone_heatmap[idx].danger * 0.98f, ai_zone_heatmap[idx].danger + incident_weight);
  ai_zone_heatmap[idx].danger = ai_clampf(ai_zone_heatmap[idx].danger, 0.0f, 1.0f);
  ai_zone_heatmap[idx].updated_at = time(0);
}

static void ai_heatmap_decay_tick(time_t now)
{
  int i;
  for (i = 0; i < AI_ZONE_HEATMAP_MAX; i++) {
    if (ai_zone_heatmap[i].zone_rnum < 0) continue;
    ai_zone_heatmap[i].danger = ai_clampf(ai_zone_heatmap[i].danger * 0.98f, 0.0f, 1.0f);
    ai_zone_heatmap[i].updated_at = now;
  }
}

static float ai_heatmap_danger(int zone_rnum, time_t now)
{
  int idx = ai_heatmap_slot(zone_rnum);
  (void)now;
  if (idx < 0) return 0.0f;
  return ai_clampf(ai_zone_heatmap[idx].danger, 0.0f, 1.0f);
}

static float ai_heatmap_profit(int zone_rnum)
{
  int idx = ai_heatmap_slot(zone_rnum);
  if (idx < 0) return 0.3f;
  return ai_clampf(ai_zone_heatmap[idx].profit > 0.0f ? ai_zone_heatmap[idx].profit : 0.3f, 0.0f, 1.0f);
}

static int ai_alert_slot(int zone_rnum)
{
  int i;
  for (i = 0; i < AI_ZONE_ALERT_MAX; i++)
    if (ai_zone_alerts[i].zone_rnum == zone_rnum)
      return i;
  return zone_rnum % AI_ZONE_ALERT_MAX;
}

static void ai_alert_raise(int zone_rnum, float level, long target_idnum, int duration_secs)
{
  int idx;
  time_t now = time(0);
  if (zone_rnum < 0) return;
  idx = ai_alert_slot(zone_rnum);
  ai_zone_alerts[idx].zone_rnum = zone_rnum;
  ai_zone_alerts[idx].alert_level = MAX(ai_zone_alerts[idx].alert_level, ai_clampf(level, 0.0f, 1.0f));
  ai_zone_alerts[idx].target_idnum = target_idnum;
  ai_zone_alerts[idx].raised_at = now;
  ai_zone_alerts[idx].expires_at = now + MAX(1, duration_secs);
}

static float ai_alert_level(int zone_rnum, time_t now)
{
  int idx = ai_alert_slot(zone_rnum);
  if (idx < 0 || ai_zone_alerts[idx].zone_rnum != zone_rnum) return 0.0f;
  if (ai_zone_alerts[idx].expires_at > 0 && now > ai_zone_alerts[idx].expires_at) return 0.0f;
  return ai_clampf(ai_zone_alerts[idx].alert_level, 0.0f, 1.0f);
}

static void ai_alert_decay_tick(time_t now)
{
  int i;
  for (i = 0; i < AI_ZONE_ALERT_MAX; i++) {
    if (ai_zone_alerts[i].zone_rnum < 0)
      continue;
    if (ai_zone_alerts[i].expires_at > 0 && now > ai_zone_alerts[i].expires_at) {
      memset(&ai_zone_alerts[i], 0, sizeof(ai_zone_alerts[i]));
      continue;
    }
    ai_zone_alerts[i].alert_level = ai_clampf(ai_zone_alerts[i].alert_level * 0.95f, 0.0f, 1.0f);
  }
}

static float ai_suspicion_score(struct char_data *mob, struct char_data *actor, struct ai_actor_memory_entry *e, time_t now, int speech_act)
{
  float odds = 1.0f;
  int zone;

  if (!mob || !actor || !e || !mob->ai_prof || !ai_role_is_suspicious_watcher(mob->ai_prof->role))
    return 0.0f;

  zone = (IN_ROOM(mob) != NOWHERE) ? world[IN_ROOM(mob)].zone : -1;
  if (GET_EQ(actor, WEAR_WIELD)) odds *= 3.5f;
  if (speech_act == AI_INTENT_THREAT) odds *= 4.0f;
  if (speech_act == AI_INTENT_INSULT) odds *= 2.5f;
  if (ai_belief_value_decay(e->belief_hostility, e->belief_updated_at, now, 180.0f) > 0.5f) odds *= 3.0f;
  if (ai_alert_level(zone, now) > 0.5f) odds *= 2.0f;
  if (time_info.hours >= 20 || time_info.hours <= 6) odds *= 1.5f;
  if (e->disposition_flags & AI_DISP_DISRESPECT) odds *= 2.0f;
  if (e->disposition_flags & AI_DISP_FRIENDLY) odds *= 0.6f;
  if (ai_belief_value_decay(e->belief_familiarity, e->belief_updated_at, now, 180.0f) > 0.6f) odds *= 0.5f;
  if (GET_INVIS_LEV(actor) > 0) odds *= 2.5f;

  return ai_clampf(odds / (1.0f + odds), 0.0f, 1.0f);
}

static float ai_utility_score(struct char_data *mob, enum ai_action_type action, struct char_data *actor, int speech_act, int intent, time_t now, float attention_score, int is_emote_event, float suspicion)
{
  float need_weight = 0.2f, need_value = 0.5f, context_fit = 0.5f, risk_cost = 0.0f, habit_bonus = 0.0f;
  struct ai_actor_memory_entry *e = NULL;
  struct ai_goal_entry *goal = ai_goal_active(mob, now);
  struct ai_voice_profile *vp;
  float health_pct = (GET_MAX_HIT(mob) > 0) ? (float)GET_HIT(mob)/(float)GET_MAX_HIT(mob) : 1.0f;

  if (actor && !IS_NPC(actor) && mob && mob->ai_state)
    e = ai_mem_get_or_create(mob, GET_IDNUM(actor));
  vp = (struct ai_voice_profile *)ai_voice_profile_get(mob);

  switch (action) {
    case AI_ACTION_SPEAK: need_weight = (mob->ai_prof->role==ROLE_GUARD?0.7f:mob->ai_prof->role==ROLE_MERCHANT?0.8f:mob->ai_prof->role==ROLE_CIVILIAN?0.8f:0.6f); break;
    case AI_ACTION_SPEAK_WARN: need_weight = (mob->ai_prof->role==ROLE_GUARD?0.9f:mob->ai_prof->role==ROLE_BANDIT?0.7f:0.3f); break;
    case AI_ACTION_IGNORE: need_weight = (mob->ai_prof->role==ROLE_BEAST?0.8f:mob->ai_prof->role==ROLE_UNDEAD?0.7f:mob->ai_prof->role==ROLE_SPIRIT?0.6f:0.2f); break;
    case AI_ACTION_CALL_HELP: need_weight = (mob->ai_prof->role==ROLE_GUARD?0.8f:mob->ai_prof->role==ROLE_CIVILIAN?0.6f:0.3f); break;
    case AI_ACTION_FLEE: need_weight = (mob->ai_prof->role==ROLE_BEAST?0.4f:mob->ai_prof->role==ROLE_CIVILIAN?0.7f:mob->ai_prof->role==ROLE_BANDIT?0.5f:0.1f); break;
    case AI_ACTION_OBSERVE: need_weight = (mob->ai_prof->role==ROLE_BANDIT||mob->ai_prof->role==ROLE_CULTIST||mob->ai_prof->role==ROLE_SPIRIT)?0.7f:0.3f; break;
    case AI_ACTION_EMOTE_REACT: need_weight = (mob->ai_prof->role==ROLE_BEAST?0.9f:mob->ai_prof->role==ROLE_SPIRIT?0.5f:0.2f); break;
    default: break;
  }

  switch (action) {
    case AI_ACTION_SPEAK: need_value = ai_clampf(1.0f - ((e && now > e->last_reply_time) ? (float)MIN(5, (int)(now - e->last_reply_time)/5) / 5.0f : 0.0f), 0.0f, 1.0f); break;
    case AI_ACTION_SPEAK_WARN: need_value = e ? ai_belief_value_decay(e->belief_hostility, e->belief_updated_at, now, 180.0f) : suspicion; break;
    case AI_ACTION_IGNORE: need_value = 1.0f - attention_score; break;
    case AI_ACTION_CALL_HELP: need_value = ai_clampf((1.0f - health_pct) * 2.0f, 0.0f, 1.0f); break;
    case AI_ACTION_FLEE: need_value = 1.0f - health_pct; break;
    case AI_ACTION_OBSERVE: need_value = e ? ai_belief_confidence_now(e, now) : suspicion; break;
    case AI_ACTION_EMOTE_REACT: need_value = is_emote_event ? 1.0f : 0.1f; break;
    default: break;
  }

  context_fit = 0.5f;
  if (speech_act == AI_INTENT_GREET && action == AI_ACTION_SPEAK) context_fit = 0.9f;
  if (speech_act == AI_INTENT_CONFUSION && action == AI_ACTION_SPEAK) context_fit = 0.8f;
  if (speech_act == AI_INTENT_INSULT && action == AI_ACTION_SPEAK) context_fit = 0.4f;
  if (speech_act == AI_INTENT_INSULT && action == AI_ACTION_SPEAK_WARN) context_fit = 0.9f;
  if (speech_act == AI_INTENT_INSULT && action == AI_ACTION_IGNORE) context_fit = 0.3f;
  if (speech_act == AI_INTENT_PRAISE && action == AI_ACTION_IGNORE) context_fit = 0.1f;

  if (action == AI_ACTION_SPEAK_WARN) risk_cost = 0.1f;
  else if (action == AI_ACTION_CALL_HELP) risk_cost = 0.2f;
  else if (action == AI_ACTION_FLEE) risk_cost = 0.3f + (mob->ai_prof->role==ROLE_GUARD?0.4f:0.0f);
  else if (action == AI_ACTION_IGNORE) risk_cost = 0.05f;

  if (vp) {
    if (vp->mbti_ei > 0 && (action == AI_ACTION_SPEAK || action == AI_ACTION_SPEAK_WARN)) habit_bonus += 0.15f;
    if (vp->mbti_ei <= 0 && (action == AI_ACTION_OBSERVE || action == AI_ACTION_IGNORE)) habit_bonus += 0.15f;
    if (vp->intensity >= 2 && (action == AI_ACTION_SPEAK_WARN || action == AI_ACTION_EMOTE_REACT)) habit_bonus += 0.1f;
  }

  if (goal) {
    if (goal->type == AI_GOAL_MAINTAIN_POST) {
      if (action == AI_ACTION_FLEE) risk_cost += 0.5f;
      if (action == AI_ACTION_SPEAK_WARN) need_weight += 0.2f;
    } else if (goal->type == AI_GOAL_SELL) {
      if (action == AI_ACTION_SPEAK && (intent == AI_INTENT_ASK_SERVICE || intent == AI_INTENT_BUY_ARMOR || intent == AI_INTENT_BUY_WEAPON || intent == AI_INTENT_BUY_FOOD)) context_fit += 0.3f;
    } else if (goal->type == AI_GOAL_MONITOR_SUSPECT) {
      if (action == AI_ACTION_OBSERVE) need_weight += 0.4f;
      if (action == AI_ACTION_SPEAK) need_weight -= 0.2f;
    }
  }

  return need_weight * need_value * context_fit - risk_cost + habit_bonus;
}

void ai_actor_on_room_event(struct char_data *mob, enum ai_event_type type, struct char_data *actor, const char *text)
{
  struct ai_actor_memory_entry *e;
  struct ai_conv_room_state *room_st;
  struct ai_conv_actor_state *conv_st;
  time_t now = time(0);
  int intent = AI_INTENT_NONE;
  int confidence = 0;
  int speech_class = AI_SPEECH_UNKNOWN;
  const char *line = NULL;
  int avoid_template_id = -1;
  int selected_template_id = -1;
  char normalized[256];
  float attention_score;
  float suspicion = 0.0f;
  float best_score = -999.0f;
  enum ai_action_type best_action = AI_ACTION_IGNORE;
  float mood = 0.0f;
  float alert_level = 0.0f;
  float zone_danger = 0.0f;
  float zone_profit = 0.0f;
  struct ai_goal_entry *goal;
  struct ai_context_vector ctx;
  struct ai_session_read_entry *sr = NULL;
  float cooldown_remaining = 0.0f;
  int zone = (IN_ROOM(mob) != NOWHERE) ? world[IN_ROOM(mob)].zone : -1;
  int i;
  int role = (mob && mob->ai_prof) ? mob->ai_prof->role : ROLE_UNKNOWN;
  int style = (mob && mob->ai_prof) ? mob->ai_prof->style : 0;
  int is_hunger_request = FALSE;
  int bakery_room = NOWHERE;
  int bakery_item = -1;
  int inn_room = NOWHERE;
  int inn_item = -1;
  enum ai_reply_goal chosen_goal = GOAL_INFORM;
  int chosen_goal_known = FALSE;
  struct ai_reply_context rctx;

#define AI_EVT_RETURN(_reason) do { \
  ai_dbg_evt(mob, (_reason), type, actor, text); \
  if (ai_debug) \
    ai_debug_log("AI_EVT_RETURN vnum=%d role=%s style=%d event=%s intent=%d chosen_goal=%s RETURN_REASON=%s", \
                 mob ? GET_MOB_VNUM(mob) : -1, ai_role_name_local(role), style, ai_event_reason_name(type), intent, \
                 chosen_goal_known ? ai_reply_goal_name(chosen_goal) : "UNKNOWN", (_reason)); \
  return; \
} while (0)

  ai_dbg_evt(mob, "ENTER", type, actor, text);
  memset(&rctx, 0, sizeof(rctx));
  rctx.event_id = -1;

  if (!mob || !actor || IS_NPC(actor))
    AI_EVT_RETURN("BAD_MOB_OR_ACTOR");
  if (!ai_actor_ensure_ready(mob))
    AI_EVT_RETURN("NOT_READY");
  if (IN_ROOM(mob) == NOWHERE)
    AI_EVT_RETURN("NOWHERE_ROOM");

  ai_state_refresh_local_topics(mob);
  {
    int p = ai_imtb_pick_personality(mob, role, style);
    rctx.personality = p;
    rctx.profile = ai_imtb_profile_get(p);
  }
  ai_normalize_text(text ? text : "", normalized, sizeof(normalized));
  attention_score = ai_attention_score(mob, type, actor, normalized, now);
  ai_state_push_event(mob, type, actor, normalized);

  if (attention_score < AI_ATTENTION_THRESHOLD) {
    ai_debug_log("ai_skip_attention vnum=%d event=%s score=%.2f", GET_MOB_VNUM(mob), ai_event_reason_name(type), attention_score);
    AI_EVT_RETURN("LOW_ATTENTION");
  }

  ai_context_vector_build(mob, actor, now, &ctx);

  e = ai_mem_get_or_create(mob, GET_IDNUM(actor));
  if (!e)
    AI_EVT_RETURN("NO_MEMORY_ENTRY");

  if (type == AI_EVENT_PLAYER_SAY) {
    if (ai_is_gibberish(normalized)) {
      intent = AI_INTENT_GIBBERISH;
      confidence = 8;
    } else {
      speech_class = ai_player_speech_classify(normalized, &confidence, NULL);
      intent = ai_intent_from_player_class(speech_class);
      if (speech_class == AI_SPEECH_WEATHER)
        confidence += 4;
    }
  } else {
    intent = ai_detect_intent(type, normalized);
    confidence = 10;
  }

  rctx.current_text = normalized;
  rctx.intent = intent;
  rctx.speech_act = intent;
  rctx.domain = ai_classify_domain(normalized, intent, intent);
  rctx.topic_target = ai_detect_topic_target_from_text(normalized);
  ai_detect_requested_targets(normalized, rctx.requested_targets, &rctx.requested_count);
  rctx.primary_topic_target = (rctx.requested_count > 0) ? rctx.requested_targets[0] : rctx.topic_target;
  is_hunger_request = (type == AI_EVENT_PLAYER_SAY &&
                      (ai_text_has_sub_ci(normalized, "hungry") || ai_text_has_sub_ci(normalized, "starving") || ai_text_has_sub_ci(normalized, "need food") ||
                       ai_text_has_sub_ci(normalized, "something to eat") || ai_text_has_sub_ci(normalized, "food") || ai_text_has_sub_ci(normalized, "eat") ||
                       ai_text_has_sub_ci(normalized, "meal") || ai_text_has_sub_ci(normalized, "ration") || ai_text_has_sub_ci(normalized, "bread") ||
                       ai_text_has_sub_ci(normalized, "stew")));
  if (type == AI_EVENT_PLAYER_SAY) {
    int asks_where = (!strncasecmp(normalized, "where", 5) || ai_text_has_sub_ci(normalized, "where"));
    int asks_service = (rctx.requested_count > 0);

    if (asks_where) {
      rctx.domain = DOMAIN_DIRECTIONS;
      intent = AI_INTENT_ASK_SERVICE;
      rctx.intent = intent;
      rctx.speech_act = intent;
    } else if (asks_service || is_hunger_request) {
      rctx.domain = DOMAIN_SERVICES;
      intent = AI_INTENT_ASK_SERVICE;
      rctx.intent = intent;
      rctx.speech_act = intent;
    }
  }
  if (is_hunger_request) {
    rctx.domain = DOMAIN_SERVICES;
    if (mob && ai_find_closest_service(mob, actor, TARGET_BAKERY, &bakery_room, &bakery_item))
      rctx.primary_topic_target = TARGET_BAKERY;
    else if (mob && ai_find_closest_service(mob, actor, TARGET_INN, &inn_room, &inn_item))
      rctx.primary_topic_target = TARGET_INN;
    else
      rctx.primary_topic_target = TARGET_INN;
    if (rctx.requested_count <= 0) {
      rctx.requested_targets[0] = rctx.primary_topic_target;
      rctx.requested_count = 1;
    }
  }
  rctx.confidence = 0;
  if (ai_debug) {
    int qi;
    char reqbuf[96];
    reqbuf[0] = '\0';
    for (qi = 0; qi < rctx.requested_count; qi++) {
      size_t u = strlen(reqbuf);
      if (u < sizeof(reqbuf) - 1)
        snprintf(reqbuf + u, sizeof(reqbuf) - u, "%s%s", (qi ? "," : ""), ai_topic_key_name(rctx.requested_targets[qi]));
    }
    ai_debug_log("AI_DOMAIN_SELECT vnum=%d domain=%d target=%s requested=[%s]", GET_MOB_VNUM(mob), rctx.domain, ai_topic_key_name(rctx.primary_topic_target), reqbuf);
  }

  e->last_seen_time = now;
  e->last_interaction_time = now;
  e->last_room_vnum = (IN_ROOM(mob) == NOWHERE) ? NOWHERE : GET_ROOM_VNUM(IN_ROOM(mob));
  e->last_intent = intent;
  e->belief_confidence = 1.0f;
  e->belief_last_room = e->last_room_vnum;
  e->belief_updated_at = now;
  if (actor->player.name)
    strlcpy(e->key_name, actor->player.name, sizeof(e->key_name));

  if (type == AI_EVENT_PLAYER_LEAVE) {
    e->belief_confidence = 0.85f;
    e->belief_last_direction = -1;
  }

  if (type == AI_EVENT_PLAYER_SAY)
    e->belief_familiarity = ai_clampf(e->belief_familiarity + 0.05f, 0.0f, 1.0f);

  if (intent == AI_INTENT_THREAT) e->belief_hostility += 0.25f;
  if (intent == AI_INTENT_INSULT) e->belief_hostility += 0.10f;
  if (intent == AI_INTENT_PRAISE) e->belief_hostility -= 0.05f;
  if (type == AI_EVENT_COMBAT_START) e->belief_hostility = 1.0f;
  if (intent == AI_INTENT_GREET || intent == AI_INTENT_SMALLTALK) e->belief_hostility -= 0.03f;
  e->belief_hostility = ai_clampf(e->belief_hostility, 0.0f, 1.0f);

  room_st = ai_conv_room_state_get(IN_ROOM(mob), 1);
  if (room_st) {
    if (type == AI_EVENT_PLAYER_SAY)
      room_st->last_player_speech_time = now;
    if (type == AI_EVENT_COMBAT_START)
      room_st->last_violence_time = now;
    ctx.recent_violence = (room_st->last_violence_time > 0 && (now - room_st->last_violence_time) <= 60);
  }

  conv_st = ai_conv_actor_state_get(mob, 1);
  if (conv_st && type == AI_EVENT_PLAYER_SAY) {
    conv_st->current_topic = ai_conv_topic_from_intent(intent);
    conv_st->last_speaker_id = GET_IDNUM(actor);
    conv_st->last_line_time = now;
    conv_st->updated_at = now;
  }

  if (conv_st) {
    sr = ai_session_read_get(conv_st, GET_IDNUM(actor), 1, now);
    if (sr) {
      ai_session_read_update(sr, type, intent, normalized, now);
      ai_session_read_apply_impression(sr, e, conv_st);
      ai_session_read_update_arc(sr, mob, &ctx);
      if (sr->first_impression < -0.4f)
        ai_goal_push(mob, AI_GOAL_MONITOR_SUSPECT, 0.62f, 10, 60, GET_IDNUM(actor));
    }
  }

  if (type == AI_EVENT_PLAYER_SAY && sr) {
    if (sr->last_topic_target != TARGET_NONE && (now - sr->last_turn_time) <= AI_TOPIC_CONTINUITY_WINDOW_SECS &&
        rctx.requested_count == 0 && rctx.topic_target == TARGET_NONE &&
        (ai_text_has_sub_ci(normalized, "what about") || ai_text_has_sub_ci(normalized, "and "))) {
      rctx.primary_topic_target = sr->last_topic_target;
      rctx.topic_target = sr->last_topic_target;
      rctx.requested_targets[0] = sr->last_topic_target;
      rctx.requested_count = 1;
      if (sr->last_domain != DOMAIN_NONE)
        rctx.domain = sr->last_domain;
      if (ai_debug)
        ai_debug_log("AI_CONTINUITY_REUSE vnum=%d player=%ld topic=%s domain=%d", GET_MOB_VNUM(mob), GET_IDNUM(actor), ai_topic_key_name(sr->last_topic_target), rctx.domain);
    }

    ai_working_mem_push(sr->working_mem, &sr->working_mem_head, &sr->working_mem_count, &sr->working_mem_next_id,
                        GET_IDNUM(actor), normalized, intent, rctx.topic_target, intent, rctx.domain, now, &rctx.event_id);
    rctx.callback_hint = ai_relevance_link(sr->working_mem, sr->working_mem_count, sr->working_mem_head,
                                           GET_IDNUM(actor), normalized, intent, rctx.topic_target, rctx.domain, now, &rctx.callback_hint_score);
    if (sr->working_mem_count == 0) {
      rctx.callback_hint = NULL;
      rctx.callback_prefix[0] = '\0';
      if (ai_debug)
        ai_debug_log("AI_CALLBACK_RESET_EMPTY player=%ld", GET_IDNUM(actor));
    }
    if (rctx.callback_hint && sr->arc >= AI_ARC_ACKNOWLEDGED) {
      int hint_score = rctx.callback_hint_score;
      int age_secs = (int)(now - rctx.callback_hint->when);
      int domain_match = (rctx.domain == rctx.callback_hint->domain);
      int topic_match = (rctx.primary_topic_target != TARGET_NONE && rctx.primary_topic_target == rctx.callback_hint->topic_target);
      if (hint_score >= 20 && rctx.callback_hint->player_idnum == GET_IDNUM(actor) && (domain_match || topic_match)) {
        unsigned long cseed = ai_conv_seed(mob, intent, (unsigned int)now);
        static const char *const guard_pool[] = {"You asked about that already. ", "As I mentioned — ", "Since you asked — ", NULL};
        static const char *const inn_pool[] = {"You brought that up earlier. ", "Ah right, you asked — ", "I remember you asked. ", NULL};
        static const char *const merch_pool[] = {"You asked about that. ", "Right, you mentioned that. ", NULL};
        static const char *const other_pool[] = {"You asked before. ", "As I said — ", NULL};
        const char *const *pp = other_pool;
        int pn = 0;
        int pi = 0;
        if (role == ROLE_GUARD) pp = guard_pool;
        else if (role == ROLE_MERCHANT && style == 1) pp = inn_pool;
        else if (role == ROLE_MERCHANT) pp = merch_pool;
        while (pp[pn]) pn++;
        if (pn > 0) {
          pi = (int)(cseed % (unsigned long)pn);
          snprintf(rctx.callback_prefix, sizeof(rctx.callback_prefix), "%.*s", (int)sizeof(rctx.callback_prefix) - 1, pp[pi]);
        }
        if (ai_debug)
          ai_debug_log("AI_CALLBACK_ACCEPT score=%d age=%d event=%d hint=%s", hint_score, age_secs, rctx.callback_hint->event_id, rctx.callback_hint->text);
      } else {
        rctx.callback_prefix[0] = '\0';
        if (ai_debug)
          ai_debug_log("AI_CALLBACK_REJECT best_score=%d", hint_score);
      }
    } else {
      rctx.callback_prefix[0] = '\0';
      if (ai_debug)
        ai_debug_log("AI_CALLBACK_REJECT best_score=%d", rctx.callback_hint_score);
    }
    if (rctx.domain == DOMAIN_SHOPPING || rctx.domain == DOMAIN_SERVICES || rctx.domain == DOMAIN_DIRECTIONS) {
      ai_resolve_world_facts(mob, actor, rctx.primary_topic_target, rctx.domain, &ctx, &rctx.facts);
      rctx.confidence = rctx.facts.confidence;
    }
  }

  suspicion = ai_suspicion_score(mob, actor, e, now, intent);
  suspicion = ai_clampf(suspicion + ai_context_suspicion_bias(mob, &ctx) + (sr && sr->archetype == AI_ARCH_TROUBLEMAKER ? 0.08f : 0.0f), 0.0f, 1.0f);
  if (suspicion > 0.7f)
    e->belief_hostility = MAX(e->belief_hostility, suspicion - 0.1f);

  if (suspicion > 0.85f) ai_goal_push(mob, AI_GOAL_PURSUE_OFFENDER, 0.9f, 45, 120, GET_IDNUM(actor));
  else if (suspicion > 0.7f) ai_goal_push(mob, AI_GOAL_MONITOR_SUSPECT, 0.7f, 20, 90, GET_IDNUM(actor));

  if (type == AI_EVENT_COMBAT_START) {
    ai_heatmap_update_danger(zone, 0.15f);
    ai_alert_raise(zone, 0.5f, GET_IDNUM(actor), 180);
  }

  alert_level = ctx.zone_alert;
  zone_danger = ctx.zone_danger;
  zone_profit = ai_heatmap_profit(zone);
  goal = ai_goal_active(mob, now);
  mood = ai_clampf((conv_st ? conv_st->mood_current : 0.0f) + ai_mood_drift(mob, now), -1.0f, 1.0f);

  for (i = 0; i < AI_ACTION_COUNT; i++) {
    float score = ai_utility_score(mob, (enum ai_action_type)i, actor, intent, intent, now, attention_score, (type == AI_EVENT_PLAYER_EMOTE), suspicion);
    score += ai_context_action_bias(mob, &ctx, (enum ai_action_type)i, intent);
    score += ai_session_arch_action_bias(sr, (enum ai_action_type)i);
    score += ai_session_arc_action_bias(sr, (enum ai_action_type)i);
    score += ai_session_cooldown_penalty(sr, (enum ai_action_type)i, now);
    if (mood < -0.5f && (i == AI_ACTION_SPEAK_DEFLECT || i == AI_ACTION_IGNORE)) score += 0.2f;
    if (alert_level > 0.6f && i == AI_ACTION_OBSERVE && mob->ai_prof->role == ROLE_BANDIT) score += 0.25f;
    if (mob->ai_prof->role == ROLE_BANDIT && i == AI_ACTION_OBSERVE) score += (zone_profit - zone_danger) * 0.2f;
    if (mob->ai_prof->role == ROLE_CIVILIAN && i == AI_ACTION_IGNORE) score += zone_danger * 0.2f;
    if (alert_level > 0.6f && i == AI_ACTION_SPEAK_WARN && mob->ai_prof->role == ROLE_GUARD) score += 0.25f;
    if (score > best_score) {
      best_score = score;
      best_action = (enum ai_action_type)i;
    }
  }

  if (best_action == AI_ACTION_OBSERVE) {
    ai_goal_push(mob, AI_GOAL_MONITOR_SUSPECT, 0.65f, 20, 120, GET_IDNUM(actor));
  } else if (best_action == AI_ACTION_CALL_HELP) {
    ai_goal_push(mob, AI_GOAL_REGROUP, 0.8f, 15, 90, GET_IDNUM(actor));
    ai_alert_raise(zone, 0.7f, GET_IDNUM(actor), 300);
  } else if (best_action == AI_ACTION_FLEE) {
    ai_goal_push(mob, AI_GOAL_SEEK_SAFETY, 0.85f, 15, 90, GET_IDNUM(actor));
  }

  if (!(best_action == AI_ACTION_SPEAK || best_action == AI_ACTION_SPEAK_WARN || best_action == AI_ACTION_SPEAK_DEFLECT || best_action == AI_ACTION_EMOTE_REACT)) {
    cooldown_remaining = (sr && sr->cooldown_until > now) ? (float)(sr->cooldown_until - now) : 0.0f;
    ai_debug_log("AI_EVT vnum=%d role=%s goal=%s mbti=%s tb=%s arch=%s arc=%s ex=%d attn=%.2f susp=%.2f action=%s cd=%.0f",
                 GET_MOB_VNUM(mob), ai_role_name_local(mob->ai_prof->role), ai_goal_name(goal ? goal->type : AI_GOAL_NONE), ai_mbti_string(ai_voice_profile_get(mob)),
                 ai_time_bucket_name(ctx.time_bucket), ai_arch_name(sr ? sr->archetype : AI_ARCH_UNKNOWN), ai_arc_name(sr ? sr->arc : AI_ARC_STRANGER),
                 sr ? sr->exchange_count : 0, attention_score, suspicion, ai_action_name(best_action), cooldown_remaining);
    AI_EVT_RETURN("NON_SPEAK_ACTION_SELECTED");
  }

  if (!ai_actor_room_response_slot(mob, actor, type, intent, confidence, normalized))
    AI_EVT_RETURN("ARB_SLOT_DENIED");

  if (type == AI_EVENT_PLAYER_SAY && IN_ROOM(mob) != NOWHERE) {
    struct ai_player_arb_entry *arb = ai_player_arb_lookup(IN_ROOM(mob), GET_IDNUM(actor), GET_IDNUM(mob), type, ai_text_hash_simple(normalized), now);
    if (arb) {
      if (mob == arb->responder2)
        avoid_template_id = arb->responder1_template_id;
      else if (mob == arb->responder1)
        avoid_template_id = arb->responder2_template_id;
    }
  }
  {
    time_t last_reply = sr ? sr->last_reply_time : e->last_reply_time;
    if ((now - last_reply) < AI_PER_PLAYER_REPLY_COOLDOWN_SECS)
      AI_EVT_RETURN("PER_PLAYER_REPLY_COOLDOWN");
  }

  {
    const char *pool = "POOL_NONE";
    const char *reason = ai_event_reason_name(type);
    char targeted[640];
    char voiced[300];
    char decorated[360];
    char service_line[320];
    const char *stance_prefix = NULL;
    const char *mirror_clause = NULL;
    const char *because_clause = NULL;
    int question_shape = ai_is_question_shape(normalized);
    int repeated_topic = FALSE;

    voiced[0] = '\0';
    decorated[0] = '\0';
    service_line[0] = '\0';

    if (conv_st) {
      conv_st->tone_clipped = 0;
      conv_st->tone_no_extras = 0;
      conv_st->tone_day_trade = 0;
      conv_st->tone_night_watch = 0;
      if (ctx.recent_violence || (sr && sr->arc == AI_ARC_COLD)) {
        conv_st->tone_clipped = 1;
        conv_st->tone_no_extras = 1;
      }
      if (ctx.time_bucket == AI_TIME_NIGHT && mob->ai_prof->role == ROLE_GUARD) conv_st->tone_night_watch = 1;
      if (ctx.time_bucket == AI_TIME_NIGHT && mob->ai_prof->role == ROLE_MERCHANT) conv_st->tone_night_watch = 1;
      if (ctx.time_bucket == AI_TIME_DAY && mob->ai_prof->role == ROLE_MERCHANT) conv_st->tone_day_trade = 1;
      if (sr && sr->arc == AI_ARC_STRANGER) conv_st->tone_no_extras = 1;
      if (sr && sr->arc == AI_ARC_RAPPORT && !sr->rapport_rumor_used) conv_st->tone_no_extras = 0;
    }

    {
      struct ai_reply_intention intention;
      int suspicion_bucket = (suspicion > 0.6f) ? 2 : ((suspicion > 0.35f) ? 1 : 0);
      const char *core = NULL;
      const struct ai_voice_profile *vp;
      unsigned long seed;
      int skip_voice = FALSE;

      intention = ai_form_intention(mob, intent, speech_class, suspicion_bucket, sr ? sr->arc : AI_ARC_STRANGER, &ctx, sr, e, now);
      if (is_hunger_request || (type == AI_EVENT_PLAYER_SAY && (rctx.domain == DOMAIN_SERVICES || rctx.domain == DOMAIN_DIRECTIONS)))
        intention.goal = GOAL_SERVE;
      chosen_goal = intention.goal;
      chosen_goal_known = TRUE;
      if (best_action == AI_ACTION_SPEAK_WARN)
        intention.goal = GOAL_WARN;
      else if (best_action == AI_ACTION_SPEAK_DEFLECT)
        intention.goal = GOAL_DEFLECT;
      chosen_goal = intention.goal;

      if (sr && sr->last_topic_target != TARGET_NONE && rctx.primary_topic_target != TARGET_NONE &&
          sr->last_topic_target == rctx.primary_topic_target && (now - sr->last_turn_time) <= AI_TOPIC_CONTINUITY_WINDOW_SECS)
        repeated_topic = TRUE;

      if (rctx.confidence == 0 && intention.goal == GOAL_SERVE && !is_hunger_request)
        intention.goal = GOAL_DEFLECT;

      core = ai_select_content_for_intention(mob, &intention, &rctx, sr, &rctx.from_template);
      rctx.chosen_core = core;
      line = core;
      if (!line || !*line) {
        if (type == AI_EVENT_PLAYER_SAY && (rctx.domain == DOMAIN_SERVICES || rctx.domain == DOMAIN_DIRECTIONS))
          line = ai_role_redirect_line(role, style, rctx.primary_topic_target);
        else if (rctx.domain == DOMAIN_SOCIAL)
          line = ai_select_content_for_intention(mob, &(struct ai_reply_intention){AI_INTENT_SMALLTALK, GOAL_CONNECT, STANCE_NEUTRAL, 0, 1}, &rctx, sr, &rctx.from_template);
        else
          line = ai_epistemic_line(0, role, rctx.primary_topic_target);
      }

      if (intention.goal == GOAL_SERVE && type == AI_EVENT_PLAYER_SAY && (rctx.domain == DOMAIN_SERVICES || rctx.domain == DOMAIN_DIRECTIONS) && line && *line && !ai_line_has_min_service_usefulness(line)) {
        const char *repick = ai_role_redirect_line(role, style, rctx.primary_topic_target);
        if (repick && *repick)
          line = repick;
      }

      if (type == AI_EVENT_PLAYER_SAY && rctx.requested_count > 1 &&
          (rctx.domain == DOMAIN_SHOPPING || rctx.domain == DOMAIN_SERVICES || rctx.domain == DOMAIN_DIRECTIONS)) {
        char multi_core[256];
        int qi;
        int pos = 0;
        multi_core[0] = '\0';
        for (qi = 0; qi < rctx.requested_count && qi < AI_REQ_MAX; qi++) {
          struct ai_world_facts mf;
          char seg[80];
          const char *label = ai_topic_key_name(rctx.requested_targets[qi]);
          ai_resolve_world_facts(mob, actor, rctx.requested_targets[qi], rctx.domain, &ctx, &mf);
          if (mf.confidence == 3) {
            int rem = (int)sizeof(seg) - 1;
            int sw = snprintf(seg, sizeof(seg), "%s: ", label);
            int pos2 = (sw > 0) ? MIN(sw, rem) : 0;
            if (pos2 < rem) {
              int w2 = snprintf(seg + pos2, sizeof(seg) - pos2, "%.*s", (int)(sizeof(seg) - pos2 - 1), mf.service_name[0] ? mf.service_name : "nearby");
              if (w2 > 0) pos2 += MIN(w2, (int)(sizeof(seg) - pos2 - 1));
            }
            if (pos2 < rem) {
              int segp = (int)sizeof(seg) - pos2 - 3;
              if (segp < 0)
                segp = 0;
              int w3 = snprintf(seg + pos2, sizeof(seg) - pos2, " %.*s.", segp, mf.route_snippet[0] ? mf.route_snippet : "ask around");
              if (w3 > 0) pos2 += MIN(w3, (int)(sizeof(seg) - pos2 - 1));
            }
            seg[sizeof(seg) - 1] = '\0';
          } else {
            int epip = (int)sizeof(seg) - ((int)strlen(label) + 3);
            if (epip < 0)
              epip = 0;
            snprintf(seg, sizeof(seg), "%s: %.*s", label, epip, ai_epistemic_line(mf.confidence, role, rctx.requested_targets[qi]));
          }
          if (pos < (int)sizeof(multi_core) - 1) {
            int w = snprintf(multi_core + pos, sizeof(multi_core) - pos, "%s%s", (pos ? " " : ""), seg);
            if (w > 0)
              pos += MIN(w, (int)(sizeof(multi_core) - pos - 1));
          }
        }
        multi_core[sizeof(multi_core) - 1] = '\0';
        if (multi_core[0]) {
          ai_sanitize_unresolved_tokens(multi_core, voiced, sizeof(voiced));
          line = voiced;
          rctx.chosen_core = voiced;
          skip_voice = TRUE;
          intention.be_brief = 1;
        }
      }
      if (!line || !*line) {
        switch (role) {
          case ROLE_GUARD: line = "State your business."; break;
          case ROLE_MERCHANT: line = "Ask about wares or rooms."; break;
          case ROLE_BANDIT: line = "Keep moving and keep quiet."; break;
          case ROLE_CULTIST: line = "Mind your words."; break;
          case ROLE_SPIRIT: line = "Speak clearly."; break;
          case ROLE_BOSS: line = "Be brief."; break;
          default: line = "What do you need?"; break;
        }
      }

      if (intention.goal == GOAL_SERVE && rctx.confidence <= 2) {
        const char *epi = ai_epistemic_line(rctx.confidence, role, rctx.topic_target);
        if (epi && *epi) {
          line = epi;
          rctx.chosen_core = epi;
          intention.be_brief = 1;
        }
      }

      if (type == AI_EVENT_PLAYER_SAY && line && *line && rctx.requested_count <= 1) {
        unsigned long dseed = ai_conv_seed(mob, intent, (unsigned int)now);
        if (!repeated_topic && ai_pick_stance_prefix(role, style, dseed, &stance_prefix) && stance_prefix && *stance_prefix && ai_line_is_role_legal(stance_prefix, role, style)) {
          snprintf(decorated, sizeof(decorated), "%s %s", stance_prefix, line);
          line = decorated;
          if (ai_debug)
            ai_debug_log("AI_STANCE_PICK vnum=%d prefix='%s'", GET_MOB_VNUM(mob), stance_prefix);
        }
        if (question_shape) {
          mirror_clause = ai_pick_question_mirror_clause(rctx.primary_topic_target, role, style, dseed);
          if (mirror_clause && *mirror_clause) {
            snprintf(service_line, sizeof(service_line), "%s %s", mirror_clause, line);
            if (strlen(service_line) < sizeof(service_line) - 1) {
              line = service_line;
              if (ai_debug)
                ai_debug_log("AI_MIRROR_PICK vnum=%d mirror='%s'", GET_MOB_VNUM(mob), mirror_clause);
            }
          }
        }
      }

      if (sr && sr->arc == AI_ARC_RAPPORT && !sr->rapport_rumor_used && sr->archetype != AI_ARCH_TRANSACTOR && best_action == AI_ACTION_SPEAK && intention.goal == GOAL_INFORM) {
        static const char *const rapport_rumor_pool[] = {
          "Between us, I heard a traveler mention odd lights near the old road.",
          "Quiet word: someone reported strange lights by the old road.",
          NULL
        };
        line = ai_pick_phrase(rapport_rumor_pool);
        sr->rapport_rumor_used = 1;
      }

      if (!skip_voice && line && *line) {
        vp = ai_voice_profile_get(mob);
        seed = ai_conv_seed(mob, intent, (unsigned int)now);
        rctx.chosen_core = line;
        ai_voice_assemble(mob, vp, &intention, &rctx, seed, voiced, sizeof(voiced));
        line = voiced;
        if (line && *line && !ai_line_is_role_legal(line, role, style)) {
          snprintf(voiced, sizeof(voiced), "%.*s", (int)sizeof(voiced) - 1, core ? core : "");
          line = voiced;
          if (!line[0] || !ai_line_is_role_legal(line, role, style))
            line = ai_role_redirect_line(role, style, TARGET_NONE);
        }
      }

      if (line && *line && !ai_line_is_role_legal(line, role, style)) {
        snprintf(voiced, sizeof(voiced), "%.*s", (int)sizeof(voiced) - 1, core ? core : "");
        line = voiced;
        if (!line[0] || !ai_line_is_role_legal(line, role, style)) {
          const char *redirect = ai_role_redirect_line(role, style, TARGET_NONE);
          if (!redirect || !*redirect || !ai_line_is_role_legal(redirect, role, style))
            redirect = "Let's keep to what I can answer.";
          line = redirect;
        }
      }

      if (line && *line && type == AI_EVENT_PLAYER_SAY && (rctx.domain == DOMAIN_SERVICES || rctx.domain == DOMAIN_DIRECTIONS) && rctx.facts.confidence >= 3) {
        because_clause = ai_role_because_clause(role, style);
        if (because_clause && *because_clause) {
          snprintf(service_line, sizeof(service_line), "%s", line);
          ai_append_clause(service_line, sizeof(service_line), because_clause);
          if (strlen(service_line) < sizeof(service_line) - 1)
            line = service_line;
        }
      }

    }

    if (!line || !*line)
      AI_EVT_RETURN("EMPTY_LINE_AFTER_ASSEMBLY");

    ai_set_last_speech_meta(mob, pool, reason);
    {
      char finalbuf[300];
      ai_sanitize_unresolved_tokens(line, finalbuf, sizeof(finalbuf));
      snprintf(targeted, sizeof(targeted), "$n says to %s, '%s'", GET_NAME(actor), finalbuf);
    }
    ai_actor_schedule_reaction_speech(mob, actor, targeted);
    e->last_reply_time = now;
    if (sr)
      sr->last_reply_time = now;
    if (sr)
      sr->cooldown_until = now + ((best_action == AI_ACTION_EMOTE_REACT) ? 3 : 2);
    if (sr && rctx.event_id >= 0)
      ai_working_mem_mark_answered(sr->working_mem, sr->working_mem_count, sr->working_mem_head, rctx.event_id);
    if (type == AI_EVENT_PLAYER_SAY && sr) {
      sr->last_topic_target = rctx.primary_topic_target;
      sr->last_domain = rctx.domain;
      sr->last_turn_time = now;
      sr->last_player_utter_hash = (unsigned int)ai_text_hash_simple(normalized);
    }

    if (type == AI_EVENT_PLAYER_SAY && selected_template_id >= 0 && IN_ROOM(mob) != NOWHERE) {
      struct ai_player_arb_entry *arb = ai_player_arb_lookup(IN_ROOM(mob), GET_IDNUM(actor), GET_IDNUM(mob), type, ai_text_hash_simple(normalized), now);
      if (arb) {
        if (mob == arb->responder1)
          arb->responder1_template_id = selected_template_id;
        else if (mob == arb->responder2)
          arb->responder2_template_id = selected_template_id;
      }
    }
  }

  cooldown_remaining = (sr && sr->cooldown_until > now) ? (float)(sr->cooldown_until - now) : 0.0f;
  ai_debug_log("AI_EVT vnum=%d role=%s goal=%s mbti=%s tb=%s arch=%s arc=%s ex=%d attn=%.2f susp=%.2f action=%s cd=%.0f",
               GET_MOB_VNUM(mob), ai_role_name_local(mob->ai_prof->role), ai_goal_name(goal ? goal->type : AI_GOAL_NONE), ai_mbti_string(ai_voice_profile_get(mob)),
               ai_time_bucket_name(ctx.time_bucket), ai_arch_name(sr ? sr->archetype : AI_ARCH_UNKNOWN), ai_arc_name(sr ? sr->arc : AI_ARC_STRANGER),
               sr ? sr->exchange_count : 0, attention_score, suspicion, ai_action_name(best_action), cooldown_remaining);

#undef AI_EVT_RETURN
}



void ai_actor_event_enter(struct char_data *actor, room_rnum room)
{
  struct char_data *mob;
  ai_dbg_evt(NULL, "ENTER_DISPATCH", AI_EVENT_PLAYER_ENTER, actor, actor ? GET_NAME(actor) : "");
  if (!actor || IS_NPC(actor) || room == NOWHERE) {
    ai_dbg_evt(NULL, "RETURN_BAD_ACTOR_OR_ROOM", AI_EVENT_PLAYER_ENTER, actor, "");
    return;
  }
  for (mob = world[room].people; mob; mob = mob->next_in_room)
    if (IS_NPC(mob) && MOB_FLAGGED(mob, MOB_AI_ACTOR) && mob != actor) {
      if (!ai_actor_ensure_ready(mob)) {
        ai_dbg_evt(mob, "RETURN_NOT_READY", AI_EVENT_PLAYER_ENTER, actor, "");
        continue;
      }
      ai_dbg_evt(mob, "DISPATCH", AI_EVENT_PLAYER_ENTER, actor, actor->player.short_descr ? actor->player.short_descr : actor->player.name);
      ai_actor_on_room_event(mob, AI_EVENT_PLAYER_ENTER, actor, actor->player.short_descr ? actor->player.short_descr : actor->player.name);
    }
}

void ai_actor_event_leave(struct char_data *actor, room_rnum room)
{
  struct char_data *mob;
  ai_dbg_evt(NULL, "LEAVE_DISPATCH", AI_EVENT_PLAYER_LEAVE, actor, "");
  if (!actor || IS_NPC(actor) || room == NOWHERE) {
    ai_dbg_evt(NULL, "RETURN_BAD_ACTOR_OR_ROOM", AI_EVENT_PLAYER_LEAVE, actor, "");
    return;
  }
  for (mob = world[room].people; mob; mob = mob->next_in_room)
    if (IS_NPC(mob) && MOB_FLAGGED(mob, MOB_AI_ACTOR) && mob != actor) {
      if (!ai_actor_ensure_ready(mob)) {
        ai_dbg_evt(mob, "RETURN_NOT_READY", AI_EVENT_PLAYER_LEAVE, actor, "");
        continue;
      }
      ai_dbg_evt(mob, "DISPATCH", AI_EVENT_PLAYER_LEAVE, actor, "");
      ai_actor_on_room_event(mob, AI_EVENT_PLAYER_LEAVE, actor, NULL);
    }
}

void ai_actor_event_say(struct char_data *actor, const char *msg)
{
  struct char_data *mob;
  ai_dbg_evt(NULL, "SAY_DISPATCH", AI_EVENT_PLAYER_SAY, actor, msg ? msg : "");
  if (!actor || IS_NPC(actor) || IN_ROOM(actor) == NOWHERE) {
    ai_dbg_evt(NULL, "RETURN_BAD_ACTOR_OR_ROOM", AI_EVENT_PLAYER_SAY, actor, msg ? msg : "");
    return;
  }
  for (mob = world[IN_ROOM(actor)].people; mob; mob = mob->next_in_room)
    if (IS_NPC(mob) && MOB_FLAGGED(mob, MOB_AI_ACTOR) && mob != actor) {
      if (!ai_actor_ensure_ready(mob)) {
        ai_dbg_evt(mob, "RETURN_NOT_READY", AI_EVENT_PLAYER_SAY, actor, msg ? msg : "");
        continue;
      }
      ai_dbg_evt(mob, "DISPATCH", AI_EVENT_PLAYER_SAY, actor, msg ? msg : "");
      ai_actor_on_room_event(mob, AI_EVENT_PLAYER_SAY, actor, msg ? msg : "");
    }
}

void ai_actor_event_emote(struct char_data *actor, const char *msg)
{
  struct char_data *mob;
  if (!actor || IS_NPC(actor) || IN_ROOM(actor) == NOWHERE) return;
  for (mob = world[IN_ROOM(actor)].people; mob; mob = mob->next_in_room)
    if (IS_NPC(mob) && MOB_FLAGGED(mob, MOB_AI_ACTOR) && mob != actor) {
      if (!mob->ai_state || !mob->ai_state->brain) ai_actor_init(mob);
      ai_actor_on_room_event(mob, AI_EVENT_PLAYER_EMOTE, actor, msg ? msg : "");
    }
}

void ai_actor_event_combat_start(struct char_data *attacker, struct char_data *victim)
{
  struct char_data *mob;
  room_rnum room;
  if (!attacker || !victim || IN_ROOM(attacker) == NOWHERE) return;
  room = IN_ROOM(attacker);
  for (mob = world[room].people; mob; mob = mob->next_in_room)
    if (IS_NPC(mob) && MOB_FLAGGED(mob, MOB_AI_ACTOR) && mob != attacker && mob != victim) {
      if (!mob->ai_state || !mob->ai_state->brain) ai_actor_init(mob);
      ai_actor_on_room_event(mob, AI_EVENT_COMBAT_START, attacker, "combat");
    }
}

void ai_actor_event_corpse(struct char_data *dead, room_rnum room)
{
  struct char_data *mob;
  if (room == NOWHERE || !ai_actor_brain_enabled()) return;
  for (mob = world[room].people; mob; mob = mob->next_in_room)
    if (IS_NPC(mob) && MOB_FLAGGED(mob, MOB_AI_ACTOR)) {
      if (!mob->ai_state || !mob->ai_state->brain) ai_actor_init(mob);
      ai_actor_brain_on_corpse(mob, dead);
    }
}

void ai_actor_event_drop(struct char_data *actor, struct obj_data *obj)
{
  struct char_data *mob;
  if (!actor || IS_NPC(actor) || IN_ROOM(actor) == NOWHERE || !ai_actor_brain_enabled()) return;
  for (mob = world[IN_ROOM(actor)].people; mob; mob = mob->next_in_room)
    if (IS_NPC(mob) && MOB_FLAGGED(mob, MOB_AI_ACTOR) && mob != actor) {
      if (!mob->ai_state || !mob->ai_state->brain) ai_actor_init(mob);
      ai_actor_brain_on_drop(mob, actor, obj);
    }
}

void ai_actor_event_give(struct char_data *actor, struct char_data *to, struct obj_data *obj)
{
  if (!to || !IS_NPC(to) || !MOB_FLAGGED(to, MOB_AI_ACTOR) || !ai_actor_brain_enabled()) return;
  if (!to->ai_state || !to->ai_state->brain) ai_actor_init(to);
  ai_actor_brain_on_give(to, actor, obj, to);
}

/*
 * AI Actor testing checklist:
 * 1) Spawn an AI guard and confirm patrol and arrest response.
 * 2) Spawn an AI merchant and confirm trade refusal after theft or attack.
 * 3) Spawn an AI bandit and confirm ambush behavior.
 * 4) Confirm non-AI mobs remain unchanged.
 * 5) Confirm no speech spam and no noticeable performance spikes.
 */
