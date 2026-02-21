#include "conf.h"
#include "sysdep.h"

#include "structs.h"
#include "utils.h"
#include "comm.h"
#include "handler.h"
#include "interpreter.h"
#include "db.h"
#include "fight.h"
#include "graph.h"
#include "ai_actor.h"
#include "ai_actor_brain.h"

#define AIG_PROTECT    (1 << 0)
#define AIG_TRADE      (1 << 1)
#define AIG_SERVE      (1 << 2)
#define AIG_HUNT       (1 << 3)
#define AIG_FLEE       (1 << 4)
#define AIG_RECRUIT    (1 << 5)
#define AIG_INTIMIDATE (1 << 6)
#define AIG_REPORT     (1 << 7)
#define AIG_ASSIST     (1 << 8)

static int g_aictl_enabled = TRUE;

enum ai_actor_archetype {
  ARCH_GUARD = 0, ARCH_CONSTABLE, ARCH_MERCHANT, ARCH_INNKEEPER, ARCH_BANDIT,
  ARCH_CULTIST, ARCH_WOLF, ARCH_SKELETON, ARCH_SPIRIT, ARCH_COMMANDER, ARCH_GENERIC
};

static const char *state_name(enum ai_actor_brain_state s) {
  switch (s) {
    case AI_BRAIN_IDLE: return "Idle"; case AI_BRAIN_OBSERVE: return "Observe";
    case AI_BRAIN_ENGAGE: return "Engage"; case AI_BRAIN_WARN: return "Warn";
    case AI_BRAIN_ASSIST: return "Assist"; case AI_BRAIN_FLEE: return "Flee";
    case AI_BRAIN_PURSUE: return "Pursue"; case AI_BRAIN_REPORT: return "Report";
    case AI_BRAIN_TRADE: return "Trade"; default: return "Unknown";
  }
}

static int arch_from_vnum(mob_vnum vnum) {
  if (vnum < 19500 || vnum > 19509) return ARCH_GENERIC;
  return vnum - 19500;
}

static struct ai_actor_brain_mem *mem_get(struct ai_actor_brain *b, long idnum, time_t now) {
  int i, ev = 0, score = 999999;
  if (!b || idnum <= 0) return NULL;
  for (i = 0; i < b->mem_count; i++) if (b->mem[i].idnum == idnum) return &b->mem[i];
  if (b->mem_count < AI_BRAIN_MEM_MAX) {
    memset(&b->mem[b->mem_count], 0, sizeof(b->mem[b->mem_count]));
    b->mem[b->mem_count].idnum = idnum;
    b->mem[b->mem_count].relationship = AI_REL_NEUTRAL;
    b->mem[b->mem_count].last_update = now;
    return &b->mem[b->mem_count++];
  }
  for (i = 0; i < AI_BRAIN_MEM_MAX; i++) {
    int s = abs(b->mem[i].trust) + abs(b->mem[i].fear) + abs(b->mem[i].hostility) + b->mem[i].crime_flags;
    if (s < score) { score = s; ev = i; }
  }
  memset(&b->mem[ev], 0, sizeof(b->mem[ev]));
  b->mem[ev].idnum = idnum;
  b->mem[ev].relationship = AI_REL_NEUTRAL;
  b->mem[ev].last_update = now;
  return &b->mem[ev];
}

static void brain_say(struct char_data *mob, struct char_data *target, const char *msg, int combat_event) {
  static room_vnum room_last = NOWHERE;
  static time_t room_last_t = 0;
  static int room_count = 0;
  char buf[256];
  struct ai_actor_brain *b;
  room_vnum rv;

  if (!mob || !msg || !*msg || IN_ROOM(mob) == NOWHERE || !g_aictl_enabled) return;
  b = mob->ai_state ? mob->ai_state->brain : NULL;
  if (!b) return;
  if (time(0) < b->next_global_speak) return;

  rv = GET_ROOM_VNUM(IN_ROOM(mob));
  if (room_last != rv || room_last_t != time(0)) { room_last = rv; room_last_t = time(0); room_count = 0; }
  if ((!combat_event && room_count >= 1) || (combat_event && room_count >= 2)) return;
  if (rand_number(0, 99) > 42 + b->traits.curiosity / 2) return;

  if (target)
    snprintf(buf, sizeof(buf), "$n says to %s, '%s'", GET_NAME(target), msg);
  else
    snprintf(buf, sizeof(buf), "$n says, '%s'", msg);
  act(buf, FALSE, mob, NULL, NULL, TO_ROOM);
  b->next_global_speak = time(0) + rand_number(8, 18);
  room_count++;
}

