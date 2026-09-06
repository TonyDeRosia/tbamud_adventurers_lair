#include "conf.h"
#include "sysdep.h"
#include "structs.h"
#include "utils.h"
#include "comm.h"
#include "db.h"
#include "handler.h"
#include "interpreter.h"
#include "genolc.h"
#include "genzon.h"
#include "oasis.h"
#include "dg_scripts.h"
#include "modify.h"
#undef ZCMD
#include <assert.h>

static int roll_value = 1;
static int rolls = 0;
static int mobile_reads = 0;
static int object_reads = 0;
static int equipped = 0;
static int given = 0;
static struct char_data mob_pool[32];
static struct obj_data obj_pool[64];
static int mob_pool_used = 0;
static int obj_pool_used = 0;

static int test_rand_number(int lo, int hi)
{
  assert(lo == 1 && hi == 100);
  rolls++;
  return roll_value;
}

static struct char_data *test_read_mobile(mob_vnum nr, int type)
{
  struct char_data *m;
  assert(mob_pool_used < 32);
  m = &mob_pool[mob_pool_used++];
  memset(m, 0, sizeof(*m));
  m->nr = nr;
  m->next = character_list;
  character_list = m;
  mob_index[nr].number++;
  mobile_reads++;
  return m;
}

static struct obj_data *test_read_object(obj_vnum nr, int type)
{
  struct obj_data *o;
  assert(obj_pool_used < 64);
  o = &obj_pool[obj_pool_used++];
  memset(o, 0, sizeof(*o));
  o->item_number = nr;
  o->in_room = NOWHERE;
  o->next = object_list;
  object_list = o;
  obj_index[nr].number++;
  object_reads++;
  return o;
}

static void test_char_to_room(struct char_data *m, room_rnum r) { m->in_room = r; }
static void test_obj_to_room(struct obj_data *o, room_rnum r) { o->in_room = r; }
static void test_obj_to_char(struct obj_data *o, struct char_data *m) { o->carried_by = m; given++; }
static void test_equip_char(struct char_data *m, struct obj_data *o, int slot) { o->worn_by = m; o->worn_on = slot; equipped++; }
static void test_load_mtrigger(struct char_data *m) {}
static void test_load_otrigger(struct obj_data *o) {}
static int test_wear_otrigger(struct obj_data *o, struct char_data *m, int slot) { return 1; }
static void test_reset_wtrigger(struct room_data *r) {}

#include "engine.inc"

static void clear_runtime(void)
{
  character_list = NULL;
  object_list = NULL;
  memset(mob_pool, 0, sizeof(mob_pool));
  memset(obj_pool, 0, sizeof(obj_pool));
  mob_pool_used = obj_pool_used = 0;
  mobile_reads = object_reads = rolls = equipped = given = 0;
  mob_index[0].number = 0;
  obj_index[0].number = 0;
}

static void terminator(struct reset_com *cmd, int at)
{
  memset(&cmd[at], 0, sizeof(cmd[at]));
  cmd[at].spawn_chance = 100;
  cmd[at].command = 'S';
}

