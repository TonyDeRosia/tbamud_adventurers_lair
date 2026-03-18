#include "conf.h"
#include "sysdep.h"

#include "structs.h"
#include "utils.h"
#include "db.h"
#include "comm.h"
#include "handler.h"
#include "interpreter.h"
#include "ai_actor.h"
#include "ai_actor_brain.h"
#include "npc_social_ai.h"

ACMD(do_say);

static void ai_actor_sync_profile(struct char_data *mob)
{
  struct npc_social_profile p;
  if (!mob) return;
  if (!mob->ai_prof) CREATE(mob->ai_prof, struct ai_actor_profile, 1);
  if (!mob->ai_prof) return;
  memset(mob->ai_prof, 0, sizeof(*mob->ai_prof));
  npc_ai_build_profile(mob, &p);
  switch (p.role) {
    case NPC_ROLE_GUARD: mob->ai_prof->role = ROLE_GUARD; break;
    case NPC_ROLE_MERCHANT: mob->ai_prof->role = ROLE_MERCHANT; break;
    case NPC_ROLE_BANDIT: mob->ai_prof->role = ROLE_BANDIT; break;
    case NPC_ROLE_CIVILIAN: mob->ai_prof->role = ROLE_CIVILIAN; break;
    default: mob->ai_prof->role = ROLE_UNKNOWN; break;
  }
  mob->ai_prof->aggression = (p.temperament == NPC_TEMP_AGGRESSIVE) ? AGG_OPPORTUNISTIC :
                             (p.temperament == NPC_TEMP_TIMID ? AGG_PEACEFUL : AGG_RETALIATE);
  mob->ai_prof->social = (p.social_style == NPC_SOCIAL_EXTROVERT) ? SOC_TALKATIVE :
                         (p.social_style == NPC_SOCIAL_INTROVERT ? SOC_SILENT : SOC_WARNING);
  mob->ai_prof->talk_cooldown_secs = 10;
  mob->ai_prof->initialized = TRUE;
}

uint32_t ai_actor_compute_signature(struct char_data *mob)
{
  uint32_t sig = 0;
  if (!mob || !IS_NPC(mob)) return 0;
  sig = (uint32_t)GET_MOB_VNUM(mob);
  sig ^= (uint32_t)MOB_FLAGS(mob)[0] * 2654435761u;
  return sig;
}

void ai_actor_build_profile(struct char_data *mob, int full_reset)
{
  (void)full_reset;
  ai_actor_sync_profile(mob);
}

void ai_actor_rebuild_profile(struct char_data *mob) { ai_actor_sync_profile(mob); }
void ai_actor_refresh_profile(struct char_data *mob, int force) { (void)force; ai_actor_sync_profile(mob); }

void ai_actor_refresh_live_mobs_by_vnum(mob_vnum vnum)
{
  struct char_data *it;
  for (it = character_list; it; it = it->next)
    if (IS_NPC(it) && GET_MOB_VNUM(it) == vnum)
      ai_actor_sync_profile(it);
}

void ai_actor_init(struct char_data *mob)
{
  if (!mob || !IS_NPC(mob)) return;
  if (!mob->ai_state) CREATE(mob->ai_state, struct ai_actor_state, 1);
  if (mob->ai_state) memset(mob->ai_state, 0, sizeof(*mob->ai_state));
  ai_actor_sync_profile(mob);
}

void ai_actor_free(struct char_data *mob)
{
  if (!mob) return;
  if (mob->ai_prof) { free(mob->ai_prof); mob->ai_prof = NULL; }
  if (mob->ai_state) { free(mob->ai_state); mob->ai_state = NULL; }
}

int ai_actor_tick(struct char_data *mob, time_t now)
{
  struct npc_social_profile p;
  enum npc_priority prio;
  if (!mob || !IS_NPC(mob) || !CONFIG_AI_ACTOR_ENABLED || !ai_actor_brain_enabled()) return FALSE;
  if (!npc_ai_is_humanoid_social_candidate(mob)) return FALSE;
  if (!mob->ai_state || !mob->ai_prof) ai_actor_init(mob);

  npc_ai_build_profile(mob, &p);
  prio = npc_ai_choose_priority(mob, &p, now);
  if (prio == NPC_PRIO_ENGAGE || prio == NPC_PRIO_WARN)
    npc_ai_handle_room_danger(mob, FIGHTING(mob), now);
  npc_ai_maybe_do_ambient_action(mob, &p, now);
  return TRUE;
}

void ai_actor_record_damage(struct char_data *mob, struct char_data *actor, int dam)
{
  if (!npc_ai_is_humanoid_social_candidate(mob) || !actor || dam <= 0) return;
  npc_ai_update_memory(mob, actor, -2, 4, 8, time(0));
}

