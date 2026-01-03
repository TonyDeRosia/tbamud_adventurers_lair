#include "conf.h"
#include "sysdep.h"

extern struct descriptor_data *descriptor_list;

#include "structs.h"
#include "utils.h"
#include "comm.h"
#include "interpreter.h"
#include "handler.h"
#include "db.h"
#include "clan.h"
#include "screen.h"
#include "modify.h"

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

    {
      const char *G = CBGRN(d->character, C_NRM);
      const char *R = CCNRM(d->character, C_NRM);

      if (from)
        send_to_char(d->character, "\r\n%s[Clan]%s %s%s%s: %s%s%s\r\n", G, R, G, GET_NAME(from), R, G, msg, R);
      else
        send_to_char(d->character, "\r\n%s[Clan]%s %s%s%s\r\n", G, R, G, msg, R);
    }
  }
}

static int is_valid_plain_clan_name(const char *s)
{
  /* For matching/commands: only letters and numbers, no color codes or spaces. */
  if (!s || !*s)
    return 0;

  for (size_t i = 0; s[i]; i++) {
    if (s[i] == '\\')
      return 0;
    if (isspace((unsigned char)s[i]))
      return 0;
    if (!isalnum((unsigned char)s[i]))
      return 0;
  }
  return 1;
}

static int is_valid_display_clan_name(const char *s)
{
  /* Allow: letters/numbers, and tbaMUD color codes like @R @n (stored as a tab
   * followed by a color code), no spaces. */
  if (!s || !*s)
    return 0;

  for (size_t i = 0; s[i]; i++) {
    if (s[i] == '\t') {
      if (!s[i + 1])
        return 0; /* A lone tab can't start a color code. */
      i++; /* Skip the color code character. */
      continue;
    }

    if (s[i] == '@')
      continue; /* Escaped @@ that survived parsing. */

    if (isspace((unsigned char)s[i]))
      return 0;
    if (!isalnum((unsigned char)s[i]))
      return 0;
  }
  return 1;
}

static void normalize_display_name(char *dest, size_t destlen, const char *src)
{
  /* Translate both legacy \tX sequences and builder-friendly @X into the
   * protocol tab codes used by the output layer. */
  size_t di = 0;

  for (size_t si = 0; src[si] && di + 1 < destlen; si++) {
    if (src[si] == '\\' && src[si + 1] == 't') {
      dest[di++] = '@';
      si++; /* Skip the 't'. */
    } else {
      dest[di++] = src[si];
    }
  }

  dest[di] = '\0';
  parse_at(dest);
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

static void roster_format_color_field(char *out, size_t outsz, const char *src, size_t width)
{
  size_t pos = 0, vis = 0;
  const char *p;

  if (!out || outsz == 0)
    return;

  out[0] = '\0';

  if (!src)
    src = "Unknown";

  for (p = src; *p && pos + 1 < outsz; ) {
    if (*p == '\t' && *(p + 1)) {
      if (pos + 2 >= outsz)
        break;
      out[pos++] = *p++;
      out[pos++] = *p++;
      continue;
    }

    if (vis >= width)
      break;

    out[pos++] = *p++;
    vis++;
  }

  while (vis < width && pos + 1 < outsz) {
    out[pos++] = ' ';
    vis++;
  }

  out[pos] = '\0';
}


static void clan_show_roster(struct char_data *ch)
{
  struct descriptor_data *d;
  struct char_data *tch;
  int clan_id, count = 0;
  const char *B = CCBLU(ch, C_NRM);
  const char *R = CCNRM(ch, C_NRM);
  const char *Y = CCYEL(ch, C_NRM);
  const char *C = CCCYN(ch, C_NRM);
  const char *clan_name = clan_display_name_by_id(GET_CLAN_ID(ch));
  char clan_name_buf[128];

  if (!clan_name)
    clan_name = "(unknown clan)";

  /* Leave one space of padding on either side of the clan name to fit the 70-char interior. */
  roster_format_color_field(clan_name_buf, sizeof(clan_name_buf), clan_name, 68);

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
    "%s╔══════════════════════════════════════════════════════════════════════╗%s\r\n"
    "%s║%s %s %s ║%s\r\n"
    "%s║%s                              %sClan Roster%s                             ║%s%s\r\n"
    "%s╠══════════════════════════════════════════════════════════════════════╣%s\r\n"
    "%s║%s %sName                     Race            Class            Level      %s║%s%s\r\n"
    "%s╠══════════════════════════════════════════════════════════════════════╣%s\r\n"
    , B, R,
      B, R, clan_name_buf, B, R,
      B, R, Y, R, B, R,
      B, R,
      B, R, C, R, B, R,
      B, R
  );

  if (count == 0) {
    send_to_char(ch,
      "%s║%s No clan members are currently online.                                %s║%s\r\n"
      , B, R, B, R
    );
  } else {
    for (i = 0; i < count; i++) {
      const char *rname = roster_race_name(GET_RACE(list[i]));
      const char *cname = roster_class_name(GET_CLASS(list[i]));
      char name_buf[64], race_buf[32], class_buf[32];

      roster_format_color_field(name_buf, sizeof(name_buf), GET_NAME(list[i]), 24);
      roster_format_color_field(race_buf, sizeof(race_buf), (rname ? rname : "Unknown"), 15);
      roster_format_color_field(class_buf, sizeof(class_buf), (cname ? cname : "Unknown"), 15);

      send_to_char(ch, "%s║%s %s %s %s %5d       %s║%s\r\n",
        B, R,
        name_buf,
        race_buf,
        class_buf,
        GET_LEVEL(list[i]),
        B, R
      );
    }
  }

  send_to_char(ch,
    "%s╚══════════════════════════════════════════════════════════════════════╝%s\r\n"
    "\r\n"
    , B, R
  );

  if (list)
    free(list);
}




