/* ***********************************************************************
*    File:   quest.c                                  Part of CircleMUD  *
* Version:   2.1 (December 2005) Written for CircleMud CWG / Suntzu      *
* Purpose:   To provide special quest-related code.                      *
* Copyright: Kenneth Ray                                                 *
* Original Version Details:                                              *
* Morgaelin - quest.c                                                    *
* Copyright (C) 1997 MS                                                  *
*********************************************************************** */

#include "conf.h"
#include "sysdep.h"

#include "structs.h"
#include "utils.h"
#include "interpreter.h"
#include "handler.h"
#include "db.h"
#include "comm.h"
#include "screen.h"
#include "dg_scripts.h"
#include "quest.h"
#include "quest_rewards.h"
#include "act.h" /* for do_tell */
#include "class.h"
#include "shop.h"
#include "spec_procs.h"
#include "mail.h"


/*--------------------------------------------------------------------------
 * Exported global variables
 *--------------------------------------------------------------------------*/
const char *quest_types[] = {
  "Object",
  "Room",
  "Find mob",
  "Kill mob",
  "Save mob",
  "Return object",
  "Clear room",
  "\n"
};
const char *aq_flags[] = {
  "REPEATABLE",
  "\n"
};


/*--------------------------------------------------------------------------
 * Local (file scope) global variables
 *--------------------------------------------------------------------------*/
static int cmd_tell;

static const char *quest_cmd[] = {
  "request", "list", "buy", "progress", "drop", "info", "complete", "status", "\n"};

static const char *quest_mort_usage =
  "Quest Commands:\r\n"
  "  quest request   - Request a new Guild contract.\r\n"
  "  quest list      - View the Quest Point reward catalog.\r\n"
  "  quest buy <#>   - Purchase a reward from the catalog.\r\n"
  "  quest progress  - Check your current contract status.\r\n"
  "  quest drop      - Abandon your current contract.\r\n"
  "  quest info      - Review your current contract details.\r\n"
  "  quest complete  - Turn in a completed contract.";

static const char *quest_imm_usage =
  "Quest Commands:\r\n"
  "  quest request   - Request a new Guild contract.\r\n"
  "  quest list      - View the Quest Point reward catalog.\r\n"
  "  quest buy <#>   - Purchase a reward from the catalog.\r\n"
  "  quest progress  - Check your current contract status.\r\n"
  "  quest drop      - Abandon your current contract.\r\n"
  "  quest info      - Review your current contract details.\r\n"
  "  quest complete  - Turn in a completed contract.\r\n"
  "  quest status <vnum> - Immortal legacy quest inspection.";

#define KQUEST_DURATION_SECS (60 * 60)
#define KQUEST_COOLDOWN_SECS (5 * 60)
#define CAMPAIGN_DURATION_SECS (6 * 24 * 60 * 60)
#define CAMPAIGN_MIN_TARGETS 8
#define CAMPAIGN_SMALL_TARGETS 9
#define CAMPAIGN_STANDARD_TARGETS 11

#define CAMPAIGN_XP_MULTIPLIER_MIN_PCT 150
#define CAMPAIGN_XP_MULTIPLIER_MAX_PCT 300
#define CAMPAIGN_QP_MULTIPLIER_MIN_PCT 240
#define CAMPAIGN_QP_MULTIPLIER_MAX_PCT 360
#define CAMPAIGN_GOLD_MULTIPLIER_MIN_PCT 300
#define CAMPAIGN_GOLD_MULTIPLIER_MAX_PCT 450

static const char *campaign_cmd[] = {
  "request", "info", "quit", "check", "brief", "today", "\n"
};

struct campaign_rewards {
  int xp;
  int qp;
  int gold;
  int trains;
  int practices;
};

static void clear_kill_quest(struct char_data *ch)
{
  GET_KQUEST_ACTIVE(ch) = 0;
  GET_KQUEST_COMPLETE(ch) = 0;
  GET_KQUEST_TARGET(ch) = NOBODY;
  GET_KQUEST_ROOM(ch) = NOWHERE;
  GET_KQUEST_GIVER(ch) = NOBODY;
  GET_KQUEST_TIME(ch) = 0;
  GET_KQUEST_EXPIRES_AT(ch) = 0;
  GET_KQUEST_TARGET_ID(ch) = 0;
}

static int seconds_to_minutes_ceiling(time_t seconds_remaining)
{
  if (seconds_remaining <= 0)
    return 0;

  return (int)((seconds_remaining + 59) / 60);
}

bool is_on_quest(struct char_data *ch)
{
  return ch && !IS_NPC(ch) && GET_KQUEST_ACTIVE(ch) != 0;
}

bool is_quest_ready(struct char_data *ch)
{
  return is_on_quest(ch) && GET_KQUEST_COMPLETE(ch) != 0;
}

bool is_quest_expired(struct char_data *ch)
{
  time_t now;

  if (!is_on_quest(ch))
    return FALSE;

  now = time(0);
  return GET_KQUEST_EXPIRES_AT(ch) > 0 && now >= GET_KQUEST_EXPIRES_AT(ch);
}

bool is_on_quest_cooldown(struct char_data *ch)
{
  time_t now;

  if (!ch || IS_NPC(ch))
    return FALSE;

  now = time(0);
  return GET_KQUEST_COOLDOWN_EXPIRES_AT(ch) > now;
}

int get_quest_minutes_remaining(struct char_data *ch)
{
  time_t now;

  if (!is_on_quest(ch))
    return 0;

  now = time(0);
  return seconds_to_minutes_ceiling(GET_KQUEST_EXPIRES_AT(ch) - now);
}

int get_quest_cooldown_minutes_remaining(struct char_data *ch)
{
  time_t now;

  if (!ch || IS_NPC(ch))
    return 0;

  now = time(0);
  return seconds_to_minutes_ceiling(GET_KQUEST_COOLDOWN_EXPIRES_AT(ch) - now);
}

static void start_kill_quest_cooldown(struct char_data *ch)
{
  GET_KQUEST_COOLDOWN_EXPIRES_AT(ch) = time(0) + KQUEST_COOLDOWN_SECS;
  GET_KQUEST_COOLDOWN_NOTIFIED(ch) = 0;
}

static void notify_quest_cooldown_ready_if_needed(struct char_data *ch)
{
  time_t now;

  if (!ch || IS_NPC(ch))
    return;

  if (GET_KQUEST_COOLDOWN_EXPIRES_AT(ch) <= 0 || GET_KQUEST_COOLDOWN_NOTIFIED(ch))
    return;

  now = time(0);
  if (now < GET_KQUEST_COOLDOWN_EXPIRES_AT(ch))
    return;

  send_to_char(ch, "%sQUEST: You may now quest again.%s\r\n", QRED, QNRM);
  GET_KQUEST_COOLDOWN_NOTIFIED(ch) = 1;
  save_char(ch);
}

static void expire_kill_quest_if_needed(struct char_data *ch, bool notify)
{
  if (!is_on_quest(ch) || !is_quest_expired(ch))
    return;

  clear_kill_quest(ch);
  start_kill_quest_cooldown(ch);
  if (notify)
    send_to_char(ch, "Your quest has expired. You have failed to complete it in time.\r\n");
  save_char(ch);
}

bool is_questmaster_mob(struct char_data *mob)
{
  if (!mob || !IS_NPC(mob))
    return FALSE;

  /* Builder flag is primary; legacy special remains compatibility fallback. */
  return MOB_FLAGGED(mob, MOB_QUEST_MASTER) ||
         GET_MOB_SPEC(mob) == questmaster;
}

static struct char_data *find_present_questmaster(struct char_data *ch)
{
  struct char_data *mob;

  for (mob = world[IN_ROOM(ch)].people; mob; mob = mob->next_in_room) {
    if (is_questmaster_mob(mob) && CAN_SEE(ch, mob))
      return mob;
  }

  return NULL;
}

static int is_service_or_protected_mob(struct char_data *mob)
{
  SPECIAL(*spec);

  if (!mob || !IS_NPC(mob))
    return TRUE;

  if (MOB_FLAGGED(mob, MOB_NOKILL) ||
      MOB_FLAGGED(mob, MOB_GUILD_MASTER) ||
      MOB_FLAGGED(mob, MOB_QUEST_MASTER))
    return TRUE;

  spec = GET_MOB_SPEC(mob);
  if (spec == questmaster || spec == shop_keeper || spec == guild || spec == postmaster || spec == receptionist ) {
    return TRUE;
  }

  return FALSE;
}

static int room_is_quest_safe(room_rnum room)
{
  if (!VALID_ROOM_RNUM(room))
    return TRUE;

  return ROOM_FLAGGED(room, ROOM_PEACEFUL) ||
         ROOM_FLAGGED(room, ROOM_GODROOM) ||
         ROOM_FLAGGED(room, ROOM_DEATH) ||
         ROOM_FLAGGED(room, ROOM_NOMOB);
}

/*
 * Build a map of rooms a normal quest route can reach from start.
 *
 * Normal closed-but-unlocked doors are considered traversable because a player
 * can open them. Locked exits are excluded because the quest system cannot
 * assume the player owns, can obtain, or can pick the required key/lock.
 *
 * Peaceful and NOMOB rooms may be crossed as ordinary travel space, but
 * GODROOM and DEATH rooms are never used as quest-route transit.
 */
static int quest_route_room_blocked(room_rnum room)
{
  if (!VALID_ROOM_RNUM(room))
    return TRUE;

  return ROOM_FLAGGED(room, ROOM_GODROOM) ||
         ROOM_FLAGGED(room, ROOM_DEATH);
}

static int quest_route_edge_traversable(room_rnum room, int dir)
{
  struct room_direction_data *exit;

  if (!VALID_ROOM_RNUM(room) || dir < 0 || dir >= DIR_COUNT)
    return FALSE;

  exit = world[room].dir_option[dir];
  if (!exit || exit->to_room == NOWHERE || !VALID_ROOM_RNUM(exit->to_room))
    return FALSE;

  if (EXIT_FLAGGED(exit, EX_LOCKED))
    return FALSE;

  if (quest_route_room_blocked(exit->to_room))
    return FALSE;

  return TRUE;
}

