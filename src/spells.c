/**************************************************************************
*  File: spells.c                                          Part of tbaMUD *
*  Usage: Implementation of "manual spells."                              *
*                                                                         *
*  All rights reserved.  See license for complete information.            *
*                                                                         *
*  Copyright (C) 1993, 94 by the Trustees of the Johns Hopkins University *
*  CircleMUD is based on DikuMUD, Copyright (C) 1990, 1991.               *
**************************************************************************/

#include "conf.h"
#include "sysdep.h"
#include "structs.h"
#include "utils.h"
#include "comm.h"
#include "spells.h"
#include "handler.h"
#include "db.h"
#include "constants.h"
#include "interpreter.h"
#include "dg_scripts.h"
#include "act.h"
#include "fight.h"
#include "criticalhits.h"
#include "mud_event.h"
#include "screen.h"
#include "graph.h"

static int clampi(int v, int lo, int hi)
{
  if (v < lo)
    return lo;
  if (v > hi)
    return hi;
  return v;
}

static int warlock_power(struct char_data *ch)
{
  return GET_INT(ch) + GET_WIS(ch);
}

static int remove_flagged_affects(struct char_data *victim, int aff_flag)
{
  struct affected_type *af, *next;
  int removed = 0;

  if (!victim)
    return 0;

  for (af = victim->affected; af; af = next) {
    next = af->next;
    if (IS_SET_AR(af->bitvector, aff_flag)) {
      affect_remove(victim, af);
      removed = 1;
    }
  }

  if (AFF_FLAGGED(victim, aff_flag)) {
    REMOVE_BIT_AR(AFF_FLAGS(victim), aff_flag);
    removed = 1;
  }

  return removed;
}

static int remove_spell_affect_if_present(struct char_data *victim, int spellnum)
{
  if (!victim || spellnum <= 0)
    return 0;
  if (!affected_by_spell(victim, spellnum))
    return 0;
  affect_from_char(victim, spellnum);
  return 1;
}

static int cancellation_remove_one(struct char_data *victim)
{
  if (remove_spell_affect_if_present(victim, SPELL_SANCTUARY)) return 1;
  if (remove_flagged_affects(victim, AFF_FLYING)) return 1;
  if (remove_flagged_affects(victim, AFF_DETECT_INVIS)) return 1;
  if (remove_flagged_affects(victim, AFF_SENSE_LIFE)) return 1;
  if (remove_flagged_affects(victim, AFF_INVISIBLE)) return 1;
  if (remove_flagged_affects(victim, AFF_TRUESIGHT)) return 1;
  if (remove_spell_affect_if_present(victim, SPELL_BARKSKIN)) return 1;
  if (remove_spell_affect_if_present(victim, SPELL_STONE_SKIN)) return 1;
  if (remove_flagged_affects(victim, AFF_WARDED)) return 1;
  if (remove_flagged_affects(victim, AFF_SHIELDED)) return 1;
  return 0;
}

static int identify_is_warning_flag(const char *flag)
{
  if (!flag || !*flag)
    return FALSE;

  return !str_cmp(flag, "nodrop")
      || !str_cmp(flag, "cursed")
      || !str_cmp(flag, "no_sell")
      || !str_cmp(flag, "nosell")
      || !str_cmp(flag, "no-rent")
      || !str_cmp(flag, "norent")
      || !str_cmp(flag, "anti-good")
      || !str_cmp(flag, "anti-evil")
      || !str_cmp(flag, "anti-neutral");
}

static void identify_send_border(struct char_data *ch, const char *border, const char *reset)
{
  send_to_char(ch, "%s+------------------------------------------------------------------------------+%s\r\n", border, reset);
}

static void identify_send_section_header(struct char_data *ch, const char *border, const char *label, const char *reset, const char *title)
{
  send_to_char(ch, "%s|%s %s[%s]%s\r\n", border, reset, label, title, reset);
}