/* Clan list (clist / clanlist). Shows existing clans and online member counts. */
struct clanlist_row {
  int id;
  const char *name;
  const char *display_name;
  int online;
};

static int clanlist_cmp(const void *a, const void *b)
{
  const struct clanlist_row *ra = (const struct clanlist_row *)a;
  const struct clanlist_row *rb = (const struct clanlist_row *)b;

  if (!ra->name && !rb->name) return 0;
  if (!ra->name) return 1;
  if (!rb->name) return -1;

  return strcasecmp(ra->name, rb->name);
}

/* Clan editor (basic) */
#define CLANEDIT_MAIN 0
#define CLANEDIT_SET_NAME 1
#define CLANEDIT_SET_DISPLAY_NAME 2
#define CLANEDIT_SET_TAG 3
#define CLANEDIT_CONFIRM_QUIT 4

static void clanedit_menu(struct descriptor_data *d)
{
  int id = d->clanedit_id;
  const char *nm = clan_name_by_id(id);
  const char *disp = clan_display_name_by_id(id);
  if (!nm) nm = "(unknown)";
  if (!disp) disp = "(unknown)";

  write_to_output(d,
    "\r\n"
    "Clan Editor\r\n"
    "Clan: [%d] %s\r\n"
    "Plain: %s\r\n"
    "\r\n"
    "1) Name (plain, used for commands)\r\n"
    "2) Display name (colors shown to players)\r\n"
    "3) Tag\r\n"
    "Q) Quit\r\n"
    "\r\n"
    "Enter choice: ",
    id, disp, nm);
}

