/**************************************************************************
*  File: act.other.c                                       Part of tbaMUD *
*  Usage: Miscellaneous player-level commands.                             *
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
#include "interpreter.h"
#include "handler.h"
#include "db.h"
#include "spells.h"
#include "screen.h"
#include "house.h"
#include "constants.h"
#include "dg_scripts.h"
#include "act.h"
#include "ai_actor.h"
#include "spec_procs.h"
#include "class.h"
#include "classtrack.h"
#include "fight.h"
#include "mail.h"  /* for has_mail() */
#include "shop.h"
#include "quest.h"
#include "criticalhits.h"

#define GLORY_PRACTICE_COST 250
#define GLORY_TRAIN_COST 600
#include "modify.h"
#include "pfdefaults.h"

/* ABILITY LIST FORMATTER v4 */
#ifndef ABIL_COL_WIDTH
#define ABIL_COL_WIDTH 27
#endif
#define LEVEL_LABEL_PADDING 10
#define SPELL_ROW_GAP 2
#define SPELL_TERM_WIDTH 78
#define SPELL_LEFT_MIN_WIDTH 24
#define SPELL_LEFT_MAX_WIDTH 40
#define SPELL_LEFT_COL_WIDTH 36

struct abil_row {
  int id;
  int lvl;
  const char *name;
  int pct;
};

static int abil_row_cmp(const void *va, const void *vb)
{
  const struct abil_row *a = (const struct abil_row *)va;
  const struct abil_row *b = (const struct abil_row *)vb;

  if (a->lvl != b->lvl)
    return (a->lvl - b->lvl);

#ifdef HAVE_STRCASECMP
  return strcasecmp(a->name, b->name);
#else
  return strcmp(a->name, b->name);
#endif
}

static int is_spell_id(int i)
{
#ifdef IS_SPELL
  return IS_SPELL(i);
#else
  /* Fallback: treat everything as a spell unless it is explicitly a skill. */
#ifdef IS_SKILL
  return !IS_SKILL(i);
#else
  return 1;
#endif
#endif
}

static int is_skill_id(int i)
{
#ifdef IS_SKILL
  return IS_SKILL(i);
#else
#ifdef IS_SPELL
  return !IS_SPELL(i);
#else
  return 0;
#endif
#endif
}
static int is_active_on_char(struct char_data *ch, int spellnum)
{
  struct affected_type *af;
  for (af = ch->affected; af; af = af->next) {
    if (af->spell == spellnum)
      return 1;
  }
  return 0;
}

static int contains_ci(const char *haystack, const char *needle)
{
  size_t nlen;

  if (!needle || !*needle)
    return TRUE;
  if (!haystack)
    return FALSE;

  nlen = strlen(needle);
  for (; *haystack; haystack++)
    if (!strncasecmp(haystack, needle, nlen))
      return TRUE;

  return FALSE;
}

static int ability_matches_damage_filter(const char *filter, const char *nm)
{
  static const char *damage_words[] = {
    "fire", "cold", "acid", "electric", "lightning", "poison", "disease",
    "shadow", "holy", "mental", "energy", "water", "air", "earth",
    "negative", "light", "magic", "sonic", NULL
  };
  int i;

  if (!str_cmp(filter, "damage"))
    filter = "";

  for (i = 0; damage_words[i]; i++) {
    if (*filter && str_cmp(filter, damage_words[i]))
      continue;
    if (contains_ci(nm, damage_words[i]))
      return TRUE;
  }

  return FALSE;
}

static int shadow_capacity(struct char_data *ch)
{
  int cap;
  if (!ch)
    return 0;
  cap = 2 + (GET_INT(ch) / 8) + (GET_WIS(ch) / 8);
  return MIN(MAX_SHADOW_ROSTER, MAX(2, cap));
}

static int sanitize_shadow_name_input(const char *input, char *out, size_t out_sz)
{
  const char *src;
  char *dst;
  size_t remaining;

  if (!input || !out || out_sz == 0)
    return FALSE;

  while (*input && isspace((unsigned char)*input))
    input++;
  if (!*input)
    return FALSE;

  src = input;
  dst = out;
  remaining = out_sz - 1;
  while (*src && remaining > 0) {
    unsigned char c = (unsigned char)*src;

    if (iscntrl(c))
      return FALSE;
    if (c == '%' || c == '&' || c == '{' || c == '}' || c == '`')
      return FALSE;

    *dst++ = *src++;
    remaining--;
  }
  *dst = '\0';

  while (dst > out && isspace((unsigned char)dst[-1])) {
    dst--;
    *dst = '\0';
  }

  return shadow_name_looks_valid(out);
}

static int shadow_find_slot(struct char_data *ch, const char *selector)
{
  int slot, i;
  if (!ch || !selector || !*selector)
    return -1;
  if (isdigit((unsigned char)*selector)) {
    slot = atoi(selector) - 1;
    if (slot >= 0 && slot < MAX_SHADOW_ROSTER)
      return slot;
  }
  for (i = 0; i < MAX_SHADOW_ROSTER; i++) {
    if (SHADOW_SLOT_OCCUPIED(ch, i) && isname(selector, SHADOW_SLOT_NAME(ch, i)))
      return i;
  }
  return -1;
}

static struct char_data *shadow_active_mob(struct char_data *ch, int slot)
{
  struct follow_type *f;
  struct affected_type *af;
  if (!ch || slot < 0)
    return NULL;
  for (f = ch->followers; f; f = f->next) {
    struct char_data *mob = f->follower;
    if (!mob || mob->master != ch)
      continue;
    for (af = mob->affected; af; af = af->next) {
      if (af->spell == SPELL_SHADOW_EXTRACTION && af->location == APPLY_NONE && af->modifier == slot + 2)
        return mob;
    }
  }
  return NULL;
}

static void remove_follower_link_silently(struct char_data *master, struct char_data *follower)
{
  struct follow_type *node, *prev = NULL;

  if (!master || !follower)
    return;

  for (node = master->followers; node; prev = node, node = node->next) {
    if (node->follower != follower)
      continue;
    if (prev)
      prev->next = node->next;
    else
      master->followers = node->next;
    free(node);
    break;
  }
}

static void shadow_prepare_for_removal(struct char_data *mob)
{
  struct char_data *fighter, *next_fighter;

  if (!mob)
    return;

  GET_SUMMON_TIMER(mob) = 0;
  HUNTING(mob) = NULL;

  if (FIGHTING(mob))
    stop_fighting(mob);

  for (fighter = combat_list; fighter; fighter = next_fighter) {
    next_fighter = fighter->next_fighting;
    if (FIGHTING(fighter) == mob)
      stop_fighting(fighter);
  }

  while (mob->followers) {
    struct follow_type *next = mob->followers->next;
    if (mob->followers->follower)
      mob->followers->follower->master = NULL;
    free(mob->followers);
    mob->followers = next;
  }

  if (mob->master) {
    remove_follower_link_silently(mob->master, mob);
    mob->master = NULL;
  }
  REMOVE_BIT_AR(AFF_FLAGS(mob), AFF_CHARM);
}

static void shadow_sync_active_flags(struct char_data *ch)
{
  int i;
  if (!ch)
    return;
  for (i = 0; i < MAX_SHADOW_ROSTER; i++) {
    if (!SHADOW_SLOT_OCCUPIED(ch, i)) {
      SHADOW_SLOT_ACTIVE(ch, i) = 0;
      continue;
    }
    if (SHADOW_SLOT_ACTIVE(ch, i) && !shadow_active_mob(ch, i))
      SHADOW_SLOT_ACTIVE(ch, i) = 0;
  }
}

static int ability_matches_filter(struct char_data *ch, int ability, const char *filter, int show_spells)
{
  const struct spell_info_type *si = &spell_info[ability];
  const char *nm = si->name ? si->name : "";

  if (!filter || !*filter)
    return 1;

  if (!str_cmp(filter, "affected"))
    return show_spells && is_active_on_char(ch, ability);
  if (!str_cmp(filter, "unaffected"))
    return show_spells && !is_active_on_char(ch, ability);
  if (!str_cmp(filter, "combat"))
    return si->violent || IS_SET(si->routines, MAG_DAMAGE);
  if (!str_cmp(filter, "healing") || !str_cmp(filter, "curative"))
    return IS_SET(si->routines, MAG_POINTS) || contains_ci(nm, "cure") || contains_ci(nm, "heal");
  if (!str_cmp(filter, "movement"))
    return contains_ci(nm, "fly") || contains_ci(nm, "recall") || contains_ci(nm, "portal") || contains_ci(nm, "haste");
  if (!str_cmp(filter, "area"))
    return IS_SET(si->routines, MAG_AREAS | MAG_MASSES);
  if (!str_cmp(filter, "spellup"))
    return !si->violent && IS_SET(si->routines, MAG_AFFECTS) && IS_SET(si->targets, TAR_SELF_ONLY);
  if (!str_cmp(filter, "resist"))
    return contains_ci(nm, "resist") || contains_ci(nm, "protection");
  if (!str_cmp(filter, "stats"))
    return contains_ci(nm, "strength") || contains_ci(nm, "dex") || contains_ci(nm, "con")
        || contains_ci(nm, "int") || contains_ci(nm, "wis") || contains_ci(nm, "charisma")
        || contains_ci(nm, "armor") || contains_ci(nm, "bless");
  if (!str_cmp(filter, "weapon"))
    return contains_ci(nm, "weapon") || contains_ci(nm, "shield") || contains_ci(nm, "enchant");
  if (!str_cmp(filter, "learned"))
    return GET_SKILL(ch, ability) > 0;
  if (!str_cmp(filter, "object"))
    return IS_SET(si->targets, TAR_OBJ_ROOM | TAR_OBJ_INV | TAR_OBJ_WORLD | TAR_OBJ_EQUIP) || IS_SET(si->routines, MAG_ALTER_OBJS);
  if (!str_cmp(filter, "attack") || !str_cmp(filter, "bad"))
    return si->violent;
  if (!str_cmp(filter, "damage") || !str_cmp(filter, "fire") || !str_cmp(filter, "cold") ||
      !str_cmp(filter, "acid") || !str_cmp(filter, "electric") || !str_cmp(filter, "poison") ||
      !str_cmp(filter, "disease") || !str_cmp(filter, "holy") || !str_cmp(filter, "shadow") ||
      !str_cmp(filter, "mental") || !str_cmp(filter, "energy") || !str_cmp(filter, "water") ||
      !str_cmp(filter, "air") || !str_cmp(filter, "earth") || !str_cmp(filter, "negative") ||
      !str_cmp(filter, "light") || !str_cmp(filter, "magic") || !str_cmp(filter, "sonic"))
    return ability_matches_damage_filter(filter, nm);

  return contains_ci(nm, filter);
}

void show_ability_table_aligned(struct char_data *ch, int show_spells, int show_all, const char *filter)
{
  int i;
  int cls = GET_CLASS(ch);
  int col = 0;
  int last_lvl = -1;
  int col_width = ABIL_COL_WIDTH;
  int name_width = 17;

  /* "Level 99: " is 10 chars; padding below matches practice output. */

  struct abil_row rows[TOP_SPELL_DEFINE + 1];
  int n = 0;

  send_to_char(ch, "%s:\r\n", show_spells ? "SPELLS" : "SKILLS");

  /* Collect rows */
  for (i = 1; i <= TOP_SPELL_DEFINE; i++) {
    int lvl;
    int pct;
    const char *nm;

    if (show_spells) {
      if (!is_spell_id(i)) continue;
    } else {
      if (!is_skill_id(i)) continue;
    }

    lvl = spell_info[i].min_level[cls];
    if (i == SKILL_STUDY && (lvl <= 0 || lvl >= LVL_IMMORT))
      lvl = 1;
    lvl = classtrack_get_study_display_level(ch, i, lvl);
    if (lvl <= 0) continue;

    if (show_all && lvl >= LVL_IMMORT) continue;
    pct = GET_SKILL(ch, i);
    if (!show_all && pct <= 0) continue;
    if (pct <= 0) pct = -1;
    nm = spell_info[i].name;
    if (!nm || !*nm) continue;
    if (!ability_matches_filter(ch, i, filter, show_spells)) continue;

    /* filter placeholders */
    if (!strcmp(nm, "!UNUSED!")) continue;

    if (pct >= 0 && pct > 100) pct = 100;
    rows[n].id = i;
    rows[n].lvl = lvl;
    rows[n].name = nm;
    rows[n].pct = pct;
    n++;
  }

  if (n == 0) {
    send_to_char(ch, "None.\r\n");
    return;
  }

  qsort(rows, (size_t)n, sizeof(rows[0]), abil_row_cmp);

  if (show_spells) {
    int idx = 0;
    int global_left_name_len = 0;
    int global_right_name_len = 0;
    int fixed_left_cell_width;
    int fixed_right_cell_width;

    for (i = 0; i < n; i++) {
      int level = rows[i].lvl;
      int level_idx = 0;

      while (i + level_idx < n && rows[i + level_idx].lvl == level) {
        int name_len = (int)strlen(rows[i + level_idx].name);

        if ((level_idx % 2) == 0) {
          if (name_len > global_left_name_len)
            global_left_name_len = name_len;
        } else {
          if (name_len > global_right_name_len)
            global_right_name_len = name_len;
        }
        level_idx++;
      }
      i += (level_idx - 1);
    }
    fixed_left_cell_width = global_left_name_len + 7;   /* " [100%]" or " [ -- ]" */
    fixed_right_cell_width = global_right_name_len + 7; /* " [100%]" or " [ -- ]" */

    while (idx < n) {
      int level = rows[idx].lvl;
      int level_start = idx;
      int level_count = 0;
      int first_row_for_level = 1;

      while ((level_start + level_count) < n && rows[level_start + level_count].lvl == level) {
        level_count++;
      }

      i = level_start;
      while (i < level_start + level_count) {
        char left_cell[256];
        char right_cell[256];
        int has_right = (i + 1 < level_start + level_count);
        int left_name_len = (int)strlen(rows[i].name);
        int right_name_len = 0;
        int left_pad = MAX(0, global_left_name_len - left_name_len);
        int right_pad = 0;
        int pair_fits = FALSE;
        int avail_after_indent = SPELL_TERM_WIDTH - LEVEL_LABEL_PADDING;

        if (has_right) {
          right_name_len = (int)strlen(rows[i + 1].name);
          right_pad = MAX(0, global_right_name_len - right_name_len);
          if (fixed_left_cell_width + SPELL_ROW_GAP + fixed_right_cell_width <= avail_after_indent)
            pair_fits = TRUE;
        }

        if (rows[i].pct < 0)
          snprintf(left_cell, sizeof(left_cell), "%s%*s [ -- ]", rows[i].name, left_pad, "");
        else
          snprintf(left_cell, sizeof(left_cell), "%s%*s [%3d%%]", rows[i].name, left_pad, "", rows[i].pct);

        if (has_right) {
          if (rows[i + 1].pct < 0)
            snprintf(right_cell, sizeof(right_cell), "%s%*s [ -- ]", rows[i + 1].name, right_pad, "");
          else
            snprintf(right_cell, sizeof(right_cell), "%s%*s [%3d%%]", rows[i + 1].name, right_pad, "", rows[i + 1].pct);
        }

        if (first_row_for_level) {
          send_to_char(ch, "%sLevel %-2d%s:%s ",
                       CCCYN(ch, C_NRM),
                       level,
                       CCWHT(ch, C_NRM),
                       CCNRM(ch, C_NRM));
          first_row_for_level = 0;
        } else {
          send_to_char(ch, "\r\n%*s", LEVEL_LABEL_PADDING, "");
        }

        if (pair_fits) {
          send_to_char(ch, "%-*s%*s%s",
                       fixed_left_cell_width + count_color_chars(left_cell), left_cell,
                       SPELL_ROW_GAP, "", right_cell);
          i += 2;
        } else {
          send_to_char(ch, "%s", left_cell);
          i++;
        }
      }

      send_to_char(ch, "\r\n");
      idx = level_start + level_count;
    }
    return;
  }

  /* Print */
  for (i = 0; i < n; i++) {
    char cell[256];

    if (rows[i].lvl != last_lvl) {
      if (col != 0) {
        send_to_char(ch, "\r\n");
        col = 0;
      }
      send_to_char(ch, "%sLevel %-2d%s:%s ",
                   CCCYN(ch, C_NRM),
                   rows[i].lvl,
                   CCWHT(ch, C_NRM),
                   CCNRM(ch, C_NRM));
      last_lvl = rows[i].lvl;
    } else if (col == 0) {
      send_to_char(ch, "\r\n%*s", LEVEL_LABEL_PADDING, "");
    } else {
      send_to_char(ch, "  ");
    }

    if (rows[i].pct < 0)
      snprintf(cell, sizeof(cell), "%-*.*s [ -- ]",
               name_width, name_width, rows[i].name);
    else
      snprintf(cell, sizeof(cell), "%-*.*s [%3d%%]",
               name_width, name_width, rows[i].name, rows[i].pct);
    send_to_char(ch, "%-*s", col_width + count_color_chars(cell), cell);

    col++;
    if (col >= 2) {
      send_to_char(ch, "\r\n");
      col = 0;
    }
  }

  if (col != 0)
    send_to_char(ch, "\r\n");
}

