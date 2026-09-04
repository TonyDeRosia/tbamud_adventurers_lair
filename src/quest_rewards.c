/* Adventurer's Lair - data-driven Quest Point reward catalog. */

#include "conf.h"
#include "sysdep.h"

#include "structs.h"
#include "utils.h"
#include "interpreter.h"
#include "comm.h"
#include "db.h"
#include "handler.h"
#include "quest.h"
#include "quest_rewards.h"

#define QUEST_REWARD_FILE LIB_MISC"quest_rewards.cfg"
#define QUEST_REWARD_MAX 200
#define QUEST_REWARD_ID_LEN 40
#define QUEST_REWARD_TYPE_LEN 20
#define QUEST_REWARD_ARG_LEN 40
#define QUEST_REWARD_NAME_LEN 120
#define QUEST_STAT_CAP 20

struct quest_reward_entry {
  char id[QUEST_REWARD_ID_LEN];
  int cost;
  char type[QUEST_REWARD_TYPE_LEN];
  int arg1;
  char arg2[QUEST_REWARD_ARG_LEN];
  char name[QUEST_REWARD_NAME_LEN];
};

static char *trim_ws(char *s)
{
  char *end;

  while (*s && isspace((unsigned char)*s))
    s++;

  if (!*s)
    return s;

  end = s + strlen(s) - 1;
  while (end > s && isspace((unsigned char)*end))
    *end-- = '\0';

  return s;
}

static int split_reward_line(char *line, char **fields, int max_fields)
{
  int count = 0;
  char *p = line;

  if (!line || !fields || max_fields <= 0)
    return 0;

  fields[count++] = p;
  while (*p && count < max_fields) {
    if (*p == '|') {
      *p = '\0';
      fields[count++] = p + 1;
    }
    p++;
  }

  return count;
}

static int load_quest_rewards(struct quest_reward_entry *entries, int max_entries)
{
  FILE *fl;
  char line[512];
  int count = 0, lineno = 0;

  if (!(fl = fopen(QUEST_REWARD_FILE, "r"))) {
    mudlog(BRF, LVL_IMMORT, TRUE,
           "SYSERR: Unable to open %s: %s",
           QUEST_REWARD_FILE, strerror(errno));
    return -1;
  }

  while (fgets(line, sizeof(line), fl) && count < max_entries) {
    char *fields[6], *s, *endptr;
    long cost, arg1;
    int n;

    lineno++;
    s = trim_ws(line);
    if (!*s || *s == '#')
      continue;

    n = split_reward_line(s, fields, 6);
    if (n != 6) {
      mudlog(BRF, LVL_IMMORT, TRUE,
             "SYSERR: %s line %d: expected 6 pipe-delimited fields.",
             QUEST_REWARD_FILE, lineno);
      continue;
    }

    for (n = 0; n < 6; n++)
      fields[n] = trim_ws(fields[n]);

    if (!*fields[0] || !*fields[2] || !*fields[5]) {
      mudlog(BRF, LVL_IMMORT, TRUE,
             "SYSERR: %s line %d: id, type, and display name are required.",
             QUEST_REWARD_FILE, lineno);
      continue;
    }

    errno = 0;
    cost = strtol(fields[1], &endptr, 10);
    if (errno || *trim_ws(endptr) || cost <= 0 || cost > INT_MAX) {
      mudlog(BRF, LVL_IMMORT, TRUE,
             "SYSERR: %s line %d: invalid QP cost '%s'.",
             QUEST_REWARD_FILE, lineno, fields[1]);
      continue;
    }

    errno = 0;
    arg1 = strtol(fields[3], &endptr, 10);
    if (errno || *trim_ws(endptr) || arg1 <= 0 || arg1 > INT_MAX) {
      mudlog(BRF, LVL_IMMORT, TRUE,
             "SYSERR: %s line %d: invalid arg1 '%s'.",
             QUEST_REWARD_FILE, lineno, fields[3]);
      continue;
    }

    snprintf(entries[count].id, sizeof(entries[count].id), "%s", fields[0]);
    snprintf(entries[count].type, sizeof(entries[count].type), "%s", fields[2]);
    snprintf(entries[count].arg2, sizeof(entries[count].arg2), "%s", fields[4]);
    snprintf(entries[count].name, sizeof(entries[count].name), "%s", fields[5]);
    entries[count].cost = (int)cost;
    entries[count].arg1 = (int)arg1;
    count++;
  }

  fclose(fl);
  return count;
}

