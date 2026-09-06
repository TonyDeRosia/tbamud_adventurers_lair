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

static int roll_values[256];
static int roll_len = 0;
static int roll_pos = 0;
static int rolls = 0;
static int mobile_reads = 0;
static int object_reads = 0;
static int equipped = 0;
static int given = 0;
static int despawned_mobs = 0;
static int despawned_objs = 0;

static struct char_data mob_pool[64];
static struct obj_data obj_pool[128];
static int mob_pool_used = 0;
static int obj_pool_used = 0;

static void set_rolls(const int *values, int count)
{
  int i;
  assert(count >= 0 && count <= (int)(sizeof(roll_values) / sizeof(roll_values[0])));
  for (i = 0; i < count; i++)
    roll_values[i] = values[i];
  roll_len = count;
  roll_pos = 0;
  rolls = 0;
}

static int test_rand_number(int lo, int hi)
{
  assert(lo == 1 && hi == 100);
  assert(roll_pos < roll_len);
  rolls++;
  return roll_values[roll_pos++];
}

static struct char_data *test_read_mobile(mob_vnum nr, int type)
{
  struct char_data *m;
  assert(mob_pool_used < (int)(sizeof(mob_pool) / sizeof(mob_pool[0])));
  m = &mob_pool[mob_pool_used++];
  memset(m, 0, sizeof(*m));
  m->nr = nr;
  m->in_room = NOWHERE;
  m->next = character_list;
  character_list = m;
  mob_index[nr].number++;
  mobile_reads++;
  return m;
}

static struct obj_data *test_read_object(obj_vnum nr, int type)
{
  struct obj_data *o;
  assert(obj_pool_used < (int)(sizeof(obj_pool) / sizeof(obj_pool[0])));
  o = &obj_pool[obj_pool_used++];
  memset(o, 0, sizeof(*o));
  o->item_number = nr;
  o->in_room = NOWHERE;
  o->worn_on = -1;
  o->next = object_list;
  object_list = o;
  obj_index[nr].number++;
  object_reads++;
  return o;
}

static void test_char_to_room(struct char_data *m, room_rnum r)
{
  m->in_room = r;
}

static void test_obj_to_room(struct obj_data *o, room_rnum r)
{
  o->in_room = r;
  o->carried_by = NULL;
  o->worn_by = NULL;
  o->in_obj = NULL;
}

static void test_obj_to_char(struct obj_data *o, struct char_data *m)
{
  o->in_room = NOWHERE;
  o->carried_by = m;
  o->worn_by = NULL;
  o->next_content = m->carrying;
  m->carrying = o;
  given++;
}

static void test_equip_char(struct char_data *m, struct obj_data *o, int slot)
{
  assert(slot >= 0 && slot < NUM_WEARS);
  o->in_room = NOWHERE;
  o->carried_by = NULL;
  o->worn_by = m;
  o->worn_on = slot;
  m->equipment[slot] = o;
  equipped++;
}

static struct obj_data *test_unequip_char(struct char_data *m, int slot)
{
  struct obj_data *o;
  assert(slot >= 0 && slot < NUM_WEARS);
  o = m->equipment[slot];
  if (!o)
    return NULL;
  m->equipment[slot] = NULL;
  o->worn_by = NULL;
  o->worn_on = -1;
  return o;
}

static void test_extract_obj(struct obj_data *o)
{
  struct obj_data **pp;

  if (!o)
    return;

  if (o->carried_by) {
    struct obj_data **cp = &o->carried_by->carrying;
    while (*cp && *cp != o)
      cp = &(*cp)->next_content;
    if (*cp == o)
      *cp = o->next_content;
  }

  if (o->worn_by && o->worn_on >= 0 && o->worn_on < NUM_WEARS &&
      o->worn_by->equipment[o->worn_on] == o)
    o->worn_by->equipment[o->worn_on] = NULL;

  pp = &object_list;
  while (*pp && *pp != o)
    pp = &(*pp)->next;
  if (*pp == o)
    *pp = o->next;

  if (o->item_number >= 0 && o->item_number <= top_of_objt &&
      obj_index[o->item_number].number > 0)
    obj_index[o->item_number].number--;

  o->in_room = NOWHERE;
  o->carried_by = NULL;
  o->worn_by = NULL;
  o->in_obj = NULL;
  o->next = NULL;
  o->next_content = NULL;
  o->item_number = NOTHING;
  despawned_objs++;
}

static void test_extract_char(struct char_data *m)
{
  struct char_data **pp;

  if (!m)
    return;

  pp = &character_list;
  while (*pp && *pp != m)
    pp = &(*pp)->next;
  if (*pp == m)
    *pp = m->next;

  if (m->nr >= 0 && m->nr <= top_of_mobt && mob_index[m->nr].number > 0)
    mob_index[m->nr].number--;

  m->next = NULL;
  m->in_room = NOWHERE;
  despawned_mobs++;
}

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
  mobile_reads = object_reads = equipped = given = 0;
  despawned_mobs = despawned_objs = 0;
  roll_len = roll_pos = rolls = 0;
  mob_index[0].number = 0;
  obj_index[0].number = 0;
}