void show_identify_item(struct char_data *ch, struct obj_data *obj, enum identify_detail_level detail)
{
  int i, found;
  size_t len, tok_len;
  char bitbuf[MAX_STRING_LENGTH], typebuf[256], wearbuf[MAX_STRING_LENGTH];
  char flags_buf[MAX_STRING_LENGTH], flag_word[128], aff_summary[256];
  char line[MAX_STRING_LENGTH];
  const char *B = CCBLU(ch, C_NRM);
  const char *L = CCCYN(ch, C_NRM);
  const char *R = CCNRM(ch, C_NRM);
  const char *V = CCWHT(ch, C_NRM);
  const char *G = CCGRN(ch, C_NRM);
  const char *RED = CCRED(ch, C_NRM);
  const char *Y = CCYEL(ch, C_NRM);
  const char *M = CCMAG(ch, C_NRM);
  int affect_count = 0;
  int flag_columns = 0;

  if (!ch || !obj)
    return;

  sprinttype(GET_OBJ_TYPE(obj), item_types, typebuf, sizeof(typebuf));
  sprintbitarray(GET_OBJ_WEAR(obj), wear_bits, TW_ARRAY_MAX, wearbuf);
  sprintbitarray(GET_OBJ_EXTRA(obj), extra_bits, EF_ARRAY_MAX, flags_buf);

  identify_send_border(ch, B, R);
  send_to_char(ch, "%s|%s %sIdentify Appraisal%s%-57.57s%s |\r\n",
               B, R, L, R, "", B);
  identify_send_border(ch, B, R);

  identify_send_section_header(ch, B, L, R, "Header / Identity");
  send_to_char(ch, "%s|%s %sName:%s %s%s\r\n", B, R, L, R, obj->short_description ? obj->short_description : "<None>", R);
  send_to_char(ch, "%s|%s %sKeywords:%s %s%s\r\n", B, R, L, R, obj->name ? obj->name : "<None>", R);
  send_to_char(ch, "%s|%s %sId:%s %s%ld%s  %sType:%s %s%s%s  %sLevel:%s %s%d%s\r\n",
               B, R, L, R, V, obj_script_id(obj), R, L, R, V, typebuf, R, L, R, V, GET_OBJ_LEVEL(obj), R);

  if (GET_OBJ_AFFECT(obj)) {
    sprintbitarray(GET_OBJ_AFFECT(obj), affected_bits, AF_ARRAY_MAX, bitbuf);
    send_to_char(ch, "%s|%s %sAbilities:%s %s%s%s\r\n", B, R, L, R, V, bitbuf, R);
  }

  identify_send_border(ch, B, R);
  identify_send_section_header(ch, B, L, R, "Core Properties");
  send_to_char(ch, "%s|%s %sWorth:%s %s%d%s  %sWeight:%s %s%d%s  %sRent:%s %s%d%s\r\n",
               B, R, L, R, V, GET_OBJ_COST(obj), R, L, R, V, GET_OBJ_WEIGHT(obj), R, L, R, V, GET_OBJ_RENT(obj), R);
  send_to_char(ch, "%s|%s %sWearable:%s %s%s%s\r\n", B, R, L, R, V, wearbuf, R);

  send_to_char(ch, "%s|%s %sFlags:%s ", B, R, L, R);
  found = FALSE;
  tok_len = 0;
  while (*(flags_buf + tok_len) && *(flags_buf + tok_len) == ' ')
    tok_len++;
  flags_buf[MAX_STRING_LENGTH - 1] = '\0';
  for (len = tok_len; len <= strlen(flags_buf); len++) {
    if (flags_buf[len] != ' ' && flags_buf[len] != '\0')
      continue;
    if (len == tok_len) {
      tok_len = len + 1;
      continue;
    }
    snprintf(flag_word, sizeof(flag_word), "%.*s", (int)(len - tok_len), flags_buf + tok_len);
    send_to_char(ch, "%s%s%s%s",
                 identify_is_warning_flag(flag_word) ? Y : V,
                 flag_word,
                 R,
                 flags_buf[len] == '\0' ? "" : " ");
    found = TRUE;
    flag_columns++;
    if (flag_columns >= 5 && flags_buf[len] != '\0') {
      send_to_char(ch, "\r\n%s|%s %-8.8s ", B, R, "");
      flag_columns = 0;
    }
    tok_len = len + 1;
    while (*(flags_buf + tok_len) && *(flags_buf + tok_len) == ' ')
      tok_len++;
  }
  if (!found)
    send_to_char(ch, "%snone%s", V, R);
  send_to_char(ch, "\r\n");

  identify_send_border(ch, B, R);

  switch (GET_OBJ_TYPE(obj)) {
  case ITEM_SCROLL:
  case ITEM_POTION:
    len = i = 0;

    if (GET_OBJ_VAL(obj, 1) >= 1) {
      i = snprintf(bitbuf + len, sizeof(bitbuf) - len, " %s", skill_name(GET_OBJ_VAL(obj, 1)));
      if (i >= 0)
        len += i;
    }

    if (GET_OBJ_VAL(obj, 2) >= 1 && len < sizeof(bitbuf)) {
      i = snprintf(bitbuf + len, sizeof(bitbuf) - len, " %s", skill_name(GET_OBJ_VAL(obj, 2)));
      if (i >= 0)
        len += i;
    }

    if (GET_OBJ_VAL(obj, 3) >= 1 && len < sizeof(bitbuf))
      snprintf(bitbuf + len, sizeof(bitbuf) - len, " %s", skill_name(GET_OBJ_VAL(obj, 3)));

    send_to_char(ch, "%s|%s %sSpell Data:%s This %s casts:%s %s%s\r\n",
                 B, R, L, R, item_types[(int) GET_OBJ_TYPE(obj)], R, V, bitbuf);
    break;
  case ITEM_WAND:
  case ITEM_STAFF:
    send_to_char(ch, "%s|%s %sSpell Data:%s This %s casts:%s %s%s%s\r\n",
                 B, R, L, R, item_types[(int) GET_OBJ_TYPE(obj)], R, V, skill_name(GET_OBJ_VAL(obj, 3)), R);
    send_to_char(ch, "%s|%s %sCharges:%s %s%d%s maximum, %s%d%s remaining\r\n",
                 B, R, L, R, V, GET_OBJ_VAL(obj, 1), R, V, GET_OBJ_VAL(obj, 2), R);
    break;
  case ITEM_WEAPON:
    send_to_char(ch, "%s|%s %sWeapon Data:%s Damage dice %s%dD%d%s, avg/round %s%.1f%s\r\n",
                 B, R, L, R, V, GET_OBJ_VAL(obj, 1), GET_OBJ_VAL(obj, 2), R, V,
                 ((GET_OBJ_VAL(obj, 2) + 1) / 2.0) * GET_OBJ_VAL(obj, 1), R);
    break;
  case ITEM_ARMOR:
    send_to_char(ch, "%s|%s %sArmor Data:%s AC apply %s%d%s\r\n",
                 B, R, L, R, V, GET_OBJ_VAL(obj, 0), R);
    break;
  default:
    break;
  }

  identify_send_border(ch, B, R);
  identify_send_section_header(ch, B, L, R, "Notes / Summary");
  for (i = 0; i < MAX_OBJ_AFFECT; i++) {
    if (obj->affected[i].location != APPLY_NONE && obj->affected[i].modifier != 0) {
      affect_count++;
    }
  }

  snprintf(aff_summary, sizeof(aff_summary), "Item has %d modifier affect%s.", affect_count, affect_count == 1 ? "" : "s");
  send_to_char(ch, "%s|%s %sNotes:%s %s%s%s\r\n", B, R, L, R, M, aff_summary, R);
  if (detail == IDENTIFY_FULL) {
    identify_send_border(ch, B, R);
    identify_send_section_header(ch, B, L, R, "Detailed Modifiers");

    found = FALSE;
    for (i = 0; i < MAX_OBJ_AFFECT; i++) {
      if ((obj->affected[i].location != APPLY_NONE) &&
          (obj->affected[i].modifier != 0)) {
        sprinttype(obj->affected[i].location, apply_types, bitbuf, sizeof(bitbuf));
        snprintf(line, sizeof(line), "%-22.22s : %s%+d%s", bitbuf,
                 obj->affected[i].modifier >= 0 ? G : RED, obj->affected[i].modifier, R);
        send_to_char(ch, "%s|%s %s%s%s\r\n", B, R, V, line, R);
        found = TRUE;
      }
    }
    if (!found)
      send_to_char(ch, "%s|%s %sNo detailed modifiers on this item.%s\r\n", B, R, V, R);
  }

  identify_send_border(ch, B, R);
}

/* Handle followers when an owner teleports or recalls. */
void handle_followers_after_owner_teleport_or_recall(struct char_data *ch)
{
  struct follow_type *f, *next;

  if (!ch)
    return;

  for (f = ch->followers; f; f = next) {
    struct char_data *follower = f->follower;

    next = f->next;

    if (!follower || follower->master != ch)
      continue;

    if (is_purchased_pet(ch, follower)) {
      room_rnum to_room = IN_ROOM(ch);

      if (IN_ROOM(follower) == to_room)
        continue;

      act("$n disappears.", TRUE, follower, 0, 0, TO_ROOM);
      char_from_room(follower);
      char_to_room(follower, to_room);
      act("$n arrives at $N's side.", TRUE, follower, 0, ch, TO_ROOM);
      look_at_room(follower, 0);
    } else if (!IS_NPC(follower) && AFF_FLAGGED(follower, AFF_CHARM)) {
      break_charm_follower(ch, follower);
    }
  }
}

static int corruption_duration(int level)
{
  if (level >= 100)
    return 8;
  if (level >= 80)
    return 8;
  if (level >= 60)
    return 7;
  if (level >= 40)
    return 6;
  if (level >= 20)
    return 5;
  return 4;
}

static int corruption_damage_per_tick(int level)
{
  int damage = 1 + (level / 10);

  return MIN(damage, 12);
}

static int active_temp_summons(struct char_data *ch)
{
  struct follow_type *f;
  int count = 0;

  if (!ch)
    return 0;
  for (f = ch->followers; f; f = f->next) {
    struct char_data *mob = f->follower;
    if (!mob || mob->master != ch || !IS_NPC(mob))
      continue;
    if (AFF_FLAGGED(mob, AFF_CHARM) && GET_SUMMON_TIMER(mob) > 0)
      count++;
  }
  return count;
}

static struct char_data *summon_temp_follower(struct char_data *ch, mob_vnum vnum, int level, int rounds)
{
  struct char_data *mob = read_mobile(vnum, VIRTUAL);
  if (!mob)
    return NULL;