void clanedit_parse(struct descriptor_data *d, char *arg)
{
  if (!d || !d->character) return;

  skip_spaces(&arg);
  if (!arg) arg = "";

  switch (d->clanedit_mode) {
    case CLANEDIT_SET_NAME:
      if (!*arg) {
        write_to_output(d, "Clan name cannot be empty. Enter new clan name: ");
        return;
      }

      if (!is_valid_plain_clan_name(arg)) {
        write_to_output(d, "Clan names may only use letters or numbers (no color codes). Try again: ");
        return;
      }

      if (strlen(arg) >= CLAN_NAME_LEN) {
        write_to_output(d, "Clan names must be under %d characters. Try again: ", CLAN_NAME_LEN);
        return;
      }

      if (!clan_set_name_and_save(d->clanedit_id, arg)) {
        write_to_output(d, "Unable to save the new clan name.\r\n");
        d->clanedit_mode = CLANEDIT_MAIN;
        clanedit_menu(d);
        return;
      }

      write_to_output(d, "Clan name updated to %s.\r\n", clan_name_by_id(d->clanedit_id));
      d->clanedit_mode = CLANEDIT_MAIN;
      clanedit_menu(d);
      return;

    case CLANEDIT_SET_DISPLAY_NAME:
      if (!*arg) {
        write_to_output(d, "Display name cannot be empty. Enter new display name: ");
        return;
      }

      char parsed_name[CLAN_DISPLAY_LEN];

      normalize_display_name(parsed_name, sizeof(parsed_name), arg);

      if (!is_valid_display_clan_name(parsed_name)) {
        write_to_output(d, "Display names may only use letters, numbers, and color codes. Try again: ");
        return;
      }

      if (strlen(parsed_name) >= CLAN_DISPLAY_LEN) {
        write_to_output(d, "Display names must be under %d characters. Try again: ", CLAN_DISPLAY_LEN);
        return;
      }

      if (!clan_set_display_name_and_save(d->clanedit_id, parsed_name)) {
        write_to_output(d, "Unable to save the new display name.\r\n");
        d->clanedit_mode = CLANEDIT_MAIN;
        clanedit_menu(d);
        return;
      }

      write_to_output(d, "Clan display name updated to %s.\r\n", clan_display_name_by_id(d->clanedit_id));
      d->clanedit_mode = CLANEDIT_MAIN;
      clanedit_menu(d);
      return;

    case CLANEDIT_SET_TAG:
      write_to_output(d, "\r\nTag edit is not persisted yet. Use file edits for now.\r\n");
      d->clanedit_mode = CLANEDIT_MAIN;
      clanedit_menu(d);
      return;

    case CLANEDIT_CONFIRM_QUIT:
      if (!*arg) {
        write_to_output(d, "Save changes before exiting? (Y/N): ");
        return;
      }

      switch (LOWER(*arg)) {
        case 'y':
          if (clan_save_all())
            write_to_output(d, "Clans saved. Exiting clan editor.\r\n");
          else
            write_to_output(d, "Could not save clans, exiting anyway.\r\n");
          break;

        case 'n':
          write_to_output(d, "Exiting clan editor without saving.\r\n");
          break;

        default:
          write_to_output(d, "Please enter Y or N: ");
          return;
      }

      d->clanedit_id = 0;
      d->clanedit_mode = CLANEDIT_MAIN;
      STATE(d) = CON_PLAYING;
      return;

    default:
      break;
  }

  if (!*arg) {
    clanedit_menu(d);
    return;
  }

  switch (LOWER(*arg)) {
    case '1':
      d->clanedit_mode = CLANEDIT_SET_NAME;
      write_to_output(d, "Enter new clan name: ");
      return;
    case '2':
      d->clanedit_mode = CLANEDIT_SET_DISPLAY_NAME;
      write_to_output(d, "Enter new clan display name (colors allowed): ");
      return;
    case '3':
      d->clanedit_mode = CLANEDIT_SET_TAG;
      write_to_output(d, "Enter new clan tag (shown in who): ");
      return;
    case 'q':
      d->clanedit_mode = CLANEDIT_CONFIRM_QUIT;
      write_to_output(d, "Save changes before exiting? (Y/N): ");
      return;
    default:
      write_to_output(d, "Invalid choice.\r\n");
      clanedit_menu(d);
      return;
  }
}

ACMD(do_clist)
{
  struct clanlist_row rows[256];
  int n = 0, i;

  if (IS_NPC(ch))
    return;

  /* Build list from clan ids. */
#ifdef MAX_CLANS
  int max_id = MAX_CLANS;
#else
  /* Fallback if MAX_CLANS is not visible for some reason. */
  int max_id = clan_next_id();
#endif

  if (max_id > (int)(sizeof(rows) / sizeof(rows[0])))
    max_id = (int)(sizeof(rows) / sizeof(rows[0]));

  for (i = 1; i < max_id; i++) {
    const char *nm = NULL;
    if (!clan_exists(i))
      continue;

    nm = clan_name_by_id(i);
    if (!nm || !*nm)
      continue;

    rows[n].id = i;
    rows[n].name = nm;
    rows[n].display_name = clan_display_name_by_id(i);
    rows[n].online = 0;
    n++;
  }

  if (n <= 0) {
    send_to_char(ch, "No clans found.\r\n");
    return;
  }

  /* Count online members for each clan. */
  for (i = 0; i < n; i++) {
    struct descriptor_data *d;
    for (d = descriptor_list; d; d = d->next) {
      struct char_data *tch;
      if (!d->character)
        continue;
      tch = d->character;
      if (!IS_PLAYING(d))
        continue;
      if (IS_NPC(tch))
        continue;
      if (GET_CLAN_ID(tch) == rows[i].id)
        rows[i].online++;
    }
  }

  qsort(rows, n, sizeof(rows[0]), clanlist_cmp);

  send_to_char(ch, "Clans\r\n");
  send_to_char(ch, "-----\r\n");
  for (i = 0; i < n; i++) {
    /* Keep it compact, like who/finger style. */
    send_to_char(ch, "%-20s Online: %d\r\n", rows[i].display_name ? rows[i].display_name : rows[i].name, rows[i].online);
  }
  send_to_char(ch, "\r\n%d clan%s listed.\r\n", n, (n == 1 ? "" : "s"));
}



