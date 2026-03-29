#include "conf.h"
#include "sysdep.h"

#include "structs.h"
#include "utils.h"
#include "comm.h"
#include "db.h"
#include "interpreter.h"
#include "custom_channels.h"

#define CUSTOM_CHANNEL_FILE_VERSION 1

struct custom_channel_data *custom_channel_list = NULL;

static const char *custom_channel_type_names[] = {
  "ic",
  "ooc",
  "staff",
  "generic",
  "\n"
};

static const char *custom_channel_scope_names[] = {
  "global",
  "zone",
  "room",
  "\n"
};

static void normalize_token(const char *src, char *dst, size_t dst_size)
{
  size_t i;

  if (!dst_size)
    return;

  for (i = 0; i + 1 < dst_size && src && src[i]; i++)
    dst[i] = LOWER(src[i]);
  dst[i] = '\0';
}

static int channel_name_valid(const char *name)
{
  const char *p;

  if (!name || !*name)
    return FALSE;

  for (p = name; *p; p++) {
    if (!isalnum(*p) && *p != '_' && *p != '-')
      return FALSE;
  }

  return TRUE;
}

static int custom_channel_color_valid(const char *color)
{
  return (color && strlen(color) == 1 && strchr("nkrgybmcwKRGYBMCW", color[0]) != NULL);
}

static int custom_channel_alias_conflicts_command(const char *alias)
{
  if (!alias || !*alias)
    return FALSE;

  return (find_command(alias) >= 0);
}

static int custom_channel_name_in_use(const char *name, struct custom_channel_data *skip)
{
  struct custom_channel_data *chan;

  for (chan = custom_channel_list; chan; chan = chan->next)
    if (chan != skip && !str_cmp(chan->name, name))
      return TRUE;

  return FALSE;
}

static int custom_channel_alias_in_use(const char *alias, struct custom_channel_data *skip)
{
  struct custom_channel_data *chan;

  if (!alias || !*alias)
    return FALSE;

  for (chan = custom_channel_list; chan; chan = chan->next)
    if (chan != skip && *chan->alias && !str_cmp(chan->alias, alias))
      return TRUE;

  return FALSE;
}

static void custom_channel_insert_sorted(struct custom_channel_data *channel)
{
  struct custom_channel_data *iter, *prev = NULL;

  for (iter = custom_channel_list; iter; iter = iter->next) {
    if (str_cmp(channel->name, iter->name) < 0)
      break;
    prev = iter;
  }

  if (prev == NULL) {
    channel->next = custom_channel_list;
    custom_channel_list = channel;
  } else {
    channel->next = prev->next;
    prev->next = channel;
  }
}

static struct custom_channel_data *custom_channel_alloc_defaults(const char *name)
{
  struct custom_channel_data *channel;

  CREATE(channel, struct custom_channel_data, 1);
  memset(channel, 0, sizeof(*channel));

  strlcpy(channel->name, name, sizeof(channel->name));
  strlcpy(channel->display, name, sizeof(channel->display));
  channel->enabled = TRUE;
  channel->min_send_level = 0;
  channel->min_hear_level = 0;
  channel->type = CUSTOM_CHANNEL_TYPE_GENERIC;
  channel->scope = CUSTOM_CHANNEL_SCOPE_GLOBAL;
  channel->log_enabled = FALSE;
  strlcpy(channel->color, "n", sizeof(channel->color));

  return channel;
}

static int custom_channel_scope_matches(const struct custom_channel_data *channel, struct char_data *sender, struct char_data *listener)
{
  if (!channel || !sender || !listener || IN_ROOM(sender) == NOWHERE || IN_ROOM(listener) == NOWHERE)
    return FALSE;

  switch (channel->scope) {
    case CUSTOM_CHANNEL_SCOPE_GLOBAL:
      return TRUE;
    case CUSTOM_CHANNEL_SCOPE_ZONE:
      return (world[IN_ROOM(sender)].zone == world[IN_ROOM(listener)].zone);
    case CUSTOM_CHANNEL_SCOPE_ROOM:
      return (IN_ROOM(sender) == IN_ROOM(listener));
    default:
      return FALSE;
  }
}

static int custom_channel_can_hear(const struct custom_channel_data *channel, struct char_data *sender, struct char_data *listener)
{
  if (!channel || !listener)
    return FALSE;

  if (!channel->enabled)
    return FALSE;

  if (GET_LEVEL(listener) < channel->min_hear_level)
    return FALSE;

  if (!custom_channel_scope_matches(channel, sender, listener))
    return FALSE;

  if (PLR_FLAGGED(listener, PLR_WRITING))
    return FALSE;

  if (ROOM_FLAGGED(IN_ROOM(listener), ROOM_SOUNDPROOF) && GET_LEVEL(sender) < LVL_GOD)
    return FALSE;

  return TRUE;
}