static struct char_data *present_questmaster(struct char_data *ch)
{
  struct char_data *mob;

  if (!ch || !VALID_ROOM_RNUM(IN_ROOM(ch)))
    return NULL;

  for (mob = world[IN_ROOM(ch)].people; mob; mob = mob->next_in_room)
    if (is_questmaster_mob(mob))
      return mob;

  return NULL;
}

static int reward_type_supported(const struct quest_reward_entry *r)
{
  return !str_cmp(r->type, "DIAMOND") ||
         !str_cmp(r->type, "TRAIN") ||
         !str_cmp(r->type, "PRACTICE") ||
         !str_cmp(r->type, "GLORY") ||
         !str_cmp(r->type, "OBJECT") ||
         !str_cmp(r->type, "STAT");
}

static int prepare_stat_reward(struct char_data *ch,
                               const struct quest_reward_entry *r,
                               sbyte **field,
                               const char **label)
{
  if (!str_cmp(r->arg2, "STR")) {
    *field = &ch->real_abils.str; *label = "Strength";
  } else if (!str_cmp(r->arg2, "DEX")) {
    *field = &ch->real_abils.dex; *label = "Dexterity";
  } else if (!str_cmp(r->arg2, "CON")) {
    *field = &ch->real_abils.con; *label = "Constitution";
  } else if (!str_cmp(r->arg2, "INT")) {
    *field = &ch->real_abils.intel; *label = "Intelligence";
  } else if (!str_cmp(r->arg2, "WIS")) {
    *field = &ch->real_abils.wis; *label = "Wisdom";
  } else if (!str_cmp(r->arg2, "CHA")) {
    *field = &ch->real_abils.cha; *label = "Charisma";
  } else {
    return FALSE;
  }

  return ((int)**field + r->arg1 <= QUEST_STAT_CAP);
}

static int can_grant_reward(struct char_data *ch,
                            const struct quest_reward_entry *r,
                            struct obj_data **prepared_obj,
                            sbyte **stat_field,
                            const char **stat_label)
{
  *prepared_obj = NULL;
  *stat_field = NULL;
  *stat_label = NULL;

  if (!reward_type_supported(r)) {
    send_to_char(ch, "That reward type is not supported by this build.\r\n");
    return FALSE;
  }

  if (!str_cmp(r->type, "OBJECT")) {
    long count = 1;
    char *end = NULL;

    if (r->arg2[0]) {
      count = strtol(r->arg2, &end, 10);
      if (!end || *trim_ws(end) || count != 1) {
        send_to_char(ch,
          "Object rewards currently support a count of exactly 1 per entry.\r\n");
        return FALSE;
      }
    }

    *prepared_obj = read_object((obj_vnum)r->arg1, VIRTUAL);
    if (!*prepared_obj) {
      send_to_char(ch,
        "That reward refers to an object that does not currently exist.\r\n");
      return FALSE;
    }
  } else if (!str_cmp(r->type, "STAT")) {
    if (!prepare_stat_reward(ch, r, stat_field, stat_label)) {
      send_to_char(ch,
        "That stat reward is invalid or would exceed the stat cap of %d.\r\n",
        QUEST_STAT_CAP);
      return FALSE;
    }
  }

  return TRUE;
}