static void show_adventurer_study_catalog(struct char_data *ch, int show_spells, const char *filter)
{
  send_to_char(ch,
               "As an Adventurer, your future abilities are not defined by a fixed class list.\r\n"
               "Use STUDY <ability> to pursue new techniques.\r\n");
  send_to_char(ch, "Known %s:\r\n", show_spells ? "spells" : "skills");
  show_ability_table_aligned(ch, show_spells, 0, filter);
}

static void show_ability_filter_help(struct char_data *ch, const char *cmd_name)
{
  send_to_char(ch,
    "Usage: %s [all] [filter]\r\n"
    "Filters: affected, unaffected, combat, healing, movement, area, spellup,\r\n"
    "         resist, stats, weapon, learned, object, attack, damage,\r\n"
    "         fire, cold, acid, electric, poison, disease, holy, shadow,\r\n"
    "         mental, energy, water, air, earth, negative, light, magic, sonic.\r\n",
    cmd_name);
}


/* Local defined utility functions */
/* do_group utility functions */
static void print_group(struct char_data *ch);
static void display_group_list(struct char_data * ch);

ACMD(do_quit)
{
  if (IS_NPC(ch) || !ch->desc)
    return;

  if (subcmd != SCMD_QUIT && GET_LEVEL(ch) < LVL_IMMORT)
    send_to_char(ch, "You have to type quit--no less, to quit!\r\n");
  else if (GET_POS(ch) == POS_FIGHTING)
    send_to_char(ch, "No way!  You're fighting for your life!\r\n");
  else if (GET_POS(ch) < POS_STUNNED) {
    send_to_char(ch, "You die before your time...\r\n");
    die(ch, NULL);
  } else {
    act("$n has left the game.", TRUE, ch, 0, 0, TO_ROOM);
    mudlog(NRM, MAX(LVL_IMMORT, GET_INVIS_LEV(ch)), TRUE, "%s has quit the game.", GET_NAME(ch));

    if (GET_QUEST_TIME(ch) != -1)
      quest_timeout(ch);

    send_to_char(ch, "Goodbye, friend.. Come back soon!\r\n");

    /* We used to check here for duping attempts, but we may as well do it right
     * in extract_char(), since there is no check if a player rents out and it
     * can leave them in an equally screwy situation. */

    if (CONFIG_FREE_RENT)
      Crash_rentsave(ch, 0);

    GET_LOADROOM(ch) = GET_ROOM_VNUM(IN_ROOM(ch));

    /* Stop snooping so you can't see passwords during deletion or change. */
    if (ch->desc->snoop_by) {
      write_to_output(ch->desc->snoop_by, "Your victim is no longer among us.\r\n");
      ch->desc->snoop_by->snooping = NULL;
      ch->desc->snoop_by = NULL;
    }

    extract_char(ch);		/* Char is saved before extracting. */
  }
}

ACMD(do_save)
{
  if (IS_NPC(ch) || !ch->desc)
    return;

  send_to_char(ch, "Saving %s.\r\n", GET_NAME(ch));
  save_char(ch);
  Crash_crashsave(ch);
  if (ROOM_FLAGGED(IN_ROOM(ch), ROOM_HOUSE_CRASH))
    House_crashsave(GET_ROOM_VNUM(IN_ROOM(ch)));
  GET_LOADROOM(ch) = GET_ROOM_VNUM(IN_ROOM(ch));
}

ACMD(do_recall)
{
  int recall_move_cost;
  int move_before_recall;

  if (IS_NPC(ch) || !GET_SKILL(ch, SKILL_RECALL)) {
    send_to_char(ch, "You have no idea how.\r\n");
    return;
  }

  if (GET_POS(ch) <= POS_STUNNED || GET_POS(ch) == POS_SLEEPING) {
    send_to_char(ch, "You are in no condition to recall.\r\n");
    return;
  }

  if (ZONE_FLAGGED(GET_ROOM_ZONE(IN_ROOM(ch)), ZONE_NOASTRAL)) {
    send_to_char(ch, "A bright flash prevents your recall from working!");
    return;
  }

  send_to_char(ch, "You focus and force yourself back to safety, collapsing from exhaustion.\r\n");
  act("$n vanishes in a blur of motion.", TRUE, ch, 0, 0, TO_ROOM);

  move_before_recall = GET_MOVE(ch);
  recall_move_cost = MAX(1, move_before_recall / 2);
  GET_MOVE(ch) = MAX(0, move_before_recall - recall_move_cost);
  WAIT_STATE(ch, PULSE_VIOLENCE * 2);

  char_from_room(ch);
  char_to_room(ch, r_mortal_start_room);
  act("$n appears in the middle of the room.", TRUE, ch, 0, 0, TO_ROOM);
  look_at_room(ch, 0);
  entry_memory_mtrigger(ch);
  greet_mtrigger(ch, -1);
  greet_memory_mtrigger(ch);
  handle_followers_after_owner_teleport_or_recall(ch);
  improve_ability_from_use(ch, SKILL_RECALL, TRUE);
}

/* Generic function for commands which are normally overridden by special
 * procedures - i.e., shop commands, mail commands, etc. */
ACMD(do_not_here)
{
  send_to_char(ch, "Sorry, but you cannot do that here!\r\n");
}

ACMD(do_sneak)
{
  struct affected_type af;
  byte percent;

  if (IS_NPC(ch) || !GET_SKILL(ch, SKILL_SNEAK)) {
    send_to_char(ch, "You have no idea how to do that.\r\n");
    return;
  }
  send_to_char(ch, "Okay, you'll try to move silently for a while.\r\n");
  if (AFF_FLAGGED(ch, AFF_SNEAK))
    affect_from_char(ch, SKILL_SNEAK);

  percent = rand_number(1, 101);	/* 101% is a complete failure */

  if (percent > GET_SKILL(ch, SKILL_SNEAK) + dex_app_skill[GET_DEX(ch)].sneak)
    return;

  new_affect(&af);
  af.spell = SKILL_SNEAK;
  af.duration = GET_LEVEL(ch);
  SET_BIT_AR(af.bitvector, AFF_SNEAK);
  affect_to_char(ch, &af);
}

ACMD(do_hide)
{
  byte percent;

  if (IS_NPC(ch) || !GET_SKILL(ch, SKILL_HIDE)) {
    send_to_char(ch, "You have no idea how to do that.\r\n");
    return;
  }

  send_to_char(ch, "You attempt to hide yourself.\r\n");

  if (AFF_FLAGGED(ch, AFF_HIDE))
    REMOVE_BIT_AR(AFF_FLAGS(ch), AFF_HIDE);

  percent = rand_number(1, 101);	/* 101% is a complete failure */

  if (percent > GET_SKILL(ch, SKILL_HIDE) + dex_app_skill[GET_DEX(ch)].hide)
    return;

  SET_BIT_AR(AFF_FLAGS(ch), AFF_HIDE);
}

ACMD(do_steal)
{
  struct char_data *vict;
  struct obj_data *obj;
  char vict_name[MAX_INPUT_LENGTH], obj_name[MAX_INPUT_LENGTH];
  int percent, gold, eq_pos, pcsteal = 0, ohoh = 0;

  if (IS_NPC(ch) || !GET_SKILL(ch, SKILL_STEAL)) {
    send_to_char(ch, "You have no idea how to do that.\r\n");
    return;
  }
  if (ROOM_FLAGGED(IN_ROOM(ch), ROOM_PEACEFUL)) {
    send_to_char(ch, "This room just has such a peaceful, easy feeling...\r\n");
    return;
  }

  two_arguments(argument, obj_name, vict_name);

  if (!(vict = get_char_vis(ch, vict_name, NULL, FIND_CHAR_ROOM))) {
    send_to_char(ch, "Steal what from who?\r\n");
    return;
  } else if (vict == ch) {
    send_to_char(ch, "Come on now, that's rather stupid!\r\n");
    return;
  }

  /* 101% is a complete failure */
  percent = rand_number(1, 101) - dex_app_skill[GET_DEX(ch)].p_pocket;

  if (GET_POS(vict) < POS_SLEEPING)
    percent = -1;		/* ALWAYS SUCCESS, unless heavy object. */

  if (!CONFIG_PT_ALLOWED && !IS_NPC(vict))
    pcsteal = 1;

  if (!AWAKE(vict))	/* Easier to steal from sleeping people. */
    percent -= 50;

  /* No stealing if not allowed. If it is no stealing from Imm's or Shopkeepers. */
  if (GET_LEVEL(vict) >= LVL_IMMORT || pcsteal || GET_MOB_SPEC(vict) == shop_keeper)
    percent = 101;		/* Failure */

  if (str_cmp(obj_name, "coins") && str_cmp(obj_name, "gold")) {

    if (!(obj = get_obj_in_list_vis(ch, obj_name, NULL, vict->carrying))) {

      for (eq_pos = 0; eq_pos < NUM_WEARS; eq_pos++)
	if (GET_EQ(vict, eq_pos) &&
	    (isname(obj_name, GET_EQ(vict, eq_pos)->name)) &&
	    CAN_SEE_OBJ(ch, GET_EQ(vict, eq_pos))) {
	  obj = GET_EQ(vict, eq_pos);
	  break;
	}
      if (!obj) {
	act("$E hasn't got that item.", FALSE, ch, 0, vict, TO_CHAR);
	return;
      } else {			/* It is equipment */
	if ((GET_POS(vict) > POS_STUNNED)) {
	  send_to_char(ch, "Steal the equipment now?  Impossible!\r\n");
	  return;
	} else {
          if (!give_otrigger(obj, vict, ch) ||
              !receive_mtrigger(ch, vict, obj) ) {
            send_to_char(ch, "Impossible!\r\n");
            return;
          }
	  act("You unequip $p and steal it.", FALSE, ch, obj, 0, TO_CHAR);
	  act("$n steals $p from $N.", FALSE, ch, obj, vict, TO_NOTVICT);
	  obj_to_char(unequip_char(vict, eq_pos), ch);
	}
      }
    } else {			/* obj found in inventory */

      percent += GET_OBJ_WEIGHT(obj);	/* Make heavy harder */

      if (percent > GET_SKILL(ch, SKILL_STEAL)) {
	ohoh = TRUE;
	send_to_char(ch, "Oops..\r\n");
	act("$n tried to steal something from you!", FALSE, ch, 0, vict, TO_VICT);
	act("$n tries to steal something from $N.", TRUE, ch, 0, vict, TO_NOTVICT);
      } else {			/* Steal the item */
	if (IS_CARRYING_N(ch) + 1 < CAN_CARRY_N(ch)) {
          if (!give_otrigger(obj, vict, ch) ||
              !receive_mtrigger(ch, vict, obj) ) {
            send_to_char(ch, "Impossible!\r\n");
            return;
          }
	  if (IS_CARRYING_W(ch) + GET_OBJ_WEIGHT(obj) < CAN_CARRY_W(ch)) {
	    obj_from_char(obj);
	    obj_to_char(obj, ch);
	    send_to_char(ch, "Got it!\r\n");
	  }
	} else
	  send_to_char(ch, "You cannot carry that much.\r\n");
      }
    }
  } else {			/* Steal some coins */
    if (AWAKE(vict) && (percent > GET_SKILL(ch, SKILL_STEAL))) {
      ohoh = TRUE;
      send_to_char(ch, "Oops..\r\n");
      act("You discover that $n has $s hands in your wallet.", FALSE, ch, 0, vict, TO_VICT);
      act("$n tries to steal gold from $N.", TRUE, ch, 0, vict, TO_NOTVICT);
    } else {
      /* Steal some gold coins */
      gold = (GET_GOLD(vict) * rand_number(1, 10)) / 100;
      gold = MIN(1782, gold);
      if (gold > 0) {
		increase_gold(ch, gold);
		decrease_gold(vict, gold);
        if (gold > 1)
	  send_to_char(ch, "Bingo!  You got %d gold coins.\r\n", gold);
	else
	  send_to_char(ch, "You manage to swipe a solitary gold coin.\r\n");
      } else {
	send_to_char(ch, "You couldn't get any gold...\r\n");
      }
    }
  }

  if (!IS_NPC(ch) && vict && vict != ch)
    ai_actor_record_room_crime(NULL, ch, MEM_STOLE);

  if (ohoh && IS_NPC(vict) && AWAKE(vict))
    hit(vict, ch, TYPE_UNDEFINED);
}
ACMD(do_skills)
{
  char arg[MAX_INPUT_LENGTH], filter[MAX_INPUT_LENGTH];
  int show_all = 0;
  *filter = '\0';

  half_chop(argument, arg, filter);
  if ((*arg && (!str_cmp(arg, "help") || !str_cmp(arg, "?"))) ||
      (*filter && (!str_cmp(filter, "help") || !str_cmp(filter, "?")))) {
    show_ability_filter_help(ch, "skills");
    return;
  }
  if (*arg && !str_cmp(arg, "all"))
    show_all = 1;
  else if (*arg && !*filter)
    strlcpy(filter, arg, sizeof(filter));

  send_to_char(ch, "You have %d practice sessions remaining.\r\n", GET_PRACTICES(ch));
  if (show_all && !GET_CLASS_LOCKED(ch)) {
    show_adventurer_study_catalog(ch, 0, filter);
    return;
  }
  if (show_all)
    send_to_char(ch, "Showing all skills your class can learn at any level.\r\n");
  show_ability_table_aligned(ch, 0, show_all, filter);
}
ACMD(do_spellbook)
{
  if (IS_NPC(ch))
    return;

  list_skills(ch);
  send_to_char(ch, "\r\n");
  list_spells(ch);
}


