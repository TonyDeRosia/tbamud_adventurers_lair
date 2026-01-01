#include "conf.h"
#include "sysdep.h"

#include "structs.h"
#include "utils.h"
#include "comm.h"
#include "interpreter.h"
#include "handler.h"
#include "db.h"
#include "clan.h"

/* rank ideas for now */
#define CLAN_RANK_MEMBER 0
#define CLAN_RANK_LEADER 2

static void send_to_clan(int clan_id, const char *msg, struct char_data *from)
{
  struct descriptor_data *d;

  if (clan_id <= 0 || !msg || !*msg)
    return;

  for (d = descriptor_list; d; d = d->next) {
    if (!d->character)
      continue;
    if (STATE(d) != CON_PLAYING)
      continue;
    if (GET_CLAN_ID(d->character) != clan_id)
      continue;

    if (from)
      send_to_char(d->character, "\r\n[Clan] %s: %s\r\n", GET_NAME(from), msg);
    else
      send_to_char(d->character, "\r\n[Clan] %s\r\n", msg);
  }
}

static int is_valid_clan_name(const char *s)
{
  /* Allow: letters/numbers, and tbaMUD color codes like \tR \tn, no spaces. */
  if (!s || !*s)
    return 0;

  for (size_t i = 0; s[i]; i++) {
    if (s[i] == '\\') {
      /* Accept \tX */
      if (s[i + 1] == 't' && s[i + 2]) {
        i += 2; /* skip over \ t X */
        continue;
      }
      return 0;
    }
    if (isspace((unsigned char)s[i]))
      return 0;
    if (!isalnum((unsigned char)s[i]))
      return 0;
  }
  return 1;
}


static void clan_help(struct char_data *ch)
{
  send_to_char(ch,
    "Clan commands:\r\n"
    "  clan                 shows your clan status\r\n"
    "  clan list            shows clan file location\r\n"
    "  clan <message>       sends clan chat\r\n"
    "Shortcuts:\r\n"
    "  ccreate <Name>       create clan (admins level 104 only)\r\n"
    "  cinvite <player>     not active yet\r\n"
    "  cjoin                not active yet\r\n"
    "  cquit [yes]          leave clan (confirm with yes)\r\n"
    "  cpromote ...         not active yet\r\n"
    "  cdemote ...          not active yet\r\n"
  );
}
/* Online roster. Offline roster can be added later by reading clan.dat membership. */
static int roster_cmp(const void *a, const void *b)
{
  const struct char_data *ca = *(const struct char_data * const *)a;
  const struct char_data *cb = *(const struct char_data * const *)b;

  int la = GET_LEVEL(ca);
  int lb = GET_LEVEL(cb);

  if (la != lb)
    return (lb - la);

  return strcasecmp(GET_NAME(ca), GET_NAME(cb));
}

/* Resolve race/class names without hard depending on any one symbol.
   Weak refs resolve to NULL if the symbol is not present in this codebase. */
extern const char *race_name_by_id(int id) __attribute__((weak));
extern const char *race_name(int id) __attribute__((weak));
extern const char *pc_race_types[] __attribute__((weak));

extern const char *class_name_by_id(int id) __attribute__((weak));
extern const char *class_name(int id) __attribute__((weak));
extern const char *pc_class_types[] __attribute__((weak));
extern const char *class_abbrevs[] __attribute__((weak));

static const char *roster_race_name(int id)
{
  if (race_name_by_id) return race_name_by_id(id);
  if (race_name) return race_name(id);
  if (pc_race_types) return pc_race_types[id];
  return "Unknown";
}

static const char *roster_class_name(int id)
{
  if (class_name_by_id) return class_name_by_id(id);
  if (class_name) return class_name(id);
  if (pc_class_types) return pc_class_types[id];
  if (class_abbrevs) return class_abbrevs[id];
  return "Unknown";
}


