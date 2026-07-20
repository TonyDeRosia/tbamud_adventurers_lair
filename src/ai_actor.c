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
#include "fight.h"
#include "act.h"

ACMD(do_say);
#define AI_CLAMP(value, low, high) ((value) < (low) ? (low) : ((value) > (high) ? (high) : (value)))

static void ai_actor_threat_event(struct char_data *mob, struct char_data *actor, int base, int confidence, time_t now);
static struct ai_actor_memory_entry *ai_threat_memory(struct char_data *mob, long id);

static const char *social_names[] = { "Silent", "Reserved", "Polite", "Friendly", "Talkative", "Boastful", "Rude", "Hostile", "Extorting", "Preacher", "Gossip" };
static const char *dialogue_names[] = { "Greeting", "Friendly response", "Suspicious response", "Hostile response", "Ambient speech", "Ambient emote", "Farewell", "Warning", "Challenge", "Threat", "Call for help", "Fear" };
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
  c->notice_entry=c->notice_departure=c->notice_speech=c->notice_whispers=c->notice_emotes=c->notice_combat=c->notice_self_attack=c->notice_ally_attack=c->notice_corpses=c->notice_gifts=c->notice_crimes=TRUE;
  c->notice_drops=FALSE; c->hearing_sensitivity=c->observation_sensitivity=c->suspicion_threshold=50; c->recognition_confidence=60;
  c->memory_enabled=TRUE; c->memory_max_actors=AI_MEM_MAX; c->memory_ordinary_duration=60; c->memory_important_duration=240; c->trust_gain=c->trust_loss=c->fear_gain=c->hostility_gain=c->familiarity_gain=100; c->fear_decay=c->hostility_decay=c->forgiveness=10; c->familiarity_decay=5;
  c->remember_attacks=c->remember_assistance=c->remember_crimes=c->remember_gifts=c->remember_insults=c->remember_conversations=c->remember_threats=c->remember_last_room=c->remember_deaths=TRUE;
  c->threat_enabled[AI_THREAT_OBSERVE]=c->threat_enabled[AI_THREAT_WARN]=c->threat_enabled[AI_THREAT_CHALLENGE]=c->threat_enabled[AI_THREAT_CALL_HELP]=c->threat_enabled[AI_THREAT_ASSIST]=c->threat_enabled[AI_THREAT_ATTACK]=c->threat_enabled[AI_THREAT_FLEE]=TRUE;
  c->threat_cooldown=10; c->calm_reset_time=120; c->repeated_event_window=90;
  c->combat_style=AI_COMBAT_BALANCED; c->combat_enabled=TRUE; c->may_initiate=TRUE; c->may_assist=TRUE; c->may_call_help=TRUE; c->may_flee=TRUE;
  c->protect_trusted=c->protect_group=TRUE; c->avoid_incapacitated=c->retaliate_self=c->retaliate_ally=c->retaliate_hostile=c->switch_targets=TRUE;
  c->assist_severity=60; c->target_switch_threshold=25; c->max_allies=4; c->max_responders=2; c->combat_cooldown=10;
  c->threat_step_count=5; { static const int defaults[5][3]={{AI_THREAT_OBSERVE,10,10},{AI_THREAT_WARN,25,30},{AI_THREAT_CHALLENGE,45,30},{AI_THREAT_CALL_HELP,60,60},{AI_THREAT_ATTACK,80,0}}; for(i=0;i<5;i++){c->threat_steps[i].type=defaults[i][0];c->threat_steps[i].minimum_severity=defaults[i][1];c->threat_steps[i].cooldown=defaults[i][2];c->threat_steps[i].max_repetitions=1;} }
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
int mob_ai_dialogue_delete(struct mob_ai_config *c, int k, int i)
{ int n; if (!c || k < 0 || k >= AI_DIALOGUE_CATEGORIES || i < 0 || i >= c->dialogue_count[k]) return FALSE; free(c->dialogue[k][i]); for (n=i; n+1<c->dialogue_count[k]; n++) c->dialogue[k][n]=c->dialogue[k][n+1]; c->dialogue[k][--c->dialogue_count[k]]=NULL; return TRUE; }
int mob_ai_dialogue_move(struct mob_ai_config *c, int k, int from, int to)
{ char *line; if (!c || k < 0 || k >= AI_DIALOGUE_CATEGORIES || from < 0 || from >= c->dialogue_count[k] || to < 0 || to >= c->dialogue_count[k]) return FALSE; line=c->dialogue[k][from]; if(from<to) for(;from<to;from++) c->dialogue[k][from]=c->dialogue[k][from+1]; else for(;from>to;from--) c->dialogue[k][from]=c->dialogue[k][from-1]; c->dialogue[k][to]=line; return TRUE; }
int ai_actor_personality_response_modifier(const int p[AI_ACTOR_PERSONALITIES])
{ static const int weight[AI_ACTOR_PERSONALITIES] = { -2, 1, 3, 1, -1, 1, -1, 2, 1, 1, -3, 2 }; int i, score=0; if (!p) return 0; for (i=0;i<AI_ACTOR_PERSONALITIES;i++) score += (p[i]-50)*weight[i]; return AI_CLAMP(score/10, -35, 35); }
void mob_ai_config_validate(struct mob_ai_config *c)
{ int i,k; if (!c) return; c->combat_style=AI_CLAMP(c->combat_style,AI_COMBAT_PASSIVE,AI_COMBAT_BOSS); c->assist_severity=AI_CLAMP(c->assist_severity,0,100); c->target_switch_threshold=AI_CLAMP(c->target_switch_threshold,0,100); c->max_allies=AI_CLAMP(c->max_allies,0,10); c->max_responders=AI_CLAMP(c->max_responders,0,10); c->combat_cooldown=AI_CLAMP(c->combat_cooldown,1,300); for(i=0;i<AI_TARGET_WEIGHTS;i++) c->target_weight[i]=AI_CLAMP(c->target_weight[i],-100,100); c->mode=AI_CLAMP(c->mode,MOB_AI_INFERRED,MOB_AI_INFERRED_OVERRIDES); c->role=AI_CLAMP(c->role,ROLE_UNKNOWN,ROLE_BOSS); c->movement=AI_CLAMP(c->movement,AI_MOVE_STATIONARY,AI_MOVE_RETURN_HOME); c->social=AI_CLAMP(c->social,AI_SOCIAL_SILENT,AI_SOCIAL_GOSSIP); c->roam_radius=AI_CLAMP(c->roam_radius,0,100); c->pursuit_distance=AI_CLAMP(c->pursuit_distance,0,100); c->movement_delay=AI_CLAMP(c->movement_delay,1,60); c->speech_cooldown=AI_CLAMP(c->speech_cooldown,AI_SOCIAL_COOLDOWN_MIN,AI_SOCIAL_COOLDOWN_MAX); c->room_speech_cooldown=AI_CLAMP(c->room_speech_cooldown,AI_SOCIAL_COOLDOWN_MIN,AI_SOCIAL_COOLDOWN_MAX); c->emote_cooldown=AI_CLAMP(c->emote_cooldown,AI_SOCIAL_COOLDOWN_MIN,AI_SOCIAL_COOLDOWN_MAX); c->flee_hp_percent=AI_CLAMP(c->flee_hp_percent,0,100); c->surrender_hp_percent=AI_CLAMP(c->surrender_hp_percent,0,100); for(i=0;i<AI_ACTOR_PERSONALITIES;i++) c->personality[i]=AI_CLAMP(c->personality[i],0,100); for(k=0;k<AI_DIALOGUE_CATEGORIES;k++) c->dialogue_count[k]=AI_CLAMP(c->dialogue_count[k],0,AI_DIALOGUE_MAX_LINES); c->hearing_sensitivity=AI_CLAMP(c->hearing_sensitivity,0,100); c->observation_sensitivity=AI_CLAMP(c->observation_sensitivity,0,100); c->suspicion_threshold=AI_CLAMP(c->suspicion_threshold,0,100); c->recognition_confidence=AI_CLAMP(c->recognition_confidence,0,100); c->memory_max_actors=AI_CLAMP(c->memory_max_actors,1,AI_MEM_MAX); c->memory_ordinary_duration=AI_CLAMP(c->memory_ordinary_duration,1,1440); c->memory_important_duration=AI_CLAMP(c->memory_important_duration,c->memory_ordinary_duration,10080); c->trust_gain=AI_CLAMP(c->trust_gain,0,200); c->trust_loss=AI_CLAMP(c->trust_loss,0,200); c->fear_gain=AI_CLAMP(c->fear_gain,0,200); c->hostility_gain=AI_CLAMP(c->hostility_gain,0,200); c->familiarity_gain=AI_CLAMP(c->familiarity_gain,0,200); c->fear_decay=AI_CLAMP(c->fear_decay,0,100); c->hostility_decay=AI_CLAMP(c->hostility_decay,0,100); c->familiarity_decay=AI_CLAMP(c->familiarity_decay,0,100); c->forgiveness=AI_CLAMP(c->forgiveness,0,100); c->threat_cooldown=AI_CLAMP(c->threat_cooldown,1,300); c->calm_reset_time=AI_CLAMP(c->calm_reset_time,1,3600); c->repeated_event_window=AI_CLAMP(c->repeated_event_window,1,600); c->threat_step_count=AI_CLAMP(c->threat_step_count,0,AI_THREAT_STEP_MAX); for(i=0;i<AI_THREAT_RESPONSE_MAX;i++) c->threat_enabled[i]=!!c->threat_enabled[i]; for(i=0;i<c->threat_step_count;i++){ c->threat_steps[i].minimum_severity=AI_CLAMP(c->threat_steps[i].minimum_severity,0,100); c->threat_steps[i].cooldown=AI_CLAMP(c->threat_steps[i].cooldown,0,300); c->threat_steps[i].max_repetitions=AI_CLAMP(c->threat_steps[i].max_repetitions,1,10); c->threat_steps[i].advance_on_failure=!!c->threat_steps[i].advance_on_failure; } }

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
    mob->ai_prof->assist_enabled = c->may_assist;
    mob->ai_prof->call_help_enabled = c->may_call_help;
    mob->ai_prof->combat_style=c->combat_style; mob->ai_prof->combat_enabled=c->combat_enabled; mob->ai_prof->may_initiate=c->may_initiate; mob->ai_prof->may_assist=c->may_assist; mob->ai_prof->may_call_help=c->may_call_help; mob->ai_prof->may_flee=c->may_flee;
    mob->ai_prof->protect_trusted=c->protect_trusted; mob->ai_prof->protect_group=c->protect_group; mob->ai_prof->protect_same_role=c->protect_same_role; mob->ai_prof->protect_same_prototype=c->protect_same_prototype;
    mob->ai_prof->avoid_incapacitated=c->avoid_incapacitated; mob->ai_prof->retaliate_self=c->retaliate_self; mob->ai_prof->retaliate_ally=c->retaliate_ally; mob->ai_prof->retaliate_hostile=c->retaliate_hostile; mob->ai_prof->switch_targets=c->switch_targets;
    mob->ai_prof->assist_severity=c->assist_severity; mob->ai_prof->target_switch_threshold=c->target_switch_threshold; mob->ai_prof->max_allies=c->max_allies; mob->ai_prof->max_responders=c->max_responders; mob->ai_prof->combat_cooldown=c->combat_cooldown; memcpy(mob->ai_prof->target_weight,c->target_weight,sizeof(c->target_weight));
    mob->ai_prof->hunt_enabled = c->hunt_enabled;
    memcpy(&mob->ai_prof->notice_entry, &c->notice_entry, sizeof(c->notice_entry));
    memcpy(&mob->ai_prof->memory_enabled, &c->memory_enabled, sizeof(c->memory_enabled));
    memcpy(mob->ai_prof->threat_enabled, c->threat_enabled, sizeof(c->threat_enabled)); mob->ai_prof->threat_cooldown=c->threat_cooldown; mob->ai_prof->calm_reset_time=c->calm_reset_time; mob->ai_prof->repeated_event_window=c->repeated_event_window; mob->ai_prof->threat_step_count=c->threat_step_count; memcpy(mob->ai_prof->threat_steps,c->threat_steps,sizeof(c->threat_steps));
  }
  mob->ai_prof->initialized = TRUE;
}