static int can_use_practice_trainer(struct char_data *ch)
{
  struct obj_data *obj;
  struct char_data *mob;
  int i;

  if (GET_ROOM_SPEC(IN_ROOM(ch)) == guild)
    return TRUE;

  for (i = 0; i < NUM_WEARS; i++)
    if (GET_EQ(ch, i) && GET_OBJ_SPEC(GET_EQ(ch, i)) == guild)
      return TRUE;

  for (obj = ch->carrying; obj; obj = obj->next_content)
    if (GET_OBJ_SPEC(obj) == guild)
      return TRUE;

  for (mob = world[IN_ROOM(ch)].people; mob; mob = mob->next_in_room)
    if (!MOB_FLAGGED(mob, MOB_NOTDEADYET) && GET_MOB_SPEC(mob) == guild)
      return TRUE;

  for (obj = world[IN_ROOM(ch)].contents; obj; obj = obj->next_content)
    if (GET_OBJ_SPEC(obj) == guild)
      return TRUE;

  return FALSE;
}

static int study_is_valid_ability_id(int id)
{
  return (id > 0 &&
          id <= TOP_SPELL_DEFINE &&
          id <= MAX_SKILLS &&
          spell_info[id].name &&
          *spell_info[id].name &&
          str_cmp(spell_info[id].name, "!UNUSED!"));
}

enum study_item_result {
  STUDY_ITEM_SUCCESS = 0,
  STUDY_ITEM_NOT_FOUND,
  STUDY_ITEM_INVALID_SOURCE,
  STUDY_ITEM_NO_VALID_SLOTS,
  STUDY_ITEM_NO_STUDYABLE_SPELLS,
  STUDY_ITEM_ALL_KNOWN,
  STUDY_ITEM_ALL_TOO_ADVANCED,
  STUDY_ITEM_ALL_BLOCKED_PATH,
  STUDY_ITEM_TARGET_NOT_PRESENT
};

struct study_item_debug_info {
  int known_count;
  int item_level_blocked_count;
  int path_blocked_count;
};

static int study_item_level_allows(struct char_data *ch, struct obj_data *study_obj)
{
  if (!ch || !study_obj)
    return FALSE;

  return (GET_LEVEL(ch) >= GET_OBJ_LEVEL(study_obj));
}

static void study_debug_imm(struct char_data *ch, const char *fmt, ...)
{
  va_list args;
  char buf[MAX_STRING_LENGTH];

  if (!ch || IS_NPC(ch) || GET_LEVEL(ch) < LVL_IMMORT)
    return;

  va_start(args, fmt);
  vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);

  send_to_char(ch, "[study debug] %s\r\n", buf);
}

static const char *study_item_result_name(enum study_item_result result)
{
  switch (result) {
    case STUDY_ITEM_SUCCESS: return "success";
    case STUDY_ITEM_NOT_FOUND: return "not-found";
    case STUDY_ITEM_INVALID_SOURCE: return "invalid-source";
    case STUDY_ITEM_NO_VALID_SLOTS: return "no-valid-slots";
    case STUDY_ITEM_NO_STUDYABLE_SPELLS: return "no-studyable-spells";
    case STUDY_ITEM_ALL_KNOWN: return "all-known";
    case STUDY_ITEM_ALL_TOO_ADVANCED: return "item-level-blocked";
    case STUDY_ITEM_ALL_BLOCKED_PATH: return "path-blocked";
    case STUDY_ITEM_TARGET_NOT_PRESENT: return "target-not-present";
    default: return "unknown";
  }
}

static int study_can_learn_ability(struct char_data *student, int id, char *why, size_t whylen)
{
  int required_level;
  int ability_archetype;

  if (!study_is_valid_ability_id(id)) {
    if (why && whylen)
      snprintf(why, whylen, "You do not recognize that technique.");
    return FALSE;
  }

  if (GET_SKILL(student, id) > 0) {
    if (why && whylen)
      snprintf(why, whylen, "You already know that.");
    return FALSE;
  }

  required_level = classtrack_get_study_min_level(id);
  if (required_level > 0 && GET_LEVEL(student) < required_level) {
    if (why && whylen)
      snprintf(why, whylen, "You are not experienced enough to study that yet.");
    return FALSE;
  }

  ability_archetype = classtrack_get_ability_archetype(id);
  if (ability_archetype >= 0 && ability_archetype < NUM_ARCHETYPES) {
    if (why && whylen)
      why[0] = '\0';
    if (!classtrack_can_study_archetype(student, ability_archetype, why, whylen)) {
      if (why && whylen && !*why)
        snprintf(why, whylen,
                 "You have gone too far down your current path to learn that kind of power.");
      return FALSE;
    }
  }

  return TRUE;
}

static int study_extract_item_spells(struct obj_data *obj, int *out, int out_cap)
{
  int spell_slots[3];
  int slot_count = 0;
  int c, i, j;

  if (!obj || !out || out_cap <= 0)
    return 0;

  switch (GET_OBJ_TYPE(obj)) {
    case ITEM_WAND:
    case ITEM_STAFF:
      spell_slots[slot_count++] = GET_OBJ_VAL(obj, 3);
      break;
    case ITEM_SCROLL:
    case ITEM_POTION:
      spell_slots[slot_count++] = GET_OBJ_VAL(obj, 1);
      spell_slots[slot_count++] = GET_OBJ_VAL(obj, 2);
      spell_slots[slot_count++] = GET_OBJ_VAL(obj, 3);
      break;
    default:
      return 0;
  }

  c = 0;
  for (i = 0; i < slot_count && c < out_cap; i++) {
    int sid = spell_slots[i];
    int duplicate = FALSE;

    if (sid <= 0 || sid > TOP_SPELL_DEFINE || sid > MAX_SPELLS)
      continue;
    if (!study_is_valid_ability_id(sid))
      continue;

    for (j = 0; j < c; j++) {
      if (out[j] == sid) {
        duplicate = TRUE;
        break;
      }
    }
    if (!duplicate)
      out[c++] = sid;
  }

  return c;
}

static int study_item_contains_spell(const int *spell_ids, int spell_count, int target_id)
{
  int i;

  if (!spell_ids || spell_count <= 0 || target_id <= 0)
    return FALSE;

  for (i = 0; i < spell_count; i++)
    if (spell_ids[i] == target_id)
      return TRUE;

  return FALSE;
}

static int study_is_numbered_item_token(const char *token)
{
  const char *dot;
  const char *p;

  if (!token || !*token)
    return FALSE;

  dot = strchr(token, '.');
  if (!dot || dot == token || *(dot + 1) == '\0')
    return FALSE;

  for (p = token; p < dot; p++)
    if (!isdigit(*p))
      return FALSE;

  return TRUE;
}

static struct obj_data *study_resolve_inventory_item_source(struct char_data *ch,
                                                            const char *item_text)
{
  char lookup[MAX_INPUT_LENGTH];

  if (!ch || !item_text || !*item_text)
    return NULL;

  strlcpy(lookup, item_text, sizeof(lookup));
  return get_obj_in_list_vis(ch, lookup, NULL, ch->carrying);
}

static int study_find_item_and_target(struct char_data *ch, char *argument,
                                      struct obj_data **study_obj_out,
                                      int *target_id_out,
                                      int *has_target_out,
                                      int *used_numbered_item_token_out)
{
  char work[MAX_INPUT_LENGTH];
  char item_name[MAX_INPUT_LENGTH] = "";
  char target_name[MAX_INPUT_LENGTH] = "";
  char *tokens[64];
  int token_count = 0;
  char *tok;
  int i, j;
  struct obj_data *obj = NULL;
  int first_token_numbered = FALSE;

  if (!ch || !study_obj_out || !target_id_out || !has_target_out ||
      !used_numbered_item_token_out || !argument || !*argument)
    return FALSE;

  *study_obj_out = NULL;
  *target_id_out = -1;
  *has_target_out = FALSE;
  *used_numbered_item_token_out = FALSE;

  strlcpy(work, argument, sizeof(work));
  tok = strtok(work, " ");
  while (tok && token_count < 64) {
    tokens[token_count++] = tok;
    tok = strtok(NULL, " ");
  }

  if (token_count <= 0)
    return FALSE;

  first_token_numbered = study_is_numbered_item_token(tokens[0]);
  *used_numbered_item_token_out = first_token_numbered;

  for (i = token_count; i >= 1; i--) {
    size_t item_len = 0;
    item_name[0] = '\0';
    for (j = 0; j < i; j++) {
      int wrote = snprintf(item_name + item_len, sizeof(item_name) - item_len,
                           "%s%s", item_len ? " " : "", tokens[j]);
      if (wrote < 0)
        break;
      if ((size_t)wrote >= sizeof(item_name) - item_len) {
        item_len = sizeof(item_name) - 1;
        item_name[item_len] = '\0';
        break;
      }
      item_len += (size_t)wrote;
    }

    obj = study_resolve_inventory_item_source(ch, item_name);
    if (!obj)
      continue;

    *study_obj_out = obj;
    if (i < token_count) {
      size_t target_len = 0;
      target_name[0] = '\0';
      for (j = i; j < token_count; j++) {
        int wrote = snprintf(target_name + target_len, sizeof(target_name) - target_len,
                             "%s%s", target_len ? " " : "", tokens[j]);
        if (wrote < 0)
          break;
        if ((size_t)wrote >= sizeof(target_name) - target_len) {
          target_len = sizeof(target_name) - 1;
          target_name[target_len] = '\0';
          break;
        }
        target_len += (size_t)wrote;
      }
      *target_id_out = find_skill_num(target_name);
      *has_target_out = TRUE;
    }
    return TRUE;
  }

  return FALSE;
}

