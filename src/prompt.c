#include "conf.h"
#include "sysdep.h"

#include "structs.h"
#include "utils.h"
#include "class.h"
#include "comm.h"
#include "interpreter.h"
#include "quest.h"
#include "prompt.h"

static const char *default_prompt_template = "[%h / %H] [%m / %M] [%v / %V] [%X] ";

static const char *translate_color_brace(char code)
{
  switch (code) {
  case 'n': case 'N': case 'x': case 'X': return "\tn"; /* normal/reset */
  case 'd': return "\td"; /* dark grey / black */
  case 'D': return "\tD"; /* light grey */
  case 'a': return "\ta"; /* dark azure */
  case 'A': return "\tA"; /* light azure */
  case 'r': return "\tr"; /* dark red */
  case 'R': return "\tR"; /* light red */
  case 'g': return "\tg"; /* dark green */
  case 'G': return "\tG"; /* light green */
  case 'y': return "\ty"; /* dark yellow */
  case 'Y': return "\tY"; /* light yellow */
  case 'b': return "\tb"; /* dark blue */
  case 'B': return "\tB"; /* light blue */
  case 'm': return "\tm"; /* dark magenta */
  case 'M': return "\tM"; /* light magenta */
  case 'c': return "\tc"; /* dark cyan */
  case 'C': return "\tC"; /* light cyan */
  case 'w': return "\tw"; /* dark white */
  case 'W': return "\tW"; /* light white */
  case 'o': return "\to"; /* dark orange */
  case 'O': return "\tO"; /* light orange */
  case 'p': return "\tp"; /* dark pink */
  case 'P': return "\tP"; /* light pink */
  default:  return NULL;
  }
}

static size_t translate_prompt_escapes(const char *src, char *dest, size_t dest_size)
{
  size_t pos = 0;

  if (dest_size == 0)
    return 0;

  for (; *src && pos + 1 < dest_size; src++) {
    if (*src == '\\' && src[1]) {
      src++;

      switch (*src) {
      case '\\':
        dest[pos++] = '\\';
        break;
      case 'e':
        dest[pos++] = '\x1B';
        break;
      case 't':
        dest[pos++] = '\t';
        break;
      case 'n':
        dest[pos++] = '\n';
        break;
      case 'r':
        dest[pos++] = '\r';
        break;
      case 'x':
        if (isxdigit((unsigned char) src[1]) && isxdigit((unsigned char) src[2])) {
          char hex[3] = { src[1], src[2], '\0' };

          dest[pos++] = (char) strtol(hex, NULL, 16);
          src += 2;
        } else {
          dest[pos++] = '\\';
          if (pos + 1 < dest_size)
            dest[pos++] = 'x';
        }
        break;
      default:
        dest[pos++] = *src;
        break;
      }
    } else if (*src == '{' && src[1]) {
      const char *color = translate_color_brace(src[1]);

      if (color) {
        size_t add_len = MIN(dest_size - pos - 1, strlen(color));

        memcpy(dest + pos, color, add_len);
        pos += add_len;
        src++;
      } else
        dest[pos++] = *src;
    } else
      dest[pos++] = *src;
  }

  dest[pos] = '\0';
  return pos;
}

static int prompt_tnl(struct char_data *ch)
{
  if (GET_LEVEL(ch) >= LVL_IMMORT)
    return 0;

  int next_level = GET_LEVEL(ch) + 1;
  return MAX(0, level_exp(GET_CLASS(ch), next_level) - GET_EXP(ch));
}

static void append_prompt_text(char *prompt, size_t *pos, const char *text)
{
  size_t remaining = MAX_PROMPT_LENGTH - *pos;
  size_t add_len = MIN(remaining, strlen(text));

  if (add_len > 0) {
    memcpy(prompt + *pos, text, add_len);
    *pos += add_len;
  }

  prompt[*pos] = '\0';
}

static void append_prompt_number(char *prompt, size_t *pos, int value)
{
  char numbuf[24];

  snprintf(numbuf, sizeof(numbuf), "%d", value);
  append_prompt_text(prompt, pos, numbuf);
}

static void append_prompt_percent(char *prompt, size_t *pos, struct descriptor_data *d)
{
  (void) d;
  append_prompt_text(prompt, pos, "%");
}

static void append_prompt_hit(char *prompt, size_t *pos, struct descriptor_data *d)
{
  append_prompt_number(prompt, pos, GET_HIT(d->character));
}

static void append_prompt_max_hit(char *prompt, size_t *pos, struct descriptor_data *d)
{
  append_prompt_number(prompt, pos, GET_MAX_HIT(d->character));
}

