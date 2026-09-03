/* Linked-engine regression: no mocked control, commands, events or output. */
#include "conf.h"
#include "sysdep.h"
#include "structs.h"
#include "utils.h"
#include "comm.h"
#include "interpreter.h"
#include "db.h"
#include "handler.h"
#include "fight.h"
#include "spells.h"
#include "act.h"
#include "lists.h"
#include "dg_event.h"
#include "control.h"
#include "class.h"
#include <assert.h>
static int checks;
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x); exit(1); } checks++; } while(0)
static void reset_output(struct descriptor_data *d) {
  d->bufptr=0;d->bufspace=d->large_outbuf?LARGE_BUFSIZE-1:SMALL_BUFSIZE-1;*d->output=0;
}
static struct char_data *new_body(char *name, int npc) {
  struct char_data *ch=calloc(1,sizeof(*ch));clear_char(ch);
  ch->player_specials=calloc(1,sizeof(*ch->player_specials));
  ch->player.title=strdup("");ch->player.long_descr=strdup("");ch->player.description=strdup("");
  ch->player.name=strdup(name);ch->player.short_descr=strdup(name);
  ch->events=create_list();GET_LEVEL(ch)=60;GET_POS(ch)=POS_STANDING;
  GET_HIT(ch)=GET_MAX_HIT(ch)=500;GET_MANA(ch)=GET_MAX_MANA(ch)=500;
  GET_MOVE(ch)=GET_MAX_MOVE(ch)=500;GET_INT(ch)=GET_WIS(ch)=15;
  GET_DEX(ch)=GET_STR(ch)=GET_CON(ch)=15;
  if(npc) SET_BIT_AR(MOB_FLAGS(ch),MOB_ISNPC);
  ch->next=character_list;character_list=ch;char_to_room(ch,0);return ch;
}
static struct descriptor_data *new_desc(struct char_data *ch, int id) {
  struct descriptor_data *d=calloc(1,sizeof(*d));d->desc_num=id;d->character=ch;
  STATE(d)=CON_PLAYING;d->output=d->small_outbuf;d->bufspace=SMALL_BUFSIZE-1;
  d->pProtocol=ProtocolCreate();d->events=create_list();d->login_time=time(NULL);
  strcpy(d->host,"192.0.2.42");strcpy(d->acct_name,id==1?"caster-account":"target-account");
  d->next=descriptor_list;descriptor_list=d;if(ch)ch->desc=d;return d;
}