  GET_LEVEL(mob) = MAX(1, level);
  SET_BIT_AR(AFF_FLAGS(mob), AFF_CHARM);
  char_to_room(mob, IN_ROOM(ch));
  add_follower(mob, ch);
  if (GROUP(ch) && GROUP_LEADER(GROUP(ch)) == ch)
    join_group(mob, GROUP(ch));
  GET_SUMMON_TIMER(mob) = MAX(1, rounds);
  load_mtrigger(mob);
  return mob;
}

static int bfs_within_range(room_rnum src, room_rnum target, int max_depth, int *first_dir_out)
{
  room_rnum *queue;
  int *depth, *first_dir, head = 0, tail = 0, r;

  if (src == NOWHERE || target == NOWHERE || max_depth < 0)
    return FALSE;
  if (src == target) {
    if (first_dir_out)
      *first_dir_out = BFS_ALREADY_THERE;
    return TRUE;
  }

  CREATE(queue, room_rnum, top_of_world + 1);
  CREATE(depth, int, top_of_world + 1);
  CREATE(first_dir, int, top_of_world + 1);
  for (r = 0; r <= top_of_world; r++)
    first_dir[r] = BFS_NO_PATH;

  queue[tail] = src;
  depth[tail] = 0;
  first_dir[src] = BFS_ALREADY_THERE;
  tail++;

  while (head < tail) {
    room_rnum room = queue[head];
    int d = depth[head];
    int dir;
    head++;
    if (d >= max_depth)
      continue;

    for (dir = 0; dir < DIR_COUNT; dir++) {
      struct room_direction_data *exit = world[room].dir_option[dir];
      room_rnum nr;
      if (!exit || exit->to_room == NOWHERE || IS_SET(exit->exit_info, EX_CLOSED))
        continue;
      nr = exit->to_room;
      if (first_dir[nr] != BFS_NO_PATH)
        continue;
      first_dir[nr] = (first_dir[room] == BFS_ALREADY_THERE) ? dir : first_dir[room];
      if (nr == target) {
        if (first_dir_out)
          *first_dir_out = first_dir[nr];
        free(queue);
        free(depth);
        free(first_dir);
        return TRUE;
      }
      queue[tail] = nr;
      depth[tail] = d + 1;
      tail++;
    }
  }

  free(queue);
  free(depth);
  free(first_dir);
  return FALSE;
}

/* Special spells appear below. */
ASPELL(spell_create_water)
{
  int water;

  if (ch == NULL || obj == NULL)
    return;
  /* level = MAX(MIN(level, LVL_IMPL), 1);	 - not used */

  if (GET_OBJ_TYPE(obj) == ITEM_DRINKCON) {
    if ((GET_OBJ_VAL(obj, 2) != LIQ_WATER) && (GET_OBJ_VAL(obj, 1) != 0)) {
      name_from_drinkcon(obj);
      GET_OBJ_VAL(obj, 2) = LIQ_SLIME;
      name_to_drinkcon(obj, LIQ_SLIME);
    } else {
      water = MAX(GET_OBJ_VAL(obj, 0) - GET_OBJ_VAL(obj, 1), 0);
      if (water > 0) {
	if (GET_OBJ_VAL(obj, 1) >= 0)
	  name_from_drinkcon(obj);
	GET_OBJ_VAL(obj, 2) = LIQ_WATER;
	GET_OBJ_VAL(obj, 1) += water;
	name_to_drinkcon(obj, LIQ_WATER);
	weight_change_object(obj, water);
	act("$p is filled.", FALSE, ch, obj, 0, TO_CHAR);
      }
    }
  }
}

ASPELL(spell_recall)
{
  if (victim == NULL || IS_NPC(victim))
    return;

  if (ZONE_FLAGGED(GET_ROOM_ZONE(IN_ROOM(victim)), ZONE_NOASTRAL)) {
    send_to_char(ch, "A bright flash prevents your spell from working!");
    return;
  }

  act("$n disappears.", TRUE, victim, 0, 0, TO_ROOM);
  char_from_room(victim);
  char_to_room(victim, r_mortal_start_room);
  act("$n appears in the middle of the room.", TRUE, victim, 0, 0, TO_ROOM);
  look_at_room(victim, 0);
  entry_memory_mtrigger(victim);
  greet_mtrigger(victim, -1);
  greet_memory_mtrigger(victim);
  handle_followers_after_owner_teleport_or_recall(victim);
}

ASPELL(spell_teleport)
{
  room_rnum to_room;

  if (victim == NULL || IS_NPC(victim))
    return;

  if (ZONE_FLAGGED(GET_ROOM_ZONE(IN_ROOM(victim)), ZONE_NOASTRAL)) {
    send_to_char(ch, "A bright flash prevents your spell from working!");
    return;
  }

  do {
    to_room = rand_number(0, top_of_world);
  } while (ROOM_FLAGGED(to_room, ROOM_PRIVATE) || ROOM_FLAGGED(to_room, ROOM_DEATH) ||
           ROOM_FLAGGED(to_room, ROOM_GODROOM) || ZONE_FLAGGED(GET_ROOM_ZONE(to_room), ZONE_CLOSED) ||
           ZONE_FLAGGED(GET_ROOM_ZONE(to_room), ZONE_NOASTRAL));

  act("$n slowly fades out of existence and is gone.",
      FALSE, victim, 0, 0, TO_ROOM);
  char_from_room(victim);
  char_to_room(victim, to_room);
  act("$n slowly fades into existence.", FALSE, victim, 0, 0, TO_ROOM);
  look_at_room(victim, 0);
  entry_memory_mtrigger(victim);
  greet_mtrigger(victim, -1);
  greet_memory_mtrigger(victim);
  handle_followers_after_owner_teleport_or_recall(victim);
}

#define SUMMON_FAIL "You failed.\r\n"
ASPELL(spell_summon)
{
  if (ch == NULL || victim == NULL)
    return;

  if (GET_LEVEL(victim) > MIN(LVL_IMMORT - 1, level + 3)) {
    send_to_char(ch, "%s", SUMMON_FAIL);
    return;
  }

  if (ZONE_FLAGGED(GET_ROOM_ZONE(IN_ROOM(victim)), ZONE_NOASTRAL) ||
      ZONE_FLAGGED(GET_ROOM_ZONE(IN_ROOM(ch)), ZONE_NOASTRAL)) {
    send_to_char(ch, "A bright flash prevents your spell from working!");
    return;
  }

  if (!CONFIG_PK_ALLOWED) {
    if (MOB_FLAGGED(victim, MOB_AGGRESSIVE)) {
      act("As the words escape your lips and $N travels\r\n"
	  "through time and space towards you, you realize that $E is\r\n"
	  "aggressive and might harm you, so you wisely send $M back.",
	  FALSE, ch, 0, victim, TO_CHAR);
      return;
    }
    if (!IS_NPC(victim) && !PRF_FLAGGED(victim, PRF_SUMMONABLE) &&
	!PLR_FLAGGED(victim, PLR_KILLER)) {
      send_to_char(victim, "%s just tried to summon you to: %s.\r\n"
	      "This failed because you have summon protection on.\r\n"
	      "Type NOSUMMON to allow other players to summon you.\r\n",
	      GET_NAME(ch), world[IN_ROOM(ch)].name);

      send_to_char(ch, "You failed because %s has summon protection on.\r\n", GET_NAME(victim));
      mudlog(BRF, MAX(LVL_IMMORT, MAX(GET_INVIS_LEV(ch), GET_INVIS_LEV(victim))), TRUE, 
        "%s failed summoning %s to %s.", GET_NAME(ch), GET_NAME(victim), world[IN_ROOM(ch)].name);
      return;
    }
  }

  if (MOB_FLAGGED(victim, MOB_NOSUMMON) ||
      (IS_NPC(victim) && mag_savingthrow(victim, SAVING_SPELL, 0))) {
    send_to_char(ch, "%s", SUMMON_FAIL);
    return;
  }

  act("$n disappears suddenly.", TRUE, victim, 0, 0, TO_ROOM);

  char_from_room(victim);
  char_to_room(victim, IN_ROOM(ch));

  act("$n arrives suddenly.", TRUE, victim, 0, 0, TO_ROOM);
  act("$n has summoned you!", FALSE, ch, 0, victim, TO_VICT);
  look_at_room(victim, 0);
  entry_memory_mtrigger(victim);
  greet_mtrigger(victim, -1);
  greet_memory_mtrigger(victim);
}

