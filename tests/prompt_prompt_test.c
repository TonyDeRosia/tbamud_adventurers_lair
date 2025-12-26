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

int main(void)
{
  struct descriptor_data d;
  struct char_data ch;

  init_test_character(&d, &ch);

  const char *rendered = make_prompt(&d);
  if (rendered == NULL)
    return 1;

  if (strcmp(rendered, "[42 / 99] [10 / 20] [5 / 15] [150] ") != 0)
    return 1;

  return 0;
}
