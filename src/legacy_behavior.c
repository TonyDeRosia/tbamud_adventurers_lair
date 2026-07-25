#include "conf.h"
#include "sysdep.h"
#include "structs.h"
#include "utils.h"
#include "db.h"
#include "shop.h"
#include "quest.h"
#include "dg_scripts.h"
#include "spec_procs.h"
#include "ai_actor.h"
#include "legacy_behavior.h"

SPECIAL(postmaster);
SPECIAL(receptionist);
SPECIAL(cryogenicist);
SPECIAL(questmaster);
SPECIAL(shop_keeper);

static const struct legacy_special_metadata special_metadata[] = {
 {"Mayor",mayor,LBD_ROUTINE|LBD_MOVEMENT|LBD_POSTURE|LBD_AMBIENT_SPEECH|LBD_DOORS,0,1,0,0,0,"Runs the classic Midgaard Mayor fixed daily routine.","Timed wake and sleep\r\n  Fixed daily route\r\n  Opens and closes city gates\r\n  Timed public speeches\r\n  Changes posture\r\n  Waits between route steps",FALSE},
 {"Snake",snake,LBD_COMBAT_TACTICS,0,0,0,1,2,"Uses the legacy poison combat special.","Poison bite during combat",FALSE},
 {"Thief",thief,LBD_COMBAT_TACTICS,0,0,0,1,2,"Uses the legacy stealing special.","Attempts to steal from nearby players",FALSE},
 {"Magic User",magic_user,LBD_COMBAT_TACTICS,0,0,0,1,2,"Casts legacy combat spells.","Selects and casts level-based combat spells",FALSE},
 {"Puff",puff,LBD_AMBIENT_SPEECH,0,1,0,0,1,"Produces Puff's legacy ambient messages.","Timed ambient speech and emotes",FALSE},
 {"Fido",fido,LBD_SCAVENGING,0,1,0,0,2,"Consumes corpses and exposes their contents.","Finds and consumes NPC corpses; drops corpse contents",FALSE},
 {"Janitor",janitor,LBD_SCAVENGING,0,1,0,0,2,"Cleans unwanted objects from rooms.","Picks up legacy trash objects",FALSE},
 {"Cityguard",cityguard,LBD_COMBAT_INIT|LBD_COMBAT_TACTICS,0,1,0,1,2,"Enforces legacy city law.","Selects criminals and assists in combat",FALSE},
 {"Postmaster",postmaster,LBD_SERVICE,1,0,1,0,1,"Provides the legacy mail service.","Mail service commands",FALSE},
 {"Receptionist",receptionist,LBD_SERVICE,1,0,1,0,1,"Provides the legacy inn service.","Rent and offer commands",FALSE},
 {"Cryogenicist",cryogenicist,LBD_SERVICE,1,0,1,0,1,"Provides the legacy cryogenic inn service.","Cryogenic rent commands",FALSE},
 {"Guildmaster",guild,LBD_SERVICE,1,0,1,0,1,"Provides legacy guild training.","Practice and training commands",FALSE},
 {"Guild Guard",guild_guard,LBD_SERVICE|LBD_MOVEMENT,1,0,1,0,2,"Guards a legacy guild entrance.","Blocks unauthorized movement",FALSE},
 {"Questmaster",questmaster,LBD_SERVICE,1,0,1,0,1,"Provides legacy quest commands.","Quest service commands",FALSE},
 {"Shopkeeper",shop_keeper,LBD_SERVICE,1,0,1,0,1,"Provides legacy shop transactions.","Buy, sell, list and value commands",FALSE},
 {NULL,NULL,0,0,0,0,0,0,NULL,NULL,FALSE}
};
static void add(char *o,size_t n,const char *f,...){va_list a;size_t u=strlen(o);if(u>=n)return;va_start(a,f);vsnprintf(o+u,n-u,f,a);va_end(a);}
const struct legacy_special_metadata *legacy_special_metadata(SPECIAL(*func)) { int i; if(!func)return NULL; for(i=0;special_metadata[i].name;i++)if(special_metadata[i].func==func)return &special_metadata[i]; return NULL; }
const char *legacy_assignment_origin_name(int o){static const char*n[]={"None","Hard-coded mobile VNUM assignment","Registered special lookup","Direct unregistered function assignment","Shop data","Quest data","Guild flag/assignment","DG prototype attachment","Unknown / runtime-assigned"};return o>=0&&o<=LAO_UNKNOWN?n[o]:n[LAO_UNKNOWN];}
static int shop_for(const struct char_data*m){int i;for(i=0;shop_index&&i<=top_shop;i++)if(SHOP_KEEPER(i)==GET_MOB_RNUM(m))return i;return -1;}
static int quest_count(const struct char_data*m){int i,n=0;for(i=0;aquest_table&&i<total_quests;i++)if(QST_MASTER(i)==GET_MOB_VNUM(m))n++;return n;}
static int origin(const struct char_data*m){SPECIAL(*f)=GET_MOB_SPEC(m);if(shop_for(m)>=0)return LAO_SHOP_DATA;if(quest_count(m))return LAO_QUEST_DATA;if(MOB_FLAGGED(m,MOB_GUILD_MASTER))return LAO_GUILD_FLAG;if(GET_MOB_VNUM(m)==3105&&f==mayor)return LAO_HARDCODED_VNUM;if(f)return legacy_special_metadata(f)?LAO_REGISTERED:LAO_DIRECT_CUSTOM;return LAO_NONE;}
static void domain_add(unsigned d, unsigned bit, const char *name, char *out, size_t size)
{ if (d & bit) add(out, size, "%s%s", *out ? ", " : "", name); }
static void domains(unsigned d,char*o,size_t n){o[0]=0;
 domain_add(d,LBD_SERVICE,"Service Commands",o,n); domain_add(d,LBD_ROUTINE,"Routine/Time",o,n);
 domain_add(d,LBD_MOVEMENT,"Movement",o,n); domain_add(d,LBD_POSTURE,"Posture",o,n);
 domain_add(d,LBD_AMBIENT_SPEECH,"Ambient Communication",o,n); domain_add(d,LBD_COMBAT_INIT,"Combat Initiation",o,n);
 domain_add(d,LBD_COMBAT_TACTICS,"Combat Tactics",o,n); domain_add(d,LBD_SCAVENGING,"Scavenging",o,n);
 domain_add(d,LBD_DOORS,"Door Interaction",o,n); domain_add(d,LBD_SCRIPT,"Script/Arbitrary",o,n);
 domain_add(d,LBD_MEMORY,"Memory",o,n); domain_add(d,LBD_HELPER,"Helper",o,n); domain_add(d,LBD_FLEE,"Fleeing",o,n); }