int ai_threat_response_available(int type) { return type >= AI_THREAT_OBSERVE && type <= AI_THREAT_FLEE && type != AI_THREAT_ASSIST && type != AI_THREAT_FOLLOW && type != AI_THREAT_ARREST; }
int ai_threat_response_targeted(int type) { return type == AI_THREAT_WARN || type == AI_THREAT_CHALLENGE || type == AI_THREAT_ATTACK || type == AI_THREAT_FOLLOW || type == AI_THREAT_ARREST; }
int ai_threat_step_valid(const struct ai_threat_step *s, const int enabled[AI_THREAT_RESPONSE_MAX]) { return s && s->type >= 0 && s->type < AI_THREAT_RESPONSE_MAX && ai_threat_response_available(s->type) && enabled && enabled[s->type] && s->minimum_severity >= 0 && s->minimum_severity <= 100 && s->cooldown >= 0 && s->max_repetitions > 0 && !(s->cooldown == 0 && s->max_repetitions > 1); }
int ai_threat_severity(int base, const struct ai_actor_memory_entry *m, const int p[AI_ACTOR_PERSONALITIES], int confidence, int injured) { int v=base; if(m) v += m->hostility/4 + m->fear/6; if(p){v+=(p[AI_TRAIT_AGGRESSION]-50)/5;v+=(p[AI_TRAIT_LOYALTY]-50)/8;v+=(p[AI_TRAIT_PRIDE]-50)/10;v-=(p[AI_TRAIT_PATIENCE]-50)/8;v-=(p[AI_TRAIT_COMPASSION]-50)/8;} if(injured && (!m || m->hostility < 30))v-=15; return AI_CLAMP(v*AI_CLAMP(confidence,0,100)/100,0,100); }
int ai_threat_choose_step(const struct ai_actor_profile *p, const struct ai_actor_memory_entry *m, time_t now) { int i; if(!p||!m)return -1; if(m->threat_last_event && now-m->threat_last_event>=p->calm_reset_time) return 0; for(i=0;i<p->threat_step_count;i++){const struct ai_threat_step*s=&p->threat_steps[i];if(ai_threat_step_valid(s,p->threat_enabled)&&m->threat_severity>=s->minimum_severity&&(!m->threat_last_action||now-m->threat_last_action>=s->cooldown)&&m->threat_repetitions<s->max_repetitions)return i;} return -1; }

