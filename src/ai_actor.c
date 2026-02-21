#include "conf.h"
#include "sysdep.h"

#include <ctype.h>
#include <math.h>

#include "structs.h"
#include "utils.h"
#include "db.h"
#include "comm.h"
#include "handler.h"
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

static struct char_data *ai_find_player_by_idnum_room(struct char_data *mob, long idnum);
static const char *ai_pick_phrase(const char *const *pool);
static const char *ai_pool_pick(const char *const *pool);
static int ai_role_can_give_directions(int role);
static int ai_role_can_answer_intent(int role, int style, int intent);
static const char *ai_role_redirect_line(int role, int style);
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
static unsigned long ai_hash_mix(unsigned long h, unsigned long v);
static unsigned long ai_hash_text_stable(const char *text);
static unsigned long ai_conv_seed(struct char_data *mob, int intent, unsigned int counter);
static int ai_template_pick_index(const int *ids, int count, unsigned long seed, int avoid_id, int avoid_prev, int avoid_recent1, int avoid_recent2);
static struct ai_conv_reply_state *ai_conv_reply_state_get(struct char_data *mob, int create);
static int ai_is_weather_smalltalk(const char *text);
static const char *ai_synonym_pick(const struct ai_synonym_group *groups, const char *token, unsigned long seed, int slot);
static void ai_template_expand(const char *tpl, const struct ai_synonym_group *groups, unsigned long seed, char *out, size_t outsz);
static const char *ai_template_reply_for_intent(struct char_data *mob, int intent, const char *text, int avoid_template_id, int *out_template_id);
static void ai_voice_profile_derive(struct char_data *mob, struct ai_voice_profile *out);
static const struct ai_voice_profile *ai_voice_profile_get(struct char_data *mob);
static const char *ai_word(const char *concept, int tier);
static const char *ai_word_sn(const char *concept, int vocab_tier, int sn);
static const char *ai_phrase(const char *tag, int tier, int rhythm, unsigned long seed, int slot);
static const char *ai_mbti_string(const struct ai_voice_profile *vp);
static void ai_mbti_compound_modifier(const struct ai_voice_profile *vp, int speech_act, int *out_add_followup_question, int *out_add_topic_lean, int *out_suppress_opener, int *out_use_emotional_color, unsigned long seed);
static int ai_line_is_role_legal(const char *line, int role, int style);
static struct ai_reply_intention ai_form_intention(struct char_data *mob, int speech_act, int speech_class, int suspicion_bucket, int arc_state, const struct ai_context_vector *ctx, const struct ai_session_read_entry *sr, struct ai_actor_memory_entry *e, time_t now);
static const char *ai_select_content_for_intention(struct char_data *mob, const struct ai_reply_intention *in, const char *player_text, int *out_from_template);
static void ai_voice_assemble(struct char_data *mob, const struct ai_voice_profile *vp, const struct ai_reply_intention *in, int speech_act, const char *core_content, unsigned long seed, char *out, size_t outsz);
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

  snprintf(tmp, sizeof(tmp), "%s %s %s %s", parts[0], parts[1], parts[2], parts[3]);

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
    snprintf(saybuf, sizeof(saybuf), "%s", msg + 3);
    do_echo(mob, saybuf, 0, SCMD_EMOTE);
  } else {
    snprintf(saybuf, sizeof(saybuf), "%s", msg);
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
  snprintf(st->pending_speech, sizeof(st->pending_speech), "%s", msg);
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

static void ai_voice_profile_derive(struct char_data *mob, struct ai_voice_profile *out)
{
  unsigned long seed;
  unsigned long long seed64;
  int role;
  int zone = (mob && IN_ROOM(mob) != NOWHERE) ? world[IN_ROOM(mob)].zone : 0;
  if (!mob || !out || !mob->ai_prof) return;
  role = mob->ai_prof->role;
  seed = ai_hash_mix(ai_hash_mix((unsigned long)GET_MOB_VNUM(mob), (unsigned long)role), (unsigned long)zone);
  out->vocab_tier = seed % 4;
  out->rhythm = (seed >> 4) % 4;
  out->hedge_style = (seed >> 8) % 4;
  out->topic_lean = (seed >> 12) % 5;
  out->tic_index = (seed >> 16) % 8;
  out->opener_index = (seed >> 20) % 6;
  out->closer_index = (seed >> 24) % 6;
  out->intensity = (seed >> 28) % 3;
  seed64 = ((unsigned long long)seed << 32) | ai_hash_mix(seed, (unsigned long)GET_MOB_VNUM(mob));
  out->mbti_ei = (int)((seed64 >> 32) % 2ULL);
  out->mbti_sn = (int)((seed64 >> 34) % 2ULL);
  out->mbti_tf = (int)((seed64 >> 36) % 2ULL);
  out->mbti_jp = (int)((seed64 >> 38) % 2ULL);
  if (role == ROLE_BEAST) { out->vocab_tier = 0; out->rhythm = (out->rhythm % 2) ? 2 : 0; out->intensity = out->intensity ? 1 : 0; out->mbti_ei=0; out->mbti_sn=0; out->mbti_tf=0; out->mbti_jp=0; }
  if (role == ROLE_UNDEAD) { out->vocab_tier = 1 + (out->vocab_tier % 2); out->rhythm = (out->rhythm % 2) ? 2 : 0; out->hedge_style = (out->hedge_style % 2) ? 3 : 0; out->mbti_ei=0; out->mbti_jp=0; }
  if (role == ROLE_SPIRIT) { out->vocab_tier = 2 + (out->vocab_tier % 2); out->rhythm = (out->rhythm % 2) ? 3 : 1; out->mbti_sn=1; out->mbti_ei=0; }
  if (role == ROLE_GUARD) { if (out->vocab_tier > 2) out->vocab_tier = 2; out->hedge_style = (out->hedge_style % 2) ? 3 : 0; out->mbti_jp=0; if (out->mbti_ei && ((seed64>>40)%3ULL==0ULL)) out->mbti_ei=0; if (out->mbti_tf && ((seed64>>42)%4ULL!=0ULL)) out->mbti_tf=0; }
  if (role == ROLE_CULTIST) { out->hedge_style = 2; out->topic_lean = 4; out->mbti_sn=1; if ((seed64>>41)%3ULL!=0ULL) out->mbti_ei=0; if ((seed64>>42)%4ULL!=0ULL) out->mbti_tf=0; }
  if (role == ROLE_BOSS) { out->hedge_style = (out->hedge_style % 2) ? 3 : 0; if (out->vocab_tier == 0) out->vocab_tier = 1; out->mbti_jp=0; if (out->mbti_ei && ((seed64>>40)%3ULL==0ULL)) out->mbti_ei=0; if (out->mbti_tf && ((seed64>>42)%4ULL!=0ULL)) out->mbti_tf=0; }
  if (role == ROLE_MERCHANT) { out->mbti_ei = ((seed64>>43)%3ULL==0ULL)?0:1; out->mbti_sn = ((seed64>>44)%3ULL==0ULL)?1:0; out->mbti_jp = ((seed64>>45)%3ULL==0ULL)?1:0; }
  if (role == ROLE_BANDIT) { if ((seed64>>46)%4ULL!=0ULL) out->mbti_tf=0; if ((seed64>>47)%3ULL!=0ULL) out->mbti_jp=1; }
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

static const char *ai_select_content_for_intention(struct char_data *mob, const struct ai_reply_intention *in, const char *player_text, int *out_from_template)
{
  static char best[256];
  const char *cands[8];
  int score[8];
  int n = 0;
  int i;
  int role = (mob && mob->ai_prof) ? mob->ai_prof->role : ROLE_UNKNOWN;
  int style = (mob && mob->ai_prof) ? mob->ai_prof->style : 0;
  int role_fit = ai_role_can_answer_intent(role, style, in ? in->topic : AI_INTENT_NONE) ? 20 : 0;
  static const char *const clarify_pool[] = {"What do you mean?", "Could you say that another way?", "Can you be more clear?", NULL};
  static const char *const deflect_guard[] = {"State your need and keep it brief.", "I have duty to keep.", NULL};
  static const char *const deflect_inn[] = {"If you need rest, ask for a room.", "I can offer a meal, not gossip.", NULL};
  static const char *const deflect_merch[] = {"Ask about wares or prices.", "Trade talk only for now.", NULL};
  static const char *const deflect_bandit[] = {"Keep walking.", "Not your concern, mark.", NULL};
  static const char *const deflect_cult[] = {"The pattern is not for you.", "That lies beyond your sight.", NULL};
  static const char *const warn_strong[] = {"Watch yourself and state your business.", "Careful now, or move along.", NULL};
  static const char *const warn_mild[] = {"Easy now. Keep it civil.", "Let's keep this calm.", NULL};
  static const char *const dismiss_pool[] = {"Keep it brief.", "Not today.", "Move along.", NULL};
  static const char *const connect_guard[] = {"Well met. Keep to the law.", "Greetings. Stay alert.", NULL};
  static const char *const connect_inn[] = {"Welcome. Warm yourself by the hearth.", "Good day. Rest here if you need.", NULL};
  static const char *const connect_merch[] = {"Hello. Looking for wares?", "Greetings. Prices are fair today.", NULL};
  static const char *const connect_bandit[] = {"Yeah?", "You talking to me?", NULL};
  static const char *const connect_cult[] = {"The veil stirs. Speak.", "I hear the currents. Go on.", NULL};
  static const char *const serve_shop[] = {"I can show you wares and prices.", "Tell me what stock you seek.", NULL};
  static const char *const serve_inn[] = {"We have warm beds and stew.", "You can rest here and get ale.", NULL};
  static const char *const serve_bank[] = {"The bank is east from here.", "Ask at the vault counter in town.", NULL};
  static const char *const serve_dir[] = {"Head north, then east at the square.", "Follow the main road and ask at the post.", NULL};

  if (out_from_template)
    *out_from_template = 0;
  best[0] = '\0';
  if (!in)
    return "What do you mean?";

  if (in->goal == GOAL_CLARIFY) {
    snprintf(best, sizeof(best), "%s", ai_pick_phrase(clarify_pool));
    return best;
  }

  if ((in->goal == GOAL_INFORM || in->goal == GOAL_SERVE) && role_fit < AI_MIN_ROLE_FITNESS)
    { const char *fallback = ai_role_redirect_line(role, style); return fallback ? fallback : "I'd rather not discuss that."; }

  if (in->goal == GOAL_DEFLECT) {
    const char *pick = (role == ROLE_GUARD || role == ROLE_BOSS) ? ai_pick_phrase(deflect_guard) :
                       ((role == ROLE_MERCHANT && style == 1) ? ai_pick_phrase(deflect_inn) :
                       (role == ROLE_MERCHANT ? ai_pick_phrase(deflect_merch) :
                       (role == ROLE_BANDIT ? ai_pick_phrase(deflect_bandit) : ai_pick_phrase(deflect_cult))));
    cands[n++] = pick;
  } else if (in->goal == GOAL_WARN) {
    cands[n++] = ai_pick_phrase((role == ROLE_GUARD || role == ROLE_BOSS) ? warn_strong : warn_mild);
  } else if (in->goal == GOAL_DISMISS) {
    cands[n++] = ai_pick_phrase(dismiss_pool);
  } else if (in->goal == GOAL_CONNECT) {
    cands[n++] = (role == ROLE_GUARD || role == ROLE_BOSS) ? ai_pick_phrase(connect_guard) :
                 ((role == ROLE_MERCHANT && style == 1) ? ai_pick_phrase(connect_inn) :
                 (role == ROLE_MERCHANT ? ai_pick_phrase(connect_merch) :
                 (role == ROLE_BANDIT ? ai_pick_phrase(connect_bandit) : ai_pick_phrase(connect_cult))));
  } else if (in->goal == GOAL_SERVE) {
    if (in->topic == AI_INTENT_INN)
      cands[n++] = ai_pick_phrase(serve_inn);
    else if (in->topic == AI_INTENT_BANK)
      cands[n++] = ai_pick_phrase(serve_bank);
    else if (in->topic == AI_INTENT_DIRECTIONS)
      cands[n++] = ai_pick_phrase(serve_dir);
    else
      cands[n++] = ai_pick_phrase(serve_shop);
  } else {
    if (in->topic == AI_INTENT_SMALLTALK)
      cands[n++] = "Things are steady enough.";
    else if (in->topic == AI_INTENT_QUEST)
      cands[n++] = "Ask clearly and I'll answer what I can.";
    else
      cands[n++] = ai_role_redirect_line(role, style);
  }

  if (in->goal == GOAL_INFORM || in->goal == GOAL_SERVE) {
    const char *tpl = ai_template_reply_for_intent(mob, in->topic, player_text ? player_text : "", -1, NULL);
    if (tpl && n < 8)
      cands[n++] = tpl;
  }

  for (i = 0; i < n; i++) {
    const char *cand = cands[i];
    score[i] = 0;
    if (!cand || !*cand || !ai_line_is_role_legal(cand, role, style)) {
      score[i] = -999;
      continue;
    }
    if (in->be_brief)
      score[i] += (strlen(cand) <= 72) ? 5 : -3;
    if (in->stance == STANCE_WARM && (ai_text_has_sub_ci(cand, "welcome") || ai_text_has_sub_ci(cand, "warm") || ai_text_has_sub_ci(cand, "rest")))
      score[i] += 4;
    if ((in->stance == STANCE_HOSTILE || in->stance == STANCE_GUARDED) && (ai_text_has_sub_ci(cand, "watch") || ai_text_has_sub_ci(cand, "brief") || ai_text_has_sub_ci(cand, "move along")))
      score[i] += 4;
    if (player_text && *player_text && ai_text_has_sub_ci(player_text, "food") && ai_text_has_sub_ci(cand, "stew"))
      score[i] += 3;
  }

  {
    int bi = -1;
    int bs = -1000;
    for (i = 0; i < n; i++) {
      if (score[i] > bs) {
        bs = score[i];
        bi = i;
      }
    }
    if (bi >= 0 && bs > -900) {
      snprintf(best, sizeof(best), "%s", cands[bi]);
      return best;
    }
  }

  {
    const char *fallback = ai_role_redirect_line(role, style);
    if (!fallback) fallback = "I'd rather not discuss that.";
    snprintf(best, sizeof(best), "%s", fallback);
  }
  return best;
}

static void ai_voice_assemble(struct char_data *mob, const struct ai_voice_profile *vp, const struct ai_reply_intention *in, int speech_act, const char *core_content, unsigned long seed, char *out, size_t outsz)
{
  char core[256], work[512], topic_tag[24], buf[128];
  int rhythm, add_q, add_topic, suppress_opener, use_emotional;
  struct ai_conv_actor_state *st = ai_conv_actor_state_get(mob, 0);
  const char *opener = "", *closer = "", *hedge = "", *topic = "";
  size_t len;

  if (!out || outsz == 0)
    return;
  ai_voice_apply_tokens(vp, core_content ? core_content : "", core, sizeof(core));

  if (in && in->goal == GOAL_CLARIFY) {
    snprintf(out, outsz, "%s", core);
    len = strlen(out);
    if (len == 0 || out[len - 1] != '?')
      snprintf(out, outsz, "What do you mean?");
    return;
  }

  ai_mbti_compound_modifier(vp, speech_act, &add_q, &add_topic, &suppress_opener, &use_emotional, seed);
  if (st && st->tone_no_extras) {
    add_q = 0;
    add_topic = 0;
  }
  if (st && st->tone_clipped)
    suppress_opener = 1;
  if (in && in->be_brief) {
    add_q = 0;
    add_topic = 0;
  }

  rhythm = vp ? vp->rhythm : 1;
  if (vp && vp->mbti_ei && rhythm == 0) rhythm = 1;
  if (vp && !vp->mbti_ei && rhythm == 3) rhythm = 2;
  if (vp && vp->hedge_style == 1) hedge = ai_phrase("HEDGE_UNCERTAIN", vp->vocab_tier, rhythm, seed, 2);
  else if (vp && vp->hedge_style == 2) hedge = ai_phrase("HEDGE_EVASIVE", vp->vocab_tier, rhythm, seed, 3);
  else if (vp && vp->hedge_style == 3) hedge = ai_phrase("HEDGE_CONFIDENT", vp->vocab_tier, rhythm, seed, 4);
  opener = suppress_opener ? "" : ai_phrase("OPENER", vp ? vp->vocab_tier : 1, rhythm, seed, vp ? vp->opener_index : 0);
  closer = ai_phrase("CLOSER", vp ? vp->vocab_tier : 1, rhythm, seed, vp ? vp->closer_index : 1);

  if (in && (in->goal == GOAL_DEFLECT || in->goal == GOAL_DISMISS || in->goal == GOAL_SERVE)) {
    opener = "";
    hedge = "";
    add_topic = 0;
    add_q = 0;
    use_emotional = (in->goal == GOAL_SERVE && mob && mob->ai_prof && mob->ai_prof->role == ROLE_MERCHANT && mob->ai_prof->style == 1);
  }
  if (in && in->goal == GOAL_WARN) {
    opener = "";
    hedge = "";
    add_q = 0;
    add_topic = 0;
    use_emotional = 0;
    if (!(vp && vp->rhythm >= 2 && in->stance != STANCE_HOSTILE))
      rhythm = 0;
  }

  snprintf(topic_tag, sizeof(topic_tag), "TOPIC_%s", (vp && vp->topic_lean==0)?"DUTY":(vp&&vp->topic_lean==1)?"TRADE":(vp&&vp->topic_lean==2)?"COMFORT":(vp&&vp->topic_lean==3)?"DANGER":"MYSTERY");
  topic = ai_phrase(topic_tag, vp ? vp->vocab_tier : 1, rhythm, seed, 5);
  if (!ai_line_is_role_legal(topic, mob->ai_prof->role, mob->ai_prof->style))
    topic = "";
  if (!ai_line_is_role_legal(closer, mob->ai_prof->role, mob->ai_prof->style))
    closer = "";

  if (rhythm == 0 || (st && st->tone_clipped))
    snprintf(work, sizeof(work), "%s", core);
  else if (rhythm == 1)
    snprintf(work, sizeof(work), "%s%s%s%s%s.", opener, *opener?" ":"", hedge, *hedge?" ":"", core);
  else if (rhythm == 2)
    snprintf(work, sizeof(work), "%s", core);
  else
    snprintf(work, sizeof(work), "%s %s %s.", opener, hedge, core);

  if (in && in->goal == GOAL_CONNECT) {
    if (closer && *closer)
      snprintf(work + strlen(work), sizeof(work) - strlen(work), " %s", closer);
  } else if (in && in->goal == GOAL_INFORM) {
    if (!in->be_brief && add_topic && topic && *topic)
      snprintf(work + strlen(work), sizeof(work) - strlen(work), " %s.", topic);
    if (closer && *closer)
      snprintf(work + strlen(work), sizeof(work) - strlen(work), " %s", closer);
  } else if (in && (in->goal == GOAL_DEFLECT || in->goal == GOAL_DISMISS || in->goal == GOAL_WARN || in->goal == GOAL_SERVE)) {
    if (closer && *closer)
      snprintf(work + strlen(work), sizeof(work) - strlen(work), " %s", closer);
  } else {
    if (add_topic && topic && *topic)
      snprintf(work + strlen(work), sizeof(work) - strlen(work), " %s.", topic);
    if (closer && *closer)
      snprintf(work + strlen(work), sizeof(work) - strlen(work), " %s", closer);
  }

  if (use_emotional) {
    const char *feel = (speech_act==AI_INTENT_PRAISE||speech_act==AI_INTENT_GREET||speech_act==AI_INTENT_SMALLTALK) ? ai_phrase("FEEL_POSITIVE", vp->vocab_tier, rhythm, seed, 7) : ai_phrase("FEEL_NEGATIVE", vp->vocab_tier, rhythm, seed, 8);
    snprintf(buf, sizeof(buf), " %s", feel);
    snprintf(work + strlen(work), sizeof(work) - strlen(work), "%s", buf);
  }

  if (in && in->goal == GOAL_CONNECT && vp && vp->mbti_ei && in->stance != STANCE_HOSTILE && !(st && st->tone_no_extras))
    snprintf(work + strlen(work), sizeof(work) - strlen(work), " %s", ai_followup_pick(speech_act, seed));

  len = strlen(work);
  if (len > 180) {
    size_t cut = 180;
    while (cut > 0 && work[cut] != '.' && work[cut] != '?' && work[cut] != '!')
      cut--;
    if (cut == 0)
      cut = 180;
    work[cut] = '\0';
  }

  snprintf(out, outsz, "%s", work);
  if (ai_debug)
    ai_debug_log("VOICE vnum=%d role=%s tier=%d rhythm=%d tic=%d mbti=%s out=%s", GET_MOB_VNUM(mob), ai_role_name_local(mob->ai_prof->role), vp->vocab_tier, vp->rhythm, vp->tic_index, ai_mbti_string(vp), out);
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
                                   room_rnum *out_room, int *out_first_dir)
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

static const char *ai_role_redirect_line(int role, int style)
{
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
  if (needles && IN_ROOM(mob) != NOWHERE && ai_bfs_find_target_room(IN_ROOM(mob), AI_BFS_MAX_DEPTH, needles, &found, &first_dir)) {
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

static const char *ai_line_for_intent(struct char_data *mob, struct ai_actor_memory_entry *e, int intent, int attitude, const char *text, enum ai_action_type action, int avoid_template_id, int *out_template_id, const char **out_pool, const char **out_reason)
{
  static char line[224];
  static char voiced[224];
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
    core = ai_role_redirect_line(role, style);
    skip_voice = TRUE;
    goto finalize;
  }

  if (intent == AI_INTENT_DIRECTIONS || intent == AI_INTENT_BUY_WEAPON || intent == AI_INTENT_BUY_ARMOR || intent == AI_INTENT_BUY_FOOD ||
      intent == AI_INTENT_INN || intent == AI_INTENT_BANK || intent == AI_INTENT_HEAL || intent == AI_INTENT_TRAIN) {
    topic = ai_detect_topic_target_from_text(text);
    if (topic == TARGET_NONE && e && (time(0) - e->last_topic_time) <= AI_TOPIC_MEMORY_WINDOW_SECS)
      topic = e->last_topic;
    if (topic != TARGET_NONE)
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
    core = ai_role_redirect_line(role, style);
    skip_voice = TRUE;
  }

  if (core && !ai_line_is_role_legal(core, role, style))
    core = NULL;

  if (!core && intent == AI_INTENT_CONFUSION)
    core = "What do you mean?";
  if (!core)
    core = "I am not sure I follow. Ask me in another way.";

finalize:
  if (skip_voice || !core)
    return core;
  vp = ai_voice_profile_get(mob);
  ai_voice_assemble(mob, vp, NULL, intent, core, seed, voiced, sizeof(voiced));
  snprintf(line, sizeof(line), "%s", voiced);
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
  int intent;
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

  memset(scores, 0, sizeof(scores));
  if (out_confidence) *out_confidence = 0;
  if (out_is_weather) *out_is_weather = FALSE;
  if (!text || !*text)
    return AI_SPEECH_UNKNOWN;

  if (ai_text_has_sub_ci(text, "hello") || ai_text_has_sub_ci(text, "hi") || ai_text_has_sub_ci(text, "hey") ||
      ai_text_has_sub_ci(text, "greetings") || ai_text_has_sub_ci(text, "yo") || ai_text_has_sub_ci(text, "good morning"))
    scores[AI_SPEECH_GREET] += 8;

  if (ai_text_has_sub_ci(text, "weather") || ai_text_has_sub_ci(text, "rain") || ai_text_has_sub_ci(text, "sun") || ai_text_has_sub_ci(text, "storm") ||
      ai_text_has_sub_ci(text, "wind") || ai_text_has_sub_ci(text, "snow") || ai_text_has_sub_ci(text, "fog") || ai_text_has_sub_ci(text, "nice day")) {
    scores[AI_SPEECH_WEATHER] += 12;
    if (out_is_weather) *out_is_weather = TRUE;
  }

  if (ai_text_has_sub_ci(text, "how are you") || ai_text_has_sub_ci(text, "what s up") || ai_text_has_sub_ci(text, "how goes") ||
      ai_text_has_sub_ci(text, "how is it going") || ai_text_has_sub_ci(text, "what are you doing") || ai_text_has_sub_ci(text, "tell me about"))
    scores[AI_SPEECH_SMALLTALK] += 9;
  if (ai_text_has_sub_ci(text, "chat") || ai_text_has_sub_ci(text, "talk") || ai_text_has_sub_ci(text, "bored") || ai_text_has_sub_ci(text, "day going"))
    scores[AI_SPEECH_SMALLTALK] += 4;

  if (ai_text_has_sub_ci(text, "where") || ai_text_has_sub_ci(text, "which way") || ai_text_has_sub_ci(text, "how do i get") ||
      ai_text_has_sub_ci(text, "directions") || ai_text_has_sub_ci(text, "find") || ai_text_has_sub_ci(text, "locate") || ai_text_has_sub_ci(text, "path"))
    scores[AI_SPEECH_DIRECTIONS] += 10;

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
    scores[AI_SPEECH_SMALLTALK] += 2;

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

static struct ai_player_arb_entry *ai_player_arb_lookup(room_rnum room, long actor_id, enum ai_event_type type, unsigned long text_hash, time_t now)
{
  int i;

  for (i = 0; i < AI_PLAYER_ARB_CACHE_MAX; i++) {
    struct ai_player_arb_entry *e = &ai_player_arb_cache[i];
    if (e->created_at > 0 && (now - e->created_at) > AI_PLAYER_ARB_TTL_SECS)
      memset(e, 0, sizeof(*e));
    if (e->created_at <= 0)
      continue;
    if (e->room == room && e->actor_id == actor_id && e->type == type && e->text_hash == text_hash)
      return e;
  }

  return NULL;
}

static struct ai_player_arb_entry *ai_player_arb_get_or_create(room_rnum room, long actor_id, enum ai_event_type type, unsigned long text_hash, time_t now)
{
  struct ai_player_arb_entry *found;
  int i;
  int oldest = 0;

  found = ai_player_arb_lookup(room, actor_id, type, text_hash, now);
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
  arb = ai_player_arb_get_or_create(IN_ROOM(mob), GET_IDNUM(actor), type, text_hash, now);
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
    if ((now - it->ai_state->last_talk_time) < AI_PER_PLAYER_REPLY_COOLDOWN_SECS)
      continue;

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
  int intent;
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

  if (!mob || !actor || !mob->ai_prof || !mob->ai_state || IS_NPC(actor))
    return;

  ai_state_refresh_local_topics(mob);
  ai_normalize_text(text ? text : "", normalized, sizeof(normalized));
  attention_score = ai_attention_score(mob, type, actor, normalized, now);
  ai_state_push_event(mob, type, actor, normalized);

  if (attention_score < AI_ATTENTION_THRESHOLD) {
    ai_debug_log("ai_skip_attention vnum=%d event=%s score=%.2f", GET_MOB_VNUM(mob), ai_event_reason_name(type), attention_score);
    return;
  }

  ai_context_vector_build(mob, actor, now, &ctx);

  e = ai_mem_get_or_create(mob, GET_IDNUM(actor));
  if (!e)
    return;

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
    ai_debug_log("AI_EVT vnum=%d role=%s mbti=%s tb=%s arch=%s arc=%s ex=%d attn=%.2f susp=%.2f action=%s cd=%.0f",
                 GET_MOB_VNUM(mob), ai_role_name_local(mob->ai_prof->role), ai_mbti_string(ai_voice_profile_get(mob)),
                 ai_time_bucket_name(ctx.time_bucket), ai_arch_name(sr ? sr->archetype : AI_ARCH_UNKNOWN), ai_arc_name(sr ? sr->arc : AI_ARC_STRANGER),
                 sr ? sr->exchange_count : 0, attention_score, suspicion, ai_action_name(best_action), cooldown_remaining);
    return;
  }

  if (!ai_actor_room_response_slot(mob, actor, type, intent, confidence, normalized))
    return;

  if (type == AI_EVENT_PLAYER_SAY && IN_ROOM(mob) != NOWHERE) {
    struct ai_player_arb_entry *arb = ai_player_arb_lookup(IN_ROOM(mob), GET_IDNUM(actor), type, ai_text_hash_simple(normalized), now);
    if (arb) {
      if (mob == arb->responder2)
        avoid_template_id = arb->responder1_template_id;
      else if (mob == arb->responder1)
        avoid_template_id = arb->responder2_template_id;
    }
  }
  if ((now - e->last_reply_time) < AI_PER_PLAYER_REPLY_COOLDOWN_SECS)
    return;

  {
    const char *pool = "POOL_NONE";
    const char *reason = ai_event_reason_name(type);
    char targeted[256];

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
      static char voiced[224];

      intention = ai_form_intention(mob, intent, speech_class, suspicion_bucket, sr ? sr->arc : AI_ARC_STRANGER, &ctx, sr, e, now);
      if (best_action == AI_ACTION_SPEAK_WARN)
        intention.goal = GOAL_WARN;
      else if (best_action == AI_ACTION_SPEAK_DEFLECT)
        intention.goal = GOAL_DEFLECT;

      core = ai_select_content_for_intention(mob, &intention, normalized, NULL);
      line = core;

      if (intention.goal == GOAL_SERVE && (intention.topic == AI_INTENT_DIRECTIONS || intention.topic == AI_INTENT_BANK || intention.topic == AI_INTENT_INN)) {
        const char *dir = ai_direction_line(mob, ai_detect_topic_target_from_text(normalized));
        if (dir && *dir) {
          line = dir;
          skip_voice = TRUE;
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
        ai_voice_assemble(mob, vp, &intention, intent, line, seed, voiced, sizeof(voiced));
        line = voiced;
      }

    }

    if (!line || !*line)
      return;

    ai_set_last_speech_meta(mob, pool, reason);
    snprintf(targeted, sizeof(targeted), "$n says to %s, '%s'", GET_NAME(actor), line);
    ai_actor_schedule_reaction_speech(mob, actor, targeted);
    e->last_reply_time = now;
    if (sr)
      sr->cooldown_until = now + ((best_action == AI_ACTION_EMOTE_REACT) ? 3 : 2);

    if (type == AI_EVENT_PLAYER_SAY && selected_template_id >= 0 && IN_ROOM(mob) != NOWHERE) {
      struct ai_player_arb_entry *arb = ai_player_arb_lookup(IN_ROOM(mob), GET_IDNUM(actor), type, ai_text_hash_simple(normalized), now);
      if (arb) {
        if (mob == arb->responder1)
          arb->responder1_template_id = selected_template_id;
        else if (mob == arb->responder2)
          arb->responder2_template_id = selected_template_id;
      }
    }
  }

  cooldown_remaining = (sr && sr->cooldown_until > now) ? (float)(sr->cooldown_until - now) : 0.0f;
  ai_debug_log("AI_EVT vnum=%d role=%s mbti=%s tb=%s arch=%s arc=%s ex=%d attn=%.2f susp=%.2f action=%s cd=%.0f",
               GET_MOB_VNUM(mob), ai_role_name_local(mob->ai_prof->role), ai_mbti_string(ai_voice_profile_get(mob)),
               ai_time_bucket_name(ctx.time_bucket), ai_arch_name(sr ? sr->archetype : AI_ARCH_UNKNOWN), ai_arc_name(sr ? sr->arc : AI_ARC_STRANGER),
               sr ? sr->exchange_count : 0, attention_score, suspicion, ai_action_name(best_action), cooldown_remaining);
}



void ai_actor_event_enter(struct char_data *actor, room_rnum room)
{
  struct char_data *mob;
  if (!actor || IS_NPC(actor) || room == NOWHERE) return;
  for (mob = world[room].people; mob; mob = mob->next_in_room)
    if (IS_NPC(mob) && MOB_FLAGGED(mob, MOB_AI_ACTOR) && mob != actor) {
      if (!mob->ai_state || !mob->ai_state->brain) ai_actor_init(mob);
      ai_actor_on_room_event(mob, AI_EVENT_PLAYER_ENTER, actor, actor->player.short_descr ? actor->player.short_descr : actor->player.name);
    }
}

void ai_actor_event_leave(struct char_data *actor, room_rnum room)
{
  struct char_data *mob;
  if (!actor || IS_NPC(actor) || room == NOWHERE) return;
  for (mob = world[room].people; mob; mob = mob->next_in_room)
    if (IS_NPC(mob) && MOB_FLAGGED(mob, MOB_AI_ACTOR) && mob != actor) {
      if (!mob->ai_state || !mob->ai_state->brain) ai_actor_init(mob);
      ai_actor_on_room_event(mob, AI_EVENT_PLAYER_LEAVE, actor, NULL);
    }
}

void ai_actor_event_say(struct char_data *actor, const char *msg)
{
  struct char_data *mob;
  if (!actor || IS_NPC(actor) || IN_ROOM(actor) == NOWHERE) return;
  for (mob = world[IN_ROOM(actor)].people; mob; mob = mob->next_in_room)
    if (IS_NPC(mob) && MOB_FLAGGED(mob, MOB_AI_ACTOR) && mob != actor) {
      if (!mob->ai_state || !mob->ai_state->brain) ai_actor_init(mob);
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