static enum study_item_result study_try_from_item(struct char_data *ch, struct obj_data *study_obj,
                                                   int requested_ability_id,
                                                   int has_requested_ability,
                                                   int *ability_id_out)
{
  int i;
  int raw_spell_ids[3];
  int raw_spell_count = 0;
  int eligible_spell_ids[3];
  int eligible_count = 0;
  struct study_item_debug_info debug_info = {0, 0, 0};
  int item_level_gate_passed = FALSE;
  char raw_names[MAX_STRING_LENGTH] = "";
  char eligible_names[MAX_STRING_LENGTH] = "";

  if (!study_obj)
    return STUDY_ITEM_NOT_FOUND;

  if (GET_OBJ_TYPE(study_obj) != ITEM_WAND &&
      GET_OBJ_TYPE(study_obj) != ITEM_STAFF &&
      GET_OBJ_TYPE(study_obj) != ITEM_SCROLL &&
      GET_OBJ_TYPE(study_obj) != ITEM_POTION) {
    study_debug_imm(ch, "item '%s' type %d is not a study source",
                    study_obj->short_description, GET_OBJ_TYPE(study_obj));
    return STUDY_ITEM_INVALID_SOURCE;
  }

  raw_spell_count = study_extract_item_spells(study_obj, raw_spell_ids, 3);
  if (raw_spell_count <= 0) {
    study_debug_imm(ch, "item '%s' has no valid spell slots", study_obj->short_description);
    return STUDY_ITEM_NO_VALID_SLOTS;
  }

  if (has_requested_ability) {
    if (!study_is_valid_ability_id(requested_ability_id) ||
        !study_item_contains_spell(raw_spell_ids, raw_spell_count, requested_ability_id)) {
      study_debug_imm(ch, "item '%s' does not contain requested ability id=%d",
                      study_obj->short_description, requested_ability_id);
      return STUDY_ITEM_TARGET_NOT_PRESENT;
    }
  }

  item_level_gate_passed = study_item_level_allows(ch, study_obj);

  for (i = 0; i < raw_spell_count; i++) {
    int sid = raw_spell_ids[i];
    int ability_archetype = classtrack_get_ability_archetype(sid);
    size_t raw_len = strlen(raw_names);

    if (has_requested_ability && sid != requested_ability_id)
      continue;

    {
      int wrote = snprintf(raw_names + raw_len, sizeof(raw_names) - raw_len, "%s%s",
                           raw_len ? ", " : "", spell_info[sid].name);
      if (wrote >= (int)(sizeof(raw_names) - raw_len))
        raw_names[sizeof(raw_names) - 1] = '\0';
    }

    if (GET_SKILL(ch, sid) > 0) {
      debug_info.known_count++;
      study_debug_imm(ch, "item-study spell=%s item_level=%d player_level=%d item_gate=pass path_gate=skip result=known",
                      spell_info[sid].name, GET_OBJ_LEVEL(study_obj), GET_LEVEL(ch));
      continue;
    }

    if (!item_level_gate_passed) {
      debug_info.item_level_blocked_count++;
      study_debug_imm(ch, "item-study spell=%s item_level=%d player_level=%d item_gate=fail path_gate=skip result=item-level-blocked",
                      spell_info[sid].name, GET_OBJ_LEVEL(study_obj), GET_LEVEL(ch));
      continue;
    }

    if (ability_archetype >= 0 && ability_archetype < NUM_ARCHETYPES &&
        !classtrack_can_study_archetype(ch, ability_archetype, NULL, 0)) {
      debug_info.path_blocked_count++;
      study_debug_imm(ch, "item-study spell=%s item_level=%d player_level=%d item_gate=pass path_gate=fail result=path-blocked",
                      spell_info[sid].name, GET_OBJ_LEVEL(study_obj), GET_LEVEL(ch));
      continue;
    }

    eligible_spell_ids[eligible_count++] = sid;
    study_debug_imm(ch, "item-study spell=%s item_level=%d player_level=%d item_gate=pass path_gate=pass result=eligible",
                    spell_info[sid].name, GET_OBJ_LEVEL(study_obj), GET_LEVEL(ch));
    {
      size_t eligible_len = strlen(eligible_names);
      int wrote = snprintf(eligible_names + eligible_len,
                           sizeof(eligible_names) - eligible_len, "%s%s",
                           eligible_len ? ", " : "", spell_info[sid].name);
      if (wrote >= (int)(sizeof(eligible_names) - eligible_len))
        eligible_names[sizeof(eligible_names) - 1] = '\0';
    }
  }

  study_debug_imm(ch, "item '%s' vnum=%d type %d target=%s candidates=[%s] eligible=[%s]",
                  study_obj->short_description, GET_OBJ_VNUM(study_obj),
                  GET_OBJ_TYPE(study_obj),
                  (has_requested_ability && study_is_valid_ability_id(requested_ability_id)) ?
                    spell_info[requested_ability_id].name : "auto",
                  *raw_names ? raw_names : "none",
                  *eligible_names ? eligible_names : "none");

  if (eligible_count > 0) {
    int chosen = (eligible_count == 1) ? 0 : rand_number(0, eligible_count - 1);
    *ability_id_out = eligible_spell_ids[chosen];
    return STUDY_ITEM_SUCCESS;
  }

  if (debug_info.known_count > 0 &&
      debug_info.item_level_blocked_count == 0 &&
      debug_info.path_blocked_count == 0)
    return STUDY_ITEM_ALL_KNOWN;

  if (debug_info.item_level_blocked_count > 0 &&
      debug_info.known_count == 0 &&
      debug_info.path_blocked_count == 0)
    return STUDY_ITEM_ALL_TOO_ADVANCED;

  if (debug_info.path_blocked_count > 0 &&
      debug_info.known_count == 0 &&
      debug_info.item_level_blocked_count == 0)
    return STUDY_ITEM_ALL_BLOCKED_PATH;

  if (debug_info.known_count == raw_spell_count)
    return STUDY_ITEM_ALL_KNOWN;

  if (debug_info.item_level_blocked_count == raw_spell_count)
    return STUDY_ITEM_ALL_TOO_ADVANCED;

  if (debug_info.path_blocked_count == raw_spell_count)
    return STUDY_ITEM_ALL_BLOCKED_PATH;

  return STUDY_ITEM_NO_STUDYABLE_SPELLS;
}

static int study_is_on_cooldown(struct char_data *ch, time_t now)
{
  time_t cooldown_until = GET_STUDY_COOLDOWN_UNTIL(ch);

  return (cooldown_until > now);
}

static int study_success_chance(struct char_data *ch, int source_level, int source_complexity)
{
  int study_pct;
  int level_delta;
  int chance;

  if (!ch)
    return 0;

  study_pct = GET_SKILL(ch, SKILL_STUDY);
  if (study_pct < 0)
    study_pct = 0;
  if (study_pct > 100)
    study_pct = 100;

  if (source_level < 1)
    source_level = 1;
  if (source_complexity < 1)
    source_complexity = 1;

  level_delta = GET_LEVEL(ch) - source_level;

  /* Keep low Study mastery difficult while still allowing easy sources. */
  chance = 5 + (study_pct * 3) / 4;
  chance += URANGE(-20, level_delta * 4, 20);
  if (source_level <= 5)
    chance += 10;
  else if (source_level <= 10)
    chance += 5;

  /* Simpler sources (single-technique study) are slightly easier. */
  chance += (source_complexity == 1) ? 5 : 0;

  return URANGE(3, chance, 95);
}

static int study_attempt_succeeds(struct char_data *ch, int source_level, int source_complexity)
{
  int chance = study_success_chance(ch, source_level, source_complexity);
  return (rand_number(1, 100) <= chance);
}

static void study_apply_cooldown(struct char_data *ch, time_t now, int cooldown_secs)
{
  GET_STUDY_COOLDOWN_UNTIL(ch) = now + cooldown_secs;
}

ACMD(do_study)
{
  struct obj_data *study_obj = NULL;
  char reason[MAX_INPUT_LENGTH];
  char argument_copy[MAX_INPUT_LENGTH];
  int ability_id;
  int requested_ability_id = -1;
  int has_requested_ability = FALSE;
  int used_numbered_item_token = FALSE;
  int parsed_item = FALSE;
  enum study_item_result item_result;
  const int study_cooldown_secs = 2;
  time_t now = time(NULL);
  int source_level = 1;
  int source_complexity = 1;

  skip_spaces(&argument);

  if (IS_NPC(ch))
    return;

  if (!*argument) {
    send_to_char(ch, "Usage: study <spell, skill, or magical item>\r\n");
    return;
  }

  if (study_is_on_cooldown(ch, now)) {
    send_to_char(ch, "You need a moment to gather your thoughts before studying again.\r\n");
    return;
  }

  if (GET_SKILL(ch, SKILL_STUDY) <= 0) {
    send_to_char(ch, "You do not yet understand how to study new techniques.\r\n");
    return;
  }

  ability_id = find_skill_num(argument);
  if (study_is_valid_ability_id(ability_id)) {
    if (!study_can_learn_ability(ch, ability_id, reason, sizeof(reason))) {
      send_to_char(ch, "%s\r\n", *reason ? reason : "You cannot study that right now.");
      study_apply_cooldown(ch, now, study_cooldown_secs);
      return;
    }
    source_level = classtrack_get_study_min_level(ability_id);
    if (source_level < 1)
      source_level = 1;
    if (!study_attempt_succeeds(ch, source_level, 1)) {
      send_to_char(ch, "You misread the pattern and learn nothing.\r\n");
      study_apply_cooldown(ch, now, study_cooldown_secs);
      return;
    }

    SET_SKILL(ch, ability_id, 1);
    classtrack_record_study_learn_level(ch, ability_id, GET_LEVEL(ch));
    send_to_char(ch, "You study the knowledge of %s and begin to understand it.\r\n",
                 spell_info[ability_id].name);
    study_apply_cooldown(ch, now, study_cooldown_secs);
    return;
  }

  strlcpy(argument_copy, argument, sizeof(argument_copy));
  parsed_item = study_find_item_and_target(ch, argument_copy, &study_obj,
                                           &requested_ability_id, &has_requested_ability,
                                           &used_numbered_item_token);
  if (!parsed_item || !study_obj) {
    if (used_numbered_item_token)
      send_to_char(ch, "You do not have that item to study.\r\n");
    else
      send_to_char(ch, "You do not recognize that technique.\r\n");
    return;
  }

  item_result = study_try_from_item(ch, study_obj, requested_ability_id,
                                    has_requested_ability, &ability_id);
  if (item_result != STUDY_ITEM_SUCCESS) {
    switch (item_result) {
      case STUDY_ITEM_INVALID_SOURCE:
      case STUDY_ITEM_NO_VALID_SLOTS:
        send_to_char(ch, "You study %s, but there is no technique within it for you to study.\r\n",
                     study_obj->short_description);
        break;
      case STUDY_ITEM_TARGET_NOT_PRESENT:
        send_to_char(ch, "You study %s, but that item does not contain that technique.\r\n",
                     study_obj->short_description);
        break;
      case STUDY_ITEM_ALL_KNOWN:
        send_to_char(ch, "You study %s, but you already know all of the knowledge bound within it.\r\n",
                     study_obj->short_description);
        break;
      case STUDY_ITEM_ALL_TOO_ADVANCED:
        send_to_char(ch, "You study %s, but it is beyond your current ability to study.\r\n",
                     study_obj->short_description);
        break;
      case STUDY_ITEM_ALL_BLOCKED_PATH:
        send_to_char(ch, "You study %s, but your current path rejects the knowledge bound within it.\r\n",
                     study_obj->short_description);
        break;
      case STUDY_ITEM_NO_STUDYABLE_SPELLS:
      default:
        send_to_char(ch, "You study %s, but cannot yet make use of the knowledge bound within it.\r\n",
                     study_obj->short_description);
        break;
    }
    study_debug_imm(ch,
                    "item-study final=%s item='%s' vnum=%d item_level=%d player_level=%d target=%s learned=none",
                    study_item_result_name(item_result),
                    study_obj->short_description,
                    GET_OBJ_VNUM(study_obj),
                    GET_OBJ_LEVEL(study_obj),
                    GET_LEVEL(ch),
                    (has_requested_ability && study_is_valid_ability_id(requested_ability_id)) ?
                      spell_info[requested_ability_id].name : "auto");
    if (item_result != STUDY_ITEM_INVALID_SOURCE &&
        item_result != STUDY_ITEM_NO_VALID_SLOTS &&
        item_result != STUDY_ITEM_TARGET_NOT_PRESENT)
      study_apply_cooldown(ch, now, study_cooldown_secs);
    return;
  }

  source_level = GET_OBJ_LEVEL(study_obj);
  source_complexity = 0;
  if (GET_OBJ_TYPE(study_obj) == ITEM_SCROLL ||
      GET_OBJ_TYPE(study_obj) == ITEM_WAND ||
      GET_OBJ_TYPE(study_obj) == ITEM_STAFF ||
      GET_OBJ_TYPE(study_obj) == ITEM_POTION) {
    int i;
    for (i = 1; i <= 3; i++) {
      int sid = GET_OBJ_VAL(study_obj, i);
      if (study_is_valid_ability_id(sid))
        source_complexity++;
    }
  }
  if (!study_attempt_succeeds(ch, source_level, source_complexity)) {
    send_to_char(ch, "You fail to properly grasp the knowledge within it.\r\n");
    study_debug_imm(ch,
                    "item-study final=attempt-failed item='%s' vnum=%d item_level=%d player_level=%d stored_spell=%s learned=none",
                    study_obj->short_description,
                    GET_OBJ_VNUM(study_obj),
                    GET_OBJ_LEVEL(study_obj),
                    GET_LEVEL(ch),
                    spell_info[ability_id].name);
    study_apply_cooldown(ch, now, study_cooldown_secs);
    return;
  }

  SET_SKILL(ch, ability_id, 1);
  classtrack_record_study_learn_level(ch, ability_id, GET_LEVEL(ch));
  player_record_identified_item(ch, study_obj);
  if (has_requested_ability)
    send_to_char(ch, "You focus on %s within %s and begin to understand it.\r\n",
                 spell_info[ability_id].name, study_obj->short_description);
  else
    send_to_char(ch, "You study %s and begin to understand %s.\r\n",
                 study_obj->short_description, spell_info[ability_id].name);
  study_debug_imm(ch,
                  "item-study final=%s item='%s' vnum=%d item_level=%d player_level=%d stored_spell=%s learned=%s",
                  study_item_result_name(STUDY_ITEM_SUCCESS),
                  study_obj->short_description,
                  GET_OBJ_VNUM(study_obj),
                  GET_OBJ_LEVEL(study_obj),
                  GET_LEVEL(ch),
                  spell_info[ability_id].name,
                  spell_info[ability_id].name);
  study_apply_cooldown(ch, now, study_cooldown_secs);
}


ACMD(do_practice)
{
  char arg[MAX_INPUT_LENGTH];

  if (IS_NPC(ch))
    return;

  one_argument(argument, arg);

  if (*arg) {
    if (!can_use_practice_trainer(ch))
      send_to_char(ch, "You can only practice skills in your guild.\r\n");
    return;
  }

  list_known_abilities(ch);
}

ACMD(do_buypractice)
{
  if (IS_NPC(ch))
    return;

  if (!can_use_practice_trainer(ch)) {
    send_to_char(ch, "You must be at your class trainer to buy practice sessions.\r\n");
    return;
  }

  if (GET_GLORY(ch) < GLORY_PRACTICE_COST) {
    send_to_char(ch, "A practice session costs %d Glory. You currently have %d Glory.\r\n",
                 GLORY_PRACTICE_COST, GET_GLORY(ch));
    return;
  }

  GET_GLORY(ch) -= GLORY_PRACTICE_COST;
  GET_PRACTICES(ch) += 1;
  send_to_char(ch, "You spend %d Glory to buy a practice session. Practices: %d. Glory left: %d.\r\n",
               GLORY_PRACTICE_COST, GET_PRACTICES(ch), GET_GLORY(ch));
}