enum ai_relationship ai_actor_relationship(const struct ai_actor_memory_entry *m)
{ if (!m) return AI_REL_UNKNOWN; if (m->hostility >= 30 && m->fear >= 30) return AI_REL_HOSTILE_FEARED; if (m->trust >= 30 && m->fear >= 30) return AI_REL_TRUSTED_FEARED; if (m->hostility >= 30) return AI_REL_HOSTILE; if (m->fear >= 30) return AI_REL_FEARED; if (m->trust >= 20 || m->familiarity >= 20) return m->trust >= 30 ? AI_REL_TRUSTED : AI_REL_FAMILIAR; return AI_REL_UNKNOWN; }

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

int ai_actor_is_local_ally(struct char_data *mob, struct char_data *other, const char **reason)
{
  struct ai_actor_memory_entry *m;
  if (reason) *reason = "unrelated";
  if (!mob || !other || mob == other || IN_ROOM(mob) != IN_ROOM(other)) return FALSE;
  if (mob->master == other || other->master == mob || (mob->master && mob->master == other->master)) { if(reason)*reason="master/follower"; return TRUE; }
  if (mob->ai_prof && mob->ai_prof->protect_same_prototype && IS_NPC(mob) && IS_NPC(other) && GET_MOB_RNUM(mob) == GET_MOB_RNUM(other)) { if(reason)*reason="same prototype"; return TRUE; }
  if (mob->ai_prof && mob->ai_prof->protect_same_role && other->ai_prof && mob->ai_prof->role == other->ai_prof->role) { if(reason)*reason="same role"; return TRUE; }
  m=ai_threat_memory(mob, GET_IDNUM(other));
  if (mob->ai_prof && mob->ai_prof->protect_trusted && m && ai_actor_relationship(m) == AI_REL_TRUSTED) { if(reason)*reason="trusted memory"; return TRUE; }
  return FALSE;
}