const char *legacy_behavior_domain_list(unsigned d,char*o,size_t n){if(!o||!n)return "";domains(d,o,n);return o;}
int legacy_behavior_script_count(const struct char_data*m){int n=0;struct trig_proto_list*t;if(!m)return 0;for(t=m->proto_script;t;t=t->next)n++;return n;}
unsigned legacy_ai_domains(const struct char_data*m){const struct mob_ai_config*c=m?m->ai_config:NULL;unsigned d=0;if(!c)return 0;if(c->movement!=AI_MOVE_STATIONARY||c->schedule_enabled)d|=LBD_MOVEMENT|LBD_ROUTINE;if(c->ambient_speech_enabled)d|=LBD_AMBIENT_SPEECH;if(c->combat_enabled)d|=LBD_COMBAT_INIT|LBD_COMBAT_TACTICS;if(c->memory_enabled)d|=LBD_MEMORY;if(c->may_assist)d|=LBD_HELPER;if(c->may_flee)d|=LBD_FLEE;return d;}
int legacy_behavior_warning_count(const struct char_data*m){unsigned a=legacy_ai_domains(m);const struct legacy_special_metadata*x=m?legacy_special_metadata(GET_MOB_SPEC(m)):NULL;int n=0;if(!m||!MOB_FLAGGED(m,MOB_AI_ACTOR))return 0;if(GET_MOB_SPEC(m)&&!MOB_FLAGGED(m,MOB_SPEC))n++;if(MOB_FLAGGED(m,MOB_SPEC)&&!GET_MOB_SPEC(m))n++;if(x&&x->func==mayor&&(a&x->domains))n++;if(GET_MOB_SPEC(m)&&!x)n++;if(MOB_FLAGGED(m,MOB_SCAVENGER)||MOB_FLAGGED(m,MOB_MEMORY)||MOB_FLAGGED(m,MOB_HELPER)||MOB_FLAGGED(m,MOB_AGGRESSIVE)||MOB_FLAGGED(m,MOB_AGGR_GOOD)||MOB_FLAGGED(m,MOB_AGGR_EVIL)||MOB_FLAGGED(m,MOB_AGGR_NEUTRAL))n++;if(MOB_FLAGGED(m,MOB_SENTINEL)&&(a&LBD_MOVEMENT))n++;if(MOB_FLAGGED(m,MOB_NOSLEEP)&&m->ai_config&&m->ai_config->schedule_enabled)n++;return n;}
void legacy_behavior_summary(const struct char_data*m,char*o,size_t z,int detailed){const struct legacy_special_metadata*x;unsigned d=0,a;char ds[512];int sh,qs;struct trig_proto_list*t;if(!o||!z)return;o[0]=0;if(!m){add(o,z,"Legacy Behavior: unavailable\r\n");return;}x=legacy_special_metadata(GET_MOB_SPEC(m));sh=shop_for(m);qs=quest_count(m);if(x)d|=x->domains;if(sh>=0)d|=LBD_SERVICE;if(qs)d|=LBD_SERVICE;if(m->proto_script)d|=LBD_SCRIPT;if(MOB_FLAGGED(m,MOB_SCAVENGER))d|=LBD_SCAVENGING;if(MOB_FLAGGED(m,MOB_MEMORY))d|=LBD_MEMORY;if(MOB_FLAGGED(m,MOB_HELPER))d|=LBD_HELPER;if(MOB_FLAGGED(m,MOB_WIMPY))d|=LBD_FLEE;domains(d,ds,sizeof(ds));add(o,z,"Legacy Behavior:\r\n  Special: %s\r\n  Origin: %s\r\n  Domains: %s\r\n",GET_MOB_SPEC(m)?(x?x->name:"Unknown custom function"):"None",legacy_assignment_origin_name(origin(m)),*ds?ds:"None detected");if(sh>=0)add(o,z,"  Service: Shopkeeper (shop %d)\r\n",SHOP_NUM(sh));if(qs)add(o,z,"  Service: Questmaster (%d quest bindings)\r\n",qs);if(MOB_FLAGGED(m,MOB_GUILD_MASTER))add(o,z,"  Service: Guildmaster\r\n");if(m->proto_script){int c=0;for(t=m->proto_script;t;t=t->next)c++;add(o,z,"  DG Scripts: %d attached\r\n",c);}a=legacy_ai_domains(m);if(MOB_FLAGGED(m,MOB_AI_ACTOR)&&d&a)add(o,z,"  AI Collision: Possible (%d compatibility warnings)\r\n",legacy_behavior_warning_count(m));else add(o,z,"  AI Collision: None known\r\n");if(detailed){add(o,z,"  Runtime ordering: special procedures run before AI Actor; compatibility-owned domains preserve old semantics.\r\n");add(o,z,"  Compatibility Mode: Legacy Preserving\r\n  Feature Flag: AI_LEGACY_ARBITRATION_ENABLED defaults FALSE; ownership is diagnostic-only until safe explicit ownership is persisted.\r\n  Configured Owner / Effective Owner / Observed Action This Pulse: Compatibility / Compatibility / runtime-only bounded diagnostics.\r\n  Domain Ownership: Movement: Compatibility, Routine: Compatibility, Posture: Compatibility, Ambient Communication: Compatibility, Combat Initiation: Compatibility, Memory Retaliation: Compatibility, Helper/Coordination: Compatibility, Scavenging: Compatibility, Fleeing: Compatibility.\r\n  Service Commands: Legacy service, outside Phase 2A\r\n");if(m->proto_script)add(o,z,"  DG Scripts: External behavior authority; not arbitrated in Phase 2A\r\n");if(x&&x->func==mayor)add(o,z,"  Mayor note: periodic route handling returns false; shared static route state can affect multiple Mayors.\r\n");if(MOB_FLAGGED(m,MOB_AI_ACTOR)&&x&&x->func==mayor&&(a&x->domains))add(o,z,"  HIGH RISK: Mayor and AI Actor can act in the same mobile pulse.\r\n");if(MOB_FLAGGED(m,MOB_AI_ACTOR)&&GET_MOB_SPEC(m)&&!x)add(o,z,"  HIGH RISK: unknown custom special overlaps AI Actor ownership.\r\n");if(MOB_FLAGGED(m,MOB_SCAVENGER))add(o,z,"  WARNING: SCAVENGER is normally suppressed by an eligible AI tick.\r\n");if(MOB_FLAGGED(m,MOB_MEMORY))add(o,z,"  WARNING: MEMORY is normally suppressed by an eligible AI tick.\r\n");if(MOB_FLAGGED(m,MOB_HELPER))add(o,z,"  WARNING: HELPER is normally suppressed by an eligible AI tick.\r\n");if(MOB_FLAGGED(m,MOB_AGGRESSIVE)||MOB_FLAGGED(m,MOB_AGGR_GOOD)||MOB_FLAGGED(m,MOB_AGGR_EVIL)||MOB_FLAGGED(m,MOB_AGGR_NEUTRAL))add(o,z,"  WARNING: aggressive legacy initiation is normally suppressed by an eligible AI tick.\r\n");if(MOB_FLAGGED(m,MOB_SENTINEL)&&(a&LBD_MOVEMENT))add(o,z,"  WARNING: SENTINEL restricts AI autonomous movement.\r\n");if(MOB_FLAGGED(m,MOB_NOSLEEP)&&m->ai_config&&m->ai_config->schedule_enabled)add(o,z,"  WARNING: NO_SLEEP can conflict with scheduled sleep.\r\n");if(MOB_FLAGGED(m,MOB_SPEC)&&!GET_MOB_SPEC(m))add(o,z,"  WARNING: MOB_SPEC is set but no function pointer is attached.\r\n");if(GET_MOB_SPEC(m)&&!MOB_FLAGGED(m,MOB_SPEC))add(o,z,"  WARNING: function pointer attached but MOB_SPEC is absent.\r\n");}}

