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
#include "graph.h"
#include "act.h"

ACMD(do_say);
#define AI_CLAMP(value, low, high) ((value) < (low) ? (low) : ((value) > (high) ? (high) : (value)))

static void ai_preview_add(char *out, size_t size, const char *fmt, ...);
static void ai_actor_threat_event(struct char_data *mob, struct char_data *actor, int base, int confidence, time_t now);
static struct ai_actor_memory_entry *ai_threat_memory(struct char_data *mob, long id);
static struct ai_patrol_route *ai_schedule_route(struct mob_ai_config *c, int id);
static void ai_actor_dispatch_help(struct ai_help_event *, struct char_data *, struct char_data *, struct char_data *, int, time_t);

static struct ai_help_event ai_help_events[AI_HELP_EVENT_MAX];
static unsigned long ai_next_help_event_id = 1;
static const char *ai_target_names[AI_TARGET_WEIGHTS] = { "Current attacker", "Attacker of trusted actor", "Attacker of group member", "Known hostile", "Lowest health / closest to death", "Player character", "NPC", "Previous target" };

static void ai_compat_add(char *out, size_t size, const char *fmt, ...)
{
  va_list args; size_t used = strlen(out);
  if (used >= size) return;
  va_start(args, fmt); vsnprintf(out + used, size - used, fmt, args); va_end(args);
}

int ai_actor_compatibility_warning_count(const struct char_data *mob)
{
  const struct mob_ai_config *c;
  int warnings = 0, i, j;
  if (!mob || !MOB_FLAGGED(mob, MOB_AI_ACTOR)) return 0;
  c = mob->ai_config;
  if (!c) return 0;
  if (MOB_FLAGGED(mob, MOB_SCAVENGER)) warnings++;
  if (c->movement == AI_MOVE_RANDOM) warnings++;
  if (MOB_FLAGGED(mob, MOB_SENTINEL) && c->schedule_enabled) warnings++;
  if (MOB_FLAGGED(mob, MOB_SENTINEL) && MOB_FLAGGED(mob, MOB_WIMPY)) warnings++;
  if (c->hunt_enabled) warnings++;
  if (c->mode == MOB_AI_INFERRED_OVERRIDES && !c->override_mask) warnings++;
  for (i = 0; i < c->patrol_count; i++)
    for (j = 1; j < c->patrols[i].waypoint_count; j++)
      if (real_room(c->patrols[i].waypoints[j - 1].room_vnum) != NOWHERE && real_room(c->patrols[i].waypoints[j].room_vnum) != NOWHERE &&
          world[real_room(c->patrols[i].waypoints[j - 1].room_vnum)].zone != world[real_room(c->patrols[i].waypoints[j].room_vnum)].zone && MOB_FLAGGED(mob, MOB_STAY_ZONE)) warnings++;
  return warnings;
}

void ai_actor_compatibility_report(const struct char_data *mob, char *out, size_t size, int detailed)
{
  const struct mob_ai_config *c = mob ? mob->ai_config : NULL;
  int i;
  if (!out || !size)
    return;
  out[0] = '\0';
  ai_compat_add(out, size, "\r\n                    AI Actor Compatibility\r\n\r\nRuntime Ownership\r\n  IMPLEMENTED AI Actor pulse: eligible ticks normally claim the pulse, even idle.\r\n  INFO Legacy mobile tail: scavenging, wandering, aggression, MEMORY, rebellion, HELPER, and hunting do not run on a claimed pulse.\r\n  INFO Special procedure: runs before AI Actor; a handled procedure prevents both AI Actor and the legacy tail for that pulse.\r\n  INFO Scripts: may act through event/trigger paths outside the normal pulse.\r\n");
  if (!mob || !MOB_FLAGGED(mob, MOB_AI_ACTOR)) { ai_compat_add(out,size,"\r\nINFO: AI_ACTOR is not enabled for this mobile.\r\n"); return; }
  ai_compat_add(out,size,"\r\nProfile Mode Runtime Notes\r\n  %s: role, movement, and social use mode/override logic; not every field has identical override-mask coverage.\r\n", c ? (c->mode == MOB_AI_CUSTOM ? "Custom" : c->mode == MOB_AI_INFERRED_OVERRIDES ? "Overrides" : "Inferred") : "Inferred (no stored config)");
  ai_compat_add(out,size,"  INFO Social eligibility currently checks NPC, AI_ACTOR, and a valid room; race, body type, intelligence, and speech capability are not checked. ai_brain_can_speak is always true.\r\n");
  if (!c) { ai_compat_add(out,size,"\r\nINFO: Inferred configuration is used; no stored configuration is present.\r\n"); return; }
  if (MOB_FLAGGED(mob,MOB_SENTINEL)) ai_compat_add(out,size,"\r\nHard Restriction\r\n  SENTINEL: blocks legacy wandering and causes AI schedule movement failure; it does not universally block scripts, specials, or forced movement.\r\n");
  if (MOB_FLAGGED(mob,MOB_STAY_ZONE)) ai_compat_add(out,size,"  STAY_ZONE: restricts checked movement steps to the current zone; it does not provide route analysis.\r\n");
  if (MOB_FLAGGED(mob,MOB_MEMORY)) ai_compat_add(out,size,"\r\nSeparate Legacy System\r\n  MEMORY: legacy attacker list, normally bypassed on AI-owned pulses; it is not synchronized with AI relationship memory.\r\n");
  else if (c->memory_enabled) ai_compat_add(out,size,"\r\nINFO: AI Actor memory is enabled; the legacy MEMORY flag is not required.\r\n");
  if (MOB_FLAGGED(mob,MOB_HELPER)) ai_compat_add(out,size,"  HELPER: legacy fight assistance, normally bypassed on AI-owned pulses; it does not enable AI assistance.\r\n");
  if (MOB_FLAGGED(mob,MOB_SCAVENGER)) ai_compat_add(out,size,"  WARNING: SCAVENGER is legacy-only and normally bypassed; no AI Actor scavenging implementation was found.\r\n");
  if (MOB_FLAGGED(mob,MOB_WIMPY)) ai_compat_add(out,size,"  WIMPY: inference input for combat/flee defaults; legacy flee remains separate.\r\n");
  if (MOB_FLAGGED(mob,MOB_AGGRESSIVE)||MOB_FLAGGED(mob,MOB_AGGR_GOOD)||MOB_FLAGGED(mob,MOB_AGGR_EVIL)||MOB_FLAGGED(mob,MOB_AGGR_NEUTRAL)) ai_compat_add(out,size,"  AGGRESSIVE/AGGR_*: inference inputs; legacy immediate aggression is a separate tail behavior normally bypassed on AI-owned pulses.\r\n");
  if (c->movement == AI_MOVE_RANDOM) ai_compat_add(out,size,"\r\nWARNING: Random movement is stored/configurable, but no active AI random movement tick was found.\r\n");
  if (MOB_FLAGGED(mob,MOB_SENTINEL)&&c->schedule_enabled) ai_compat_add(out,size,"WARNING: SENTINEL causes AI schedule travel to fail.\r\n");
  if (MOB_FLAGGED(mob,MOB_SENTINEL)&&MOB_FLAGGED(mob,MOB_WIMPY)) ai_compat_add(out,size,"WARNING: WIMPY favors flee behavior while SENTINEL constrains autonomous paths; current behavior is split across systems.\r\n");
  if (c->hunt_enabled) ai_compat_add(out,size,"WARNING: hunt_enabled is compiled/stored, but no AI tick hunt path was found.\r\n");
  if (AFF_FLAGGED(mob,AFF_NOTRACK)) ai_compat_add(out,size,"INFO: NO_TRACK has no audited AI movement consumer.\r\n");
  if (c->mode==MOB_AI_INFERRED_OVERRIDES&&!c->override_mask) ai_compat_add(out,size,"WARNING: Overrides mode has no supported override bits active.\r\n");
  for(i=0;i<c->patrol_count;i++) if(c->patrols[i].waypoint_count>1) ai_compat_add(out,size,"INFO: Patrol destinations are moved only when directly adjacent; no pathfinding is provided.\r\n");
  if (detailed) ai_compat_add(out,size,"\r\nPARTIAL: ai_actor_brain_think/callbacks are stubbed or no-op. UNSUPPORTED: target weights retain their existing runtime limitations.\r\nH) Help  Q) Return\r\n");
}

static const char *social_names[] = { "Silent", "Reserved", "Polite", "Friendly", "Talkative", "Boastful", "Rude", "Hostile", "Extorting", "Preacher", "Gossip" };
static const char *dialogue_names[] = { "Greeting", "Friendly response", "Suspicious response", "Hostile response", "Ambient speech", "Ambient emote", "Farewell", "Warning", "Challenge", "Threat", "Call for help", "Fear", "Schedule departure", "Schedule arrival", "Work", "Guard", "Patrol", "Sleep", "Wake", "Schedule failure" };
const char *ai_social_style_name(int style) { return (style >= 0 && style <= AI_SOCIAL_GOSSIP) ? social_names[style] : "Reserved"; }
const char *ai_dialogue_category_name(int category) { return (category >= 0 && category < AI_DIALOGUE_CATEGORIES) ? dialogue_names[category] : "Unknown"; }
const char *ai_actor_config_role_name(int role)
{
  static const char *names[] = { "Generic", "Guard", "Merchant", "Bandit",
    "Beast", "Undead", "Spirit", "Cultist", "Civilian", "Boss" };
  return role >= ROLE_UNKNOWN && role <= ROLE_BOSS ? names[role] : "Unknown";
}
const char *ai_actor_config_movement_name(int movement)
{
  static const char *names[] = { "Stationary", "Random", "Patrol", "Scheduled",
    "Guard room", "Return home" };
  return movement >= AI_MOVE_STATIONARY && movement <= AI_MOVE_RETURN_HOME ? names[movement] : "Unknown";
}
const char *ai_actor_config_role_summary(int role)
{
  static const char *summaries[] = {
    "No distinct role defaults.", "Uses guard-oriented inferred defaults.",
    "Uses merchant-oriented inferred defaults.", "Uses bandit-oriented inferred defaults.",
    "Uses beast-oriented inferred defaults.", "Uses undead-oriented inferred defaults.",
    "Uses spirit-oriented inferred defaults.", "Uses cultist-oriented inferred defaults.",
    "Uses civilian-oriented inferred defaults.", "Uses boss-oriented inferred defaults."
  };
  return role >= ROLE_UNKNOWN && role <= ROLE_BOSS ? summaries[role] : "Unknown or corrupted role value.";
}
const char *ai_actor_config_movement_summary(int movement)
{
  static const char *summaries[] = {
    "Remains in its room unless moved by combat, scripts, or schedules.", "Uses normal random mobile movement.",
    "Follows an authored patrol route.", "Uses authored schedule entries to choose destinations and activities.",
    "Returns to the configured guard room.", "Returns to the configured home room."
  };
  return movement >= AI_MOVE_STATIONARY && movement <= AI_MOVE_RETURN_HOME ? summaries[movement] : "Unknown or corrupted movement value.";
}

