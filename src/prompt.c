#include "conf.h"
#include "sysdep.h"

#include "structs.h"
#include "utils.h"
#include "class.h"
#include "comm.h"
#include "constants.h"
#include "db.h"
#include "handler.h"
#include "oasis.h"
#include "pfdefaults.h"
#include "prompt.h"

#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

#ifndef MAX
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif

static size_t append_prompt(char *prompt, size_t len, size_t size, const char *format, ...);
static int prompt_tnl(struct char_data *ch);
static int prompt_percent(int current, int maximum);
static size_t append_prompt_str(char *prompt, size_t len, size_t size, const char *value);
static size_t append_prompt_int(char *prompt, size_t len, size_t size, long value);
static size_t append_prompt_token(struct descriptor_data *d, char *prompt, size_t len, size_t size, const char *token, size_t token_len);
static size_t expand_prompt_template(struct descriptor_data *d, const char *template, char *prompt, size_t len, size_t size);

static size_t append_prompt(char *prompt, size_t len, size_t size, const char *format, ...)
{
  va_list args;

  va_start(args, format);
  int count = vsnprintf(prompt + len, size - len, format, args);
  va_end(args);

  if (count < 0)
    return len;

  return MIN(size, len + (size_t) count);
}

static int prompt_tnl(struct char_data *ch)
{
  if (GET_LEVEL(ch) >= LVL_IMMORT)
    return 0;

  int next_level = GET_LEVEL(ch) + 1;
  return MAX(0, level_exp(GET_CLASS(ch), next_level) - GET_EXP(ch));
}

static int prompt_percent(int current, int maximum)
{
  if (maximum <= 0)
    maximum = 1;

  return (100 * current) / maximum;
}

static size_t append_prompt_str(char *prompt, size_t len, size_t size, const char *value)
{
  return append_prompt(prompt, len, size, "%s", value);
}

static size_t append_prompt_int(char *prompt, size_t len, size_t size, long value)
{
  return append_prompt(prompt, len, size, "%ld", value);
}

static size_t prompt_token_hp_current(struct descriptor_data *d, char *prompt, size_t len, size_t size)
{
  return append_prompt_int(prompt, len, size, GET_HIT(d->character));
}

static size_t prompt_token_hp_max(struct descriptor_data *d, char *prompt, size_t len, size_t size)
{
  return append_prompt_int(prompt, len, size, GET_MAX_HIT(d->character));
}

static size_t prompt_token_hp_percent(struct descriptor_data *d, char *prompt, size_t len, size_t size)
{
  return append_prompt_int(prompt, len, size, prompt_percent(GET_HIT(d->character), GET_MAX_HIT(d->character)));
}

static size_t prompt_token_mana_current(struct descriptor_data *d, char *prompt, size_t len, size_t size)
{
  return append_prompt_int(prompt, len, size, GET_MANA(d->character));
}

static size_t prompt_token_mana_max(struct descriptor_data *d, char *prompt, size_t len, size_t size)
{
  return append_prompt_int(prompt, len, size, GET_MAX_MANA(d->character));
}

static size_t prompt_token_mana_percent(struct descriptor_data *d, char *prompt, size_t len, size_t size)
{
  return append_prompt_int(prompt, len, size, prompt_percent(GET_MANA(d->character), GET_MAX_MANA(d->character)));
}

static size_t prompt_token_move_current(struct descriptor_data *d, char *prompt, size_t len, size_t size)
{
  return append_prompt_int(prompt, len, size, GET_MOVE(d->character));
}

static size_t prompt_token_move_max(struct descriptor_data *d, char *prompt, size_t len, size_t size)
{
  return append_prompt_int(prompt, len, size, GET_MAX_MOVE(d->character));
}

static size_t prompt_token_move_percent(struct descriptor_data *d, char *prompt, size_t len, size_t size)
{
  return append_prompt_int(prompt, len, size, prompt_percent(GET_MOVE(d->character), GET_MAX_MOVE(d->character)));
}