void legacy_behavior_effective_preview(const struct char_data*m,char*o,size_t z)
{
  const struct legacy_special_metadata*x; int scripts;
  if (!o || !z) return;
  o[0] = 0;
  if (!m) { add(o,z,"Effective Mob Behavior: unavailable\r\n"); return; }
  x=legacy_special_metadata(GET_MOB_SPEC(m));scripts=legacy_behavior_script_count(m);
  add(o,z,"Effective Mob Behavior\r\n----------------------\r\n\r\nLegacy Sources\r\n\r\nNPC Flags:\r\n");
  if(MOB_FLAGGED(m,MOB_SENTINEL))add(o,z,"  SENTINEL\r\n");
  if(MOB_FLAGGED(m,MOB_STAY_ZONE))add(o,z,"  STAY_ZONE\r\n");
  if(MOB_FLAGGED(m,MOB_MEMORY))add(o,z,"  MEMORY\r\n");
  if(MOB_FLAGGED(m,MOB_HELPER))add(o,z,"  HELPER\r\n");
  if(MOB_FLAGGED(m,MOB_WIMPY))add(o,z,"  WIMPY\r\n");
  if(MOB_FLAGGED(m,MOB_SCAVENGER))add(o,z,"  SCAVENGER\r\n");
  if(MOB_FLAGGED(m,MOB_NOCHARM))add(o,z,"  NO_CHARM\r\n");
  if(MOB_FLAGGED(m,MOB_NOSUMMON))add(o,z,"  NO_SUMMN\r\n");
  add(o,z,"\r\nSpecial Procedure:\r\n  %s\r\n\r\nDG Scripts:\r\n  %s",GET_MOB_SPEC(m)?(x?x->name:"Unknown custom function"):"None",scripts?"Attached":"None");
  if(scripts)add(o,z," (%d)",scripts);
  add(o,z,"\r\n\r\nEffective Capabilities:\r\n  %s\r\n",MOB_FLAGGED(m,MOB_SENTINEL)?"Does not wander randomly":"May wander through legacy mobile activity");
  if(MOB_FLAGGED(m,MOB_MEMORY))add(o,z,"  Remembers attackers through the NPC MEMORY flag\r\n");
  if(MOB_FLAGGED(m,MOB_HELPER))add(o,z,"  Helps allies through the NPC HELPER flag\r\n");
  if(MOB_FLAGGED(m,MOB_WIMPY))add(o,z,"  May flee through the NPC WIMPY flag\r\n");
  if(MOB_FLAGGED(m,MOB_SCAVENGER))add(o,z,"  Scavenges through the NPC SCAVENGER flag\r\n");
  if(x)add(o,z,"  %s\r\n",x->capabilities);
  if(MOB_FLAGGED(m,MOB_NOCHARM))add(o,z,"  Cannot be charmed\r\n");
  if(MOB_FLAGGED(m,MOB_NOSUMMON))add(o,z,"  Cannot be summoned\r\n");
  add(o,z,"\r\nAI Actor Extensions:\r\n  %s\r\n\r\nWarnings:\r\n  %s\r\n",MOB_FLAGGED(m,MOB_AI_ACTOR)?"Enabled (legacy-owned domains remain authoritative)":"Disabled",legacy_behavior_warning_count(m)?"See Diagnostics and Sources":"None");
}