static void clan_show_roster(struct char_data *ch)
{
  struct descriptor_data *d;
  struct char_data *tch;
  int clan_id, count = 0;

  clan_id = GET_CLAN_ID(ch);
  if (clan_id <= 0) {
    send_to_char(ch, "You are not in a clan.\r\n");
    return;
  }

  /* Count online clan members */
  for (d = descriptor_list; d; d = d->next) {
    if (STATE(d) != CON_PLAYING)
      continue;
    if (!(tch = d->character))
      continue;
    if (IS_NPC(tch))
      continue;
    if (GET_CLAN_ID(tch) != clan_id)
      continue;
    count++;
  }

  /* Build list */
  struct char_data **list = NULL;
  if (count > 0) {
    list = (struct char_data **)calloc((size_t)count, sizeof(*list));
    if (!list) {
      send_to_char(ch, "Roster: out of memory.\r\n");
      return;
    }
  }

  int i = 0;
  for (d = descriptor_list; d; d = d->next) {
    if (STATE(d) != CON_PLAYING)
      continue;
    if (!(tch = d->character))
      continue;
    if (IS_NPC(tch))
      continue;
    if (GET_CLAN_ID(tch) != clan_id)
      continue;
    if (i < count)
      list[i++] = tch;
  }

  if (count > 1)
    qsort(list, (size_t)count, sizeof(*list), roster_cmp);

  send_to_char(ch,
    "\r\n"
    "╔══════════════════════════════════════════════════════════════════════╗\r\n"
    "║                              Clan Roster                             ║\r\n"
    "╠══════════════════════════════════════════════════════════════════════╣\r\n"
    "║ Name                     Race            Class            Level      ║\r\n"
    "╠══════════════════════════════════════════════════════════════════════╣\r\n"
  );

  if (count == 0) {
    send_to_char(ch,
      "║ No clan members are currently online.                                ║\r\n"
    );
  } else {
    for (i = 0; i < count; i++) {
      const char *rname = roster_race_name(GET_RACE(list[i]));
      const char *cname = roster_class_name(GET_CLASS(list[i]));
      send_to_char(ch, "║ %-24.24s %-15.15s %-15.15s %5d      ║\r\n",
        GET_NAME(list[i]),
        (rname ? rname : "Unknown"),
        (cname ? cname : "Unknown"),
        GET_LEVEL(list[i])
      );
    }
  }

  send_to_char(ch,
    "╚══════════════════════════════════════════════════════════════════════╝\r\n"
    "\r\n"
  );

  if (list)
    free(list);
}



ACMD(do_roster)
{
  if (IS_NPC(ch))
    return;

  if (GET_CLAN_ID(ch) <= 0) {
    send_to_char(ch, "You are not in a clan.\r\n");
    return;
  }

  clan_show_roster(ch);
}

ACMD(do_clan)
{
  if (IS_NPC(ch))
    return;

  if (GET_CLAN_ID(ch) <= 0) {
    send_to_char(ch, "You are not in a clan.\r\n");
    return;
  }

  skip_spaces(&argument);

  if (!*argument) {
    send_to_char(ch, "Usage: clan <message>\r\n");
    return;
  }

  send_to_clan(GET_CLAN_ID(ch), argument, ch);
}


/* Shortcut wrappers */
static void clan_wrap(struct char_data *ch, const char *prefix, const char *argument, int cmd, int subcmd)
{
  char buf[MAX_INPUT_LENGTH];
  if (argument && *argument)
    snprintf(buf, sizeof(buf), "%s %s", prefix, argument);
  else
    snprintf(buf, sizeof(buf), "%s", prefix);
  do_clan(ch, buf, cmd, subcmd);
}

ACMD(do_ccreate)
{
  char name[128];

  if (IS_NPC(ch))
    return;

  if (GET_LEVEL(ch) < 104) {
    send_to_char(ch, "Only immortals (level 104+) may create clans right now.\r\n");
    return;
  }

  if (GET_CLAN_ID(ch) > 0) {
    send_to_char(ch, "You are already in a clan.\r\n");
    return;
  }

  one_argument(argument, name);

  if (!*name) {
    clan_help(ch);
    send_to_char(ch, "Usage: ccreate <clanname>\r\n");
    send_to_char(ch, "Tip: you can include color codes like \\tRName\\tn (no spaces).\r\n");
    return;
  }

  if (!is_valid_clan_name(name)) {
    send_to_char(ch, "Invalid clan name. Use letters and numbers only.\r\n");
    send_to_char(ch, "You may also include color codes like \\tRName\\tn (no spaces).\r\n");
    return;
  }

  {
    int new_id = clan_next_id();
    if (new_id <= 0) {
      send_to_char(ch, "Clan system error: could not allocate a new clan id.\r\n");
      return;
    }

    if (!clan_create_and_save(new_id, GET_IDNUM(ch), name)) {
      send_to_char(ch, "Clan system error: could not create clan.\r\n");
      return;
    }

    SET_CLAN_ID(ch, new_id);
    SET_CLAN_RANK(ch, 10); /* leader rank for now */
    save_char(ch);

    send_to_char(ch, "Clan created.\r\n");
  }
}

ACMD(do_cinvite)  { clan_wrap(ch, "invite",  argument, cmd, subcmd); }
ACMD(do_cjoin)    { clan_wrap(ch, "join",    argument, cmd, subcmd); }
ACMD(do_cquit)    { clan_wrap(ch, "quit",    argument, cmd, subcmd); }
ACMD(do_cpromote) { clan_wrap(ch, "promote", argument, cmd, subcmd); }
ACMD(do_cdemote)  { clan_wrap(ch, "demote",  argument, cmd, subcmd); }
