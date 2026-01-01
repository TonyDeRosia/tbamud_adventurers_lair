#ifndef CLAN_H
#define CLAN_H

#define CLAN_NONE 0

struct clan_data {
  int id;
  long leader_idnum;
  char name[64];
  struct clan_data *next;
};

void clan_boot(void);
void clan_shutdown(void);

int clan_exists(int clan_id);
const char *clan_name_by_id(int clan_id);
int clan_next_id(void);

int clan_create_and_save(int new_id, long leader_idnum, const char *name);

#endif
