#include "conf.h"
#include "sysdep.h"

#include "structs.h"
#include "utils.h"
#include "comm.h"
#include "db.h"
#include "clan.h"
static struct clan_data *clan_list = NULL;

static void clan_free_all(void)
{
  struct clan_data *c, *n;
  for (c = clan_list; c; c = n) {
    n = c->next;
    free(c);
  }
  clan_list = NULL;
}

static struct clan_data *clan_by_id(int clan_id)
{
  struct clan_data *c;
  if (clan_id <= 0)
    return NULL;

  for (c = clan_list; c; c = c->next)
    if (c->id == clan_id)
      return c;

  return NULL;
}

static int clan_save_all(void)
{
  FILE *fl;
  struct clan_data *c;

  fl = fopen("misc/clans.dat", "w");
  if (!fl)
    return 0;

  for (c = clan_list; c; c = c->next)
    fprintf(fl, "%d %ld %s\n", c->id, c->leader_idnum, c->name);

  fclose(fl);
  return 1;
}

static void clan_add(int id, long leader_idnum, const char *name)
{
  struct clan_data *c = calloc(1, sizeof(*c));
  if (!c)
    return;

  c->id = id;
  c->leader_idnum = leader_idnum;
  strlcpy(c->name, name ? name : "Unnamed", sizeof(c->name));
  c->next = clan_list;
  clan_list = c;
}

void clan_boot(void)
{
  FILE *fl;
  char line[256];
  int id = 0;
  long leader = 0;
  char name[128];

  clan_free_all();

  fl = fopen("misc/clans.dat", "r");
  if (!fl) {
    mudlog(NRM, LVL_GOD, TRUE, "SYSERR: clan_boot: could not open misc/clans.dat");
    return;
  }

  while (fgets(line, sizeof(line), fl)) {
    if (!*line || line[0] == '#')
      continue;

    name[0] = '\0';
    if (sscanf(line, "%d %ld %127s", &id, &leader, name) == 3) {
      if (id > 0 && *name)
        clan_add(id, leader, name);
    }
  }

  fclose(fl);
  mudlog(NRM, LVL_GOD, TRUE, "Clan system: loaded clans");
}

void clan_shutdown(void)
{
  clan_free_all();
}

int clan_exists(int clan_id)
{
  return clan_by_id(clan_id) != NULL;
}

const char *clan_name_by_id(int clan_id)
{
  struct clan_data *c = clan_by_id(clan_id);
  if (!c)
    return "None";
  return c->name;
}

int clan_next_id(void)
{
  int max_id = 0;
  struct clan_data *c;
  for (c = clan_list; c; c = c->next)
    if (c->id > max_id)
      max_id = c->id;
  return max_id + 1;
}

int clan_create_and_save(int new_id, long leader_idnum, const char *name)
{
  FILE *fl;

  if (new_id <= 0 || !name || !*name)
    return 0;

  fl = fopen("misc/clans.dat", "a");
  if (!fl)
    return 0;

  fprintf(fl, "%d %ld %s\n", new_id, leader_idnum, name);
  fclose(fl);

  clan_add(new_id, leader_idnum, name);
  return 1;
}

int clan_set_name_and_save(int clan_id, const char *name)
{
  struct clan_data *c;

  c = clan_by_id(clan_id);
  if (!c || !name || !*name)
    return 0;

  strlcpy(c->name, name, sizeof(c->name));
  return clan_save_all();
}
