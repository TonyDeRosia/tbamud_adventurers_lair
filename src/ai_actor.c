#include "conf.h"
#include "sysdep.h"

#include <ctype.h>

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

struct ai_conv_actor_state {
  struct char_data *mob;
  int current_topic;
  long partner_id;
  long last_speaker_id;
  time_t last_line_time;
  time_t topic_expires_at;
  int depth_counter;
  time_t updated_at;
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
};

#define AI_CONV_ACTOR_STATE_MAX 512
#define AI_CONV_ROOM_STATE_MAX 256
static struct ai_conv_actor_state ai_conv_actor_states[AI_CONV_ACTOR_STATE_MAX];
static struct ai_conv_room_state ai_conv_room_states[AI_CONV_ROOM_STATE_MAX];

static int ai_debug = AI_ACTOR_DEBUG;

static struct char_data *ai_find_player_by_idnum_room(struct char_data *mob, long idnum);
static const char *ai_pick_phrase(const char *const *pool);
static const char *ai_pool_pick(const char *const *pool);
static int ai_role_can_give_directions(int role);
static int ai_role_can_answer_intent(int role, int style, int intent);
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
  if (!mob || !IS_NPC(mob) || !MOB_FLAGGED(mob, MOB_AI_ACTOR))
    return;

  ai_actor_refresh_profile(mob, TRUE);
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
static const char *const role_guard_greet[] = {"Good day. Keep things lawful.", "Welcome. Keep the peace.", "Morning. Move smart and stay calm.", "Eyes open. No trouble today.", "You are safe if you act right.", "Keep your blade sheathed in town.", "Report crimes and keep walking.", "Stay civil and we get along.", "The square is watched. Behave.", "Mind the law and you'll do fine.", "Need directions? Ask plainly.", "Order first, comfort second.", NULL};
static const char *const role_guard_service[] = {"Need a direction? I can point you to the inn, bank, or market.", "For rooms and rest, head to the inn. For trade, market stalls.", "The law office keeps records; the bank is east from here.", "Travelers rest at the inn. Keep your coin close on the road.", "Need help finding a healer? I can point the way.", "If you're lost, follow the main road to the square fountain.", "Merchants trade nearby; keep business clean and legal.", "Ask clearly and I'll give directions, not discounts.", NULL};
static const char *const role_merchant_greet[] = {"Welcome, traveler. Browse my wares.", "Fresh stock and fair measures today.", "Take your time; prices are honest.", "Looking to buy or sell?", "Careful hands, quality goods.", "Coin talks, and I listen.", "Best rates in this quarter.", "See anything you fancy?", "Trade straight, leave happy.", "Step closer and have a look.", "Fine goods, no tricks.", "I can help you outfit your journey.", NULL};
static const char *const role_merchant_service[] = {"I buy and sell. Show me what you carry.", "Need wares? I've got supplies and tools.", "Trade window's open; let's do business.", "If you need kit for the road, I can sort you out.", "Sell loot, buy provisions, move fast.", "My stock rotates often; check the shelves.", "I can price your goods fairly.", "If you seek rest, the inn is across the square.", NULL};
static const char *const role_innkeeper_greet[] = {"Welcome in. Warm beds and hot stew.", "Evening, friend. Rest and room available.", "Boots off, worries down, hearth's warm.", "Need a quiet room tonight?", "The fire's hot and the ale's fresh.", "Travel's hard; rest here.", "I've got blankets, broth, and a bed.", "Come in out of the weather.", "A calm table and a softer mattress await.", "Sit, breathe, and settle in.", "You're welcome so long as you keep it civil.", "Long road behind you? I've got rest for that.", NULL};
static const char *const role_innkeeper_service[] = {"I can offer a room, a meal, and a place to rest.", "Need an inn room? I can set you up.", "Rest your wounds by the hearth and take a bed upstairs.", "Food's hot, beds are clean, and noise stays low.", "You can rent a room or just sit and recover.", "If you need healing rest, this is your best stop.", "Stay the night and start fresh at dawn.", "No shop haggling here; comfort's what I sell.", NULL};
static const char *const role_bandit_greet[] = {"You're new. Keep your coin visible.", "Road's rough. Pay attention.", "Nice purse. Shame if it wandered.", "Walk light and don't stare.", "You look like trouble worth weighing.", "Eyes down, pockets up.", "I've seen richer folk go missing.", "Careful where you step, friend.", "This lane charges tolls in silver.", "Keep moving and maybe we smile.", "You breathe easy for someone in my street.", "Hope you can afford local manners.", NULL};
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