static void custom_channels_seed_default(void)
{
  struct custom_channel_data *ooc;

  if (custom_channel_list)
    return;

  ooc = custom_channel_alloc_defaults("ooc");
  strlcpy(ooc->display, "OOC", sizeof(ooc->display));
  strlcpy(ooc->color, "C", sizeof(ooc->color));
  ooc->type = CUSTOM_CHANNEL_TYPE_OOC;
  ooc->scope = CUSTOM_CHANNEL_SCOPE_GLOBAL;
  ooc->log_enabled = TRUE;

  custom_channel_insert_sorted(ooc);
  custom_channels_save();
  log("Custom channels: seeded default 'ooc' channel.");
}

static int parse_bool_field(const char *value, int *target)
{
  if (!value || !*value || !target)
    return FALSE;

  if (!str_cmp(value, "1") || !str_cmp(value, "yes") || !str_cmp(value, "on") || !str_cmp(value, "true")) {
    *target = TRUE;
    return TRUE;
  }

  if (!str_cmp(value, "0") || !str_cmp(value, "no") || !str_cmp(value, "off") || !str_cmp(value, "false")) {
    *target = FALSE;
    return TRUE;
  }

  return FALSE;
}

static void custom_channel_parse_line(char *line, int line_no)
{
  struct custom_channel_data *chan;
  char *fields[10];
  char norm_name[CUSTOM_CHANNEL_NAME_LEN];
  char norm_alias[CUSTOM_CHANNEL_ALIAS_LEN];
  int i, count = 0;

  while (*line && isspace(*line))
    line++;

  if (!*line || *line == '#')
    return;

  for (i = 0; i < 10; i++)
    fields[i] = NULL;

  fields[count++] = strtok(line, "|\r\n");
  while (count < 10 && (fields[count] = strtok(NULL, "|\r\n")) != NULL)
    count++;

  if (count != 10) {
    log("SYSERR: custom channel file line %d malformed; expected 10 fields, got %d", line_no, count);
    return;
  }

  normalize_token(fields[0], norm_name, sizeof(norm_name));
  if (!channel_name_valid(norm_name)) {
    log("SYSERR: custom channel line %d invalid name '%s'", line_no, fields[0]);
    return;
  }

  if (custom_channel_name_in_use(norm_name, NULL)) {
    log("SYSERR: custom channel line %d duplicate name '%s'", line_no, norm_name);
    return;
  }

  chan = custom_channel_alloc_defaults(norm_name);

  if (*fields[1])
    strlcpy(chan->display, fields[1], sizeof(chan->display));

  normalize_token(fields[2], norm_alias, sizeof(norm_alias));
  if (*norm_alias) {
    if (!channel_name_valid(norm_alias) || custom_channel_alias_conflicts_command(norm_alias) ||
        custom_channel_name_in_use(norm_alias, NULL) || custom_channel_alias_in_use(norm_alias, NULL)) {
      log("SYSERR: custom channel line %d alias '%s' invalid/conflicting, alias ignored", line_no, fields[2]);
    } else {
      strlcpy(chan->alias, norm_alias, sizeof(chan->alias));
    }
  }

  if (!custom_channel_color_valid(fields[3])) {
    log("SYSERR: custom channel line %d invalid color '%s', defaulting to n", line_no, fields[3]);
  } else {
    strlcpy(chan->color, fields[3], sizeof(chan->color));
  }

  if (!parse_bool_field(fields[4], &chan->enabled)) {
    log("SYSERR: custom channel line %d invalid enabled flag '%s'", line_no, fields[4]);
    free(chan);
    return;
  }

  if (!is_number(fields[5]) || !is_number(fields[6])) {
    log("SYSERR: custom channel line %d invalid level values", line_no);
    free(chan);
    return;
  }

  chan->min_send_level = MAX(0, MIN(LVL_IMPL, atoi(fields[5])));
  chan->min_hear_level = MAX(0, MIN(LVL_IMPL, atoi(fields[6])));

  chan->type = search_block(fields[7], custom_channel_type_names, TRUE);
  chan->scope = search_block(fields[8], custom_channel_scope_names, TRUE);

  if (chan->type < 0 || chan->scope < 0) {
    log("SYSERR: custom channel line %d invalid type/scope '%s'/'%s'", line_no, fields[7], fields[8]);
    free(chan);
    return;
  }

  if (!parse_bool_field(fields[9], &chan->log_enabled)) {
    log("SYSERR: custom channel line %d invalid log flag '%s'", line_no, fields[9]);
    free(chan);
    return;
  }

  custom_channel_insert_sorted(chan);
}