static void terminator(struct reset_com *cmd, int at)
{
  memset(&cmd[at], 0, sizeof(cmd[at]));
  cmd[at].spawn_chance = 100;
  cmd[at].command = 'S';
}

static int tracked_mob_count(void)
{
  return count_custom_reset_mobs(18, 1879, 1810);
}

int main(void)
{
  struct char_data unrelated;
  struct reset_com *cmd;
  struct char_data *tracked;
  struct obj_data *tracked_obj;
  int count;
  int one[] = { 1 };
  int fail35[] = { 100 };
  int two_success_one_fail[] = { 10, 90, 20 };
  int three_fail[] = { 90, 90, 90 };
  int fail_two[] = { 100, 100 };
  int fail_three[] = { 100, 100, 100 };

  logfile = stderr;
  zone_table = calloc(1, sizeof(*zone_table));
  world = calloc(2, sizeof(*world));
  mob_index = calloc(1, sizeof(*mob_index));
  mob_proto = calloc(1, sizeof(*mob_proto));
  obj_index = calloc(1, sizeof(*obj_index));
  obj_proto = calloc(1, sizeof(*obj_proto));

  assert(zone_table && world && mob_index && mob_proto && obj_index && obj_proto);

  top_of_zone_table = 0;
  top_of_world = 1;
  top_of_mobt = 0;
  top_of_objt = 0;

  zone_table[0].number = 18;
  zone_table[0].bot = 0;
  zone_table[0].top = 1;

  world[0].number = 1879;
  world[0].zone = 0;
  world[0].name = "Test Spawn Room";
  world[1].number = 1880;
  world[1].zone = 0;
  world[1].name = "Wander Room";

  mob_index[0].vnum = 1810;
  mob_proto[0].player.short_descr = "a wandering tree";
  obj_index[0].vnum = 1845;
  obj_proto[0].short_description = "a test object";

  assert(parse_reset_count("1", &count) && count == 1);
  assert(parse_reset_count("100", &count) && count == 100);
  assert(!parse_reset_count("0", &count));
  assert(!parse_reset_count("101", &count));

  /* ------------------------------------------------------------------ */
  /* Custom mob: same-vnum mobs elsewhere do not block the owned slot.  */
  /* ------------------------------------------------------------------ */
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
  unrelated.in_room = 1;
  unrelated.next = NULL;
  character_list = &unrelated;
  mob_index[0].number = 1;

  set_rolls(one, 1);
  test_reset_zone(0);
  assert(mobile_reads == 1);
  assert(rolls == 1);
  tracked = character_list;
  assert(tracked != &unrelated);
  assert(tracked->mob_specials.reset_spawn_tracked);
  assert(tracked_mob_count() == 1);

  /* A wandering reset-owned mob is still rerolled and may despawn. */
  tracked->in_room = 1;
  mobile_reads = 0;
  set_rolls(fail35, 1);
  test_reset_zone(0);
  assert(rolls == 1);
  assert(mobile_reads == 0);
  assert(despawned_mobs == 1);
  assert(tracked_mob_count() == 0);

  /* ------------------------------------------------------------------ */
  /* Combat protection: failed presence rolls never remove combatants.   */
  /* ------------------------------------------------------------------ */
  set_rolls(one, 1);
  test_reset_zone(0);
  tracked = character_list;
  assert(tracked != &unrelated);
  assert(tracked_mob_count() == 1);

  tracked->char_specials.fighting = &unrelated;
  set_rolls(fail35, 1);
  test_reset_zone(0);
  assert(tracked_mob_count() == 1);
  assert(despawned_mobs == 1); /* unchanged from the earlier despawn */

  tracked->char_specials.fighting = NULL;
  unrelated.char_specials.fighting = tracked;
  set_rolls(fail35, 1);
  test_reset_zone(0);
  assert(tracked_mob_count() == 1);
  assert(despawned_mobs == 1);

  unrelated.char_specials.fighting = NULL;
  set_rolls(fail35, 1);
  test_reset_zone(0);
  assert(tracked_mob_count() == 0);
  assert(despawned_mobs == 2);

  /* ------------------------------------------------------------------ */
  /* count=3 means 3 independent presence rolls every reset.             */
  /* ------------------------------------------------------------------ */
  free(zone_table[0].cmd);
  clear_runtime();
  zone_table[0].cmd = calloc(2, sizeof(*zone_table[0].cmd));
  cmd = zone_table[0].cmd;
  cmd[0].command = 'M';
  cmd[0].arg1 = 0;
  cmd[0].arg2 = 3;
  cmd[0].arg3 = 0;
  cmd[0].spawn_count = 3;
  cmd[0].spawn_chance = 50;
  terminator(cmd, 1);

  set_rolls(two_success_one_fail, 3);
  test_reset_zone(0);
  assert(rolls == 3);
  assert(mobile_reads == 2);
  assert(tracked_mob_count() == 2);

  mobile_reads = 0;
  set_rolls(three_fail, 3);
  test_reset_zone(0);
  assert(rolls == 3);
  assert(mobile_reads == 0);
  assert(despawned_mobs == 2);
  assert(tracked_mob_count() == 0);

  /* 100% means deterministic full presence and consumes no RNG calls. */
  cmd[0].spawn_chance = 100;
  set_rolls(NULL, 0);
  test_reset_zone(0);
  assert(rolls == 0);
  assert(mobile_reads == 3);
  assert(tracked_mob_count() == 3);

  /* A failed reroll removes noncombat excess but preserves a combatant. */
  tracked = character_list;
  tracked->char_specials.fighting = &unrelated;
  cmd[0].spawn_chance = 1;
  set_rolls(fail_three, 3);
  test_reset_zone(0);
  assert(tracked_mob_count() == 1);
  assert(despawned_mobs == 4);

  tracked->char_specials.fighting = NULL;
  set_rolls(fail_three, 3);
  test_reset_zone(0);
  assert(tracked_mob_count() == 0);
  assert(despawned_mobs == 5);

  /* ------------------------------------------------------------------ */
  /* Dependent E/G applies only to newly spawned mobs; despawn drops no  */
  /* reset gear because reroll removal destroys inventory/equipment.     */
  /* ------------------------------------------------------------------ */
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

  set_rolls(NULL, 0);
  test_reset_zone(0);
  assert(mobile_reads == 2);
  assert(equipped == 2);
  assert(given == 2);

  /* Existing survivors at 100% are not re-equipped on later resets. */
  mobile_reads = equipped = given = 0;
  set_rolls(NULL, 0);
  test_reset_zone(0);
  assert(mobile_reads == 0);
  assert(equipped == 0);
  assert(given == 0);

  cmd[0].spawn_chance = 1;
  set_rolls(fail_two, 2);
  test_reset_zone(0);
  assert(despawned_mobs == 2);
  assert(despawned_objs == 4); /* two E objects + two G objects destroyed */

  /* ------------------------------------------------------------------ */
  /* Legacy reset semantics are unchanged.                               */
  /* ------------------------------------------------------------------ */
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
  unrelated.in_room = 1;
  character_list = &unrelated;
  mob_index[0].number = 1;

  set_rolls(NULL, 0);
  test_reset_zone(0);
  assert(mobile_reads == 0 && rolls == 0);

  character_list = NULL;
  mob_index[0].number = 0;
  test_reset_zone(0);
  assert(mobile_reads == 1 && rolls == 0);

  /* ------------------------------------------------------------------ */
  /* Custom room objects reroll too, but a player-taken object is        */
  /* released from reset ownership and is never deleted from inventory.  */
  /* ------------------------------------------------------------------ */
  free(zone_table[0].cmd);
  clear_runtime();
  zone_table[0].cmd = calloc(2, sizeof(*zone_table[0].cmd));
  cmd = zone_table[0].cmd;
  cmd[0].command = 'O';
  cmd[0].arg1 = 0;
  cmd[0].arg2 = 1;
  cmd[0].arg3 = 0;
  cmd[0].spawn_count = 1;
  cmd[0].spawn_chance = 35;
  terminator(cmd, 1);

  obj_index[0].number = 50; /* unrelated copies elsewhere do not matter */

  set_rolls(one, 1);
  test_reset_zone(0);
  assert(object_reads == 1);
  tracked_obj = object_list;
  assert(tracked_obj && tracked_obj->reset_spawn_tracked);

  object_reads = 0;
  set_rolls(fail35, 1);
  test_reset_zone(0);
  assert(object_reads == 0);
  assert(despawned_objs == 1);

  /* Spawn another, then simulate a player picking it up. */
  set_rolls(one, 1);
  test_reset_zone(0);
  tracked_obj = object_list;
  assert(tracked_obj && tracked_obj->reset_spawn_tracked);

  tracked_obj->in_room = NOWHERE;
  tracked_obj->carried_by = &unrelated;
  unrelated.carrying = tracked_obj;

  set_rolls(fail35, 1);
  test_reset_zone(0);
  assert(despawned_objs == 1); /* carried object was not deleted */
  assert(!tracked_obj->reset_spawn_tracked);
  assert(tracked_obj->carried_by == &unrelated);

  /* The released item no longer occupies the room reset slot. */
  object_reads = 0;
  set_rolls(one, 1);
  test_reset_zone(0);
  assert(object_reads == 1);

  puts("PASS: rerolled presence, combat-safe mob despawn, no despawn loot, object safety, legacy compatibility.");
  return 0;
}