static int quest_build_reachable_map(room_rnum start, byte *reachable)
{
  room_rnum *queue;
  room_rnum room;
  int head = 0, tail = 0, dir;
  size_t count;

  if (!reachable || !VALID_ROOM_RNUM(start) || quest_route_room_blocked(start))
    return 0;

  count = (size_t)top_of_world + 1;
  memset(reachable, 0, count * sizeof(*reachable));
  CREATE(queue, room_rnum, count);

  reachable[start] = TRUE;
  queue[tail++] = start;

  while (head < tail) {
    room = queue[head++];

    for (dir = 0; dir < DIR_COUNT; dir++) {
      room_rnum to_room;

      if (!quest_route_edge_traversable(room, dir))
        continue;

      to_room = world[room].dir_option[dir]->to_room;
      if (reachable[to_room])
        continue;

      reachable[to_room] = TRUE;
      queue[tail++] = to_room;
    }
  }

  free(queue);
  return tail;
}

/*
 * Build a reverse reachability map for a destination room.
 *
 * A marked room can reach destination by ordinary quest-route edges. Combined
 * with quest_build_reachable_map(), this lets quest selection require a true
 * round trip rather than merely a one-way route to the target.
 */
static int quest_build_returnable_map(room_rnum destination, byte *returnable)
{
  room_rnum *queue;
  room_rnum *incoming_from;
  int *incoming_head;
  int *incoming_next;
  room_rnum room;
  size_t count, max_edges;
  int edge_count = 0;
  int head = 0, tail = 0;
  int dir, i;

  if (!returnable || !VALID_ROOM_RNUM(destination) ||
      quest_route_room_blocked(destination))
    return 0;

  count = (size_t)top_of_world + 1;
  max_edges = count * DIR_COUNT;

  memset(returnable, 0, count * sizeof(*returnable));
  CREATE(queue, room_rnum, count);
  CREATE(incoming_head, int, count);
  CREATE(incoming_next, int, max_edges);
  CREATE(incoming_from, room_rnum, max_edges);

  for (i = 0; i <= top_of_world; i++)
    incoming_head[i] = -1;

  for (room = 0; room <= top_of_world; room++) {
    if (quest_route_room_blocked(room))
      continue;

    for (dir = 0; dir < DIR_COUNT; dir++) {
      room_rnum to_room;

      if (!quest_route_edge_traversable(room, dir))
        continue;

      to_room = world[room].dir_option[dir]->to_room;
      incoming_from[edge_count] = room;
      incoming_next[edge_count] = incoming_head[to_room];
      incoming_head[to_room] = edge_count;
      edge_count++;
    }
  }

  returnable[destination] = TRUE;
  queue[tail++] = destination;

  while (head < tail) {
    room = queue[head++];

    for (i = incoming_head[room]; i != -1; i = incoming_next[i]) {
      room_rnum from_room = incoming_from[i];

      if (returnable[from_room])
        continue;

      returnable[from_room] = TRUE;
      queue[tail++] = from_room;
    }
  }

  free(incoming_from);
  free(incoming_next);
  free(incoming_head);
  free(queue);
  return tail;
}

static int campaign_size_step(int target_count)
{
  int divisor = MAX(1, MAX_CAMPAIGN_TARGETS - CAMPAIGN_MIN_TARGETS);
  int numerator = URANGE(CAMPAIGN_MIN_TARGETS, target_count, MAX_CAMPAIGN_TARGETS) - CAMPAIGN_MIN_TARGETS;
  return (numerator * 100) / divisor;
}

static int campaign_scaled_percent(int target_count, int min_pct, int max_pct)
{
  int step = campaign_size_step(target_count);
  return min_pct + ((max_pct - min_pct) * step) / 100;
}

static int campaign_level_xp_span(struct char_data *ch)
{
  int level;
  int at_level;
  int next_level;

  if (!ch || IS_NPC(ch))
    return 0;

  level = URANGE(1, GET_CAMPAIGN_LEVEL(ch), LVL_IMMORT - 1);
  if (level >= LVL_IMMORT - 1)
    return 0;

  at_level = level_exp(GET_CLASS(ch), level);
  next_level = level_exp(GET_CLASS(ch), level + 1);
  return MAX(1, next_level - at_level);
}

static void calculate_campaign_rewards(struct char_data *ch, int target_count, struct campaign_rewards *out)
{
  int quest_qp_base;
  int quest_gold_base;
  int xp_span;
  int xp_pct;
  int qp_pct;
  int gold_pct;
  int level_bonus;

  if (!out)
    return;

  out->xp = 0;
  out->qp = 0;
  out->gold = 0;
  out->trains = 0;
  out->practices = 0;

  if (!ch || IS_NPC(ch))
    return;

  quest_qp_base = MAX(5, GET_LEVEL(ch) / 2);
  quest_gold_base = MAX(100, GET_LEVEL(ch) * 75);
  xp_span = campaign_level_xp_span(ch);

  xp_pct = campaign_scaled_percent(target_count, CAMPAIGN_XP_MULTIPLIER_MIN_PCT, CAMPAIGN_XP_MULTIPLIER_MAX_PCT);
  qp_pct = campaign_scaled_percent(target_count, CAMPAIGN_QP_MULTIPLIER_MIN_PCT, CAMPAIGN_QP_MULTIPLIER_MAX_PCT);
  gold_pct = campaign_scaled_percent(target_count, CAMPAIGN_GOLD_MULTIPLIER_MIN_PCT, CAMPAIGN_GOLD_MULTIPLIER_MAX_PCT);

  out->xp = (int)MIN((long long)INT_MAX, ((long long)xp_span * (long long)xp_pct) / 100LL);
  out->qp = (int)MIN((long long)INT_MAX, ((long long)quest_qp_base * (long long)qp_pct) / 100LL);
  out->gold = (int)MIN((long long)INT_MAX, ((long long)quest_gold_base * (long long)gold_pct) / 100LL);

  level_bonus = MIN(8, MAX(0, GET_LEVEL(ch) / 15));

  if (target_count <= CAMPAIGN_SMALL_TARGETS) {
    out->trains = 1;
    out->practices = 7;
  } else if (target_count <= CAMPAIGN_STANDARD_TARGETS) {
    out->trains = 2;
    out->practices = 12;
  } else {
    out->trains = 3;
    out->practices = 18;
  }

  out->trains = MIN(5, out->trains + (GET_LEVEL(ch) >= 45 ? 1 : 0));
  out->practices = MIN(30, out->practices + level_bonus);
}

static void clear_campaign(struct char_data *ch)
{
  int i;
  GET_CAMPAIGN_ACTIVE(ch) = 0;
  GET_CAMPAIGN_LEVEL(ch) = 0;
  GET_CAMPAIGN_EXPIRES_AT(ch) = 0;
  GET_CAMPAIGN_REWARD_QP(ch) = 0;
  GET_CAMPAIGN_REWARD_GOLD(ch) = 0;
  GET_CAMPAIGN_REWARD_XP(ch) = 0;
  GET_CAMPAIGN_REWARD_TRAINS(ch) = 0;
  GET_CAMPAIGN_REWARD_PRACTICES(ch) = 0;
  GET_CAMPAIGN_TARGET_COUNT(ch) = 0;
  for (i = 0; i < MAX_CAMPAIGN_TARGETS; i++) {
    GET_CAMPAIGN_TARGET_VNUM(ch, i) = NOBODY;
    GET_CAMPAIGN_TARGET_ROOM(ch, i) = NOWHERE;
    GET_CAMPAIGN_TARGET_REQUIRED(ch, i) = 0;
    GET_CAMPAIGN_TARGET_REMAINING(ch, i) = 0;
  }
}

static bool is_on_campaign(struct char_data *ch)
{
  return ch && !IS_NPC(ch) && GET_CAMPAIGN_ACTIVE(ch) != 0;
}

static bool is_campaign_expired(struct char_data *ch)
{
  time_t now;

  if (!is_on_campaign(ch))
    return FALSE;

  now = time(0);
  return GET_CAMPAIGN_EXPIRES_AT(ch) > 0 && now >= GET_CAMPAIGN_EXPIRES_AT(ch);
}

static int campaign_remaining_targets(struct char_data *ch)
{
  int i, remaining = 0;
  for (i = 0; i < GET_CAMPAIGN_TARGET_COUNT(ch); i++)
    if (GET_CAMPAIGN_TARGET_REMAINING(ch, i) > 0)
      remaining += GET_CAMPAIGN_TARGET_REMAINING(ch, i);
  return remaining;
}

static int campaign_seconds_remaining(struct char_data *ch)
{
  time_t now;
  if (!is_on_campaign(ch))
    return 0;
  now = time(0);
  if (GET_CAMPAIGN_EXPIRES_AT(ch) <= now)
    return 0;
  return (int)(GET_CAMPAIGN_EXPIRES_AT(ch) - now);
}

static void format_campaign_time_left(int total_seconds, char *buf, size_t bufsz)
{
  int days, hours, minutes;
  if (total_seconds < 0)
    total_seconds = 0;
  days = total_seconds / (24 * 60 * 60);
  total_seconds %= (24 * 60 * 60);
  hours = total_seconds / (60 * 60);
  total_seconds %= (60 * 60);
  minutes = total_seconds / 60;
  snprintf(buf, bufsz, "%d day%s, %d hour%s and %d minute%s",
           days, days == 1 ? "" : "s",
           hours, hours == 1 ? "" : "s",
           minutes, minutes == 1 ? "" : "s");
}

static void format_campaign_deadline(time_t when, char *buf, size_t bufsz)
{
  struct tm *tm_ptr = localtime(&when);
  if (!tm_ptr) {
    strlcpy(buf, "Unknown", bufsz);
    return;
  }
  strftime(buf, bufsz, "%I:%M%p on %d %b %Y", tm_ptr);
}

static const char *campaign_target_name(mob_vnum vnum)
{
  mob_rnum rnum = real_mobile(vnum);
  if (rnum == NOBODY)
    return "an unknown target";
  return GET_NAME(&mob_proto[rnum]);
}

static const char *campaign_target_area(room_vnum rvnum)
{
  room_rnum rr = real_room(rvnum);
  if (rr == NOWHERE)
    return "Unknown Area";
  return zone_table[world[rr].zone].name;
}