/* Used by the locate object spell to check the alias list on objects */
static int isname_obj(char *search, char *list)
{
  char *found_in_list; /* But could be something like 'ring' in 'shimmering.' */
  char searchname[128];
  char namelist[MAX_STRING_LENGTH];
  int found_pos = -1;
  int found_name=0; /* found the name we're looking for */
  int match = 1;
  int i;

  /* Force to lowercase for string comparisons */
  sprintf(searchname, "%s", search);
  for (i = 0; searchname[i]; i++)
    searchname[i] = LOWER(searchname[i]);

  sprintf(namelist, "%s", list);
  for (i = 0; namelist[i]; i++)
    namelist[i] = LOWER(namelist[i]);

  /* see if searchname exists any place within namelist */
  found_in_list = strstr(namelist, searchname);
  if (!found_in_list) {
    return 0;
  }

  /* Found the name in the list, now see if it's a valid hit. The following
   * avoids substrings (like ring in shimmering) is it at beginning of
   * namelist? */
  for (i = 0; searchname[i]; i++)
    if (searchname[i] != namelist[i])
      match = 0;

  if (match) /* It was found at the start of the namelist string. */
    found_name = 1;
  else { /* It is embedded inside namelist. Is it preceded by a space? */
    found_pos = found_in_list - namelist;
    if (namelist[found_pos-1] == ' ')
      found_name = 1;
  }

  if (found_name)
    return 1;
  else
    return 0;
}

ASPELL(spell_locate_object)
{
  struct obj_data *i;
  char name[MAX_INPUT_LENGTH];
  int j;

  if (!obj) {
    send_to_char(ch, "You sense nothing.\r\n");
    return;
  }

  /*  added a global var to catch 2nd arg. */
  sprintf(name, "%s", cast_arg2);

  j = GET_LEVEL(ch) / 2;  /* # items to show = twice char's level */

  for (i = object_list; i && (j > 0); i = i->next) {
    if (!isname_obj(name, i->name))
      continue;

  send_to_char(ch, "%c%s", UPPER(*i->short_description), i->short_description + 1);

    if (i->carried_by)
      send_to_char(ch, " is being carried by %s.\r\n", PERS(i->carried_by, ch));
    else if (IN_ROOM(i) != NOWHERE)
      send_to_char(ch, " is in %s.\r\n", world[IN_ROOM(i)].name);
    else if (i->in_obj)
      send_to_char(ch, " is in %s.\r\n", i->in_obj->short_description);
    else if (i->worn_by)
      send_to_char(ch, " is being worn by %s.\r\n", PERS(i->worn_by, ch));
    else
      send_to_char(ch, "'s location is uncertain.\r\n");

    j--;
  }
}

ASPELL(spell_charm)
{
  struct affected_type af;

  if (victim == NULL || ch == NULL)
    return;

  if (victim == ch)
    send_to_char(ch, "You like yourself even better!\r\n");
  else if (!IS_NPC(victim) && !PRF_FLAGGED(victim, PRF_SUMMONABLE))
    send_to_char(ch, "You fail because SUMMON protection is on!\r\n");
  else if (AFF_FLAGGED(victim, AFF_SANCTUARY))
    send_to_char(ch, "Your victim is protected by sanctuary!\r\n");
  else if (MOB_FLAGGED(victim, MOB_NOCHARM))
    send_to_char(ch, "Your victim resists!\r\n");
  else if (AFF_FLAGGED(ch, AFF_CHARM))
    send_to_char(ch, "You can't have any followers of your own!\r\n");
  else if (AFF_FLAGGED(victim, AFF_CHARM) || level < GET_LEVEL(victim))
    send_to_char(ch, "You fail.\r\n");
  /* player charming another player - no legal reason for this */
  else if (!CONFIG_PK_ALLOWED && !IS_NPC(victim))
    send_to_char(ch, "You fail - shouldn't be doing it anyway.\r\n");
  else if (circle_follow(victim, ch))
    send_to_char(ch, "Sorry, following in circles is not allowed.\r\n");
  else if (mag_savingthrow(victim, SAVING_PARA, 0))
    send_to_char(ch, "Your victim resists!\r\n");
  else {
    if (victim->master)
      stop_follower(victim);

    add_follower(victim, ch);

    new_affect(&af);
    af.spell = SPELL_CHARM;
    af.duration = 24 * 2;
    if (GET_CHA(ch))
      af.duration *= GET_CHA(ch);
    if (GET_INT(victim))
      af.duration /= GET_INT(victim);
    SET_BIT_AR(af.bitvector, AFF_CHARM);
    affect_to_char(victim, &af);

    act("Isn't $n just such a nice fellow?", FALSE, ch, 0, victim, TO_VICT);
    if (IS_NPC(victim))
      REMOVE_BIT_AR(MOB_FLAGS(victim), MOB_SPEC);
  }
}

ASPELL(spell_corruption)
{
  struct affected_type af;
  int caster_level = GET_LEVEL(ch);

  if (victim == NULL || ch == NULL)
    return;

  new_affect(&af);
  af.spell = SPELL_CORRUPTION;
  af.duration = corruption_duration(caster_level);
  af.modifier = corruption_damage_per_tick(caster_level);
  af.location = APPLY_NONE;

  affect_join(victim, &af, FALSE, FALSE, FALSE, FALSE);

  act("You envelop $N in a wave of corrupting energy.", FALSE, ch, 0, victim, TO_CHAR);
  act("$n envelopes $N in a wave of corrupting energy.", TRUE, ch, 0, victim, TO_ROOM);

  if (ch != victim)
  /* 0 damage prints like a miss. Make the initial hit at least 1. */
  damage(ch, victim, (af.modifier < 1 ? 1 : af.modifier), SPELL_CORRUPTION);

}