static void apply_profile(struct ai_actor_brain *b, mob_vnum vnum) {
  if (!b) return;
  memset(b, 0, sizeof(*b));
  b->archetype = arch_from_vnum(vnum);
  b->social_style = SOC_WARNING;
  b->goal_mask = AIG_PROTECT;
  b->state = AI_BRAIN_IDLE;
  b->roam_allowed = FALSE;
  b->next_move_at = time(0) + rand_number(20, 60);

  switch (b->archetype) {
    case ARCH_GUARD: b->traits = (struct ai_actor_traits){80,20,30,50,55,80,30}; b->goal_mask=AIG_PROTECT|AIG_INTIMIDATE; break;
    case ARCH_CONSTABLE: b->traits = (struct ai_actor_traits){70,20,35,45,50,85,25}; b->goal_mask=AIG_PROTECT|AIG_REPORT; b->roam_allowed=TRUE; break;
    case ARCH_MERCHANT: b->traits = (struct ai_actor_traits){30,80,40,45,15,65,40}; b->goal_mask=AIG_TRADE|AIG_FLEE; b->state=AI_BRAIN_TRADE; break;
    case ARCH_INNKEEPER: b->traits = (struct ai_actor_traits){35,55,45,65,20,70,35}; b->goal_mask=AIG_SERVE|AIG_TRADE|AIG_FLEE; b->state=AI_BRAIN_TRADE; break;
    case ARCH_BANDIT: b->traits = (struct ai_actor_traits){55,75,35,10,70,40,20}; b->goal_mask=AIG_HUNT|AIG_INTIMIDATE|AIG_FLEE; b->roam_allowed=TRUE; break;
    case ARCH_CULTIST: b->traits = (struct ai_actor_traits){50,30,65,25,45,35,90}; b->goal_mask=AIG_RECRUIT|AIG_SERVE; break;
    case ARCH_WOLF: b->traits = (struct ai_actor_traits){45,5,30,5,60,30,10}; b->goal_mask=AIG_HUNT|AIG_FLEE; b->roam_allowed=TRUE; break;
    case ARCH_SKELETON: b->traits = (struct ai_actor_traits){70,0,0,0,65,50,0}; b->goal_mask=AIG_HUNT; b->social_style=SOC_SILENT; break;
    case ARCH_SPIRIT: b->traits = (struct ai_actor_traits){25,5,75,15,20,20,95}; b->goal_mask=AIG_SERVE|AIG_FLEE; break;
    case ARCH_COMMANDER: b->traits = (struct ai_actor_traits){85,15,40,35,60,90,20}; b->goal_mask=AIG_PROTECT|AIG_ASSIST|AIG_INTIMIDATE; break;
    default: b->traits = (struct ai_actor_traits){50,50,50,50,50,50,50}; b->goal_mask=AIG_PROTECT; break;
  }
}

void ai_actor_brain_init(struct char_data *mob) {
  if (!mob || !mob->ai_state || !IS_NPC(mob) || !MOB_FLAGGED(mob, MOB_AI_ACTOR)) return;
  if (!mob->ai_state->brain) CREATE(mob->ai_state->brain, struct ai_actor_brain, 1);
  if (!mob->ai_state->brain) return;
  apply_profile(mob->ai_state->brain, GET_MOB_VNUM(mob));
}

void ai_actor_brain_free(struct char_data *mob) {
  if (!mob || !mob->ai_state || !mob->ai_state->brain) return;
  free(mob->ai_state->brain);
  mob->ai_state->brain = NULL;
}

static void update_rel(struct ai_actor_brain_mem *m) {
  if (!m) return;
  if (m->hostility + m->fear > 14 || m->crime_flags) m->relationship = AI_REL_ENEMY;
  else if (m->trust > 10 && m->hostility < 4) m->relationship = AI_REL_ALLY;
  else m->relationship = AI_REL_NEUTRAL;
}