static size_t prompt_token_strength(struct descriptor_data *d, char *prompt, size_t len, size_t size)
{
  return append_prompt_int(prompt, len, size, GET_STR(d->character));
}

static size_t prompt_token_intelligence(struct descriptor_data *d, char *prompt, size_t len, size_t size)
{
  return append_prompt_int(prompt, len, size, GET_INT(d->character));
}

static size_t prompt_token_wisdom(struct descriptor_data *d, char *prompt, size_t len, size_t size)
{
  return append_prompt_int(prompt, len, size, GET_WIS(d->character));
}

static size_t prompt_token_dexterity(struct descriptor_data *d, char *prompt, size_t len, size_t size)
{
  return append_prompt_int(prompt, len, size, GET_DEX(d->character));
}

static size_t prompt_token_constitution(struct descriptor_data *d, char *prompt, size_t len, size_t size)
{
  return append_prompt_int(prompt, len, size, GET_CON(d->character));
}

static size_t prompt_token_charisma(struct descriptor_data *d, char *prompt, size_t len, size_t size)
{
  return append_prompt_int(prompt, len, size, GET_CHA(d->character));
}

static size_t prompt_token_char_name(struct descriptor_data *d, char *prompt, size_t len, size_t size)
{
  return append_prompt_str(prompt, len, size, GET_NAME(d->character));
}

static size_t prompt_token_level(struct descriptor_data *d, char *prompt, size_t len, size_t size)
{
  return append_prompt_int(prompt, len, size, GET_LEVEL(d->character));
}

static size_t prompt_token_class(struct descriptor_data *d, char *prompt, size_t len, size_t size)
{
  return append_prompt_str(prompt, len, size, pc_class_types[(int) GET_CLASS(d->character)]);
}

static size_t prompt_token_race(struct descriptor_data *d, char *prompt, size_t len, size_t size)
{
  return append_prompt_str(prompt, len, size, "(none)");
}

static size_t prompt_token_sex(struct descriptor_data *d, char *prompt, size_t len, size_t size)
{
  const char *abbr = "N";

  switch (GET_SEX(d->character)) {
  case SEX_MALE:
    abbr = "M";
    break;
  case SEX_FEMALE:
    abbr = "F";
    break;
  default:
    abbr = "N";
    break;
  }

  return append_prompt_str(prompt, len, size, abbr);
}

static size_t prompt_token_title(struct descriptor_data *d, char *prompt, size_t len, size_t size)
{
  return append_prompt_str(prompt, len, size, GET_TITLE(d->character));
}

static size_t prompt_token_exp(struct descriptor_data *d, char *prompt, size_t len, size_t size)
{
  return append_prompt_int(prompt, len, size, GET_EXP(d->character));
}

static size_t prompt_token_tnl(struct descriptor_data *d, char *prompt, size_t len, size_t size)
{
  return append_prompt_int(prompt, len, size, prompt_tnl(d->character));
}

static size_t prompt_token_gold(struct descriptor_data *d, char *prompt, size_t len, size_t size)
{
  return append_prompt_int(prompt, len, size, GET_GOLD(d->character));
}

static size_t prompt_token_bank(struct descriptor_data *d, char *prompt, size_t len, size_t size)
{
  return append_prompt_int(prompt, len, size, GET_BANK_GOLD(d->character));
}

static size_t prompt_token_quest_points(struct descriptor_data *d, char *prompt, size_t len, size_t size)
{
  return append_prompt_int(prompt, len, size, GET_QUESTPOINTS(d->character));
}

static size_t prompt_token_practices(struct descriptor_data *d, char *prompt, size_t len, size_t size)
{
  return append_prompt_int(prompt, len, size, GET_PRACTICES(d->character));
}

static size_t prompt_token_ac(struct descriptor_data *d, char *prompt, size_t len, size_t size)
{
  return append_prompt_int(prompt, len, size, GET_AC(d->character));
}