static void expire_campaign_if_needed(struct char_data *ch, bool notify)
{
  if (!is_on_campaign(ch) || !is_campaign_expired(ch))
    return;

  clear_campaign(ch);
  if (notify)
    send_to_char(ch, "%sYour campaign has expired. The contract has been revoked.%s\r\n", QRED, QNRM);
  save_char(ch);
}

struct campaign_candidate_data {
  mob_vnum vnum;
  room_vnum room_vnum;
  int zone;
};

static int select_campaign_targets(struct char_data *ch, struct campaign_candidate_data selected[MAX_CAMPAIGN_TARGETS], int *selected_count)
{
  struct char_data *mob;
  struct campaign_candidate_data pool[2000];
  byte *reachable, *returnable;
  int pool_count = 0, i, j, need, max_targets;
  int level_window_low = GET_LEVEL(ch) - 5;
  int level_window_high = GET_LEVEL(ch) + 2;

  if (!ch || IN_ROOM(ch) == NOWHERE)
    return 0;

  CREATE(reachable, byte, (size_t)top_of_world + 1);
  CREATE(returnable, byte, (size_t)top_of_world + 1);
  quest_build_reachable_map(IN_ROOM(ch), reachable);
  quest_build_returnable_map(IN_ROOM(ch), returnable);

  for (mob = character_list; mob; mob = mob->next) {
    room_rnum in_room;
    int mob_level;

    if (!IS_NPC(mob) || IN_ROOM(mob) == NOWHERE)
      continue;
    if (is_service_or_protected_mob(mob))
      continue;
    if (MOB_FLAGGED(mob, MOB_NOTDEADYET))
      continue;

    in_room = IN_ROOM(mob);
    if (room_is_quest_safe(in_room))
      continue;
    if (!reachable[in_room] || !returnable[in_room])
      continue;

    mob_level = GET_LEVEL(mob);
    if (mob_level < level_window_low || mob_level > level_window_high)
      continue;

    if (pool_count >= (int)(sizeof(pool) / sizeof(pool[0])))
      break;

    pool[pool_count].vnum = GET_MOB_VNUM(mob);
    pool[pool_count].room_vnum = GET_ROOM_VNUM(in_room);
    pool[pool_count].zone = world[in_room].zone;
    pool_count++;
  }

  if (pool_count < CAMPAIGN_MIN_TARGETS) {
    free(returnable);
    free(reachable);
    return 0;
  }

  free(returnable);
  free(reachable);
  max_targets = MIN(MAX_CAMPAIGN_TARGETS, pool_count);
  need = rand_number(CAMPAIGN_MIN_TARGETS, max_targets);
  *selected_count = 0;

  for (i = 0; i < pool_count; i++) {
    j = rand_number(i, pool_count - 1);
    if (i != j) {
      struct campaign_candidate_data tmp = pool[i];
      pool[i] = pool[j];
      pool[j] = tmp;
    }
  }

  for (i = 0; i < pool_count && *selected_count < need; i++) {
    mob_vnum vnum = pool[i].vnum;
    int zone = pool[i].zone;
    int seen_vnum = FALSE, seen_zone = FALSE;

    for (j = 0; j < *selected_count; j++) {
      if (selected[j].vnum == vnum)
        seen_vnum = TRUE;
      if (selected[j].zone == zone)
        seen_zone = TRUE;
    }
    if (seen_vnum || seen_zone)
      continue;

    selected[*selected_count] = pool[i];
    (*selected_count)++;
  }

  for (i = 0; i < pool_count && *selected_count < need; i++) {
    mob_vnum vnum = pool[i].vnum;
    int seen_vnum = FALSE;
    for (j = 0; j < *selected_count; j++)
      if (selected[j].vnum == vnum)
        seen_vnum = TRUE;
    if (seen_vnum)
      continue;
    selected[*selected_count] = pool[i];
    (*selected_count)++;
  }

  return *selected_count >= CAMPAIGN_MIN_TARGETS;
}

static struct char_data *select_kill_quest_target(struct char_data *ch)
{
  struct char_data *mob, *choice = NULL;
  byte *reachable, *returnable;
  int weight, total = 0;
  int level_diff;

  if (!ch || IN_ROOM(ch) == NOWHERE)
    return NULL;

  CREATE(reachable, byte, (size_t)top_of_world + 1);
  CREATE(returnable, byte, (size_t)top_of_world + 1);
  quest_build_reachable_map(IN_ROOM(ch), reachable);
  quest_build_returnable_map(IN_ROOM(ch), returnable);

  for (mob = character_list; mob; mob = mob->next) {
    if (!IS_NPC(mob) || IN_ROOM(mob) == NOWHERE)
      continue;
    if (room_is_quest_safe(IN_ROOM(mob)))
      continue;
    if (!reachable[IN_ROOM(mob)] || !returnable[IN_ROOM(mob)])
      continue;
    if (is_service_or_protected_mob(mob))
      continue;
    if (MOB_FLAGGED(mob, MOB_NOTDEADYET))
      continue;

    level_diff = GET_LEVEL(ch) - GET_LEVEL(mob);
    if (level_diff < -3 || level_diff > 8)
      continue;

    if (level_diff >= 0 && level_diff <= 4)
      weight = 6;
    else if (level_diff <= 6)
      weight = 3;
    else
      weight = 1;

    total += weight;
    if (rand_number(1, total) <= weight)
      choice = mob;
  }

  free(returnable);
  free(reachable);
  return choice;
}

static void quest_request_kill(struct char_data *ch)
{
  struct char_data *qm, *target;
  room_rnum tr;

  qm = find_present_questmaster(ch);
  if (!qm) {
    send_to_char(ch, "You must be in the same room as a quest master to request a quest.\r\n");
    return;
  }
  expire_kill_quest_if_needed(ch, TRUE);
  if (GET_QUEST(ch) != NOTHING || GET_KQUEST_ACTIVE(ch)) {
    send_to_char(ch, "%s tells you, 'You already have a quest to finish first.'\r\n", GET_NAME(qm));
    return;
  }
  if (is_on_quest_cooldown(ch)) {
    send_to_char(ch, "You must wait %d minute%s before requesting another quest.\r\n",
                 get_quest_cooldown_minutes_remaining(ch),
                 get_quest_cooldown_minutes_remaining(ch) == 1 ? "" : "s");
    return;
  }

  target = select_kill_quest_target(ch);
  if (!target) {
    send_to_char(ch, "%s tells you, 'I have no suitable hunt for you right now. Return shortly.'\r\n", GET_NAME(qm));
    return;
  }

  tr = IN_ROOM(target);
  GET_KQUEST_ACTIVE(ch) = 1;
  GET_KQUEST_COMPLETE(ch) = 0;
  GET_KQUEST_TARGET(ch) = GET_MOB_VNUM(target);
  GET_KQUEST_ROOM(ch) = GET_ROOM_VNUM(tr);
  GET_KQUEST_GIVER(ch) = GET_MOB_VNUM(qm);
  GET_KQUEST_TIME(ch) = 60;
  GET_KQUEST_EXPIRES_AT(ch) = time(0) + KQUEST_DURATION_SECS;
  GET_KQUEST_TARGET_ID(ch) = 0; /* Dynamic kill quests are VNUM-based, not instance-based. */
  GET_KQUEST_COOLDOWN_NOTIFIED(ch) = 1;

  send_to_char(ch, "You ask %s for a quest.\r\n", GET_NAME(qm));
  send_to_char(ch, "%s tells you, 'Thank you, brave %s!'\r\n", GET_NAME(qm), GET_NAME(ch));
  send_to_char(ch, "%s tells you, 'An enemy of the Guild, %s, has been causing trouble on the roads!'\r\n",
      GET_NAME(qm), GET_NAME(target));
  send_to_char(ch, "%s tells you, 'Seek %s out somewhere near %s in the area of %s.'\r\n",
      GET_NAME(qm), GET_NAME(target), world[tr].name, zone_table[world[tr].zone].name);
  send_to_char(ch, "%s tells you, 'Good luck, %s. Return safely!'\r\n", GET_NAME(qm), GET_NAME(ch));
  send_to_char(ch, "You have 60 minutes to complete your quest.\r\n");
  save_char(ch);
}

static const char *qvalidate_base_reason(struct char_data *mob, byte *reachable, byte *returnable)
{
  room_rnum room;

  if (!mob || !IS_NPC(mob))
    return "not an NPC";
  if (IN_ROOM(mob) == NOWHERE)
    return "not in a room";
  if (is_service_or_protected_mob(mob))
    return "service/protected mob";
  if (MOB_FLAGGED(mob, MOB_NOTDEADYET))
    return "NOTDEADYET";
  room = IN_ROOM(mob);
  if (room_is_quest_safe(room))
    return "quest-safe target room";
  if (!reachable[room])
    return "no route from here";
  if (!returnable[room])
    return "no return route";
  return NULL;
}

static int qvalidate_kill_level_ok(struct char_data *ch, struct char_data *mob)
{
  int level_diff = GET_LEVEL(ch) - GET_LEVEL(mob);
  return level_diff >= -3 && level_diff <= 8;
}

static int qvalidate_campaign_level_ok(struct char_data *ch, struct char_data *mob)
{
  int mob_level = GET_LEVEL(mob);
  return mob_level >= GET_LEVEL(ch) - 5 && mob_level <= GET_LEVEL(ch) + 2;
}

