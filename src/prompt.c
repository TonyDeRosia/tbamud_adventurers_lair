#include "conf.h"
#include "sysdep.h"

#include "structs.h"
#include "utils.h"
#include "class.h"
#include "comm.h"
#include "interpreter.h"
#include "prompt.h"

static const char *default_prompt_template = "[%h / %H] [%m / %M] [%v / %V] [%X] ";

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

static void build_custom_prompt(char *prompt, struct descriptor_data *d)
{
  const char *tpl = GET_PROMPT(d->character);
  size_t pos = 0;

  if (tpl == NULL || *tpl == '\0')
    tpl = default_prompt_template;

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

    switch (*tpl) {
    case '%':
      append_prompt_text(prompt, &pos, "%");
      break;
    case 'h':
      append_prompt_number(prompt, &pos, GET_HIT(d->character));
      break;
    case 'H':
      append_prompt_number(prompt, &pos, GET_MAX_HIT(d->character));
      break;
    case 'm':
      append_prompt_number(prompt, &pos, GET_MANA(d->character));
      break;
    case 'M':
      append_prompt_number(prompt, &pos, GET_MAX_MANA(d->character));
      break;
    case 'v':
      append_prompt_number(prompt, &pos, GET_MOVE(d->character));
      break;
    case 'V':
      append_prompt_number(prompt, &pos, GET_MAX_MOVE(d->character));
      break;
    case 'x':
      append_prompt_number(prompt, &pos, GET_EXP(d->character));
      break;
    case 'X':
      append_prompt_number(prompt, &pos, prompt_tnl(d->character));
      break;
    default:
      append_prompt_text(prompt, &pos, "%");
      append_prompt_text(prompt, &pos, (char[2]){ *tpl, '\0' });
      break;
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
  if (IS_NPC(ch))
    return;

  skip_spaces(&argument);

  if (!*argument) {
    const char *current = GET_PROMPT(ch);

    send_to_char(ch, "Your prompt is: %s\r\nDefault prompt is: %s\r\n",
                 (current && *current) ? current : default_prompt_template,
                 default_prompt_template);
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

  strlcpy(GET_PROMPT(ch), argument, MAX_PROMPT_LENGTH + 1);
  send_to_char(ch, "Prompt set.\r\n");
}