ACMD(do_train)
{
  char arg[MAX_INPUT_LENGTH];
  sbyte *stat_field = NULL;
  const char *stat_name = NULL;
  const char *stat_label = NULL;
  int old_trains, new_trains;
  int old_val, new_val;
  int old_carry, new_carry;
  int old_evasion, new_evasion;
  int old_melee_crit, new_melee_crit;
  int old_spell_crit, new_spell_crit;
  int old_heal_crit, new_heal_crit;
  int old_max_hit, new_max_hit;
  int old_max_mana, new_max_mana;
  int old_max_move, new_max_move;
  const int stat_cap = 20;

  if (IS_NPC(ch))
    return;

  one_argument(argument, arg);

  if (!can_use_practice_trainer(ch)) {
    send_to_char(ch, "You can only train at your guild.\r\n");
    return;
  }

  if (!*arg) {
    send_to_char(ch, "You have %d training sessions available.\r\n\r\n", GET_TRAINS(ch));
    send_to_char(ch, "Base stats\r\n");
    send_to_char(ch, "  Str %d/%d%s   Dex %d/%d%s   Con %d/%d%s\r\n",
                 ch->real_abils.str, stat_cap, ch->real_abils.str >= stat_cap ? " [MAX]" : "",
                 ch->real_abils.dex, stat_cap, ch->real_abils.dex >= stat_cap ? " [MAX]" : "",
                 ch->real_abils.con, stat_cap, ch->real_abils.con >= stat_cap ? " [MAX]" : "");
    send_to_char(ch, "  Int %d/%d%s   Wis %d/%d%s   Cha %d/%d%s\r\n\r\n",
                 ch->real_abils.intel, stat_cap, ch->real_abils.intel >= stat_cap ? " [MAX]" : "",
                 ch->real_abils.wis, stat_cap, ch->real_abils.wis >= stat_cap ? " [MAX]" : "",
                 ch->real_abils.cha, stat_cap, ch->real_abils.cha >= stat_cap ? " [MAX]" : "");
    send_to_char(ch, "Train str dex con int wis cha (cost 1, cap %d)\r\n", stat_cap);
    send_to_char(ch, "Train hit mana move (cost 10)\r\n");
    return;
  }

  if (!str_cmp(arg, "hit")) {
    if (GET_TRAINS(ch) < 10) {
      send_to_char(ch, "Training hit requires 10 training sessions.\r\n");
      send_to_char(ch, "  Hit points: %d\r\n", GET_MAX_HIT(ch));
      send_to_char(ch, "  Training sessions remaining: %d\r\n", GET_TRAINS(ch));
      return;
    }

    old_trains = GET_TRAINS(ch);
    old_max_hit = GET_MAX_HIT(ch);
    GET_TRAINS(ch) -= 10;
    GET_MAX_HIT(ch) += 10;
    new_trains = GET_TRAINS(ch);
    new_max_hit = GET_MAX_HIT(ch);
    send_to_char(ch, "Training successful.\r\n");
    send_to_char(ch, "  Hit points: %d -> %d\r\n", old_max_hit, new_max_hit);
    send_to_char(ch, "  Training sessions: %d -> %d\r\n", old_trains, new_trains);
    return;
  }

  if (!str_cmp(arg, "mana")) {
    if (GET_TRAINS(ch) < 10) {
      send_to_char(ch, "Training mana requires 10 training sessions.\r\n");
      send_to_char(ch, "  Mana: %d\r\n", effective_max_mana(ch));
      send_to_char(ch, "  Training sessions remaining: %d\r\n", GET_TRAINS(ch));
      return;
    }

    old_trains = GET_TRAINS(ch);
    old_max_mana = effective_max_mana(ch);
    GET_TRAINS(ch) -= 10;
    GET_MAX_MANA(ch) += 10;
    new_trains = GET_TRAINS(ch);
    new_max_mana = effective_max_mana(ch);
    send_to_char(ch, "Training successful.\r\n");
    send_to_char(ch, "  Mana: %d -> %d\r\n", old_max_mana, new_max_mana);
    send_to_char(ch, "  Training sessions: %d -> %d\r\n", old_trains, new_trains);
    return;
  }

  if (!str_cmp(arg, "move")) {
    if (GET_TRAINS(ch) < 10) {
      send_to_char(ch, "Training move requires 10 training sessions.\r\n");
      send_to_char(ch, "  Move: %d\r\n", effective_max_move(ch));
      send_to_char(ch, "  Training sessions remaining: %d\r\n", GET_TRAINS(ch));
      return;
    }

    old_trains = GET_TRAINS(ch);
    old_max_move = effective_max_move(ch);
    GET_TRAINS(ch) -= 10;
    GET_MAX_MOVE(ch) += 10;
    new_trains = GET_TRAINS(ch);
    new_max_move = effective_max_move(ch);
    send_to_char(ch, "Training successful.\r\n");
    send_to_char(ch, "  Move: %d -> %d\r\n", old_max_move, new_max_move);
    send_to_char(ch, "  Training sessions: %d -> %d\r\n", old_trains, new_trains);
    return;
  }

  if (!str_cmp(arg, "str")) {
    stat_field = &ch->real_abils.str;
    stat_name = "str";
    stat_label = "Strength";
  } else if (!str_cmp(arg, "dex")) {
    stat_field = &ch->real_abils.dex;
    stat_name = "dex";
    stat_label = "Dexterity";
  } else if (!str_cmp(arg, "con")) {
    stat_field = &ch->real_abils.con;
    stat_name = "con";
    stat_label = "Constitution";
  } else if (!str_cmp(arg, "int")) {
    stat_field = &ch->real_abils.intel;
    stat_name = "int";
    stat_label = "Intelligence";
  } else if (!str_cmp(arg, "wis")) {
    stat_field = &ch->real_abils.wis;
    stat_name = "wis";
    stat_label = "Wisdom";
  } else if (!str_cmp(arg, "cha")) {
    stat_field = &ch->real_abils.cha;
    stat_name = "cha";
    stat_label = "Charisma";
  }

  if (stat_field != NULL) {
    if (GET_TRAINS(ch) < 1) {
      send_to_char(ch, "Training %s requires 1 training session. You currently have %d.\r\n",
                   stat_name, GET_TRAINS(ch));
      send_to_char(ch, "  %s: %d/%d\r\n", stat_label, *stat_field, stat_cap);
      return;
    }

    if (*stat_field >= stat_cap) {
      send_to_char(ch, "%s is already at %d/%d and cannot be trained higher.\r\n",
                   stat_label, *stat_field, stat_cap);
      send_to_char(ch, "  Training sessions remaining: %d\r\n", GET_TRAINS(ch));
      return;
    }

    old_trains = GET_TRAINS(ch);
    old_val = *stat_field;
    old_carry = CAN_CARRY_W(ch);
    old_evasion = compute_evasion(ch);
    old_melee_crit = crit_total_melee(ch);
    old_spell_crit = crit_total_spell(ch);
    old_heal_crit = crit_total_heal(ch);
    old_max_hit = GET_MAX_HIT(ch);
    old_max_mana = effective_max_mana(ch);
    old_max_move = effective_max_move(ch);
    GET_TRAINS(ch) -= 1;
    (*stat_field)++;
    affect_total(ch);
    new_trains = GET_TRAINS(ch);
    new_val = *stat_field;
    new_carry = CAN_CARRY_W(ch);
    new_evasion = compute_evasion(ch);
    new_melee_crit = crit_total_melee(ch);
    new_spell_crit = crit_total_spell(ch);
    new_heal_crit = crit_total_heal(ch);
    new_max_hit = GET_MAX_HIT(ch);
    new_max_mana = effective_max_mana(ch);
    new_max_move = effective_max_move(ch);

    send_to_char(ch, "Training successful.\r\n");
    send_to_char(ch, "  %s: %d -> %d\r\n", stat_label, old_val, new_val);
    send_to_char(ch, "  Training sessions: %d -> %d\r\n", old_trains, new_trains);
    if (old_carry != new_carry)
      send_to_char(ch, "  Carry Capacity: %d -> %d\r\n", old_carry, new_carry);
    if (old_evasion != new_evasion)
      send_to_char(ch, "  Evasion: %d -> %d\r\n", old_evasion, new_evasion);
    if (old_melee_crit != new_melee_crit)
      send_to_char(ch, "  Critical hit: %d -> %d\r\n", old_melee_crit, new_melee_crit);
    if (old_spell_crit != new_spell_crit)
      send_to_char(ch, "  Critical Spell: %d -> %d\r\n", old_spell_crit, new_spell_crit);
    if (old_heal_crit != new_heal_crit)
      send_to_char(ch, "  Critical Heal: %d -> %d\r\n", old_heal_crit, new_heal_crit);
    if (old_max_hit != new_max_hit)
      send_to_char(ch, "  Hit points: %d -> %d\r\n", old_max_hit, new_max_hit);
    if (old_max_mana != new_max_mana)
      send_to_char(ch, "  Mana: %d -> %d\r\n", old_max_mana, new_max_mana);
    if (old_max_move != new_max_move)
      send_to_char(ch, "  Move: %d -> %d\r\n", old_max_move, new_max_move);
    return;
  }

  send_to_char(ch, "You have %d training sessions available.\r\n", GET_TRAINS(ch));
  send_to_char(ch, "Train str dex con int wis cha (cost 1, cap %d) or hit mana move (cost 10).\r\n",
               stat_cap);
}

ACMD(do_buytrain)
{
  if (IS_NPC(ch))
    return;

  if (!can_use_practice_trainer(ch)) {
    send_to_char(ch, "You must be at your class trainer to buy training sessions.\r\n");
    return;
  }

  if (GET_GLORY(ch) < GLORY_TRAIN_COST) {
    send_to_char(ch, "A training session costs %d Glory. You currently have %d Glory.\r\n",
                 GLORY_TRAIN_COST, GET_GLORY(ch));
    return;
  }

  GET_GLORY(ch) -= GLORY_TRAIN_COST;
  GET_TRAINS(ch) += 1;
  send_to_char(ch, "You spend %d Glory to buy a training session. Training sessions: %d. Glory left: %d.\r\n",
               GLORY_TRAIN_COST, GET_TRAINS(ch), GET_GLORY(ch));
}

ACMD(do_visible)
{
  if (GET_LEVEL(ch) >= LVL_IMMORT) {
    perform_immort_vis(ch);
    return;
  }

  if AFF_FLAGGED(ch, AFF_INVISIBLE) {
    appear(ch);
    send_to_char(ch, "You break the spell of invisibility.\r\n");
  } else
    send_to_char(ch, "You are already visible.\r\n");
}

ACMD(do_title)
{
  skip_spaces(&argument);
  delete_doubledollar(argument);
  parse_at(argument);

  if (IS_NPC(ch))
    send_to_char(ch, "Your title is fine... go away.\r\n");
  else if (PLR_FLAGGED(ch, PLR_NOTITLE))
    send_to_char(ch, "You can't title yourself -- you shouldn't have abused it!\r\n");
  else if (strstr(argument, "(") || strstr(argument, ")"))
    send_to_char(ch, "Titles can't contain the ( or ) characters.\r\n");
  else if (strlen(argument) > MAX_TITLE_LENGTH)
    send_to_char(ch, "Sorry, titles can't be longer than %d characters.\r\n", MAX_TITLE_LENGTH);
  else {
    set_title(ch, argument);
    send_to_char(ch, "Okay, you're now %s%s%s.\r\n", GET_NAME(ch), *GET_TITLE(ch) ? " " : "", GET_TITLE(ch));
    save_char(ch);
  }
}

static void print_group(struct char_data *ch)
{
  struct char_data * k;

  send_to_char(ch, "Your group consists of:\r\n");

  while ((k = (struct char_data *) simple_list(ch->group->members)) != NULL)
    send_to_char(ch, "%-*s: %s[%4d/%-4d]H [%4d/%-4d]M [%4d/%-4d]V%s\r\n",
	    count_color_chars(GET_NAME(k))+22, GET_NAME(k), 
            GROUP_LEADER(GROUP(ch)) == k ? CBGRN(ch, C_NRM) : CCGRN(ch, C_NRM),
            GET_HIT(k), GET_MAX_HIT(k),
            GET_MANA(k), effective_max_mana(k),
            GET_MOVE(k), GET_MAX_MOVE(k),
            CCNRM(ch, C_NRM));
}

static void display_group_list(struct char_data * ch)
{
  struct group_data * group;
  int count = 0;
	
  if (group_list->iSize) {
    send_to_char(ch, "#   Group Leader     # of Members    In Zone\r\n"
                     "---------------------------------------------------\r\n");
		
    while ((group = (struct group_data *) simple_list(group_list)) != NULL) {
			if (IS_SET(GROUP_FLAGS(group), GROUP_NPC))
			  continue;
      if (GROUP_LEADER(group) && !IS_SET(GROUP_FLAGS(group), GROUP_ANON))
        send_to_char(ch, "%-2d) %s%-12s     %-2d              %s%s\r\n", 
          ++count,
          IS_SET(GROUP_FLAGS(group), GROUP_OPEN) ? CCGRN(ch, C_NRM) : CCRED(ch, C_NRM), 
          GET_NAME(GROUP_LEADER(group)), group->members->iSize, zone_table[world[IN_ROOM(GROUP_LEADER(group))].zone].name,
          CCNRM(ch, C_NRM));
      else
        send_to_char(ch, "%-2d) Hidden\r\n", ++count);
				
		}
  }
  if (count)
    send_to_char(ch, "\r\n"
                     "%sSeeking Members%s\r\n"
                     "%sClosed%s\r\n", 
                     CCGRN(ch, C_NRM), CCNRM(ch, C_NRM),
                     CCRED(ch, C_NRM), CCNRM(ch, C_NRM));
  else
    send_to_char(ch, "\r\n"
                     "Currently no groups formed.\r\n");
}