static void qvalidate_print_mob(struct char_data *ch, struct char_data *mob,
                                byte *reachable, byte *returnable)
{
  const char *reason = qvalidate_base_reason(mob, reachable, returnable);
  room_rnum room = IN_ROOM(mob);
  int kill_ok = FALSE, campaign_ok = FALSE;

  if (!reason) {
    kill_ok = qvalidate_kill_level_ok(ch, mob);
    campaign_ok = qvalidate_campaign_level_ok(ch, mob);
  }

  send_to_char(ch, "Mob %d: %s\r\n", GET_MOB_VNUM(mob), GET_NAME(mob));
  send_to_char(ch, "  Level: %d\r\n", GET_LEVEL(mob));

  if (VALID_ROOM_RNUM(room)) {
    send_to_char(ch, "  Room: %d (%s)\r\n",
                 GET_ROOM_VNUM(room), world[room].name);
    send_to_char(ch, "  Area: %d (%s)\r\n",
                 zone_table[world[room].zone].number,
                 zone_table[world[room].zone].name);
    send_to_char(ch, "  Route from current room: %s\r\n",
                 reachable[room] ? "YES" : "NO");
    send_to_char(ch, "  Return route: %s\r\n",
                 returnable[room] ? "YES" : "NO");
  }

  if (reason) {
    send_to_char(ch, "  Dynamic quest eligible: NO (%s)\r\n", reason);
    send_to_char(ch, "  Campaign eligible: NO (%s)\r\n", reason);
  } else {
    send_to_char(ch, "  Dynamic quest eligible: %s%s\r\n",
                 kill_ok ? "YES" : "NO",
                 kill_ok ? "" : " (outside kill-quest level range)");
    send_to_char(ch, "  Campaign eligible: %s%s\r\n",
                 campaign_ok ? "YES" : "NO",
                 campaign_ok ? "" : " (outside campaign level range)");
  }
}

ACMD(do_qvalidate)
{
  char arg1[MAX_INPUT_LENGTH], arg2[MAX_INPUT_LENGTH];
  struct char_data *mob;
  byte *reachable, *returnable;
  int found = 0;

  if (IS_NPC(ch) || IN_ROOM(ch) == NOWHERE)
    return;

  two_arguments(argument, arg1, arg2);

  if (!*arg1) {
    send_to_char(ch,
      "Usage:\r\n"
      "  qvalidate mob <vnum>\r\n"
      "  qvalidate area <zone>\r\n"
      "\r\nValidation is performed from your current room using the same round-trip\r\n"
      "route rules used by dynamic quests and campaigns.\r\n");
    return;
  }

  CREATE(reachable, byte, (size_t)top_of_world + 1);
  CREATE(returnable, byte, (size_t)top_of_world + 1);
  quest_build_reachable_map(IN_ROOM(ch), reachable);
  quest_build_returnable_map(IN_ROOM(ch), returnable);

  if (is_abbrev(arg1, "mob")) {
    mob_vnum vnum;

    if (!*arg2 || (vnum = atoi(arg2)) <= 0) {
      send_to_char(ch, "Usage: qvalidate mob <vnum>\r\n");
      free(returnable);
      free(reachable);
      return;
    }

    send_to_char(ch, "Quest validation from room %d for mob %d:\r\n",
                 GET_ROOM_VNUM(IN_ROOM(ch)), vnum);

    for (mob = character_list; mob; mob = mob->next) {
      if (!IS_NPC(mob) || GET_MOB_VNUM(mob) != vnum)
        continue;
      found++;
      qvalidate_print_mob(ch, mob, reachable, returnable);
    }

    if (!found)
      send_to_char(ch,
        "No live instance of mob %d is currently loaded. The current dynamic\r\n"
        "quest selectors also cannot choose an unloaded mob instance.\r\n", vnum);
  } else if (is_abbrev(arg1, "area") || is_abbrev(arg1, "zone")) {
    int zone_vnum, total = 0, route_ok = 0, kill_ok = 0, campaign_ok = 0;
    int shown = 0;

    if (!*arg2 || (zone_vnum = atoi(arg2)) < 0) {
      send_to_char(ch, "Usage: qvalidate area <zone>\r\n");
      free(returnable);
      free(reachable);
      return;
    }

    send_to_char(ch, "Quest validation from room %d for area %d:\r\n",
                 GET_ROOM_VNUM(IN_ROOM(ch)), zone_vnum);

    for (mob = character_list; mob; mob = mob->next) {
      const char *reason;
      room_rnum room;

      if (!IS_NPC(mob) || IN_ROOM(mob) == NOWHERE)
        continue;

      room = IN_ROOM(mob);
      if (zone_table[world[room].zone].number != zone_vnum)
        continue;

      total++;
      reason = qvalidate_base_reason(mob, reachable, returnable);
      if (!reason) {
        route_ok++;
        if (qvalidate_kill_level_ok(ch, mob))
          kill_ok++;
        if (qvalidate_campaign_level_ok(ch, mob))
          campaign_ok++;
      }

      if (shown < 40) {
        send_to_char(ch, "  [%5d] L%-3d %-28s  Q:%s C:%s%s%s\r\n",
                     GET_MOB_VNUM(mob), GET_LEVEL(mob), GET_NAME(mob),
                     (!reason && qvalidate_kill_level_ok(ch, mob)) ? "YES" : "NO ",
                     (!reason && qvalidate_campaign_level_ok(ch, mob)) ? "YES" : "NO ",
                     reason ? "  (" : "",
                     reason ? reason : "");
        if (reason)
          send_to_char(ch, ")\r\n");
        shown++;
      }
    }

    if (!total) {
      send_to_char(ch, "No live NPC instances are currently loaded in area %d.\r\n",
                   zone_vnum);
    } else {
      if (total > shown)
        send_to_char(ch, "... %d additional loaded NPC instance%s omitted.\r\n",
                     total - shown, (total - shown) == 1 ? "" : "s");
      send_to_char(ch,
        "\r\nSummary: loaded=%d  route-valid=%d  dynamic-quest=%d  campaign=%d\r\n",
        total, route_ok, kill_ok, campaign_ok);
    }
  } else {
    send_to_char(ch,
      "Usage: qvalidate mob <vnum> | qvalidate area <zone>\r\n");
  }

  free(returnable);
  free(reachable);
}

static void quest_drop_kill(struct char_data *ch)
{
  expire_kill_quest_if_needed(ch, TRUE);

  if (!is_on_quest(ch)) {
    send_to_char(ch, "You do not currently have a Guild contract to drop.\r\n");
    return;
  }

  clear_kill_quest(ch);
  start_kill_quest_cooldown(ch);
  send_to_char(ch,
      "You abandon your current Guild contract. You may request another in %d minute%s.\r\n",
      get_quest_cooldown_minutes_remaining(ch),
      get_quest_cooldown_minutes_remaining(ch) == 1 ? "" : "s");
  save_char(ch);
}

static void quest_info_kill(struct char_data *ch)
{
  room_rnum room;
  const char *target_name;

  expire_kill_quest_if_needed(ch, TRUE);
  if (!is_on_quest(ch)) {
    send_to_char(ch, "You are not currently on a kill quest.\r\n");
    return;
  }
  if (is_quest_ready(ch)) {
    send_to_char(ch, "You have slain your target. Return to a quest master and type 'quest complete'.\r\n");
    send_to_char(ch, "Time remaining: %d minute%s.\r\n",
                 get_quest_minutes_remaining(ch),
                 get_quest_minutes_remaining(ch) == 1 ? "" : "s");
    return;
  }

  room = real_room(GET_KQUEST_ROOM(ch));
  if (room == NOWHERE) {
    send_to_char(ch, "Your assigned target can no longer be located. Please request a new quest.\r\n");
    return;
  }

  target_name = (real_mobile(GET_KQUEST_TARGET(ch)) != NOBODY) ?
    GET_NAME(&mob_proto[real_mobile(GET_KQUEST_TARGET(ch))]) : "your target";
  send_to_char(ch, "You are on a quest to slay %s!\r\n", target_name);
  send_to_char(ch, "%s can be found in the vicinity of %s which\r\nis in the general area of %s.\r\n",
      target_name,
      world[room].name, zone_table[world[room].zone].name);
  send_to_char(ch, "Time remaining: %d minute%s.\r\n",
               get_quest_minutes_remaining(ch),
               get_quest_minutes_remaining(ch) == 1 ? "" : "s");
}

static void quest_list_rewards(struct char_data *ch)
{
  struct char_data *qm = find_present_questmaster(ch);

  if (!qm) {
    send_to_char(ch,
        "You must be with a questmaster to view the Guild's Quest Point rewards.\r\n");
    return;
  }

  quest_rewards_list(ch);
}

static void quest_buy_reward(struct char_data *ch, char *argument)
{
  quest_rewards_buy(ch, argument);
}

static void quest_progress_kill(struct char_data *ch)
{
  expire_kill_quest_if_needed(ch, TRUE);

  if (!is_on_quest(ch)) {
    if (is_on_quest_cooldown(ch))
      send_to_char(ch,
          "No active contract. You may request another in %d minute%s.\r\n",
          get_quest_cooldown_minutes_remaining(ch),
          get_quest_cooldown_minutes_remaining(ch) == 1 ? "" : "s");
    else
      send_to_char(ch, "You do not currently have an active Guild contract.\r\n");
    return;
  }

  send_to_char(ch, "Guild Contract: %s\r\n",
               is_quest_ready(ch) ? "TARGET SLAIN - READY TO TURN IN" : "IN PROGRESS");
  send_to_char(ch, "Time remaining: %d minute%s.\r\n",
               get_quest_minutes_remaining(ch),
               get_quest_minutes_remaining(ch) == 1 ? "" : "s");

  if (is_quest_ready(ch))
    send_to_char(ch, "Return to any questmaster and use 'quest complete'.\r\n");
  else
    send_to_char(ch, "Use 'quest info' to review your target and location.\r\n");
}

static void quest_complete_kill(struct char_data *ch)
{
  struct char_data *qm;
  int qp_reward, gold_reward, exp_reward;

  expire_kill_quest_if_needed(ch, TRUE);
  if (!is_on_quest(ch)) {
    send_to_char(ch, "You have no active kill quest to complete.\r\n");
    return;
  }
  if (!is_quest_ready(ch)) {
    send_to_char(ch, "You have not yet slain your assigned target.\r\n");
    return;
  }

  qm = find_present_questmaster(ch);
  if (!qm) {
    send_to_char(ch, "You must return to a quest master to complete this quest.\r\n");
    return;
  }

  qp_reward = MAX(5, GET_LEVEL(ch) / 2);
  gold_reward = MAX(100, GET_LEVEL(ch) * 75);
  exp_reward = MAX(250, GET_LEVEL(ch) * 250);

  GET_QUESTPOINTS(ch) += qp_reward;
  increase_gold(ch, gold_reward);
  gain_exp(ch, exp_reward);
  send_to_char(ch, "You inform %s that you have completed your quest.\r\n", GET_NAME(qm));
  send_to_char(ch, "%s tells you, 'Congratulations, %s, on completing your quest!'\r\n",
               GET_NAME(qm), GET_NAME(ch));
  send_to_char(ch, "%s tells you, 'As a reward, I am giving you %d quest points and %d gold.'\r\n",
               GET_NAME(qm), qp_reward, gold_reward);
  send_to_char(ch, "%s tells you, 'Give campaigns a try %s, see %sHELP CAMPAIGNS%s.'\r\n",
               GET_NAME(qm), GET_NAME(ch), QYEL, QNRM);
  send_to_char(ch, "You receive %d experience.\r\n", exp_reward);

  clear_kill_quest(ch);
  start_kill_quest_cooldown(ch);
  save_char(ch);
}

