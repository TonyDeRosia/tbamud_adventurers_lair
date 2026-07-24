#include "conf.h"
#include "sysdep.h"

#include "structs.h"
#include "utils.h"
#include "comm.h"
#include "db.h"
#include "ai_actor_brain.h"
#include "ai_actor.h"

static int g_ai_actor_brain_enabled = TRUE;

struct ai_brain_profile { int can_speak; };
static struct ai_brain_profile g_brain_profile = { 1 };

const ai_brain_profile *ai_brain_get(struct char_data *mob) { (void)mob; return &g_brain_profile; }
int ai_brain_can_speak(const struct char_data *mob) { return mob && mob->ai_prof && mob->ai_prof->communication == AI_COMM_SPEAK; }

void ai_actor_brain_init(struct char_data *mob) { (void)mob; }
void ai_actor_brain_free(struct char_data *mob) { (void)mob; }
int ai_actor_brain_think(struct char_data *mob, time_t now) {
  /* The coordinator deliberately returns only a bounded idle action; execution
   * stays in ai_actor.c through established movement/social gateways. */
  if (!mob || !mob->ai_state || !mob->ai_prof || FIGHTING(mob)) return AI_IDLE_NONE;
  if (mob->ai_prof->movement == AI_MOVE_RANDOM && now >= mob->ai_state->next_random_move)
    return AI_IDLE_MOVE_RANDOM;
  if (mob->ai_prof->communication == AI_COMM_VOCALIZE) return AI_IDLE_VOCALIZE;
  if (ai_brain_can_speak(mob) && mob->ai_prof->ambient_speech_enabled) return AI_IDLE_SPEAK;
  return AI_IDLE_OBSERVE;
}
void ai_actor_brain_on_enter(struct char_data *mob, struct char_data *actor) { (void)mob; (void)actor; }
void ai_actor_brain_on_leave(struct char_data *mob, struct char_data *actor) { (void)mob; (void)actor; }
void ai_actor_brain_on_say(struct char_data *mob, struct char_data *actor, const char *msg) { (void)mob; (void)actor; (void)msg; }
void ai_actor_brain_on_emote(struct char_data *mob, struct char_data *actor, const char *msg) { (void)mob; (void)actor; (void)msg; }
void ai_actor_brain_on_combat_start(struct char_data *mob, struct char_data *attacker, struct char_data *victim) { (void)mob; (void)attacker; (void)victim; }
void ai_actor_brain_on_attacked(struct char_data *mob, struct char_data *attacker, int dam) { (void)mob; (void)attacker; (void)dam; }
void ai_actor_brain_on_corpse(struct char_data *mob, struct char_data *dead) { (void)mob; (void)dead; }
void ai_actor_brain_on_drop(struct char_data *mob, struct char_data *actor, struct obj_data *obj) { (void)mob; (void)actor; (void)obj; }
void ai_actor_brain_on_give(struct char_data *mob, struct char_data *actor, struct obj_data *obj, struct char_data *to) { (void)mob; (void)actor; (void)obj; (void)to; }

int ai_actor_brain_enabled(void) { return g_ai_actor_brain_enabled; }
void ai_actor_brain_set_enabled(int enabled) { g_ai_actor_brain_enabled = (enabled ? TRUE : FALSE); }

void ai_actor_brain_show_state(struct char_data *viewer, struct char_data *mob)
{
  const struct ai_actor_state *s;
  if (!viewer || !mob) return;
  /* AI brain uses bounded idle decisions; only current enum state is exposed. */
  s = mob->ai_state;
  send_to_char(viewer, "\r\nAI Actor State\r\n--------------\r\nIdentity\r\n  Mob vnum: %d  Current room: %d  Archetype: %s\r\nCapabilities\r\n  Communication: %s  Memory: %s  Assistance: %s\r\n", GET_MOB_VNUM(mob), IN_ROOM(mob) == NOWHERE ? 0 : world[IN_ROOM(mob)].number, mob->ai_prof ? ai_actor_archetype_name(mob->ai_prof->archetype) : "Unavailable", mob->ai_prof ? ai_actor_communication_name(mob->ai_prof->communication) : "Unavailable", mob->ai_prof ? ai_actor_memory_style_name(mob->ai_prof->memory_style) : "Unavailable", mob->ai_prof ? ai_actor_assistance_style_name(mob->ai_prof->assistance_style) : "Unavailable");
  if (!s) { send_to_char(viewer, "Runtime state unavailable.\r\n"); return; }
  send_to_char(viewer, "Decision\r\n  Current decision: %s  Last action: %s  Owner: %s\r\nMovement\r\n  Current state: %s  Cooldown remaining: %ld seconds  Last result: %s\r\n  Last blocked reason: %s\r\nCommunication\r\n  Capability: %s  Vocalization lines: %d  Cooldown remaining: %ld seconds\r\n  Last delivery: %s\r\nState\r\n  Fighting: %s  Brain toggle: %s\r\n", s->last_tick_result == AI_TICK_ACTED ? "Acted" : s->last_tick_result == AI_TICK_EXCLUSIVE ? "Exclusive owner" : "Idle", s->last_idle_action == AI_IDLE_MOVE_RANDOM ? "Random movement" : s->last_idle_action == AI_IDLE_VOCALIZE ? "Creature vocalization" : s->last_idle_action == AI_IDLE_SPEAK ? "Dialogue" : s->last_idle_action == AI_IDLE_EMOTE ? "Emote" : s->last_idle_action == AI_IDLE_OBSERVE ? "Observe" : "None", FIGHTING(mob) ? "Combat" : s->schedule_wander_suppressed ? "Schedule / Patrol" : "Idle", mob->ai_prof ? ai_actor_config_movement_name(mob->ai_prof->movement) : "Unavailable", (long)MAX(0, s->next_random_move - time(0)), s->last_move_result ? "Moved" : "No movement", s->last_blocked_reason[0] ? s->last_blocked_reason : "None", mob->ai_prof ? ai_actor_communication_name(mob->ai_prof->communication) : "Unavailable", mob->ai_config ? mob->ai_config->vocalization_count : 0, (long)MAX(0, s->next_vocalization - time(0)), s->last_vocalization_result ? "Delivered" : "None / blocked", FIGHTING(mob) ? "Yes" : "No", ai_actor_brain_enabled() ? "ON" : "OFF");
}