static void append_prompt_mana(char *prompt, size_t *pos, struct descriptor_data *d)
{
  append_prompt_number(prompt, pos, GET_MANA(d->character));
}

static void append_prompt_max_mana(char *prompt, size_t *pos, struct descriptor_data *d)
{
  append_prompt_number(prompt, pos, GET_MAX_MANA(d->character));
}

static void append_prompt_move(char *prompt, size_t *pos, struct descriptor_data *d)
{
  append_prompt_number(prompt, pos, GET_MOVE(d->character));
}

static void append_prompt_max_move(char *prompt, size_t *pos, struct descriptor_data *d)
{
  append_prompt_number(prompt, pos, GET_MAX_MOVE(d->character));
}

static void append_prompt_exp(char *prompt, size_t *pos, struct descriptor_data *d)
{
  append_prompt_number(prompt, pos, GET_EXP(d->character));
}

static void append_prompt_tnl(char *prompt, size_t *pos, struct descriptor_data *d)
{
  append_prompt_number(prompt, pos, prompt_tnl(d->character));
}

static void append_prompt_level(char *prompt, size_t *pos, struct descriptor_data *d)
{
  append_prompt_number(prompt, pos, GET_LEVEL(d->character));
}

static void append_prompt_armor(char *prompt, size_t *pos, struct descriptor_data *d)
{
  append_prompt_number(prompt, pos, compute_armor_class(d->character));
}

static void append_prompt_alignment(char *prompt, size_t *pos, struct descriptor_data *d)
{
  append_prompt_number(prompt, pos, GET_ALIGNMENT(d->character));
}

static void append_prompt_gold(char *prompt, size_t *pos, struct descriptor_data *d)
{
  append_prompt_number(prompt, pos, GET_GOLD(d->character));
}

static void append_prompt_quest_points(char *prompt, size_t *pos, struct descriptor_data *d)
{
  append_prompt_number(prompt, pos, GET_QUESTPOINTS(d->character));
}

static void append_prompt_completed_quests(char *prompt, size_t *pos, struct descriptor_data *d)
{
  append_prompt_number(prompt, pos, GET_NUM_QUESTS(d->character));
}

static void append_prompt_current_quest(char *prompt, size_t *pos, struct descriptor_data *d)
{
  if (GET_QUEST(d->character) == NOTHING)
    append_prompt_number(prompt, pos, 0);
  else
    append_prompt_number(prompt, pos, GET_QUEST(d->character));
}

static void append_prompt_position(char *prompt, size_t *pos, struct descriptor_data *d)
{
  const char *position = position_types[(int) GET_POS(d->character)];

  if (position == NULL || *position == '\n')
    position = "Unknown";

  append_prompt_text(prompt, pos, position);
}

static void append_prompt_room_vnum(char *prompt, size_t *pos, struct descriptor_data *d)
{
  if (IN_ROOM(d->character) == NOWHERE)
    append_prompt_number(prompt, pos, NOWHERE);
  else
    append_prompt_number(prompt, pos, GET_ROOM_VNUM(IN_ROOM(d->character)));
}

static void append_prompt_name(char *prompt, size_t *pos, struct descriptor_data *d)
{
  append_prompt_text(prompt, pos, GET_NAME(d->character));
}

static void append_prompt_age(char *prompt, size_t *pos, struct descriptor_data *d)
{
  append_prompt_number(prompt, pos, GET_AGE(d->character));
}

static void append_prompt_playtime(char *prompt, size_t *pos, struct descriptor_data *d)
{
  struct time_info_data playing_time = *real_time_passed(
    (time(0) - d->character->player.time.logon) + d->character->player.time.played, 0);
  char buf[32];

  snprintf(buf, sizeof(buf), "%dd %dh", playing_time.day, playing_time.hours);
  append_prompt_text(prompt, pos, buf);
}

struct prompt_token_info {
  char code;
  const char *description;
  void (*append)(char *prompt, size_t *pos, struct descriptor_data *d);
};

static const struct prompt_token_info prompt_tokens[] = {
  { '%', "Literal %%", append_prompt_percent },
  { 'h', "Current hit points", append_prompt_hit },
  { 'H', "Maximum hit points", append_prompt_max_hit },
  { 'm', "Current mana", append_prompt_mana },
  { 'M', "Maximum mana", append_prompt_max_mana },
  { 'v', "Current movement", append_prompt_move },
  { 'V', "Maximum movement", append_prompt_max_move },
  { 'x', "Total experience", append_prompt_exp },
  { 'X', "Experience to next level", append_prompt_tnl },
  { 'l', "Current level", append_prompt_level },
  { 'a', "Armor class", append_prompt_armor },
  { 'A', "Alignment", append_prompt_alignment },
  { 'g', "Gold carried", append_prompt_gold },
  { 'q', "Quest points", append_prompt_quest_points },
  { 'Q', "Completed quests", append_prompt_completed_quests },
  { 'c', "Current quest vnum (0 if none)", append_prompt_current_quest },
  { 'p', "Position (standing, etc)", append_prompt_position },
  { 'r', "Current room vnum", append_prompt_room_vnum },
  { 'n', "Character name", append_prompt_name },
  { 'y', "Age in years", append_prompt_age },
  { 't', "Total playtime (days/hours)", append_prompt_playtime },
};