int main(void)
{
  struct char_data unrelated;
  struct reset_com *cmd;
  struct char_data *tracked;
  int count;

  logfile = stderr;
  zone_table = calloc(1, sizeof(*zone_table));
  world = calloc(1, sizeof(*world));
  mob_index = calloc(1, sizeof(*mob_index));
  mob_proto = calloc(1, sizeof(*mob_proto));
  obj_index = calloc(1, sizeof(*obj_index));
  obj_proto = calloc(1, sizeof(*obj_proto));

  assert(zone_table && world && mob_index && mob_proto && obj_index && obj_proto);

  top_of_zone_table = 0;
  top_of_world = 0;
  top_of_mobt = 0;
  top_of_objt = 0;

  zone_table[0].number = 18;
  zone_table[0].bot = 0;
  zone_table[0].top = 0;
  world[0].number = 1879;
  world[0].zone = 0;
  world[0].name = "Test Room";
  mob_index[0].vnum = 1810;
  mob_proto[0].player.short_descr = "a wandering tree";
  obj_index[0].vnum = 1845;
  obj_proto[0].short_description = "a test object";

  assert(parse_reset_count("1", &count) && count == 1);
  assert(parse_reset_count("100", &count) && count == 100);
  assert(!parse_reset_count("0", &count));
  assert(!parse_reset_count("101", &count));

  /* Custom mob reset: unrelated same-vnum mob must NOT block this reset. */
  clear_runtime();
  zone_table[0].cmd = calloc(2, sizeof(*zone_table[0].cmd));
  cmd = zone_table[0].cmd;
  cmd[0].command = 'M';
  cmd[0].arg1 = 0;
  cmd[0].arg2 = 1;
  cmd[0].arg3 = 0;
  cmd[0].spawn_count = 1;
  cmd[0].spawn_chance = 35;
  terminator(cmd, 1);

  memset(&unrelated, 0, sizeof(unrelated));
  unrelated.nr = 0;
  unrelated.next = NULL;
  character_list = &unrelated;
  mob_index[0].number = 1;

  roll_value = 35;
  test_reset_zone(0);
  assert(mobile_reads == 1);
  assert(rolls == 1);
  tracked = character_list;
  assert(tracked != &unrelated);
  assert(tracked->mob_specials.reset_spawn_tracked);
  assert(tracked->mob_specials.reset_spawn_room == 1879);

  /* A live slot remains occupied even if the spawned mob wanders away. */
  tracked->in_room = NOWHERE;
  mobile_reads = rolls = 0;
  test_reset_zone(0);
  assert(mobile_reads == 0);
  assert(rolls == 0);

  /* Remove the tracked spawn: the vacant slot gets exactly one roll. */
  character_list = &unrelated;
  mobile_reads = rolls = 0;
  roll_value = 36;
  test_reset_zone(0);
  assert(mobile_reads == 0 && rolls == 1);
  roll_value = 35;
  test_reset_zone(0);
  assert(mobile_reads == 1 && rolls == 2);

  /* count=3 means three owned slots; 100% fills all vacancies. */
  clear_runtime();
  cmd[0].spawn_count = 3;
  cmd[0].spawn_chance = 100;
  test_reset_zone(0);
  assert(mobile_reads == 3);
  assert(rolls == 0);
  mobile_reads = 0;
  test_reset_zone(0);
  assert(mobile_reads == 0);

  /* Dependent E/G commands run for every newly spawned custom mob. */
  free(zone_table[0].cmd);
  clear_runtime();
  zone_table[0].cmd = calloc(4, sizeof(*zone_table[0].cmd));
  cmd = zone_table[0].cmd;
  cmd[0].command = 'M';
  cmd[0].arg1 = 0;
  cmd[0].arg2 = 2;
  cmd[0].arg3 = 0;
  cmd[0].spawn_count = 2;
  cmd[0].spawn_chance = 100;
  cmd[1].command = 'E';
  cmd[1].if_flag = 1;
  cmd[1].arg1 = 0;
  cmd[1].arg2 = 100;
  cmd[1].arg3 = 0;
  cmd[1].spawn_chance = 100;
  cmd[2].command = 'G';
  cmd[2].if_flag = 1;
  cmd[2].arg1 = 0;
  cmd[2].arg2 = 100;
  cmd[2].arg3 = -1;
  cmd[2].spawn_chance = 100;
  terminator(cmd, 3);

  test_reset_zone(0);
  assert(mobile_reads == 2);
  assert(equipped == 2);
  assert(given == 2);

  /* Legacy resets retain stock global-max semantics and do not roll chance. */
  free(zone_table[0].cmd);
  clear_runtime();
  zone_table[0].cmd = calloc(2, sizeof(*zone_table[0].cmd));
  cmd = zone_table[0].cmd;
  cmd[0].command = 'M';
  cmd[0].arg1 = 0;
  cmd[0].arg2 = 1;
  cmd[0].arg3 = 0;
  cmd[0].spawn_count = 0;
  cmd[0].spawn_chance = 100;
  terminator(cmd, 1);

  memset(&unrelated, 0, sizeof(unrelated));
  unrelated.nr = 0;
  character_list = &unrelated;
  mob_index[0].number = 1;
  test_reset_zone(0);
  assert(mobile_reads == 0 && rolls == 0);

  character_list = NULL;
  mob_index[0].number = 0;
  test_reset_zone(0);
  assert(mobile_reads == 1 && rolls == 0);

  /* Custom room object reset is likewise independent of global prototype count. */
  free(zone_table[0].cmd);
  clear_runtime();
  zone_table[0].cmd = calloc(2, sizeof(*zone_table[0].cmd));
  cmd = zone_table[0].cmd;
  cmd[0].command = 'O';
  cmd[0].arg1 = 0;
  cmd[0].arg2 = 1;
  cmd[0].arg3 = 0;
  cmd[0].spawn_count = 1;
  cmd[0].spawn_chance = 100;
  terminator(cmd, 1);

  obj_index[0].number = 50; /* unrelated copies exist */
  test_reset_zone(0);
  assert(object_reads == 1);
  object_reads = 0;
  test_reset_zone(0);
  assert(object_reads == 0);

  puts("PASS: per-reset count/chance slots, wandering ownership, legacy compatibility, dependent E/G.");
  return 0;
}