static const char *ai_line_for_intent(struct char_data *mob, struct ai_actor_memory_entry *e, int intent, int attitude, const char *text, const char **out_pool, const char **out_reason)
{
  static char line[224];
  int innkeeper;
  int role;
  int style;
  int topic = TARGET_NONE;
  const char *dir_line = NULL;
  const char *redir;

  static const char *const gib_guard[] = {"Slow down and say that clearly.", "I did not catch that. Ask again plain.", NULL};
  static const char *const gib_merch[] = {"I cannot parse that. Ask for wares plainly.", "Try that again with clear words.", NULL};
  static const char *const gib_inn[] = {"Easy now. Ask for room or meal plain.", "I missed that. Say it slow.", NULL};
  static const char *const gib_civ[] = {"I do not understand. Maybe ask a guard.", "Could you say that another way?", NULL};

  static const char *const weather_guard[] = {"Skies look steady, but keep a cloak handy.", "Weather turns fast near the roads; stay prepared.", NULL};
  static const char *const weather_inn[] = {"If rain comes, the hearth stays warm here.", "Bad weather fills my rooms early.", NULL};
  static const char *const weather_merch[] = {"Weather shifts prices almost as much as caravans.", "Dry roads mean better stock by dusk.", NULL};

  if (!mob || !mob->ai_prof)
    return NULL;
  if (out_pool) *out_pool = "POOL_NONE";
  if (out_reason) *out_reason = "AMBIENT";

  role = mob->ai_prof->role;
  style = mob->ai_prof->style;
  innkeeper = (role == ROLE_MERCHANT && style == 1);

  if (!ai_role_can_answer_intent(role, style, intent)) {
    redir = ai_role_redirect_line(role, style);
    return redir;
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

  if (intent == AI_INTENT_SMALLTALK && (ai_text_has_sub_ci(text, "weather") || ai_text_has_sub_ci(text, "rain") || ai_text_has_sub_ci(text, "sun") || ai_text_has_sub_ci(text, "storm") || ai_text_has_sub_ci(text, "nice day"))) {
    if (role == ROLE_GUARD)
      return ai_pick_phrase(weather_guard);
    if (role == ROLE_MERCHANT && innkeeper)
      return ai_pick_phrase(weather_inn);
    if (role == ROLE_MERCHANT)
      return ai_pick_phrase(weather_merch);
    return "Weather's been shifting all day.";
  }

  if (role == ROLE_GUARD) {
    if (intent == AI_INTENT_GREET || intent == AI_INTENT_SMALLTALK) return ai_pick_weighted_phrase(role_guard_greet, role_rare_guard);
    if (intent == AI_INTENT_GIBBERISH) return ai_pick_phrase(gib_guard);
    if (intent == AI_INTENT_DIRECTIONS || intent == AI_INTENT_HEAL || intent == AI_INTENT_BANK || intent == AI_INTENT_INN || intent == AI_INTENT_QUEST)
      return dir_line ? dir_line : ai_pick_phrase(role_guard_service);
  } else if (role == ROLE_MERCHANT) {
    if (innkeeper) {
      if (intent == AI_INTENT_GREET || intent == AI_INTENT_SMALLTALK) return ai_pick_weighted_phrase(role_innkeeper_greet, role_rare_innkeeper);
      if (intent == AI_INTENT_GIBBERISH) return ai_pick_phrase(gib_inn);
      if (intent == AI_INTENT_INN || intent == AI_INTENT_BUY_FOOD || intent == AI_INTENT_DIRECTIONS || intent == AI_INTENT_RUMOR)
        return dir_line ? dir_line : ai_pick_phrase(role_innkeeper_service);
    } else {
      if (intent == AI_INTENT_GREET || intent == AI_INTENT_SMALLTALK) return ai_pick_weighted_phrase(role_merchant_greet, role_rare_merchant);
      if (intent == AI_INTENT_GIBBERISH) return ai_pick_phrase(gib_merch);
      if (intent == AI_INTENT_BUY_WEAPON || intent == AI_INTENT_BUY_ARMOR || intent == AI_INTENT_BUY_FOOD || intent == AI_INTENT_DIRECTIONS)
        return dir_line ? dir_line : ai_pick_phrase(role_merchant_service);
    }
  } else if (role == ROLE_BOSS) {
    if (intent == AI_INTENT_GREET || intent == AI_INTENT_SMALLTALK) return "Stay sharp. This post does not sleep.";
    if (intent == AI_INTENT_GIBBERISH) return "Collect yourself and speak plainly.";
    if (intent == AI_INTENT_DIRECTIONS || intent == AI_INTENT_QUEST) return dir_line ? dir_line : "Find a guard post and ask plainly.";
  } else if (role == ROLE_CIVILIAN || role == ROLE_UNKNOWN) {
    if (intent == AI_INTENT_GREET || intent == AI_INTENT_SMALLTALK) return "Hello.";
    if (intent == AI_INTENT_GIBBERISH) return ai_pick_phrase(gib_civ);
    if (intent == AI_INTENT_DIRECTIONS || intent == AI_INTENT_RUMOR) return dir_line ? dir_line : "I'm not sure. Maybe ask a guard.";
  } else if (role == ROLE_BANDIT) {
    if (intent == AI_INTENT_GREET || intent == AI_INTENT_SMALLTALK) return ai_pick_weighted_phrase(role_bandit_greet, role_rare_bandit);
  } else if (role == ROLE_BEAST || role == ROLE_UNDEAD || role == ROLE_SPIRIT || role == ROLE_CULTIST) {
    if (intent == AI_INTENT_GIBBERISH)
      return NULL;
  }

  if (intent == AI_INTENT_INSULT || intent == AI_INTENT_EMOTE_SPIT || intent == AI_INTENT_THREAT)
    return (attitude < -20) ? "Last warning. Respect the law or leave." : "Mind your tongue and keep the peace.";
  if (intent >= AI_INTENT_EMOTE_DANCE) {
    if (role == ROLE_GUARD) return ai_pick_phrase(role_guard_emote);
    if (role == ROLE_MERCHANT && innkeeper) return ai_pick_phrase(role_innkeeper_emote);
    if (role == ROLE_MERCHANT) return ai_pick_phrase(role_merchant_emote);
    if (role == ROLE_BANDIT) return ai_pick_phrase(role_bandit_emote);
    if (role == ROLE_BEAST) return ai_pick_phrase(role_beast_emote);
    if (role == ROLE_UNDEAD) return ai_pick_phrase(role_undead_emote);
    if (role == ROLE_SPIRIT) return ai_pick_phrase(role_spirit_emote);
    if (role == ROLE_CULTIST) return (intent == AI_INTENT_EMOTE_SPIT) ? "Blasphemy has a price." : "Ritual, not revelry.";
  }

  if (intent == AI_INTENT_ASK_SERVICE || intent == AI_INTENT_CONFUSION)
    return ai_role_redirect_line(role, style);

  (void)line;
  return NULL;
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

static int ai_detect_intent(enum ai_event_type type, const char *text)
{
  if (type == AI_EVENT_PLAYER_EMOTE) {
    if (ai_text_has_sub_ci(text, "dance")) return AI_INTENT_EMOTE_DANCE;
    if (ai_text_has_sub_ci(text, "spit")) return AI_INTENT_EMOTE_SPIT;
    if (ai_text_has_sub_ci(text, "hug")) return AI_INTENT_EMOTE_HUG;
    if (ai_text_has_sub_ci(text, "wave")) return AI_INTENT_EMOTE_WAVE;
    return AI_INTENT_NONE;
  }

  if (type != AI_EVENT_PLAYER_SAY)
    return AI_INTENT_NONE;

  if (ai_is_gibberish(text))
    return AI_INTENT_GIBBERISH;

  if (ai_text_has_sub_ci(text, "weather") || ai_text_has_sub_ci(text, "rain") || ai_text_has_sub_ci(text, "sun") || ai_text_has_sub_ci(text, "storm") || ai_text_has_sub_ci(text, "nice day"))
    return AI_INTENT_SMALLTALK;
  if (ai_text_has_sub_ci(text, "how are you") || ai_text_has_sub_ci(text, "what's up"))
    return AI_INTENT_SMALLTALK;
  if (ai_text_has_sub_ci(text, "hello") || ai_text_has_sub_ci(text, "hi") || ai_text_has_sub_ci(text, "hey") ||
      ai_text_has_sub_ci(text, "greetings") || ai_text_has_sub_ci(text, "yo"))
    return AI_INTENT_GREET;
  if (ai_text_has_sub_ci(text, "weapon") || ai_text_has_sub_ci(text, "sword") || ai_text_has_sub_ci(text, "dagger") ||
      ai_text_has_sub_ci(text, "axe") || ai_text_has_sub_ci(text, "bow") || ai_text_has_sub_ci(text, "mace") ||
      ai_text_has_sub_ci(text, "staff") || ai_text_has_sub_ci(text, "armory"))
    return AI_INTENT_BUY_WEAPON;
  if (ai_text_has_sub_ci(text, "armor") || ai_text_has_sub_ci(text, "shield") || ai_text_has_sub_ci(text, "helm") ||
      ai_text_has_sub_ci(text, "mail") || ai_text_has_sub_ci(text, "plate"))
    return AI_INTENT_BUY_ARMOR;
  if (ai_text_has_sub_ci(text, "food") || ai_text_has_sub_ci(text, "eat") || ai_text_has_sub_ci(text, "hungry") ||
      ai_text_has_sub_ci(text, "drink") || ai_text_has_sub_ci(text, "tavern") || ai_text_has_sub_ci(text, "bakery"))
    return AI_INTENT_BUY_FOOD;
  if (ai_text_has_sub_ci(text, "heal") || ai_text_has_sub_ci(text, "healer") || ai_text_has_sub_ci(text, "cleric") ||
      ai_text_has_sub_ci(text, "temple") || ai_text_has_sub_ci(text, "shrine") || ai_text_has_sub_ci(text, "cure"))
    return AI_INTENT_HEAL;
  if (ai_text_has_sub_ci(text, "bank") || ai_text_has_sub_ci(text, "deposit") || ai_text_has_sub_ci(text, "withdraw") ||
      ai_text_has_sub_ci(text, "vault") || ai_text_has_sub_ci(text, "exchange"))
    return AI_INTENT_BANK;
  if (ai_text_has_sub_ci(text, "inn") || ai_text_has_sub_ci(text, "room") || ai_text_has_sub_ci(text, "rest") ||
      ai_text_has_sub_ci(text, "sleep") || ai_text_has_sub_ci(text, "rent"))
    return AI_INTENT_INN;
  if (ai_text_has_sub_ci(text, "train") || ai_text_has_sub_ci(text, "practice") || ai_text_has_sub_ci(text, "guild") || ai_text_has_sub_ci(text, "trainer"))
    return AI_INTENT_TRAIN;
  if (ai_text_has_sub_ci(text, "rumor") || ai_text_has_sub_ci(text, "gossip") || ai_text_has_sub_ci(text, "news") || ai_text_has_sub_ci(text, "heard"))
    return AI_INTENT_RUMOR;
  if (ai_text_has_sub_ci(text, "quest") || ai_text_has_sub_ci(text, "job") || ai_text_has_sub_ci(text, "task") ||
      ai_text_has_sub_ci(text, "mission") || ai_text_has_sub_ci(text, "help me"))
    return AI_INTENT_QUEST;
  if (ai_text_has_sub_ci(text, "where") || ai_text_has_sub_ci(text, "how") || ai_text_has_sub_ci(text, "which way") ||
      ai_text_has_sub_ci(text, "how do i get") || ai_text_has_sub_ci(text, "how to get") || ai_text_has_sub_ci(text, "get there") ||
      ai_text_has_sub_ci(text, "directions") || ai_text_has_sub_ci(text, "find") || ai_text_has_sub_ci(text, "locate"))
    return AI_INTENT_DIRECTIONS;
  if (ai_text_has_sub_ci(text, "buy") || ai_text_has_sub_ci(text, "sell") || ai_text_has_sub_ci(text, "wares") || ai_text_has_sub_ci(text, "shop"))
    return AI_INTENT_ASK_SERVICE;
  if (ai_text_has_sub_ci(text, "die") || ai_text_has_sub_ci(text, "kill") || ai_text_has_sub_ci(text, "attack") ||
      ai_text_has_sub_ci(text, "fight") || ai_text_has_sub_ci(text, "threat") || ai_text_has_sub_ci(text, "mug") || ai_text_has_sub_ci(text, "rob"))
    return AI_INTENT_THREAT;
  if (ai_text_has_sub_ci(text, "idiot") || ai_text_has_sub_ci(text, "stupid") || ai_text_has_sub_ci(text, "trash") ||
      ai_text_has_sub_ci(text, "ugly") || ai_text_has_sub_ci(text, "hate") || ai_text_has_sub_ci(text, "shut up"))
    return AI_INTENT_INSULT;
  if (ai_text_has_sub_ci(text, "thanks") || ai_text_has_sub_ci(text, "good") || ai_text_has_sub_ci(text, "nice") ||
      ai_text_has_sub_ci(text, "great") || ai_text_has_sub_ci(text, "appreciate"))
    return AI_INTENT_PRAISE;
  if (ai_text_has_sub_ci(text, "what") || ai_text_has_sub_ci(text, "help") || ai_text_has_sub_ci(text, "lost"))
    return AI_INTENT_CONFUSION;

  return AI_INTENT_NONE;
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

static int ai_actor_room_response_slot(struct char_data *mob, struct char_data *actor, enum ai_event_type type, int intent)
{
  struct char_data *it;
  struct char_data *top1 = NULL, *top2 = NULL;
  int best1 = -9999, best2 = -9999;

  if (!mob || !actor || IN_ROOM(mob) == NOWHERE || IN_ROOM(actor) != IN_ROOM(mob))
    return FALSE;

  for (it = world[IN_ROOM(mob)].people; it; it = it->next_in_room) {
    int pri;

    if (!IS_NPC(it) || !MOB_FLAGGED(it, MOB_AI_ACTOR) || !it->ai_prof || !it->ai_state)
      continue;
    if ((time(0) - it->ai_state->last_talk_time) < AI_PER_PLAYER_REPLY_COOLDOWN_SECS)
      continue;

    pri = ai_event_fit_bonus(it, type, intent);
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

  return (mob == top1 || mob == top2);
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

void ai_actor_on_room_event(struct char_data *mob, enum ai_event_type type, struct char_data *actor, const char *text)
{
  struct ai_actor_memory_entry *e;
  struct ai_conv_room_state *room_st;
  time_t now = time(0);
  int intent;
  const char *line = NULL;
  char normalized[256];

  if (!mob || !actor || !mob->ai_prof || !mob->ai_state || IS_NPC(actor))
    return;

  ai_state_refresh_local_topics(mob);
  ai_normalize_text(text ? text : "", normalized, sizeof(normalized));
  e = ai_mem_get_or_create(mob, GET_IDNUM(actor));
  intent = ai_detect_intent(type, normalized);
  room_st = ai_conv_room_state_get(IN_ROOM(mob), 1);
  if (room_st && type == AI_EVENT_PLAYER_SAY)
    room_st->last_player_speech_time = now;

  if (type == AI_EVENT_PLAYER_SAY) {
    struct ai_conv_actor_state *conv_st = ai_conv_actor_state_get(mob, 1);
    if (conv_st) {
      conv_st->current_topic = ai_conv_topic_from_intent(intent);
      conv_st->last_speaker_id = GET_IDNUM(actor);
      conv_st->last_line_time = now;
      conv_st->updated_at = now;
    }
  }

  if (e) {
    e->last_seen_time = now;
    e->last_interaction_time = now;
    e->last_room_vnum = (IN_ROOM(mob) == NOWHERE) ? NOWHERE : GET_ROOM_VNUM(IN_ROOM(mob));
    e->last_intent = intent;
    if (actor->player.name)
      strlcpy(e->key_name, actor->player.name, sizeof(e->key_name));

    /* Intent scoring table:
     * GREET +2
     * PRAISE +5
     * INSULT -12 (DISRESPECT)
     * EMOTE_SPIT -20 (DISRESPECT)
     * THREAT -25 (THREATENED)
     */
    if (intent == AI_INTENT_GREET)
      e->attitude += 2;
    else if (intent == AI_INTENT_PRAISE) {
      e->attitude += 5;
      e->disposition_flags |= AI_DISP_FRIENDLY;
    } else if (intent == AI_INTENT_INSULT) {
      e->attitude -= 12;
      e->disposition_flags |= (AI_DISP_DISRESPECT | AI_DISP_ANNOYED_ME);
    } else if (intent == AI_INTENT_EMOTE_SPIT) {
      e->attitude -= 20;
      e->disposition_flags |= (AI_DISP_DISRESPECT | AI_DISP_ANNOYED_ME);
    } else if (intent == AI_INTENT_THREAT) {
      e->attitude -= 25;
      e->disposition_flags |= (AI_DISP_THREATENED | AI_DISP_ATTACKED_ME);
    }

    e->attitude = MAX(-100, MIN(100, e->attitude));
    e->last_update = now;
  }

  if (type == AI_EVENT_COMBAT_START && e) {
    e->disposition_flags |= AI_DISP_ATTACKED_ME;
    e->hostility = MIN(60, e->hostility + 4);
    e->attitude = MAX(-100, e->attitude - 12);
  }

  ai_state_push_event(mob, type, actor, normalized);

  if (!e || !intent)
    return;
  if (!ai_actor_room_response_slot(mob, actor, type, intent))
    return;
  if ((now - e->last_reply_time) < AI_PER_PLAYER_REPLY_COOLDOWN_SECS)
    return;

  {
    const char *pool = "POOL_NONE";
    const char *reason = ai_event_reason_name(type);
    char targeted[256];

    line = ai_line_for_intent(mob, e, intent, e->attitude, normalized, &pool, &reason);
    if (!line || !*line)
      return;

    ai_set_last_speech_meta(mob, pool, reason);
    snprintf(targeted, sizeof(targeted), "$n says to %s, '%s'", GET_NAME(actor), line);
    ai_actor_schedule_reaction_speech(mob, actor, targeted);
    e->last_reply_time = now;
  }
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
