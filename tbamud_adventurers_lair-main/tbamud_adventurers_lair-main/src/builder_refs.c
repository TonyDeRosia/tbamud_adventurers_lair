#include "conf.h"
#include "sysdep.h"
#include "structs.h"
#include "utils.h"
#include "comm.h"
#include "db.h"
#include "shop.h"
#include "dg_scripts.h"
#include "genzon.h"
#include "builder_refs.h"

struct indexed_ref { enum builder_ref_type target_type; int target_vnum; struct builder_reference ref; };
static struct indexed_ref *index_entries;
static size_t index_count, index_capacity;
static int index_valid;
static unsigned long generation = 1;

static const char *safe(const char *s) { return s && *s ? s : "(unnamed)"; }
static void add(enum builder_ref_type target_type, int target, enum builder_ref_type source_type,
                int source, const char *name, const char *relation, int location, const char *origin)
{
  size_t i;
  struct indexed_ref *e;
  for (i = 0; i < index_count; i++)
    if (index_entries[i].target_type == target_type && index_entries[i].target_vnum == target &&
        index_entries[i].ref.type == source_type && index_entries[i].ref.vnum == source &&
        index_entries[i].ref.location_vnum == location &&
        !strcmp(index_entries[i].ref.relationship, relation)) return;
  if (index_count == index_capacity) {
    index_capacity = index_capacity ? index_capacity * 2 : 256;
    RECREATE(index_entries, struct indexed_ref, index_capacity);
  }
  e = &index_entries[index_count++];
  e->target_type = target_type; e->target_vnum = target;
  e->ref.type = source_type; e->ref.vnum = source; e->ref.display_name = safe(name);
  e->ref.relationship = relation; e->ref.location_vnum = location; e->ref.source = origin;
}

static void add_scripts(enum builder_ref_type owner_type, int owner_vnum, const char *name,
                        struct trig_proto_list *scripts)
{
  for (; scripts; scripts = scripts->next)
    add(BREF_TRIGGER, scripts->vnum, owner_type, owner_vnum, name,
        "Attached Trigger", owner_vnum, "Prototype attachment");
}