ASPELL(spell_plague_bolt)
{
  struct affected_type af;
  int power;
  int dam;
  int mult = 200;
  int pen;
  int dur_ticks;
  int stat_bonus;
  int lvl_bonus;

  if (victim == NULL || ch == NULL)
    return;

  act("You fling a \tGplague bolt\tn at $N!\tn", FALSE, ch, 0, victim, TO_CHAR);
  act("$n flings a \tGplague bolt\tn at you!\tn", FALSE, ch, 0, victim, TO_VICT);
  act("$n flings a \tGplague bolt\tn at $N!\tn", TRUE, ch, 0, victim, TO_ROOM);

  power = warlock_power(ch);
  stat_bonus = MIN(6, power / 10);
  lvl_bonus = level / 6;
  dam = dice(3, 6) + 3 + stat_bonus + lvl_bonus;
  if (mag_savingthrow(victim, SAVING_SPELL, 0))
    dam = (dam * 75) / 100;

  if (crit_check_spell(ch, &mult)) {
    dam = (dam * mult) / 100;
    crit_show_banner(ch, victim, mult);
  }

  if (dam < 1)
    dam = 1;

  if (damage(ch, victim, dam, SPELL_PLAGUE_BOLT) == -1)
    return;

  pen = clampi(1 + MAX(0, power - 20) / 10, 1, 4);
  dur_ticks = clampi(2 + MAX(0, power - 20) / 12, 2, 6);

  new_affect(&af);
  af.spell = SPELL_PLAGUE_BOLT;
  af.duration = dur_ticks;
  af.modifier = -pen;
  af.location = APPLY_STR;
  SET_BIT_AR(af.bitvector, AFF_POISON);
  affect_join(victim, &af, FALSE, FALSE, FALSE, FALSE);

  act("\tGSickness\tn spreads through $N's body!\tn", FALSE, ch, 0, victim, TO_CHAR);
  act("\tGSickness\tn spreads through your body!\tn", FALSE, ch, 0, victim, TO_VICT);
  act("\tGSickness\tn spreads through $N's body!\tn", TRUE, ch, 0, victim, TO_ROOM);
}

ASPELL(spell_enfeeblement)
{
  struct affected_type af;
  struct mud_event_data *event;
  int power;
  int stat_pen;
  int dur_sec;

  if (victim == NULL || ch == NULL)
    return;

  act("You whisper a \tDcruel hex\tn and sap $N's strength.\tn", FALSE, ch, 0, victim, TO_CHAR);
  act("$n whispers a \tDcruel hex\tn and your limbs go weak.\tn", FALSE, ch, 0, victim, TO_VICT);
  act("$n whispers a \tDcruel hex\tn and $N's limbs go weak.\tn", TRUE, ch, 0, victim, TO_ROOM);

  if (mag_savingthrow(victim, SAVING_SPELL, 0)) {
    act("$N shakes off your \tDhex\tn.\tn", FALSE, ch, 0, victim, TO_CHAR);
    act("You shake off $n's \tDhex\tn.\tn", FALSE, ch, 0, victim, TO_VICT);
    act("$N shakes off $n's \tDhex\tn.\tn", TRUE, ch, 0, victim, TO_ROOM);
    return;
  }

  power = warlock_power(ch);
  stat_pen = clampi(1 + MAX(0, power - 20) / 8, 1, 6);
  dur_sec = 30 + MIN(60, MAX(0, power - 20) * 2);
  dur_sec = clampi(dur_sec, 30, 90);

  if (affected_by_spell(victim, SPELL_ENFEEBLEMENT))
    affect_from_char(victim, SPELL_ENFEEBLEMENT);

  new_affect(&af);
  af.spell = SPELL_ENFEEBLEMENT;
  af.duration = -1;
  af.modifier = -stat_pen;
  af.location = APPLY_STR;
  affect_join(victim, &af, FALSE, FALSE, FALSE, FALSE);

  af.location = APPLY_DEX;
  affect_join(victim, &af, FALSE, FALSE, FALSE, FALSE);

  event = char_has_mud_event(victim, eSPL_ENFEEBLEMENT);
  if (event && event->pEvent)
    event_cancel(event->pEvent);

  NEW_EVENT(eSPL_ENFEEBLEMENT, victim, NULL, dur_sec * PASSES_PER_SEC);
}

ASPELL(spell_memento_mori)
{
  struct affected_type af;
  int power;
  int hr_pen;
  int sv_pen;
  int dur_ticks;
  int saved;

  if (victim == NULL || ch == NULL)
    return;

  act("You trace a \tDgrave sigil\tn over $N and whisper, '\tDMemento Mori\tn'.\tn",
      FALSE, ch, 0, victim, TO_CHAR);
  act("$n traces a \tDgrave sigil\tn over you and whispers, '\tDMemento Mori\tn'.\tn",
      FALSE, ch, 0, victim, TO_VICT);
  act("$n traces a \tDgrave sigil\tn over $N and whispers, '\tDMemento Mori\tn'.\tn",
      TRUE, ch, 0, victim, TO_ROOM);

  power = warlock_power(ch);
  hr_pen = clampi(1 + MAX(0, power - 20) / 12, 1, 4);
  sv_pen = clampi(1 + MAX(0, power - 20) / 10, 1, 6);
  dur_ticks = clampi(2 + MAX(0, power - 20) / 16, 2, 5);

  saved = mag_savingthrow(victim, SAVING_SPELL, 0);
  if (saved) {
    hr_pen = MAX(1, hr_pen / 2);
    sv_pen = MAX(1, sv_pen / 2);
    dur_ticks = MAX(1, dur_ticks / 2);
    act("$N resists most of your \tDomen\tn, but the mark still bites.\tn",
        FALSE, ch, 0, victim, TO_CHAR);
    act("You resist most of the \tDomen\tn, but the mark still bites.\tn",
        FALSE, ch, 0, victim, TO_VICT);
    act("$N resists most of the \tDomen\tn, but the mark still bites.\tn",
        TRUE, ch, 0, victim, TO_ROOM);
  }

  new_affect(&af);
  af.spell = SPELL_MEMENTO_MORI;
  af.duration = dur_ticks;
  af.modifier = -hr_pen;
  af.location = APPLY_HITROLL;
#ifdef AFF_CURSE
  SET_BIT_AR(af.bitvector, AFF_CURSE);
#endif
  affect_join(victim, &af, FALSE, FALSE, FALSE, FALSE);

  af.modifier = sv_pen;
  af.location = APPLY_SAVING_SPELL;
  affect_join(victim, &af, FALSE, FALSE, FALSE, FALSE);

  act("A \tDcold omen\tn settles on $N.\tn", FALSE, ch, 0, victim, TO_CHAR);
  act("A \tDcold omen\tn settles on you.\tn", FALSE, ch, 0, victim, TO_VICT);
  act("A \tDcold omen\tn settles on $N.\tn", TRUE, ch, 0, victim, TO_ROOM);
}

