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

int clan_save_all(void)
{
  FILE *fl;
  struct clan_data *c;

  fl = fopen("misc/clans.dat", "w");
  if (!fl)
    return 0;

  for (c = clan_list; c; c = c->next)
    fprintf(fl, "%d\t%ld\t%s\t%s\n", c->id, c->leader_idnum, c->name, c->display_name);

  fclose(fl);
  return 1;
}

static void clan_add(int id, long leader_idnum, const char *name, const char *display_name)
{
  struct clan_data *c = calloc(1, sizeof(*c));
  if (!c)
    return;

  c->id = id;
  c->leader_idnum = leader_idnum;
  strlcpy(c->name, name ? name : "Unnamed", sizeof(c->name));
  if (display_name && *display_name)
    strlcpy(c->display_name, display_name, sizeof(c->display_name));
  else
    strlcpy(c->display_name, c->name, sizeof(c->display_name));
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
  char display_name[128];

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
    display_name[0] = '\0';
    if (sscanf(line, "%d\t%ld\t%127[^\t]\t%127[^\r\n]", &id, &leader, name, display_name) >= 3 ||
        sscanf(line, "%d %ld %127s %127s", &id, &leader, name, display_name) >= 3) {
      if (id > 0 && *name)
        clan_add(id, leader, name, *display_name ? display_name : NULL);
    }
  }

  fclose(fl);
  mudlog(NRM, LVL_GOD, TRUE, "Clan system: loaded clans");
}

void clan_shutdown(void)
{
  clan_save_all();
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

const char *clan_display_name_by_id(int clan_id)
{
  struct clan_data *c = clan_by_id(clan_id);
  if (!c)
    return "None";
  if (c->display_name && *c->display_name)
    return c->display_name;
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
  if (new_id <= 0 || !name || !*name)
    return 0;

  clan_add(new_id, leader_idnum, name, name);
  return clan_save_all();
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

int clan_set_display_name_and_save(int clan_id, const char *display_name)
{
  struct clan_data *c;

  c = clan_by_id(clan_id);
  if (!c || !display_name || !*display_name)
    return 0;

  strlcpy(c->display_name, display_name, sizeof(c->display_name));
  return clan_save_all();
}
