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


#define AI_ARCH_MAX 4

typedef struct ai_brain_profile {
  uint32_t seed;
  uint8_t is_humanoid;
  uint8_t can_speak;
  uint8_t temperament;
  uint8_t cognition;
  uint8_t tolerance;
  float imtb_i;
  float imtb_m;
  float imtb_t;
  float imtb_b;
  uint8_t archetype_count;
  uint8_t archetypes[AI_ARCH_MAX];
  int role;
  int role_fitness;
  uint32_t caps;
  time_t built_at;
} ai_brain_profile;

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

static void ai_safe_append(char *dst, size_t dstsz, const char *src) {
  size_t len;
  if (!dst || dstsz == 0 || !src) return;
  len = strlen(dst);
  if (len >= dstsz - 1) return;
  snprintf(dst + len, dstsz - len, "%s", src);
}

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

static void brain_say(struct char_data *mob, struct char_data *target, const char *msg, int combat_event, int delayed_entry_reaction) {
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

  if (delayed_entry_reaction)
    ai_actor_schedule_reaction_speech(mob, target, buf);
  else
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
    brain_say(mob, actor, "Keep the peace here.", FALSE, TRUE);
  else if (b->archetype == ARCH_MERCHANT || b->archetype == ARCH_INNKEEPER)
    brain_say(mob, actor, "Welcome. Looking for wares or rest?", FALSE, TRUE);
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
    brain_say(mob, actor, "Report the crime clearly.", FALSE, FALSE);
  } else if (b->archetype == ARCH_SPIRIT) {
    char reply[96];
    snprintf(reply, sizeof(reply), "...%s...", key[0] ? key : "echo");
    brain_say(mob, actor, reply, FALSE, FALSE);
  } else if (b->archetype == ARCH_CULTIST && (strstr(msg, "faith") || strstr(msg, "power"))) {
    b->state = AI_BRAIN_ENGAGE;
    brain_say(mob, actor, "Join us and be remade.", FALSE, FALSE);
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
    brain_say(mob, attacker, "Stand down now!", TRUE, FALSE);
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
    brain_say(mob, actor, "Careful where you leave your goods.", FALSE, FALSE);
}

