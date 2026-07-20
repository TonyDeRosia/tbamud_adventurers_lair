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
#define AI_CLAMP(value, low, high) ((value) < (low) ? (low) : ((value) > (high) ? (high) : (value)))

static const char *social_names[] = { "Silent", "Reserved", "Polite", "Friendly", "Talkative", "Boastful", "Rude", "Hostile", "Extorting", "Preacher", "Gossip" };
static const char *dialogue_names[] = { "Greeting", "Friendly response", "Suspicious response", "Hostile response", "Ambient speech", "Ambient emote", "Farewell" };
const char *ai_social_style_name(int style) { return (style >= 0 && style <= AI_SOCIAL_GOSSIP) ? social_names[style] : "Reserved"; }
const char *ai_dialogue_category_name(int category) { return (category >= 0 && category < AI_DIALOGUE_CATEGORIES) ? dialogue_names[category] : "Unknown"; }

struct mob_ai_config *mob_ai_config_new(void)
{
  struct mob_ai_config *c; int i;
  CREATE(c, struct mob_ai_config, 1); if (!c) return NULL;
  c->mode = MOB_AI_INFERRED; c->movement = AI_MOVE_STATIONARY; c->social = AI_SOCIAL_RESERVED;
  c->greeting_enabled = c->respond_strangers = c->respond_trusted = c->respond_feared = c->respond_hostile = TRUE;
  c->speech_cooldown = 10; c->room_speech_cooldown = 10; c->emote_cooldown = 15;
  c->flee_hp_percent = 20; c->surrender_hp_percent = 10; c->movement_delay = 1;
  for (i = 0; i < AI_ACTOR_PERSONALITIES; i++) c->personality[i] = 50;
  return c;
}
struct mob_ai_config *mob_ai_config_copy(const struct mob_ai_config *from)
{
  struct mob_ai_config *c; int k, i;
  if (!from) return NULL; CREATE(c, struct mob_ai_config, 1); if (!c) return NULL;
  *c = *from;
  for (k=0;k<AI_DIALOGUE_CATEGORIES;k++) for(i=0;i<AI_DIALOGUE_MAX_LINES;i++) c->dialogue[k][i] = from->dialogue[k][i] ? strdup(from->dialogue[k][i]) : NULL;
  return c;
}
void mob_ai_config_free(struct mob_ai_config *c) { int k,i; if (!c) return; for(k=0;k<AI_DIALOGUE_CATEGORIES;k++) for(i=0;i<AI_DIALOGUE_MAX_LINES;i++) free(c->dialogue[k][i]); free(c); }
int mob_ai_dialogue_set(struct mob_ai_config *c, int k, int i, const char *line)
{ char clean[AI_DIALOGUE_LINE_MAX], *p; if (!c || k<0 || k>=AI_DIALOGUE_CATEGORIES || i<0 || i>=AI_DIALOGUE_MAX_LINES || !line) return FALSE; strlcpy(clean,line,sizeof(clean)); for(p=clean;*p;p++) if(*p=='\r'||*p=='\n') *p=' '; skip_spaces(&p); if(!*p) return FALSE; free(c->dialogue[k][i]); c->dialogue[k][i]=strdup(p); if(i>=c->dialogue_count[k]) c->dialogue_count[k]=i+1; return TRUE; }
void mob_ai_config_validate(struct mob_ai_config *c)
{ int i,k; if (!c) return; c->mode=AI_CLAMP(c->mode,MOB_AI_INFERRED,MOB_AI_INFERRED_OVERRIDES); c->role=AI_CLAMP(c->role,ROLE_UNKNOWN,ROLE_BOSS); c->movement=AI_CLAMP(c->movement,AI_MOVE_STATIONARY,AI_MOVE_RETURN_HOME); c->social=AI_CLAMP(c->social,AI_SOCIAL_SILENT,AI_SOCIAL_GOSSIP); c->roam_radius=AI_CLAMP(c->roam_radius,0,100); c->pursuit_distance=AI_CLAMP(c->pursuit_distance,0,100); c->movement_delay=AI_CLAMP(c->movement_delay,1,60); c->speech_cooldown=AI_CLAMP(c->speech_cooldown,AI_SOCIAL_COOLDOWN_MIN,AI_SOCIAL_COOLDOWN_MAX); c->room_speech_cooldown=AI_CLAMP(c->room_speech_cooldown,AI_SOCIAL_COOLDOWN_MIN,AI_SOCIAL_COOLDOWN_MAX); c->emote_cooldown=AI_CLAMP(c->emote_cooldown,AI_SOCIAL_COOLDOWN_MIN,AI_SOCIAL_COOLDOWN_MAX); c->flee_hp_percent=AI_CLAMP(c->flee_hp_percent,0,100); c->surrender_hp_percent=AI_CLAMP(c->surrender_hp_percent,0,100); for(i=0;i<AI_ACTOR_PERSONALITIES;i++) c->personality[i]=AI_CLAMP(c->personality[i],0,100); for(k=0;k<AI_DIALOGUE_CATEGORIES;k++) c->dialogue_count[k]=AI_CLAMP(c->dialogue_count[k],0,AI_DIALOGUE_MAX_LINES); }

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
  if (mob->ai_config) {
    struct mob_ai_config *c = mob->ai_config;
    mob_ai_config_validate(c);
    if (c->mode == MOB_AI_CUSTOM || (c->override_mask & AI_OVERRIDE_ROLE)) mob->ai_prof->role = c->role;
    if (c->mode == MOB_AI_CUSTOM || (c->override_mask & AI_OVERRIDE_MOVEMENT)) mob->ai_prof->movement = c->movement;
    if (c->mode == MOB_AI_CUSTOM || (c->override_mask & AI_OVERRIDE_SOCIAL)) mob->ai_prof->social = c->social;
    mob->ai_prof->home_room_vnum = c->home_room_vnum;
    mob->ai_prof->roam_radius = c->roam_radius;
    mob->ai_prof->flee_hp_percent = c->flee_hp_percent;
    mob->ai_prof->surrender_hp_percent = c->surrender_hp_percent;
    memcpy(mob->ai_prof->personality, c->personality, sizeof(c->personality));
    mob->ai_prof->greeting_enabled=c->greeting_enabled; mob->ai_prof->ambient_speech_enabled=c->ambient_speech_enabled; mob->ai_prof->ambient_emotes_enabled=c->ambient_emotes_enabled; mob->ai_prof->whisper_enabled=c->whisper_enabled;
    mob->ai_prof->respond_strangers=c->respond_strangers; mob->ai_prof->respond_trusted=c->respond_trusted; mob->ai_prof->respond_feared=c->respond_feared; mob->ai_prof->respond_hostile=c->respond_hostile;
    mob->ai_prof->talk_cooldown_secs = c->speech_cooldown; mob->ai_prof->room_talk_cooldown_secs=c->room_speech_cooldown; mob->ai_prof->emote_cooldown_secs=c->emote_cooldown;
    memcpy(mob->ai_prof->dialogue_count,c->dialogue_count,sizeof(c->dialogue_count));
    { int dk,di; for(dk=0;dk<AI_DIALOGUE_CATEGORIES;dk++) for(di=0;di<AI_DIALOGUE_MAX_LINES;di++) mob->ai_prof->dialogue[dk][di]=c->dialogue[dk][di]; }
    mob->ai_prof->assist_enabled = c->assist_enabled;
    mob->ai_prof->call_help_enabled = c->call_help_enabled;
    mob->ai_prof->hunt_enabled = c->hunt_enabled;
  }
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
    if (IS_NPC(it) && GET_MOB_VNUM(it) == vnum) {
      mob_rnum rnum = GET_MOB_RNUM(it);
      if (rnum != NOBODY) {
        mob_ai_config_free(it->ai_config);
        it->ai_config = mob_ai_config_copy(mob_proto[rnum].ai_config);
      }
      ai_actor_sync_profile(it);
    }
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
  mob_ai_config_free(mob->ai_config);
  mob->ai_config = NULL;
}