static void campaign_show_header(struct char_data *ch)
{
  send_to_char(ch, "%s--------------------------[ YOUR CURRENT CAMPAIGN ]----------------------%s\r\n", QYEL, QNRM);
}

static void campaign_request(struct char_data *ch)
{
  struct campaign_candidate_data selected[MAX_CAMPAIGN_TARGETS];
  struct campaign_rewards rewards;
  int count, i;

  expire_campaign_if_needed(ch, TRUE);
  if (is_on_campaign(ch)) {
    send_to_char(ch, "%sYou already have an active campaign. Use 'campaign info'.%s\r\n", QRED, QNRM);
    return;
  }

  if (!select_campaign_targets(ch, selected, &count)) {
    send_to_char(ch, "%sNo suitable campaign targets are available for your level right now. Please try again later.%s\r\n", QRED, QNRM);
    return;
  }

  clear_campaign(ch);
  GET_CAMPAIGN_ACTIVE(ch) = 1;
  GET_CAMPAIGN_LEVEL(ch) = GET_LEVEL(ch);
  GET_CAMPAIGN_EXPIRES_AT(ch) = time(0) + CAMPAIGN_DURATION_SECS;
  GET_CAMPAIGN_TARGET_COUNT(ch) = count;
  calculate_campaign_rewards(ch, count, &rewards);
  GET_CAMPAIGN_REWARD_QP(ch) = rewards.qp;
  GET_CAMPAIGN_REWARD_GOLD(ch) = rewards.gold;
  GET_CAMPAIGN_REWARD_XP(ch) = rewards.xp;
  GET_CAMPAIGN_REWARD_TRAINS(ch) = rewards.trains;
  GET_CAMPAIGN_REWARD_PRACTICES(ch) = rewards.practices;

  for (i = 0; i < count; i++) {
    GET_CAMPAIGN_TARGET_VNUM(ch, i) = selected[i].vnum;
    GET_CAMPAIGN_TARGET_ROOM(ch, i) = selected[i].room_vnum;
    GET_CAMPAIGN_TARGET_REQUIRED(ch, i) = 1;
    GET_CAMPAIGN_TARGET_REMAINING(ch, i) = 1;
  }

  send_to_char(ch, "%sCampaign accepted.%s %sYou have %d targets and 6 real-world days to finish.%s\r\n",
               QGRN, QNRM, QCYN, count, QNRM);
  send_to_char(ch, "Use %s'campaign info'%s for full details, %s'cp check'%s for remaining targets.\r\n",
               QYEL, QNRM, QYEL, QNRM);
  save_char(ch);
}

static void campaign_info(struct char_data *ch)
{
  int i;
  char left_buf[128], deadline_buf[64];

  expire_campaign_if_needed(ch, TRUE);
  if (!is_on_campaign(ch)) {
    send_to_char(ch, "%sYou do not currently have an active campaign.%s\r\n", QRED, QNRM);
    return;
  }

  format_campaign_time_left(campaign_seconds_remaining(ch), left_buf, sizeof(left_buf));
  format_campaign_deadline(GET_CAMPAIGN_EXPIRES_AT(ch), deadline_buf, sizeof(deadline_buf));

  campaign_show_header(ch);
  send_to_char(ch, "%sComplete By........:%s [ %s%s%s ]\r\n", QCYN, QNRM, QWHT, deadline_buf, QNRM);
  send_to_char(ch, "%sTime Left..........:%s [ %s%s%s ]\r\n", QCYN, QNRM, QGRN, left_buf, QNRM);
  send_to_char(ch, "%sLevel Taken........:%s [ %5d ]\r\n", QCYN, QNRM, GET_CAMPAIGN_LEVEL(ch));
  send_to_char(ch, "%sExperience.........:%s [ %5d ]\r\n", QCYN, QNRM, GET_CAMPAIGN_REWARD_XP(ch));
  send_to_char(ch, "%sQuest Points.......:%s [ %5d ]\r\n", QCYN, QNRM, GET_CAMPAIGN_REWARD_QP(ch));
  send_to_char(ch, "%sGold Coins.........:%s [ %5d ]\r\n", QCYN, QNRM, GET_CAMPAIGN_REWARD_GOLD(ch));
  send_to_char(ch, "%sTrains.............:%s [ %5d ]\r\n", QCYN, QNRM, GET_CAMPAIGN_REWARD_TRAINS(ch));
  send_to_char(ch, "%sPractices..........:%s [ %5d ]\r\n", QCYN, QNRM, GET_CAMPAIGN_REWARD_PRACTICES(ch));
  send_to_char(ch, "%s----------------------------[ Campaign Victims ]-------------------------%s\r\n", QYEL, QNRM);
  send_to_char(ch, "The targets for this campaign are:\r\n");
  for (i = 0; i < GET_CAMPAIGN_TARGET_COUNT(ch); i++) {
    send_to_char(ch, "Find and kill %d * %s%s%s (%s%s%s)\r\n",
                 GET_CAMPAIGN_TARGET_REQUIRED(ch, i),
                 QGRN, campaign_target_name(GET_CAMPAIGN_TARGET_VNUM(ch, i)), QNRM,
                 QCYN, campaign_target_area(GET_CAMPAIGN_TARGET_ROOM(ch, i)), QNRM);
  }
  send_to_char(ch, "%s--------------------------------------------------------------------------%s\r\n", QYEL, QNRM);
  send_to_char(ch, "Use '%scp check%s' to see only targets that you still need to kill.\r\n", QYEL, QNRM);
}

static void campaign_check(struct char_data *ch)
{
  int i, shown = 0;
  char left_buf[128];

  expire_campaign_if_needed(ch, TRUE);
  if (!is_on_campaign(ch)) {
    send_to_char(ch, "%sYou do not currently have an active campaign.%s\r\n", QRED, QNRM);
    return;
  }

  for (i = 0; i < GET_CAMPAIGN_TARGET_COUNT(ch); i++) {
    if (GET_CAMPAIGN_TARGET_REMAINING(ch, i) <= 0)
      continue;
    shown++;
    send_to_char(ch, "You still have to kill * %s%s%s (%s%s%s)\r\n",
                 QGRN, campaign_target_name(GET_CAMPAIGN_TARGET_VNUM(ch, i)), QNRM,
                 QCYN, campaign_target_area(GET_CAMPAIGN_TARGET_ROOM(ch, i)), QNRM);
  }
  if (!shown)
    send_to_char(ch, "%sAll campaign targets are complete. Rewards will be granted automatically.%s\r\n", QGRN, QNRM);

  format_campaign_time_left(campaign_seconds_remaining(ch), left_buf, sizeof(left_buf));
  send_to_char(ch, "\r\nYou have %s%s%s left to finish this campaign.\r\n", QGRN, left_buf, QNRM);
  send_to_char(ch, "You may take a campaign at this level.\r\n");
}

static void campaign_brief(struct char_data *ch)
{
  int i, shown = 0;
  expire_campaign_if_needed(ch, TRUE);
  if (!is_on_campaign(ch)) {
    send_to_char(ch, "%sNo active campaign.%s\r\n", QRED, QNRM);
    return;
  }

  send_to_char(ch, "%sCampaign brief:%s %d remaining target%s.\r\n",
               QYEL, QNRM, campaign_remaining_targets(ch), campaign_remaining_targets(ch) == 1 ? "" : "s");
  for (i = 0; i < GET_CAMPAIGN_TARGET_COUNT(ch); i++) {
    if (GET_CAMPAIGN_TARGET_REMAINING(ch, i) <= 0)
      continue;
    shown++;
    send_to_char(ch, " - %s%s%s (%s%s%s)\r\n",
                 QGRN, campaign_target_name(GET_CAMPAIGN_TARGET_VNUM(ch, i)), QNRM,
                 QCYN, campaign_target_area(GET_CAMPAIGN_TARGET_ROOM(ch, i)), QNRM);
    if (shown >= 5)
      break;
  }
}

static void campaign_today(struct char_data *ch)
{
  char left_buf[128];
  expire_campaign_if_needed(ch, FALSE);
  if (!is_on_campaign(ch)) {
    send_to_char(ch, "%sCampaign Status:%s You may request a campaign now.\r\n", QYEL, QNRM);
    return;
  }
  format_campaign_time_left(campaign_seconds_remaining(ch), left_buf, sizeof(left_buf));
  send_to_char(ch, "%sCampaign Status:%s Active with %s%d%s target%s remaining. Time left: %s%s%s.\r\n",
               QYEL, QNRM, QGRN, campaign_remaining_targets(ch), QNRM,
               campaign_remaining_targets(ch) == 1 ? "" : "s", QGRN, left_buf, QNRM);
}

static void campaign_quit(struct char_data *ch)
{
  expire_campaign_if_needed(ch, FALSE);
  if (!is_on_campaign(ch)) {
    send_to_char(ch, "%sYou have no active campaign to quit.%s\r\n", QRED, QNRM);
    return;
  }
  clear_campaign(ch);
  send_to_char(ch, "%sYou abandon your campaign and forfeit all rewards.%s\r\n", QRED, QNRM);
  save_char(ch);
}

/*--------------------------------------------------------------------------*/
/* Utility Functions                                                        */
/*--------------------------------------------------------------------------*/

qst_rnum real_quest(qst_vnum vnum)
{
  int rnum;

  for (rnum = 0; rnum < total_quests; rnum++)
    if (QST_NUM(rnum) == vnum)
      return(rnum);
  return(NOTHING);
}