void ai_actor_record_help(struct char_data *mob, struct char_data *actor, int amount)
{
  if (!npc_ai_is_humanoid_social_candidate(mob) || !actor || amount <= 0) return;
  npc_ai_update_memory(mob, actor, 3, 0, -2, time(0));
}

void ai_actor_record_crime(struct char_data *mob, struct char_data *criminal, int flags)
{
  (void)flags;
  if (!npc_ai_is_humanoid_social_candidate(mob) || !criminal) return;
  npc_ai_update_memory(mob, criminal, -4, 8, 4, time(0));
}

void ai_actor_record_room_crime(struct char_data *witness, struct char_data *criminal, int flags)
{
  struct char_data *mob;
  if (witness && npc_ai_is_humanoid_social_candidate(witness)) {
    ai_actor_record_crime(witness, criminal, flags);
    return;
  }
  if (!criminal || IN_ROOM(criminal) == NOWHERE) return;
  for (mob = world[IN_ROOM(criminal)].people; mob; mob = mob->next_in_room)
    if (npc_ai_is_humanoid_social_candidate(mob))
      ai_actor_record_crime(mob, criminal, flags);
}

enum ai_actor_persona get_actor_persona(struct char_data *ch)
{
  (void)ch;
  return AI_PERSONA_NEUTRAL;
}

void ai_actor_on_room_event(struct char_data *mob, enum ai_event_type type, struct char_data *actor, const char *text)
{
  time_t now = time(0);
  if (!npc_ai_is_humanoid_social_candidate(mob) || !actor || IS_NPC(actor)) return;
  switch (type) {
    case AI_EVENT_PLAYER_ENTER: npc_ai_handle_player_enter(mob, actor, now); break;
    case AI_EVENT_PLAYER_LEAVE: npc_ai_handle_player_leave(mob, actor, now); break;
    case AI_EVENT_PLAYER_SAY: npc_ai_handle_speech_event(mob, actor, text, now); break;
    case AI_EVENT_COMBAT_START: npc_ai_handle_room_danger(mob, actor, now); break;
    default: break;
  }
}

void ai_actor_event_enter(struct char_data *actor, room_rnum room)
{
  struct char_data *mob;
  if (!actor || IS_NPC(actor) || room == NOWHERE) return;
  for (mob = world[room].people; mob; mob = mob->next_in_room)
    ai_actor_on_room_event(mob, AI_EVENT_PLAYER_ENTER, actor, NULL);
}

void ai_actor_event_leave(struct char_data *actor, room_rnum room)
{
  struct char_data *mob;
  if (!actor || IS_NPC(actor) || room == NOWHERE) return;
  for (mob = world[room].people; mob; mob = mob->next_in_room)
    ai_actor_on_room_event(mob, AI_EVENT_PLAYER_LEAVE, actor, NULL);
}

void ai_actor_event_say(struct char_data *actor, const char *msg)
{
  struct char_data *mob;
  if (!actor || IS_NPC(actor) || IN_ROOM(actor) == NOWHERE) return;
  for (mob = world[IN_ROOM(actor)].people; mob; mob = mob->next_in_room)
    ai_actor_on_room_event(mob, AI_EVENT_PLAYER_SAY, actor, msg);
}

void ai_actor_event_emote(struct char_data *actor, const char *msg)
{
  ai_actor_event_say(actor, msg);
}

void ai_actor_event_combat_start(struct char_data *attacker, struct char_data *victim)
{
  struct char_data *mob;
  room_rnum room;
  room = (attacker && IN_ROOM(attacker) != NOWHERE) ? IN_ROOM(attacker) : (victim ? IN_ROOM(victim) : NOWHERE);
  if (room == NOWHERE) return;
  for (mob = world[room].people; mob; mob = mob->next_in_room)
    if (npc_ai_is_humanoid_social_candidate(mob))
      npc_ai_handle_room_danger(mob, attacker, time(0));
}

void ai_actor_event_corpse(struct char_data *dead, room_rnum room)
{
  (void)dead;
  (void)room;
}

void ai_actor_event_drop(struct char_data *actor, struct obj_data *obj)
{
  (void)actor;
  (void)obj;
}

void ai_actor_event_give(struct char_data *actor, struct char_data *to, struct obj_data *obj)
{
  (void)actor;
  (void)to;
  (void)obj;
}

void ai_actor_schedule_reaction_speech(struct char_data *mob, struct char_data *target, const char *msg)
{
  (void)target;
  if (!npc_ai_is_humanoid_social_candidate(mob) || !msg || !*msg) return;
  do_say(mob, (char *)msg, 0, 0);
}