static void decay_memory(struct ai_actor_brain *b, time_t now) {
  int i;
  if (!b) return;
  for (i = 0; i < b->mem_count; i++) {
    int steps;
    struct ai_actor_brain_mem *m = &b->mem[i];
    if (m->idnum <= 0) continue;
    steps = (int)((now - m->last_update) / 60);
    if (steps <= 0) continue;
    if (steps > 4) steps = 4;
    while (steps-- > 0) {
      if (m->trust > 0) m->trust--;
      if (m->fear > 0) m->fear--;
      if (m->hostility > 0) m->hostility--;
    }
    if ((now - m->last_update) > 180 && m->crime_flags) m->crime_flags &= ~MEM_TRESPASS;
    if ((now - m->last_update) > 300 && m->crime_flags) m->crime_flags &= ~(MEM_STOLE | MEM_ATTACKED_ME);
    if ((now - m->last_update) > 420 && m->crime_flags) m->crime_flags &= ~MEM_WANTED;
    if ((now - m->last_update) > 540 && m->crime_flags) m->crime_flags &= ~MEM_MURDER;
    update_rel(m);
    m->last_update = now;
  }
}

static void remember_actor(struct char_data *mob, struct char_data *actor, const char *summary, int trust, int fear, int hostility, int crimes) {
  struct ai_actor_brain *b;
  struct ai_actor_brain_mem *m;
  if (!mob || !actor || IS_NPC(actor) || !mob->ai_state) return;
  b = mob->ai_state->brain;
  if (!b) return;
  m = mem_get(b, GET_IDNUM(actor), time(0));
  if (!m) return;
  m->trust += trust;
  m->fear += fear;
  m->hostility += hostility;
  m->crime_flags |= crimes;
  m->last_seen = time(0);
  m->last_update = time(0);
  if (summary && *summary) strlcpy(m->last_action, summary, sizeof(m->last_action));
  update_rel(m);
}

int ai_actor_brain_enabled(void) { return g_aictl_enabled; }
void ai_actor_brain_set_enabled(int enabled) { g_aictl_enabled = (enabled ? TRUE : FALSE); }

void ai_actor_brain_on_enter(struct char_data *mob, struct char_data *actor) {
  struct ai_actor_brain *b;
  if (!mob || !actor || IS_NPC(actor) || !mob->ai_state) return;
  b = mob->ai_state->brain; if (!b) return;
  remember_actor(mob, actor, "entered room", 0, 0, 0, 0);
  b->state = (b->goal_mask & AIG_TRADE) ? AI_BRAIN_TRADE : AI_BRAIN_OBSERVE;
  if (b->archetype == ARCH_GUARD || b->archetype == ARCH_CONSTABLE)
    brain_say(mob, actor, "Keep the peace here.", FALSE);
  else if (b->archetype == ARCH_MERCHANT || b->archetype == ARCH_INNKEEPER)
    brain_say(mob, actor, "Welcome. Looking for wares or rest?", FALSE);
}

void ai_actor_brain_on_leave(struct char_data *mob, struct char_data *actor) {
  if (!mob || !actor || IS_NPC(actor) || !mob->ai_state || !mob->ai_state->brain) return;
  remember_actor(mob, actor, "left room", 0, 0, 0, 0);
  mob->ai_state->brain->state = AI_BRAIN_IDLE;
}

static const char *extract_keyword(const char *msg, char *out, size_t outsz) {
  size_t i = 0;
  if (!msg || !*msg) return "";
  while (*msg && !isalpha((unsigned char)*msg)) msg++;
  while (*msg && isalpha((unsigned char)*msg) && i + 1 < outsz) out[i++] = LOWER(*msg++);
  out[i] = '\0';
  return out;
}

