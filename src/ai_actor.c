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
#define AI_EVENT_IGNORE_MSG_SECS 4
#define AI_INTENT_THRESHOLD 25
#define AI_INTENT_COOLDOWN_MIN 6
#define AI_INTENT_COOLDOWN_MAX 12
#define AI_TALK_COOLDOWN_MIN 12
#define AI_TALK_COOLDOWN_MAX 20

static int ai_debug = AI_ACTOR_DEBUG;

static struct char_data *ai_find_player_by_idnum_room(struct char_data *mob, long idnum);
static const char *ai_pick_phrase(const char *const *pool);

static size_t ai_appendf(char *buf, size_t bufsz, size_t off, const char *fmt, ...)
{
  int wrote;
  va_list args;

  if (!buf || bufsz == 0)
    return 0;
  if (off >= bufsz)
    return bufsz - 1;

  va_start(args, fmt);
  wrote = vsnprintf(buf + off, bufsz - off, fmt, args);
  va_end(args);

  if (wrote < 0)
    return off;
  if ((size_t)wrote >= (bufsz - off))
    return bufsz - 1;
  return off + (size_t)wrote;
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

  if (MOB_FLAGGED(mob, MOB_AGGRESSIVE)) {
    score[ROLE_BANDIT] += 2;
    score[ROLE_BEAST] += 2;
  }
  if (MOB_FLAGGED(mob, MOB_SENTINEL))
    score[ROLE_GUARD] += 3;
  if (MOB_FLAGGED(mob, MOB_HELPER))
    score[ROLE_GUARD] += 2;
  if (MOB_FLAGGED(mob, MOB_SCAVENGER)) {
    score[ROLE_BANDIT] += 1;
    score[ROLE_BEAST] += 1;
  }
  if (ai_mob_has_shop_data(mob))
    score[ROLE_MERCHANT] += 8;

  if (IN_ROOM(mob) != NOWHERE) {
    zone_lvl = (zone_table[world[IN_ROOM(mob)].zone].min_level + zone_table[world[IN_ROOM(mob)].zone].max_level) / 2;
    if (zone_lvl >= 80)
      score[ROLE_UNDEAD] += 1;
    if (zone_lvl >= 110)
      score[ROLE_BOSS] += 2;
  }

  for (i = ROLE_UNKNOWN; i <= ROLE_BOSS; i++) {
    if (score[i] > best_score) {
      best_score = score[i];
      best_role = i;
    }
  }

  if (best_score <= 0)
    best_role = ROLE_UNKNOWN;

  mob->ai_prof->role = best_role;
  if (best_role == ROLE_UNKNOWN) {
    mob->ai_prof->movement = MOB_FLAGGED(mob, MOB_SENTINEL) ? MOVE_SENTINEL : MOVE_PATROL;
    mob->ai_prof->aggression = MOB_FLAGGED(mob, MOB_AGGRESSIVE) ? AGG_OPPORTUNISTIC : AGG_RETALIATE;
    mob->ai_prof->social = SOC_SILENT;
    mob->ai_prof->morale = MORALE_NORMAL;
    mob->ai_prof->style = 0;
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
    size_t off = 0;
    off = ai_appendf(mob->ai_prof->matched_keywords, sizeof(mob->ai_prof->matched_keywords), off,
                     "g%d", score[ROLE_GUARD]);
    off = ai_appendf(mob->ai_prof->matched_keywords, sizeof(mob->ai_prof->matched_keywords), off,
                     " m%d", score[ROLE_MERCHANT]);
    off = ai_appendf(mob->ai_prof->matched_keywords, sizeof(mob->ai_prof->matched_keywords), off,
                     " b%d", score[ROLE_BANDIT]);
    off = ai_appendf(mob->ai_prof->matched_keywords, sizeof(mob->ai_prof->matched_keywords), off,
                     " be%d", score[ROLE_BEAST]);
    off = ai_appendf(mob->ai_prof->matched_keywords, sizeof(mob->ai_prof->matched_keywords), off,
                     " u%d", score[ROLE_UNDEAD]);
    off = ai_appendf(mob->ai_prof->matched_keywords, sizeof(mob->ai_prof->matched_keywords), off,
                     " s%d", score[ROLE_SPIRIT]);
    off = ai_appendf(mob->ai_prof->matched_keywords, sizeof(mob->ai_prof->matched_keywords), off,
                     " c%d", score[ROLE_CULTIST]);
    ai_appendf(mob->ai_prof->matched_keywords, sizeof(mob->ai_prof->matched_keywords), off,
               " boss%d", score[ROLE_BOSS]);
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
static const char *const role_guard_greet[] = {"Keep the peace.", "Eyes open, no trouble.", NULL};
static const char *const role_merchant_greet[] = {"Fresh wares, fair rates.", "Browse first, buy smart.", NULL};
static const char *const role_innkeeper_greet[] = {"Warm beds and warm stew.", "Rest your boots by the fire.", NULL};
static const char *const role_bandit_greet[] = {"Keep your coin close.", "Road tax might find you.", NULL};
static const char *const role_unknown_idle[] = {"$n watches quietly.", "$n studies the room in silence.", NULL};

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

  score_idle = (pf->role == ROLE_UNKNOWN) ? 26 : 6;
  score_greet = (st->pending_event_type == AI_EVENT_PLAYER_ENTER) ? 26 : 0;
  score_social = (st->pending_event_type == AI_EVENT_PLAYER_EMOTE) ? 24 : 0;
  score_say = (st->pending_event_type == AI_EVENT_PLAYER_SAY) ? 22 : 0;
  score_warn = (st->social_spam_count >= 3) ? 26 : 0;
  score_flee = (pf->morale == MORALE_COWARD || pf->role == ROLE_MERCHANT || pf->role == ROLE_UNKNOWN) ? 8 : 0;

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
    else *out_line = "...";
    return 3;
  }
  if (score_greet >= AI_INTENT_THRESHOLD && ai_can_speak_now(mob, time(0))) {
    *out_target = target;
    if (pf->role == ROLE_GUARD) *out_line = ai_pick_phrase(role_guard_greet);
    else if (pf->role == ROLE_MERCHANT && pf->style == 1) *out_line = ai_pick_phrase(role_innkeeper_greet);
    else if (pf->role == ROLE_MERCHANT) *out_line = ai_pick_phrase(role_merchant_greet);
    else if (pf->role == ROLE_BANDIT) *out_line = ai_pick_phrase(role_bandit_greet);
    else *out_line = "Well met.";
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
  st->next_tick = now + rand_number(2, 4);

  if (st->talk_cooldown_pulses > 0) st->talk_cooldown_pulses = MAX(0, st->talk_cooldown_pulses - (int)(2 * PASSES_PER_SEC));
  if (st->intent_cooldown_pulses > 0) st->intent_cooldown_pulses = MAX(0, st->intent_cooldown_pulses - (int)(2 * PASSES_PER_SEC));
  if (st->social_spam_count > 0 && !rand_number(0, 2)) st->social_spam_count--;

  ai_mem_decay(mob, now);

  if (FIGHTING(mob)) {
    if (ai_try_flee_or_surrender(mob, now))
      return TRUE;
    return FALSE;
  }
  if (IN_ROOM(mob) == NOWHERE)
    return FALSE;

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

static const char *ai_pick_phrase(const char *const *pool)
{
  int n = 0;
  while (pool[n]) n++;
  if (!n) return NULL;
  return pool[rand_number(0, n - 1)];
}

void ai_actor_on_room_event(struct char_data *mob, enum ai_event_type type, struct char_data *actor, const char *text)
{
  struct ai_actor_memory_entry *e;
  time_t now = time(0);

  if (!mob || !actor || !mob->ai_prof || !mob->ai_state || IS_NPC(actor))
    return;

  e = ai_mem_get_or_create(mob, GET_IDNUM(actor));
  if (e) {
    e->last_seen_time = now;
    e->last_interaction_time = now;
    e->last_room_vnum = (IN_ROOM(mob) == NOWHERE) ? NOWHERE : GET_ROOM_VNUM(IN_ROOM(mob));
    if (actor->player.name)
      strlcpy(e->key_name, actor->player.name, sizeof(e->key_name));
    if (type == AI_EVENT_PLAYER_SAY) {
      e->trust = MIN(40, e->trust + 1);
      e->attitude = MAX(-100, MIN(100, e->attitude + 1));
    } else if (type == AI_EVENT_PLAYER_EMOTE) {
      if (text && (strstr(text, "spit") || strstr(text, "insult"))) {
        e->disposition_flags |= AI_DISP_ANNOYED_ME;
        e->hostility = MIN(40, e->hostility + 2);
        e->attitude = MAX(-100, e->attitude - 6);
      } else {
        e->attitude = MAX(-100, MIN(100, e->attitude + 2));
      }
    }
  }

  if (type == AI_EVENT_COMBAT_START && e) {
    e->disposition_flags |= AI_DISP_ATTACKED_ME;
    e->hostility = MIN(60, e->hostility + 4);
    e->attitude = MAX(-100, e->attitude - 12);
  }

  ai_state_push_event(mob, type, actor, text ? text : "");
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