ASPELL(spell_devour_soul)
{
  struct affected_type af;
  int power;
  int dam;
  int mult = 200;
  int pct;
  int mana_d;
  int move_d;
  int hr_pen;
  int sv_pen;
  int dur_ticks;

  if (victim == NULL || ch == NULL)
    return;

  act("You reach out with \tDcold jaws\tn to \tDevour\tn $N's soul!\tn", FALSE, ch, 0, victim, TO_CHAR);
  act("$n reaches out with \tDcold jaws\tn to \tDevour\tn your soul!\tn", FALSE, ch, 0, victim, TO_VICT);
  act("$n reaches out with \tDcold jaws\tn to \tDevour\tn $N's soul!\tn", TRUE, ch, 0, victim, TO_ROOM);

  power = warlock_power(ch);
  dam = dice(8, 15) + (level * 3) + (power * 3 / 2);
  pct = clampi(1 + MAX(0, power - 20) / 2, 1, 15);

  if (mag_savingthrow(victim, SAVING_SPELL, 0)) {
    dam = (dam * 75) / 100;
    pct = MAX(1, pct / 2);
  }

  if (crit_check_spell(ch, &mult)) {
    dam = (dam * mult) / 100;
    crit_show_banner(ch, victim, mult);
  }

  if (damage(ch, victim, dam, SPELL_DEVOUR_SOUL) == -1)
    return;

  mana_d = MAX(1, (GET_MAX_MANA(victim) * pct) / 100);
  move_d = MAX(1, (GET_MAX_MOVE(victim) * pct) / 100);
  mana_d = MIN(mana_d, GET_MANA(victim));
  move_d = MIN(move_d, GET_MOVE(victim));

  GET_MANA(victim) = MAX(0, GET_MANA(victim) - mana_d);
  GET_MOVE(victim) = MAX(0, GET_MOVE(victim) - move_d);
  GET_MANA(ch) = MIN(GET_MAX_MANA(ch), GET_MANA(ch) + mana_d);
  GET_MOVE(ch) = MIN(GET_MAX_MOVE(ch), GET_MOVE(ch) + move_d);

  act("You drink in $N's essence, restoring your power.\tn", FALSE, ch, 0, victim, TO_CHAR);
  act("You feel your power ripped away as $n feeds on you!\tn", FALSE, ch, 0, victim, TO_VICT);
  act("$n feeds on $N's essence!\tn", TRUE, ch, 0, victim, TO_ROOM);

  hr_pen = clampi(1 + MAX(0, power - 20) / 8, 1, 6);
  sv_pen = clampi(1 + MAX(0, power - 20) / 6, 1, 8);
  dur_ticks = clampi(2 + MAX(0, power - 20) / 12, 2, 6);

  new_affect(&af);
  af.spell = SPELL_DEVOUR_SOUL;
  af.duration = dur_ticks;
  af.modifier = -hr_pen;
  af.location = APPLY_HITROLL;
  affect_join(victim, &af, FALSE, FALSE, FALSE, FALSE);

  af.modifier = sv_pen;
  af.location = APPLY_SAVING_SPELL;
  affect_join(victim, &af, FALSE, FALSE, FALSE, FALSE);
}

ASPELL(spell_vampiric_touch)
{
  int dam;
  int healed;

  if (victim == NULL || ch == NULL)
    return;

  set_spell_damage_type(DAM_NECROTIC);
  dam = (level * 3) + dice(3, MAX(1, level / 3));
  if (mag_savingthrow(victim, SAVING_SPELL, 0))
    dam /= 2;

  dam = damage(ch, victim, dam, SPELL_VAMPIRIC_TOUCH);
  if (dam <= 0)
    return;

  healed = (dam * 40) / 100;
  if (healed > 0)
    GET_HIT(ch) = MIN(GET_MAX_HIT(ch), GET_HIT(ch) + healed);
}

ASPELL(spell_greater_heal)
{
  int healing;

  if (victim == NULL || ch == NULL)
    return;

  healing = (level * 5) + dice(6, MAX(1, level));
  GET_HIT(victim) = MIN(GET_MAX_HIT(victim), GET_HIT(victim) + healing);
  update_pos(victim);

  if (affected_by_spell(victim, SPELL_POISON))
    affect_from_char(victim, SPELL_POISON);
  if (affected_by_spell(victim, SPELL_CURSE))
    affect_from_char(victim, SPELL_CURSE);
  if (affected_by_spell(victim, SPELL_BLINDNESS))
    affect_from_char(victim, SPELL_BLINDNESS);

  remove_flagged_affects(victim, AFF_POISON);
  remove_flagged_affects(victim, AFF_CURSE);
  remove_flagged_affects(victim, AFF_BLIND);
}

ASPELL(spell_cleanse)
{
  if (victim == NULL || ch == NULL)
    return;

  if (affected_by_spell(victim, SPELL_POISON))
    affect_from_char(victim, SPELL_POISON);
  if (affected_by_spell(victim, SPELL_CURSE))
    affect_from_char(victim, SPELL_CURSE);
  if (affected_by_spell(victim, SPELL_BLINDNESS))
    affect_from_char(victim, SPELL_BLINDNESS);

  remove_flagged_affects(victim, AFF_POISON);
  remove_flagged_affects(victim, AFF_CURSE);
  remove_flagged_affects(victim, AFF_BLIND);
  remove_flagged_affects(victim, AFF_SILENCED);
  remove_flagged_affects(victim, AFF_CORRODED);
  remove_flagged_affects(victim, AFF_WEBBED);
  remove_flagged_affects(victim, AFF_ARCANE_LEAK);
  remove_flagged_affects(victim, AFF_FEARFUL);
  remove_flagged_affects(victim, AFF_HEXED);
  remove_flagged_affects(victim, AFF_SPELLLOCK);
}

ASPELL(spell_counterspell)
{
  struct affected_type af;

  if (!victim || !ch)
    return;

  new_affect(&af);
  af.spell = SPELL_COUNTERSPELL;
  af.duration = 1;
  af.location = APPLY_NONE;
  SET_BIT_AR(af.bitvector, AFF_SPELLLOCK);
  affect_join(victim, &af, FALSE, FALSE, FALSE, FALSE);
}

ASPELL(spell_spell_steal)
{
  struct affected_type *af, *chosen = NULL, copied;
  int failed_save;

  if (!victim || !ch)
    return;

  for (af = victim->affected; af; af = af->next) {
    if (af->spell <= 0 || af->duration <= 0)
      continue;
    if (IS_SET_AR(af->bitvector, AFF_CURSE) ||
        IS_SET_AR(af->bitvector, AFF_POISON) ||
        IS_SET_AR(af->bitvector, AFF_FEARFUL) ||
        IS_SET_AR(af->bitvector, AFF_WEBBED) ||
        IS_SET_AR(af->bitvector, AFF_SILENCED) ||
        IS_SET_AR(af->bitvector, AFF_STUNNED) ||
        IS_SET_AR(af->bitvector, AFF_TIME_SNARE) ||
        IS_SET_AR(af->bitvector, AFF_SPELLLOCK))
      continue;
    if (af->modifier >= 0 || af->location == APPLY_NONE ||
        IS_SET_AR(af->bitvector, AFF_SANCTUARY) ||
        IS_SET_AR(af->bitvector, AFF_WARDED) ||
        IS_SET_AR(af->bitvector, AFF_MIRROR_IMAGE) ||
        IS_SET_AR(af->bitvector, AFF_ELEMENTAL_WARD_FIRE) ||
        IS_SET_AR(af->bitvector, AFF_ELEMENTAL_WARD_COLD) ||
        IS_SET_AR(af->bitvector, AFF_ELEMENTAL_WARD_LIGHTNING) ||
        IS_SET_AR(af->bitvector, AFF_ELEMENTAL_WARD_ACID)) {
      chosen = af;
      break;
    }
  }

  if (!chosen) {
    act("There is nothing to steal from $N.", FALSE, ch, 0, victim, TO_CHAR);
    return;
  }

  copied = *chosen;
  failed_save = !mag_savingthrow(victim, SAVING_SPELL, 0);
  affect_remove(victim, chosen);
  if (failed_save) {
    copied.next = NULL;
    affect_join(ch, &copied, FALSE, FALSE, FALSE, FALSE);
  }
}