static const char *ai_pick_dialogue(struct char_data *mob, int category, int emote)
{
  int i, hash; const char *line;
  if (!mob || !mob->ai_prof || category < 0 || category >= AI_DIALOGUE_CATEGORIES) return NULL;
  for (i=0;i<mob->ai_prof->dialogue_count[category];i++) {
    line=mob->ai_prof->dialogue[category][i]; if (!line) continue;
    hash=(int)strlen(line); if (hash == (emote ? mob->ai_state->last_emote_hash : mob->ai_state->last_speech_hash)) continue;
    if (emote) mob->ai_state->last_emote_hash=hash; else mob->ai_state->last_speech_hash=hash;
    return line;
  } return NULL;
}
static void ai_social_say(struct char_data *mob, int category, const char *fallback, time_t now)
{ const char *line=ai_pick_dialogue(mob,category,FALSE); if (!line) line=fallback; if (!line) return; do_say(mob,(char *)line,0,0); mob->ai_state->last_spoke=now; mob->ai_state->last_room_spoke=now; mob->ai_state->last_room_vnum_spoke=world[IN_ROOM(mob)].number; }
static void ai_social_emote(struct char_data *mob, time_t now)
{ const char *line=ai_pick_dialogue(mob,AI_DIALOGUE_AMBIENT_EMOTE,TRUE); char buf[MAX_STRING_LENGTH]; if(!line)return; snprintf(buf,sizeof(buf),"$n %s.",line); act(buf,TRUE,mob,0,0,TO_ROOM); mob->ai_state->last_emote_time=now; }