int is_complete(struct char_data *ch, qst_vnum vnum)
{
  int i;

  for (i = 0; i < GET_NUM_QUESTS(ch); i++)
    if (ch->player_specials->saved.completed_quests[i] == vnum)
      return TRUE;
  return FALSE;
}

qst_vnum find_quest_by_qmnum(struct char_data *ch, mob_vnum qm, int num)
{
  qst_rnum rnum;
  int found=0;
  for (rnum = 0; rnum < total_quests; rnum++) {
    if (qm == QST_MASTER(rnum))
      if (++found == num)
        return (QST_NUM(rnum));
  }
  return NOTHING;
}

/*--------------------------------------------------------------------------*/
/* Quest Loading and Unloading Functions                                    */
/*--------------------------------------------------------------------------*/

void destroy_quests(void)
{
  qst_rnum rnum = 0;

  if (!aquest_table)
    return;

  for (rnum = 0; rnum < total_quests; rnum++){
    free_quest_strings(&aquest_table[rnum]);
  }
  free(aquest_table);
  aquest_table = NULL;
  total_quests = 0;

  return;
}

int count_quests(qst_vnum low, qst_vnum high)
{
  int i, j;

  if (!aquest_table)
    return 0;

  for (i = j = 0; i < total_quests; i++)
    if (QST_NUM(i) >= low && QST_NUM(i) <= high)
      j++;

  return j;
}

void parse_quest(FILE *quest_f, int nr)
{
  static char line[256];
  static int i = 0, j;
  int retval = 0, t[7];
  char f1[128], buf2[MAX_STRING_LENGTH];
  aquest_table[i].vnum = nr;
  aquest_table[i].qm = NOBODY;
  aquest_table[i].name = NULL;
  aquest_table[i].desc = NULL;
  aquest_table[i].info = NULL;
  aquest_table[i].done = NULL;
  aquest_table[i].quit = NULL;
  aquest_table[i].flags = 0;
  aquest_table[i].type = -1;
  aquest_table[i].target = -1;
  aquest_table[i].prereq = NOTHING;
  for (j = 0; j < 7; j++)
    aquest_table[i].value[j] = 0;
  aquest_table[i].prev_quest = NOTHING;
  aquest_table[i].next_quest = NOTHING;
  aquest_table[i].func = NULL;

  aquest_table[i].gold_reward = 0;
  aquest_table[i].exp_reward  = 0;
  aquest_table[i].obj_reward  = NOTHING;

  /* begin to parse the data */
  aquest_table[i].name = fread_string(quest_f, buf2);
  aquest_table[i].desc = fread_string(quest_f, buf2);
  aquest_table[i].info = fread_string(quest_f, buf2);
  aquest_table[i].done = fread_string(quest_f, buf2);
  aquest_table[i].quit = fread_string(quest_f, buf2);
  if (!get_line(quest_f, line) ||
      (retval = sscanf(line, " %d %d %s %d %d %d %d",
             t, t+1, f1, t+2, t+3, t + 4, t + 5)) != 7) {
    log("Format error in numeric line (expected 7, got %d), %s\n",
        retval, line);
    exit(1);
  }
  aquest_table[i].type       = t[0];
  aquest_table[i].qm         = (real_mobile(t[1]) == NOBODY) ? NOBODY : t[1];
  aquest_table[i].flags      = asciiflag_conv(f1);
  aquest_table[i].target     = (t[2] == -1) ? NOTHING : t[2];
  aquest_table[i].prev_quest = (t[3] == -1) ? NOTHING : t[3];
  aquest_table[i].next_quest = (t[4] == -1) ? NOTHING : t[4];
  aquest_table[i].prereq     = (t[5] == -1) ? NOTHING : t[5];
  if (!get_line(quest_f, line) ||
      (retval = sscanf(line, " %d %d %d %d %d %d %d",
          t, t+1, t+2, t+3, t+4, t + 5, t + 6)) != 7) {
    log("Format error in numeric line (expected 7, got %d), %s\n",
        retval, line);
    exit(1);
  }
  for (j = 0; j < 7; j++)
    aquest_table[i].value[j] = t[j];

  if (!get_line(quest_f, line) ||
      (retval = sscanf(line, " %d %d %d",
             t, t+1, t+2)) != 3) {
    log("Format error in numeric (rewards) line (expected 3, got %d), %s\n",
        retval, line);
    exit(1);
  }

  aquest_table[i].gold_reward = t[0];
  aquest_table[i].exp_reward  = t[1];
  aquest_table[i].obj_reward  = (t[2] == -1) ? NOTHING : t[2];

  for (;;) {
    if (!get_line(quest_f, line)) {
      log("Format error in %s\n", line);
      exit(1);
    }
    switch(*line) {
    case 'S':
      total_quests = ++i;
      return;
    }
  }
} /* parse_quest */

void assign_the_quests(void)
{
  qst_rnum rnum;
  mob_rnum mrnum;

  cmd_tell = find_command("tell");

  for (rnum = 0; rnum < total_quests; rnum ++) {
    if (QST_MASTER(rnum) == NOBODY) {
      log("SYSERR: Quest #%d has no questmaster specified.", QST_NUM(rnum));
      continue;
    }
    if ((mrnum = real_mobile(QST_MASTER(rnum))) == NOBODY) {
      log("SYSERR: Quest #%d has an invalid questmaster.", QST_NUM(rnum));
      continue;
    }
    if (mob_index[(mrnum)].func &&
 mob_index[(mrnum)].func != questmaster)
      QST_FUNC(rnum) = mob_index[(mrnum)].func;
    mob_index[(mrnum)].func = questmaster;
  }
}

/*--------------------------------------------------------------------------*/
/* Quest Completion Functions                                               */
/*--------------------------------------------------------------------------*/
void set_quest(struct char_data *ch, qst_rnum rnum)
{
  GET_QUEST(ch) = QST_NUM(rnum);
  GET_QUEST_TIME(ch) = QST_TIME(rnum);
  GET_QUEST_COUNTER(ch) = QST_QUANTITY(rnum);
  SET_BIT_AR(PRF_FLAGS(ch), PRF_QUEST);
  return;
}

void clear_quest(struct char_data *ch)
{
  GET_QUEST(ch) = NOTHING;
  GET_QUEST_TIME(ch) = -1;
  GET_QUEST_COUNTER(ch) = 0;
  REMOVE_BIT_AR(PRF_FLAGS(ch), PRF_QUEST);
  return;
}

void add_completed_quest(struct char_data *ch, qst_vnum vnum)
{
  qst_vnum *temp;
  int i;

  CREATE(temp, qst_vnum, GET_NUM_QUESTS(ch) +1);
  for (i=0; i < GET_NUM_QUESTS(ch); i++)
    temp[i] = ch->player_specials->saved.completed_quests[i];

  temp[GET_NUM_QUESTS(ch)] = vnum;
  GET_NUM_QUESTS(ch)++;

  if (ch->player_specials->saved.completed_quests)
    free(ch->player_specials->saved.completed_quests);
  ch->player_specials->saved.completed_quests = temp;
}

void remove_completed_quest(struct char_data *ch, qst_vnum vnum)
{
  qst_vnum *temp;
  int i, j = 0;

  CREATE(temp, qst_vnum, GET_NUM_QUESTS(ch));
  for (i = 0; i < GET_NUM_QUESTS(ch); i++)
    if (ch->player_specials->saved.completed_quests[i] != vnum)
      temp[j++] = ch->player_specials->saved.completed_quests[i];

  GET_NUM_QUESTS(ch)--;

  if (ch->player_specials->saved.completed_quests)
    free(ch->player_specials->saved.completed_quests);
  ch->player_specials->saved.completed_quests = temp;
}

void generic_complete_quest(struct char_data *ch)
{
  qst_rnum rnum;
  qst_vnum vnum = GET_QUEST(ch);
  struct obj_data *new_obj;
  int happy_qp, happy_gold, happy_exp;

  if (--GET_QUEST_COUNTER(ch) <= 0) {
    rnum = real_quest(vnum);
    if (IS_HAPPYHOUR && IS_HAPPYQP) {
      happy_qp = (int)(QST_POINTS(rnum) * (((float)(100+HAPPY_QP))/(float)100));
      happy_qp = MAX(happy_qp, 0);
      GET_QUESTPOINTS(ch) += happy_qp;
      send_to_char(ch,
          "%s\r\nYou have been awarded %d quest points for your service.\r\n",
          QST_DONE(rnum), happy_qp);
	} else {
      GET_QUESTPOINTS(ch) += QST_POINTS(rnum);
      send_to_char(ch,
          "%s\r\nYou have been awarded %d quest points for your service.\r\n",
          QST_DONE(rnum), QST_POINTS(rnum));
    }
    if (QST_GOLD(rnum)) {
      if ((IS_HAPPYHOUR) && (IS_HAPPYGOLD)) {
        happy_gold = (int)(QST_GOLD(rnum) * (((float)(100+HAPPY_GOLD))/(float)100));
        happy_gold = MAX(happy_gold, 0);
        increase_gold(ch, happy_gold);
        send_to_char(ch,
              "You have been awarded %d gold coins for your service.\r\n",
              happy_gold);
	  } else {
        increase_gold(ch, QST_GOLD(rnum));
        send_to_char(ch,
              "You have been awarded %d gold coins for your service.\r\n",
              QST_GOLD(rnum));
      }
    }
    if (QST_EXP(rnum)) {
      gain_exp(ch, QST_EXP(rnum));
      if ((IS_HAPPYHOUR) && (IS_HAPPYEXP)) {
        happy_exp = (int)(QST_EXP(rnum) * (((float)(100+HAPPY_EXP))/(float)100));
        happy_exp = MAX(happy_exp, 0);
        send_to_char(ch,
              "You have been awarded %d experience for your service.\r\n",
              happy_exp);
      } else {
        send_to_char(ch,
              "You have been awarded %d experience points for your service.\r\n",
              QST_EXP(rnum));
      }
    }
    if (QST_OBJ(rnum) && QST_OBJ(rnum) != NOTHING) {
      if (real_object(QST_OBJ(rnum)) != NOTHING) {
        if ((new_obj = read_object((QST_OBJ(rnum)),VIRTUAL)) != NULL) {
            obj_to_char(new_obj, ch);
            send_to_char(ch, "You have been presented with %s%s for your service.\r\n",
                GET_OBJ_SHORT(new_obj), CCNRM(ch, C_NRM));
        }
      }
    }
    if (!IS_SET(QST_FLAGS(rnum), AQ_REPEATABLE))
      add_completed_quest(ch, vnum);
    clear_quest(ch);
    if ((real_quest(QST_NEXT(rnum)) != NOTHING) &&
        (QST_NEXT(rnum) != vnum) &&
        !is_complete(ch, QST_NEXT(rnum))) {
      rnum = real_quest(QST_NEXT(rnum));
      set_quest(ch, rnum);
      send_to_char(ch,
          "The next stage of your quest awaits:\r\n%s",
          QST_INFO(rnum));
    }
  }
  save_char(ch);
}