ASPELL(spell_cancellation)
{
  int remove_count = 1;
  int i;

  if (!victim || !ch)
    return;

  if (!mag_savingthrow(victim, SAVING_SPELL, 0))
    remove_count = 2;

  for (i = 0; i < remove_count; i++) {
    if (!cancellation_remove_one(victim))
      break;
  }
}

ASPELL(spell_identify)
{
  if (obj) {
    show_identify_item(ch, obj, IDENTIFY_FULL);
  } else if (victim) {		/* victim */
    struct time_info_data *victim_age = age(victim);

    send_to_char(ch, "Name: %s\r\n", GET_NAME(victim));
    if (!IS_NPC(victim))
      send_to_char(ch, "%s is %d years, %d months, %d days and %d hours old.\r\n",
	      GET_NAME(victim), victim_age->year, victim_age->month,
	      victim_age->day, victim_age->hours);
    send_to_char(ch, "Height %d cm, Weight %d pounds\r\n", GET_HEIGHT(victim), GET_WEIGHT(victim));
    send_to_char(ch, "Level: %d, Hits: %d, Mana: %d\r\n", GET_LEVEL(victim), GET_HIT(victim), GET_MANA(victim));
    send_to_char(ch, "AC: %d, Hitroll: %d, Damroll: %d\r\n", compute_armor_class(victim), GET_HITROLL(victim), GET_DAMROLL(victim));
    send_to_char(ch, "Str: %d/%d, Int: %d, Wis: %d, Dex: %d, Con: %d, Cha: %d\r\n",
	GET_STR(victim), GET_ADD(victim), GET_INT(victim),
	GET_WIS(victim), GET_DEX(victim), GET_CON(victim), GET_CHA(victim));
  }
}

/* Cannot use this spell on an equipped object or it will mess up the wielding
 * character's hit/dam totals. */
ASPELL(spell_enchant_weapon)
{
  int i;

  if (ch == NULL || obj == NULL)
    return;

  /* Either already enchanted or not a weapon. */
  if (GET_OBJ_TYPE(obj) != ITEM_WEAPON || OBJ_FLAGGED(obj, ITEM_MAGIC))
    return;

  /* Make sure no other affections. */
  for (i = 0; i < MAX_OBJ_AFFECT; i++)
    if (obj->affected[i].location != APPLY_NONE)
      return;

  SET_BIT_AR(GET_OBJ_EXTRA(obj), ITEM_MAGIC);

  obj->affected[0].location = APPLY_HITROLL;
  obj->affected[0].modifier = 1 + (level >= 18);

  obj->affected[1].location = APPLY_DAMROLL;
  obj->affected[1].modifier = 1 + (level >= 20);

  if (IS_GOOD(ch)) {
    SET_BIT_AR(GET_OBJ_EXTRA(obj), ITEM_ANTI_EVIL);
    act("$p glows blue.", FALSE, ch, obj, 0, TO_CHAR);
  } else if (IS_EVIL(ch)) {
    SET_BIT_AR(GET_OBJ_EXTRA(obj), ITEM_ANTI_GOOD);
    act("$p glows red.", FALSE, ch, obj, 0, TO_CHAR);
  } else
    act("$p glows yellow.", FALSE, ch, obj, 0, TO_CHAR);
}

ASPELL(spell_detect_poison)
{
  if (victim) {
    if (victim == ch) {
      if (AFF_FLAGGED(victim, AFF_POISON))
        send_to_char(ch, "You can sense poison in your blood.\r\n");
      else
        send_to_char(ch, "You feel healthy.\r\n");
    } else {
      if (AFF_FLAGGED(victim, AFF_POISON))
        act("You sense that $E is poisoned.", FALSE, ch, 0, victim, TO_CHAR);
      else
        act("You sense that $E is healthy.", FALSE, ch, 0, victim, TO_CHAR);
    }
  }

  if (obj) {
    switch (GET_OBJ_TYPE(obj)) {
    case ITEM_DRINKCON:
    case ITEM_FOUNTAIN:
    case ITEM_FOOD:
      if (GET_OBJ_VAL(obj, 3))
	act("You sense that $p has been contaminated.",FALSE,ch,obj,0,TO_CHAR);
      else
	act("You sense that $p is safe for consumption.", FALSE, ch, obj, 0,
	    TO_CHAR);
      break;
    default:
      send_to_char(ch, "You sense that it should not be consumed.\r\n");
    }
  }
}

ASPELL(spell_conjure_elemental)
{
  mob_vnum vnum = MOBVNUM_LESSER_ELEMENTAL;

  if (!ch)
    return;
  if (active_temp_summons(ch) > 0) {
    send_to_char(ch, "You already control an active summon.\r\n");
    return;
  }
  if (level >= 35)
    vnum = MOBVNUM_ELDER_ELEMENTAL;
  else if (level >= 25)
    vnum = MOBVNUM_GREATER_ELEMENTAL;
  else if (level >= 15)
    vnum = MOBVNUM_ELEMENTAL;
  summon_temp_follower(ch, vnum, MAX(1, level - 5), 10);
}

ASPELL(spell_call_wolves)
{
  int i, count;
  if (!ch)
    return;
  count = dice(1, 2) + 1;
  for (i = 0; i < count; i++)
    summon_temp_follower(ch, MOBVNUM_SUMMONED_WOLF, MAX(2, level - 8), 8);
}

ASPELL(spell_call_bears)
{
  if (!ch)
    return;
  summon_temp_follower(ch, MOBVNUM_SUMMONED_BEAR, MAX(1, level - 5), 8);
}

ASPELL(spell_animate_dead_greater)
{
  struct obj_data *corpse = obj;

  if (!ch)
    return;
  if (active_temp_summons(ch) > 0) {
    send_to_char(ch, "You already control an active summon.\r\n");
    return;
  }
  if (!corpse || !IS_CORPSE(corpse) || GET_OBJ_TIMER(corpse) > CONFIG_MAX_NPC_CORPSE_TIME) {
    send_to_char(ch, "You must target an NPC corpse in this room.\r\n");
    return;
  }
  summon_temp_follower(ch, MOBVNUM_GREATER_UNDEAD, MAX(1, level), 10);
  extract_obj(corpse);
}

ASPELL(spell_abyss_gate)
{
  mob_vnum vnum;
  struct char_data *mob;

  if (!ch)
    return;
  if (level < 30)
    vnum = MOBVNUM_LESSER_DEMON;
  else if (level < 40)
    vnum = MOBVNUM_DEMON;
  else
    vnum = MOBVNUM_GREATER_DEMON;

  mob = summon_temp_follower(ch, vnum, MAX(1, level - 3), 5);
  if (mob && rand_number(1, 100) <= 25) {
    stop_follower(mob);
    REMOVE_BIT_AR(AFF_FLAGS(mob), AFF_CHARM);
    hit(mob, ch, TYPE_UNDEFINED);
  }
}

ASPELL(spell_gate)
{
  mob_vnum vnum;

  if (!ch)
    return;
  if (active_temp_summons(ch) > 0) {
    send_to_char(ch, "You cannot open a mighty gate while another summon is active.\r\n");
    return;
  }
  if (IS_GOOD(ch))
    vnum = MOBVNUM_CELESTIAL_GUARDIAN;
  else if (IS_EVIL(ch))
    vnum = MOBVNUM_DEMON_LORD;
  else
    vnum = MOBVNUM_ELEMENTAL_TITAN;
  summon_temp_follower(ch, vnum, level + 2, 3);
}