int ai_actor_tick(struct char_data *mob, time_t now)
{
  struct npc_social_profile p;
  enum npc_priority prio;
  if (!mob || !IS_NPC(mob) || !MOB_FLAGGED(mob, MOB_AI_ACTOR) || !CONFIG_AI_ACTOR_ENABLED || !ai_actor_brain_enabled()) return FALSE;
  if (!npc_ai_is_humanoid_social_candidate(mob)) return FALSE;
  if (!mob->ai_state || !mob->ai_prof) ai_actor_init(mob);

  npc_ai_build_profile(mob, &p);
  if (mob->ai_prof->social == AI_SOCIAL_SILENT) return TRUE;
  if (mob->ai_prof->ambient_emotes_enabled && now - mob->ai_state->last_emote_time >= mob->ai_prof->emote_cooldown_secs && rand_number(1,100) <= 10 + mob->ai_prof->personality[AI_TRAIT_CURIOSITY] / 5 - mob->ai_prof->personality[AI_TRAIT_DISCIPLINE] / 10) { ai_social_emote(mob,now); return TRUE; }
  if (mob->ai_prof->ambient_speech_enabled && now - mob->ai_state->last_spoke >= mob->ai_prof->talk_cooldown_secs && now - mob->ai_state->last_room_spoke >= mob->ai_prof->room_talk_cooldown_secs && rand_number(1,100) <= 5 + mob->ai_prof->personality[AI_TRAIT_SOCIABILITY]/3 - mob->ai_prof->personality[AI_TRAIT_DISCIPLINE]/8) { ai_social_say(mob,AI_DIALOGUE_AMBIENT_SPEECH, mob->ai_prof->social == AI_SOCIAL_BOASTFUL ? "My work speaks for itself." : NULL,now); return TRUE; }
  prio = npc_ai_choose_priority(mob, &p, now);
  if (prio == NPC_PRIO_ENGAGE || prio == NPC_PRIO_WARN) npc_ai_handle_room_danger(mob, FIGHTING(mob), now);
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
  if (!mob->ai_state || !mob->ai_prof) ai_actor_init(mob);
  if (mob->ai_prof->social == AI_SOCIAL_SILENT) return;
  if (type == AI_EVENT_PLAYER_ENTER && mob->ai_prof->greeting_enabled && now - mob->ai_state->last_spoke >= mob->ai_prof->talk_cooldown_secs && rand_number(1,100) <= 5 + mob->ai_prof->personality[AI_TRAIT_SOCIABILITY]/3) { ai_social_say(mob, AI_DIALOGUE_GREETING, mob->ai_prof->social == AI_SOCIAL_FRIENDLY ? "Welcome." : NULL, now); return; }
  if (type == AI_EVENT_PLAYER_SAY && mob->ai_prof->respond_strangers && now - mob->ai_state->last_spoke >= mob->ai_prof->talk_cooldown_secs) { int category = mob->ai_prof->personality[AI_TRAIT_SUSPICION] > 60 ? AI_DIALOGUE_SUSPICIOUS : AI_DIALOGUE_FRIENDLY; if (mob->ai_prof->social == AI_SOCIAL_HOSTILE || mob->ai_prof->personality[AI_TRAIT_AGGRESSION] > 70) category=AI_DIALOGUE_HOSTILE; ai_social_say(mob,category, NULL,now); if (mob->ai_state->last_spoke == now) return; }
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