/* Vatiken's Group System: Version 1.1 */
ACMD(do_group)
{
  char buf[MAX_STRING_LENGTH];
  struct char_data *vict;

  argument = one_argument(argument, buf);

  if (!*buf) {
    if (GROUP(ch))
      print_group(ch);
    else
      send_to_char(ch, "You must specify a group option, or type HELP GROUP for more info.\r\n");
    return;
  }
  
  if (is_abbrev(buf, "new")) {
    if (GROUP(ch))
      send_to_char(ch, "You are already in a group.\r\n");
    else
      create_group(ch);
  } else if (is_abbrev(buf, "list"))
    display_group_list(ch);
  else if (is_abbrev(buf, "join")) {
    skip_spaces(&argument);
    if (!(vict = get_char_vis(ch, argument, NULL, FIND_CHAR_ROOM))) {
      send_to_char(ch, "Join who?\r\n");
      return;
    } else if (vict == ch) {
      send_to_char(ch, "That would be one lonely grouping.\r\n");
      return;
    } else if (GROUP(ch)) {
      send_to_char(ch, "But you are already part of a group.\r\n");
      return;
    } else if (!GROUP(vict)) {
      act("$E$u is not part of a group!", FALSE, ch, 0, vict, TO_CHAR);
      return;
    } else if (!IS_SET(GROUP_FLAGS(GROUP(vict)), GROUP_OPEN)) {
      send_to_char(ch, "That group isn't accepting members.\r\n");
      return;
    }   
    join_group(ch, GROUP(vict)); 
  } else if (is_abbrev(buf, "kick")) {
    skip_spaces(&argument);
    if (!(vict = get_char_vis(ch, argument, NULL, FIND_CHAR_ROOM))) {
      send_to_char(ch, "Kick out who?\r\n");
      return;
    } else if (vict == ch) {
      send_to_char(ch, "There are easier ways to leave the group.\r\n");
      return;
    } else if (!GROUP(ch) ) {
      send_to_char(ch, "But you are not part of a group.\r\n");
      return;
    } else if (GROUP_LEADER(GROUP(ch)) != ch ) {
      send_to_char(ch, "Only the group's leader can kick members out.\r\n");
      return;
    } else if (GROUP(vict) != GROUP(ch)) {
      act("$E$u is not a member of your group!", FALSE, ch, 0, vict, TO_CHAR);
      return;
    } 
    send_to_char(ch, "You have kicked %s out of the group.\r\n", GET_NAME(vict));
    send_to_char(vict, "You have been kicked out of the group.\r\n"); 
    leave_group(vict);
  } else if (is_abbrev(buf, "regroup")) {
    if (!GROUP(ch)) {
      send_to_char(ch, "But you aren't part of a group!\r\n");
      return;
    }
    vict = GROUP_LEADER(GROUP(ch));
    if (ch == vict) {
      send_to_char(ch, "You are the group leader and cannot re-group.\r\n");
    } else {
      leave_group(ch);
      join_group(ch, GROUP(vict));
    }
  } else if (is_abbrev(buf, "leave")) {
    
    if (!GROUP(ch)) {
      send_to_char(ch, "But you aren't part of a group!\r\n");
      return;
    }
		
    leave_group(ch);
  } else if (is_abbrev(buf, "option")) {
    skip_spaces(&argument);
    if (!GROUP(ch)) {
      send_to_char(ch, "But you aren't part of a group!\r\n");
      return;
    } else if (GROUP_LEADER(GROUP(ch)) != ch) {
      send_to_char(ch, "Only the group leader can adjust the group flags.\r\n");
      return;
    }
    if (is_abbrev(argument, "open")) {
      TOGGLE_BIT(GROUP_FLAGS(GROUP(ch)), GROUP_OPEN);
      send_to_char(ch, "The group is now %s to new members.\r\n", IS_SET(GROUP_FLAGS(GROUP(ch)), GROUP_OPEN) ? "open" : "closed");
    } else if (is_abbrev(argument, "anonymous")) {
      TOGGLE_BIT(GROUP_FLAGS(GROUP(ch)), GROUP_ANON);
      send_to_char(ch, "The group location is now %s to other players.\r\n", IS_SET(GROUP_FLAGS(GROUP(ch)), GROUP_ANON) ? "invisible" : "visible");
    } else 
      send_to_char(ch, "The flag options are: Open, Anonymous\r\n");
  } else {
    send_to_char(ch, "You must specify a group option, or type HELP GROUP for more info.\r\n");		
  }

}

static struct char_data *find_charmed_follower(struct char_data *ch, const char *name)
{
  struct follow_type *follower;
  struct char_data *first_charmed = NULL, *named_match = NULL, *pet_named = NULL;

  for (follower = ch->followers; follower; follower = follower->next) {
    struct char_data *current = follower->follower;

    if (!current || current->master != ch)
      continue;
    if (!AFF_FLAGGED(current, AFF_CHARM))
      continue;

    if (!first_charmed)
      first_charmed = current;

    if (!pet_named && isname("pet", current->player.name))
      pet_named = current;

    if (name && *name && isname(name, current->player.name)) {
      named_match = current;
      break;
    }
  }

  if (named_match)
    return named_match;
  if (name && *name)
    return NULL;
  if (pet_named)
    return pet_named;

  return first_charmed;
}

static void detach_charmed_follower(struct char_data *pet)
{
  struct follow_type *f, *prev = NULL;

  if (!pet || !pet->master)
    return;

  for (f = pet->master->followers; f; prev = f, f = f->next) {
    if (f->follower != pet)
      continue;

    if (prev)
      prev->next = f->next;
    else
      pet->master->followers = f->next;

    free(f);
    break;
  }

  pet->master = NULL;
}

ACMD(do_pet_release)
{
  char arg[MAX_INPUT_LENGTH];
  struct char_data *pet;

  one_argument(argument, arg);

  if (!*arg) {
    send_to_char(ch, "Dismiss whom?\r\n");
    return;
  }

  if (!(pet = get_char_room_vis(ch, arg, NULL))) {
    send_to_char(ch, "%s", CONFIG_NOPERSON);
    return;
  }

  if (pet->master != ch) {
    send_to_char(ch, "That pet is not following you.\r\n");
    return;
  }

  if (!IS_NPC(pet)) {
    if (AFF_FLAGGED(pet, AFF_CHARM))
      break_charm_follower(ch, pet);
    else
      send_to_char(ch, "You can only dismiss a charmed follower.\r\n");
    return;
  }

  if (!is_purchased_pet(ch, pet)) {
    send_to_char(ch, "You can only dismiss a purchased pet.\r\n");
    return;
  }

  act("$N stops following you.", FALSE, ch, 0, pet, TO_CHAR);
  act("$N stops following $n.", FALSE, ch, 0, pet, TO_ROOM);

  if (pet->master) {
    struct follow_type *f, *prev = NULL;

    for (f = ch->followers; f; prev = f, f = f->next) {
      if (f->follower != pet)
        continue;

      if (prev)
        prev->next = f->next;
      else
        ch->followers = f->next;

      free(f);
      break;
    }

    pet->master = NULL;
    REMOVE_BIT_AR(AFF_FLAGS(pet), AFF_CHARM);
  }

  extract_char(pet);
}

ACMD(do_heel)
{
  char arg[MAX_INPUT_LENGTH];
  struct char_data *pet = NULL, *it;

  one_argument(argument, arg);

  for (it = character_list; it; it = it->next) {
    if (!IS_NPC(it))
      continue;
    if (it->master != ch)
      continue;
    if (!AFF_FLAGGED(it, AFF_CHARM))
      continue;
    if (!is_purchased_pet(ch, it))
      continue;
    if (*arg && !isname(arg, it->player.name))
      continue;
    pet = it;
    break;
  }

  if (!pet) {
    send_to_char(ch, "You have no purchased pet by that name to heel.\r\n");
    return;
  }

  if (IN_ROOM(pet) == IN_ROOM(ch)) {
    act("$N is already here at your side.", FALSE, ch, 0, pet, TO_CHAR);
    return;
  }

  if (FIGHTING(pet))
    stop_fighting(pet);

  if (IN_ROOM(pet) != NOWHERE)
    char_from_room(pet);
  char_to_room(pet, IN_ROOM(ch));
  GET_POS(pet) = POS_STANDING;

  act("You whistle sharply and $N heels to your side.", FALSE, ch, 0, pet, TO_CHAR);
  act("$N hustles back to $n and takes position at $s side.", FALSE, ch, 0, pet, TO_ROOM);
  act("$n hustles in and heels beside $N.", FALSE, pet, 0, ch, TO_ROOM);
}

ACMD(do_shadow)
{
  char shadow_subcmd[MAX_INPUT_LENGTH], selector[MAX_INPUT_LENGTH];
  char display_name[MAX_SHADOW_NAME_LENGTH + 1];
  int i, slot, used = 0, active = 0, cap;
  struct char_data *mob;

  half_chop(argument, shadow_subcmd, selector);
  cap = shadow_capacity(ch);
  shadow_sync_active_flags(ch);

  if (!*shadow_subcmd || is_abbrev(shadow_subcmd, "list") || is_abbrev(shadow_subcmd, "storage")) {
    int preview_level, preview_max_hp, preview_hitroll, preview_power;

    for (i = 0; i < cap; i++) {
      if (SHADOW_SLOT_OCCUPIED(ch, i)) {
        used++;
        if (SHADOW_SLOT_ACTIVE(ch, i))
          active++;
      }
    }
    send_to_char(ch, "%sShadow Storage%s: %d / %d slots used. %sActive%s: %d.\r\n",
                 CCYEL(ch, C_NRM), CCNRM(ch, C_NRM),
                 used, cap,
                 CCGRN(ch, C_NRM), CCNRM(ch, C_NRM), active);
    if (!used) {
      send_to_char(ch, "You have no stored shadows.\r\n");
    } else {
      for (i = 0; i < cap; i++) {
        if (!SHADOW_SLOT_OCCUPIED(ch, i))
          send_to_char(ch, " [%2d] %s[Empty]%s\r\n", i + 1, CCGRN(ch, C_NRM), CCNRM(ch, C_NRM));
        else {
          compute_shadow_preview_stats(ch, i, &preview_level, &preview_max_hp, &preview_hitroll, NULL, &preview_power);
          send_to_char(ch, " [%2d] %s[Shadow: %-30.30s]%s Lvl %-3d [Power:%-3d] [HP:%-3d] [Hit:%+d] %s%s%s\r\n",
                       i + 1,
                       CCYEL(ch, C_NRM), shadow_slot_display_name(ch, i), CCNRM(ch, C_NRM),
                       preview_level,
                       preview_power,
                       preview_max_hp,
                       preview_hitroll,
                       SHADOW_SLOT_ACTIVE(ch, i) ? CCGRN(ch, C_NRM) : "",
                       SHADOW_SLOT_ACTIVE(ch, i) ? "[Active]" : "[Stored]",
                       SHADOW_SLOT_ACTIVE(ch, i) ? CCNRM(ch, C_NRM) : "");
        }
      }
    }
    send_to_char(ch, "Commands:\r\n");
    send_to_char(ch, "  shadow summon <slot|name>\r\n");
    send_to_char(ch, "  shadow store <slot|name>\r\n");
    send_to_char(ch, "  shadow release <slot|name>\r\n");
    send_to_char(ch, "  shadow rename <slot|name> <new name>\r\n");
    return;
  }

  if (is_abbrev(shadow_subcmd, "summon")) {
    char deny_reason[MAX_INPUT_LENGTH];
    int mana_cost;

    if (!*selector) {
      send_to_char(ch, "Usage: shadow summon <slot|name>\r\n");
      return;
    }
    slot = shadow_find_slot(ch, selector);
    if (slot < 0 || slot >= cap || !SHADOW_SLOT_OCCUPIED(ch, slot)) {
      send_to_char(ch, "No stored shadow matches '%s'.\r\n", selector);
      return;
    }
    if (SHADOW_SLOT_ACTIVE(ch, slot)) {
      send_to_char(ch, "That shadow is already summoned.\r\n");
      return;
    }
    if (!shadow_can_summon_more(ch, deny_reason, sizeof(deny_reason))) {
      send_to_char(ch, "%s\r\n", *deny_reason ? deny_reason : "You cannot maintain any more active shadows right now.");
      return;
    }
    mana_cost = 10 + (MAX(1, SHADOW_SLOT_LEVEL(ch, slot)) * 2);
    if (GET_MANA(ch) < mana_cost) {
      send_to_char(ch, "You lack the mana to summon this shadow.\r\n");
      return;
    }
    GET_MANA(ch) -= mana_cost;
    if (!summon_stored_shadow(ch, slot)) {
      GET_MANA(ch) += mana_cost;
      send_to_char(ch, "The shadow resists your call right now.\r\n");
      return;
    }
    strlcpy(display_name, shadow_slot_display_name(ch, slot), sizeof(display_name));
    act("You call forth $T from your shadow storage.", FALSE, ch, NULL, display_name, TO_CHAR);
    act("$n calls forth $T from $s shadow storage.", FALSE, ch, NULL, display_name, TO_ROOM);
    send_to_char(ch, "You expend %d mana.\r\n", mana_cost);
    save_char(ch);
    return;
  }

  if (is_abbrev(shadow_subcmd, "store")) {
    if (!*selector) {
      send_to_char(ch, "Usage: shadow store <slot|name>\r\n");
      return;
    }
    slot = shadow_find_slot(ch, selector);
    if (slot < 0 || slot >= cap || !SHADOW_SLOT_OCCUPIED(ch, slot)) {
      send_to_char(ch, "No stored shadow matches '%s'.\r\n", selector);
      return;
    }
    mob = shadow_active_mob(ch, slot);
    if (!mob) {
      send_to_char(ch, "That shadow is not currently summoned.\r\n");
      SHADOW_SLOT_ACTIVE(ch, slot) = 0;
      save_char(ch);
      return;
    }
    strlcpy(display_name, shadow_slot_display_name(ch, slot), sizeof(display_name));
    SHADOW_SLOT_ACTIVE(ch, slot) = 0;
    shadow_prepare_for_removal(mob);
    extract_char(mob);
    send_to_char(ch, "You return %s to your shadow storage.\r\n", display_name);
    save_char(ch);
    return;
  }

  if (is_abbrev(shadow_subcmd, "release")) {
    if (!*selector) {
      send_to_char(ch, "Usage: shadow release <slot|name>\r\n");
      return;
    }
    slot = shadow_find_slot(ch, selector);
    if (slot < 0 || slot >= cap || !SHADOW_SLOT_OCCUPIED(ch, slot)) {
      send_to_char(ch, "No stored shadow matches '%s'.\r\n", selector);
      return;
    }
    mob = shadow_active_mob(ch, slot);
    strlcpy(display_name, shadow_slot_display_name(ch, slot), sizeof(display_name));
    if (mob) {
      shadow_prepare_for_removal(mob);
      extract_char(mob);
    }
    act("You release $t back into the void.", FALSE, ch, NULL, display_name, TO_CHAR);
    act("$n releases $t back into the void.", FALSE, ch, NULL, display_name, TO_ROOM);
    SHADOW_SLOT_OCCUPIED(ch, slot) = 0;
    SHADOW_SLOT_ACTIVE(ch, slot) = 0;
    SHADOW_SLOT_LEVEL(ch, slot) = 0;
    SHADOW_SLOT_VNUM(ch, slot) = NOBODY;
    SHADOW_SLOT_PROFILE_VALID(ch, slot) = 0;
    SHADOW_SLOT_MAX_HIT(ch, slot) = 0;
    SHADOW_SLOT_AC(ch, slot) = 0;
    SHADOW_SLOT_HITROLL(ch, slot) = 0;
    SHADOW_SLOT_DAMROLL(ch, slot) = 0;
    SHADOW_SLOT_MAX_MANA(ch, slot) = 0;
    SHADOW_SLOT_DAMNODICE(ch, slot) = 0;
    SHADOW_SLOT_DAMSIZEDICE(ch, slot) = 0;
    SHADOW_SLOT_STR(ch, slot) = 0;
    SHADOW_SLOT_INT(ch, slot) = 0;
    SHADOW_SLOT_WIS(ch, slot) = 0;
    SHADOW_SLOT_DEX(ch, slot) = 0;
    SHADOW_SLOT_CON(ch, slot) = 0;
    SHADOW_SLOT_CHA(ch, slot) = 0;
    SHADOW_SLOT_NAME(ch, slot)[0] = '\0';
    save_char(ch);
    return;
  }

  if (is_abbrev(shadow_subcmd, "rename")) {
    char target[MAX_INPUT_LENGTH], new_name[MAX_INPUT_LENGTH];
    char safe_name[MAX_SHADOW_NAME_LENGTH + 1];

    half_chop(selector, target, new_name);
    if (!*target) {
      send_to_char(ch, "Usage: shadow rename <slot|name> <new name>\r\n");
      return;
    }
    if (!*new_name) {
      send_to_char(ch, "Rename it to what?\r\n");
      return;
    }

    slot = shadow_find_slot(ch, target);
    if (slot < 0 || slot >= cap || !SHADOW_SLOT_OCCUPIED(ch, slot)) {
      send_to_char(ch, "You do not have that shadow.\r\n");
      return;
    }
    if (!sanitize_shadow_name_input(new_name, safe_name, sizeof(safe_name))) {
      send_to_char(ch, "That is not a valid shadow name.\r\n");
      return;
    }

    strlcpy(SHADOW_SLOT_NAME(ch, slot), safe_name, MAX_SHADOW_NAME_LENGTH + 1);
    mob = shadow_active_mob(ch, slot);
    if (mob)
      apply_shadow_identity_to_mob(ch, mob, slot);
    send_to_char(ch, "You rename the shadow to %s.\r\n", safe_name);
    save_char(ch);
    return;
  }

  send_to_char(ch, "Usage: shadow <list|storage|summon|store|release|rename>\r\n");
}