static void build_index(void)
{
  int i, z, c, last_mob = -1;
  index_count = 0;
  for (i = 0; i <= top_of_mobt; i++) {
    add_scripts(BREF_MOB, mob_index[i].vnum, mob_proto[i].player.short_descr, mob_proto[i].proto_script);
    z = real_zone_by_thing(mob_index[i].vnum);
    if (z != NOWHERE) add(BREF_ZONE, zone_table[z].number, BREF_MOB, mob_index[i].vnum,
        mob_proto[i].player.short_descr, "Contains Mobile", -1, "Prototype index");
  }
  for (i = 0; i <= top_of_objt; i++) {
    add_scripts(BREF_OBJECT, obj_index[i].vnum, obj_proto[i].short_description, obj_proto[i].proto_script);
    z = real_zone_by_thing(obj_index[i].vnum);
    if (z != NOWHERE) add(BREF_ZONE, zone_table[z].number, BREF_OBJECT, obj_index[i].vnum,
        obj_proto[i].short_description, "Contains Object", -1, "Prototype index");
  }
  for (i = 0; i <= top_of_world; i++) {
    int dir;
    add(BREF_ROOM, world[i].number, BREF_ZONE, zone_table[world[i].zone].number,
        zone_table[world[i].zone].name, "Zone", world[i].number, "Room index");
    add(BREF_ZONE, zone_table[world[i].zone].number, BREF_ROOM, world[i].number,
        world[i].name, "Contains Room", world[i].number, "Room index");
    add_scripts(BREF_ROOM, world[i].number, world[i].name, world[i].proto_script);
    for (dir = 0; dir < NUM_OF_DIRS; dir++) if (world[i].dir_option[dir] && world[i].dir_option[dir]->to_room != NOWHERE) {
      room_rnum to = world[i].dir_option[dir]->to_room;
      add(BREF_ROOM, world[to].number, BREF_ROOM, world[i].number, world[i].name,
          "Incoming Exit", world[i].number, "Room exit");
      add(BREF_ROOM, world[i].number, BREF_ROOM, world[to].number, world[to].name,
          "Outgoing Exit", world[i].number, "Room exit");
      if (world[i].zone != world[to].zone)
        add(BREF_ZONE, zone_table[world[i].zone].number, BREF_ZONE, zone_table[world[to].zone].number,
            zone_table[world[to].zone].name, "Linked Zone", world[i].number, "Room exit");
    }
  }
  for (z = 0; z <= top_of_zone_table; z++) for (c = 0; zone_table[z].cmd[c].command != 'S'; c++) {
    struct reset_com *cmd = &zone_table[z].cmd[c];
    int room = (cmd->command == 'M' || cmd->command == 'O' || cmd->command == 'T') && cmd->arg3 != NOWHERE ? world[cmd->arg3].number : -1;
    if (cmd->command == 'M') {
      last_mob = cmd->arg1;
      add(BREF_MOB, mob_index[cmd->arg1].vnum, BREF_ROOM, room, room >= 0 ? world[cmd->arg3].name : "(no room)", "Spawned In", room, "Zone reset");
      add(BREF_MOB, mob_index[cmd->arg1].vnum, BREF_ZONE, zone_table[z].number, zone_table[z].name, "Loaded By Zone", room, "Zone reset");
      add(BREF_ROOM, room, BREF_MOB, mob_index[cmd->arg1].vnum, mob_proto[cmd->arg1].player.short_descr, "Mob Reset", room, "Zone reset");
    } else if (cmd->command == 'O' || cmd->command == 'G' || cmd->command == 'E' || cmd->command == 'P') {
      const char *rel = cmd->command == 'E' ? "Equipped By Mob Reset" : cmd->command == 'P' ? "Contained In Object" : "Loaded By Zone";
      int source = cmd->command == 'P' ? obj_index[cmd->arg3].vnum : zone_table[z].number;
      enum builder_ref_type st = cmd->command == 'P' ? BREF_OBJECT : BREF_ZONE;
      add(BREF_OBJECT, obj_index[cmd->arg1].vnum, st, source,
          cmd->command == 'P' ? obj_proto[cmd->arg3].short_description : zone_table[z].name, rel, room, "Zone reset");
      if (cmd->command == 'E' && last_mob >= 0)
        add(BREF_OBJECT, obj_index[cmd->arg1].vnum, BREF_MOB, mob_index[last_mob].vnum,
            mob_proto[last_mob].player.short_descr, rel, room, "Zone reset");
      if (room >= 0) add(BREF_ROOM, room, BREF_OBJECT, obj_index[cmd->arg1].vnum,
          obj_proto[cmd->arg1].short_description, "Object Reset", room, "Zone reset");
    } else if (cmd->command == 'T') {
      add(BREF_TRIGGER, trig_index[cmd->arg2]->vnum, BREF_ZONE, zone_table[z].number,
          zone_table[z].name, "Reset Attachment", room, "Zone reset");
    }
  }
  for (i = 0; i <= top_shop; i++) {
    int j;
    if (shop_index[i].keeper != NOBODY) add(BREF_MOB, mob_index[shop_index[i].keeper].vnum,
        BREF_SHOP, shop_index[i].vnum, "Shop", "Shopkeeper", -1, "Shop index");
    for (j = 0; shop_index[i].in_room[j] != NOWHERE; j++)
      add(BREF_ROOM, shop_index[i].in_room[j], BREF_SHOP, shop_index[i].vnum,
          "Shop", "Shop", shop_index[i].in_room[j], "Shop index");
  }
  for (i = 0; i <= top_of_trigt; i++) if (trig_index[i]) {
    z = real_zone_by_thing(trig_index[i]->vnum);
    if (z != NOWHERE) add(BREF_ZONE, zone_table[z].number, BREF_TRIGGER, trig_index[i]->vnum,
        trig_index[i]->proto->name, "Contains Trigger", -1, "Trigger index");
  }
  index_valid = TRUE;
}

struct builder_reference_list builder_refs_find(enum builder_ref_type type, int vnum)
{
  struct builder_reference_list out = { NULL, 0 }; size_t i;
  if (!index_valid) build_index();
  for (i = 0; i < index_count; i++) if (index_entries[i].target_type == type && index_entries[i].target_vnum == vnum) {
    RECREATE(out.entries, struct builder_reference, out.count + 1);
    out.entries[out.count++] = index_entries[i].ref;
  }
  return out;
}
void builder_refs_free(struct builder_reference_list *list) { free(list->entries); list->entries = NULL; list->count = 0; }
void builder_refs_invalidate(void) { index_valid = FALSE; generation++; }
unsigned long builder_refs_generation(void) { return generation; }

void builder_refs_display(struct descriptor_data *d, enum builder_ref_type type, int vnum, const char *title)
{
  static const char *kinds[] = { "Mob", "Object", "Room", "Trigger", "Zone", "Shop", "Special" };
  struct builder_reference_list list = builder_refs_find(type, vnum); size_t i;
  write_to_output(d, "\r\n%s References\r\n--------------------------\r\n", title);
  if (!list.count) write_to_output(d, "No references found.\r\n");
  for (i = 0; i < list.count; i++) {
    write_to_output(d, "%-18s %s %d - %s", list.entries[i].relationship,
      kinds[list.entries[i].type], list.entries[i].vnum, list.entries[i].display_name);
    if (list.entries[i].location_vnum >= 0)
      write_to_output(d, "  [location %d]", list.entries[i].location_vnum);
    write_to_output(d, "  (%s)\r\n", list.entries[i].source);
  }
  write_to_output(d, "\r\nTotal References: %lu\r\nSource data only; this screen is read-only.\r\nPress ENTER to return. ", (unsigned long)list.count);
  builder_refs_free(&list);
}