/* Clan editor entry point. Full editor will be implemented after state plumbing is stable. */
ACMD(do_clanedit)
{
  int id = 0;

  if (IS_NPC(ch))
    return;

  if (!ch->desc) {
    send_to_char(ch, "Clanedit requires a live descriptor.\r\n");
    return;
  }

  skip_spaces(&argument);

  if (!*argument) {
    send_to_char(ch, "Usage: clanedit <clan name>\r\n");
    return;
  }

  if (is_number(argument)) {
    id = atoi(argument);
  } else {
    id = clan_id_by_name(argument);
  }

  if (id <= 0 || !clan_name_by_id(id)) {
    send_to_char(ch, "Clan not found.\r\n");
    return;
  }

  if (GET_LEVEL(ch) < LVL_IMPL && GET_CLAN_ID(ch) != id) {
    send_to_char(ch, "You do not have permission to edit that clan.\r\n");
    return;
  }

  ch->desc->clanedit_id = id;
  ch->desc->clanedit_mode = CLANEDIT_MAIN;
  STATE(ch->desc) = CON_CLANEDIT;

  clanedit_menu(ch->desc);
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
  char arg1[MAX_INPUT_LENGTH];
  char arg2[MAX_INPUT_LENGTH];
  int clan_id = 0;

  if (IS_NPC(ch))
    return;

  skip_spaces(&argument);
  if (!*argument) {
    send_to_char(ch, "Usage:\r\n  clan <message>\r\n  cjoin <clan name or id>\r\n  cquit\r\n");
    return;
  }

  /* Parse first word. We allow both: "clan join X" and wrapper calls like cjoin -> "join X". */
  argument = any_one_arg(argument, arg1);

  /* Helper: resolve clan id by numeric id or exact name match using clan_name_by_id(). */
  if (!str_cmp(arg1, "join")) {
    if (GET_CLAN_ID(ch) > 0) {
      send_to_char(ch, "You are already in a clan.\r\n");
      return;
    }

    skip_spaces(&argument);
    if (!*argument) {
      send_to_char(ch, "Usage: cjoin <clan name or id>\r\n");
      return;
    }

    argument = any_one_arg(argument, arg2);

    clan_id = is_number(arg2) ? atoi(arg2) : clan_id_by_name(arg2);

    if (clan_id <= 0 || !clan_name_by_id(clan_id)) {
      send_to_char(ch, "No such clan.\r\n");
      return;
    }

    SET_CLAN_ID(ch, clan_id);
    SET_CLAN_RANK(ch, CLAN_RANK_MEMBER);

    send_to_char(ch, "You have joined %s.\r\n", clan_display_name_by_id(clan_id));

    /* Save if available in your codebase; harmless if you remove it later. */
    save_char(ch);
    return;
  }

  if (!str_cmp(arg1, "quit")) {
    if (GET_CLAN_ID(ch) <= 0) {
      send_to_char(ch, "You are not in a clan.\r\n");
      return;
    }
    send_to_char(ch, "You have left %s.\r\n", clan_display_name_by_id(GET_CLAN_ID(ch)));
    SET_CLAN_ID(ch, 0);
    SET_CLAN_RANK(ch, 0);
    save_char(ch);
    return;
  }

  if (!str_cmp(arg1, "invite") || !str_cmp(arg1, "promote") || !str_cmp(arg1, "demote")) {
    if (GET_CLAN_ID(ch) <= 0) {
      send_to_char(ch, "You are not in a clan.\r\n");
      return;
    }
    send_to_char(ch, "That clan function is not implemented yet.\r\n");
    return;
  }

  /* Otherwise treat as clan chat. The wrapper also lands here for "join" only if something went wrong. */
  if (GET_CLAN_ID(ch) <= 0) {
    send_to_char(ch, "You are not in a clan.\r\n");
    return;
  }

  /* Rebuild the message with arg1 included. */
  {
    char msg[MAX_INPUT_LENGTH * 2];
    if (*argument)
      snprintf(msg, sizeof(msg), "%s %s", arg1, argument);
    else
      snprintf(msg, sizeof(msg), "%s", arg1);
    send_to_clan(GET_CLAN_ID(ch), msg, ch);
  }
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
    send_to_char(ch, "Tip: keep the base name plain; set colors later in clanedit.\r\n");
    return;
  }

  if (!is_valid_plain_clan_name(name)) {
    send_to_char(ch, "Invalid clan name. Use letters and numbers only (no color codes).\r\n");
    send_to_char(ch, "Tip: set the colored display with 'clanedit' after creation.\r\n");
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