static void custom_channels_load(void)
{
  FILE *fl;
  char line[MAX_STRING_LENGTH];
  int line_no = 0;

  if (!(fl = fopen(CUSTOM_CHANNELS_FILE, "r"))) {
    if (errno != ENOENT)
      log("SYSERR: Unable to open custom channels file '%s': %s", CUSTOM_CHANNELS_FILE, strerror(errno));
    return;
  }

  while (fgets(line, sizeof(line), fl)) {
    line_no++;

    if (!strncmp(line, "VERSION|", 8))
      continue;

    custom_channel_parse_line(line, line_no);
  }

  fclose(fl);
}

void custom_channels_free(void)
{
  struct custom_channel_data *chan, *next;

  for (chan = custom_channel_list; chan; chan = next) {
    next = chan->next;
    free(chan);
  }

  custom_channel_list = NULL;
}

void custom_channels_boot(void)
{
  custom_channels_free();
  custom_channels_load();
  custom_channels_seed_default();
}

int custom_channels_save(void)
{
  FILE *fl;
  struct custom_channel_data *chan;

  if (!(fl = fopen(CUSTOM_CHANNELS_FILE, "w"))) {
    log("SYSERR: Unable to write custom channels file '%s': %s", CUSTOM_CHANNELS_FILE, strerror(errno));
    return FALSE;
  }

  fprintf(fl, "VERSION|%d\n", CUSTOM_CHANNEL_FILE_VERSION);
  for (chan = custom_channel_list; chan; chan = chan->next) {
    fprintf(fl, "%s|%s|%s|%s|%d|%d|%d|%s|%s|%d\n",
      chan->name,
      chan->display,
      chan->alias,
      custom_channel_color_valid(chan->color) ? chan->color : "n",
      chan->enabled,
      chan->min_send_level,
      chan->min_hear_level,
      custom_channel_type_name(chan->type),
      custom_channel_scope_name(chan->scope),
      chan->log_enabled);
  }

  fclose(fl);
  return TRUE;
}

struct custom_channel_data *custom_channel_find_by_name(const char *name)
{
  struct custom_channel_data *chan;
  char normalized[CUSTOM_CHANNEL_NAME_LEN];

  normalize_token(name, normalized, sizeof(normalized));

  for (chan = custom_channel_list; chan; chan = chan->next)
    if (!str_cmp(chan->name, normalized))
      return chan;

  return NULL;
}

struct custom_channel_data *custom_channel_find_by_alias(const char *alias)
{
  struct custom_channel_data *chan;
  char normalized[CUSTOM_CHANNEL_ALIAS_LEN];

  normalize_token(alias, normalized, sizeof(normalized));

  if (!*normalized)
    return NULL;

  for (chan = custom_channel_list; chan; chan = chan->next)
    if (*chan->alias && !str_cmp(chan->alias, normalized))
      return chan;

  return NULL;
}

struct custom_channel_data *custom_channel_find(const char *name_or_alias)
{
  struct custom_channel_data *chan;

  if (!name_or_alias || !*name_or_alias)
    return NULL;

  chan = custom_channel_find_by_name(name_or_alias);
  if (chan)
    return chan;

  return custom_channel_find_by_alias(name_or_alias);
}

int custom_channel_create(const char *name, char *errmsg, size_t errmsg_size)
{
  struct custom_channel_data *channel;
  char normalized[CUSTOM_CHANNEL_NAME_LEN];

  normalize_token(name, normalized, sizeof(normalized));

  if (!channel_name_valid(normalized)) {
    snprintf(errmsg, errmsg_size, "Invalid channel name. Use letters, numbers, '_' or '-'.");
    return FALSE;
  }

  if (custom_channel_name_in_use(normalized, NULL)) {
    snprintf(errmsg, errmsg_size, "A channel with that name already exists.");
    return FALSE;
  }

  channel = custom_channel_alloc_defaults(normalized);
  custom_channel_insert_sorted(channel);

  if (!custom_channels_save()) {
    snprintf(errmsg, errmsg_size, "Created in memory, but failed to save configuration.");
    return FALSE;
  }

  return TRUE;
}