const char *mob_behavior_owner_name(int owner)
{
  static const char *names[] = { "Compatibility", "Legacy", "AI", "Disabled" };
  return owner >= MOB_BEHAVIOR_OWNER_COMPATIBILITY && owner <= MOB_BEHAVIOR_OWNER_DISABLED ? names[owner] : "Compatibility";
}

struct behavior_domain_definition { unsigned mask; const char *display; const char *token; int editable; };
static const struct behavior_domain_definition behavior_domains[MOB_BEHAVIOR_DOMAIN_COUNT] = {
 {LBD_SERVICE,"Service Commands","Service",FALSE}, {LBD_ROUTINE,"Routine","Routine",TRUE},
 {LBD_MOVEMENT,"Movement","Movement",TRUE}, {LBD_POSTURE,"Posture","Posture",TRUE},
 {LBD_AMBIENT_SPEECH,"Ambient Communication","AmbientCommunication",TRUE},
 {LBD_COMBAT_INIT,"Combat Initiation","CombatInitiation",TRUE},
 {LBD_COMBAT_TACTICS,"Combat Tactics","CombatTactics",FALSE},
 {LBD_SCAVENGING,"Scavenging","Scavenging",TRUE}, {LBD_DOORS,"Door Interaction","Doors",FALSE},
 {LBD_SCRIPT,"DG Scripts","DGScript",FALSE}, {LBD_MEMORY,"Memory Retaliation","MemoryRetaliation",TRUE},
 {LBD_HELPER,"Helper/Coordination","HelperCoordination",TRUE}, {LBD_FLEE,"Fleeing","Fleeing",TRUE}
};
static const unsigned editable_domains[MOB_BEHAVIOR_EDITABLE_DOMAIN_COUNT] = {
 LBD_ROUTINE,LBD_MOVEMENT,LBD_POSTURE,LBD_AMBIENT_SPEECH,LBD_COMBAT_INIT,LBD_MEMORY,LBD_HELPER,LBD_SCAVENGING,LBD_FLEE
};
_Static_assert(sizeof(behavior_domains)/sizeof(behavior_domains[0]) == MOB_BEHAVIOR_DOMAIN_COUNT, "domain table/count mismatch");
_Static_assert(sizeof(editable_domains)/sizeof(editable_domains[0]) == MOB_BEHAVIOR_EDITABLE_DOMAIN_COUNT, "editable table/count mismatch");
const char *mob_behavior_domain_name(unsigned domain) { int i=mob_behavior_domain_index(domain); return i>=0?behavior_domains[i].display:"Unknown"; }
const char *mob_behavior_domain_token(unsigned domain) { int i=mob_behavior_domain_index(domain); return i>=0?behavior_domains[i].token:NULL; }
unsigned mob_behavior_editable_domain(unsigned index) { return index<MOB_BEHAVIOR_EDITABLE_DOMAIN_COUNT?editable_domains[index]:0; }