static const struct prompt_token_info *find_prompt_token(char code)
{
  size_t i;

  for (i = 0; i < sizeof(prompt_tokens) / sizeof(prompt_tokens[0]); i++)
    if (prompt_tokens[i].code == code)
      return &prompt_tokens[i];

  return NULL;
}

static void build_custom_prompt(char *prompt, struct descriptor_data *d)
{
  const char *tpl = GET_PROMPT(d->character);
  char processed_tpl[MAX_PROMPT_LENGTH + 1];
  size_t pos = 0;

  if (tpl == NULL || *tpl == '\0')
    tpl = default_prompt_template;

  translate_prompt_escapes(tpl, processed_tpl, sizeof(processed_tpl));
  tpl = processed_tpl;

  for (; *tpl && pos < MAX_PROMPT_LENGTH; tpl++) {
    if (*tpl != '%') {
      append_prompt_text(prompt, &pos, (char[2]){ *tpl, '\0' });
      continue;
    }

    tpl++;

    if (*tpl == '\0') {
      append_prompt_text(prompt, &pos, "%");
      break;
    }

    const struct prompt_token_info *token = find_prompt_token(*tpl);

    if (token)
      token->append(prompt, &pos, d);
    else {
      append_prompt_text(prompt, &pos, "%");
      append_prompt_text(prompt, &pos, (char[2]){ *tpl, '\0' });
    }
  }
}

char *make_prompt(struct descriptor_data *d)
{
  static char prompt[MAX_PROMPT_LENGTH];

  *prompt = '\0';

  if (d->showstr_count)
    snprintf(prompt, sizeof(prompt),
      "[ Return to continue, (q)uit, (r)efresh, (b)ack, or page number (%d/%d) ]",
      d->showstr_page, d->showstr_count);
  else if (d->str)
    strcpy(prompt, "] ");       /* strcpy: OK (for 'MAX_PROMPT_LENGTH >= 3') */
  else if (STATE(d) == CON_PLAYING && d->character)
    build_custom_prompt(prompt, d);

  return prompt;
}

void queue_prompt(struct descriptor_data *d)
{
  if (d == NULL || d->character == NULL || IS_NPC(d->character))
    return;

  if (STATE(d) != CON_PLAYING)
    return;

  write_to_output(d, "%s", make_prompt(d));
}

ACMD(do_prompt)
{
  char processed[MAX_PROMPT_LENGTH + 1];
  size_t processed_len = 0;

  if (IS_NPC(ch))
    return;

  skip_spaces(&argument);

  if (!*argument) {
    const char *current = GET_PROMPT(ch);
    size_t i;

    send_to_char(ch, "Your prompt is: %s\r\nDefault prompt is: %s\r\n",
                 (current && *current) ? current : default_prompt_template,
                 default_prompt_template);

    send_to_char(ch, "Use %%<code> to insert the values below:\r\n");

    for (i = 0; i < sizeof(prompt_tokens) / sizeof(prompt_tokens[0]); i++)
      send_to_char(ch, "  %%%c - %s\r\n", prompt_tokens[i].code, prompt_tokens[i].description);

    return;
  }

  if (!strcasecmp(argument, "reset")) {
    *GET_PROMPT(ch) = '\0';
    send_to_char(ch, "Prompt reset to default.\r\n");
    return;
  }

  if (strlen(argument) > MAX_PROMPT_LENGTH) {
    send_to_char(ch, "Prompt may not exceed %d characters.\r\n", MAX_PROMPT_LENGTH);
    return;
  }

  processed_len = translate_prompt_escapes(argument, processed, sizeof(processed));

  if (processed_len > MAX_PROMPT_LENGTH) {
    send_to_char(ch, "Prompt may not exceed %d characters after processing color codes.\r\n", MAX_PROMPT_LENGTH);
    return;
  }

  strlcpy(GET_PROMPT(ch), processed, MAX_PROMPT_LENGTH + 1);
  send_to_char(ch, "Prompt set.\r\n");
}
