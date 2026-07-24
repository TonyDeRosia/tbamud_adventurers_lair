#include "conf.h"
#include "sysdep.h"

#include "structs.h"
#include "utils.h"
#include "comm.h"
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
  if (!viewer || !mob) return;
  send_to_char(viewer, "AI brain uses bounded idle decisions.\\r\\n");
  send_to_char(viewer, "Brain toggle: %s\\r\\n", ai_actor_brain_enabled() ? "ON" : "OFF");
}