int mob_behavior_domain_index(unsigned domain)
{
  int i;
  for (i = 0; i < MOB_BEHAVIOR_DOMAIN_COUNT; i++)
    if (domain == (1U << i))
      return i;
  return -1;
}

void mob_behavior_context_init(struct mob_behavior_pulse_context *ctx, struct char_data *mob, int enabled)
{
  int i;
  const struct legacy_special_metadata *meta;
  if (!ctx)
    return;
  memset(ctx, 0, sizeof(*ctx));
  ctx->mob = mob;
  ctx->compatibility_mode = MOB_BEHAVIOR_COMPAT_LEGACY_PRESERVING;
  ctx->arbitration_enabled = enabled;
  for (i = 0; i < MOB_BEHAVIOR_DOMAIN_COUNT; i++)
    ctx->configured_owner[i] = ctx->effective_owner[i] = MOB_BEHAVIOR_OWNER_COMPATIBILITY;
  if (!mob)
    return;
  if (mob->ai_config) for (i = 0; i < MOB_BEHAVIOR_DOMAIN_COUNT; i++)
    ctx->configured_owner[i] = ctx->effective_owner[i] = mob->ai_config->behavior_owner[i];
  meta = legacy_special_metadata(GET_MOB_SPEC(mob));
  if (meta)
    ctx->legacy_special_domains = meta->domains;
  ctx->ai_configured_domains = legacy_ai_domains(mob);
  if (MOB_FLAGGED(mob, MOB_SCAVENGER)) ctx->legacy_tail_domains |= LBD_SCAVENGING;
  if (!MOB_FLAGGED(mob, MOB_SENTINEL)) ctx->legacy_tail_domains |= LBD_MOVEMENT;
  if (MOB_FLAGGED(mob, MOB_MEMORY)) ctx->legacy_tail_domains |= LBD_MEMORY | LBD_COMBAT_INIT;
  if (MOB_FLAGGED(mob, MOB_HELPER)) ctx->legacy_tail_domains |= LBD_HELPER | LBD_COMBAT_INIT;
  if (MOB_FLAGGED(mob, MOB_AGGRESSIVE) || MOB_FLAGGED(mob, MOB_AGGR_GOOD) || MOB_FLAGGED(mob, MOB_AGGR_EVIL) || MOB_FLAGGED(mob, MOB_AGGR_NEUTRAL)) ctx->legacy_tail_domains |= LBD_COMBAT_INIT;
  if (MOB_FLAGGED(mob, MOB_WIMPY)) ctx->legacy_tail_domains |= LBD_FLEE;
  if (GET_MOB_SPEC(mob) && !meta && enabled) {
    ctx->unknown_special_locks = MOB_BEHAVIOR_PHASE2A_DOMAINS;
    for (i = 0; i < MOB_BEHAVIOR_DOMAIN_COUNT; i++) if (ctx->unknown_special_locks & (1U << i)) {
      ctx->effective_owner[i] = MOB_BEHAVIOR_OWNER_LEGACY;
      snprintf(ctx->lock_reason[i], sizeof(ctx->lock_reason[i]), "Unknown custom special: conservative legacy lock");
    }
  }
  if (MOB_FLAGGED(mob, MOB_NOSLEEP) && mob->ai_config && mob->ai_config->schedule_enabled) {
    i = mob_behavior_domain_index(LBD_POSTURE);
    snprintf(ctx->lock_reason[i], sizeof(ctx->lock_reason[i]), "NO_SLEEP conflict diagnosed; compatibility preserving");
  }
}

