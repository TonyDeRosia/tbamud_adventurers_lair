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
#include "race.h"

static int clampi(int v, int lo, int hi)
{
  if (v < lo)
    return lo;
  if (v > hi)
    return hi;
  return v;
}

static int spell_dur_short_manual(int level);
static int spell_dur_medium_manual(int level);
static int spell_dur_long_manual(int level);
static int spell_dmg_low_manual(int level);
static int spell_dmg_medium_manual(int level);
static int spell_dmg_high_manual(int level);
static int spell_dmg_extreme_manual(int level);
static int spell_dmg_ultra_manual(int level);
static struct char_data *summon_temp_follower(struct char_data *ch, mob_vnum vnum, int level, int rounds);

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

static int is_shadow_servant(struct char_data *mob, struct char_data *owner)
{
  if (!mob || !IS_NPC(mob) || !AFF_FLAGGED(mob, AFF_CHARM) || GET_SUMMON_TIMER(mob) <= 0)
    return FALSE;
  if (owner && mob->master != owner)
    return FALSE;
  return affected_by_spell(mob, SPELL_CALL_SHADOW_LEGION)
      || affected_by_spell(mob, SPELL_SHADOW_EXTRACTION)
      || affected_by_spell(mob, SPELL_ARISE_GREATER);
}

static void mark_shadow_servant(struct char_data *mob, int source_spell, int duration)
{
  struct affected_type af;
  if (!mob)
    return;
  new_affect(&af);
  af.spell = source_spell;
  af.duration = MAX(1, duration);
  af.location = APPLY_NONE;
  af.modifier = 1;
  affect_join(mob, &af, FALSE, FALSE, FALSE, FALSE);
}

static int count_shadow_servants_in_room(struct char_data *ch)
{
  struct follow_type *f;
  int count = 0;
  if (!ch || IN_ROOM(ch) == NOWHERE)
    return 0;
  for (f = ch->followers; f; f = f->next) {
    struct char_data *mob = f->follower;
    if (!mob || IN_ROOM(mob) != IN_ROOM(ch))
      continue;
    if (is_shadow_servant(mob, ch))
      count++;
  }
  return count;
}