int ai_actor_target_score(struct char_data *mob, struct char_data *candidate)
{
  struct ai_actor_memory_entry *m; int score=0, hp;
  if (!mob || !candidate || mob == candidate || IN_ROOM(mob) == NOWHERE || IN_ROOM(mob) != IN_ROOM(candidate) || DEAD(candidate) || !CAN_SEE(mob,candidate) || ROOM_FLAGGED(IN_ROOM(mob), ROOM_PEACEFUL)) return -100000;
  if (mob->ai_prof->avoid_incapacitated && GET_POS(candidate) < POS_FIGHTING) return -100000;
  m=ai_threat_memory(mob,GET_IDNUM(candidate)); hp=GET_MAX_HIT(candidate) ? GET_HIT(candidate)*100/GET_MAX_HIT(candidate) : 100;
  if (FIGHTING(candidate)==mob) score+=mob->ai_prof->target_weight[AI_TARGET_CURRENT_ATTACKER];
  if (m && ai_actor_relationship(m)>=AI_REL_HOSTILE) score+=mob->ai_prof->target_weight[AI_TARGET_KNOWN_HOSTILE]+m->threat_severity;
  score+=mob->ai_prof->target_weight[AI_TARGET_LOW_HEALTH]*(100-hp)/100;
  score+=IS_NPC(candidate)?mob->ai_prof->target_weight[AI_TARGET_NPC]:mob->ai_prof->target_weight[AI_TARGET_PLAYER];
  if (mob->ai_state && mob->ai_state->last_selected_target_idnum==GET_IDNUM(candidate)) score+=mob->ai_prof->target_weight[AI_TARGET_PREVIOUS];
  if (mob->ai_prof->combat_style==AI_COMBAT_OPPORTUNIST) score+=(100-hp)/4;
  return score;
}