void ai_actor_brain_on_say(struct char_data *mob, struct char_data *actor, const char *msg) {
  char key[24];
  struct ai_actor_brain *b;
  if (!mob || !actor || IS_NPC(actor) || !mob->ai_state) return;
  b = mob->ai_state->brain; if (!b) return;
  remember_actor(mob, actor, "spoke", 1, 0, 0, 0);
  snprintf(b->last_speaker, sizeof(b->last_speaker), "%s", GET_NAME(actor));
  snprintf(b->last_loud_event, sizeof(b->last_loud_event), "say:%s", extract_keyword(msg, key, sizeof(key)));

  if ((strstr(msg, "help") || strstr(msg, "guard")) && (b->archetype == ARCH_GUARD || b->archetype == ARCH_CONSTABLE)) {
    b->state = AI_BRAIN_REPORT;
    brain_say(mob, actor, "Report the crime clearly.", FALSE);
  } else if (b->archetype == ARCH_SPIRIT) {
    char reply[96];
    snprintf(reply, sizeof(reply), "...%s...", key[0] ? key : "echo");
    brain_say(mob, actor, reply, FALSE);
  } else if (b->archetype == ARCH_CULTIST && (strstr(msg, "faith") || strstr(msg, "power"))) {
    b->state = AI_BRAIN_ENGAGE;
    brain_say(mob, actor, "Join us and be remade.", FALSE);
  }
}

void ai_actor_brain_on_emote(struct char_data *mob, struct char_data *actor, const char *msg) {
  if (!mob || !actor || IS_NPC(actor)) return;
  remember_actor(mob, actor, msg ? msg : "emoted", 0, 0, 0, 0);
}

void ai_actor_brain_on_combat_start(struct char_data *mob, struct char_data *attacker, struct char_data *victim) {
  struct ai_actor_brain *b;
  if (!mob || !mob->ai_state || !mob->ai_state->brain || !attacker || !victim) return;
  b = mob->ai_state->brain;
  snprintf(b->last_loud_event, sizeof(b->last_loud_event), "combat erupted");
  if (!IS_NPC(attacker)) remember_actor(mob, attacker, "started combat", -2, 3, 6, MEM_ASSAULT);
  if (b->archetype == ARCH_GUARD || b->archetype == ARCH_CONSTABLE) {
    b->state = AI_BRAIN_WARN;
    brain_say(mob, attacker, "Stand down now!", TRUE);
    if (!FIGHTING(mob) && IN_ROOM(mob) == IN_ROOM(attacker)) hit(mob, attacker, 0);
  } else if (b->archetype == ARCH_MERCHANT || b->archetype == ARCH_INNKEEPER) {
    b->state = AI_BRAIN_FLEE;
  }
}

void ai_actor_brain_on_attacked(struct char_data *mob, struct char_data *attacker, int dam) {
  struct ai_actor_brain *b;
  if (!mob || !attacker || IS_NPC(attacker) || !mob->ai_state || !mob->ai_state->brain) return;
  b = mob->ai_state->brain;
  remember_actor(mob, attacker, "attacked me", -3, 6, MAX(3, dam / 6), MEM_ASSAULT | MEM_WANTED);
  b->state = AI_BRAIN_ENGAGE;
}

void ai_actor_brain_on_corpse(struct char_data *mob, struct char_data *dead) {
  if (!mob || !mob->ai_state || !mob->ai_state->brain) return;
  strlcpy(mob->ai_state->brain->last_loud_event, "corpse appeared", sizeof(mob->ai_state->brain->last_loud_event));
  if (mob->ai_state->brain->archetype == ARCH_GUARD)
    mob->ai_state->brain->state = AI_BRAIN_REPORT;
}

void ai_actor_brain_on_drop(struct char_data *mob, struct char_data *actor, struct obj_data *obj) {
  if (!mob || !actor || IS_NPC(actor) || !mob->ai_state || !mob->ai_state->brain) return;
  remember_actor(mob, actor, "dropped item", 0, 0, 0, 0);
  if ((mob->ai_state->brain->archetype == ARCH_MERCHANT || mob->ai_state->brain->archetype == ARCH_INNKEEPER) && obj)
    brain_say(mob, actor, "Careful where you leave your goods.", FALSE);
}

void ai_actor_brain_on_give(struct char_data *mob, struct char_data *actor, struct obj_data *obj, struct char_data *to) {
  if (!mob || !actor || IS_NPC(actor) || !to || to != mob) return;
  remember_actor(mob, actor, obj ? "gifted item" : "gave something", 4, 0, 0, 0);
  brain_say(mob, actor, "A fair exchange.", FALSE);
}