static size_t prompt_token_hitroll(struct descriptor_data *d, char *prompt, size_t len, size_t size)
{
  return append_prompt_int(prompt, len, size, GET_HITROLL(d->character));
}

static size_t prompt_token_damroll(struct descriptor_data *d, char *prompt, size_t len, size_t size)
{
  return append_prompt_int(prompt, len, size, GET_DAMROLL(d->character));
}

static size_t prompt_token_alignment(struct descriptor_data *d, char *prompt, size_t len, size_t size)
{
  return append_prompt_int(prompt, len, size, GET_ALIGNMENT(d->character));
}

static size_t prompt_token_fighting_target(struct descriptor_data *d, char *prompt, size_t len, size_t size)
{
  if (FIGHTING(d->character) == NULL)
    return len;

  return append_prompt_str(prompt, len, size, GET_NAME(FIGHTING(d->character)));
}

static size_t prompt_token_fighting_percent(struct descriptor_data *d, char *prompt, size_t len, size_t size)
{
  if (FIGHTING(d->character) == NULL)
    return len;

  return append_prompt_int(prompt, len, size, prompt_percent(GET_HIT(FIGHTING(d->character)), GET_MAX_HIT(FIGHTING(d->character))));
}

static size_t prompt_token_position(struct descriptor_data *d, char *prompt, size_t len, size_t size)
{
  return append_prompt_str(prompt, len, size, position_types[(int) GET_POS(d->character)]);
}

static size_t prompt_token_room(struct descriptor_data *d, char *prompt, size_t len, size_t size)
{
  if (IN_ROOM(d->character) == NOWHERE)
    return len;

  return append_prompt_int(prompt, len, size, GET_ROOM_VNUM(IN_ROOM(d->character)));
}

static size_t prompt_token_zone(struct descriptor_data *d, char *prompt, size_t len, size_t size)
{
  if (IN_ROOM(d->character) == NOWHERE)
    return len;

  return append_prompt_int(prompt, len, size, world[IN_ROOM(d->character)].zone);
}

static size_t prompt_token_invis(struct descriptor_data *d, char *prompt, size_t len, size_t size)
{
  if (GET_INVIS_LEV(d->character) == 0)
    return len;

  return append_prompt(prompt, len, size, "i%d", GET_INVIS_LEV(d->character));
}

static size_t prompt_token_olc(struct descriptor_data *d, char *prompt, size_t len, size_t size)
{
  const char *mode = olc_modes(d);

  if (mode == NULL)
    return len;

  if (OLC_ZNUM(d) != NOTHING)
    return append_prompt(prompt, len, size, "%s:%d", mode, OLC_NUM(d));

  return append_prompt_str(prompt, len, size, mode);
}

static size_t prompt_token_player_count(struct descriptor_data *d, char *prompt, size_t len, size_t size)
{
  int count = 0;
  struct descriptor_data *desc;

  for (desc = descriptor_list; desc; desc = desc->next) {
    if (STATE(desc) == CON_PLAYING && desc->character && !IS_NPC(desc->character))
      count++;
  }

  return append_prompt_int(prompt, len, size, count);
}

static size_t prompt_token_uptime(struct descriptor_data *d, char *prompt, size_t len, size_t size)
{
  time_t uptime = time(0) - boot_time;
  int days = uptime / 86400;
  int hours = (uptime / 3600) % 24;
  int minutes = (uptime / 60) % 60;

  return append_prompt(prompt, len, size, "%dd %02d:%02d", days, hours, minutes);
}

struct prompt_token_entry {
  const char *token;
  const char *description;
  bool restriction;
  size_t (*render)(struct descriptor_data *d, char *prompt, size_t len, size_t size);
};