struct mob_ai_config *mob_ai_config_new(void)
{
  struct mob_ai_config *c; int i;
  CREATE(c, struct mob_ai_config, 1); if (!c) return NULL;
  c->mode = MOB_AI_INFERRED; c->movement = AI_MOVE_STATIONARY; c->social = AI_SOCIAL_RESERVED;
  c->greeting_enabled = c->respond_strangers = c->respond_trusted = c->respond_feared = c->respond_hostile = TRUE;
  c->speech_cooldown = 10; c->room_speech_cooldown = 10; c->emote_cooldown = 15;
  c->schedule_enabled=FALSE; c->resume_after_interrupt=TRUE; c->default_failure_policy=AI_FAILURE_WAIT_RETRY; c->next_schedule_id=c->next_patrol_id=1;
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
struct mob_ai_config *mob_ai_config_copy(const struct mob_ai_config *from) {
  struct mob_ai_config *c;
  int k, i;
  if (!from)
    return NULL;
  CREATE(c, struct mob_ai_config, 1);
  if (!c)
    return NULL;
  *c = *from;
  for (k = 0; k < AI_DIALOGUE_CATEGORIES; k++)
    for (i = 0; i < AI_DIALOGUE_MAX_LINES; i++)
      c->dialogue[k][i] =
          from->dialogue[k][i] ? strdup(from->dialogue[k][i]) : NULL;
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

/* Fewer matching days is a more-specific calendar rule. */
static int ai_schedule_specificity(int mask) { int n=0; while(mask){ n+=mask&1; mask>>=1; } return 7-n; }
int ai_schedule_time_matches(int start,int end,int hour) { if(start<0||start>23||end<0||end>23||hour<0||hour>23)return FALSE; return start==end || (start<end ? hour>=start&&hour<end : hour>=start||hour<end); }
int ai_schedule_day_matches(int mask,int day) { return day>=0 && day<7 && (mask & (1<<day)); }
int ai_schedule_select(const struct mob_ai_config *c,int day,int hour) { int i,best=-1; if(!c||!c->schedule_enabled)return -1; for(i=0;i<c->schedule_count;i++){const struct ai_schedule_entry *e=&c->schedules[i],*b; int ew,bw; if(!e->enabled||!ai_schedule_day_matches(e->day_mask,day)||!ai_schedule_time_matches(e->start_hour,e->end_hour,hour))continue; if(best<0){best=i;continue;} b=&c->schedules[best]; ew=(e->end_hour-e->start_hour+24)%24; bw=(b->end_hour-b->start_hour+24)%24; if(e->priority>b->priority || (e->priority==b->priority && (ai_schedule_specificity(e->day_mask)>ai_schedule_specificity(b->day_mask) || (ai_schedule_specificity(e->day_mask)==ai_schedule_specificity(b->day_mask) && ew<bw))))best=i; } return best; }
int ai_schedule_entries_overlap(const struct ai_schedule_entry *a,const struct ai_schedule_entry *b){int h,d;if(!a||!b)return FALSE;for(d=0;d<7;d++)if(ai_schedule_day_matches(a->day_mask,d)&&ai_schedule_day_matches(b->day_mask,d))for(h=0;h<24;h++)if(ai_schedule_time_matches(a->start_hour,a->end_hour,h)&&ai_schedule_time_matches(b->start_hour,b->end_hour,h))return TRUE;return FALSE;}
int ai_patrol_advance(const struct ai_patrol_route *r,int i,int dir,int *nd){int n;if(!r||r->waypoint_count<1)return -1;n=r->waypoint_count;if(r->loop_mode==AI_PATROL_ONCE&&i>=n-1)return -1;if(r->loop_mode==AI_PATROL_PINGPONG){if(n==1){if(nd)*nd=1;return 0;}if(dir>=0&&i>=n-1){dir=-1;}else if(dir<0&&i<=0){dir=1;}if(nd)*nd=dir;return i+dir;}if(nd)*nd=1;return (i+1)%n;}
int ai_schedule_add(struct mob_ai_config*c,const struct ai_schedule_entry*e){struct ai_schedule_entry z;int i,used;if(!c||c->schedule_count>=AI_SCHEDULE_MAX)return FALSE;memset(&z,0,sizeof(z));if(e)z=*e;do { used=FALSE;if(!z.id)z.id=MAX(1,c->next_schedule_id++);for(i=0;i<c->schedule_count;i++)if(c->schedules[i].id==z.id){used=TRUE;z.id=0;break;} } while(used); if(c->next_schedule_id<=z.id)c->next_schedule_id=z.id+1;z.day_mask=z.day_mask?z.day_mask:AI_DAY_MASK_ALL;z.max_attempts=z.max_attempts?z.max_attempts:3;c->schedules[c->schedule_count++]=z;return TRUE;}
int ai_schedule_delete(struct mob_ai_config*c,int i){if(!c||i<0||i>=c->schedule_count)return FALSE;memmove(&c->schedules[i],&c->schedules[i+1],sizeof(c->schedules[0])*(c->schedule_count-i-1));c->schedule_count--;return TRUE;}
int ai_schedule_move(struct mob_ai_config*c,int a,int b){struct ai_schedule_entry z;if(!c||a<0||b<0||a>=c->schedule_count||b>=c->schedule_count)return FALSE;z=c->schedules[a];if(a<b)memmove(&c->schedules[a],&c->schedules[a+1],sizeof(z)*(b-a));else memmove(&c->schedules[b+1],&c->schedules[b],sizeof(z)*(a-b));c->schedules[b]=z;return TRUE;}
int ai_schedule_duplicate(struct mob_ai_config*c,int i){struct ai_schedule_entry z;if(!c||i<0||i>=c->schedule_count)return FALSE;z=c->schedules[i];z.id=0;return ai_schedule_add(c,&z);}
int ai_patrol_add(struct mob_ai_config*c,const struct ai_patrol_route*r){struct ai_patrol_route z;int i,used;if(!c||c->patrol_count>=AI_PATROL_MAX)return FALSE;memset(&z,0,sizeof(z));if(r)z=*r;do { used=FALSE;if(!z.id)z.id=MAX(1,c->next_patrol_id++);for(i=0;i<c->patrol_count;i++)if(c->patrols[i].id==z.id){used=TRUE;z.id=0;break;} } while(used); if(c->next_patrol_id<=z.id)c->next_patrol_id=z.id+1;c->patrols[c->patrol_count++]=z;return TRUE;}
int ai_patrol_delete(struct mob_ai_config*c,int i){int j;if(!c||i<0||i>=c->patrol_count)return FALSE;for(j=0;j<c->schedule_count;j++)if(c->schedules[j].enabled&&c->schedules[j].route_id==c->patrols[i].id)return FALSE;memmove(&c->patrols[i],&c->patrols[i+1],sizeof(c->patrols[0])*(c->patrol_count-i-1));c->patrol_count--;return TRUE;}
int ai_patrol_move(struct mob_ai_config*c,int a,int b){struct ai_patrol_route z;if(!c||a<0||b<0||a>=c->patrol_count||b>=c->patrol_count)return FALSE;z=c->patrols[a];if(a<b)memmove(&c->patrols[a],&c->patrols[a+1],sizeof(z)*(b-a));else memmove(&c->patrols[b+1],&c->patrols[b],sizeof(z)*(a-b));c->patrols[b]=z;return TRUE;}
int ai_patrol_duplicate(struct mob_ai_config*c,int i){struct ai_patrol_route z;if(!c||i<0||i>=c->patrol_count)return FALSE;z=c->patrols[i];z.id=0;return ai_patrol_add(c,&z);}
int ai_patrol_waypoint_add(struct ai_patrol_route*r,const struct ai_patrol_waypoint*w){struct ai_patrol_waypoint z;if(!r||r->waypoint_count>=AI_PATROL_WAYPOINT_MAX)return FALSE;memset(&z,0,sizeof(z));if(w)z=*w;r->waypoints[r->waypoint_count++]=z;return TRUE;}
int ai_patrol_waypoint_delete(struct ai_patrol_route*r,int i){if(!r||i<0||i>=r->waypoint_count)return FALSE;memmove(&r->waypoints[i],&r->waypoints[i+1],sizeof(r->waypoints[0])*(r->waypoint_count-i-1));r->waypoint_count--;return TRUE;}
int ai_patrol_waypoint_duplicate(struct ai_patrol_route*r,int i){struct ai_patrol_waypoint z;if(!r||i<0||i>=r->waypoint_count)return FALSE;z=r->waypoints[i];return ai_patrol_waypoint_add(r,&z);}
int ai_patrol_waypoint_move(struct ai_patrol_route*r,int a,int b){struct ai_patrol_waypoint z;if(!r||a<0||b<0||a>=r->waypoint_count||b>=r->waypoint_count)return FALSE;z=r->waypoints[a];if(a<b)memmove(&r->waypoints[a],&r->waypoints[a+1],sizeof(z)*(b-a));else memmove(&r->waypoints[b+1],&r->waypoints[b],sizeof(z)*(a-b));r->waypoints[b]=z;return TRUE;}
static const char *sched_name(int v){static const char*n[]={"Remain","Travel","Patrol","Idle","Guard","Work","Sleep","Rest","Return home"};return v>=0&&v<AI_SCHEDULE_ACTIVITY_MAX?n[v]:"INVALID";}
static int sched_width(const struct ai_schedule_entry *e){return (e->end_hour-e->start_hour+24)%24;}
static int sched_days(int m) {
  int i, n = 0;
  for (i = 0; i < 7; i++)
    n += (m >> i) & 1;
  return n;
}
static void sched_room(char *b, size_t n, int v) {
  room_rnum r;
  if (!v)
    snprintf(b, n, "None");
  else {
    r = real_room(v);
    snprintf(b, n, "%d - %s", v, r == NOWHERE ? "INVALID" : world[r].name);
  }
}
static int sched_adj(int a, int b) {
  room_rnum x = real_room(a), y = real_room(b);
  int d;
  if (x == NOWHERE || y == NOWHERE)
    return 0;
  for (d = 0; d < DIR_COUNT; d++)
    if (world[x].dir_option[d] && world[x].dir_option[d]->to_room == y)
      return 1;
  return 0;
}
void ai_actor_schedule_preview(const struct mob_ai_config *c, int d, int h,
                               char *out, size_t n) {
  int i, j, w;
  char room[128];
  if (!out || !n)
    return;
  out[0] = 0;
  if (!c) {
    ai_preview_add(
        out, n,
        "Compiled prototype preview\r\nERROR: no schedule configuration.\r\n");
    return;
  }
  w = ai_schedule_select(c, d, h);
  ai_preview_add(
      out, n,
      "Compiled prototype preview; live runtime suppression "
      "unavailable.\r\nSchedule enabled: %s; resume: %s; default failure "
      "policy: %d\r\nMovement boundary: adjacent destinations, one "
      "perform_move(mob, direction, 1), no BFS.\r\nPreview day/hour: %d/%02d; "
      "entries: %d; routes: %d\r\n",
      c->schedule_enabled ? "Yes" : "No",
      c->resume_after_interrupt ? "Yes" : "No", c->default_failure_policy, d, h,
      c->schedule_count, c->patrol_count);
  for (i = 0; i < 5; i++) {
    sched_room(room, sizeof(room),
               i == 0   ? c->home_room_vnum
               : i == 1 ? c->work_room_vnum
               : i == 2 ? c->sleep_room_vnum
               : i == 3 ? c->guard_room_vnum
                        : c->fallback_room_vnum);
    ai_preview_add(out, n, "%s room: %s\r\n",
                   i == 0   ? "Home"
                   : i == 1 ? "Work"
                   : i == 2 ? "Sleep"
                   : i == 3 ? "Guard"
                            : "Fallback",
                   room);
  }
  for (i = 0; i < c->schedule_count; i++) {
    const struct ai_schedule_entry *e = &c->schedules[i];
    int active = e->enabled && ai_schedule_day_matches(e->day_mask, d) &&
                 ai_schedule_time_matches(e->start_hour, e->end_hour, h);
    ai_preview_add(
        out, n,
        "\r\nEntry position %d; stable ID %d; enabled %s; %02d-%02d%s; days "
        "0x%02x; specificity %d; width %d; priority %d\r\nActivity %s; "
        "destination type %d value %d; arrival %d; departure %d; interruption "
        "%d; failure %d; travel timeout %d; attempts %d; retry/wait "
        "%d\r\nValidation: %s; preview day: %s; preview hour: %s; live "
        "suppression: unavailable; rank: %s\r\n",
        i + 1, e->id, e->enabled ? "Yes" : "No", e->start_hour, e->end_hour,
        e->start_hour > e->end_hour ? " overnight" : "", e->day_mask,
        sched_days(e->day_mask), sched_width(e), e->priority,
        sched_name(e->activity), e->destination, e->destination_value,
        e->arrival_action, e->departure_action, e->interruption_policy,
        e->failure_policy, e->max_travel_time, e->max_attempts,
        e->wait_duration,
        (e->id > 0 && e->day_mask && e->activity >= 0 &&
         e->activity < AI_SCHEDULE_ACTIVITY_MAX)
            ? "valid"
            : "INVALID",
        ai_schedule_day_matches(e->day_mask, d) ? "Yes" : "No",
        ai_schedule_time_matches(e->start_hour, e->end_hour, h) ? "Yes" : "No",
        i == w ? "WINNER" : "loser");
    if (e->destination == AI_DEST_ROOM_VNUM) {
      sched_room(room, sizeof(room), e->destination_value);
      ai_preview_add(out, n, "Resolved room: %s\r\n", room);
    }
    if (i == w)
      ai_preview_add(out, n,
                     "Winner explanation: eligible; canonical selection "
                     "compares higher priority, more-specific day rule, "
                     "narrower window, then stored order. Wandering: %s.\r\n",
                     (e->activity == AI_SCHEDULE_REMAIN ||
                      e->activity == AI_SCHEDULE_IDLE_SOCIAL)
                         ? "depends on runtime state"
                         : "blocks generic wandering while active");
    else if (active && w >= 0) {
      const struct ai_schedule_entry *x = &c->schedules[w];
      ai_preview_add(out, n, "Losing active candidate: %s.\r\n",
                     e->priority < x->priority ? "Lower priority"
                     : sched_days(e->day_mask) > sched_days(x->day_mask)
                         ? "Less-specific day mask"
                     : sched_width(e) > sched_width(x)
                         ? "Wider time window"
                         : "Later stored order; Resolved by stored order");
    }
  }
  if (w < 0)
    ai_preview_add(out, n,
                   "Winner: none. Wandering: allows normal wandering.\r\n");
  for (i = 0; i < c->patrol_count; i++) {
    const struct ai_patrol_route *r = &c->patrols[i];
    ai_preview_add(
        out, n,
        "\r\nPatrol position %d; stable route ID %d; label %s; enabled %s; "
        "mode %d; failure %d; waypoints %d; movement boundary adjacent.\r\n",
        i + 1, r->id, r->label, r->enabled ? "Yes" : "No", r->loop_mode,
        r->failure_policy, r->waypoint_count);
    for (j = 0; j < r->waypoint_count; j++) {
      sched_room(room, sizeof(room), r->waypoints[j].room_vnum);
      ai_preview_add(
          out, n,
          "Waypoint %d: %s; wait %d; arrival %d; forward adjacency %s\r\n",
          j + 1, room, r->waypoints[j].wait_duration,
          r->waypoints[j].arrival_action,
          j + 1 < r->waypoint_count ? (sched_adj(r->waypoints[j].room_vnum,
                                                 r->waypoints[j + 1].room_vnum)
                                           ? "adjacent"
                                           : "NOT ADJACENT")
          : r->loop_mode == AI_PATROL_LOOP
              ? (sched_adj(r->waypoints[j].room_vnum, r->waypoints[0].room_vnum)
                     ? "loop closure adjacent"
                     : "loop closure NOT ADJACENT")
              : "terminal");
    }
    ai_preview_add(out, n, "Traversal: %s\r\n",
                   r->loop_mode == AI_PATROL_LOOP ? "1 -> 2 -> 3 -> 1"
                   : r->loop_mode == AI_PATROL_PINGPONG
                       ? "1 -> 2 -> 3 -> 2 -> 1"
                       : "1 -> 2 -> 3 -> Complete");
  }
}
void ai_actor_schedule_validate(const struct mob_ai_config *c, char *out,
                                size_t n) {
  int i, j;
  if (!out || !n)
    return;
  out[0] = 0;
  ai_preview_add(out, n, "Schedule Errors\r\n");
  if (!c) {
    ai_preview_add(out, n, "ERROR: no schedule configuration.\r\n");
    return;
  }
  if (c->schedule_enabled && !c->schedule_count)
    ai_preview_add(out, n, "ERROR: schedule enabled with zero entries.\r\n");
  for (i = 0; i < c->schedule_count; i++) {
    const struct ai_schedule_entry *e = &c->schedules[i];
    if (e->id <= 0 || e->start_hour < 0 || e->start_hour > 23 ||
        e->end_hour < 0 || e->end_hour > 23 || !e->day_mask ||
        (e->day_mask & ~AI_DAY_MASK_ALL) || e->priority < -100 ||
        e->priority > 100 || e->activity < 0 ||
        e->activity >= AI_SCHEDULE_ACTIVITY_MAX || e->destination < 0 ||
        e->destination >= AI_DESTINATION_MAX || e->arrival_action < 0 ||
        e->arrival_action >= AI_SCHEDULE_ACTION_MAX ||
        e->departure_action < 0 ||
        e->departure_action >= AI_SCHEDULE_ACTION_MAX ||
        e->interruption_policy < 0 ||
        e->interruption_policy >= AI_INTERRUPT_MAX || e->failure_policy < 0 ||
        e->failure_policy >= AI_FAILURE_MAX || e->max_attempts <= 0 ||
        e->max_travel_time < 0 || e->wait_duration < 0)
      ai_preview_add(out, n, "ERROR: invalid schedule entry %d.\r\n", e->id);
    if (e->destination == AI_DEST_ROOM_VNUM &&
        real_room(e->destination_value) == NOWHERE)
      ai_preview_add(
          out, n, "ERROR: invalid direct destination in entry %d.\r\n", e->id);
    if (e->activity == AI_SCHEDULE_PATROL &&
        !ai_schedule_route((struct mob_ai_config *)c, e->route_id))
      ai_preview_add(out, n, "ERROR: missing route in entry %d.\r\n", e->id);
    for (j = i + 1; j < c->schedule_count; j++)
      if (e->id == c->schedules[j].id)
        ai_preview_add(out, n, "ERROR: duplicate stable entry ID %d.\r\n",
                       e->id);
  }
  ai_preview_add(out, n, "Schedule Warnings\r\n");
  for (i = 0; i < c->schedule_count; i++)
    for (j = i + 1; j < c->schedule_count; j++)
      if (c->schedules[i].priority == c->schedules[j].priority &&
          ai_schedule_entries_overlap(&c->schedules[i], &c->schedules[j]))
        ai_preview_add(out, n,
                       "WARNING: equal-priority overlap %d/%d resolves by "
                       "stored order.\r\n",
                       c->schedules[i].id, c->schedules[j].id);
  ai_preview_add(out, n, "Patrol Errors\r\n");
  for (i = 0; i < c->patrol_count; i++) {
    const struct ai_patrol_route *r = &c->patrols[i];
    if (r->id <= 0 || r->loop_mode < 0 || r->loop_mode >= AI_PATROL_LOOP_MAX ||
        r->failure_policy < 0 || r->failure_policy >= AI_FAILURE_MAX ||
        (r->enabled && !r->waypoint_count))
      ai_preview_add(out, n, "ERROR: invalid patrol route %d.\r\n", r->id);
    for (j = 0; j < r->waypoint_count; j++) {
      if (real_room(r->waypoints[j].room_vnum) == NOWHERE ||
          r->waypoints[j].wait_duration < 0 ||
          r->waypoints[j].arrival_action < 0 ||
          r->waypoints[j].arrival_action >= AI_SCHEDULE_ACTION_MAX)
        ai_preview_add(out, n, "ERROR: invalid waypoint %d on route %d.\r\n",
                       j + 1, r->id);
      if (j &&
          !sched_adj(r->waypoints[j - 1].room_vnum, r->waypoints[j].room_vnum))
        ai_preview_add(out, n,
                       "ERROR: invalid adjacent transition on route %d.\r\n",
                       r->id);
    }
    if (r->loop_mode == AI_PATROL_LOOP && r->waypoint_count > 1 &&
        !sched_adj(r->waypoints[r->waypoint_count - 1].room_vnum,
                   r->waypoints[0].room_vnum))
      ai_preview_add(out, n, "ERROR: invalid Loop closure on route %d.\r\n",
                     r->id);
    for (j = i + 1; j < c->patrol_count; j++)
      if (r->id == c->patrols[j].id)
        ai_preview_add(out, n, "ERROR: duplicate stable route ID %d.\r\n",
                       r->id);
  }
  ai_preview_add(
      out, n,
      "Patrol Warnings\r\nWARNING: closed exits may conditionally block "
      "authored adjacency.\r\nCross-System Errors\r\nINFO: mobile-flag "
      "conflicts require a live actor context.\r\nCross-System Warnings\r\n");
  if (c->social == AI_SOCIAL_SILENT)
    ai_preview_add(out, n,
                   "WARNING: schedule arrival/departure speech may conflict "
                   "with Silent style.\r\n");
  for (i = AI_DIALOGUE_WORK; i < AI_DIALOGUE_CATEGORIES; i++)
    if (!c->dialogue_count[i])
      ai_preview_add(out, n,
                     "WARNING: schedule dialogue category %s is empty.\r\n",
                     ai_dialogue_category_name(i));
  ai_preview_add(
      out, n,
      "INFO: combat interrupts schedule execution; major threat responses "
      "interrupt; movement yields while fighting.\r\nINFO: scheduled movement "
      "uses perform_move and preserves triggers. DG Script and "
      "special-procedure movement are not suppressed and may displace the "
      "actor.\r\nSummary\r\nINFO: validation is non-mutating; schedule "
      "dialogue is category-level.\r\n");
}

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

const char *ai_actor_target_weight_name(int index) { return index >= 0 && index < AI_TARGET_WEIGHTS ? ai_target_names[index] : "Unavailable"; }
int ai_threat_step_edit(struct mob_ai_config *c, int index, const struct ai_threat_step *step) { struct ai_threat_step z; if (!c || !step || index < 0 || index >= c->threat_step_count) return FALSE; z=*step; z.minimum_severity=AI_CLAMP(z.minimum_severity,0,100); z.cooldown=AI_CLAMP(z.cooldown,0,300); z.max_repetitions=AI_CLAMP(z.max_repetitions,1,10); z.advance_on_failure=!!z.advance_on_failure; if (!ai_threat_step_valid(&z,c->threat_enabled)) return FALSE; c->threat_steps[index]=z; return TRUE; }
int ai_threat_step_move(struct mob_ai_config *c, int from, int to) { struct ai_threat_step z; if (!c || from < 0 || to < 0 || from >= c->threat_step_count || to >= c->threat_step_count || from == to) return FALSE; z=c->threat_steps[from]; if(from<to) memmove(&c->threat_steps[from],&c->threat_steps[from+1],sizeof(z)*(to-from)); else memmove(&c->threat_steps[to+1],&c->threat_steps[to],sizeof(z)*(from-to)); c->threat_steps[to]=z; return TRUE; }
int ai_help_event_admit(struct ai_help_event *e, long id, time_t now) { int i; if(!e || !id || now > e->expires_at) return FALSE; for(i=0;i<e->responder_count;i++) if(e->responders[i]==id) return FALSE; if(e->responder_count >= e->maximum_responders || e->responder_count >= AI_HELP_EVENT_RESPONDERS) return FALSE; e->responders[e->responder_count++]=id; return TRUE; }
static struct ai_help_event *ai_help_event_new(struct char_data *source, struct char_data *target, struct char_data *victim, time_t now) { int i,slot=0; for(i=0;i<AI_HELP_EVENT_MAX;i++) { if(!ai_help_events[i].id || now > ai_help_events[i].expires_at) { slot=i; break; } if(ai_help_events[i].expires_at < ai_help_events[slot].expires_at) slot=i; } memset(&ai_help_events[slot],0,sizeof(ai_help_events[slot])); ai_help_events[slot].id=ai_next_help_event_id++; if(!ai_next_help_event_id) ai_next_help_event_id=1; ai_help_events[slot].source_id=source?GET_IDNUM(source):0; ai_help_events[slot].target_id=target?GET_IDNUM(target):0; ai_help_events[slot].victim_id=victim?GET_IDNUM(victim):0; ai_help_events[slot].room_vnum=(source&&IN_ROOM(source)!=NOWHERE)?world[IN_ROOM(source)].number:0; ai_help_events[slot].maximum_responders=source&&source->ai_prof?source->ai_prof->max_responders:0; ai_help_events[slot].created_at=now; ai_help_events[slot].expires_at=now+30; ai_help_events[slot].call_help_emitted=TRUE; return &ai_help_events[slot]; }
static int ai_style_switch_threshold(const struct ai_actor_profile *p) { int v=p->target_switch_threshold; switch(p->combat_style){case AI_COMBAT_PASSIVE:v+=100;break;case AI_COMBAT_DEFENSIVE:v+=20;break;case AI_COMBAT_AGGRESSIVE:v-=10;break;case AI_COMBAT_CONTROLLER:v-=15;break;case AI_COMBAT_BOSS:v+=75;break;default:break;} return AI_CLAMP(v,0,200); }

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
        /* Runtime contains IDs, not configuration pointers; discard stale IDs and
         * force canonical selection on the next normal schedule tick. */
        if (it->ai_state) memset(it->ai_state, 0, sizeof(*it->ai_state));
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
  if (mob->ai_prof && mob->ai_prof->protect_group && GROUP(mob) && GROUP(mob) == GROUP(other)) { if(reason)*reason="group member"; return TRUE; }
  if (mob->ai_prof && mob->ai_prof->protect_same_prototype && IS_NPC(mob) && IS_NPC(other) && GET_MOB_RNUM(mob) == GET_MOB_RNUM(other)) { if(reason)*reason="same prototype"; return TRUE; }
  if (mob->ai_prof && mob->ai_prof->protect_same_role && other->ai_prof && mob->ai_prof->role == other->ai_prof->role) { if(reason)*reason="same role"; return TRUE; }
  m=ai_threat_memory(mob, GET_IDNUM(other));
  if (mob->ai_prof && mob->ai_prof->protect_trusted && m && ai_actor_relationship(m) == AI_REL_TRUSTED) { if(reason)*reason="trusted memory"; return TRUE; }
  return FALSE;
}

int ai_actor_target_score(struct char_data *mob, struct char_data *candidate)
{
  struct ai_actor_memory_entry *m; int score=0, hp; const char *why;
  if (!mob || !mob->ai_prof || !candidate || mob == candidate || IN_ROOM(mob) == NOWHERE || IN_ROOM(mob) != IN_ROOM(candidate) || DEAD(candidate) || !CAN_SEE(mob,candidate) || ROOM_FLAGGED(IN_ROOM(mob), ROOM_PEACEFUL)) return -100000;
  if (mob->ai_prof->avoid_incapacitated && GET_POS(candidate) < POS_FIGHTING) return -100000;
  m=ai_threat_memory(mob,GET_IDNUM(candidate)); hp=GET_MAX_HIT(candidate) ? GET_HIT(candidate)*100/GET_MAX_HIT(candidate) : 100;
  if (FIGHTING(candidate)==mob) score+=mob->ai_prof->target_weight[AI_TARGET_CURRENT_ATTACKER];
  /* Candidate attacking a local ally is deliberately evaluated through the shared classifier. */
  { struct char_data *ally; for(ally=world[IN_ROOM(mob)].people;ally;ally=ally->next_in_room) if(FIGHTING(candidate)==ally && ai_actor_is_local_ally(mob,ally,&why)) { if(!str_cmp(why,"group member")) score+=mob->ai_prof->target_weight[AI_TARGET_GROUP_ATTACKER]; else score+=mob->ai_prof->target_weight[AI_TARGET_TRUSTED_ATTACKER]; } }
  if (m && ai_actor_relationship(m)>=AI_REL_HOSTILE) score+=mob->ai_prof->target_weight[AI_TARGET_KNOWN_HOSTILE]+m->threat_severity;
  score+=mob->ai_prof->target_weight[AI_TARGET_LOW_HEALTH]*(100-hp)/100;
  score+=IS_NPC(candidate)?mob->ai_prof->target_weight[AI_TARGET_NPC]:mob->ai_prof->target_weight[AI_TARGET_PLAYER];
  if (mob->ai_state && mob->ai_state->last_selected_target_idnum==GET_IDNUM(candidate)) score+=mob->ai_prof->target_weight[AI_TARGET_PREVIOUS];
  switch(mob->ai_prof->combat_style) {
    case AI_COMBAT_DEFENSIVE: if(FIGHTING(candidate)==mob) score+=20; break;
    case AI_COMBAT_AGGRESSIVE: if(m && ai_actor_relationship(m)>=AI_REL_HOSTILE) score+=20; break;
    case AI_COMBAT_PROTECTOR: if(FIGHTING(candidate)!=NULL && FIGHTING(candidate)!=mob && ai_actor_is_local_ally(mob,FIGHTING(candidate),&why)) score+=35; break;
    case AI_COMBAT_COWARDLY: score-=hp/8; break;
    case AI_COMBAT_FANATICAL: if(mob->ai_state && mob->ai_state->last_selected_target_idnum==GET_IDNUM(candidate)) score+=30; break;
    case AI_COMBAT_OPPORTUNIST: score+=(100-hp)/3 + (FIGHTING(candidate)?10:0); break;
    case AI_COMBAT_CONTROLLER: if(m) score+=m->threat_severity*2; break;
    case AI_COMBAT_BOSS: if(FIGHTING(candidate)==mob) score+=45; if(m) score+=m->threat_severity; break;
    default: break;
  }
  return score;
}
int ai_actor_should_flee(struct char_data *mob)
{
  int hp, threshold;
  if (!mob || !mob->ai_prof || !FIGHTING(mob) || !mob->ai_prof->may_flee || MOB_FLAGGED(mob,MOB_SENTINEL) || !GET_MAX_HIT(mob)) return FALSE;
  threshold=mob->ai_prof->flee_hp_percent; if (!threshold) return FALSE;
  if (mob->ai_prof->combat_style==AI_COMBAT_PASSIVE) threshold+=10;
  if (mob->ai_prof->combat_style==AI_COMBAT_DEFENSIVE) threshold+=5;
  if (mob->ai_prof->combat_style==AI_COMBAT_AGGRESSIVE) threshold-=8;
  if (mob->ai_prof->combat_style==AI_COMBAT_PROTECTOR && FIGHTING(mob) && ai_actor_is_local_ally(mob, FIGHTING(FIGHTING(mob)), NULL)) threshold-=15;
  if (mob->ai_prof->combat_style==AI_COMBAT_COWARDLY) threshold+=20;
  if (mob->ai_prof->combat_style==AI_COMBAT_FANATICAL || mob->ai_prof->combat_style==AI_COMBAT_BOSS) threshold-=30;
  threshold += (mob->ai_prof->personality[AI_TRAIT_SUSPICION]-mob->ai_prof->personality[AI_TRAIT_BRAVERY])/4;
  hp=GET_HIT(mob)*100/GET_MAX_HIT(mob); return hp <= AI_CLAMP(threshold,0,100);
}

static void ai_preview_add(char *out, size_t size, const char *fmt, ...)
{
  va_list ap; size_t used;
  if (!out || !size) return;
  used=strlen(out); if (used>=size-1) return;
  va_start(ap,fmt); vsnprintf(out+used,size-used,fmt,ap); va_end(ap);
}

void ai_actor_combat_preview(const struct mob_ai_config *c, char *out,
                             size_t size) {
  static const char *styles[] = {
      "Passive",  "Defensive", "Balanced",    "Aggressive", "Protector",
      "Cowardly", "Fanatical", "Opportunist", "Controller", "Boss"};
  int i;
  if (!out || !size)
    return;
  out[0] = '\0';
  if (!c) {
    ai_preview_add(out, size, "Combat Profile: unavailable\r\n");
    return;
  }
  ai_preview_add(
      out, size,
      "Combat Profile\r\nCombat Style: %s\r\nCombat Policy Enabled: "
      "%s\r\nEffective Initiation Policy: %s\r\nEffective Retaliation Policy: "
      "%s\r\nEffective Assistance Policy: %s\r\nEffective Call-Help Policy: "
      "%s\r\nEffective Flee Policy: %s\r\n",
      (c->combat_style >= 0 && c->combat_style < AI_COMBAT_STYLE_MAX)
          ? styles[c->combat_style]
          : "Unknown",
      c->combat_enabled ? "Yes" : "No",
      c->may_initiate ? "Enabled" : "Disabled",
      c->retaliate_self || c->retaliate_ally || c->retaliate_hostile
          ? "Enabled"
          : "Disabled",
      c->may_assist ? "Enabled" : "Disabled",
      c->may_call_help ? "Enabled" : "Disabled",
      c->may_flee ? "Enabled" : "Disabled");
  ai_preview_add(
      out, size,
      "May Initiate:%s  May Assist:%s  May Call Help:%s  May "
      "Flee:%s\r\nRetaliate Self:%s Allies:%s Known Hostile:%s\r\nProtect "
      "Trusted:%s Group:%s Same Role:%s Same Prototype:%s  Avoid "
      "Incapacitated:%s  Avoid Civilians: Unavailable\r\nFlee HP:%d%% Assist "
      "Severity:%d Switch Threshold:%d Cooldown:%d Max Responders:%d "
      "Flee-attempt Cooldown:%d\r\n",
      c->may_initiate ? "Yes" : "No", c->may_assist ? "Yes" : "No",
      c->may_call_help ? "Yes" : "No", c->may_flee ? "Yes" : "No",
      c->retaliate_self ? "Yes" : "No", c->retaliate_ally ? "Yes" : "No",
      c->retaliate_hostile ? "Yes" : "No", c->protect_trusted ? "Yes" : "No",
      c->protect_group ? "Yes" : "No", c->protect_same_role ? "Yes" : "No",
      c->protect_same_prototype ? "Yes" : "No",
      c->avoid_incapacitated ? "Yes" : "No", c->flee_hp_percent,
      c->assist_severity, c->target_switch_threshold, c->combat_cooldown,
      c->max_responders, c->combat_cooldown);
  ai_preview_add(
      out, size,
      "Style modifiers (derived): initiation 0; assist 0; flee %d; switch %d; "
      "target lock %d; current-attacker %d; protected-ally %d; known-hostile "
      "%d; health-target %d; threat-target %d\r\nTarget Weights:\r\n",
      c->combat_style == AI_COMBAT_COWARDLY ? 20
      : c->combat_style == AI_COMBAT_FANATICAL ||
              c->combat_style == AI_COMBAT_BOSS
          ? -30
          : 0,
      c->combat_style == AI_COMBAT_PASSIVE     ? 100
      : c->combat_style == AI_COMBAT_DEFENSIVE ? 20
      : c->combat_style == AI_COMBAT_AGGRESSIVE ||
              c->combat_style == AI_COMBAT_CONTROLLER
          ? -10
          : 0,
      c->combat_style == AI_COMBAT_BOSS ||
              c->combat_style == AI_COMBAT_FANATICAL
          ? 30
          : 0,
      c->target_weight[0], c->target_weight[1] + c->target_weight[2],
      c->target_weight[3], c->target_weight[4], c->target_weight[3]);
  for (i = 0; i < AI_TARGET_WEIGHTS; i++)
    ai_preview_add(out, size, "  %s: %d\r\n", ai_actor_target_weight_name(i),
                   c->target_weight[i]);
  ai_preview_add(
      out, size,
      "Spellcaster: Unavailable; Healer: Unavailable; Ranged attacker: "
      "Unavailable; Criminal: Unavailable; Highest damage output: "
      "Unavailable\r\nCoordination: Group detection available; event-scoped "
      "responder budgeting active; maximum responders %d; help event expiry "
      "30s; duplicate responder suppression active; recursive call-help "
      "suppression active; listener evaluation room-local; identity confidence "
      "and ally classification required.\r\nAlly reasons: Master; Follower; "
      "Group member; Trusted memory; Same role when enabled; Same prototype "
      "when enabled.\r\nLifecycle tracking: Combat start, Direct attack, "
      "Protected ally attack, Assistance, Target switch, Actor flee, Opponent "
      "flee, Opponent defeat, Actor defeat, Combat end, Help heard, Help "
      "answered.\r\nRescue: Unavailable; Surrender: Unavailable; Pursuit: "
      "Unavailable.\r\n",
      c->max_responders);
}

void ai_actor_combat_validate(const struct mob_ai_config *c, char *out,
                              size_t size) {
  int i, all_negative = TRUE, hostile_zero = TRUE;
  if (!out || !size)
    return;
  out[0] = '\0';
  if (!c) {
    ai_preview_add(out, size, "ERROR: no combat profile.\r\n");
    return;
  }
#define CVERR(x) ai_preview_add(out, size, "ERROR: %s\r\n", x)
#define CVWARN(x) ai_preview_add(out, size, "WARNING: %s\r\n", x)
  if (c->combat_enabled && !c->may_initiate && !c->retaliate_self &&
      !c->retaliate_ally && !c->retaliate_hostile && !c->may_assist)
    CVERR("policy has no initiation, retaliation, or assistance path");
  if (c->may_assist && c->max_responders == 0)
    CVERR("assist enabled with zero responders");
  if (c->may_call_help && !c->notice_speech)
    CVERR("call help lacks normal communication path");
  if (c->may_call_help && c->max_responders < 0)
    CVERR("local event budgeting unavailable");
  if (c->may_flee && c->combat_cooldown < 1)
    CVERR("flee lacks normal dispatch cooldown");
  if (c->surrender_hp_percent > 0)
    CVERR("surrender is unavailable");
  if (c->hunt_enabled)
    CVERR("pursuit is unavailable");
  if (c->switch_targets && c->combat_cooldown == 0 &&
      c->target_switch_threshold == 0)
    CVERR("target switching has zero cooldown and threshold");
  if (c->max_responders < 0 || c->max_responders > 10)
    CVERR("responder limit outside bounds");
  if (c->flee_hp_percent < 0 || c->flee_hp_percent > 100)
    CVERR("flee threshold outside bounds");
  if (c->assist_severity < 0 || c->assist_severity > 100)
    CVERR("assist severity outside bounds");
  if (c->target_switch_threshold < 0 || c->target_switch_threshold > 100)
    CVERR("switch threshold outside bounds");
  if (c->combat_style < 0 || c->combat_style >= AI_COMBAT_STYLE_MAX)
    CVERR("unknown combat style");
  for (i = 0; i < c->threat_step_count; i++)
    if (c->threat_steps[i].type < 0 ||
        c->threat_steps[i].type >= AI_THREAT_RESPONSE_MAX)
      CVERR("threat sequence contains invalid step");
    else if (!ai_threat_response_available(c->threat_steps[i].type))
      CVERR("threat sequence references unavailable response");
    else if (!c->threat_enabled[c->threat_steps[i].type])
      CVERR("threat sequence references disabled response");
  if (c->combat_style == AI_COMBAT_PASSIVE && c->may_initiate)
    CVWARN("Passive style with initiation enabled");
  if ((c->combat_style == AI_COMBAT_FANATICAL ||
       c->combat_style == AI_COMBAT_BOSS) &&
      c->flee_hp_percent > 50)
    CVWARN("Fanatical/Boss high flee threshold");
  if (c->combat_style == AI_COMBAT_COWARDLY && !c->may_flee)
    CVWARN("Cowardly style with fleeing disabled");
  if (c->combat_style == AI_COMBAT_PROTECTOR && !c->protect_trusted &&
      !c->protect_group && !c->protect_same_role && !c->protect_same_prototype)
    CVWARN("Protector without protection categories");
  if (c->may_assist && !c->protect_trusted && !c->protect_group &&
      !c->protect_same_role && !c->protect_same_prototype)
    CVWARN("Assist enabled with no ally category");
  if (c->protect_group && !c->notice_ally_attack)
    CVWARN("Protect Group lacks ally perception");
  if (c->retaliate_ally && !c->notice_ally_attack)
    CVWARN("ally retaliation lacks Notice Attacks on Allies");
  if (c->retaliate_hostile && !c->memory_enabled)
    CVWARN("known-hostile retaliation lacks memory");
  if (c->protect_trusted && (!c->memory_enabled || !c->remember_assistance))
    CVWARN("trusted protection lacks retained memory");
  if (c->memory_enabled == FALSE)
    CVWARN("lifecycle memory disabled with general memory");
  if (c->remember_assistance && !c->may_assist)
    CVWARN("remember assistance while assistance disabled");
  for (i = 0; i < AI_TARGET_WEIGHTS; i++) {
    if (c->target_weight[i] >= 0)
      all_negative = FALSE;
    if (i <= AI_TARGET_KNOWN_HOSTILE && c->target_weight[i] != 0)
      hostile_zero = FALSE;
  }
  if (all_negative)
    CVWARN("all supported target weights are negative");
  if (hostile_zero)
    CVWARN("all supported hostile target weights are zero");
  if ((c->combat_style == AI_COMBAT_BOSS ||
       c->combat_style == AI_COMBAT_FANATICAL) &&
      c->target_weight[AI_TARGET_PREVIOUS] > 0)
    CVWARN("previous-target weight and target lock may prevent switching");
  if (c->combat_style == AI_COMBAT_AGGRESSIVE && !c->may_initiate &&
      !c->retaliate_self)
    CVWARN("Aggressive style has no initiation or retaliation");
  if (c->may_call_help && c->max_responders <= 1)
    CVWARN("Call Help maximum responders is one or fewer");
  if (c->combat_cooldown <= 2 && c->target_switch_threshold <= 5)
    CVWARN("very low cooldown with low switch threshold");
  if (!*out)
    ai_preview_add(out, size, "Combat validation: no errors or warnings.\r\n");
#undef CVERR
#undef CVWARN
}

static int ai_actor_combat_tick(struct char_data *mob, time_t now)
{
  struct char_data *it,*best=NULL; int best_score=-100000, current_score, score;
  if (!mob->ai_prof->combat_enabled || !FIGHTING(mob) || now-mob->ai_state->last_combat_action < mob->ai_prof->combat_cooldown) return FALSE;
  if (ai_actor_should_flee(mob) && now-mob->ai_state->last_flee_attempt >= mob->ai_prof->combat_cooldown) { room_rnum before=IN_ROOM(mob); struct char_data *opponent=FIGHTING(mob); mob->ai_state->last_flee_attempt=now; do_flee(mob,"",0,0); if (IN_ROOM(mob)!=before || !FIGHTING(mob)) ai_actor_event_fled(mob,opponent,TRUE); mob->ai_state->last_combat_action=now; return TRUE; }
  current_score=ai_actor_target_score(mob,FIGHTING(mob));
  if (!mob->ai_prof->switch_targets) return FALSE;
  for(it=world[IN_ROOM(mob)].people;it;it=it->next_in_room) { score=ai_actor_target_score(mob,it); if(score>best_score) { best_score=score; best=it; } }
  if (best && best != FIGHTING(mob) && best_score >= current_score + ai_style_switch_threshold(mob->ai_prof)) { struct char_data *old=FIGHTING(mob); stop_fighting(mob); hit(mob,best,0); if (FIGHTING(mob)==best) { mob->ai_state->last_switch_from_id=GET_IDNUM(old); mob->ai_state->last_switch_to_id=GET_IDNUM(best); } mob->ai_state->last_selected_target_idnum=GET_IDNUM(best); mob->ai_state->last_combat_action=mob->ai_state->last_target_switch=now; return TRUE; }
  return FALSE;
}

void ai_actor_patrol_preview(const struct mob_ai_config *c, int route_id, char *out, size_t size)
{ int i, j; const struct ai_patrol_route *r=NULL; if(!out||!size)return; out[0]=0; if(!c){ai_preview_add(out,size,"ERROR: no schedule configuration.\r\n");return;} for(i=0;i<c->patrol_count;i++)if(c->patrols[i].id==route_id){r=&c->patrols[i];break;} if(!r){ai_preview_add(out,size,"ERROR: patrol route %d is unavailable.\r\n",route_id);return;} ai_preview_add(out,size,"Route #%d %s mode %d, %d waypoints.\r\n",r->id,r->enabled?"enabled":"disabled",r->loop_mode,r->waypoint_count); for(i=0;i<r->waypoint_count;i++){room_rnum room=real_room(r->waypoints[i].room_vnum);ai_preview_add(out,size," %d: %d (%s), wait %d, arrival %d%s\r\n",i+1,r->waypoints[i].room_vnum,room==NOWHERE?"Invalid":world[room].name,r->waypoints[i].wait_duration,r->waypoints[i].arrival_action,i+1==r->waypoint_count&&r->loop_mode==AI_PATROL_LOOP?"; then returns to waypoint 1":"");} if(r->loop_mode==AI_PATROL_PINGPONG)ai_preview_add(out,size,"Ping-pong reverses at each endpoint.\r\n"); if(r->loop_mode==AI_PATROL_ONCE)ai_preview_add(out,size,"Final waypoint is terminal.\r\n"); for(i=0;i<r->waypoint_count;i++){int next=(i+1<r->waypoint_count)?i+1:(r->loop_mode==AI_PATROL_LOOP?0:-1);if(next>=0){room_rnum from=real_room(r->waypoints[i].room_vnum),to=real_room(r->waypoints[next].room_vnum);int adjacent=FALSE;if(from!=NOWHERE&&to!=NOWHERE)for(j=0;j<DIR_COUNT;j++)if(world[from].dir_option[j]&&world[from].dir_option[j]->to_room==to)adjacent=TRUE;ai_preview_add(out,size,"   transition %d -> %d: %s\r\n",i+1,next+1,adjacent?"adjacent":"NOT ADJACENT");}} }

static struct ai_patrol_route *ai_schedule_route(struct mob_ai_config *c,int id){int i;for(i=0;c&&i<c->patrol_count;i++)if(c->patrols[i].id==id)return &c->patrols[i];return NULL;}
static struct ai_schedule_entry *ai_schedule_entry(struct mob_ai_config *c,int id){int i;for(i=0;c&&i<c->schedule_count;i++)if(c->schedules[i].id==id)return &c->schedules[i];return NULL;}
int ai_schedule_interruption_is_minor(int reason){return reason==AI_SCHEDULE_INTERRUPT_MINOR_EVENT;}
int ai_schedule_entry_activation_signature(const struct ai_schedule_entry *e,int day){return e ? (e->id*257+day*31+e->start_hour*24+e->end_hour) : 0;}
int ai_schedule_entry_is_suppressed_for_window(const struct ai_actor_state *s,const struct ai_schedule_entry *e,int day){return s&&e&&s->schedule_skipped_id==e->id&&s->schedule_activation_day==day&&s->schedule_activation_start==e->start_hour&&s->schedule_activation_end==e->end_hour;}
int ai_schedule_retry_ready(const struct ai_actor_state *s,time_t now){return !s||!s->schedule_retry_at||now>=s->schedule_retry_at;}
int ai_schedule_travel_timed_out(const struct ai_actor_state *s,const struct ai_schedule_entry *e,time_t now){return s&&e&&e->max_travel_time>0&&s->schedule_state==AI_SCHED_TRAVELING&&now-s->schedule_started_at>e->max_travel_time;}
int ai_schedule_should_block_wandering(const struct ai_actor_state *s){if(!s)return FALSE;return s->schedule_state==AI_SCHED_SELECTED||s->schedule_state==AI_SCHED_PREPARING_DEPARTURE||s->schedule_state==AI_SCHED_TRAVELING||s->schedule_state==AI_SCHED_ARRIVED||s->schedule_state==AI_SCHED_ACTIVE||s->schedule_state==AI_SCHED_WAITING_WAYPOINT||s->schedule_state==AI_SCHED_RESUMING||s->schedule_state==AI_SCHED_FAILED;}
void ai_actor_schedule_show_state(struct char_data *viewer, const struct char_data *mob)
{ const struct ai_actor_state *s; char current[128], expected[128]; if(!viewer||!mob)return; send_to_char(viewer,"\r\nSchedule Diagnostics (read-only)\r\nPrototype Configuration\r\n"); if(!mob->ai_config){send_to_char(viewer,"Schedule configured: No\r\n");return;} sched_room(current,sizeof(current),IN_ROOM(mob)==NOWHERE?0:world[IN_ROOM(mob)].number); send_to_char(viewer,"Actor: %s  Prototype VNUM: %d  Current room: %s\r\nSchedule configured: Yes  Schedule enabled: %s  Game day/hour: %d/%02d\r\n",GET_NAME(mob),GET_MOB_VNUM(mob),current,mob->ai_config->schedule_enabled?"Yes":"No",((35*time_info.month)+(time_info.day+1))%7,time_info.hours); if(!mob->ai_state){send_to_char(viewer,"Live Runtime State\r\nUnavailable: no AI runtime state.\r\n");return;} s=mob->ai_state;sched_room(expected,sizeof(expected),s->expected_room_vnum);send_to_char(viewer,"Live Runtime State\r\nState: %d  Active entry: %d  Previous entry: %d  Route: %d  Waypoint: %d  Direction: %d\r\nExpected room: %s  Wandering suppressed: %s  Interrupted: %s reason %d\r\nAttempts: %d  Failures: %d  Retry ready: %s  Retry at: %ld  Travel started: %ld  Last evaluation: %ld\r\nSkipped ID: %d  Runtime-disabled ID: %d  Resume entry/route/waypoint: %d/%d/%d\r\nDeparture emitted: %s  Arrival emitted: %s  Failure emitted: %s\r\n",s->schedule_state,s->active_schedule_id,s->previous_schedule_id,s->schedule_route_id,s->schedule_waypoint,s->patrol_direction,expected,s->schedule_wander_suppressed?"Yes":"No",s->schedule_interrupted?"Yes":"No",s->schedule_reason,s->schedule_attempts,s->schedule_failures,ai_schedule_retry_ready(s,time(0))?"Yes":"No",(long)s->schedule_retry_at,(long)s->schedule_started_at,(long)s->last_schedule_eval,s->schedule_skipped_id,s->schedule_disabled_id,s->resume_schedule_id,s->resume_route_id,s->resume_waypoint,s->schedule_departure_done?"Yes":"No",s->schedule_arrival_done?"Yes":"No",s->schedule_failure_emitted?"Yes":"No"); }

static void ai_schedule_set_state(struct ai_actor_state*s,int state){if(s)s->schedule_state=state;}
static void ai_schedule_reset_transition_flags(struct ai_actor_state*s){s->schedule_departure_done=s->schedule_arrival_done=0;s->last_arrival_action=s->last_departure_action=AI_SCHEDULE_ACTION_NONE;}
static void ai_schedule_begin_entry(struct ai_actor_state*s,const struct ai_schedule_entry*e,int day,time_t now){s->previous_schedule_id=s->active_schedule_id;s->active_schedule_id=e->id;s->schedule_route_id=e->route_id;s->schedule_destination_vnum=0;s->schedule_waypoint=0;s->patrol_direction=1;s->schedule_attempts=s->schedule_failures=0;s->schedule_retry_at=s->schedule_wait_until=0;s->schedule_started_at=now;s->schedule_activation_day=day;s->schedule_activation_start=e->start_hour;s->schedule_activation_end=e->end_hour;ai_schedule_reset_transition_flags(s);ai_schedule_set_state(s,AI_SCHED_SELECTED);}
static void ai_schedule_complete_entry(struct ai_actor_state*s){s->previous_schedule_id=s->active_schedule_id;ai_schedule_set_state(s,AI_SCHED_COMPLETED);}
static int ai_schedule_destination(struct char_data *m,const struct ai_schedule_entry *e){struct mob_ai_config*c=m->ai_config;switch(e->destination){case AI_DEST_CURRENT_ROOM:return IN_ROOM(m)!=NOWHERE?world[IN_ROOM(m)].number:0;case AI_DEST_ROOM_VNUM:return e->destination_value;case AI_DEST_HOME:return c->home_room_vnum;case AI_DEST_WORK:return c->work_room_vnum;case AI_DEST_SLEEP:return c->sleep_room_vnum;case AI_DEST_GUARD:return c->guard_room_vnum;case AI_DEST_FALLBACK:return c->fallback_room_vnum;case AI_DEST_SPAWN:return GET_MOB_VNUM(m);default:return 0;}}
static void ai_schedule_dialogue(struct char_data*m,int category,time_t now){if(m->ai_prof&&m->ai_prof->social!=AI_SOCIAL_SILENT&&now-m->ai_state->last_spoke>=m->ai_config->speech_cooldown)ai_social_say(m,category,NULL,now);}
static void ai_schedule_action(struct char_data*m,int action,int category,time_t now){if(action==AI_SCHEDULE_ACTION_SPEAK)ai_schedule_dialogue(m,category,now);else if(action==AI_SCHEDULE_ACTION_EMOTE)ai_social_emote(m,now);else if(action==AI_SCHEDULE_ACTION_SIT)do_sit(m,"",0,0);else if(action==AI_SCHEDULE_ACTION_REST)do_rest(m,"",0,0);else if(action==AI_SCHEDULE_ACTION_SLEEP&&!FIGHTING(m))do_sleep(m,"",0,0);else if(action==AI_SCHEDULE_ACTION_STAND)do_stand(m,"",0,0);else if(action==AI_SCHEDULE_ACTION_WAKE)do_wake(m,"",0,0);}
void ai_actor_schedule_interrupt(struct char_data*m,int reason,long actor_id,time_t now){struct ai_actor_state*s;(void)actor_id;if(!m||(s=m->ai_state)==NULL||!s->active_schedule_id||s->schedule_interrupted)return;if(ai_schedule_interruption_is_minor(reason))return;s->resume_schedule_id=s->active_schedule_id;s->resume_route_id=s->schedule_route_id;s->resume_waypoint=s->schedule_waypoint;s->resume_direction=s->patrol_direction;s->resume_state=s->schedule_state;s->resume_destination_vnum=s->schedule_destination_vnum;s->resume_departure_done=s->schedule_departure_done;s->resume_arrival_done=s->schedule_arrival_done;s->schedule_interrupted=TRUE;s->schedule_reason=reason;ai_schedule_reset_transition_flags(s);ai_schedule_set_state(s,AI_SCHED_INTERRUPTED);s->expected_room_vnum=IN_ROOM(m)!=NOWHERE?world[IN_ROOM(m)].number:0;s->schedule_wait_until=now;}
static void ai_schedule_mark_skip(struct ai_actor_state*s,const struct ai_schedule_entry*e){s->schedule_skipped_id=e->id;s->schedule_activation_start=e->start_hour;s->schedule_activation_end=e->end_hour;}
static void ai_schedule_apply_failure_policy(struct char_data*m,struct ai_schedule_entry*e,time_t now){struct ai_actor_state*s=m->ai_state;if(s->schedule_failure_applied)return;s->schedule_failure_applied=TRUE;if(e->failure_policy==AI_FAILURE_SKIP){ai_schedule_mark_skip(s,e);s->active_schedule_id=0;ai_schedule_set_state(s,AI_SCHED_ABORTED);}else if(e->failure_policy==AI_FAILURE_RESTART&&s->schedule_failures<=e->max_attempts){s->schedule_waypoint=0;s->patrol_direction=1;s->schedule_attempts=0;s->schedule_started_at=now;s->schedule_retry_at=now+MAX(1,e->wait_duration);ai_schedule_reset_transition_flags(s);}else if(e->failure_policy==AI_FAILURE_FALLBACK){s->schedule_destination_vnum=m->ai_config->fallback_room_vnum;if(!s->schedule_destination_vnum||real_room(s->schedule_destination_vnum)==NOWHERE){ai_schedule_set_state(s,AI_SCHED_FAILED);s->schedule_retry_at=now+MAX(1,e->wait_duration);}else{s->schedule_attempts=0;ai_schedule_reset_transition_flags(s);ai_schedule_set_state(s,AI_SCHED_SELECTED);}}else if(e->failure_policy==AI_FAILURE_ABORT){ai_schedule_mark_skip(s,e);s->active_schedule_id=0;ai_schedule_set_state(s,AI_SCHED_ABORTED);}else if(e->failure_policy==AI_FAILURE_DISABLE_UNTIL_CHANGE){s->schedule_disabled_id=e->id;s->active_schedule_id=0;ai_schedule_set_state(s,AI_SCHED_RUNTIME_DISABLED);} }
static void ai_schedule_failure(struct char_data*m,struct ai_schedule_entry*e,time_t now){struct ai_actor_state*s=m->ai_state;if(s->schedule_state==AI_SCHED_FAILED&&!ai_schedule_retry_ready(s,now))return;s->schedule_failures++;ai_schedule_set_state(s,AI_SCHED_FAILED);if(!s->schedule_failure_emitted){ai_schedule_dialogue(m,AI_DIALOGUE_SCHEDULE_FAILURE,now);s->schedule_failure_emitted=TRUE;}s->schedule_retry_at=now+MAX(1,e->wait_duration);if(s->schedule_attempts>=e->max_attempts)ai_schedule_apply_failure_policy(m,e,now);}
static void ai_schedule_apply_interruption_policy(struct char_data*m,struct ai_schedule_entry*e,time_t now){struct ai_actor_state*s=m->ai_state;if(e->interruption_policy==AI_INTERRUPT_IGNORE&&ai_schedule_interruption_is_minor(s->schedule_reason))return;if(e->interruption_policy==AI_INTERRUPT_SKIP||e->interruption_policy==AI_INTERRUPT_ABORT||!m->ai_config->resume_after_interrupt){ai_schedule_mark_skip(s,e);s->active_schedule_id=0;ai_schedule_set_state(s,AI_SCHED_ABORTED);}else if(e->interruption_policy==AI_INTERRUPT_RESTART){ai_schedule_begin_entry(s,e,s->schedule_activation_day,now);}else if(e->interruption_policy==AI_INTERRUPT_FALLBACK){s->schedule_destination_vnum=m->ai_config->fallback_room_vnum;ai_schedule_reset_transition_flags(s);ai_schedule_set_state(s,AI_SCHED_SELECTED);}else ai_schedule_set_state(s,AI_SCHED_RESUMING);s->schedule_interrupted=FALSE;}
static int ai_schedule_resume_check(struct char_data*m,struct mob_ai_config*c,struct ai_schedule_entry*winner,int day){struct ai_actor_state*s=m->ai_state;struct ai_schedule_entry*e=ai_schedule_entry(c,s->resume_schedule_id);struct ai_patrol_route*r;if(!e||!e->enabled||!ai_schedule_day_matches(e->day_mask,day)||!ai_schedule_time_matches(e->start_hour,e->end_hour,time_info.hours))return AI_SCHEDULE_RESUME_ENTRY_EXPIRED;if(winner&&winner->id!=e->id)return AI_SCHEDULE_RESUME_ENTRY_REPLACED;if(FIGHTING(m)||m->master)return AI_SCHEDULE_RESUME_CONTROL_CONFLICT;if(e->activity==AI_SCHEDULE_PATROL){r=ai_schedule_route(c,s->resume_route_id);if(!r||!r->enabled||s->resume_waypoint<0||s->resume_waypoint>=r->waypoint_count)return AI_SCHEDULE_RESUME_ROUTE_INVALID;}return AI_SCHEDULE_RESUME_VALID;}
static int ai_actor_schedule_tick(struct char_data*m,time_t now){struct mob_ai_config*c=m->ai_config;struct ai_actor_state*s=m->ai_state;struct ai_schedule_entry*e;struct ai_patrol_route*r=NULL;int day=((35*time_info.month)+(time_info.day+1))%7,idx,dest,dir;if(!c||!c->schedule_enabled){s->schedule_wander_suppressed=FALSE;return FALSE;}s->last_schedule_eval=now;if(FIGHTING(m)){ai_actor_schedule_interrupt(m,AI_SCHEDULE_INTERRUPT_COMBAT,0,now);return TRUE;}if(s->expected_room_vnum&&IN_ROOM(m)!=NOWHERE&&world[IN_ROOM(m)].number!=s->expected_room_vnum&&now!=s->last_schedule_move){ai_actor_schedule_interrupt(m,AI_SCHEDULE_INTERRUPT_UNKNOWN_DISPLACEMENT,0,now);return TRUE;}idx=ai_schedule_select(c,day,time_info.hours);if(idx<0){s->schedule_wander_suppressed=FALSE;return FALSE;}e=&c->schedules[idx];if(s->schedule_disabled_id&&s->schedule_disabled_id!=e->id)s->schedule_disabled_id=0;if(s->schedule_disabled_id==e->id||ai_schedule_entry_is_suppressed_for_window(s,e,day)){s->schedule_wander_suppressed=FALSE;return FALSE;}if(s->schedule_interrupted){ai_schedule_apply_interruption_policy(m,e,now);return ai_schedule_should_block_wandering(s);}if(s->schedule_state==AI_SCHED_RESUMING){if(ai_schedule_resume_check(m,c,e,day)==AI_SCHEDULE_RESUME_VALID){s->active_schedule_id=s->resume_schedule_id;s->schedule_route_id=s->resume_route_id;s->schedule_waypoint=s->resume_waypoint;s->patrol_direction=s->resume_direction;s->schedule_destination_vnum=s->resume_destination_vnum;s->schedule_departure_done=s->resume_departure_done;s->schedule_arrival_done=s->resume_arrival_done;ai_schedule_set_state(s,s->resume_state);}else s->active_schedule_id=0;}if(s->active_schedule_id!=e->id)ai_schedule_begin_entry(s,e,day,now);s->schedule_wander_suppressed=TRUE;if(!ai_schedule_retry_ready(s,now)||s->schedule_wait_until>now)return TRUE;if(ai_schedule_travel_timed_out(s,e,now)){ai_schedule_failure(m,e,now);return TRUE;}if(e->activity==AI_SCHEDULE_PATROL){r=ai_schedule_route(c,e->route_id);if(!r||!r->enabled||s->schedule_waypoint<0||s->schedule_waypoint>=r->waypoint_count){ai_schedule_failure(m,e,now);return TRUE;}dest=r->waypoints[s->schedule_waypoint].room_vnum;}else dest=s->schedule_destination_vnum?s->schedule_destination_vnum:ai_schedule_destination(m,e);if(!dest||real_room(dest)==NOWHERE){s->schedule_attempts++;ai_schedule_failure(m,e,now);return TRUE;}s->schedule_destination_vnum=dest;if(IN_ROOM(m)!=NOWHERE&&world[IN_ROOM(m)].number==dest){if(!s->schedule_arrival_done){ai_schedule_action(m,r?r->waypoints[s->schedule_waypoint].arrival_action:e->arrival_action,r?AI_DIALOGUE_PATROL:AI_DIALOGUE_SCHEDULE_ARRIVAL,now);s->schedule_arrival_done=TRUE;ai_schedule_set_state(s,AI_SCHED_ARRIVED);return TRUE;}if(r){if(s->schedule_wait_until==0){s->schedule_wait_until=now+MAX(0,r->waypoints[s->schedule_waypoint].wait_duration);ai_schedule_set_state(s,AI_SCHED_WAITING_WAYPOINT);return TRUE;}s->schedule_wait_until=0;s->schedule_waypoint=ai_patrol_advance(r,s->schedule_waypoint,s->patrol_direction,&s->patrol_direction);ai_schedule_reset_transition_flags(s);if(s->schedule_waypoint<0){ai_schedule_complete_entry(s);ai_schedule_mark_skip(s,e);}else ai_schedule_set_state(s,AI_SCHED_SELECTED);return TRUE;}if(e->activity==AI_SCHEDULE_SLEEP||e->activity==AI_SCHEDULE_REST){int want=e->activity==AI_SCHEDULE_SLEEP?POS_SLEEPING:POS_RESTING;if(GET_POS(m)!=want){ai_schedule_action(m,e->activity==AI_SCHEDULE_SLEEP?AI_SCHEDULE_ACTION_SLEEP:AI_SCHEDULE_ACTION_REST,AI_DIALOGUE_SLEEP,now);return TRUE;}}ai_schedule_set_state(s,AI_SCHED_ACTIVE);if(e->activity==AI_SCHEDULE_WORK)ai_schedule_dialogue(m,AI_DIALOGUE_WORK,now);if(e->activity==AI_SCHEDULE_GUARD)ai_schedule_dialogue(m,AI_DIALOGUE_GUARD,now);return TRUE;}if(GET_POS(m)==POS_SLEEPING){do_wake(m,"",0,0);return TRUE;}if(GET_POS(m)<POS_STANDING){do_stand(m,"",0,0);return TRUE;}if(!s->schedule_departure_done){ai_schedule_action(m,e->departure_action,AI_DIALOGUE_SCHEDULE_DEPARTURE,now);s->schedule_departure_done=TRUE;ai_schedule_set_state(s,AI_SCHED_PREPARING_DEPARTURE);return TRUE;}if(MOB_FLAGGED(m,MOB_SENTINEL)||m->master||IN_ROOM(m)==NOWHERE){s->schedule_attempts++;ai_schedule_failure(m,e,now);return TRUE;}for(dir=0;dir<DIR_COUNT;dir++)if(EXIT(m,dir)&&EXIT(m,dir)->to_room!=NOWHERE&&world[EXIT(m,dir)->to_room].number==dest)break;if(dir==DIR_COUNT||ROOM_FLAGGED(EXIT(m,dir)->to_room,ROOM_NOMOB)||ROOM_FLAGGED(EXIT(m,dir)->to_room,ROOM_DEATH)||(MOB_FLAGGED(m,MOB_STAY_ZONE)&&world[EXIT(m,dir)->to_room].zone!=world[IN_ROOM(m)].zone)){s->schedule_attempts++;ai_schedule_failure(m,e,now);return TRUE;}s->last_schedule_move=now;s->expected_room_vnum=dest;ai_schedule_set_state(s,AI_SCHED_TRAVELING);if(!perform_move(m,dir,1)||IN_ROOM(m)==NOWHERE||world[IN_ROOM(m)].number!=dest){s->schedule_attempts++;ai_schedule_failure(m,e,now);}else{s->schedule_attempts=0;s->schedule_failure_emitted=s->schedule_failure_applied=0;s->schedule_started_at=now;}return TRUE;}

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
  if (ai_actor_schedule_tick(mob, now)) return TRUE;
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

/* Lifecycle facts are deliberately represented by the existing bounded memory
 * entries.  State only remembers the current transition IDs, never characters. */
static void ai_actor_lifecycle_memory(struct char_data *mob, struct char_data *other,
                                      int trust, int hostility, int fear, time_t now)
{
  if (!mob || !other || !mob->ai_prof || !mob->ai_prof->memory_enabled)
    return;
  npc_ai_update_memory(mob, other, trust, hostility, fear, now);
}

void ai_actor_event_combat_end(struct char_data *actor, struct char_data *opponent, int reason)
{
  struct ai_actor_state *s;
  if (!actor || !actor->ai_state || !actor->ai_prof) return;
  s=actor->ai_state;
  if (!s->combat_active || s->combat_end_recorded) return;
  s->combat_end_recorded=TRUE; s->combat_end_reason=reason;
  if (opponent && actor->ai_prof->memory_enabled && actor->ai_prof->remember_last_room)
    ai_actor_lifecycle_memory(actor,opponent,0,0,0,time(0));
  s->combat_active=FALSE; s->combat_opponent_id=0; s->last_switch_from_id=s->last_switch_to_id=0;
}

void ai_actor_event_fled(struct char_data *actor, struct char_data *opponent, int actor_fled)
{
  if (!actor || !actor->ai_state || !actor->ai_prof) return;
  if (actor->ai_state->combat_end_recorded) return;
  if (opponent && actor->ai_prof->remember_attacks)
    ai_actor_lifecycle_memory(actor,opponent,0,actor_fled ? 1 : 2,actor_fled ? 3 : 0,time(0));
  ai_actor_event_combat_end(actor,opponent,actor_fled ? 2 : 3);
}

void ai_actor_event_defeat(struct char_data *actor, struct char_data *opponent)
{
  if (!actor || !actor->ai_prof) return;
  if (opponent && actor->ai_prof->remember_deaths)
    ai_actor_lifecycle_memory(actor,opponent,0,4,8,time(0));
  ai_actor_event_combat_end(actor,opponent,1);
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
static void ai_preview_add(char *out, size_t size, const char *fmt, ...);
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
  else if(s->type==AI_THREAT_CALL_HELP) { if(!m->threat_help_called && mob->ai_prof->may_call_help && mob->ai_prof->social!=AI_SOCIAL_SILENT){ struct ai_help_event *event=ai_help_event_new(mob,actor,mob,now); ai_social_say(mob,AI_DIALOGUE_CALL_HELP,"Help! There is danger here!",now);m->threat_help_called=TRUE;mob->ai_state->last_help_event_id=event->id;ai_actor_dispatch_help(event,mob,actor,mob,TRUE,now);} }
  else if(s->type==AI_THREAT_FLEE) { if(mob->ai_prof->social!=AI_SOCIAL_SILENT) ai_social_say(mob,AI_DIALOGUE_FEAR,"I cannot face this!",now); }
  else if(s->type==AI_THREAT_ATTACK && IN_ROOM(mob)==IN_ROOM(actor) && !FIGHTING(mob)) hit(mob,actor,0);
  if(s->type>=AI_THREAT_WARN && s->type!=AI_THREAT_IGNORE) ai_actor_schedule_interrupt(mob,AI_SCHEDULE_INTERRUPT_MAJOR_THREAT,GET_IDNUM(actor),now);
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

static void ai_actor_dispatch_help(struct ai_help_event *e, struct char_data *caller, struct char_data *target, struct char_data *victim, int relayed, time_t now) { struct char_data *mob; const char *why; int severity; if(!e||!caller||IN_ROOM(caller)==NOWHERE)return; for(mob=world[IN_ROOM(caller)].people;mob;mob=mob->next_in_room) { if(mob==caller||!npc_ai_is_humanoid_social_candidate(mob)||!mob->ai_prof)continue; if(mob->ai_prof->notice_speech && mob->ai_prof->memory_enabled && mob->ai_state->last_help_heard_event_id!=e->id) { mob->ai_state->last_help_heard_event_id=e->id; if(mob->ai_prof->remember_conversations) ai_actor_lifecycle_memory(mob,caller,1,0,0,now); } if(!mob->ai_prof->combat_enabled||!mob->ai_prof->may_assist||FIGHTING(mob)||!mob->ai_prof->notice_ally_attack||!CAN_SEE(mob,caller))continue; if(!victim||!ai_actor_is_local_ally(mob,victim,&why))continue; if(!target||!CAN_SEE(mob,target)) { if(relayed||mob->ai_prof->recognition_confidence>50)continue; } severity=ai_threat_severity(70,ai_threat_memory(mob,target?GET_IDNUM(target):0),mob->ai_prof->personality,target&&CAN_SEE(mob,target)?100:45,FALSE); if(severity<mob->ai_prof->assist_severity)continue; if(mob->ai_prof->combat_style==AI_COMBAT_PASSIVE&&severity<90)continue; if(mob->ai_prof->combat_style==AI_COMBAT_COWARDLY&&strcmp(why,"trusted memory")&&strcmp(why,"master/follower")&&strcmp(why,"group member"))continue; if(!ai_help_event_admit(e,GET_IDNUM(mob),now))continue; mob->ai_state->last_help_event_id=e->id; if(target&&IN_ROOM(target)==IN_ROOM(mob)&&!ROOM_FLAGGED(IN_ROOM(mob),ROOM_PEACEFUL)) { hit(mob,target,0); if(FIGHTING(mob)==target && mob->ai_state->last_help_answered_event_id!=e->id) { mob->ai_state->last_help_answered_event_id=e->id; if(mob->ai_prof->remember_assistance) { ai_actor_lifecycle_memory(mob,victim,2,0,0,now); ai_actor_lifecycle_memory(mob,target,0,3,0,now); } } } } }

void ai_actor_event_combat_start(struct char_data *attacker, struct char_data *victim) { struct char_data *mob; room_rnum room; time_t now=time(0); room=(attacker&&IN_ROOM(attacker)!=NOWHERE)?IN_ROOM(attacker):(victim?IN_ROOM(victim):NOWHERE); if(room==NOWHERE)return; if(attacker&&attacker->ai_state&&!attacker->ai_state->combat_active) { attacker->ai_state->combat_active=TRUE; attacker->ai_state->combat_end_recorded=FALSE; attacker->ai_state->combat_event_id++; attacker->ai_state->combat_opponent_id=victim?GET_IDNUM(victim):0; attacker->ai_state->combat_started_at=now; if(victim&&attacker->ai_prof&&attacker->ai_prof->remember_attacks) ai_actor_lifecycle_memory(attacker,victim,0,1,0,now); } if(victim&&victim->ai_state&&!victim->ai_state->combat_active) { victim->ai_state->combat_active=TRUE; victim->ai_state->combat_end_recorded=FALSE; victim->ai_state->combat_event_id++; victim->ai_state->combat_opponent_id=attacker?GET_IDNUM(attacker):0; victim->ai_state->combat_started_at=now; } for(mob=world[room].people;mob;mob=mob->next_in_room) if(npc_ai_is_humanoid_social_candidate(mob)&&mob->ai_prof&&mob->ai_prof->notice_combat) npc_ai_handle_room_danger(mob,attacker,now); { struct ai_help_event *event=ai_help_event_new(victim,attacker,victim,now); ai_actor_dispatch_help(event,victim,attacker,victim,FALSE,now); } }

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

void ai_actor_event_attack(struct char_data *attacker, struct char_data *victim, int damage) { struct char_data *mob; const char *why; if (!attacker || !victim) return; if (npc_ai_is_humanoid_social_candidate(victim) && victim->ai_prof && victim->ai_prof->notice_self_attack && victim->ai_prof->remember_attacks) ai_actor_record_damage(victim,attacker,damage); if(IN_ROOM(victim)==NOWHERE)return; for(mob=world[IN_ROOM(victim)].people;mob;mob=mob->next_in_room) if(mob!=victim&&npc_ai_is_humanoid_social_candidate(mob)&&mob->ai_prof&&mob->ai_prof->notice_ally_attack&&mob->ai_prof->remember_attacks&&ai_actor_is_local_ally(mob,victim,&why)&&CAN_SEE(mob,attacker)) ai_actor_lifecycle_memory(mob,attacker,0,3,2,time(0)); }
void ai_actor_event_crime(struct char_data *criminal, int flags) { if(!criminal||IN_ROOM(criminal)==NOWHERE)return; { struct char_data *m; for(m=world[IN_ROOM(criminal)].people;m;m=m->next_in_room) if(npc_ai_is_humanoid_social_candidate(m)&&m->ai_prof&&m->ai_prof->notice_crimes&&m->ai_prof->remember_crimes) ai_actor_record_crime(m,criminal,flags); } }

void ai_actor_schedule_reaction_speech(struct char_data *mob, struct char_data *target, const char *msg)
{
  (void)target;
  if (!npc_ai_is_humanoid_social_candidate(mob) || !msg || !*msg) return;
  do_say(mob, (char *)msg, 0, 0);
}