ASPELL(spell_portal)
{
  struct obj_data *origin, *dest;
  room_rnum target_room;

  if (!ch || !victim)
    return;
  target_room = IN_ROOM(victim);
  if (target_room == NOWHERE || ROOM_FLAGGED(target_room, ROOM_PRIVATE) ||
      ROOM_FLAGGED(target_room, ROOM_DEATH) || ROOM_FLAGGED(target_room, ROOM_NOMAGIC)) {
    send_to_char(ch, "Your portal cannot anchor there.\r\n");
    return;
  }

  origin = read_object(OBJVNUM_SPELL_PORTAL, VIRTUAL);
  dest = read_object(OBJVNUM_SPELL_PORTAL, VIRTUAL);
  if (!origin || !dest) {
    send_to_char(ch, "The portal magic fizzles; the focus object is missing.\r\n");
    return;
  }
  GET_OBJ_VAL(origin, 0) = GET_ROOM_VNUM(target_room);
  GET_OBJ_VAL(dest, 0) = GET_ROOM_VNUM(IN_ROOM(ch));
  GET_OBJ_TIMER(origin) = 5;
  GET_OBJ_TIMER(dest) = 5;
  obj_to_room(origin, IN_ROOM(ch));
  obj_to_room(dest, target_room);
  send_to_room(IN_ROOM(ch), "A shimmering portal tears open in the air here!\r\n");
  send_to_room(target_room, "A shimmering portal tears open in the air here!\r\n");
}

ASPELL(spell_locate_corpse)
{
  struct obj_data *i;
  room_rnum room = NOWHERE;
  int dir = BFS_NO_PATH;
  char *query = cast_arg2;

  if (!ch || !query || !*query) {
    send_to_char(ch, "Locate which corpse?\r\n");
    return;
  }

  for (i = object_list; i; i = i->next) {
    if (!IS_CORPSE(i) || !IN_ROOM(i))
      continue;
    if (strstr(i->short_description, query) || strstr(i->name, query)) {
      room = IN_ROOM(i);
      break;
    }
  }
  if (room == NOWHERE) {
    send_to_char(ch, "You sense no matching corpse.\r\n");
    return;
  }

  if (bfs_within_range(IN_ROOM(ch), room, 10, &dir) && dir >= 0)
    send_to_char(ch, "The corpse of %s lies in %s (vnum %d) to the %s.\r\n",
                 query, world[room].name, GET_ROOM_VNUM(room), dirs[dir]);
  else
    send_to_char(ch, "The corpse of %s lies in %s (vnum %d) to the far away.\r\n",
                 query, world[room].name, GET_ROOM_VNUM(room));
}

ASPELL(spell_word_of_recall_mass)
{
  struct char_data *tch;
  if (!ch)
    return;
  if (ROOM_FLAGGED(IN_ROOM(ch), ROOM_DEATH) || ZONE_FLAGGED(GET_ROOM_ZONE(IN_ROOM(ch)), ZONE_NOASTRAL)) {
    send_to_char(ch, "A dark force blocks your mass recall.\r\n");
    return;
  }

  if (!GROUP(ch))
    return;

  while ((tch = (struct char_data *) simple_list(GROUP(ch)->members)) != NULL) {
    room_rnum to_room;
    if (IN_ROOM(tch) != IN_ROOM(ch))
      continue;
    to_room = (!IS_NPC(tch) && GET_LOADROOM(tch) != NOWHERE) ? real_room(GET_LOADROOM(tch)) : r_mortal_start_room;
    if (to_room == NOWHERE)
      to_room = r_mortal_start_room;
    act("$n disappears.", TRUE, tch, 0, 0, TO_ROOM);
    char_from_room(tch);
    char_to_room(tch, to_room);
    act("$n appears in the middle of the room.", TRUE, tch, 0, 0, TO_ROOM);
    look_at_room(tch, 0);
  }
}

ASPELL(spell_astral_projection)
{
  struct char_data *tch;
  if (!ch || !victim)
    return;
  tch = victim;
  send_to_char(ch, "[Astral] %s (vnum %d)\r\n%s\r\n",
               world[IN_ROOM(tch)].name, GET_ROOM_VNUM(IN_ROOM(tch)), world[IN_ROOM(tch)].description);
  for (tch = world[IN_ROOM(victim)].people; tch; tch = tch->next_in_room)
    if (CAN_SEE(ch, tch))
      send_to_char(ch, "  %s is here.\r\n", PERS(tch, ch));
  WAIT_STATE(ch, 2 * PULSE_VIOLENCE);
}

ASPELL(spell_ethereal_jaunt)
{
  room_rnum to_room;
  zone_rnum zone;
  int tries = 200;

  if (!ch)
    return;
  if (spell_on_cooldown(ch, SPELL_ETHEREAL_JAUNT)) {
    send_to_char(ch, "Ethereal jaunt is still on cooldown.\r\n");
    return;
  }
  zone = world[IN_ROOM(ch)].zone;
  do {
    to_room = rand_number(zone_table[zone].bot, zone_table[zone].top);
  } while (--tries > 0 && (ROOM_FLAGGED(to_room, ROOM_PRIVATE) || ROOM_FLAGGED(to_room, ROOM_DEATH)));
  if (tries <= 0) {
    send_to_char(ch, "The ethereal plane refuses your passage.\r\n");
    return;
  }
  act("$n blurs and vanishes through the ethereal plane.", TRUE, ch, 0, 0, TO_ROOM);
  char_from_room(ch);
  char_to_room(ch, to_room);
  look_at_room(ch, 0);
  set_spell_cooldown(ch, SPELL_ETHEREAL_JAUNT, 3);
}

ASPELL(spell_leyline_tap)
{
  int restored;
  if (!ch)
    return;
  if (FIGHTING(ch)) {
    send_to_char(ch, "You cannot tap a leyline while in combat.\r\n");
    return;
  }
  if (spell_on_cooldown(ch, SPELL_LEYLINE_TAP)) {
    send_to_char(ch, "You must wait before tapping another leyline.\r\n");
    return;
  }
  restored = (level * 3) + dice(3, MAX(1, level));
  GET_MANA(ch) = MIN(GET_MAX_MANA(ch), GET_MANA(ch) + restored);
  set_spell_cooldown(ch, SPELL_LEYLINE_TAP, 5);
}

ASPELL(spell_temporal_shift)
{
  if (!ch)
    return;
  if (victim == ch) {
    GET_WAIT_STATE(ch) = 0;
    return;
  }
  if (!victim)
    return;
  if (!mag_savingthrow(victim, SAVING_SPELL, 0))
    GET_WAIT_STATE(victim) = MAX(GET_WAIT_STATE(victim), 3 * PULSE_VIOLENCE);
}

ASPELL(spell_chrono_shift)
{
  if (!ch)
    return;
  if (spell_on_cooldown(ch, SPELL_CHRONO_SHIFT)) {
    send_to_char(ch, "Your timeline is still stabilizing.\r\n");
    return;
  }
  if (GET_HP_LAST_ROUND(ch) > GET_HIT(ch))
    GET_HIT(ch) = MIN(GET_MAX_HIT(ch), GET_HP_LAST_ROUND(ch));
  set_spell_cooldown(ch, SPELL_CHRONO_SHIFT, 10);
}