/* Add new prompt tokens by adding a single entry to this table. */
static const struct prompt_token_entry prompt_tokens[] = {
  {"h",   "Current hit points",                         FALSE, prompt_token_hp_current},
  {"H",   "Maximum hit points",                         FALSE, prompt_token_hp_max},
  {"p",   "Hit point percent",                         FALSE, prompt_token_hp_percent},
  {"m",   "Current mana",                               FALSE, prompt_token_mana_current},
  {"M",   "Maximum mana",                               FALSE, prompt_token_mana_max},
  {"q",   "Mana percent",                               FALSE, prompt_token_mana_percent},
  {"v",   "Current move points",                        FALSE, prompt_token_move_current},
  {"V",   "Maximum move points",                        FALSE, prompt_token_move_max},
  {"P",   "Move percent",                               FALSE, prompt_token_move_percent},
  {"n",   "Character name",                             FALSE, prompt_token_char_name},
  {"l",   "Character level",                            FALSE, prompt_token_level},
  {"c",   "Class name",                                 FALSE, prompt_token_class},
  {"r",   "Race name (if enabled)",                     FALSE, prompt_token_race},
  {"s",   "Sex/Gender",                                 FALSE, prompt_token_sex},
  {"t",   "Title",                                      FALSE, prompt_token_title},
  {"x",   "Current experience",                         FALSE, prompt_token_exp},
  {"X",   "Experience to next level",                   FALSE, prompt_token_tnl},
  {"f",   "Current fight target",                       FALSE, prompt_token_fighting_target},
  {"F",   "Fight target hit percent",                   FALSE, prompt_token_fighting_percent},
  {"pos", "Current position",                           FALSE, prompt_token_position},
  {"room","Room vnum",                                  FALSE, prompt_token_room},
  {"zone","Zone number",                                FALSE, prompt_token_zone},
  {"inv", "Invisibility level",                         TRUE,  prompt_token_invis},
  {"olc", "OLC editing context",                        TRUE,  prompt_token_olc},
  {"players", "Connected player count",                 TRUE,  prompt_token_player_count},
  {"uptime",  "Server uptime",                           TRUE,  prompt_token_uptime},
  {"str",  "Strength score",                             FALSE, prompt_token_strength},
  {"int",  "Intelligence score",                         FALSE, prompt_token_intelligence},
  {"wis",  "Wisdom score",                               FALSE, prompt_token_wisdom},
  {"dex",  "Dexterity score",                            FALSE, prompt_token_dexterity},
  {"con",  "Constitution score",                         FALSE, prompt_token_constitution},
  {"cha",  "Charisma score",                             FALSE, prompt_token_charisma},
  {"lvl",  "Character level",                            FALSE, prompt_token_level},
  {"level","Character level",                            FALSE, prompt_token_level},
  {"exp",  "Current experience",                         FALSE, prompt_token_exp},
  {"tnl",  "Experience to next level",                   FALSE, prompt_token_tnl},
  {"gold", "Gold on hand",                                FALSE, prompt_token_gold},
  {"bank", "Gold in bank",                                FALSE, prompt_token_bank},
  {"qp",   "Quest points",                                FALSE, prompt_token_quest_points},
  {"prac", "Practice sessions",                           FALSE, prompt_token_practices},
  {"ac",   "Armor class",                                 FALSE, prompt_token_ac},
  {"hr",   "Hitroll",                                     FALSE, prompt_token_hitroll},
  {"dr",   "Damroll",                                     FALSE, prompt_token_damroll},
  {"align","Alignment",                                   FALSE, prompt_token_alignment},
};

#define NUM_PROMPT_TOKENS (sizeof(prompt_tokens) / sizeof(prompt_tokens[0]))

static size_t append_prompt_token(struct descriptor_data *d, char *prompt, size_t len, size_t size, const char *token, size_t token_len)
{
  size_t i;

  for (i = 0; i < NUM_PROMPT_TOKENS; i++) {
    const struct prompt_token_entry *entry = &prompt_tokens[i];

    if (strlen(entry->token) != token_len || strncmp(entry->token, token, token_len))
      continue;

    if (entry->restriction && GET_LEVEL(d->character) < LVL_IMMORT)
      return len;

    return entry->render(d, prompt, len, size);
  }

  /* TODO: Register unsupported prompt tokens in prompt_tokens to activate them. */
  return len;
}