void ai_actor_brain_on_give(struct char_data *mob, struct char_data *actor, struct obj_data *obj, struct char_data *to) {
  if (!mob || !actor || IS_NPC(actor) || !to || to != mob) return;
  remember_actor(mob, actor, obj ? "gifted item" : "gave something", 4, 0, 0, 0);
  brain_say(mob, actor, "A fair exchange.", FALSE, FALSE);
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
    if (b->archetype == ARCH_COMMANDER) brain_say(mob, NULL, "Hold the line!", TRUE, FALSE);
    return FALSE;
  }

  for (plr = world[IN_ROOM(mob)].people; plr; plr = plr->next_in_room) {
    struct ai_actor_brain_mem *m;
    if (IS_NPC(plr)) continue;
    m = mem_get(b, GET_IDNUM(plr), now);
    if (!m) continue;
    if ((b->archetype == ARCH_GUARD || b->archetype == ARCH_CONSTABLE) && (m->crime_flags & (MEM_WANTED | MEM_MURDER | MEM_ASSAULT))) {
      b->state = AI_BRAIN_WARN;
      brain_say(mob, plr, "You are remembered. Behave yourself.", FALSE, FALSE);
      break;
    }
    if (b->archetype == ARCH_BANDIT && m->relationship != AI_REL_ENEMY && GET_LEVEL(plr) < GET_LEVEL(mob)) {
      b->state = AI_BRAIN_PURSUE;
      brain_say(mob, plr, "Keep walking, unless your purse is heavy.", FALSE, FALSE);
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
  if (mob->ai_prof)
    send_to_char(viewer, "  role=%d mode=%d temperament=%d scores=%s\r\n", mob->ai_prof->role, mob->ai_prof->mode, mob->ai_prof->aggression,
                 mob->ai_prof->matched_keywords[0] ? mob->ai_prof->matched_keywords : "-");
  if (mob->ai_state) {
    char topics[160];
    topics[0] = '\0';
    if (mob->ai_state->local_topic_mask & AI_TOPIC_MIDGAARD) ai_safe_append(topics, sizeof(topics), "MIDGAARD ");
    if (mob->ai_state->local_topic_mask & AI_TOPIC_TEMPLE) ai_safe_append(topics, sizeof(topics), "TEMPLE ");
    if (mob->ai_state->local_topic_mask & AI_TOPIC_MARKET) ai_safe_append(topics, sizeof(topics), "MARKET ");
    if (mob->ai_state->local_topic_mask & AI_TOPIC_INN) ai_safe_append(topics, sizeof(topics), "INN ");
    if (mob->ai_state->local_topic_mask & AI_TOPIC_BANK) ai_safe_append(topics, sizeof(topics), "BANK ");
    if (mob->ai_state->local_topic_mask & AI_TOPIC_ALLEY) ai_safe_append(topics, sizeof(topics), "ALLEY ");
    if (mob->ai_state->local_topic_mask & AI_TOPIC_WILDERNESS) ai_safe_append(topics, sizeof(topics), "WILDERNESS ");
    if (mob->ai_state->local_topic_mask & AI_TOPIC_DUNGEON) ai_safe_append(topics, sizeof(topics), "DUNGEON ");
    if (mob->ai_state->local_topic_mask & AI_TOPIC_SEWER) ai_safe_append(topics, sizeof(topics), "SEWER ");
    if (mob->ai_state->local_topic_mask & AI_TOPIC_CASTLE) ai_safe_append(topics, sizeof(topics), "CASTLE ");
    if (!topics[0]) snprintf(topics, sizeof(topics), "%s", "NONE");
    send_to_char(viewer, "  last_pool=%s last_reason=%s topics=0x%x [%s]\r\n",
                 mob->ai_state->last_pool_name[0] ? mob->ai_state->last_pool_name : "POOL_NONE",
                 mob->ai_state->last_speak_reason[0] ? mob->ai_state->last_speak_reason : "NONE",
                 mob->ai_state->local_topic_mask, topics);
  }
  send_to_char(viewer, "  memory entries=%d\r\n", b->mem_count);
  for (i = 0; i < b->mem_count; i++) {
    struct ai_actor_brain_mem *m = &b->mem[i];
    if (m->idnum <= 0) continue;
    send_to_char(viewer, "    id=%ld rel=%d trust=%d fear=%d host=%d crime=0x%x action=%s\r\n",
                 m->idnum, m->relationship, m->trust, m->fear, m->hostility, m->crime_flags,
                 m->last_action[0] ? m->last_action : "-");
  }
}


#define AI_BRAIN_PROFILE_MAX 1024
#define AI_BRAIN_CAP_DIRECTIONS   (1u << 0)
#define AI_BRAIN_CAP_LAW          (1u << 1)
#define AI_BRAIN_CAP_TRADE        (1u << 2)
#define AI_BRAIN_CAP_TRAINING     (1u << 3)
#define AI_BRAIN_CAP_LODGING      (1u << 4)
#define AI_BRAIN_CAP_FOOD         (1u << 5)
#define AI_BRAIN_CAP_RUMOR        (1u << 6)
#define AI_BRAIN_CAP_RELIGION     (1u << 7)
#define AI_BRAIN_CAP_QUEST        (1u << 8)

struct ai_brain_slot {
  struct char_data *mob;
  ai_brain_profile profile;
};

static struct ai_brain_slot ai_brain_slots[AI_BRAIN_PROFILE_MAX];

static struct ai_brain_slot *ai_brain_slot_get(struct char_data *mob, int create) {
  int i, oldest = 0;
  for (i = 0; i < AI_BRAIN_PROFILE_MAX; i++) {
    if (ai_brain_slots[i].mob == mob) return &ai_brain_slots[i];
    if (!ai_brain_slots[i].mob && create) {
      ai_brain_slots[i].mob = mob;
      memset(&ai_brain_slots[i].profile, 0, sizeof(ai_brain_slots[i].profile));
      return &ai_brain_slots[i];
    }
    if (ai_brain_slots[i].profile.built_at < ai_brain_slots[oldest].profile.built_at) oldest = i;
  }
  if (!create) return NULL;
  ai_brain_slots[oldest].mob = mob;
  memset(&ai_brain_slots[oldest].profile, 0, sizeof(ai_brain_slots[oldest].profile));
  return &ai_brain_slots[oldest];
}

static int ai_brain_has_kw(const char *txt, const char *const *pool) {
  int i;
  if (!txt || !*txt || !pool) return FALSE;
  for (i = 0; pool[i]; i++) if (strstr(txt, pool[i])) return TRUE;
  return FALSE;
}

static void ai_brain_collect_text(struct char_data *mob, char *buf, size_t bufsz) {
  int i;
  size_t len;
  if (!mob || !buf || bufsz == 0) return;
  buf[0] = '\0';
  if (GET_NAME(mob)) snprintf(buf, bufsz, "%s", GET_NAME(mob));
  len = strlen(buf);
  if (mob->player.short_descr && len + 2 < bufsz) snprintf(buf + len, bufsz - len, " %s", mob->player.short_descr);
  len = strlen(buf);
  if (mob->player.long_descr && len + 2 < bufsz) snprintf(buf + len, bufsz - len, " %s", mob->player.long_descr);
  for (i = 0; i < NUM_WEARS; i++) {
    struct obj_data *eq = GET_EQ(mob, i);
    if (!eq) continue;
    len = strlen(buf);
    if (eq->short_description && len + 2 < bufsz) snprintf(buf + len, bufsz - len, " %s", eq->short_description);
    len = strlen(buf);
    if (eq->name && len + 2 < bufsz) snprintf(buf + len, bufsz - len, " %s", eq->name);
  }
}

static void ai_brain_build_profile(struct char_data *mob, ai_brain_profile *p) {
  static const char *const humanoid_kw[] = {"guard","merchant","teacher","trainer","wizard","priest","banker","advisor","innkeeper","guildmaster","captain","wise",NULL};
  static const char *const speaker_kw[] = {"says","speaks","teacher","merchant","guard","priest","wizard","innkeeper","banker","advisor","guide",NULL};
  static const char *const beast_kw[] = {"wolf","bear","boar","spider","hound","beast","serpent","bat","rat","animal",NULL};
  static const char *const guard_kw[] = {"guard","watch","constable","warden","captain","sheriff",NULL};
  static const char *const merchant_kw[] = {"merchant","trader","shop","vendor","banker",NULL};
  static const char *const inn_kw[] = {"innkeeper","tavern","inn","bartender","hostler",NULL};
  static const char *const trainer_kw[] = {"trainer","teacher","instructor","guildmaster",NULL};
  static const char *const priest_kw[] = {"priest","cleric","temple","holy","chapel",NULL};
  static const char *const wise_speaker_kw[] = {"wise wolf","speaks","guide",NULL};
  static const char *const honor_kw[] = {"honor","lawful","disciplined","order",NULL};
  static const char *const smart_kw[] = {"scholar","wise","wizard","strategic",NULL};
  static const char *const warm_kw[] = {"kind","warm","gentle","friendly",NULL};
  static const char *const formal_kw[] = {"regal","honor","disciplined","formal",NULL};
  static const char *const short_fuse_kw[] = {"angry","snarling","hostile",NULL};
  static const char *const demon_kw[] = {"demon","devil","fiend",NULL};
  static const char *const royal_kw[] = {"royal","lord","lady","regal","commander",NULL};
  static const char *const undead_kw2[] = {"undead","skeleton","lich","ghost",NULL};
  char text[2048];
  unsigned long h;
  int fit = 0;
  if (!mob || !p) return;
  memset(p, 0, sizeof(*p));
  ai_brain_collect_text(mob, text, sizeof(text));
  { const unsigned char *hp=(const unsigned char *)text; h=5381UL; while (*hp) { h=((h<<5)+h) ^ (unsigned long)(*hp++); } }
  p->seed = (uint32_t)(GET_MOB_VNUM(mob) * 2654435761u) ^ (uint32_t)h;
  p->is_humanoid = ai_brain_has_kw(text, humanoid_kw) || !ai_brain_has_kw(text, beast_kw);
  p->can_speak = p->is_humanoid && ai_brain_has_kw(text, speaker_kw);
  if (!p->can_speak && ai_brain_has_kw(text, wise_speaker_kw)) p->can_speak = 1;

  p->imtb_i = p->is_humanoid ? 0.65f : 0.35f;
  p->imtb_m = ai_brain_has_kw(text, honor_kw) ? 0.75f : 0.45f;
  p->imtb_t = ai_brain_has_kw(text, smart_kw) ? 0.72f : 0.40f;
  p->imtb_b = ai_brain_has_kw(text, warm_kw) ? 0.70f : 0.45f;

  p->temperament = ai_brain_has_kw(text, formal_kw) ? 0 : 1;
  p->cognition = (p->imtb_t > 0.65f) ? 2 : 1;
  p->tolerance = ai_brain_has_kw(text, short_fuse_kw) ? 2 : 1;

  if (ai_brain_has_kw(text, demon_kw) && p->archetype_count < AI_ARCH_MAX) p->archetypes[p->archetype_count++] = 1;
  if (ai_brain_has_kw(text, royal_kw) && p->archetype_count < AI_ARCH_MAX) p->archetypes[p->archetype_count++] = 2;
  if (ai_brain_has_kw(text, beast_kw) && p->archetype_count < AI_ARCH_MAX) p->archetypes[p->archetype_count++] = 3;
  if (ai_brain_has_kw(text, undead_kw2) && p->archetype_count < AI_ARCH_MAX) p->archetypes[p->archetype_count++] = 4;

  p->role = ROLE_CIVILIAN;
  if (ai_brain_has_kw(text, guard_kw)) { p->role = ROLE_GUARD; fit += 55; p->caps |= AI_BRAIN_CAP_DIRECTIONS | AI_BRAIN_CAP_LAW; }
  if (ai_brain_has_kw(text, merchant_kw)) { p->role = ROLE_MERCHANT; fit += 50; p->caps |= AI_BRAIN_CAP_TRADE | AI_BRAIN_CAP_DIRECTIONS; }
  if (ai_brain_has_kw(text, inn_kw)) { p->role = ROLE_CIVILIAN; fit += 40; p->caps |= AI_BRAIN_CAP_LODGING | AI_BRAIN_CAP_FOOD | AI_BRAIN_CAP_RUMOR | AI_BRAIN_CAP_DIRECTIONS; }
  if (ai_brain_has_kw(text, trainer_kw)) { fit += 45; p->caps |= AI_BRAIN_CAP_TRAINING; }
  if (ai_brain_has_kw(text, priest_kw)) { fit += 35; p->caps |= AI_BRAIN_CAP_RELIGION | AI_BRAIN_CAP_QUEST; }
  if (ai_brain_has_kw(text, beast_kw) && !p->can_speak) {
    p->role = ROLE_BEAST;
    p->caps = 0;
    fit = 5;
  }
  if (MOB_FLAGGED(mob, MOB_GUILD_MASTER)) { fit += 30; p->caps |= AI_BRAIN_CAP_TRAINING; }

  p->role_fitness = fit;
  p->built_at = time(0);

#if AI_ACTOR_DEBUG
  log("AI_BRAIN_PROFILE mob=%s vnum=%d seed=%u speak=%d role=%d fit=%d caps=0x%x", GET_NAME(mob), GET_MOB_VNUM(mob), p->seed, p->can_speak, p->role, p->role_fitness, (unsigned int)p->caps);
#endif
}

const ai_brain_profile *ai_brain_get(struct char_data *mob) {
  struct ai_brain_slot *sl;
  if (!mob) return NULL;
  sl = ai_brain_slot_get(mob, 0);
  return sl ? &sl->profile : NULL;
}

void ai_brain_ensure(struct char_data *mob) {
  struct ai_brain_slot *sl;
  if (!mob || !IS_NPC(mob) || !MOB_FLAGGED(mob, MOB_AI_ACTOR)) return;
  sl = ai_brain_slot_get(mob, 1);
  if (!sl) return;
  if (sl->profile.built_at == 0) ai_brain_build_profile(mob, &sl->profile);
}

void ai_brain_on_spawn(struct char_data *mob) { ai_brain_ensure(mob); }

void ai_brain_infer_role_and_caps(struct char_data *mob, int *out_role, int *out_fit, uint32_t *out_caps) {
  const ai_brain_profile *p;
  ai_brain_ensure(mob);
  p = ai_brain_get(mob);
  if (out_role) *out_role = p ? p->role : ROLE_UNKNOWN;
  if (out_fit) *out_fit = p ? p->role_fitness : 0;
  if (out_caps) *out_caps = p ? p->caps : 0;
}

int ai_brain_can_speak(const struct char_data *mob) {
  struct ai_brain_slot *sl;
  if (!mob) return FALSE;
  sl = ai_brain_slot_get((struct char_data *)mob, 1);
  if (!sl || sl->profile.built_at == 0) ai_brain_build_profile((struct char_data *)mob, &sl->profile);
  return sl ? (sl->profile.can_speak ? TRUE : FALSE) : FALSE;
}

int ai_brain_voice_style(const struct char_data *mob) {
  struct ai_brain_slot *sl;
  if (!mob) return 0;
  sl = ai_brain_slot_get((struct char_data *)mob, 1);
  if (!sl || sl->profile.built_at == 0) ai_brain_build_profile((struct char_data *)mob, &sl->profile);
  return sl ? sl->profile.temperament : 0;
}

int ai_brain_knows_domain(const struct char_data *mob, int domain) {
  const ai_brain_profile *p = ai_brain_get((struct char_data *)mob);
  if (!p) return FALSE;
  switch (domain) {
    case 1: return (p->caps & (AI_BRAIN_CAP_TRADE | AI_BRAIN_CAP_LODGING | AI_BRAIN_CAP_FOOD | AI_BRAIN_CAP_TRAINING)) ? TRUE : FALSE;
    case 3: return (p->caps & AI_BRAIN_CAP_DIRECTIONS) ? TRUE : FALSE;
    case 4: return (p->caps & AI_BRAIN_CAP_RUMOR) ? TRUE : FALSE;
    case 5: return (p->caps & AI_BRAIN_CAP_QUEST) ? TRUE : FALSE;
    case 6: return (p->caps & AI_BRAIN_CAP_LAW) ? TRUE : FALSE;
    default: return p->can_speak ? TRUE : FALSE;
  }
}

int ai_brain_pick_referral_in_room(struct char_data *mob, struct char_data *player, int domain, struct char_data **out_target) {
  struct char_data *ch;
  if (out_target) *out_target = NULL;
  if (!mob || IN_ROOM(mob) == NOWHERE) return FALSE;
  for (ch = world[IN_ROOM(mob)].people; ch; ch = ch->next_in_room) {
    if (!IS_NPC(ch) || ch == mob || (player && ch == player) || !MOB_FLAGGED(ch, MOB_AI_ACTOR)) continue;
    ai_brain_ensure(ch);
    if (ai_brain_knows_domain(ch, domain)) {
      if (out_target) *out_target = ch;
      return TRUE;
    }
  }
  return FALSE;
}