int ai_actor_brain_think(struct char_data *mob, time_t now) {
  struct ai_actor_brain *b;
  struct char_data *plr;

  if (!mob || !mob->ai_state || !mob->ai_state->brain || !g_aictl_enabled) return FALSE;
  b = mob->ai_state->brain;
  if ((now - b->last_think) < 2) return FALSE;
  b->last_think = now;
  decay_memory(b, now);

  if (FIGHTING(mob)) {
    b->state = AI_BRAIN_ENGAGE;
    if (b->archetype == ARCH_COMMANDER) brain_say(mob, NULL, "Hold the line!", TRUE);
    return FALSE;
  }

  for (plr = world[IN_ROOM(mob)].people; plr; plr = plr->next_in_room) {
    struct ai_actor_brain_mem *m;
    if (IS_NPC(plr)) continue;
    m = mem_get(b, GET_IDNUM(plr), now);
    if (!m) continue;
    if ((b->archetype == ARCH_GUARD || b->archetype == ARCH_CONSTABLE) && (m->crime_flags & (MEM_WANTED | MEM_MURDER | MEM_ASSAULT))) {
      b->state = AI_BRAIN_WARN;
      brain_say(mob, plr, "You are remembered. Behave yourself.", FALSE);
      break;
    }
    if (b->archetype == ARCH_BANDIT && m->relationship != AI_REL_ENEMY && GET_LEVEL(plr) < GET_LEVEL(mob)) {
      b->state = AI_BRAIN_PURSUE;
      brain_say(mob, plr, "Keep walking, unless your purse is heavy.", FALSE);
      if (!rand_number(0, 2)) hit(mob, plr, 0);
      break;
    }
  }

  if (b->roam_allowed && !MOB_FLAGGED(mob, MOB_SENTINEL) && now >= b->next_move_at && GET_POS(mob) == POS_STANDING) {
    int door = rand_number(0, NUM_OF_DIRS - 1);
    if (CAN_GO(mob, door) && !ROOM_FLAGGED(EXIT(mob, door)->to_room, ROOM_NOMOB) && !ROOM_FLAGGED(EXIT(mob, door)->to_room, ROOM_DEATH))
      perform_move(mob, door, 1);
    b->next_move_at = now + rand_number(20, 60);
    return TRUE;
  }
  return FALSE;
}

void ai_actor_brain_show_state(struct char_data *viewer, struct char_data *mob) {
  int i;
  struct ai_actor_brain *b;
  if (!viewer || !mob || !mob->ai_state || !mob->ai_state->brain) {
    send_to_char(viewer, "No AI actor brain data.\r\n");
    return;
  }
  b = mob->ai_state->brain;
  send_to_char(viewer, "AI State for %s (vnum %d):\r\n", GET_NAME(mob), GET_MOB_VNUM(mob));
  send_to_char(viewer, "  state=%s social=%d goals=0x%x roam=%d\r\n", state_name(b->state), b->social_style, b->goal_mask, b->roam_allowed);
  send_to_char(viewer, "  traits Bv%d Gr%d Cu%d Em%d Ag%d Di%d Su%d\r\n", b->traits.bravery, b->traits.greed, b->traits.curiosity,
               b->traits.empathy, b->traits.aggression, b->traits.discipline, b->traits.superstition);
  send_to_char(viewer, "  ctx speaker='%s' loud='%s' fight='%s'\r\n", b->last_speaker, b->last_loud_event, b->last_fight_outcome);
  send_to_char(viewer, "  memory entries=%d\r\n", b->mem_count);
  for (i = 0; i < b->mem_count; i++) {
    struct ai_actor_brain_mem *m = &b->mem[i];
    if (m->idnum <= 0) continue;
    send_to_char(viewer, "    id=%ld rel=%d trust=%d fear=%d host=%d crime=0x%x action=%s\r\n",
                 m->idnum, m->relationship, m->trust, m->fear, m->hostility, m->crime_flags,
                 m->last_action[0] ? m->last_action : "-");
  }
}