void autoquest_trigger_check(struct char_data *ch, struct char_data *vict,
                struct obj_data *object, int type)
{
  struct char_data *i;
  qst_rnum rnum;
  int found = TRUE;

  if (IS_NPC(ch))
    return;
  if (GET_QUEST(ch) == NOTHING)  /* No current quest, skip this */
    return;
  if (GET_QUEST_TYPE(ch) != type)
    return;
  if ((rnum = real_quest(GET_QUEST(ch))) == NOTHING)
    return;
  switch (type) {
    case AQ_OBJ_FIND:
      if (QST_TARGET(rnum) == GET_OBJ_VNUM(object))
        generic_complete_quest(ch);
      break;
    case AQ_ROOM_FIND:
      if (QST_TARGET(rnum) == world[IN_ROOM(ch)].number)
        generic_complete_quest(ch);
      break;
    case AQ_MOB_FIND:
      for (i=world[IN_ROOM(ch)].people; i; i = i->next_in_room)
        if (IS_NPC(i))
          if (QST_TARGET(rnum) == GET_MOB_VNUM(i))
            generic_complete_quest(ch);
      break;
    case AQ_MOB_KILL:
      if (!IS_NPC(ch) && IS_NPC(vict) && (ch != vict))
          if (QST_TARGET(rnum) == GET_MOB_VNUM(vict))
            generic_complete_quest(ch);
      break;
    case AQ_MOB_SAVE:
       if (ch == vict)
        found = FALSE;
      for (i = world[IN_ROOM(ch)].people; i && found; i = i->next_in_room)
          if (i && IS_NPC(i) && !MOB_FLAGGED(i, MOB_NOTDEADYET))
            if ((GET_MOB_VNUM(i) != QST_TARGET(rnum)) &&
                !AFF_FLAGGED(i, AFF_CHARM))
              found = FALSE;
      if (found)
        generic_complete_quest(ch);
      break;
    case AQ_OBJ_RETURN:
      if (IS_NPC(vict) && (GET_MOB_VNUM(vict) == QST_RETURNMOB(rnum)))
        if (object && (GET_OBJ_VNUM(object) == QST_TARGET(rnum)))
          generic_complete_quest(ch);
      break;
    case AQ_ROOM_CLEAR:
      if (QST_TARGET(rnum) == world[IN_ROOM(ch)].number) {
        for (i = world[IN_ROOM(ch)].people; i && found; i = i->next_in_room)
          if (i && IS_NPC(i) && !MOB_FLAGGED(i, MOB_NOTDEADYET))
            found = FALSE;
        if (found)
   generic_complete_quest(ch);
      }
      break;
    default:
      log("SYSERR: Invalid quest type passed to autoquest_trigger_check");
      break;
  }
}

void quest_timeout(struct char_data *ch)
{
  if ((GET_QUEST(ch) != NOTHING) && (GET_QUEST_TIME(ch) != -1)) {
    clear_quest(ch);
    send_to_char(ch, "You have run out of time to complete the quest.\r\n");
  }
}

void check_timed_quests(void)
{
  struct char_data *ch;

  for (ch = character_list; ch; ch = ch->next) {
    if (!IS_NPC(ch) && (GET_QUEST(ch) != NOTHING) && (GET_QUEST_TIME(ch) != -1))
      if (--GET_QUEST_TIME(ch) == 0)
        quest_timeout(ch);
    if (!IS_NPC(ch))
      expire_kill_quest_if_needed(ch, TRUE);
    if (!IS_NPC(ch))
      expire_campaign_if_needed(ch, TRUE);
    if (!IS_NPC(ch))
      notify_quest_cooldown_ready_if_needed(ch);
  }
}

void quest_kill_trigger_check(struct char_data *ch, struct char_data *vict)
{
  if (!ch || IS_NPC(ch) || !vict || !IS_NPC(vict))
    return;
  expire_kill_quest_if_needed(ch, TRUE);
  if (!is_on_quest(ch) || is_quest_ready(ch) || is_quest_expired(ch))
    return;
  if (GET_KQUEST_TARGET(ch) != GET_MOB_VNUM(vict))
    return;

  GET_KQUEST_COMPLETE(ch) = 1;
  send_to_char(ch, "\tRQuest Target Slain!\tn\r\n");
  send_to_char(ch, "\tYQUEST: You have almost completed your QUEST!\tn\r\n");
  send_to_char(ch, "\tYReturn to the questmaster before your time runs out.\tn\r\n");
  save_char(ch);
}

void campaign_kill_trigger_check(struct char_data *ch, struct char_data *vict)
{
  int i, remaining;

  if (!ch || IS_NPC(ch) || !vict || !IS_NPC(vict))
    return;

  expire_campaign_if_needed(ch, TRUE);
  if (!is_on_campaign(ch))
    return;

  for (i = 0; i < GET_CAMPAIGN_TARGET_COUNT(ch); i++) {
    if (GET_CAMPAIGN_TARGET_REMAINING(ch, i) <= 0)
      continue;
    if (GET_CAMPAIGN_TARGET_VNUM(ch, i) != GET_MOB_VNUM(vict))
      continue;

    GET_CAMPAIGN_TARGET_REMAINING(ch, i)--;
    remaining = campaign_remaining_targets(ch);
    send_to_char(ch, "%sCAMPAIGN:%s You have slain one of your campaign targets: %s%s%s.\r\n",
                 QYEL, QNRM, QGRN, campaign_target_name(GET_CAMPAIGN_TARGET_VNUM(ch, i)), QNRM);
    send_to_char(ch, "%sCAMPAIGN:%s %d target%s remain.\r\n",
                 QYEL, QNRM, remaining, remaining == 1 ? "" : "s");

    if (remaining <= 0) {
      int campaign_trains = GET_CAMPAIGN_REWARD_TRAINS(ch);
      int campaign_practices = GET_CAMPAIGN_REWARD_PRACTICES(ch);

      gain_exp(ch, GET_CAMPAIGN_REWARD_XP(ch));
      GET_QUESTPOINTS(ch) += GET_CAMPAIGN_REWARD_QP(ch);
      increase_gold(ch, GET_CAMPAIGN_REWARD_GOLD(ch));
      GET_TRAINS(ch) = MIN(INT_MAX - campaign_trains, GET_TRAINS(ch)) + campaign_trains;
      GET_PRACTICES(ch) = MIN(INT_MAX - campaign_practices, GET_PRACTICES(ch)) + campaign_practices;
      send_to_char(ch, "%sCAMPAIGN:%s You have completed your campaign!\r\n", QYEL, QNRM);
      send_to_char(ch, "%sCAMPAIGN:%s You receive %s%d%s experience, %s%d%s gold, %s%d%s quest points, %s%d%s train%s, and %s%d%s practice%s.\r\n",
                   QYEL, QNRM,
                   QGRN, GET_CAMPAIGN_REWARD_XP(ch), QNRM,
                   QGRN, GET_CAMPAIGN_REWARD_GOLD(ch), QNRM,
                   QGRN, GET_CAMPAIGN_REWARD_QP(ch), QNRM,
                   QGRN, campaign_trains, QNRM, campaign_trains == 1 ? "" : "s",
                   QGRN, campaign_practices, QNRM, campaign_practices == 1 ? "" : "s");
      clear_campaign(ch);
    }
    save_char(ch);
    return;
  }
}

int is_player_quest_target(struct char_data *viewer, struct char_data *mob)
{
  if (!viewer || IS_NPC(viewer) || !mob || !IS_NPC(mob))
    return FALSE;
  expire_kill_quest_if_needed(viewer, FALSE);
  if (!is_on_quest(viewer) || is_quest_ready(viewer) || is_quest_expired(viewer))
    return FALSE;
  if (GET_KQUEST_TARGET(viewer) != GET_MOB_VNUM(mob))
    return FALSE;
  if (GET_KQUEST_TARGET_ID(viewer) > 0 && GET_KQUEST_TARGET_ID(viewer) != char_script_id(mob))
    return FALSE;

  return TRUE;
}

/*--------------------------------------------------------------------------*/
/* Quest Command Helper Functions                                           */
/*--------------------------------------------------------------------------*/

void list_quests(struct char_data *ch, zone_rnum zone, qst_vnum vmin, qst_vnum vmax)
{
  qst_rnum rnum;
  qst_vnum bottom, top;
  int counter = 0;

  if (zone != NOWHERE) {
    bottom = zone_table[zone].bot;
    top    = zone_table[zone].top;
  } else {
    bottom = vmin;
    top    = vmax;
  }
  /* Print the header for the quest listing. */
  send_to_char (ch,
  "Index VNum    Description                                  Questmaster\r\n"
  "----- ------- -------------------------------------------- -----------\r\n");
  for (rnum = 0; rnum < total_quests ; rnum++)
    if (QST_NUM(rnum) >= bottom && QST_NUM(rnum) <= top)
      send_to_char(ch, "\tg%4d\tn) [\tg%-5d\tn] \tc%-44.44s\tn \ty[%5d]\tn\r\n",
          ++counter, QST_NUM(rnum), QST_NAME(rnum),
          QST_MASTER(rnum) == NOBODY ? 0 : QST_MASTER(rnum));
  if (!counter)
    send_to_char(ch, "None found.\r\n");
}