int ai_actor_should_flee(struct char_data *mob)
{
  int hp, threshold;
  if (!mob || !mob->ai_prof || !FIGHTING(mob) || !mob->ai_prof->may_flee || MOB_FLAGGED(mob,MOB_SENTINEL) || !GET_MAX_HIT(mob)) return FALSE;
  threshold=mob->ai_prof->flee_hp_percent; if (!threshold) return FALSE;
  if (mob->ai_prof->combat_style==AI_COMBAT_COWARDLY) threshold+=20;
  if (mob->ai_prof->combat_style==AI_COMBAT_FANATICAL || mob->ai_prof->combat_style==AI_COMBAT_BOSS) threshold-=15;
  threshold += (mob->ai_prof->personality[AI_TRAIT_SUSPICION]-mob->ai_prof->personality[AI_TRAIT_BRAVERY])/4;
  hp=GET_HIT(mob)*100/GET_MAX_HIT(mob); return hp <= AI_CLAMP(threshold,0,100);
}

static int ai_actor_combat_tick(struct char_data *mob, time_t now)
{
  struct char_data *it,*best=NULL; int best_score=-100000, current_score, score;
  if (!mob->ai_prof->combat_enabled || !FIGHTING(mob) || now-mob->ai_state->last_combat_action < mob->ai_prof->combat_cooldown) return FALSE;
  if (ai_actor_should_flee(mob)) { do_flee(mob,"",0,0); mob->ai_state->last_combat_action=now; return TRUE; }
  current_score=ai_actor_target_score(mob,FIGHTING(mob));
  if (!mob->ai_prof->switch_targets) return FALSE;
  for(it=world[IN_ROOM(mob)].people;it;it=it->next_in_room) { score=ai_actor_target_score(mob,it); if(score>best_score) { best_score=score; best=it; } }
  if (best && best != FIGHTING(mob) && best_score >= current_score + mob->ai_prof->target_switch_threshold) { stop_fighting(mob); hit(mob,best,0); mob->ai_state->last_selected_target_idnum=GET_IDNUM(best); mob->ai_state->last_combat_action=mob->ai_state->last_target_switch=now; return TRUE; }
  return FALSE;
}

