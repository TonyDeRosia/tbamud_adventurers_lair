#ifndef CLAN_H
#define CLAN_H

#define CLAN_NONE 0
#define CLAN_NAME_LEN 64
#define CLAN_DISPLAY_LEN 64


/* Clan PvP / PK policy */
#define CLAN_PVP_NO_PVP   0
#define CLAN_PVP_ALWAYS   1
#define CLAN_PVP_TOGGLE   2

struct clan_data {
  int id;
  long leader_idnum;
  char name[CLAN_NAME_LEN];
  char display_name[CLAN_DISPLAY_LEN];
  int pvp_type; /* CLAN_PVP_* */
  struct clan_data *next;
};

void clan_boot(void);
void clan_shutdown(void);

int clan_exists(int clan_id);
const char *clan_name_by_id(int clan_id);
const char *clan_display_name_by_id(int clan_id);
int clan_id_by_name(const char *name);
int clan_next_id(void);

int clan_create_and_save(int new_id, long leader_idnum, const char *name);
int clan_set_name_and_save(int clan_id, const char *name);
int clan_set_display_name_and_save(int clan_id, const char *display_name);
int clan_save_all(void);




int clan_set_pvp_type_and_save(int clan_id, int pvp_type);
int clan_pvp_type_by_id(int clan_id);
const char *clan_pvp_type_name(int pvp_type);
#endif