int mob_behavior_context_has_explicit_owner(const struct mob_behavior_pulse_context *ctx)
{
  int i;
  if (!ctx) return FALSE;
  for (i = 0; i < MOB_BEHAVIOR_DOMAIN_COUNT; i++)
    if (ctx->configured_owner[i] != MOB_BEHAVIOR_OWNER_COMPATIBILITY)
      return TRUE;
  return FALSE;
}


unsigned mob_behavior_domain_from_token(const char *token)
{
  char buf[64]; int i, j=0;
  if (!token || !*token) return 0;
  for (i=0; token[i] && j < (int)sizeof(buf)-1; i++)
    if (isalnum((unsigned char)token[i])) buf[j++] = LOWER(token[i]);
  if (token[i]) return 0;
  buf[j]=0;
  for (i=0; i<MOB_BEHAVIOR_DOMAIN_COUNT; i++) {
    char canonical[64]; int k, n=0;
    for (k=0; behavior_domains[i].token[k]; k++) canonical[n++]=LOWER(behavior_domains[i].token[k]);
    canonical[n]=0;
    if (!str_cmp(buf,canonical)) return behavior_domains[i].mask;
  }
  return 0;
}

int mob_behavior_owner_from_token(const char *token, enum mob_behavior_owner *owner)
{
  char buf[32]; int i, j=0;
  if (!token || !owner) return FALSE;
  for (i=0; token[i] && j < (int)sizeof(buf)-1; i++)
    if (isalnum((unsigned char)token[i])) buf[j++] = LOWER(token[i]);
  buf[j]=0;
  if (!str_cmp(buf, "compatibility") || !str_cmp(buf, "compat")) *owner = MOB_BEHAVIOR_OWNER_COMPATIBILITY;
  else if (!str_cmp(buf, "legacy")) *owner = MOB_BEHAVIOR_OWNER_LEGACY;
  else if (!str_cmp(buf, "ai")) *owner = MOB_BEHAVIOR_OWNER_AI;
  else if (!str_cmp(buf, "disabled") || !str_cmp(buf, "disable")) *owner = MOB_BEHAVIOR_OWNER_DISABLED;
  else return FALSE;
  return TRUE;
}