static void grant_reward(struct char_data *ch,
                         const struct quest_reward_entry *r,
                         struct obj_data *prepared_obj,
                         sbyte *stat_field,
                         const char *stat_label)
{
  if (!str_cmp(r->type, "DIAMOND")) {
    GET_DIAMONDS(ch) += r->arg1;
    send_to_char(ch, "You receive %d diamond%s.\r\n",
                 r->arg1, r->arg1 == 1 ? "" : "s");
  } else if (!str_cmp(r->type, "TRAIN")) {
    GET_TRAINS(ch) += r->arg1;
    send_to_char(ch, "You receive %d training session%s.\r\n",
                 r->arg1, r->arg1 == 1 ? "" : "s");
  } else if (!str_cmp(r->type, "PRACTICE")) {
    GET_PRACTICES(ch) += r->arg1;
    send_to_char(ch, "You receive %d practice session%s.\r\n",
                 r->arg1, r->arg1 == 1 ? "" : "s");
  } else if (!str_cmp(r->type, "GLORY")) {
    GET_GLORY(ch) += r->arg1;
    send_to_char(ch, "You receive %d Glory.\r\n", r->arg1);
  } else if (!str_cmp(r->type, "OBJECT")) {
    obj_to_char(prepared_obj, ch);
    send_to_char(ch, "You receive %s.\r\n", GET_OBJ_SHORT(prepared_obj));
  } else if (!str_cmp(r->type, "STAT")) {
    int old = *stat_field;
    *stat_field += (sbyte)r->arg1;
    affect_total(ch);
    send_to_char(ch, "%s permanently increases from %d to %d.\r\n",
                 stat_label, old, *stat_field);
  }
}

static int reward_selector_matches(const struct quest_reward_entry *r, const char *selector)
{
  if (!r || !selector || !*selector)
    return FALSE;

  if (!str_cmp(selector, r->id) || is_abbrev(selector, r->id))
    return TRUE;

  if (!str_cmp(selector, r->name) || isname(selector, r->name))
    return TRUE;

  return FALSE;
}

static int reward_is_bulk_scalar(const struct quest_reward_entry *r)
{
  return !str_cmp(r->type, "DIAMOND") ||
         !str_cmp(r->type, "TRAIN") ||
         !str_cmp(r->type, "PRACTICE") ||
         !str_cmp(r->type, "GLORY");
}

static void grant_bulk_scalar_reward(struct char_data *ch,
                                     const struct quest_reward_entry *r,
                                     int quantity)
{
  long long amount = (long long)r->arg1 * (long long)quantity;

  if (!str_cmp(r->type, "DIAMOND")) {
    GET_DIAMONDS(ch) += (int)amount;
    send_to_char(ch, "You receive %lld diamond%s.\r\n",
                 amount, amount == 1 ? "" : "s");
  } else if (!str_cmp(r->type, "TRAIN")) {
    GET_TRAINS(ch) += (int)amount;
    send_to_char(ch, "You receive %lld training session%s.\r\n",
                 amount, amount == 1 ? "" : "s");
  } else if (!str_cmp(r->type, "PRACTICE")) {
    GET_PRACTICES(ch) += (int)amount;
    send_to_char(ch, "You receive %lld practice session%s.\r\n",
                 amount, amount == 1 ? "" : "s");
  } else if (!str_cmp(r->type, "GLORY")) {
    GET_GLORY(ch) += (int)amount;
    send_to_char(ch, "You receive %lld Glory.\r\n", amount);
  }
}

