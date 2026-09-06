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
static struct char_data *actor, *target, *guard, *bystander;
static struct descriptor_data *driver, *observer;
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
static int start(int pc, enum control_mode mode, int seconds) {
  reset_output(driver);reset_output(observer);
  GET_LEVEL(actor)=mode==CONTROL_IMMORTAL_PUPPET?LVL_IMPL:60;
  return start_control_session(actor,pc?target:guard,mode,seconds);
}
static void restored(void) {
  CHECK(actor->control==NULL);CHECK(target->control==NULL);CHECK(guard->control==NULL);
  CHECK(driver->character==actor && driver->original==NULL && actor->desc==driver);
  CHECK(observer->character==target && target->desc==observer);
  CHECK(guard->desc==NULL);CHECK(!strcmp(driver->acct_name,"caster-account"));
  CHECK(!strcmp(observer->acct_name,"target-account"));
}
static void input(struct descriptor_data *d,const char *text) {
  char buf[MAX_INPUT_LENGTH];strlcpy(buf,text,sizeof(buf));CHECK(control_input(d,buf));
}
int main(void) {
  logfile=stderr;circle_srandom(12345);
  CONFIG_CONFFILE=strdup("missing-test-config");load_config();event_init();
  global_lists=create_list();group_list=create_list();
  mob_index=calloc(1,sizeof(*mob_index));top_of_mobt=0;mob_index[0].vnum=60000;
  world=calloc(2,sizeof(*world));top_of_world=1;
  zone_table=calloc(1,sizeof(*zone_table));top_of_zone_table=0;
  for(int i=0;i<2;i++) {world[i].number=100+i;world[i].name=strdup(i?"Second chamber":"First chamber");
    world[i].description=strdup("A quiet testing chamber.\r\n");world[i].light=1;}
  world[0].dir_option[NORTH]=calloc(1,sizeof(struct room_direction_data));
  world[0].dir_option[NORTH]->to_room=1;
  world[1].dir_option[SOUTH]=calloc(1,sizeof(struct room_direction_data));
  world[1].dir_option[SOUTH]->to_room=0;
  actor=new_body("caster",0);target=new_body("target",0);guard=new_body("guard",1);
  bystander=new_body("bystander",0);
  driver=new_desc(actor,1);observer=new_desc(target,2);
  mag_assign_spells();init_spell_levels();create_command_list();
  CHECK(find_command("puppet")>=0);CHECK(find_command("unpuppet")>=0);CHECK(find_command("socket")>=0);
  CHECK(!strcmp(skill_name(SPELL_PUPPET),"puppet"));CHECK(!strcmp(skill_name(SPELL_MIND_CONTROL),"mind control"));
  CHECK(spell_info[SPELL_PUPPET].violent && spell_info[SPELL_MIND_CONTROL].violent);
  CHECK(start(0,CONTROL_IMMORTAL_PUPPET,0));CHECK(driver->character==guard);CHECK(IN_ROOM(actor)==0);
  CHECK(GET_LEVEL(guard)==60);CHECK(!actor->desc);CHECK(!actor->control->expiry);
  CHECK(!start_control_session(actor,target,CONTROL_SPELL_PUPPET,30));
  CHECK(!start_control_session(guard,target,CONTROL_IMMORTAL_PUPPET,0));
  input(driver,"north");CHECK(IN_ROOM(guard)==1 && IN_ROOM(actor)==0);
  reset_output(driver);input(driver,"look");CHECK(strstr(driver->output,"Second chamber"));
  input(driver,"south");CHECK(IN_ROOM(guard)==0);
  reset_output(observer);input(driver,"say greetings");CHECK(strstr(observer->output,"Guard"));CHECK(strstr(observer->output,"greetings"));
  reset_output(observer);input(driver,"emote bows");CHECK(strstr(observer->output,"Guard bows"));
  input(driver,"mload mob 1");CHECK(driver->character==guard);CHECK(strstr(driver->output,"cannot be compelled"));
  input(driver,"unpuppet");restored();
  for(int mode=CONTROL_SPELL_PUPPET;mode<=CONTROL_MIND_CONTROL;mode++) for(int pc=0;pc<=1;pc++) {
    struct char_data *body=pc?target:guard;
    int max=pc?30:mode==CONTROL_MIND_CONTROL?90:60;
    CHECK(!start(pc,mode,max+1));CHECK(start(pc,mode,max));
    CHECK(event_time(actor->control->expiry)==max*PASSES_PER_SEC);
    CHECK(body->control==actor->control);CHECK(IN_ROOM(actor)==0);
    CHECK(body->master==NULL && actor->followers==NULL);
    CHECK(mode==CONTROL_MIND_CONTROL?driver->character==actor:driver->character==body);
    if(pc) {reset_output(driver);reset_output(observer);send_to_char(target,"body-output-marker\r\n");
      CHECK(strstr(observer->output,"body-output-marker"));
      CHECK((strstr(driver->output,"body-output-marker")!=NULL)==(mode==CONTROL_SPELL_PUPPET));
      reset_output(driver);STATE(observer)=CON_PASSWORD;
      send_to_char(target,"test-private-menu-marker\r\n");CHECK(!strstr(driver->output,"test-private-menu-marker"));
      STATE(observer)=CON_PLAYING;
      input(observer,"north");CHECK(IN_ROOM(target)==0);}
    actor->control->executing++;
    const char *blocked[]={"password","account","delete","quit","rent","save","give","drop","junk","sacrifice","sell","withdraw","mail","trade","set","force","shutdown","purge","advance","cast","order","alias","mload","socket","switch","puppet",NULL};
    for(int i=0;blocked[i];i++) CHECK(!control_command_allowed(body,blocked[i]));
    const char *allowed[]={"n","look","scan","exits","say","emote","kill","kick","bash","flee",NULL};
    for(int i=0;allowed[i];i++) CHECK(control_command_allowed(body,allowed[i]));
    actor->control->executing--;
    CHECK(!control_command_allowed(body,"north"));
    CONFIG_PK_ALLOWED=0;CHECK(!control_attack_allowed(body,bystander));
    SET_BIT_AR(ROOM_FLAGS(IN_ROOM(body)),ROOM_PEACEFUL);
    CHECK(control_attack_allowed(body,body));
    REMOVE_BIT_AR(ROOM_FLAGS(IN_ROOM(body)),ROOM_PEACEFUL);
    if(!pc) {
      CHECK(perform_kick(body,target,100,0,0)==COMBAT_SKILL_NOT_ATTEMPTED);
      CHECK(perform_bash(body,target,100,0,0)==COMBAT_SKILL_NOT_ATTEMPTED);
    }
    CONFIG_PK_ALLOWED=1;
    CHECK(!control_attack_allowed(body,actor));
    if(mode==CONTROL_MIND_CONTROL) {
      GET_WAIT_STATE(actor)=GET_WAIT_STATE(body)=0;actor->control->ready_at=pulse;
      char order[80];snprintf(order,sizeof(order),"%s north",GET_NAME(body));
      CHECK(control_order(actor,order));CHECK(IN_ROOM(body)==1 && IN_ROOM(actor)==0);
      char_from_room(body);char_to_room(body,0);
    }
    pulse+=max*PASSES_PER_SEC;event_process();restored();
  }
  for(int pc=0;pc<=1;pc++) for(int endpoint=0;endpoint<4;endpoint++) {
    CHECK(start(pc,CONTROL_SPELL_PUPPET,30));
    write_to_q("drop all",&driver->input,0);write_to_q("north",&observer->input,0);
    switch(endpoint) {case 0:end_character_control(actor);break;case 1:end_character_control(pc?target:guard);break;
      case 2:end_descriptor_control(driver);break;default:if(pc)end_descriptor_control(observer);else end_all_control();}
    restored();CHECK(driver->input.head==NULL);CHECK(!pc || observer->input.head==NULL);
    end_all_control();restored();
  }
  CHECK(start(1,CONTROL_SPELL_PUPPET,30));
  CONFIG_PK_ALLOWED=0;CHECK(damage(guard,actor,100,TYPE_HIT)>=0); /* caster body damage breaks possession */
  restored();
  if(FIGHTING(actor))stop_fighting(actor);
  if(FIGHTING(guard))stop_fighting(guard);
  GET_POS(actor)=GET_POS(guard)=GET_POS(target)=POS_STANDING;
  GET_CLASS(actor)=CLASS_MAGIC_USER;GET_LEVEL(actor)=80;GET_LEVEL(guard)=20;GET_LEVEL(target)=20;
  GET_SKILL(actor,SPELL_PUPPET)=100;GET_SKILL(actor,SPELL_MIND_CONTROL)=100;
  CHECK(saving_throw_base_chance(CLASS_MAGIC_USER, SAVING_SPELL, 100) >
        saving_throw_base_chance(CLASS_MAGIC_USER, SAVING_SPELL, 40));
  for(int spell=SPELL_PUPPET;spell<=SPELL_MIND_CONTROL;spell++) {
    SET_BIT_AR(ROOM_FLAGS(0),ROOM_PEACEFUL);cast_control(actor,guard,spell);CHECK(!actor->control);
    REMOVE_BIT_AR(ROOM_FLAGS(0),ROOM_PEACEFUL);
    int flags[]={MOB_NOCHARM,MOB_NOKILL,MOB_SPEC};
    for(int i=0;i<3;i++) {SET_BIT_AR(MOB_FLAGS(guard),flags[i]);cast_control(actor,guard,spell);CHECK(!actor->control);REMOVE_BIT_AR(MOB_FLAGS(guard),flags[i]);}
    SET_BIT_AR(AFF_FLAGS(guard),AFF_SANCTUARY);cast_control(actor,guard,spell);CHECK(!actor->control);REMOVE_BIT_AR(AFF_FLAGS(guard),AFF_SANCTUARY);
    SET_BIT_AR(AFF_FLAGS(guard),AFF_CHARM);cast_control(actor,guard,spell);CHECK(!actor->control);REMOVE_BIT_AR(AFF_FLAGS(guard),AFF_CHARM);
    CONFIG_PK_ALLOWED=0;cast_control(actor,target,spell);CHECK(!actor->control);CONFIG_PK_ALLOWED=1;
    REMOVE_BIT_AR(PRF_FLAGS(target),PRF_SUMMONABLE);cast_control(actor,target,spell);CHECK(!actor->control);
    SET_BIT_AR(PRF_FLAGS(target),PRF_SUMMONABLE);
    GET_LEVEL(guard)=GET_LEVEL(actor)+6;cast_control(actor,guard,spell);CHECK(!actor->control);GET_LEVEL(guard)=20;
    for(int pc=0;pc<=1;pc++) for(int proficiency=10;proficiency<=100;proficiency+=90) {
      struct char_data *body=pc?target:guard;int wins=0,losses=0;
      GET_SKILL(actor,spell)=proficiency;
      for(int trial=0;trial<100;trial++) {
        if(FIGHTING(actor))stop_fighting(actor);
  if(FIGHTING(body))stop_fighting(body);
        GET_POS(actor)=GET_POS(body)=POS_STANDING;reset_output(driver);reset_output(observer);
        cast_control(actor,body,spell);
        if(actor->control) {
          int max=pc?30:spell==SPELL_PUPPET?60:90;
          int seconds=pc?3+27*proficiency/100:spell==SPELL_PUPPET?10+50*proficiency/100:15+75*proficiency/100;
          CHECK(event_time(actor->control->expiry)==seconds*PASSES_PER_SEC);
          CHECK(event_time(actor->control->expiry)<=max*PASSES_PER_SEC);wins++;end_character_control(actor);
        } else losses++;
      }
      CHECK(wins>0 && losses>0);
    }
  }
  if(FIGHTING(actor))stop_fighting(actor);
  if(FIGHTING(guard))stop_fighting(guard);
  if(FIGHTING(target))stop_fighting(target);
  GET_POS(actor)=GET_POS(guard)=POS_STANDING;GET_SKILL(actor,SPELL_PUPPET)=100;
  GET_WAIT_STATE(actor)=0;GET_MANA(actor)=500;reset_output(driver);
  char cast[]="cast 'puppet' guard";command_interpreter(actor,cast);
  CHECK(GET_MANA(actor)<500);CHECK(GET_WAIT_STATE(actor)>0);end_character_control(actor);
  if(FIGHTING(actor))stop_fighting(actor);
  if(FIGHTING(guard))stop_fighting(guard);
  GET_POS(actor)=GET_POS(guard)=POS_STANDING;
  CHECK(start(0,CONTROL_SPELL_PUPPET,30));extract_char(guard);restored();REMOVE_BIT_AR(MOB_FLAGS(guard),MOB_NOTDEADYET);
  GET_LEVEL(actor)=LVL_IMPL;reset_output(driver);do_socket(actor,"",0,0);
  CHECK(strstr(driver->output,"[redacted]"));CHECK(!strstr(driver->output,"192.0.2.42"));
  reset_output(driver);do_socket(actor,"2 full",0,0);CHECK(strstr(driver->output,"192.0.2.42"));
  CHECK(!strstr(driver->output,"0x"));
  struct descriptor_data *login=new_desc(NULL,3); /* set up separately below */
  STATE(login)=CON_GET_NAME;reset_output(driver);do_socket(actor,"3",0,0);CHECK(strstr(driver->output,"(login)"));
  GET_LEVEL(actor)=60;reset_output(driver);do_socket(actor,"full",0,0);CHECK(!strstr(driver->output,"192.0.2.42"));
  GET_POS(actor)=GET_POS(target)=GET_POS(guard)=POS_STANDING;
  CHECK(start(1,CONTROL_SPELL_PUPPET,30));
  struct descriptor_data *staff=new_desc(new_body("staff",0),4);GET_LEVEL(staff->character)=LVL_IMPL;
  do_socket(staff->character,"1",0,0);CHECK(strstr(staff->output,"Controller: caster"));CHECK(strstr(staff->output,"Controlled body: target"));
  CHECK(!strstr(staff->output,"192.0.2.42"));
  SET_BIT_AR(PLR_FLAGS(actor),PLR_FROZEN);input(driver,"north");CHECK(IN_ROOM(target)==0);
  input(driver,"unpuppet");restored();REMOVE_BIT_AR(PLR_FLAGS(actor),PLR_FROZEN);
  CHECK(start(0,CONTROL_IMMORTAL_PUPPET,0));raw_kill(guard,NULL);restored();REMOVE_BIT_AR(MOB_FLAGS(guard),MOB_NOTDEADYET);
  for(int which=0;which<2;which++) {
    int pair[2];CHECK(socketpair(AF_UNIX,SOCK_STREAM,0,pair)==0);
    GET_POS(actor)=GET_POS(target)=POS_STANDING;
    CHECK(start(1,CONTROL_SPELL_PUPPET,30));
    struct descriptor_data *closed=which?observer:driver;closed->descriptor=pair[0];
    close_socket(closed);close(pair[1]);
    CHECK(!actor->control && !target->control);
    if(which) {CHECK(target->desc==NULL && driver->character==actor);observer=new_desc(target,2);}
    else {CHECK(actor->desc==NULL && target->desc==observer);driver=new_desc(actor,1);}
    restored();
  }
  GET_POS(actor)=GET_POS(target)=POS_STANDING;CHECK(start(1,CONTROL_SPELL_PUPPET,30));
  struct char_data *temp;
  char_from_room(actor);REMOVE_FROM_LIST(actor,character_list,next);free_char(actor);
  CHECK(driver->character==NULL);CHECK(target->control==NULL && target->desc==observer);
  end_all_control();
  printf("%d linked-engine assertions passed\n",checks);
  return 0;
}