static int domain_available(const struct mob_behavior_pulse_context *ctx, unsigned domain, int ai)
{
  int i = mob_behavior_domain_index(domain);
  enum mob_behavior_owner owner;
  if (!ctx || !ctx->arbitration_enabled || i < 0) return TRUE;
  owner = ctx->effective_owner[i];
  if (owner == MOB_BEHAVIOR_OWNER_DISABLED) return FALSE;
  if (ai && owner == MOB_BEHAVIOR_OWNER_LEGACY) return FALSE;
  if (!ai && owner == MOB_BEHAVIOR_OWNER_AI) return FALSE;
  return TRUE;
}
int mob_behavior_domain_available_to_ai(const struct mob_behavior_pulse_context *ctx, unsigned domain) { return domain_available(ctx, domain, TRUE); }
int mob_behavior_domain_available_to_legacy_tail(const struct mob_behavior_pulse_context *ctx, unsigned domain) { return domain_available(ctx, domain, FALSE); }
void mob_behavior_mark_action(struct mob_behavior_action_result *r, unsigned domain) { if (r) r->domains_acted |= domain; }

#define RECENT_PULSES 32
static struct { long id; int vnum; struct mob_behavior_pulse_context ctx; } recent[RECENT_PULSES];
static int recent_pos;
void mob_behavior_record_recent_pulse(struct char_data *mob, const struct mob_behavior_pulse_context *ctx)
{ if (!mob || !ctx) return; recent[recent_pos].id = (long)(intptr_t)mob; recent[recent_pos].vnum = GET_MOB_VNUM(mob); recent[recent_pos].ctx = *ctx; recent_pos = (recent_pos + 1) % RECENT_PULSES; }
void mob_behavior_recent_pulse_report(const struct char_data *mob, char *out, size_t size)
{
  int i; if (!out || !size) return; out[0] = 0; if (!mob) { add(out,size,"Recent Phase 2A pulse: unavailable\r\n"); return; }
  for (i = 0; i < RECENT_PULSES; i++) if (recent[i].id == (long)(intptr_t)mob) {
    add(out, size, "Recent Phase 2A pulse:\r\n  Special returned TRUE: %s\r\n  AI result: %s\r\n  Legacy-tail domains acted: %u\r\n  Domains blocked from AI: %u\r\n  Domains blocked from legacy tail: %u\r\n  Compatibility short-circuit: %s\r\n",
        recent[i].ctx.special_result.returned_true ? "yes" : "no",
        recent[i].ctx.ai_result.consumed_pulse ? "pulse consumed" : (recent[i].ctx.ai_result.domains_acted ? "action performed" : (recent[i].ctx.ai_result.domains_blocked ? "blocked" : "no action")),
        recent[i].ctx.legacy_tail_result.domains_acted, recent[i].ctx.unavailable_to_ai, recent[i].ctx.unavailable_to_legacy_tail,
        *recent[i].ctx.compatibility_short_circuit ? recent[i].ctx.compatibility_short_circuit : "none"); return; }
  add(out, size, "Recent Phase 2A pulse: no bounded transient record for this live NPC.\r\n");
}
int mob_behavior_mayor_ai_ownership_supported(const struct char_data *mob, unsigned domain, char *why, size_t why_size)
{
  const struct legacy_special_metadata *meta = mob ? legacy_special_metadata(GET_MOB_SPEC(mob)) : NULL;
  if (meta && meta->func == mayor && (domain & meta->domains)) {
    if (why && why_size) snprintf(why, why_size, "AI ownership unavailable: legacy Mayor special must be migrated first.");
    return FALSE;
  }
  return TRUE;
}