ACMD(do_opet)
{
  char first_arg[MAX_INPUT_LENGTH], command_part[MAX_INPUT_LENGTH];
  char cmd_sub[MAX_INPUT_LENGTH], target[MAX_INPUT_LENGTH];
  char order_argument[MAX_INPUT_LENGTH * 2];
  struct char_data *follower;
  const char *usage =
    "Usage: opet [follower] stay | opet [follower] follow | opet [follower] attack <target> | opet [follower] assist <target>\r\n";

  half_chop(argument, first_arg, command_part);

  follower = find_charmed_follower(ch, first_arg);

  if (!follower) {
    follower = find_charmed_follower(ch, NULL);
    if (*first_arg)
      strlcpy(command_part, argument, sizeof(command_part));
  }

  if (!follower || follower->master != ch) {
    send_to_char(ch, "You have no charmed follower to command.\r\n");
    return;
  }

  if (!*command_part) {
    send_to_char(ch, "%s", usage);
    return;
  }

  half_chop(command_part, cmd_sub, target);

  if (is_abbrev(cmd_sub, "stay")) {
    if (follower->master == ch) {
      act("You order $N to stay here.", FALSE, ch, 0, follower, TO_CHAR);
      act("$n orders $N to stay here.", FALSE, ch, 0, follower, TO_ROOM);
      if (GET_POS(follower) > POS_SITTING) {
        act("$n sits down obediently.", TRUE, follower, 0, 0, TO_ROOM);
        act("$n sits down obediently.", TRUE, follower, 0, 0, TO_CHAR);
      }
      GET_POS(follower) = POS_SITTING;
    } else {
      send_to_char(ch, "%s isn't following you.\r\n", GET_NAME(follower));
    }
    return;
  } else if (is_abbrev(cmd_sub, "follow")) {
    if (follower->master && follower->master != ch)
      detach_charmed_follower(follower);

    if (follower->master == ch) {
      if (GET_POS(follower) < POS_STANDING) {
        act("You motion for $N to stand up and follow you.", FALSE, ch, 0, follower, TO_CHAR);
        act("$n motions for $N to stand up and follow $m.", FALSE, ch, 0, follower, TO_ROOM);
        GET_POS(follower) = POS_STANDING;
      } else
        send_to_char(ch, "%s is already following you.\r\n", GET_NAME(follower));
      return;
    }

    add_follower(follower, ch);
    SET_BIT_AR(AFF_FLAGS(follower), AFF_CHARM);
    return;
  } else if (is_abbrev(cmd_sub, "attack")) {
    if (!*target) {
      send_to_char(ch, "Usage: opet [follower] attack <target>\r\n");
      return;
    }

    snprintf(order_argument, sizeof(order_argument), "hit %s", target);
  } else if (is_abbrev(cmd_sub, "assist")) {
    if (!*target) {
      send_to_char(ch, "Usage: opet [follower] assist <target>\r\n");
      return;
    }

    snprintf(order_argument, sizeof(order_argument), "assist %s", target);
  } else {
    send_to_char(ch, "%s", usage);
    return;
  }

  do_order(ch, order_argument, 0, 0);
}

ACMD(do_report)
{
  struct group_data *group;

  if ((group = GROUP(ch)) == NULL) {
    send_to_char(ch, "But you are not a member of any group!\r\n");
    return;
  }

  send_to_group(NULL, group, "%s reports: %d/%dH, %d/%dM, %d/%dV\r\n",
          GET_NAME(ch),
          GET_HIT(ch), GET_MAX_HIT(ch),
          GET_MANA(ch), effective_max_mana(ch),
          GET_MOVE(ch), GET_MAX_MOVE(ch));
}

ACMD(do_split)
{
  char buf[MAX_INPUT_LENGTH];
  int amount, num = 0, share, rest;
  size_t len;
  struct char_data *k;
  
  if (IS_NPC(ch))
    return;

  one_argument(argument, buf);

  if (is_number(buf)) {
    amount = atoi(buf);
    if (amount <= 0) {
      send_to_char(ch, "Sorry, you can't do that.\r\n");
      return;
    }
    if (amount > GET_GOLD(ch)) {
      send_to_char(ch, "You don't seem to have that much gold to split.\r\n");
      return;
    }
    
    if (GROUP(ch))
      while ((k = (struct char_data *) simple_list(GROUP(ch)->members)) != NULL)
        if (IN_ROOM(ch) == IN_ROOM(k) && !IS_NPC(k))
          num++;

    if (num && GROUP(ch)) {
      share = amount / num;
      rest = amount % num;
    } else {
      send_to_char(ch, "With whom do you wish to share your gold?\r\n");
      return;
    }

    decrease_gold(ch, share * (num - 1));

    /* Abusing signed/unsigned to make sizeof work. */
    len = snprintf(buf, sizeof(buf), "%s splits %d coins; you receive %d.\r\n",
		GET_NAME(ch), amount, share);
    if (rest && len < sizeof(buf)) {
      snprintf(buf + len, sizeof(buf) - len,
		"%d coin%s %s not splitable, so %s keeps the money.\r\n", rest,
		(rest == 1) ? "" : "s", (rest == 1) ? "was" : "were", GET_NAME(ch));
    }

    while ((k = (struct char_data *) simple_list(GROUP(ch)->members)) != NULL)
      if (k != ch && IN_ROOM(ch) == IN_ROOM(k) && !IS_NPC(k)) {
	      increase_gold(k, share);
	      send_to_char(k, "%s", buf);
			}

    send_to_char(ch, "You split %d coins among %d members -- %d coins each.\r\n",
	    amount, num, share);

    if (rest) {
      send_to_char(ch, "%d coin%s %s not splitable, so you keep the money.\r\n",
		rest, (rest == 1) ? "" : "s", (rest == 1) ? "was" : "were");
    }
  } else {
    send_to_char(ch, "How many coins do you wish to split with your group?\r\n");
    return;
  }
}

ACMD(do_use)
{
  char buf[MAX_INPUT_LENGTH], arg[MAX_INPUT_LENGTH];
  struct obj_data *mag_item;

  half_chop(argument, arg, buf);
  if (!*arg) {
    send_to_char(ch, "What do you want to %s?\r\n", CMD_NAME);
    return;
  }
  mag_item = GET_EQ(ch, WEAR_HOLD);

  if (!mag_item || !isname(arg, mag_item->name)) {
    switch (subcmd) {
    case SCMD_RECITE:
    case SCMD_QUAFF:
      if (!(mag_item = get_obj_in_list_vis(ch, arg, NULL, ch->carrying))) {
	send_to_char(ch, "You don't seem to have %s %s.\r\n", AN(arg), arg);
	return;
      }
      break;
    case SCMD_USE:
      send_to_char(ch, "You don't seem to be holding %s %s.\r\n", AN(arg), arg);
      return;
    default:
      log("SYSERR: Unknown subcmd %d passed to do_use.", subcmd);
      /* SYSERR_DESC: This is the same as the unhandled case in do_gen_ps(),
       * but in the function which handles 'quaff', 'recite', and 'use'. */
      return;
    }
  }
  switch (subcmd) {
  case SCMD_QUAFF:
    if (GET_OBJ_TYPE(mag_item) != ITEM_POTION) {
      send_to_char(ch, "You can only quaff potions.\r\n");
      return;
    }
    break;
  case SCMD_RECITE:
    if (GET_OBJ_TYPE(mag_item) != ITEM_SCROLL) {
      send_to_char(ch, "You can only recite scrolls.\r\n");
      return;
    }
    break;
  case SCMD_USE:
    if ((GET_OBJ_TYPE(mag_item) != ITEM_WAND) &&
	(GET_OBJ_TYPE(mag_item) != ITEM_STAFF)) {
      send_to_char(ch, "You can't seem to figure out how to use it.\r\n");
      return;
    }
    break;
  }

  mag_objectmagic(ch, mag_item, buf);
}