static size_t expand_prompt_template(struct descriptor_data *d, const char *template, char *prompt, size_t len, size_t size)
{
  const char *src = template;

  while (*src != '\0' && len < size) {
    if (*src != '%') {
      prompt[len++] = *src++;
      prompt[len] = '\0';
      continue;
    }

    src++;

    if (*src == '\0')
      break;

    if (*src == '%') {
      prompt[len++] = '%';
      prompt[len] = '\0';
      src++;
      continue;
    }

    const char *token_start = src;
    size_t tok_len = 0;

    if (*src == '(') {
      src++;
      token_start = src;

      while (*src != '\0' && *src != ')') {
        tok_len++;
        src++;
      }

      if (*src == '\0') {
        prompt[len++] = '%';
        prompt[len] = '\0';
        break;
      }

      len = append_prompt_token(d, prompt, len, size, token_start, tok_len);
      src++;
    } else {
      while (token_start[tok_len] != '\0' && !isspace((int) token_start[tok_len]))
        tok_len++;

      len = append_prompt_token(d, prompt, len, size, token_start, tok_len);
      src += tok_len;
    }
  }

  if (size > 0)
    prompt[MIN(len, size - 1)] = '\0';

  return len;
}

char *make_prompt(struct descriptor_data *d)
{
  static char prompt[MAX_PROMPT_LENGTH];

  if (d->showstr_count)
    snprintf(prompt, sizeof(prompt),
      "[ Return to continue, (q)uit, (r)efresh, (b)ack, or page number (%d/%d) ]",
      d->showstr_page, d->showstr_count);
  else if (d->str)
    strcpy(prompt, "] ");       /* strcpy: OK (for 'MAX_PROMPT_LENGTH >= 3') */
  else if (STATE(d) == CON_PLAYING && !IS_NPC(d->character)) {
    size_t len = 0;
    const char *prompt_template;

    if (*GET_PROMPT(d->character) == '\0')
      strlcpy(GET_PROMPT(d->character), PFDEF_PROMPT, MAX_PROMPT_LENGTH + 1);

    prompt_template = GET_PROMPT(d->character);

    *prompt = '\0';

    if (GET_INVIS_LEV(d->character) && len < sizeof(prompt))
      len = append_prompt(prompt, len, sizeof(prompt), "i%d ", GET_INVIS_LEV(d->character));

    len = expand_prompt_template(d, prompt_template, prompt, len, sizeof(prompt));

    if (PRF_FLAGGED(d->character, PRF_BUILDWALK) && len < sizeof(prompt))
      len = append_prompt(prompt, len, sizeof(prompt), " BUILDWALKING ");

    if (PRF_FLAGGED(d->character, PRF_AFK) && len < sizeof(prompt))
      len = append_prompt(prompt, len, sizeof(prompt), " AFK ");

    if (GET_LAST_NEWS(d->character) < newsmod)
      len = append_prompt(prompt, len, sizeof(prompt), " (news) ");

    if (GET_LAST_MOTD(d->character) < motdmod)
      len = append_prompt(prompt, len, sizeof(prompt), " (motd) ");
  } else if (STATE(d) == CON_PLAYING && IS_NPC(d->character))
    snprintf(prompt, sizeof(prompt), "%s> ", GET_NAME(d->character));
  else
    *prompt = '\0';

  return prompt;
}

void set_prompt_template(struct char_data *ch, const char *template)
{
  if (*template == '\0')
    strlcpy(GET_PROMPT(ch), PFDEF_PROMPT, MAX_PROMPT_LENGTH + 1);
  else
    strlcpy(GET_PROMPT(ch), template, MAX_PROMPT_LENGTH + 1);
}

void queue_prompt(struct descriptor_data *d)
{
  if (d == NULL || d->character == NULL || IS_NPC(d->character))
    return;

  if (STATE(d) != CON_PLAYING)
    return;

  write_to_output(d, "%s", make_prompt(d));
}