int custom_channel_delete(const char *name, char *errmsg, size_t errmsg_size)
{
  struct custom_channel_data *chan, *prev = NULL;
  char normalized[CUSTOM_CHANNEL_NAME_LEN];

  normalize_token(name, normalized, sizeof(normalized));

  for (chan = custom_channel_list; chan; chan = chan->next) {
    if (!str_cmp(chan->name, normalized))
      break;
    prev = chan;
  }

  if (!chan) {
    snprintf(errmsg, errmsg_size, "No such custom channel.");
    return FALSE;
  }

  if (prev)
    prev->next = chan->next;
  else
    custom_channel_list = chan->next;

  free(chan);

  if (!custom_channels_save()) {
    snprintf(errmsg, errmsg_size, "Deleted in memory, but failed to save configuration.");
    return FALSE;
  }

  return TRUE;
}

int custom_channel_set_display(struct custom_channel_data *channel, const char *display, char *errmsg, size_t errmsg_size)
{
  if (!channel || !display || !*display) {
    snprintf(errmsg, errmsg_size, "Display text cannot be empty.");
    return FALSE;
  }

  if (strlen(display) >= sizeof(channel->display)) {
    snprintf(errmsg, errmsg_size, "Display text is too long (max %zu).", sizeof(channel->display) - 1);
    return FALSE;
  }

  if (strchr(display, '|')) {
    snprintf(errmsg, errmsg_size, "Display text cannot contain '|'.");
    return FALSE;
  }

  strlcpy(channel->display, display, sizeof(channel->display));
  return custom_channels_save();
}

int custom_channel_set_alias(struct custom_channel_data *channel, const char *alias, char *errmsg, size_t errmsg_size)
{
  char normalized[CUSTOM_CHANNEL_ALIAS_LEN];

  if (!channel || !alias) {
    snprintf(errmsg, errmsg_size, "Alias value is required.");
    return FALSE;
  }

  normalize_token(alias, normalized, sizeof(normalized));

  if (!*normalized || !str_cmp(normalized, "none") || !str_cmp(normalized, "off") || !str_cmp(normalized, "-") ) {
    channel->alias[0] = '\0';
    return custom_channels_save();
  }

  if (!channel_name_valid(normalized)) {
    snprintf(errmsg, errmsg_size, "Invalid alias. Use letters, numbers, '_' or '-'.");
    return FALSE;
  }

  if (!str_cmp(normalized, channel->name)) {
    strlcpy(channel->alias, normalized, sizeof(channel->alias));
    return custom_channels_save();
  }

  if (custom_channel_alias_conflicts_command(normalized)) {
    snprintf(errmsg, errmsg_size, "Alias conflicts with an existing command.");
    return FALSE;
  }

  if (custom_channel_name_in_use(normalized, channel) || custom_channel_alias_in_use(normalized, channel)) {
    snprintf(errmsg, errmsg_size, "Alias conflicts with another custom channel name/alias.");
    return FALSE;
  }

  strlcpy(channel->alias, normalized, sizeof(channel->alias));
  return custom_channels_save();
}

int custom_channel_set_color(struct custom_channel_data *channel, const char *color, char *errmsg, size_t errmsg_size)
{
  if (!channel || !custom_channel_color_valid(color)) {
    snprintf(errmsg, errmsg_size, "Invalid color code. Use a single existing color letter (e.g. C, Y, R, g, W).");
    return FALSE;
  }

  strlcpy(channel->color, color, sizeof(channel->color));
  return custom_channels_save();
}

int custom_channel_set_minsend(struct custom_channel_data *channel, int level, char *errmsg, size_t errmsg_size)
{
  if (!channel || level < 0 || level > LVL_IMPL) {
    snprintf(errmsg, errmsg_size, "minsend must be between 0 and %d.", LVL_IMPL);
    return FALSE;
  }

  channel->min_send_level = level;
  return custom_channels_save();
}

int custom_channel_set_minhear(struct custom_channel_data *channel, int level, char *errmsg, size_t errmsg_size)
{
  if (!channel || level < 0 || level > LVL_IMPL) {
    snprintf(errmsg, errmsg_size, "minhear must be between 0 and %d.", LVL_IMPL);
    return FALSE;
  }

  channel->min_hear_level = level;
  return custom_channels_save();
}