void quest_list(struct char_data *ch, struct char_data *qm, char argument[MAX_INPUT_LENGTH])
{
  qst_vnum vnum;
  qst_rnum rnum;

  if ((vnum = find_quest_by_qmnum(ch, GET_MOB_VNUM(qm), atoi(argument))) == NOTHING)
    send_to_char(ch, "That is not a valid quest!\r\n");
  else if ((rnum = real_quest(vnum)) == NOTHING)
    send_to_char(ch, "That is not a valid quest!\r\n");
  else if (QST_INFO(rnum)) {
    send_to_char(ch,"Complete Details on Quest %d \tc%s\tn:\r\n%s",
                      vnum,
         QST_DESC(rnum),
         QST_INFO(rnum));
    if (QST_PREV(rnum) != NOTHING)
      send_to_char(ch, "You have to have completed quest %s first.\r\n",
          QST_NAME(real_quest(QST_PREV(rnum))));
    if (QST_TIME(rnum) != -1)
      send_to_char(ch,
         "There is a time limit of %d turn%s to complete the quest.\r\n",
          QST_TIME(rnum),
          QST_TIME(rnum) == 1 ? "" : "s");
  } else
    send_to_char(ch, "There is no further information on that quest.\r\n");
}




static void quest_stat(struct char_data *ch, const char *argument)
{
  qst_rnum rnum;
  mob_rnum qmrnum;
  char buf[MAX_STRING_LENGTH];
  char targetname[MAX_STRING_LENGTH];

  if (!*argument)
    send_to_char(ch, "%s\r\n", quest_imm_usage);
  else if ((rnum = real_quest(atoi(argument))) == NOTHING )
    send_to_char(ch, "That quest does not exist.\r\n");
  else {
    sprintbit(QST_FLAGS(rnum), aq_flags, buf, sizeof(buf));
    switch (QST_TYPE(rnum)) {
      case AQ_OBJ_FIND:
      case AQ_OBJ_RETURN:
        snprintf(targetname, sizeof(targetname), "%s",
                 real_object(QST_TARGET(rnum)) == NOTHING ?
                 "An unknown object" :
    obj_proto[real_object(QST_TARGET(rnum))].short_description);
 break;
      case AQ_ROOM_FIND:
      case AQ_ROOM_CLEAR:
        snprintf(targetname, sizeof(targetname), "%s",
          real_room(QST_TARGET(rnum)) == NOWHERE ?
                 "An unknown room" :
    world[real_room(QST_TARGET(rnum))].name);
        break;
      case AQ_MOB_FIND:
      case AQ_MOB_KILL:
      case AQ_MOB_SAVE:
 snprintf(targetname, sizeof(targetname), "%s",
                 real_mobile(QST_TARGET(rnum)) == NOBODY ?
    "An unknown mobile" :
    GET_NAME(&mob_proto[real_mobile(QST_TARGET(rnum))]));
 break;
      default:
 snprintf(targetname, sizeof(targetname), "Unknown");
 break;
    }
    qmrnum = real_mobile(QST_MASTER(rnum));
    send_to_char(ch,
        "VNum  : [\ty%5d\tn], RNum: [\ty%5d\tn] -- Questmaster: [\ty%5d\tn] \ty%s\tn\r\n"
        "Name  : \ty%s\tn\r\n"
 "Desc  : \ty%s\tn\r\n"
 "Accept Message:\r\n\tc%s\tn"
 "Completion Message:\r\n\tc%s\tn"
 "Quit Message:\r\n\tc%s\tn"
 "Type  : \ty%s\tn\r\n"
        "Target: \ty%d\tn \ty%s\tn, Quantity: \ty%d\tn\r\n"
 "Value : \ty%d\tn, Penalty: \ty%d\tn, Min Level: \ty%2d\tn, Max Level: \ty%2d\tn\r\n"
 "Flags : \tc%s\tn\r\n",
     QST_NUM(rnum), rnum,
 QST_MASTER(rnum) == NOBODY ? -1 : QST_MASTER(rnum),
 (qmrnum == NOBODY) ? "(Invalid vnum)" : GET_NAME(&mob_proto[(qmrnum)]),
        QST_NAME(rnum), QST_DESC(rnum),
        QST_INFO(rnum), QST_DONE(rnum),
 (QST_QUIT(rnum) &&
  (str_cmp(QST_QUIT(rnum), "undefined") != 0)
          ? QST_QUIT(rnum) : "Nothing\r\n"),
     quest_types[QST_TYPE(rnum)],
 QST_TARGET(rnum) == NOBODY ? -1 : QST_TARGET(rnum),
 targetname,
 QST_QUANTITY(rnum),
     QST_POINTS(rnum), QST_PENALTY(rnum), QST_MINLEVEL(rnum),
 QST_MAXLEVEL(rnum), buf);
    if (QST_PREREQ(rnum) != NOTHING)
      send_to_char(ch, "Preq  : [\ty%5d\tn] \ty%s\tn\r\n",
        QST_PREREQ(rnum) == NOTHING ? -1 : QST_PREREQ(rnum),
        QST_PREREQ(rnum) == NOTHING ? "" :
   real_object(QST_PREREQ(rnum)) == NOTHING ? "an unknown object" :
       obj_proto[real_object(QST_PREREQ(rnum))].short_description);
    if (QST_TYPE(rnum) == AQ_OBJ_RETURN)
      send_to_char(ch, "Mob   : [\ty%5d\tn] \ty%s\tn\r\n",
        QST_RETURNMOB(rnum),
 real_mobile(QST_RETURNMOB(rnum)) == NOBODY ? "an unknown mob" :
           mob_proto[real_mobile(QST_RETURNMOB(rnum))].player.short_descr);
    if (QST_TIME(rnum) != -1)
      send_to_char(ch, "Limit : There is a time limit of %d turn%s to complete.\r\n",
   QST_TIME(rnum),
   QST_TIME(rnum) == 1 ? "" : "s");
    else
      send_to_char(ch, "Limit : There is no time limit on this quest.\r\n");
    send_to_char(ch, "Prior :");
    if (QST_PREV(rnum) == NOTHING)
      send_to_char(ch, " \tyNone.\tn\r\n");
    else
      send_to_char(ch, " [\ty%5d\tn] \tc%s\tn\r\n",
        QST_PREV(rnum), QST_DESC(real_quest(QST_PREV(rnum))));
    send_to_char(ch, "Next  :");
    if (QST_NEXT(rnum) == NOTHING)
      send_to_char(ch, " \tyNone.\tn\r\n");
    else
      send_to_char(ch, " [\ty%5d\tn] \tc%s\tn\r\n",
        QST_NEXT(rnum), QST_DESC(real_quest(QST_NEXT(rnum))));
  }
}

/*--------------------------------------------------------------------------*/
/* Quest Command Processing Function and Questmaster Special                */
/*--------------------------------------------------------------------------*/

ACMD(do_quest)
{
  char arg1[MAX_INPUT_LENGTH], arg2[MAX_INPUT_LENGTH];
  int  tp;

  two_arguments(argument, arg1, arg2);
  if (!*arg1)
    send_to_char(ch, "%s\r\n", GET_LEVEL(ch) < LVL_IMMORT ?
                     quest_mort_usage : quest_imm_usage);
  else if (((tp = search_block(arg1, quest_cmd, FALSE)) == -1))
    send_to_char(ch, "%s\r\n", GET_LEVEL(ch) < LVL_IMMORT ?
                     quest_mort_usage : quest_imm_usage);
  else {
    switch (tp) {
      case SCMD_QUEST_REQUEST:
        quest_request_kill(ch);
        break;
      case SCMD_QUEST_LIST:
        quest_list_rewards(ch);
        break;
      case SCMD_QUEST_BUY:
        quest_buy_reward(ch, arg2);
        break;
      case SCMD_QUEST_PROGRESS:
        quest_progress_kill(ch);
        break;
      case SCMD_QUEST_DROP:
        quest_drop_kill(ch);
        break;
      case SCMD_QUEST_INFO:
        quest_info_kill(ch);
        break;
      case SCMD_QUEST_COMPLETE:
        quest_complete_kill(ch);
        break;
      case SCMD_QUEST_STATUS:
        if (GET_LEVEL(ch) < LVL_IMMORT)
          send_to_char(ch, "%s\r\n", quest_mort_usage);
        else
          quest_stat(ch, arg2);
        break;
      default:
        send_to_char(ch, "%s\r\n",
                     GET_LEVEL(ch) < LVL_IMMORT ? quest_mort_usage : quest_imm_usage);
        break;
    } /* switch on subcmd number */
  }
}

ACMD(do_campaign)
{
  char arg1[MAX_INPUT_LENGTH];
  int tp;

  one_argument(argument, arg1);
  if (!*arg1) {
    send_to_char(ch, "Campaign what? Options are - Request, Info, Quit, Check, Brief and Today.\r\n");
    return;
  }

  tp = search_block(arg1, campaign_cmd, FALSE);
  if (tp < 0) {
    send_to_char(ch, "Campaign what? Options are - Request, Info, Quit, Check, Brief and Today.\r\n");
    return;
  }

  switch (tp) {
    case 0: campaign_request(ch); break;
    case 1: campaign_info(ch); break;
    case 2: campaign_quit(ch); break;
    case 3: campaign_check(ch); break;
    case 4: campaign_brief(ch); break;
    case 5: campaign_today(ch); break;
    default:
      send_to_char(ch, "Campaign what? Options are - Request, Info, Quit, Check, Brief and Today.\r\n");
      break;
  }
}

SPECIAL(questmaster)
{
  qst_rnum rnum;
  struct char_data *qm = (struct char_data *)me;

  /* Keep legacy authored questmasters alive as compatibility wrappers.
   * Their saved secondary special proc still gets first chance to handle
   * unrelated commands, but player quest commands now belong to do_quest. */
  for (rnum = 0; (rnum < total_quests &&
       QST_MASTER(rnum) != GET_MOB_VNUM(qm)); rnum++);

  if (rnum >= total_quests)
    return FALSE;

  if (QST_FUNC(rnum) && (QST_FUNC(rnum)(ch, me, cmd, argument)))
    return TRUE;

  return FALSE;
}
