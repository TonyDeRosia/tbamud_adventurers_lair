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
 {"Mayor", mayor, LBD_ROUTINE|LBD_MOVEMENT|LBD_POSTURE|LBD_AMBIENT_SPEECH|LBD_DOORS,0,1,0,0,0},
 {"Snake", snake, LBD_COMBAT_TACTICS,0,0,0,1,2}, {"Thief", thief,LBD_COMBAT_TACTICS,0,0,0,1,2},
 {"Magic User", magic_user,LBD_COMBAT_TACTICS,0,0,0,1,2}, {"Puff", puff,LBD_AMBIENT_SPEECH,0,1,0,0,1},
 {"Fido", fido,LBD_SCAVENGING,0,1,0,0,2}, {"Janitor", janitor,LBD_SCAVENGING,0,1,0,0,2},
 {"Cityguard", cityguard,LBD_COMBAT_INIT|LBD_COMBAT_TACTICS,0,1,0,1,2},
 {"Postmaster",postmaster,LBD_SERVICE,1,0,1,0,1}, {"Receptionist",receptionist,LBD_SERVICE,1,0,1,0,1},
 {"Cryogenicist",cryogenicist,LBD_SERVICE,1,0,1,0,1}, {"Guildmaster",guild,LBD_SERVICE,1,0,1,0,1},
 {"Guild Guard",guild_guard,LBD_SERVICE|LBD_MOVEMENT,1,0,1,0,2}, {"Questmaster",questmaster,LBD_SERVICE,1,0,1,0,1},
 {"Shopkeeper",shop_keeper,LBD_SERVICE,1,0,1,0,1}, {NULL,NULL,0,0,0,0,0,0}
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
unsigned legacy_ai_domains(const struct char_data*m){const struct mob_ai_config*c=m?m->ai_config:NULL;unsigned d=0;if(!c)return 0;if(c->movement!=AI_MOVE_STATIONARY||c->schedule_enabled)d|=LBD_MOVEMENT|LBD_ROUTINE;if(c->ambient_speech_enabled)d|=LBD_AMBIENT_SPEECH;if(c->combat_enabled)d|=LBD_COMBAT_INIT|LBD_COMBAT_TACTICS;if(c->memory_enabled)d|=LBD_MEMORY;if(c->may_assist)d|=LBD_HELPER;if(c->may_flee)d|=LBD_FLEE;return d;}
int legacy_behavior_warning_count(const struct char_data*m){unsigned a=legacy_ai_domains(m);const struct legacy_special_metadata*x=m?legacy_special_metadata(GET_MOB_SPEC(m)):NULL;int n=0;if(!m||!MOB_FLAGGED(m,MOB_AI_ACTOR))return 0;if(GET_MOB_SPEC(m)&&!MOB_FLAGGED(m,MOB_SPEC))n++;if(MOB_FLAGGED(m,MOB_SPEC)&&!GET_MOB_SPEC(m))n++;if(x&&x->func==mayor&&(a&x->domains))n++;if(GET_MOB_SPEC(m)&&!x)n++;if(MOB_FLAGGED(m,MOB_SCAVENGER)||MOB_FLAGGED(m,MOB_MEMORY)||MOB_FLAGGED(m,MOB_HELPER)||MOB_FLAGGED(m,MOB_AGGRESSIVE)||MOB_FLAGGED(m,MOB_AGGR_GOOD)||MOB_FLAGGED(m,MOB_AGGR_EVIL)||MOB_FLAGGED(m,MOB_AGGR_NEUTRAL))n++;if(MOB_FLAGGED(m,MOB_SENTINEL)&&(a&LBD_MOVEMENT))n++;if(MOB_FLAGGED(m,MOB_NOSLEEP)&&m->ai_config&&m->ai_config->schedule_enabled)n++;return n;}
void legacy_behavior_summary(const struct char_data*m,char*o,size_t z,int detailed){const struct legacy_special_metadata*x;unsigned d=0,a;char ds[512];int sh,qs;struct trig_proto_list*t;if(!o||!z)return;o[0]=0;if(!m){add(o,z,"Legacy Behavior: unavailable\r\n");return;}x=legacy_special_metadata(GET_MOB_SPEC(m));sh=shop_for(m);qs=quest_count(m);if(x)d|=x->domains;if(sh>=0)d|=LBD_SERVICE;if(qs)d|=LBD_SERVICE;if(m->proto_script)d|=LBD_SCRIPT;if(MOB_FLAGGED(m,MOB_SCAVENGER))d|=LBD_SCAVENGING;if(MOB_FLAGGED(m,MOB_MEMORY))d|=LBD_MEMORY;if(MOB_FLAGGED(m,MOB_HELPER))d|=LBD_HELPER;if(MOB_FLAGGED(m,MOB_WIMPY))d|=LBD_FLEE;domains(d,ds,sizeof(ds));add(o,z,"Legacy Behavior:\r\n  Special: %s\r\n  Origin: %s\r\n  Domains: %s\r\n",GET_MOB_SPEC(m)?(x?x->name:"Unknown custom function"):"None",legacy_assignment_origin_name(origin(m)),*ds?ds:"None detected");if(sh>=0)add(o,z,"  Service: Shopkeeper (shop %d)\r\n",SHOP_NUM(sh));if(qs)add(o,z,"  Service: Questmaster (%d quest bindings)\r\n",qs);if(MOB_FLAGGED(m,MOB_GUILD_MASTER))add(o,z,"  Service: Guildmaster\r\n");if(m->proto_script){int c=0;for(t=m->proto_script;t;t=t->next)c++;add(o,z,"  DG Scripts: %d attached\r\n",c);}a=legacy_ai_domains(m);if(MOB_FLAGGED(m,MOB_AI_ACTOR)&&d&a)add(o,z,"  AI Collision: Possible (%d compatibility warnings)\r\n",legacy_behavior_warning_count(m));else add(o,z,"  AI Collision: None known\r\n");if(detailed){add(o,z,"  Runtime ordering: special procedures run before AI Actor; this report does not arbitrate.\r\n");if(x&&x->func==mayor)add(o,z,"  Mayor note: periodic route handling returns false; shared static route state can affect multiple Mayors.\r\n");if(MOB_FLAGGED(m,MOB_AI_ACTOR)&&x&&x->func==mayor&&(a&x->domains))add(o,z,"  HIGH RISK: Mayor and AI Actor can act in the same mobile pulse.\r\n");if(MOB_FLAGGED(m,MOB_AI_ACTOR)&&GET_MOB_SPEC(m)&&!x)add(o,z,"  HIGH RISK: unknown custom special overlaps AI Actor ownership.\r\n");if(MOB_FLAGGED(m,MOB_SCAVENGER))add(o,z,"  WARNING: SCAVENGER is normally suppressed by an eligible AI tick.\r\n");if(MOB_FLAGGED(m,MOB_MEMORY))add(o,z,"  WARNING: MEMORY is normally suppressed by an eligible AI tick.\r\n");if(MOB_FLAGGED(m,MOB_HELPER))add(o,z,"  WARNING: HELPER is normally suppressed by an eligible AI tick.\r\n");if(MOB_FLAGGED(m,MOB_AGGRESSIVE)||MOB_FLAGGED(m,MOB_AGGR_GOOD)||MOB_FLAGGED(m,MOB_AGGR_EVIL)||MOB_FLAGGED(m,MOB_AGGR_NEUTRAL))add(o,z,"  WARNING: aggressive legacy initiation is normally suppressed by an eligible AI tick.\r\n");if(MOB_FLAGGED(m,MOB_SENTINEL)&&(a&LBD_MOVEMENT))add(o,z,"  WARNING: SENTINEL restricts AI autonomous movement.\r\n");if(MOB_FLAGGED(m,MOB_NOSLEEP)&&m->ai_config&&m->ai_config->schedule_enabled)add(o,z,"  WARNING: NO_SLEEP can conflict with scheduled sleep.\r\n");if(MOB_FLAGGED(m,MOB_SPEC)&&!GET_MOB_SPEC(m))add(o,z,"  WARNING: MOB_SPEC is set but no function pointer is attached.\r\n");if(GET_MOB_SPEC(m)&&!MOB_FLAGGED(m,MOB_SPEC))add(o,z,"  WARNING: function pointer attached but MOB_SPEC is absent.\r\n");}}