int custom_channel_set_type(struct custom_channel_data *channel, const char *type_name, char *errmsg, size_t errmsg_size)
{
  char normalized[32];
  int type;

  if (!channel || !type_name) {
    snprintf(errmsg, errmsg_size, "Type is required.");
    return FALSE;
  }

  normalize_token(type_name, normalized, sizeof(normalized));
  type = search_block(normalized, custom_channel_type_names, TRUE);
  if (type < 0) {
    snprintf(errmsg, errmsg_size, "Invalid type. Use: ic, ooc, staff, generic.");
    return FALSE;
  }

  channel->type = type;
  return custom_channels_save();
}

int custom_channel_set_scope(struct custom_channel_data *channel, const char *scope_name, char *errmsg, size_t errmsg_size)
{
  char normalized[32];
  int scope;

  if (!channel || !scope_name) {
    snprintf(errmsg, errmsg_size, "Scope is required.");
    return FALSE;
  }

  normalize_token(scope_name, normalized, sizeof(normalized));
  scope = search_block(normalized, custom_channel_scope_names, TRUE);
  if (scope < 0) {
    snprintf(errmsg, errmsg_size, "Invalid scope. Use: global, zone, room.");
    return FALSE;
  }

  channel->scope = scope;
  return custom_channels_save();
}

int custom_channel_set_log(struct custom_channel_data *channel, const char *state, char *errmsg, size_t errmsg_size)
{
  int enabled;

  if (!channel || !parse_bool_field(state, &enabled)) {
    snprintf(errmsg, errmsg_size, "Invalid log value. Use on/off.");
    return FALSE;
  }

  channel->log_enabled = enabled;
  return custom_channels_save();
}

const char *custom_channel_type_name(int type)
{
  if (type >= 0 && type < CUSTOM_CHANNEL_TYPE_MAX)
    return custom_channel_type_names[type];

  return "generic";
}

const char *custom_channel_scope_name(int scope)
{
  if (scope >= 0 && scope < CUSTOM_CHANNEL_SCOPE_MAX)
    return custom_channel_scope_names[scope];

  return "global";
}

int custom_channel_send(struct char_data *ch, struct custom_channel_data *channel, const char *message)
{
  struct descriptor_data *d;
  char out[MAX_STRING_LENGTH];
  char safe_color = 'n';

  if (!ch || !channel)
    return FALSE;

  if (!channel->enabled) {
    send_to_char(ch, "That channel is currently disabled.\r\n");
    return FALSE;
  }

  if (GET_LEVEL(ch) < channel->min_send_level) {
    send_to_char(ch, "You must be at least level %d to use %s.\r\n", channel->min_send_level, channel->display);
    return FALSE;
  }

  if (IN_ROOM(ch) == NOWHERE) {
    send_to_char(ch, "You are nowhere and cannot use that channel right now.\r\n");
    return FALSE;
  }

  if (ROOM_FLAGGED(IN_ROOM(ch), ROOM_SOUNDPROOF) && GET_LEVEL(ch) < LVL_GOD) {
    send_to_char(ch, "The walls seem to absorb your words.\r\n");
    return FALSE;
  }

  if (!message || !*message) {
    send_to_char(ch, "What do you want to say on that channel?\r\n");
    return FALSE;
  }

  if (custom_channel_color_valid(channel->color))
    safe_color = channel->color[0];

  snprintf(out, sizeof(out), "\t%c(%s)\tn \tW%s\tn: %s\tn\r\n", safe_color, channel->display, GET_NAME(ch), message);

  if (channel->log_enabled) {
    mudlog(NRM, MAX(LVL_IMMORT, GET_INVIS_LEV(ch)), TRUE,
      "CUSTOM-CHANNEL [%s] %s: %s", channel->name, GET_NAME(ch), message);
  }

  for (d = descriptor_list; d; d = d->next) {
    if (STATE(d) != CON_PLAYING || !d->character)
      continue;

    if (!custom_channel_can_hear(channel, ch, d->character))
      continue;

    send_to_char(d->character, "%s", out);
  }

  return TRUE;
}

int custom_channel_try_alias_command(struct char_data *ch, const char *command, char *argument)
{
  struct custom_channel_data *channel;

  if (!ch || !command || !*command)
    return FALSE;

  channel = custom_channel_find_by_alias(command);
  if (!channel)
    channel = custom_channel_find_by_name(command);

  if (!channel)
    return FALSE;

  skip_spaces(&argument);
  if (!*argument) {
    send_to_char(ch, "Usage: %s <message>\r\n", command);
    return TRUE;
  }

  custom_channel_send(ch, channel, argument);
  return TRUE;
}