void quest_rewards_list(struct char_data *ch)
{
  struct quest_reward_entry rewards[QUEST_REWARD_MAX];
  int count, i, shown = 0;

  if (!present_questmaster(ch)) {
    send_to_char(ch,
      "You must be with a questmaster to view the Guild reward catalog.\r\n");
    return;
  }

  count = load_quest_rewards(rewards, QUEST_REWARD_MAX);
  if (count < 0) {
    send_to_char(ch,
      "The Guild reward catalog is temporarily unavailable. Please notify an immortal.\r\n");
    return;
  }

  send_to_char(ch, "Quest Point Rewards - You currently have %d QP\r\n",
               GET_QUESTPOINTS(ch));
  send_to_char(ch,
      "----------------------------------------------------------------------------\r\n");
  send_to_char(ch,
      " ##   Reward                                                     Cost\r\n");
  send_to_char(ch,
      "----------------------------------------------------------------------------\r\n");

  for (i = 0; i < count; i++) {
    if (!reward_type_supported(&rewards[i]))
      continue;

    shown++;
    send_to_char(ch, " %2d)  %-56.56s %6d QP\r\n",
                 shown, rewards[i].name, rewards[i].cost);
  }

  if (!shown) {
    send_to_char(ch, "No Quest Point rewards are currently configured.\r\n");
  } else {
    send_to_char(ch,
      "\r\nBuy by number or name:\r\n"
      "  quest buy <reward>\r\n"
      "  quest buy <quantity> <reward>\r\n"
      "  quest buy <quantity>*<reward>\r\n"
      "Shortcut: qb, for example 'qb 100 diamond'.\r\n");
  }
}