#define TOG_OFF 0
#define TOG_ON  1
ACMD(do_gen_tog)
{
  long result;
  int i;
  char arg[MAX_INPUT_LENGTH];

  const char *tog_messages[][2] = {
    {"You are now safe from summoning by other players.\r\n",
    "You may now be summoned by other players.\r\n"},
    {"Nohassle disabled.\r\n",
    "Nohassle enabled.\r\n"},
    {"Brief mode off.\r\n",
    "Brief mode on.\r\n"},
    {"Compact mode off.\r\n",
    "Compact mode on.\r\n"},
    {"You can now hear tells.\r\n",
    "You are now deaf to tells.\r\n"},
    {"You can now hear auctions.\r\n",
    "You are now deaf to auctions.\r\n"},
    {"You can now hear shouts.\r\n",
    "You are now deaf to shouts.\r\n"},
    {"You can now hear gossip.\r\n",
    "You are now deaf to gossip.\r\n"},
    {"You can now hear the congratulation messages.\r\n",
    "You are now deaf to the congratulation messages.\r\n"},
    {"You can now hear the Wiz-channel.\r\n",
    "You are now deaf to the Wiz-channel.\r\n"},
    {"You are no longer part of the Quest.\r\n",
    "Okay, you are part of the Quest!\r\n"},
    {"You will no longer see the room flags.\r\n",
    "You will now see the room flags.\r\n"},
    {"You will now have your communication repeated.\r\n",
    "You will no longer have your communication repeated.\r\n"},
    {"HolyLight mode off.\r\n",
    "HolyLight mode on.\r\n"},
    {"Nameserver_is_slow changed to NO; IP addresses will now be resolved.\r\n",
    "Nameserver_is_slow changed to YES; sitenames will no longer be resolved.\r\n"},
    {"Autoexits disabled.\r\n",
    "Autoexits enabled.\r\n"},
    {"Will no longer track through doors.\r\n",
    "Will now track through doors.\r\n"},
    {"Will no longer clear screen in OLC.\r\n",
    "Will now clear screen in OLC.\r\n"},
    {"Buildwalk Off.\r\n",
    "Buildwalk On.\r\n"},
    {"AFK flag is now off.\r\n",
    "AFK flag is now on.\r\n"},
    {"Autoloot disabled.\r\n",
    "Autoloot enabled.\r\n"},
    {"Autogold disabled.\r\n",
    "Autogold enabled.\r\n"},
    {"Autosplit disabled.\r\n",
    "Autosplit enabled.\r\n"},
    {"Autosacrifice disabled.\r\n",
    "Autosacrifice enabled.\r\n"},
    {"Autoassist disabled.\r\n",
    "Autoassist enabled.\r\n"},
    {"Automap disabled.\r\n",
    "Automap enabled.\r\n"},
    {"Autokey disabled.\r\n",
    "Autokey enabled.\r\n"},
    {"Autodoor disabled.\r\n",
    "Autodoor enabled.\r\n"},
    {"ZoneResets disabled.\r\n",
    "ZoneResets enabled.\r\n"}
  };

  if (IS_NPC(ch))
    return;

  switch (subcmd) {
  case SCMD_NOSUMMON:
    result = PRF_TOG_CHK(ch, PRF_SUMMONABLE);
    break;
  case SCMD_NOHASSLE:
    result = PRF_TOG_CHK(ch, PRF_NOHASSLE);
    break;
  case SCMD_BRIEF:
    result = PRF_TOG_CHK(ch, PRF_BRIEF);
    break;
  case SCMD_COMPACT:
    result = PRF_TOG_CHK(ch, PRF_COMPACT);
    break;
  case SCMD_NOTELL:
    result = PRF_TOG_CHK(ch, PRF_NOTELL);
    break;
  case SCMD_NOAUCTION:
    result = PRF_TOG_CHK(ch, PRF_NOAUCT);
    break;
  case SCMD_NOSHOUT:
    result = PRF_TOG_CHK(ch, PRF_NOSHOUT);
    break;
  case SCMD_NOGOSSIP:
    result = PRF_TOG_CHK(ch, PRF_NOGOSS);
    break;
  case SCMD_NOGRATZ:
    result = PRF_TOG_CHK(ch, PRF_NOGRATZ);
    break;
  case SCMD_NOWIZ:
    result = PRF_TOG_CHK(ch, PRF_NOWIZ);
    break;
  case SCMD_QUEST:
    result = PRF_TOG_CHK(ch, PRF_QUEST);
    break;
  case SCMD_SHOWVNUMS:
    result = PRF_TOG_CHK(ch, PRF_SHOWVNUMS);
    break;
  case SCMD_NOREPEAT:
    result = PRF_TOG_CHK(ch, PRF_NOREPEAT);
    break;
  case SCMD_HOLYLIGHT:
    result = PRF_TOG_CHK(ch, PRF_HOLYLIGHT);
    break;
  case SCMD_AUTOEXIT:
    result = PRF_TOG_CHK(ch, PRF_AUTOEXIT);
    break;
  case SCMD_CLS:
    result = PRF_TOG_CHK(ch, PRF_CLS);
    break;    
  case SCMD_BUILDWALK:
    if (GET_LEVEL(ch) < LVL_BUILDER) {
      send_to_char(ch, "Builders only, sorry.\r\n");
      return;
    }
    result = PRF_TOG_CHK(ch, PRF_BUILDWALK);
    if (PRF_FLAGGED(ch, PRF_BUILDWALK)) {
      one_argument(argument, arg);
      for (i=0; *arg && *(sector_types[i]) != '\n'; i++)
        if (is_abbrev(arg, sector_types[i]))
          break;
      if (*(sector_types[i]) == '\n') 
        i=0;
      GET_BUILDWALK_SECTOR(ch) = i;
      send_to_char(ch, "Default sector type is %s\r\n", sector_types[i]);
  
      mudlog(CMP, GET_LEVEL(ch), TRUE,
             "OLC: %s turned buildwalk on. Allowed zone %d", GET_NAME(ch), GET_OLC_ZONE(ch));
    } else
      mudlog(CMP, GET_LEVEL(ch), TRUE,
             "OLC: %s turned buildwalk off. Allowed zone %d", GET_NAME(ch), GET_OLC_ZONE(ch));
    break;
  case SCMD_AFK:
    result = PRF_TOG_CHK(ch, PRF_AFK);
    if (PRF_FLAGGED(ch, PRF_AFK))
      act("$n has gone AFK.", TRUE, ch, 0, 0, TO_ROOM);
    else {
      act("$n has come back from AFK.", TRUE, ch, 0, 0, TO_ROOM);
      if (has_mail(GET_IDNUM(ch)))
        send_to_char(ch, "You have mail waiting.\r\n");
    }
    break;
  case SCMD_AUTOLOOT:
    result = PRF_TOG_CHK(ch, PRF_AUTOLOOT);
    break;
  case SCMD_AUTOGOLD:
    result = PRF_TOG_CHK(ch, PRF_AUTOGOLD);
    break;
  case SCMD_AUTOSPLIT:
    result = PRF_TOG_CHK(ch, PRF_AUTOSPLIT);
    break;
  case SCMD_AUTOSAC:
    result = PRF_TOG_CHK(ch, PRF_AUTOSAC);
    break;
  case SCMD_AUTOASSIST:
    result = PRF_TOG_CHK(ch, PRF_AUTOASSIST);
    break;
  case SCMD_AUTOMAP:
    result = PRF_TOG_CHK(ch, PRF_AUTOMAP);
    break;
  case SCMD_AUTOKEY:
    result = PRF_TOG_CHK(ch, PRF_AUTOKEY);
    break;
  case SCMD_AUTODOOR:
    result = PRF_TOG_CHK(ch, PRF_AUTODOOR);
    break;
  case SCMD_ZONERESETS:
    result = PRF_TOG_CHK(ch, PRF_ZONERESETS);
    break;
  case SCMD_SHORTFLAGS:
    result = PRF_TOG_CHK(ch, PRF_SHORTFLAGS);
    send_to_char(ch, "Shortflags %s.\r\n", result ? "enabled" : "disabled");
    return;
  default:
    log("SYSERR: Unknown subcmd %d in do_gen_toggle.", subcmd);
    return;
  }

  if (result)
    send_to_char(ch, "%s", tog_messages[subcmd][TOG_ON]);
  else
    send_to_char(ch, "%s", tog_messages[subcmd][TOG_OFF]);

  return;
}

/* Toggle auto zone reset notifications with optional on/off arguments. */
ACMD(do_azr)
{
  char arg[MAX_INPUT_LENGTH];

  if (IS_NPC(ch))
    return;

  one_argument(argument, arg);

  if (!*arg) {
    int enabled = PRF_TOG_CHK(ch, PRF_ZONERESETS);
    send_to_char(ch, "Auto zone reset notifications %s.\r\n",
                 enabled ? "enabled" : "disabled");
    return;
  }

  if (is_abbrev(arg, "on")) {
    SET_BIT_AR(PRF_FLAGS(ch), PRF_ZONERESETS);
    send_to_char(ch, "Auto zone reset notifications enabled.\r\n");
  } else if (is_abbrev(arg, "off")) {
    REMOVE_BIT_AR(PRF_FLAGS(ch), PRF_ZONERESETS);
    send_to_char(ch, "Auto zone reset notifications disabled.\r\n");
  } else {
    send_to_char(ch, "Usage: azr [on|off]\r\n");
  }
}

static void show_happyhour(struct char_data *ch)
{
  char happyexp[80], happygold[80], happyqp[80];
  int secs_left;

  if ((IS_HAPPYHOUR) || (GET_LEVEL(ch) >= LVL_GRGOD))
  {
      if (HAPPY_TIME)
        secs_left = ((HAPPY_TIME - 1) * SECS_PER_MUD_HOUR) + next_tick;
      else
        secs_left = 0;

      sprintf(happyqp,   "%s+%d%%%s to Questpoints per quest\r\n", CCYEL(ch, C_NRM), HAPPY_QP,   CCNRM(ch, C_NRM));
      sprintf(happygold, "%s+%d%%%s to Gold gained per kill\r\n",  CCYEL(ch, C_NRM), HAPPY_GOLD, CCNRM(ch, C_NRM));
      sprintf(happyexp,  "%s+%d%%%s to Experience per kill\r\n",   CCYEL(ch, C_NRM), HAPPY_EXP,  CCNRM(ch, C_NRM));

      send_to_char(ch, "tbaMUD Happy Hour!\r\n"
                       "------------------\r\n"
                       "%s%s%sTime Remaining: %s%d%s hours %s%d%s mins %s%d%s secs\r\n",
                       (IS_HAPPYEXP || (GET_LEVEL(ch) >= LVL_GOD)) ? happyexp : "",
                       (IS_HAPPYGOLD || (GET_LEVEL(ch) >= LVL_GOD)) ? happygold : "",
                       (IS_HAPPYQP || (GET_LEVEL(ch) >= LVL_GOD)) ? happyqp : "",
                       CCYEL(ch, C_NRM), (secs_left / 3600), CCNRM(ch, C_NRM),
                       CCYEL(ch, C_NRM), (secs_left % 3600) / 60, CCNRM(ch, C_NRM),
                       CCYEL(ch, C_NRM), (secs_left % 60), CCNRM(ch, C_NRM) );
  }
  else
  {
      send_to_char(ch, "Sorry, there is currently no happy hour!\r\n");
  }
}

ACMD(do_happyhour)
{
  char arg[MAX_INPUT_LENGTH], val[MAX_INPUT_LENGTH];
  int num;

  if (GET_LEVEL(ch) < LVL_GOD)
  {
    show_happyhour(ch);
    return;
  }

  /* Only Imms get here, so check args */
  two_arguments(argument, arg, val);

  if (is_abbrev(arg, "experience"))
  {
    num = MIN(MAX((atoi(val)), 0), 1000);
    HAPPY_EXP = num;
    send_to_char(ch, "Happy Hour Exp rate set to +%d%%\r\n", HAPPY_EXP);
  }
  else if ((is_abbrev(arg, "gold")) || (is_abbrev(arg, "coins")))
  {
    num = MIN(MAX((atoi(val)), 0), 1000);
    HAPPY_GOLD = num;
    send_to_char(ch, "Happy Hour Gold rate set to +%d%%\r\n", HAPPY_GOLD);
  }
  else if ((is_abbrev(arg, "time")) || (is_abbrev(arg, "ticks")))
  {
    num = MIN(MAX((atoi(val)), 0), 1000);
    if (HAPPY_TIME && !num)
      game_info("Happyhour has been stopped!");
    else if (!HAPPY_TIME && num)
      game_info("A Happyhour has started!");

    HAPPY_TIME = num;
    send_to_char(ch, "Happy Hour Time set to %d ticks (%d hours %d mins and %d secs)\r\n",
                                HAPPY_TIME,
                                 (HAPPY_TIME*SECS_PER_MUD_HOUR)/3600,
                                ((HAPPY_TIME*SECS_PER_MUD_HOUR)%3600) / 60,
                                 (HAPPY_TIME*SECS_PER_MUD_HOUR)%60 );
  }
  else if ((is_abbrev(arg, "qp")) || (is_abbrev(arg, "questpoints")))
  {
    num = MIN(MAX((atoi(val)), 0), 1000);
    HAPPY_QP = num;
    send_to_char(ch, "Happy Hour Questpoints rate set to +%d%%\r\n", HAPPY_QP);
  }
  else if (is_abbrev(arg, "show"))
  {
    show_happyhour(ch);
  }
  else if (is_abbrev(arg, "default"))
  {
    HAPPY_EXP = 100;
    HAPPY_GOLD = 50;
    HAPPY_QP  = 50;
    HAPPY_TIME = 48;
    game_info("A Happyhour has started!");
  }
  else
  {
    send_to_char(ch, "Usage: %shappyhour              %s- show usage (this info)\r\n"
                     "       %shappyhour show         %s- display current settings (what mortals see)\r\n"
                     "       %shappyhour time <ticks> %s- set happyhour time and start timer\r\n"
                     "       %shappyhour qp <num>     %s- set qp percentage gain\r\n"
                     "       %shappyhour exp <num>    %s- set exp percentage gain\r\n"
                     "       %shappyhour gold <num>   %s- set gold percentage gain\r\n"
                     "       \tyhappyhour default      \tw- sets a default setting for happyhour\r\n\r\n"
                     "Configure the happyhour settings and start a happyhour.\r\n"
                     "Currently 1 hour IRL = %d ticks\r\n"
                     "If no number is specified, 0 (off) is assumed.\r\nThe command \tyhappyhour time\tn will therefore stop the happyhour timer.\r\n",
                     CCYEL(ch, C_NRM), CCNRM(ch, C_NRM),
                     CCYEL(ch, C_NRM), CCNRM(ch, C_NRM),
                     CCYEL(ch, C_NRM), CCNRM(ch, C_NRM),
                     CCYEL(ch, C_NRM), CCNRM(ch, C_NRM),
                     CCYEL(ch, C_NRM), CCNRM(ch, C_NRM),
                     CCYEL(ch, C_NRM), CCNRM(ch, C_NRM),
                     (3600 / SECS_PER_MUD_HOUR) );
  }
}
ACMD(do_spells)
{
  char arg[MAX_INPUT_LENGTH], filter[MAX_INPUT_LENGTH];
  int show_all = 0;
  *filter = '\0';

  half_chop(argument, arg, filter);
  if ((*arg && (!str_cmp(arg, "help") || !str_cmp(arg, "?"))) ||
      (*filter && (!str_cmp(filter, "help") || !str_cmp(filter, "?")))) {
    show_ability_filter_help(ch, (subcmd == 1) ? "allspells" : "spells");
    return;
  }
  if (subcmd == 1) {
    show_all = 1;
    if (*arg && !*filter)
      strlcpy(filter, arg, sizeof(filter));
  } else if (*arg && !str_cmp(arg, "all"))
    show_all = 1;
  else if (*arg && !*filter)
    strlcpy(filter, arg, sizeof(filter));

  send_to_char(ch, "You have %d practice sessions remaining.\r\n", GET_PRACTICES(ch));
  if ((show_all || subcmd == 1) && !GET_CLASS_LOCKED(ch)) {
    show_adventurer_study_catalog(ch, 1, filter);
    return;
  }
  if (show_all || subcmd == 1)
    send_to_char(ch, "Showing all spells your class can learn at any level.\r\n");
  show_ability_table_aligned(ch, 1, show_all, filter);
}
