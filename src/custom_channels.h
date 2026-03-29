#ifndef _CUSTOM_CHANNELS_H_
#define _CUSTOM_CHANNELS_H_

#include "structs.h"

#define CUSTOM_CHANNEL_NAME_LEN 32
#define CUSTOM_CHANNEL_DISPLAY_LEN 64
#define CUSTOM_CHANNEL_ALIAS_LEN 32
#define CUSTOM_CHANNEL_COLOR_LEN 8

enum custom_channel_type {
  CUSTOM_CHANNEL_TYPE_IC = 0,
  CUSTOM_CHANNEL_TYPE_OOC,
  CUSTOM_CHANNEL_TYPE_STAFF,
  CUSTOM_CHANNEL_TYPE_GENERIC,
  CUSTOM_CHANNEL_TYPE_MAX
};

enum custom_channel_scope {
  CUSTOM_CHANNEL_SCOPE_GLOBAL = 0,
  CUSTOM_CHANNEL_SCOPE_ZONE,
  CUSTOM_CHANNEL_SCOPE_ROOM,
  CUSTOM_CHANNEL_SCOPE_MAX
};

struct custom_channel_data {
  char name[CUSTOM_CHANNEL_NAME_LEN];
  char display[CUSTOM_CHANNEL_DISPLAY_LEN];
  char alias[CUSTOM_CHANNEL_ALIAS_LEN];
  char color[CUSTOM_CHANNEL_COLOR_LEN];
  int enabled;
  int min_send_level;
  int min_hear_level;
  int type;
  int scope;
  int log_enabled;
  struct custom_channel_data *next;
};

extern struct custom_channel_data *custom_channel_list;

void custom_channels_boot(void);
void custom_channels_free(void);
int custom_channels_save(void);

struct custom_channel_data *custom_channel_find_by_name(const char *name);
struct custom_channel_data *custom_channel_find_by_alias(const char *alias);
struct custom_channel_data *custom_channel_find(const char *name_or_alias);

int custom_channel_create(const char *name, char *errmsg, size_t errmsg_size);
int custom_channel_delete(const char *name, char *errmsg, size_t errmsg_size);

int custom_channel_set_display(struct custom_channel_data *channel, const char *display, char *errmsg, size_t errmsg_size);
int custom_channel_set_alias(struct custom_channel_data *channel, const char *alias, char *errmsg, size_t errmsg_size);
int custom_channel_set_color(struct custom_channel_data *channel, const char *color, char *errmsg, size_t errmsg_size);
int custom_channel_set_minsend(struct custom_channel_data *channel, int level, char *errmsg, size_t errmsg_size);
int custom_channel_set_minhear(struct custom_channel_data *channel, int level, char *errmsg, size_t errmsg_size);
int custom_channel_set_type(struct custom_channel_data *channel, const char *type_name, char *errmsg, size_t errmsg_size);
int custom_channel_set_scope(struct custom_channel_data *channel, const char *scope_name, char *errmsg, size_t errmsg_size);
int custom_channel_set_log(struct custom_channel_data *channel, const char *state, char *errmsg, size_t errmsg_size);

const char *custom_channel_type_name(int type);
const char *custom_channel_scope_name(int scope);

int custom_channel_send(struct char_data *ch, struct custom_channel_data *channel, const char *message);
int custom_channel_try_alias_command(struct char_data *ch, const char *command, char *argument);

#endif