void quest_rewards_buy(struct char_data *ch, char *argument)
{
  struct quest_reward_entry rewards[QUEST_REWARD_MAX];
  struct quest_reward_entry *selected = NULL;
  struct obj_data *prepared_obj = NULL;
  sbyte *stat_field = NULL;
  const char *stat_label = NULL;
  char input[MAX_INPUT_LENGTH];
  char first[MAX_INPUT_LENGTH];
  char selector[MAX_INPUT_LENGTH];
  char *rest, *star, *end = NULL;
  long quantity = 1, numeric_choice = 0;
  long long total_cost, total_amount;
  int count, i, shown = 0, matches = 0;

  if (!present_questmaster(ch)) {
    send_to_char(ch,
      "You must be with a questmaster to purchase Guild rewards.\r\n");
    return;
  }

  snprintf(input, sizeof(input), "%s", argument ? argument : "");
  rest = trim_ws(input);

  if (!*rest) {
    quest_rewards_list(ch);
    return;
  }

  selector[0] = '\0';

  /* Accept compact syntax such as 100*diamond. */
  star = strchr(rest, '*');
  if (star) {
    *star = '\0';
    errno = 0;
    quantity = strtol(trim_ws(rest), &end, 10);
    if (errno || !end || *trim_ws(end) || quantity <= 0 || quantity > INT_MAX) {
      send_to_char(ch, "Quantity must be a positive whole number.\r\n");
      return;
    }
    snprintf(selector, sizeof(selector), "%s", trim_ws(star + 1));
    if (!*selector) {
      send_to_char(ch, "Specify the reward you want to buy.\r\n");
      return;
    }
  } else {
    /* Accept spaced syntax such as 100 diamond. A lone number remains the
     * catalog entry number for backward compatibility. */
    rest = any_one_arg(rest, first);
    rest = trim_ws(rest);

    errno = 0;
    numeric_choice = strtol(first, &end, 10);

    if (!errno && end && !*end && numeric_choice > 0 && *rest) {
      quantity = numeric_choice;
      if (quantity > INT_MAX) {
        send_to_char(ch, "Quantity is too large.\r\n");
        return;
      }
      snprintf(selector, sizeof(selector), "%s", rest);
      numeric_choice = 0;
    } else {
      snprintf(selector, sizeof(selector), "%s", first);
      if (*rest) {
        size_t used = strlen(selector);
        snprintf(selector + used, sizeof(selector) - used, " %s", rest);
      }
      errno = 0;
      numeric_choice = strtol(selector, &end, 10);
      if (errno || !end || *trim_ws(end) || numeric_choice <= 0)
        numeric_choice = 0;
    }
  }

  count = load_quest_rewards(rewards, QUEST_REWARD_MAX);
  if (count < 0) {
    send_to_char(ch,
      "The Guild reward catalog is temporarily unavailable. Please notify an immortal.\r\n");
    return;
  }

  if (numeric_choice > 0) {
    for (i = 0; i < count; i++) {
      if (!reward_type_supported(&rewards[i]))
        continue;
      shown++;
      if (shown == numeric_choice) {
        selected = &rewards[i];
        break;
      }
    }

    if (!selected) {
      send_to_char(ch, "There is no reward numbered %ld. Use 'quest list'.\r\n",
                   numeric_choice);
      return;
    }
  } else {
    for (i = 0; i < count; i++) {
      if (!reward_type_supported(&rewards[i]))
        continue;
      if (!reward_selector_matches(&rewards[i], selector))
        continue;
      selected = &rewards[i];
      matches++;
    }

    if (matches == 0) {
      send_to_char(ch,
        "No Guild reward matches '%s'. Use 'quest list' to view the catalog.\r\n",
        selector);
      return;
    }

    if (matches > 1) {
      send_to_char(ch,
        "More than one Guild reward matches '%s'. Please be more specific:\r\n",
        selector);
      shown = 0;
      for (i = 0; i < count; i++) {
        if (!reward_type_supported(&rewards[i]))
          continue;
        shown++;
        if (reward_selector_matches(&rewards[i], selector))
          send_to_char(ch, "  %2d) %s\r\n", shown, rewards[i].name);
      }
      return;
    }
  }

  if (quantity > 1 && !reward_is_bulk_scalar(selected)) {
    send_to_char(ch,
      "That reward must currently be purchased one at a time.\r\n");
    return;
  }

  total_cost = (long long)selected->cost * (long long)quantity;
  if (total_cost <= 0 || total_cost > INT_MAX) {
    send_to_char(ch, "That bulk purchase is too large.\r\n");
    return;
  }

  if ((long long)GET_QUESTPOINTS(ch) < total_cost) {
    send_to_char(ch,
      "%ld purchase%s of %s cost%s %lld QP, but you only have %d QP.\r\n",
      quantity, quantity == 1 ? "" : "s", selected->name,
      quantity == 1 ? "s" : "", total_cost, GET_QUESTPOINTS(ch));
    return;
  }

  if (reward_is_bulk_scalar(selected)) {
    total_amount = (long long)selected->arg1 * (long long)quantity;
    if (total_amount <= 0 || total_amount > INT_MAX) {
      send_to_char(ch, "That bulk reward amount is too large.\r\n");
      return;
    }

    if ((!str_cmp(selected->type, "DIAMOND") &&
         GET_DIAMONDS(ch) > INT_MAX - (int)total_amount) ||
        (!str_cmp(selected->type, "TRAIN") &&
         GET_TRAINS(ch) > INT_MAX - (int)total_amount) ||
        (!str_cmp(selected->type, "PRACTICE") &&
         GET_PRACTICES(ch) > INT_MAX - (int)total_amount) ||
        (!str_cmp(selected->type, "GLORY") &&
         GET_GLORY(ch) > INT_MAX - (int)total_amount)) {
      send_to_char(ch, "You cannot hold that much of this reward.\r\n");
      return;
    }
  } else {
    if (!can_grant_reward(ch, selected, &prepared_obj, &stat_field, &stat_label)) {
      if (prepared_obj)
        extract_obj(prepared_obj);
      return;
    }
  }

  GET_QUESTPOINTS(ch) -= (int)total_cost;

  if (reward_is_bulk_scalar(selected))
    grant_bulk_scalar_reward(ch, selected, (int)quantity);
  else
    grant_reward(ch, selected, prepared_obj, stat_field, stat_label);

  send_to_char(ch,
    "You spend %lld Quest Point%s. You have %d QP remaining.\r\n",
    total_cost, total_cost == 1 ? "" : "s", GET_QUESTPOINTS(ch));
  save_char(ch);
}
