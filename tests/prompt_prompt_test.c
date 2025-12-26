#include "conf.h"
#include "sysdep.h"

#include "structs.h"

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

/* Stubs and globals required by prompt.c */
struct descriptor_data *descriptor_list = NULL;
struct room_data *world = NULL;
struct zone_data *zone_table = NULL;
zone_rnum top_of_zone_table = 0;
room_rnum top_of_world = 0;
time_t boot_time = 0;

void basic_mud_log(const char *format, ...) { (void)format; }
size_t write_to_output(struct descriptor_data *d, const char *txt, ...) { (void)d; (void)txt; return 0; }
int level_exp(int class_num, int level) { (void)class_num; return level * 100; }
int compute_armor_class(struct char_data *ch) { (void) ch; return 0; }
struct time_info_data *real_time_passed(time_t t2, time_t t1) { static struct time_info_data dummy; (void) t2; (void) t1; return &dummy; }
void send_to_char(struct char_data *ch, const char *messg, ...) { (void) ch; (void) messg; }
void skip_spaces(char **string) { (void) string; }

#include "prompt.c"

static void init_test_character(struct descriptor_data *d, struct char_data *ch)
{
  struct player_special_data *ps = calloc(1, sizeof(struct player_special_data));

  memset(d, 0, sizeof(*d));
  memset(ch, 0, sizeof(*ch));

  ch->player_specials = ps;
  ch->desc = d;
  d->character = ch;
  d->connected = CON_PLAYING;
  d->output = d->small_outbuf;
  d->bufspace = SMALL_BUFSIZE - 1;

  ch->player.name = strdup("Tester");
  ch->player.title = strdup("");
  ch->player.level = 1;
  ch->player.chclass = 0;

  ch->points.hit = 42;
  ch->points.max_hit = 99;
  ch->points.mana = 10;
  ch->points.max_mana = 20;
  ch->points.move = 5;
  ch->points.max_move = 15;
  ch->points.exp = 50;
}

static int expect_string_equals(const char *label, const char *actual, const char *expected)
{
  if (strcmp(actual, expected) != 0) {
    fprintf(stderr, "%s: expected '%s' but got '%s'\n", label, expected, actual);
    return 1;
  }

  return 0;
}

static void set_prompt_from_input(struct char_data *ch, const char *input)
{
  char processed[MAX_PROMPT_LENGTH + 1];

  translate_prompt_escapes(input, processed, sizeof(processed));
  strlcpy(GET_PROMPT(ch), processed, MAX_PROMPT_LENGTH + 1);
}

int main(void)
{
  struct descriptor_data d;
  struct char_data ch;
  char buffer[MAX_PROMPT_LENGTH + 1];
  int failures = 0;

  init_test_character(&d, &ch);

  const char *rendered = make_prompt(&d);
  failures += expect_string_equals("default prompt", rendered, "[42 / 99] [10 / 20] [5 / 15] [150] ");

  translate_prompt_escapes("{R[%h{n", buffer, sizeof(buffer));
  failures += expect_string_equals("brace escape translation", buffer, "\x1B[1;31m[%h\x1B[0m");

  translate_prompt_escapes("\\tG%h\\tn", buffer, sizeof(buffer));
  failures += expect_string_equals("backslash escape translation", buffer, "\tG%h\tn");

  set_prompt_from_input(&ch, "{G[%h/{r%H]{n");
  build_custom_prompt(buffer, &d);
  failures += expect_string_equals("brace prompt render", buffer, "\x1B[1;32m[42/\x1B[0;31m99]\x1B[0m");

  set_prompt_from_input(&ch, "\\tR%h\\tn> ");
  build_custom_prompt(buffer, &d);
  failures += expect_string_equals("backslash prompt render", buffer, "\tR42\tn> ");

  return failures;
}