static struct char_data *summon_shadow_servant(struct char_data *ch, mob_vnum vnum, int level, int rounds, int source_spell)
{
  struct char_data *mob = summon_temp_follower(ch, vnum, level, rounds);
  if (mob)
    mark_shadow_servant(mob, source_spell, rounds);
  return mob;
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

ASPELL(spell_control_weather)
{
  int change;

  if (!OUTSIDE(ch)) {
    send_to_char(ch, "You can only influence the weather from outside.\r\n");
    return;
  }

  change = dice(1, 4) * (GET_ALIGNMENT(ch) >= 0 ? 1 : -1);
  weather_info.change += change;
  weather_info.change = MAX(-12, MIN(12, weather_info.change));

  send_to_char(ch, "You weave your will into the sky.\r\n");
  act("$n gestures skyward and the winds answer.", TRUE, ch, 0, 0, TO_ROOM);
}

ASPELL(spell_ventriloquate)
{
  struct char_data *to;
  char msg[MAX_INPUT_LENGTH];
  char from_name[MAX_NAME_LENGTH + 1];
  const char *spoken = cast_arg2;

  if (!ch || IN_ROOM(ch) == NOWHERE)
    return;

  if (!victim)
    victim = ch;

  if (!spoken || !*spoken)
    spoken = "...";

  snprintf(from_name, sizeof(from_name), "%s", GET_NAME(victim));
  CAP(from_name);
  snprintf(msg, sizeof(msg), "%s says, '%s'\r\n", from_name, spoken);

  send_to_char(ch, "You throw your voice through %s.\r\n", GET_NAME(victim));
  for (to = world[IN_ROOM(ch)].people; to; to = to->next_in_room) {
    if (to != ch && to != victim)
      send_to_char(to, "%s", msg);
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
  int dam;
  int healed;
  int mana_restored;

  if (victim == NULL || ch == NULL)
    return;

  act("You devour $N's soul, consuming their life essence!", FALSE, ch, 0, victim, TO_CHAR);
  act("$n devours your soul, consuming your life essence!", FALSE, ch, 0, victim, TO_VICT);
  act("$n devours $N's soul, consuming their life essence!", TRUE, ch, 0, victim, TO_NOTVICT);

  dam = spell_dmg_extreme_manual(level);
  if (mag_savingthrow(victim, SAVING_DEATH, 0))
    dam /= 2;
  set_next_damage_type(DAM_NECROTIC);
  dam = damage(ch, victim, dam, SPELL_DEVOUR_SOUL);
  if (dam <= 0)
    return;

  healed = (dam * 60) / 100;
  mana_restored = MIN(50, dam / 4);
  if (healed > 0)
    GET_HIT(ch) = MIN(GET_MAX_HIT(ch), GET_HIT(ch) + healed);
  if (mana_restored > 0)
    GET_MANA(ch) = MIN(GET_MAX_MANA(ch), GET_MANA(ch) + mana_restored);
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

static int spell_dur_short_manual(int level) { return 2 + (level / 10); }
static int spell_dur_medium_manual(int level) { return 4 + (level / 8); }
static int spell_dur_long_manual(int level) { return 6 + (level / 6); }
static int spell_dmg_low_manual(int level) { return (level * 2) + dice(2, MAX(1, level / 4)); }
static int spell_dmg_medium_manual(int level) { return (level * 3) + dice(3, MAX(1, level / 3)); }
static int spell_dmg_high_manual(int level) { return (level * 4) + dice(4, MAX(1, level / 2)); }
static int spell_dmg_extreme_manual(int level) { return (level * 5) + dice(5, MAX(1, level / 2)); }
static int spell_dmg_ultra_manual(int level) { return (level * 6) + dice(6, MAX(1, level / 2)); }

static int spell_is_enemy(struct char_data *ch, struct char_data *tch, int spellnum)
{
  if (!ch || !tch || tch == ch)
    return FALSE;
  if (!IS_NPC(tch) && GET_LEVEL(tch) >= LVL_IMMORT)
    return FALSE;
  if (!CONFIG_PK_ALLOWED && !IS_NPC(ch) && !IS_NPC(tch))
    return FALSE;
  if (!IS_NPC(ch) && IS_NPC(tch) && AFF_FLAGGED(tch, AFF_CHARM))
    return FALSE;
  if (!IS_NPC(tch) && spell_info[spellnum].violent && GROUP(tch) && GROUP(ch) && GROUP(ch) == GROUP(tch))
    return FALSE;
  return TRUE;
}

static int spell_is_undead(struct char_data *victim)
{
  if (!victim)
    return FALSE;
  return GET_RACE(victim) == RACE_VAMPIRE;
}

static int spell_apply_flag(struct char_data *victim, int spellnum, int duration, int flag)
{
  struct affected_type af;
  if (!victim)
    return FALSE;
  new_affect(&af);
  af.spell = spellnum;
  af.duration = MAX(1, duration);
  af.location = APPLY_NONE;
  SET_BIT_AR(af.bitvector, flag);
  affect_join(victim, &af, FALSE, FALSE, FALSE, FALSE);
  return TRUE;
}

static int spell_apply_modifier(struct char_data *victim, int spellnum, int duration, int location, int modifier)
{
  struct affected_type af;
  if (!victim)
    return FALSE;
  new_affect(&af);
  af.spell = spellnum;
  af.duration = MAX(1, duration);
  af.location = location;
  af.modifier = modifier;
  affect_join(victim, &af, FALSE, FALSE, FALSE, FALSE);
  return TRUE;
}

static int goal_of_all_life_is_death_active(struct char_data *ch)
{
  return ch && affected_by_spell(ch, SPELL_GOAL_OF_ALL_LIFE_IS_DEATH);
}

static int triple_maximize_magic_active(struct char_data *ch)
{
  return ch && affected_by_spell(ch, SPELL_TRIPLE_MAXIMIZE_MAGIC);
}

static int dimensional_lock_blocks_room(room_rnum room)
{
  if (room == NOWHERE)
    return FALSE;
  return room_has_effect(&world[room], ROOM_EFFECT_DIMENSIONAL_LOCK);
}

static int spell_instant_kill(struct char_data *ch, struct char_data *victim, int spellnum, enum damage_type dtype)
{
  int kill_dam;
  if (!ch || !victim)
    return FALSE;
  kill_dam = MAX(1, GET_HIT(victim)) + 1000;
  set_next_damage_type(dtype);
  return (damage(ch, victim, kill_dam, spellnum) == -1);
}

ASPELL(spell_balefire)
{
  int dam;
  if (!ch || !victim)
    return;
  if (spell_on_cooldown(ch, SPELL_BALEFIRE)) {
    send_to_char(ch, "Balefire is still on cooldown.\r\n");
    return;
  }
  act("You unleash a lance of pure balefire that tears through $N's very existence!", FALSE, ch, 0, victim, TO_CHAR);
  act("$n unleashes a lance of pure balefire that tears through $N's very existence!", TRUE, ch, 0, victim, TO_NOTVICT);
  dam = spell_dmg_extreme_manual(level);
  if (mag_savingthrow(victim, SAVING_SPELL, 0))
    dam /= 2;
  set_next_damage_type(DAM_ARCANE);
  if (damage(ch, victim, dam, SPELL_BALEFIRE) >= 0)
    cancellation_remove_one(victim);
  set_spell_cooldown(ch, SPELL_BALEFIRE, 3);
}

ASPELL(spell_meteor)
{
  struct char_data *tch, *next_tch;
  int saved;
  int dam;
  if (!ch || !victim)
    return;
  act("You call down a meteor from the heavens onto $N!", FALSE, ch, 0, victim, TO_CHAR);
  act("$n calls down a meteor from the heavens onto $N!", TRUE, ch, 0, victim, TO_NOTVICT);
  saved = mag_savingthrow(victim, SAVING_SPELL, 0);
  dam = spell_dmg_extreme_manual(level);
  if (saved)
    dam /= 2;
  set_next_damage_type(DAM_FIRE);
  if (damage(ch, victim, dam, SPELL_METEOR) == -1)
    return;
  if (saved)
    return;
  for (tch = world[IN_ROOM(ch)].people; tch; tch = next_tch) {
    int splash;
    next_tch = tch->next_in_room;
    if (tch == victim || !spell_is_enemy(ch, tch, SPELL_METEOR))
      continue;
    splash = spell_dmg_low_manual(level);
    set_next_damage_type(DAM_FIRE);
    damage(ch, tch, splash, SPELL_METEOR);
  }
}

ASPELL(spell_meteor_swarm)
{
  struct char_data *tch, *next_tch;
  int i;
  if (!ch)
    return;
  act("You call down a swarm of meteors from the heavens!", FALSE, ch, 0, 0, TO_CHAR);
  act("$n calls down a swarm of meteors from the heavens!", TRUE, ch, 0, 0, TO_ROOM);
  for (tch = world[IN_ROOM(ch)].people; tch; tch = next_tch) {
    next_tch = tch->next_in_room;
    if (!spell_is_enemy(ch, tch, SPELL_METEOR_SWARM))
      continue;
    for (i = 0; i < 4; i++) {
      int dam = spell_dmg_high_manual(level);
      if (mag_savingthrow(tch, SAVING_SPELL, 0))
        dam /= 2;
      set_next_damage_type(DAM_FIRE);
      if (damage(ch, tch, dam, SPELL_METEOR_SWARM) == -1)
        break;
    }
  }
}

ASPELL(spell_hellfire)
{
  int dam;
  if (!ch || !victim)
    return;
  act("Hellfire erupts from below, incinerating $N!", FALSE, ch, 0, victim, TO_CHAR);
  act("Hellfire erupts from below, incinerating $N!", TRUE, ch, 0, victim, TO_NOTVICT);
  dam = spell_dmg_extreme_manual(level);
  if (mag_savingthrow(victim, SAVING_SPELL, 0))
    dam /= 2;
  if (IS_GOOD(victim))
    dam = (dam * 125) / 100;
  else if (IS_EVIL(victim))
    dam = (dam * 75) / 100;
  set_next_damage_type(DAM_FIRE);
  damage(ch, victim, dam, SPELL_HELLFIRE);
}

ASPELL(spell_wrathfire)
{
  int opposite = (IS_GOOD(ch) && IS_EVIL(victim)) || (IS_EVIL(ch) && IS_GOOD(victim));
  int dam;
  if (!ch || !victim)
    return;
  act("Wrathfire explodes from your hands, fueled by divine fury!", FALSE, ch, 0, victim, TO_CHAR);
  act("$n releases wrathfire fueled by divine fury!", TRUE, ch, 0, victim, TO_NOTVICT);
  dam = opposite ? spell_dmg_extreme_manual(level) : spell_dmg_low_manual(level);
  if (mag_savingthrow(victim, SAVING_SPELL, 0))
    dam /= 2;
  set_next_damage_type(DAM_HOLY);
  damage(ch, victim, dam, SPELL_WRATHFIRE);
}

ASPELL(spell_celestial_smite)
{
  int dam;
  if (!ch || !victim)
    return;
  act("Celestial fire descends to smite $N in your deity's name!", FALSE, ch, 0, victim, TO_CHAR);
  act("Celestial fire descends to smite $N in $n's deity's name!", TRUE, ch, 0, victim, TO_NOTVICT);
  dam = spell_is_undead(victim) ? spell_dmg_ultra_manual(level) : spell_dmg_extreme_manual(level);
  if (mag_savingthrow(victim, SAVING_SPELL, 0))
    dam /= 2;
  set_next_damage_type(DAM_HOLY);
  damage(ch, victim, dam, SPELL_CELESTIAL_SMITE);
}

ASPELL(spell_hammer_of_god)
{
  int saved;
  int dam;
  if (!ch || !victim)
    return;
  act("The Hammer of God descends on $N!", FALSE, ch, 0, victim, TO_CHAR);
  act("The Hammer of God descends on $N!", TRUE, ch, 0, victim, TO_NOTVICT);
  saved = mag_savingthrow(victim, SAVING_SPELL, 0);
  dam = spell_dmg_extreme_manual(level);
  if (saved)
    dam /= 2;
  set_next_damage_type(DAM_HOLY);
  if (damage(ch, victim, dam, SPELL_HAMMER_OF_GOD) == -1)
    return;
  if (!saved) {
    spell_apply_flag(victim, SPELL_HAMMER_OF_GOD, 2, AFF_STUNNED);
    spell_apply_flag(victim, SPELL_HAMMER_OF_GOD, spell_dur_short_manual(level), AFF_ROOTED);
  }
}

ASPELL(spell_death_knell)
{
  int dam;
  int saved;
  if (!ch || !victim)
    return;
  act("You sound the death knell for $N's existence!", FALSE, ch, 0, victim, TO_CHAR);
  act("$n sounds the death knell for $N's existence!", TRUE, ch, 0, victim, TO_NOTVICT);
  if (GET_HIT(victim) * 100 <= GET_MAX_HIT(victim) * 20) {
    dam = spell_dmg_extreme_manual(level) + MAX(0, GET_HIT(victim));
    saved = mag_savingthrow(victim, SAVING_DEATH, 0);
    if (saved)
      dam /= 2;
  } else {
    send_to_char(ch, "Death Knell fails to find purchase. %s is too healthy.\r\n", GET_NAME(victim));
    dam = spell_dmg_low_manual(level);
  }
  set_next_damage_type(DAM_NECROTIC);
  damage(ch, victim, dam, SPELL_DEATH_KNELL);
}

ASPELL(spell_unholy_word)
{
  struct char_data *tch, *next_tch;
  if (!ch)
    return;
  act("You unleash the Unholy Word, speaking blasphemy into reality!", FALSE, ch, 0, 0, TO_CHAR);
  act("$n unleashes the Unholy Word, speaking blasphemy into reality!", TRUE, ch, 0, 0, TO_ROOM);
  for (tch = world[IN_ROOM(ch)].people; tch; tch = next_tch) {
    int saved;
    int dam;
    next_tch = tch->next_in_room;
    if (!spell_is_enemy(ch, tch, SPELL_UNHOLY_WORD))
      continue;
    if (IS_EVIL(tch))
      continue;
    saved = mag_savingthrow(tch, SAVING_SPELL, 0);
    dam = spell_dmg_extreme_manual(level);
    if (saved)
      dam /= 2;
    set_next_damage_type(DAM_SHADOW);
    if (damage(ch, tch, dam, SPELL_UNHOLY_WORD) == -1)
      continue;
    if (!saved) {
      spell_apply_flag(tch, SPELL_UNHOLY_WORD, 2, AFF_STUNNED);
      spell_apply_flag(tch, SPELL_UNHOLY_WORD, spell_dur_short_manual(level), AFF_BLINDED_MAGICAL);
    }
  }
}

ASPELL(spell_holy_word)
{
  struct char_data *tch, *next_tch;
  if (!ch)
    return;
  act("You speak the Holy Word and divine light fills the room!", FALSE, ch, 0, 0, TO_CHAR);
  act("$n speaks the Holy Word and divine light fills the room!", TRUE, ch, 0, 0, TO_ROOM);
  for (tch = world[IN_ROOM(ch)].people; tch; tch = next_tch) {
    int saved;
    int dam;
    next_tch = tch->next_in_room;
    if (!spell_is_enemy(ch, tch, SPELL_HOLY_WORD))
      continue;
    if (IS_GOOD(tch))
      continue;
    if (!IS_EVIL(tch))
      continue;
    saved = mag_savingthrow(tch, SAVING_SPELL, 0);
    dam = spell_dmg_extreme_manual(level);
    if (saved)
      dam /= 2;
    set_next_damage_type(DAM_HOLY);
    if (damage(ch, tch, dam, SPELL_HOLY_WORD) == -1)
      continue;
    if (!saved) {
      spell_apply_flag(tch, SPELL_HOLY_WORD, 2, AFF_STUNNED);
      spell_apply_flag(tch, SPELL_HOLY_WORD, spell_dur_short_manual(level), AFF_BLINDED_MAGICAL);
    }
  }
}

ASPELL(spell_finger_of_death)
{
  int saved;
  if (!ch || !victim)
    return;
  act("You point your finger at $N, channeling the finger of death!", FALSE, ch, 0, victim, TO_CHAR);
  act("$n points a finger at $N, channeling the finger of death!", TRUE, ch, 0, victim, TO_NOTVICT);
  if (spell_is_undead(victim)) {
    int dam = spell_dmg_high_manual(level);
    set_next_damage_type(DAM_NECROTIC);
    damage(ch, victim, dam, SPELL_FINGER_OF_DEATH);
    return;
  }
  saved = mag_savingthrow(victim, SAVING_DEATH, 0);
  if (!saved) {
    if (spell_instant_kill(ch, victim, SPELL_FINGER_OF_DEATH, DAM_NECROTIC))
      return;
  }
  set_next_damage_type(DAM_NECROTIC);
  damage(ch, victim, spell_dmg_extreme_manual(level), SPELL_FINGER_OF_DEATH);
}

ASPELL(spell_wail_of_the_banshee)
{
  struct char_data *tch, *next_tch;
  if (!ch)
    return;
  act("You unleash the wail of a banshee!", FALSE, ch, 0, 0, TO_CHAR);
  act("$n unleashes the wail of a banshee!", TRUE, ch, 0, 0, TO_ROOM);
  for (tch = world[IN_ROOM(ch)].people; tch; tch = next_tch) {
    int saved;
    next_tch = tch->next_in_room;
    if (!spell_is_enemy(ch, tch, SPELL_WAIL_OF_THE_BANSHEE))
      continue;
    if (spell_is_undead(tch))
      continue;
    saved = mag_savingthrow(tch, SAVING_DEATH, 0);
    if (!saved) {
      if (spell_instant_kill(ch, tch, SPELL_WAIL_OF_THE_BANSHEE, DAM_NECROTIC))
        continue;
    }
    set_next_damage_type(DAM_NECROTIC);
    damage(ch, tch, spell_dmg_extreme_manual(level), SPELL_WAIL_OF_THE_BANSHEE);
  }
}

ASPELL(spell_disintegrate)
{
  int saved;
  int dam;
  if (!ch || !victim)
    return;
  act("A green ray of disintegrating energy erupts from your hand at $N!", FALSE, ch, 0, victim, TO_CHAR);
  act("A green ray of disintegrating energy erupts from $n's hand at $N!", TRUE, ch, 0, victim, TO_NOTVICT);
  saved = mag_savingthrow(victim, SAVING_SPELL, -10);
  dam = saved ? spell_dmg_high_manual(level) : (GET_MAX_HIT(victim) / 2 + spell_dmg_extreme_manual(level));
  set_next_damage_type(DAM_ARCANE);
  damage(ch, victim, dam, SPELL_DISINTEGRATE);
}

ASPELL(spell_power_word_kill)
{
  if (!ch || !victim)
    return;
  act("You speak the Power Word: Kill at $N!", FALSE, ch, 0, victim, TO_CHAR);
  act("$n speaks the Power Word: Kill at $N!", TRUE, ch, 0, victim, TO_NOTVICT);
  if (GET_MAX_HIT(victim) <= 50 + (level * 2)) {
    if (spell_instant_kill(ch, victim, SPELL_POWER_WORD_KILL, DAM_ARCANE))
      return;
  }
  set_next_damage_type(DAM_ARCANE);
  damage(ch, victim, spell_dmg_extreme_manual(level), SPELL_POWER_WORD_KILL);
}

ASPELL(spell_power_word_stun)
{
  int dur;
  if (!ch || !victim)
    return;
  act("You speak the Power Word: Stun at $N!", FALSE, ch, 0, victim, TO_CHAR);
  act("$n speaks the Power Word: Stun at $N!", TRUE, ch, 0, victim, TO_NOTVICT);
  dur = (GET_MAX_HIT(victim) <= 100 + (level * 3)) ? spell_dur_medium_manual(level) : spell_dur_short_manual(level);
  spell_apply_flag(victim, SPELL_POWER_WORD_STUN, dur, AFF_STUNNED);
}

ASPELL(spell_power_word_blind)
{
  if (!ch || !victim)
    return;
  act("You speak the Power Word: Blind at $N!", FALSE, ch, 0, victim, TO_CHAR);
  act("$n speaks the Power Word: Blind at $N!", TRUE, ch, 0, victim, TO_NOTVICT);
  spell_apply_flag(victim, SPELL_POWER_WORD_BLIND, spell_dur_medium_manual(level), AFF_BLINDED_MAGICAL);
}

ASPELL(spell_power_word_silence)
{
  if (!ch || !victim)
    return;
  act("You speak the Power Word: Silence at $N!", FALSE, ch, 0, victim, TO_CHAR);
  act("$n speaks the Power Word: Silence at $N!", TRUE, ch, 0, victim, TO_NOTVICT);
  spell_apply_flag(victim, SPELL_POWER_WORD_SILENCE, spell_dur_medium_manual(level), AFF_SILENCED);
}

ASPELL(spell_psychic_crush)
{
  int saved;
  int dam;
  if (!ch || !victim)
    return;
  act("You crush $N's mind with overwhelming psychic force!", FALSE, ch, 0, victim, TO_CHAR);
  act("$n crushes $N's mind with overwhelming psychic force!", TRUE, ch, 0, victim, TO_NOTVICT);
  saved = mag_savingthrow(victim, SAVING_SPELL, 0);
  dam = spell_dmg_extreme_manual(level);
  if (saved)
    dam /= 2;
  set_next_damage_type(DAM_PSYCHIC);
  if (damage(ch, victim, dam, SPELL_PSYCHIC_CRUSH) == -1)
    return;
  if (!saved && GET_HIT(victim) * 100 <= GET_MAX_HIT(victim) * 30) {
    if (!mag_savingthrow(victim, SAVING_DEATH, 0))
      spell_instant_kill(ch, victim, SPELL_PSYCHIC_CRUSH, DAM_PSYCHIC);
  }
}

ASPELL(spell_time_stop)
{
  struct char_data *tch;
  struct affected_type af;
  if (!ch)
    return;
  act("Time itself grinds to a halt around you!", FALSE, ch, 0, 0, TO_CHAR);
  act("$n speaks a single word and time freezes!", TRUE, ch, 0, 0, TO_ROOM);
  for (tch = world[IN_ROOM(ch)].people; tch; tch = tch->next_in_room) {
    if (tch == ch)
      continue;
    if (!IS_NPC(tch))
      continue;
    if (!spell_is_enemy(ch, tch, SPELL_TIME_STOP))
      continue;
    GET_WAIT_STATE(tch) = MAX(GET_WAIT_STATE(tch), 3 * PULSE_VIOLENCE);
  }
  if (affected_by_spell(ch, SPELL_TIME_STOP))
    affect_from_char(ch, SPELL_TIME_STOP);
  new_affect(&af);
  af.spell = SPELL_TIME_STOP;
  af.duration = 3;
  af.location = APPLY_NONE;
  af.modifier = GET_ROOM_VNUM(IN_ROOM(ch));
  affect_join(ch, &af, FALSE, FALSE, FALSE, FALSE);
}

ASPELL(spell_black_lance)
{
  int saved, dam;
  if (!ch || !victim) return;
  saved = mag_savingthrow(victim, SAVING_SPELL, 0);
  dam = triple_maximize_magic_active(ch) ? (3 * ((level * 4) + (4 * MAX(1, level / 2)))) : spell_dmg_high_manual(level);
  if (triple_maximize_magic_active(ch))
    affect_from_char(ch, SPELL_TRIPLE_MAXIMIZE_MAGIC);
  if (saved) dam /= 2;
  set_next_damage_type(DAM_SHADOW);
  if (damage(ch, victim, dam, SPELL_BLACK_LANCE) == -1) return;
  if (!saved) {
    spell_apply_flag(victim, SPELL_BLACK_LANCE, spell_dur_short_manual(level), AFF_CORRODED);
    spell_apply_modifier(victim, SPELL_BLACK_LANCE, spell_dur_short_manual(level), APPLY_AC, 15);
  }
}

ASPELL(spell_reality_slash)
{
  int saved, dam;
  if (!ch || !victim) return;
  saved = mag_savingthrow(victim, SAVING_SPELL, 0);
  dam = spell_dmg_extreme_manual(level);
  if (saved) dam /= 2;
  set_next_damage_type(DAM_FORCE);
  damage(ch, victim, dam, SPELL_REALITY_SLASH);
}

ASPELL(spell_grasp_heart)
{
  int saved, dam;
  if (!ch || !victim) return;
  if (GET_HIT(victim) * 100 <= GET_MAX_HIT(victim) * 30) {
    /* GOAL_OF_ALL_LIFE_IS_DEATH_ACTIVE */
    saved = goal_of_all_life_is_death_active(ch) ? FALSE : mag_savingthrow(victim, SAVING_DEATH, 0);
    if (!saved && spell_instant_kill(ch, victim, SPELL_GRASP_HEART, DAM_NECROTIC))
      return;
    dam = spell_dmg_extreme_manual(level);
  } else {
    saved = mag_savingthrow(victim, SAVING_DEATH, 0);
    dam = spell_dmg_high_manual(level);
    if (saved) dam /= 2;
  }
  set_next_damage_type(DAM_NECROTIC);
  if (damage(ch, victim, dam, SPELL_GRASP_HEART) == -1) return;
  if (!saved && GET_HIT(victim) > 0 && GET_HIT(victim) * 100 > GET_MAX_HIT(victim) * 30)
    spell_apply_flag(victim, SPELL_GRASP_HEART, 1, AFF_STUNNED);
}

ASPELL(spell_negative_burst)
{
  struct char_data *tch, *next_tch;
  if (!ch) return;
  for (tch = world[IN_ROOM(ch)].people; tch; tch = next_tch) {
    int saved, dam;
    next_tch = tch->next_in_room;
    if (spell_is_enemy(ch, tch, SPELL_NEGATIVE_BURST)) {
      saved = mag_savingthrow(tch, SAVING_SPELL, 0);
      dam = spell_dmg_medium_manual(level);
      if (saved) dam /= 2;
      set_next_damage_type(DAM_NECROTIC);
      damage(ch, tch, dam, SPELL_NEGATIVE_BURST);
    } else if ((AFF_FLAGGED(tch, AFF_CHARM) && tch->master == ch) || spell_is_undead(tch)) {
      GET_HIT(tch) = MIN(GET_MAX_HIT(tch), GET_HIT(tch) + spell_dmg_low_manual(level));
    }
  }
}

ASPELL(spell_true_death)
{
  int saved, dam;
  if (!ch || !victim) return;
  saved = mag_savingthrow(victim, SAVING_DEATH, 0);
  dam = spell_dmg_extreme_manual(level);
  if (saved) dam /= 2;
  set_next_damage_type(DAM_NECROTIC);
  if (damage(ch, victim, dam, SPELL_TRUE_DEATH) == -1) return;
  if (!saved)
    spell_apply_modifier(victim, SPELL_TRUE_DEATH, spell_dur_long_manual(level), APPLY_NONE, 1);
}

ASPELL(spell_perfect_unknowable)
{
  if (!ch) return;
  spell_apply_flag(ch, SPELL_PERFECT_UNKNOWABLE, spell_dur_medium_manual(level), AFF_INVISIBLE);
  spell_apply_flag(ch, SPELL_PERFECT_UNKNOWABLE, spell_dur_medium_manual(level), AFF_HIDE);
}

ASPELL(spell_crystal_body)
{
  if (!ch) return;
  spell_apply_modifier(ch, SPELL_CRYSTAL_BODY, spell_dur_medium_manual(level), APPLY_AC, -25);
}

ASPELL(spell_greater_magic_seal)
{
  if (!ch || !victim) return;
  if (!mag_savingthrow(victim, SAVING_SPELL, 0)) {
    spell_apply_flag(victim, SPELL_GREATER_MAGIC_SEAL, spell_dur_medium_manual(level), AFF_SPELLLOCK);
    spell_apply_modifier(victim, SPELL_GREATER_MAGIC_SEAL, spell_dur_medium_manual(level), APPLY_NONE, 1);
  } else {
    spell_apply_flag(victim, SPELL_GREATER_MAGIC_SEAL, spell_dur_short_manual(level), AFF_SPELLLOCK);
  }
}

ASPELL(spell_despair_aura) { if (ch) spell_apply_modifier(ch, SPELL_DESPAIR_AURA, spell_dur_medium_manual(level), APPLY_NONE, 1); }
ASPELL(spell_oblivion_spear) { if (ch && victim) { int s = mag_savingthrow(victim, SAVING_SPELL, 0), dam = triple_maximize_magic_active(ch) ? (3 * ((level * 5) + (5 * MAX(1, level / 2)))) : spell_dmg_extreme_manual(level); if (triple_maximize_magic_active(ch)) affect_from_char(ch, SPELL_TRIPLE_MAXIMIZE_MAGIC); if (s) dam /= 2; set_next_damage_type(DAM_SHADOW); if (damage(ch, victim, dam, SPELL_OBLIVION_SPEAR) != -1 && !s) GET_MANA(victim) = MAX(0, GET_MANA(victim) - dam / 4); } }
ASPELL(spell_bone_prison) { if (ch && victim) { if (!mag_savingthrow(victim, SAVING_SPELL, 0)) { spell_apply_flag(victim, SPELL_BONE_PRISON, spell_dur_medium_manual(level), AFF_ROOTED); spell_apply_modifier(victim, SPELL_BONE_PRISON, spell_dur_medium_manual(level), APPLY_AC, 10);} else spell_apply_flag(victim, SPELL_BONE_PRISON, 1, AFF_ROOTED);} }
ASPELL(spell_undying_will) { if (ch) spell_apply_modifier(ch, SPELL_UNDYING_WILL, spell_dur_long_manual(level), APPLY_NONE, 1); }
ASPELL(spell_dragon_lightning) { if (ch && victim) { int s = mag_savingthrow(victim, SAVING_SPELL, 0), dam = triple_maximize_magic_active(ch) ? (3 * ((level * 4) + (4 * MAX(1, level / 2)))) : spell_dmg_high_manual(level); if (triple_maximize_magic_active(ch)) affect_from_char(ch, SPELL_TRIPLE_MAXIMIZE_MAGIC); if (s) dam /= 2; set_next_damage_type(DAM_LIGHTNING); damage(ch, victim, dam, SPELL_DRAGON_LIGHTNING);} }

ASPELL(spell_chain_dragon_lightning)
{
  struct char_data *tch, *next_tch;
  int jumps = 0;
  if (!ch || !victim) return;
  set_next_damage_type(DAM_LIGHTNING);
  damage(ch, victim, mag_savingthrow(victim, SAVING_SPELL, 0) ? spell_dmg_high_manual(level) / 2 : spell_dmg_high_manual(level), SPELL_CHAIN_DRAGON_LIGHTNING);
  for (tch = world[IN_ROOM(ch)].people; tch && jumps < 2; tch = next_tch) {
    next_tch = tch->next_in_room;
    if (tch == victim || !spell_is_enemy(ch, tch, SPELL_CHAIN_DRAGON_LIGHTNING))
      continue;
    set_next_damage_type(DAM_LIGHTNING);
    damage(ch, tch, mag_savingthrow(tch, SAVING_SPELL, 0) ? spell_dmg_medium_manual(level) / 2 : spell_dmg_medium_manual(level), SPELL_CHAIN_DRAGON_LIGHTNING);
    jumps++;
  }
}

ASPELL(spell_hell_flame) { if (ch && victim) { set_next_damage_type(DAM_FIRE); damage(ch, victim, spell_dmg_low_manual(level), SPELL_HELL_FLAME); if (!mag_savingthrow(victim, SAVING_SPELL, 0)) spell_apply_modifier(victim, SPELL_HELL_FLAME, spell_dur_medium_manual(level), APPLY_NONE, 1); } }
ASPELL(spell_gravity_maelstrom) { if (ch && victim) { int s = mag_savingthrow(victim, SAVING_SPELL, 0), dam = spell_dmg_extreme_manual(level); if (s) dam /= 2; set_next_damage_type(DAM_FORCE); if (damage(ch, victim, dam, SPELL_GRAVITY_MAELSTROM) == -1) return; if (!s) { spell_apply_modifier(victim, SPELL_GRAVITY_MAELSTROM, spell_dur_short_manual(level), APPLY_STR, -3); spell_apply_modifier(victim, SPELL_GRAVITY_MAELSTROM, spell_dur_short_manual(level), APPLY_DEX, -3); spell_apply_flag(victim, SPELL_GRAVITY_MAELSTROM, 1, AFF_ROOTED);} } }
ASPELL(spell_call_greater_thunder) { if (ch && victim) { int dam = (level * 5) + (5 * MAX(1, level / 2)); if (mag_savingthrow(victim, SAVING_SPELL, 0)) dam /= 2; set_next_damage_type(DAM_LIGHTNING); damage(ch, victim, dam, SPELL_CALL_GREATER_THUNDER);} }
ASPELL(spell_astral_smite) { if (ch && victim) { int dam = triple_maximize_magic_active(ch) ? (3 * ((level * 4) + (4 * MAX(1, level / 2)))) : spell_dmg_high_manual(level); if (triple_maximize_magic_active(ch)) affect_from_char(ch, SPELL_TRIPLE_MAXIMIZE_MAGIC); if (mag_savingthrow(victim, SAVING_SPELL, 0)) dam /= 2; set_next_damage_type(DAM_FORCE); damage(ch, victim, dam, SPELL_ASTRAL_SMITE);} }
ASPELL(spell_greater_rejection) { if (ch && victim) { if (!mag_savingthrow(victim, SAVING_SPELL, 0) && ((AFF_FLAGGED(victim, AFF_CHARM) && victim->master && victim->master != ch) || spell_is_undead(victim))) { spell_instant_kill(ch, victim, SPELL_GREATER_REJECTION, DAM_FORCE); } else { int dam = spell_dmg_medium_manual(level); if (mag_savingthrow(victim, SAVING_SPELL, 0)) { dam = spell_dmg_low_manual(level); spell_apply_flag(victim, SPELL_GREATER_REJECTION, 1, AFF_STUNNED);} set_next_damage_type(DAM_FORCE); damage(ch, victim, dam, SPELL_GREATER_REJECTION);} } }
ASPELL(spell_fallen_down) { struct char_data *tch,*next_tch; if (!ch) return; for (tch = world[IN_ROOM(ch)].people; tch; tch = next_tch) { int s, dam; next_tch = tch->next_in_room; if (!spell_is_enemy(ch, tch, SPELL_FALLEN_DOWN)) continue; s = mag_savingthrow(tch, SAVING_SPELL, 0); dam = spell_dmg_extreme_manual(level); if (s) dam /= 2; set_next_damage_type(DAM_FIRE); damage(ch, tch, dam, SPELL_FALLEN_DOWN);} }
ASPELL(spell_ia_shub_niggurath) { struct char_data *tch,*next_tch; int tribute = 0; if (!ch) return; for (tch = world[IN_ROOM(ch)].people; tch; tch = next_tch) { int s; next_tch = tch->next_in_room; if (!spell_is_enemy(ch, tch, SPELL_IA_SHUB_NIGGURATH) || spell_is_undead(tch)) continue; /* GOAL_OF_ALL_LIFE_IS_DEATH_ACTIVE */ s = goal_of_all_life_is_death_active(ch) ? FALSE : mag_savingthrow(tch, SAVING_DEATH, 0); if (!s) { if (spell_instant_kill(ch, tch, SPELL_IA_SHUB_NIGGURATH, DAM_NECROTIC)) tribute++; } else { set_next_damage_type(DAM_NECROTIC); damage(ch, tch, spell_dmg_extreme_manual(level), SPELL_IA_SHUB_NIGGURATH);} } if (tribute >= 3) send_to_char(ch, "The void accepts your tribute and something stirs beyond.\r\n"); }
ASPELL(spell_goal_of_all_life_is_death) { if (ch && !affected_by_spell(ch, SPELL_GOAL_OF_ALL_LIFE_IS_DEATH)) spell_apply_modifier(ch, SPELL_GOAL_OF_ALL_LIFE_IS_DEATH, spell_dur_short_manual(level), APPLY_NONE, 1); }
ASPELL(spell_cry_of_the_banshee) { struct char_data *tch,*next_tch; if (!ch) return; for (tch = world[IN_ROOM(ch)].people; tch; tch = next_tch) { int s, dam; next_tch = tch->next_in_room; if (!spell_is_enemy(ch, tch, SPELL_CRY_OF_THE_BANSHEE)) continue; if (goal_of_all_life_is_death_active(ch)) { s = FALSE; dam = spell_dmg_extreme_manual(level); } else { s = mag_savingthrow(tch, SAVING_DEATH, 0); dam = spell_dmg_high_manual(level); if (s) dam /= 2; } set_next_damage_type(DAM_NECROTIC); if (damage(ch, tch, dam, SPELL_CRY_OF_THE_BANSHEE) == -1) continue; if (!s) spell_apply_flag(tch, SPELL_CRY_OF_THE_BANSHEE, spell_dur_short_manual(level), AFF_FEARFUL); } }
ASPELL(spell_napalm) { struct char_data *tch,*next_tch; if (!ch) return; for (tch = world[IN_ROOM(ch)].people; tch; tch = next_tch) { int s, dam; next_tch = tch->next_in_room; if (!spell_is_enemy(ch, tch, SPELL_NAPALM)) continue; s = mag_savingthrow(tch, SAVING_SPELL, 0); dam = spell_dmg_medium_manual(level); if (s) dam /= 2; set_next_damage_type(DAM_FIRE); if (damage(ch, tch, dam, SPELL_NAPALM) == -1) continue; if (!s) spell_apply_modifier(tch, SPELL_NAPALM, 1, APPLY_NONE, 1); } }
ASPELL(spell_body_of_effulgent_beryl) { if (!ch) return; spell_apply_modifier(ch, SPELL_BODY_OF_EFFULGENT_BERYL, spell_dur_medium_manual(level), APPLY_AC, -20); spell_apply_modifier(ch, SPELL_BODY_OF_EFFULGENT_BERYL, spell_dur_medium_manual(level), APPLY_NONE, 1); }
ASPELL(spell_vermilion_nova) { struct char_data *tch,*next_tch; if (!ch) return; for (tch = world[IN_ROOM(ch)].people; tch; tch = next_tch) { int s, dam; next_tch = tch->next_in_room; if (!spell_is_enemy(ch, tch, SPELL_VERMILION_NOVA)) continue; s = mag_savingthrow(tch, SAVING_SPELL, 0); dam = spell_dmg_high_manual(level); if (s) dam /= 2; set_next_damage_type(DAM_FIRE); if (damage(ch, tch, dam, SPELL_VERMILION_NOVA) == -1) continue; if (!s) spell_apply_flag(tch, SPELL_VERMILION_NOVA, spell_dur_short_manual(level), AFF_BURNING); } }
ASPELL(spell_nuclear_blast) { struct char_data *tch,*next_tch; if (!ch) return; for (tch = world[IN_ROOM(ch)].people; tch; tch = next_tch) { int s, dam; next_tch = tch->next_in_room; if (!spell_is_enemy(ch, tch, SPELL_NUCLEAR_BLAST)) continue; s = mag_savingthrow(tch, SAVING_SPELL, 0); dam = spell_dmg_extreme_manual(level); if (s) dam /= 2; set_next_damage_type(DAM_FORCE); if (damage(ch, tch, dam, SPELL_NUCLEAR_BLAST) == -1) continue; set_next_damage_type(DAM_FIRE); damage(ch, tch, spell_dmg_low_manual(level), SPELL_NUCLEAR_BLAST); } }
ASPELL(spell_greater_teleportation)
{
  room_rnum to_room;
  if (!ch || !victim || IN_ROOM(victim) == NOWHERE) return;
  if (dimensional_lock_blocks_room(IN_ROOM(ch))) {
    send_to_char(ch, "Dimensional lock prevents your teleportation.\r\n");
    return;
  }
  to_room = IN_ROOM(victim);
  if (ROOM_FLAGGED(to_room, ROOM_PRIVATE) || ROOM_FLAGGED(to_room, ROOM_DEATH) ||
      ROOM_FLAGGED(to_room, ROOM_GODROOM) || ZONE_FLAGGED(GET_ROOM_ZONE(to_room), ZONE_CLOSED) ||
      ZONE_FLAGGED(GET_ROOM_ZONE(to_room), ZONE_NOASTRAL) || dimensional_lock_blocks_room(to_room)) {
    send_to_char(ch, "That destination resists your greater teleportation.\r\n");
    return;
  }
  act("$n vanishes as space folds inward.", FALSE, ch, 0, 0, TO_ROOM);
  char_from_room(ch);
  char_to_room(ch, to_room);
  act("$n steps out of folded space.", FALSE, ch, 0, 0, TO_ROOM);
  look_at_room(ch, 0);
  entry_memory_mtrigger(ch);
  greet_mtrigger(ch, -1);
  greet_memory_mtrigger(ch);
  handle_followers_after_owner_teleport_or_recall(ch);
}
ASPELL(spell_silent_magic) { if (ch) spell_apply_modifier(ch, SPELL_SILENT_MAGIC, spell_dur_short_manual(level), APPLY_NONE, 1); }
ASPELL(spell_triple_maximize_magic) { if (ch) spell_apply_modifier(ch, SPELL_TRIPLE_MAXIMIZE_MAGIC, 1, APPLY_NONE, 1); }
ASPELL(spell_pantheon) { if (!ch) return; spell_apply_modifier(ch, SPELL_PANTHEON, spell_dur_medium_manual(level), APPLY_SAVING_SPELL, 6); spell_apply_modifier(ch, SPELL_PANTHEON, spell_dur_medium_manual(level), APPLY_AC, -15); spell_apply_modifier(ch, SPELL_PANTHEON, spell_dur_medium_manual(level), APPLY_NONE, 1); }
ASPELL(spell_dimensional_lock) { if (ch && IN_ROOM(ch) != NOWHERE) room_add_effect(&world[IN_ROOM(ch)], ROOM_EFFECT_DIMENSIONAL_LOCK, spell_dur_medium_manual(level), 0); }

ASPELL(spell_shadow_bind) { if (!ch || !victim) return; if (!mag_savingthrow(victim, SAVING_SPELL, 0)) { spell_apply_flag(victim, SPELL_SHADOW_BIND, spell_dur_medium_manual(level), AFF_ROOTED); spell_apply_modifier(victim, SPELL_SHADOW_BIND, spell_dur_medium_manual(level), APPLY_DEX, -2); } else spell_apply_flag(victim, SPELL_SHADOW_BIND, 1, AFF_ROOTED); act("Your shadow lashes out and binds $N in place!", FALSE, ch, 0, victim, TO_CHAR); act("Shadowy tendrils rise and bind your limbs!", FALSE, ch, 0, victim, TO_VICT); act("$n's shadow erupts and binds $N!", FALSE, ch, 0, victim, TO_NOTVICT); }

ASPELL(spell_shadow_exchange)
{
  struct char_data *anchor = victim;
  if (!ch || IN_ROOM(ch) == NOWHERE)
    return;
  if (!anchor || IN_ROOM(anchor) != IN_ROOM(ch) ||
      !(is_shadow_servant(anchor, ch) || AFF_FLAGGED(anchor, AFF_MARKED))) {
    send_to_char(ch, "You find no valid shadow exchange anchor.\r\n");
    return;
  }
  spell_apply_modifier(ch, SPELL_SHADOW_EXCHANGE, 1, APPLY_HITROLL, 10);
  act("You slip through shadow and exchange places in an instant!", FALSE, ch, 0, 0, TO_CHAR);
  act("$n slips through shadow and reappears elsewhere!", FALSE, ch, 0, 0, TO_ROOM);
}

ASPELL(spell_dagger_rain) { int i, hits; if (!ch || !victim) return; hits = MIN(6, 3 + (level / 15)); act("A rain of shadow-forged blades tears into $N!", FALSE, ch, 0, victim, TO_CHAR); act("Shadow-forged blades rain into your flesh!", FALSE, ch, 0, victim, TO_VICT); act("$n sends a rain of shadow blades into $N!", FALSE, ch, 0, victim, TO_NOTVICT); for (i = 0; i < hits; i++) { int dam = spell_dmg_low_manual(level); if (mag_savingthrow(victim, SAVING_SPELL, 0)) dam /= 2; set_next_damage_type(DAM_SHADOW); if (damage(ch, victim, dam, SPELL_DAGGER_RAIN) == -1) break; } }

ASPELL(spell_monarchs_pressure)
{
  struct char_data *tch, *next_tch;
  if (!ch) return;
  act("You release a crushing monarch's pressure over the battlefield!", FALSE, ch, 0, 0, TO_CHAR);
  act("$n releases a crushing monarch's pressure!", FALSE, ch, 0, 0, TO_ROOM);
  for (tch = world[IN_ROOM(ch)].people; tch; tch = next_tch) {
    next_tch = tch->next_in_room;
    if (!spell_is_enemy(ch, tch, SPELL_MONARCHS_PRESSURE))
      continue;
    if (!mag_savingthrow(tch, SAVING_SPELL, 0)) {
      spell_apply_flag(tch, SPELL_MONARCHS_PRESSURE, spell_dur_short_manual(level), AFF_FEARFUL);
      spell_apply_modifier(tch, SPELL_MONARCHS_PRESSURE, spell_dur_short_manual(level), APPLY_HITROLL, -6);
      WAIT_STATE(tch, PULSE_VIOLENCE);
      act("An overwhelming pressure crushes your will!", FALSE, ch, 0, tch, TO_VICT);
    } else {
      spell_apply_modifier(tch, SPELL_MONARCHS_PRESSURE, 1, APPLY_HITROLL, -2);
    }
  }
}

ASPELL(spell_shadow_domain) { if (!ch || IN_ROOM(ch) == NOWHERE) return; room_add_effect(&world[IN_ROOM(ch)], ROOM_EFFECT_SHADOW_DOMAIN, spell_dur_medium_manual(level), level); act("Darkness spreads outward as you establish a shadow domain!", FALSE, ch, 0, 0, TO_CHAR); act("Darkness spreads outward from $n into a living shadow domain!", FALSE, ch, 0, 0, TO_ROOM); }
ASPELL(spell_force_grasp) { if (!ch || !victim) return; { int saved = mag_savingthrow(victim, SAVING_SPELL, 0), dam = spell_dmg_medium_manual(level); if (saved) dam /= 2; set_next_damage_type(DAM_FORCE); damage(ch, victim, dam, SPELL_FORCE_GRASP); if (!saved) spell_apply_flag(victim, SPELL_FORCE_GRASP, 1, AFF_STUNNED);} act("Invisible force crushes around $N!", FALSE, ch, 0, victim, TO_CHAR); act("Invisible force seizes and crushes you!", FALSE, ch, 0, victim, TO_VICT); act("$n seizes $N with an invisible crushing force!", FALSE, ch, 0, victim, TO_NOTVICT); }
ASPELL(spell_shadow_step) { if (!ch) return; spell_apply_modifier(ch, SPELL_SHADOW_STEP, 1, APPLY_HITROLL, 8); act("You vanish into shadow, ready to strike from the dark!", FALSE, ch, 0, 0, TO_CHAR); act("$n vanishes into a blur of shadow!", FALSE, ch, 0, 0, TO_ROOM); }
ASPELL(spell_black_heart) { int hp_loss, mana_gain; if (!ch) return; hp_loss = MAX(1, (GET_HIT(ch) * 20) / 100); GET_HIT(ch) = MAX(1, GET_HIT(ch) - hp_loss); mana_gain = (effective_max_mana(ch) * 35) / 100; GET_MANA(ch) = MIN(effective_max_mana(ch), GET_MANA(ch) + mana_gain); spell_apply_flag(ch, SPELL_BLACK_HEART, spell_dur_short_manual(level), AFF_EMPOWERED); act("You ignite the Black Heart within and trade blood for power!", FALSE, ch, 0, 0, TO_CHAR); act("$n's chest pulses with dark power as blood becomes mana!", FALSE, ch, 0, 0, TO_ROOM); }

ASPELL(spell_call_shadow_legion)
{
  int i, count;
  if (!ch) return;
  for (i = 0; i < 4; i++) {
    struct follow_type *f;
    for (f = ch->followers; f; f = f->next) {
      if (is_shadow_servant(f->follower, ch) && affected_by_spell(f->follower, SPELL_CALL_SHADOW_LEGION)) {
        send_to_char(ch, "Only one shadow legion may be active at a time.\r\n");
        return;
      }
    }
  }
  count = rand_number(2, 4);
  for (i = 0; i < count; i++)
    summon_shadow_servant(ch, MOBVNUM_SHADOW_SOLDIER, MAX(1, level - 6), 8, SPELL_CALL_SHADOW_LEGION);
  act("Shadows rise at your command as your legion answers!", FALSE, ch, 0, 0, TO_CHAR);
  act("Shadows rise from the ground to serve $n!", FALSE, ch, 0, 0, TO_ROOM);
}

ASPELL(spell_night_hunt) { if (!ch || !victim) return; if (mag_savingthrow(victim, SAVING_SPELL, 0)) return; spell_apply_flag(victim, SPELL_NIGHT_HUNT, spell_dur_long_manual(level), AFF_MARKED); act("You brand $N for the hunt!", FALSE, ch, 0, victim, TO_CHAR); act("A hunter's brand settles into your shadow!", FALSE, ch, 0, victim, TO_VICT); act("$n brands $N for the hunt!", FALSE, ch, 0, victim, TO_NOTVICT); }
ASPELL(spell_dark_rebuke) { if (!ch || !victim) return; { int dam = spell_dmg_medium_manual(level); if (mag_savingthrow(victim, SAVING_SPELL, 0)) dam /= 2; set_next_damage_type(DAM_SHADOW); if (damage(ch, victim, dam, SPELL_DARK_REBUKE) != -1 && (victim->affected || GET_POS(victim) == POS_FIGHTING)) { set_next_damage_type(DAM_SHADOW); damage(ch, victim, spell_dmg_low_manual(level), SPELL_DARK_REBUKE); } } act("You rebuke $N with a lash of punishing shadow!", FALSE, ch, 0, victim, TO_CHAR); act("Punishing shadow lashes across your body!", FALSE, ch, 0, victim, TO_VICT); act("$n rebukes $N with punishing shadow!", FALSE, ch, 0, victim, TO_NOTVICT); }
ASPELL(spell_execution_mark) { if (!ch || !victim) return; if (!mag_savingthrow(victim, SAVING_DEATH, 0)) spell_apply_flag(victim, SPELL_EXECUTION_MARK, spell_dur_medium_manual(level), AFF_MARKED); act("You place an execution mark upon $N!", FALSE, ch, 0, victim, TO_CHAR); act("A chilling execution mark settles over you!", FALSE, ch, 0, victim, TO_VICT); act("$n marks $N for execution!", FALSE, ch, 0, victim, TO_NOTVICT); }

ASPELL(spell_shadow_extraction)
{
  struct obj_data *corpse = obj;
  struct follow_type *f;
  if (!ch) return;
  if (!corpse || !IS_CORPSE(corpse)) {
    send_to_char(ch, "You must target a fresh corpse in the room.\r\n");
    return;
  }
  for (f = ch->followers; f; f = f->next) {
    if (is_shadow_servant(f->follower, ch) && affected_by_spell(f->follower, SPELL_SHADOW_EXTRACTION)) {
      send_to_char(ch, "You already control an extracted elite shadow.\r\n");
      return;
    }
  }
  summon_shadow_servant(ch, MOBVNUM_SHADOW_ELITE, MAX(1, level), 10, SPELL_SHADOW_EXTRACTION);
  extract_obj(corpse);
  act("You drag a shadow from the fallen and force it to rise!", FALSE, ch, 0, 0, TO_CHAR);
  act("$n drags a shadow from the fallen and forces it to rise!", FALSE, ch, 0, 0, TO_ROOM);
}

ASPELL(spell_arise_greater)
{
  struct obj_data *corpse = obj;
  struct follow_type *f;
  if (!ch) return;
  if (!corpse || !IS_CORPSE(corpse)) {
    send_to_char(ch, "You must target a fresh strong corpse in the room.\r\n");
    return;
  }
  for (f = ch->followers; f; f = f->next) {
    if (is_shadow_servant(f->follower, ch) && affected_by_spell(f->follower, SPELL_ARISE_GREATER)) {
      send_to_char(ch, "Only one greater arisen servant can answer you.\r\n");
      return;
    }
  }
  summon_shadow_servant(ch, MOBVNUM_GREATER_SHADOW, MAX(1, level + 2), 12, SPELL_ARISE_GREATER);
  extract_obj(corpse);
  act("You command the fallen to arise as a greater shadow!", FALSE, ch, 0, 0, TO_CHAR);
  act("$n commands the fallen to arise as a greater shadow!", FALSE, ch, 0, 0, TO_ROOM);
}

ASPELL(spell_monarchs_authority) { struct char_data *tch,*next_tch; if (!ch) return; act("You exert the crushing authority of a monarch over the battlefield!", FALSE, ch, 0, 0, TO_CHAR); act("$n exerts a crushing monarch's authority over the battlefield!", FALSE, ch, 0, 0, TO_ROOM); for (tch = world[IN_ROOM(ch)].people; tch; tch = next_tch) { int s, dam; next_tch = tch->next_in_room; if (!spell_is_enemy(ch, tch, SPELL_MONARCHS_AUTHORITY)) continue; s = mag_savingthrow(tch, SAVING_SPELL, 0); dam = spell_dmg_medium_manual(level); if (s) dam /= 2; set_next_damage_type(DAM_FORCE); if (damage(ch, tch, dam, SPELL_MONARCHS_AUTHORITY) == -1) continue; if (!s) { spell_apply_flag(tch, SPELL_MONARCHS_AUTHORITY, spell_dur_short_manual(level), AFF_ROOTED); WAIT_STATE(tch, PULSE_VIOLENCE);} } }
ASPELL(spell_rulers_hand) { if (!ch || !victim) return; { int s = mag_savingthrow(victim, SAVING_SPELL, 0), dam = spell_dmg_high_manual(level); if (s) dam /= 2; set_next_damage_type(DAM_FORCE); damage(ch, victim, dam, SPELL_RULERS_HAND); if (!s) spell_apply_flag(victim, SPELL_RULERS_HAND, 1, AFF_STUNNED);} act("You seize $N with the invisible force of the Ruler's Hand!", FALSE, ch, 0, victim, TO_CHAR); act("Invisible force seizes your body and crushes inward!", FALSE, ch, 0, victim, TO_VICT); act("$n seizes $N with an invisible crushing force!", FALSE, ch, 0, victim, TO_NOTVICT); }
ASPELL(spell_shadow_lance) { if (!ch || !victim) return; { int s = mag_savingthrow(victim, SAVING_SPELL, 0), dam = spell_dmg_high_manual(level); if (s) dam /= 2; set_next_damage_type(DAM_SHADOW); if (damage(ch, victim, dam, SPELL_SHADOW_LANCE) != -1 && !s) spell_apply_modifier(victim, SPELL_SHADOW_LANCE, spell_dur_short_manual(level), APPLY_AC, 10);} act("You shape a razor-thin lance of shadow and drive it into $N!", FALSE, ch, 0, victim, TO_CHAR); act("A razor-thin lance of shadow pierces straight through you!", FALSE, ch, 0, victim, TO_VICT); act("$n drives a razor-thin lance of shadow into $N!", FALSE, ch, 0, victim, TO_NOTVICT); }
ASPELL(spell_shadow_burst) { struct char_data *tch,*next_tch; int bonus = MIN(10, count_shadow_servants_in_room(ch) * 2); if (!ch) return; act("You detonate the darkness around you in a Shadow Burst!", FALSE, ch, 0, 0, TO_CHAR); act("$n detonates the darkness in a violent shadow burst!", FALSE, ch, 0, 0, TO_ROOM); for (tch = world[IN_ROOM(ch)].people; tch; tch = next_tch) { int dam; next_tch = tch->next_in_room; if (!spell_is_enemy(ch, tch, SPELL_SHADOW_BURST)) continue; dam = spell_dmg_medium_manual(level) + bonus; if (mag_savingthrow(tch, SAVING_SPELL, 0)) dam /= 2; set_next_damage_type(DAM_SHADOW); damage(ch, tch, dam, SPELL_SHADOW_BURST);} }
ASPELL(spell_shadow_storm) { if (!ch || IN_ROOM(ch) == NOWHERE) return; room_add_effect(&world[IN_ROOM(ch)], ROOM_EFFECT_SHADOW_STORM, spell_dur_medium_manual(level), 0); act("You call forth a violent storm of living shadow!", FALSE, ch, 0, 0, TO_CHAR); act("$n calls forth a violent storm of living shadow!", FALSE, ch, 0, 0, TO_ROOM); }
ASPELL(spell_fatal_strike) { if (!ch || !victim) return; { int dam = spell_dmg_high_manual(level); if (GET_HIT(victim) * 100 <= GET_MAX_HIT(victim) * 30) dam += spell_dmg_medium_manual(level); if (mag_savingthrow(victim, SAVING_SPELL, 0)) dam /= 2; set_next_damage_type(DAM_FORCE); damage(ch, victim, dam, SPELL_FATAL_STRIKE);} act("You drive a fatal strike into $N's opening!", FALSE, ch, 0, victim, TO_CHAR); act("A perfectly placed killing blow tears into you!", FALSE, ch, 0, victim, TO_VICT); act("$n drives a fatal strike into $N!", FALSE, ch, 0, victim, TO_NOTVICT); }
ASPELL(spell_dominion_of_shadows) { if (!ch) return; spell_apply_modifier(ch, SPELL_DOMINION_OF_SHADOWS, spell_dur_medium_manual(level), APPLY_SAVING_SPELL, -3); spell_apply_modifier(ch, SPELL_DOMINION_OF_SHADOWS, spell_dur_medium_manual(level), APPLY_NONE, 1); act("The dominion of shadows gathers beneath your rule!", FALSE, ch, 0, 0, TO_CHAR); act("Shadows thicken and bow beneath $n's dominion!", FALSE, ch, 0, 0, TO_ROOM); }

ASPELL(spell_shadow_recall)
{
  struct follow_type *f;
  if (!ch || IN_ROOM(ch) == NOWHERE)
    return;
  for (f = ch->followers; f; f = f->next) {
    struct char_data *mob = f->follower;
    if (!is_shadow_servant(mob, ch) || IN_ROOM(mob) == NOWHERE)
      continue;
    if (world[IN_ROOM(mob)].zone != world[IN_ROOM(ch)].zone)
      continue;
    if (IN_ROOM(mob) == IN_ROOM(ch))
      continue;
    act("$n slips through the shadows.", FALSE, mob, 0, 0, TO_ROOM);
    char_from_room(mob);
    char_to_room(mob, IN_ROOM(ch));
    act("$n slips from the edges of the room.", FALSE, mob, 0, 0, TO_ROOM);
  }
  act("You call your shadows back to your side!", FALSE, ch, 0, 0, TO_CHAR);
  act("Dark shapes slip from the edges of the room to gather around $n!", FALSE, ch, 0, 0, TO_ROOM);
}

ASPELL(spell_shadow_regenesis) { if (!ch) return; spell_apply_flag(ch, SPELL_SHADOW_REGENESIS, spell_dur_long_manual(level), AFF_REGENERATING); spell_apply_modifier(ch, SPELL_SHADOW_REGENESIS, spell_dur_long_manual(level), APPLY_NONE, 4 + (level / 4)); act("Your body sinks into shadow and begins to regenerate!", FALSE, ch, 0, 0, TO_CHAR); act("$n's wounds seem to close in shadow.", FALSE, ch, 0, 0, TO_ROOM); }
ASPELL(spell_assassins_intent) { if (!ch) return; spell_apply_modifier(ch, SPELL_ASSASSINS_INTENT, spell_dur_medium_manual(level), APPLY_HITROLL, 5); spell_apply_modifier(ch, SPELL_ASSASSINS_INTENT, spell_dur_medium_manual(level), APPLY_DAMROLL, 3); act("A killing stillness settles into your movements.", FALSE, ch, 0, 0, TO_CHAR); act("$n grows unnervingly still and lethal.", FALSE, ch, 0, 0, TO_ROOM); }
ASPELL(spell_blood_dagger_tempest) { struct char_data *tch,*next_tch; if (!ch) return; act("You unleash a tempest of blood-dark daggers across the room!", FALSE, ch, 0, 0, TO_CHAR); act("$n unleashes a tempest of blood-dark daggers!", FALSE, ch, 0, 0, TO_ROOM); for (tch = world[IN_ROOM(ch)].people; tch; tch = next_tch) { int j; next_tch = tch->next_in_room; if (!spell_is_enemy(ch, tch, SPELL_BLOOD_DAGGER_TEMPEST)) continue; for (j = 0; j < 2; j++) { int dam = spell_dmg_low_manual(level); if (mag_savingthrow(tch, SAVING_SPELL, 0)) dam /= 2; set_next_damage_type(DAM_SHADOW); if (damage(ch, tch, dam, SPELL_BLOOD_DAGGER_TEMPEST) == -1) break; } } GET_HIT(ch) = MAX(1, GET_HIT(ch) - level); }
ASPELL(spell_chain_of_subjugation) { if (!ch || !victim) return; if (!mag_savingthrow(victim, SAVING_SPELL, 0)) { spell_apply_flag(victim, SPELL_CHAIN_OF_SUBJUGATION, spell_dur_medium_manual(level), AFF_ROOTED); spell_apply_modifier(victim, SPELL_CHAIN_OF_SUBJUGATION, spell_dur_medium_manual(level), APPLY_HITROLL, -4); spell_apply_modifier(victim, SPELL_CHAIN_OF_SUBJUGATION, spell_dur_medium_manual(level), APPLY_DAMROLL, -4); } else spell_apply_flag(victim, SPELL_CHAIN_OF_SUBJUGATION, 1, AFF_ROOTED); act("Chains of shadow clamp down and subjugate $N!", FALSE, ch, 0, victim, TO_CHAR); act("Chains of shadow bind your body and crush your will!", FALSE, ch, 0, victim, TO_VICT); act("Chains of shadow erupt around $N at $n's command!", FALSE, ch, 0, victim, TO_NOTVICT); }
ASPELL(spell_sovereigns_step) { if (!ch) return; spell_apply_modifier(ch, SPELL_SOVEREIGNS_STEP, spell_dur_medium_manual(level), APPLY_NONE, 10); act("You move with the effortless superiority of a sovereign.", FALSE, ch, 0, 0, TO_CHAR); act("$n begins moving with impossible precision.", FALSE, ch, 0, 0, TO_ROOM); }

ASPELL(spell_kings_command)
{
  struct follow_type *f;
  if (!ch) return;
  for (f = ch->followers; f; f = f->next) {
    struct char_data *mob = f->follower;
    if (!mob || IN_ROOM(mob) != IN_ROOM(ch) || mob->master != ch || !AFF_FLAGGED(mob, AFF_CHARM))
      continue;
    spell_apply_modifier(mob, SPELL_KINGS_COMMAND, spell_dur_short_manual(level), APPLY_HITROLL, 4);
    spell_apply_modifier(mob, SPELL_KINGS_COMMAND, spell_dur_short_manual(level), APPLY_DAMROLL, 4);
    remove_flagged_affects(mob, AFF_FEARFUL);
  }
  act("Your command crashes out like a king's decree!", FALSE, ch, 0, 0, TO_CHAR);
  act("$n's summons stiffen with deadly discipline under a king's command!", FALSE, ch, 0, 0, TO_ROOM);
}

ASPELL(spell_detect_kill_intent) { if (!ch) return; spell_apply_flag(ch, SPELL_DETECT_KILL_INTENT, spell_dur_medium_manual(level), AFF_TRUESIGHT); spell_apply_modifier(ch, SPELL_DETECT_KILL_INTENT, spell_dur_medium_manual(level), APPLY_SAVING_SPELL, -4); spell_apply_modifier(ch, SPELL_DETECT_KILL_INTENT, spell_dur_medium_manual(level), APPLY_HITROLL, 4); act("Your senses sharpen to detect even killing intent.", FALSE, ch, 0, 0, TO_CHAR); act("$n's awareness sharpens unnaturally.", FALSE, ch, 0, 0, TO_ROOM); }
ASPELL(spell_mutilate) { if (!ch || !victim) return; { int s = mag_savingthrow(victim, SAVING_SPELL, 0), dam = spell_dmg_high_manual(level); if (s) dam /= 2; set_next_damage_type(DAM_SHADOW); if (damage(ch, victim, dam, SPELL_MUTILATE) != -1 && !s) { spell_apply_modifier(victim, SPELL_MUTILATE, spell_dur_short_manual(level), APPLY_STR, -2); spell_apply_modifier(victim, SPELL_MUTILATE, spell_dur_short_manual(level), APPLY_DEX, -2);} } act("You mutilate $N with a vicious shadow-infused strike!", FALSE, ch, 0, victim, TO_CHAR); act("A vicious shadow-infused strike tears your body apart!", FALSE, ch, 0, victim, TO_VICT); act("$n mutilates $N with a vicious shadow-infused strike!", FALSE, ch, 0, victim, TO_NOTVICT); }
ASPELL(spell_shadow_armor) { if (!ch || !victim) victim = ch; if (!victim) return; spell_apply_modifier(victim, SPELL_SHADOW_ARMOR, spell_dur_long_manual(level), APPLY_AC, -25); spell_apply_modifier(victim, SPELL_SHADOW_ARMOR, spell_dur_long_manual(level), APPLY_NONE, 1); act("Living shadow forms a suit of armor around $N.", FALSE, ch, 0, victim, TO_CHAR); act("Living shadow hardens around you as armor.", FALSE, ch, 0, victim, TO_VICT); act("Living shadow hardens around $N as armor.", FALSE, ch, 0, victim, TO_NOTVICT); }
ASPELL(spell_total_occultation) { if (!ch) return; spell_apply_flag(ch, SPELL_TOTAL_OCCULTATION, spell_dur_short_manual(level), AFF_INVISIBLE); spell_apply_flag(ch, SPELL_TOTAL_OCCULTATION, spell_dur_short_manual(level), AFF_HIDE); spell_apply_modifier(ch, SPELL_TOTAL_OCCULTATION, spell_dur_short_manual(level), APPLY_NONE, 1); act("You vanish completely into total occultation.", FALSE, ch, 0, 0, TO_CHAR); act("$n disappears into complete occultation.", FALSE, ch, 0, 0, TO_ROOM); }

ASPELL(spell_domain_break)
{
  struct room_effect_data *eff, *prev = NULL;
  int priorities[] = { ROOM_EFFECT_SHADOW_DOMAIN, ROOM_EFFECT_NULL_FIELD, ROOM_EFFECT_SILENCE_FIELD, ROOM_EFFECT_TOXIC_CLOUD, ROOM_EFFECT_MIASMA, ROOM_EFFECT_WALL_OF_FIRE, ROOM_EFFECT_STATIC_FIELD };
  int i;
  if (!ch || IN_ROOM(ch) == NOWHERE)
    return;
  for (i = 0; i < (int)(sizeof(priorities)/sizeof(priorities[0])); i++) {
    prev = NULL;
    for (eff = world[IN_ROOM(ch)].effects; eff; prev = eff, eff = eff->next) {
      if (eff->effect_type != priorities[i])
        continue;
      if (prev) prev->next = eff->next;
      else world[IN_ROOM(ch)].effects = eff->next;
      free(eff);
      act("You tear apart the dominant field in the room!", FALSE, ch, 0, 0, TO_CHAR);
      act("$n tears apart the magical field saturating the room!", FALSE, ch, 0, 0, TO_ROOM);
      return;
    }
  }
  send_to_char(ch, "There is no hostile room field to break here.\r\n");
}

ASPELL(spell_hunters_instinct) { if (!ch) return; spell_apply_modifier(ch, SPELL_HUNTERS_INSTINCT, spell_dur_medium_manual(level), APPLY_HITROLL, 3); spell_apply_modifier(ch, SPELL_HUNTERS_INSTINCT, spell_dur_medium_manual(level), APPLY_SAVING_SPELL, -3); act("Your hunter's instinct surges to the surface.", FALSE, ch, 0, 0, TO_CHAR); act("$n's eyes sharpen with predatory instinct.", FALSE, ch, 0, 0, TO_ROOM); }