static void run(struct char_data *ch, const char *text) {
  char buf[MAX_INPUT_LENGTH];strlcpy(buf,text,sizeof(buf));command_interpreter(ch,buf);
}
static void reset_all(void) {
  for(struct descriptor_data *d=descriptor_list;d;d=d->next)reset_output(d);
}
int main(void) {
  logfile=stderr;CONFIG_CONFFILE=strdup("missing-test-config");load_config();event_init();
  global_lists=create_list();group_list=create_list();
  world=calloc(1,sizeof(*world));top_of_world=0;world[0].number=100;
  world[0].name=strdup("Test chamber");world[0].description=strdup("Test chamber");world[0].light=1;
  zone_table=calloc(1,sizeof(*zone_table));top_of_zone_table=0;
  struct char_data *a=new_body("PlayerA",0),*b=new_body("PlayerB",0);
  struct char_data *staff=new_body("Staff",0),*staff2=new_body("StaffTwo",0),*npc=new_body("Guard",1);
  struct descriptor_data *da=new_desc(a,1),*db=new_desc(b,2),*ds=new_desc(staff,3),*ds2=new_desc(staff2,4),*dn=new_desc(npc,5);
  GET_LEVEL(a)=1;GET_LEVEL(staff)=GET_LEVEL(staff2)=LVL_IMMORT;
  a->group=b->group=calloc(1,sizeof(struct group_data));
  create_command_list();int cmd=find_command("beseech");CHECK(cmd>=0);
  CHECK(complete_cmd_info[cmd].command_pointer==do_beseech);
  CHECK(complete_cmd_info[cmd].minimum_level==1);CHECK(complete_cmd_info[cmd].minimum_position==POS_RESTING);
  run(a,"beseech");CHECK(strstr(da->output,"Beseech what?"));
  SET_BIT_AR(PRF_FLAGS(a),PRF_NOGOSS);SET_BIT_AR(PRF_FLAGS(a),PRF_NOSHOUT);
  SET_BIT_AR(PRF_FLAGS(staff),PRF_NOGOSS);SET_BIT_AR(PRF_FLAGS(staff),PRF_NOSHOUT);
  for(int pos=POS_RESTING;pos<=POS_STANDING;pos++) {
    GET_POS(a)=pos;reset_all();run(a,"beseech secret-marker-847");
    CHECK(strstr(da->output,"You beseech the immortals, 'secret-marker-847'"));
    CHECK(strstr(ds->output,"[Beseech] PlayerA beseeches: 'secret-marker-847'"));
    CHECK(strstr(ds2->output,"secret-marker-847"));CHECK(!strstr(db->output,"secret-marker-847"));
    CHECK(!strstr(dn->output,"secret-marker-847"));
  }
  for(struct char_data *ch=character_list;ch;ch=ch->next)if(!IS_NPC(ch))
    for(int h=HIST_ALL;h<=HIST_AUCTION;h++)CHECK(GET_HISTORY(ch,h)==NULL);
  reset_all();run(b,"history all");CHECK(!strstr(db->output,"secret-marker-847"));
  reset_all();run(staff,"beseech staff-appeal");CHECK(strstr(ds2->output,"staff-appeal"));
  CHECK(strstr(ds->output,"You beseech"));CHECK(!strstr(da->output,"staff-appeal"));CHECK(!strstr(db->output,"staff-appeal"));
  SET_BIT_AR(PRF_FLAGS(a),PRF_NOREPEAT);reset_all();run(a,"beseech quiet-echo");
  CHECK(!strcmp(da->output,CONFIG_OK));CHECK(strstr(ds->output,"quiet-echo"));REMOVE_BIT_AR(PRF_FLAGS(a),PRF_NOREPEAT);
  const int flags[]={PLR_NOSHOUT,PLR_FROZEN};
  for(int i=0;i<2;i++) {SET_BIT_AR(PLR_FLAGS(a),flags[i]);reset_all();run(a,"beseech blocked-marker");
    CHECK(!strstr(ds->output,"blocked-marker"));CHECK(*da->output);REMOVE_BIT_AR(PLR_FLAGS(a),flags[i]);}
  SET_BIT_AR(AFF_FLAGS(a),AFF_SILENCED);reset_all();run(a,"beseech blocked-marker");
  CHECK(!strstr(ds->output,"blocked-marker"));CHECK(strstr(da->output,"silenced"));REMOVE_BIT_AR(AFF_FLAGS(a),AFF_SILENCED);
  SET_BIT_AR(ROOM_FLAGS(0),ROOM_SOUNDPROOF);reset_all();run(a,"beseech blocked-marker");CHECK(!strstr(ds->output,"blocked-marker"));REMOVE_BIT_AR(ROOM_FLAGS(0),ROOM_SOUNDPROOF);
  GET_POS(a)=POS_SLEEPING;reset_all();run(a,"beseech blocked-marker");CHECK(!strstr(ds->output,"blocked-marker"));GET_POS(a)=POS_STANDING;
  reset_all();run(npc,"beseech blocked-marker");CHECK(!strstr(ds->output,"blocked-marker"));CHECK(strstr(dn->output,"Only players"));
  STATE(ds)=CON_PASSWORD;reset_all();run(a,"beseech state-marker");CHECK(!strstr(ds->output,"state-marker"));CHECK(strstr(ds2->output,"state-marker"));STATE(ds)=CON_PLAYING;
  /* A disconnected immortal body without a descriptor cannot receive. */
  struct char_data *offline=new_body("OfflineStaff",0);GET_LEVEL(offline)=LVL_IMMORT;
  reset_all();run(a,"beseech offline-marker");CHECK(offline->desc==NULL);CHECK(GET_HISTORY(offline,HIST_ALL)==NULL);
  GET_LEVEL(npc)=LVL_IMPL;reset_all();run(a,"beseech npc-recipient-marker");CHECK(!strstr(dn->output,"npc-recipient-marker"));
  SET_BIT_AR(PRF_FLAGS(staff),PRF_NOWIZ);SET_BIT_AR(PLR_FLAGS(staff),PLR_WRITING);
  reset_all();run(a,"beseech staff-preferences-marker");CHECK(strstr(ds->output,"staff-preferences-marker"));
  REMOVE_BIT_AR(PLR_FLAGS(staff),PLR_WRITING);
  ds->character=NULL;reset_all();run(a,"beseech missing-marker");CHECK(!strstr(ds->output,"missing-marker"));ds->character=staff;
  /* Switched staff sees their connection's appeal; the mortal body owner does not. */
  ds->original=staff;ds->character=b;reset_all();run(a,"beseech switched-marker");CHECK(strstr(ds->output,"switched-marker"));CHECK(!strstr(db->output,"switched-marker"));
  ds->original=a;ds->character=staff;reset_all();run(b,"beseech mortal-driver-marker");CHECK(!strstr(ds->output,"mortal-driver-marker"));ds->original=NULL;ds->character=staff;
  /* Even a target promoted during possession must not mirror staff messages. */
  struct control_session session={0};session.mode=CONTROL_SPELL_PUPPET;session.controller=b;session.target=staff;session.driver=db;session.observer=ds;
  staff->control=&session;b->control=&session;db->character=staff;db->original=b;
  CHECK(control_output_peer(ds)==db);reset_all();run(a,"beseech possession-secret");
  CHECK(strstr(ds->output,"possession-secret"));CHECK(!strstr(db->output,"possession-secret"));
  staff->control=NULL;b->control=NULL;db->character=b;db->original=NULL;
  reset_all();run(a,"beseech literal %s $n $N");CHECK(strstr(ds->output,"literal %s $n $N"));
  REMOVE_BIT_AR(PRF_FLAGS(a),PRF_NOGOSS);REMOVE_BIT_AR(PRF_FLAGS(a),PRF_NOSHOUT);
  CONFIG_LEVEL_CAN_SHOUT=1;reset_all();run(a,"gossip public-marker");CHECK(strstr(db->output,"public-marker"));
  reset_all();run(staff,"tell PlayerA staff-reply");CHECK(strstr(da->output,"staff-reply"));
  printf("BESEECH linked-engine checks: %d passed\n",checks);return 0;
}