int ai_actor_tick(struct char_data *mob, time_t now)
{
  struct npc_social_profile p;
  enum npc_priority prio;
  int mi;
  if (!mob || !IS_NPC(mob) || !MOB_FLAGGED(mob, MOB_AI_ACTOR) || !CONFIG_AI_ACTOR_ENABLED || !ai_actor_brain_enabled()) return FALSE;
  if (!npc_ai_is_humanoid_social_candidate(mob)) return FALSE;
  if (!mob->ai_state || !mob->ai_prof) ai_actor_init(mob);

  /* Per-actor maintenance: no global scan and no persistent player pointers. */
  if (mob->ai_prof->memory_enabled) for (mi=0;mi<mob->ai_state->mem_count;mi++) {
    struct ai_actor_memory_entry *m=&mob->ai_state->mem[mi];
    if (m->last_update && now > m->last_update) { m->fear=AI_CLAMP(m->fear-mob->ai_prof->fear_decay,0,100); m->hostility=AI_CLAMP(m->hostility-mob->ai_prof->hostility_decay-mob->ai_prof->forgiveness,0,100); m->familiarity=AI_CLAMP(m->familiarity-mob->ai_prof->familiarity_decay,0,100); m->room_confidence=AI_CLAMP(m->room_confidence-1,0,100); m->last_update=now; }
  }

  npc_ai_build_profile(mob, &p);
  if (ai_actor_combat_tick(mob, now)) return TRUE;
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
  ai_actor_threat_event(mob, actor, 90, 100, time(0));
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
  ai_actor_threat_event(mob, criminal, 65, 100, time(0));
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

static struct ai_actor_memory_entry *ai_threat_memory(struct char_data *mob, long id) { int i; if(!mob||!mob->ai_state)return NULL; for(i=0;i<mob->ai_state->mem_count;i++) if(mob->ai_state->mem[i].idnum==id)return &mob->ai_state->mem[i]; return NULL; }
static void ai_actor_threat_event(struct char_data *mob, struct char_data *actor, int base, int confidence, time_t now) {
  struct ai_actor_memory_entry *m; int step; const struct ai_threat_step *s;
  if(!mob||!actor||!mob->ai_prof||!mob->ai_state)return;
  m=ai_threat_memory(mob,GET_IDNUM(actor)); if(!m)return;
  m->identity_confidence=AI_CLAMP(confidence,0,100); m->threat_severity=ai_threat_severity(base,m,mob->ai_prof->personality,m->identity_confidence,FALSE); m->threat_last_event=now;
  step=ai_threat_choose_step(mob->ai_prof,m,now); if(step<0)return; s=&mob->ai_prof->threat_steps[step];
  if(ai_threat_response_targeted(s->type) && m->identity_confidence < mob->ai_prof->recognition_confidence)return;
  if(s->type==AI_THREAT_OBSERVE || s->type==AI_THREAT_IGNORE) { /* record only */ }
  else if(s->type==AI_THREAT_WARN) { if(mob->ai_prof->social!=AI_SOCIAL_SILENT) ai_social_say(mob,AI_DIALOGUE_WARNING,"You should leave now.",now); m->threat_last_warning=now; }
  else if(s->type==AI_THREAT_CHALLENGE) { int threaty=m->threat_severity>=60 && (mob->ai_prof->personality[AI_TRAIT_AGGRESSION]>55 || mob->ai_prof->personality[AI_TRAIT_PRIDE]>55); if(mob->ai_prof->social!=AI_SOCIAL_SILENT) ai_social_say(mob,threaty?AI_DIALOGUE_THREAT:AI_DIALOGUE_CHALLENGE,threaty?"Do not test me.":"Explain yourself.",now); m->threat_last_challenge=now; }
  else if(s->type==AI_THREAT_CALL_HELP) { if(!m->threat_help_called && mob->ai_prof->social!=AI_SOCIAL_SILENT){ ai_social_say(mob,AI_DIALOGUE_CALL_HELP,"Help! There is danger here!",now);m->threat_help_called=TRUE;} }
  else if(s->type==AI_THREAT_FLEE) { if(mob->ai_prof->social!=AI_SOCIAL_SILENT) ai_social_say(mob,AI_DIALOGUE_FEAR,"I cannot face this!",now); }
  else if(s->type==AI_THREAT_ATTACK && IN_ROOM(mob)==IN_ROOM(actor) && !FIGHTING(mob)) hit(mob,actor,0);
  m->threat_step=step; m->threat_repetitions++; m->threat_last_action=now;
}

static int ai_social_response_enabled(struct char_data *mob, struct char_data *actor)
{
  struct ai_actor_memory_entry *m = NULL;
  int i;
  if (!mob || !mob->ai_prof || !actor) return FALSE;
  for (i = 0; mob->ai_state && i < mob->ai_state->mem_count; i++)
    if (mob->ai_state->mem[i].idnum == GET_IDNUM(actor)) { m = &mob->ai_state->mem[i]; break; }
  if (!m) return mob->ai_prof->respond_strangers;
  if (ai_actor_relationship(m) == AI_REL_HOSTILE || ai_actor_relationship(m) == AI_REL_HOSTILE_FEARED) return mob->ai_prof->respond_hostile;
  if (ai_actor_relationship(m) == AI_REL_FEARED || ai_actor_relationship(m) == AI_REL_TRUSTED_FEARED) return mob->ai_prof->respond_feared;
  if (ai_actor_relationship(m) == AI_REL_TRUSTED) return mob->ai_prof->respond_trusted;
  return mob->ai_prof->respond_strangers;
}

void ai_actor_on_room_event(struct char_data *mob, enum ai_event_type type, struct char_data *actor, const char *text)
{
  time_t now = time(0);
  if (!npc_ai_is_humanoid_social_candidate(mob) || !actor || IS_NPC(actor)) return;
  if (!mob->ai_state || !mob->ai_prof) ai_actor_init(mob);
  if ((type == AI_EVENT_PLAYER_ENTER && !mob->ai_prof->notice_entry) || (type == AI_EVENT_PLAYER_LEAVE && !mob->ai_prof->notice_departure) || (type == AI_EVENT_PLAYER_SAY && !mob->ai_prof->notice_speech) || (type == AI_EVENT_COMBAT_START && !mob->ai_prof->notice_combat)) return;
  if (type == AI_EVENT_PLAYER_ENTER) ai_actor_threat_event(mob,actor,10,100,now);
  else if (type == AI_EVENT_PLAYER_SAY && text && (strstr(text,"kill") || strstr(text,"threat"))) ai_actor_threat_event(mob,actor,45,100,now);
  else if (type == AI_EVENT_COMBAT_START) ai_actor_threat_event(mob,actor,55,80,now);
  if (mob->ai_prof->social == AI_SOCIAL_SILENT) return;
  if (type == AI_EVENT_PLAYER_ENTER && mob->ai_prof->greeting_enabled && ai_social_response_enabled(mob, actor) && now - mob->ai_state->last_spoke >= mob->ai_prof->talk_cooldown_secs && rand_number(1,100) <= AI_CLAMP(30 + ai_actor_personality_response_modifier(mob->ai_prof->personality), 1, 95)) { ai_social_say(mob, AI_DIALOGUE_GREETING, mob->ai_prof->social == AI_SOCIAL_FRIENDLY ? "Welcome." : NULL, now); return; }
  if (type == AI_EVENT_PLAYER_SAY) {
    int category;
    if (!ai_social_response_enabled(mob, actor)) return;
    if (now - mob->ai_state->last_spoke < mob->ai_prof->talk_cooldown_secs) return;
    category = mob->ai_prof->personality[AI_TRAIT_SUSPICION] > 60 ? AI_DIALOGUE_SUSPICIOUS : AI_DIALOGUE_FRIENDLY;
    if (mob->ai_prof->social == AI_SOCIAL_HOSTILE || mob->ai_prof->personality[AI_TRAIT_AGGRESSION] > 70) category=AI_DIALOGUE_HOSTILE;
    ai_social_say(mob,category, NULL,now);
    return; /* Configured response toggles also gate the legacy responder. */
  }
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

void ai_actor_event_whisper(struct char_data *actor, struct char_data *target, const char *msg)
{
  if (!actor || !target || !target->ai_prof || !target->ai_prof->whisper_enabled || !target->ai_prof->notice_whispers) return;
  ai_actor_on_room_event(target, AI_EVENT_PLAYER_SAY, actor, msg);
}

void ai_actor_event_emote(struct char_data *actor, const char *msg)
{
  struct char_data *mob; if (!actor || IN_ROOM(actor)==NOWHERE) return; for(mob=world[IN_ROOM(actor)].people;mob;mob=mob->next_in_room) if (mob->ai_prof && mob->ai_prof->notice_emotes) ai_actor_on_room_event(mob, AI_EVENT_PLAYER_SAY, actor, msg);
}

void ai_actor_event_combat_start(struct char_data *attacker, struct char_data *victim)
{
  struct char_data *mob;
  room_rnum room;
  room = (attacker && IN_ROOM(attacker) != NOWHERE) ? IN_ROOM(attacker) : (victim ? IN_ROOM(victim) : NOWHERE);
  if (room == NOWHERE) return;
  for (mob = world[room].people; mob; mob = mob->next_in_room) {
    const char *why;
    if (npc_ai_is_humanoid_social_candidate(mob) && mob->ai_prof && mob->ai_prof->notice_combat)
      npc_ai_handle_room_danger(mob, attacker, time(0));
    if (mob != victim && mob->ai_prof && mob->ai_prof->combat_enabled && mob->ai_prof->may_assist && !FIGHTING(mob) &&
        mob->ai_prof->notice_ally_attack && ai_actor_is_local_ally(mob, victim, &why) &&
        !ROOM_FLAGGED(room, ROOM_PEACEFUL) && CAN_SEE(mob, attacker)) {
      int responders=0; struct char_data *scan;
      for(scan=world[room].people;scan;scan=scan->next_in_room) if(FIGHTING(scan)==attacker) responders++;
      if (responders < mob->ai_prof->max_responders) hit(mob, attacker, 0); /* Standard combat entry preserves triggers and legality checks. */
    }
  }
}

void ai_actor_event_corpse(struct char_data *dead, room_rnum room)
{
  struct char_data *mob; (void)dead; if(room==NOWHERE)return; for(mob=world[room].people;mob;mob=mob->next_in_room) if(npc_ai_is_humanoid_social_candidate(mob)&&mob->ai_prof&&mob->ai_prof->notice_corpses) mob->ai_state->last_action_time=time(0);
}

void ai_actor_event_drop(struct char_data *actor, struct obj_data *obj)
{
  struct char_data *mob; (void)obj; if(!actor||IN_ROOM(actor)==NOWHERE)return; for(mob=world[IN_ROOM(actor)].people;mob;mob=mob->next_in_room) if(npc_ai_is_humanoid_social_candidate(mob)&&mob->ai_prof&&mob->ai_prof->notice_drops) mob->ai_state->last_action_time=time(0);
}

void ai_actor_event_give(struct char_data *actor, struct char_data *to, struct obj_data *obj)
{
  (void)obj; if (!actor || !to || !npc_ai_is_humanoid_social_candidate(to) || !to->ai_prof || !to->ai_prof->notice_gifts || !to->ai_prof->memory_enabled || !to->ai_prof->remember_gifts) return; npc_ai_update_memory(to,actor,to->ai_prof->trust_gain/25,0,0,time(0));
}

void ai_actor_event_attack(struct char_data *attacker, struct char_data *victim, int damage) { if (!attacker || !victim || !npc_ai_is_humanoid_social_candidate(victim) || !victim->ai_prof || !victim->ai_prof->notice_self_attack || !victim->ai_prof->remember_attacks) return; ai_actor_record_damage(victim,attacker,damage); }
void ai_actor_event_crime(struct char_data *criminal, int flags) { if(!criminal||IN_ROOM(criminal)==NOWHERE)return; { struct char_data *m; for(m=world[IN_ROOM(criminal)].people;m;m=m->next_in_room) if(npc_ai_is_humanoid_social_candidate(m)&&m->ai_prof&&m->ai_prof->notice_crimes&&m->ai_prof->remember_crimes) ai_actor_record_crime(m,criminal,flags); } }

void ai_actor_schedule_reaction_speech(struct char_data *mob, struct char_data *target, const char *msg)
{
  (void)target;
  if (!npc_ai_is_humanoid_social_candidate(mob) || !msg || !*msg) return;
  do_say(mob, (char *)msg, 0, 0);
}
