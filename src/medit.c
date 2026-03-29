/**************************************************************************
*  File: medit.c                                           Part of tbaMUD *
*  Usage: Oasis OLC - Mobiles.                                            *
*                                                                         *
* Copyright 1996 Harvey Gilpin. 1997-2001 George Greer.                   *
**************************************************************************/

#include "conf.h"
#include <stdio.h>
#include "sysdep.h"
#include "structs.h"

#include "utils.h"
#include "interpreter.h"
#include "comm.h"
#include "spells.h"
#include "db.h"
#include "shop.h"
#include "genolc.h"
#include "genmob.h"
#include "genzon.h"
#include "genshp.h"
#include "oasis.h"
#include "handler.h"
#include "constants.h"
#include "improved-edit.h"
#include "dg_olc.h"
#include "screen.h"
#include "fight.h"
#include "modify.h"      /* for smash_tilde */
#include "mob_behavior.h"

/* Use shared action_bits[] for display so OLC remains aligned with runtime
 * bit names as new mob flags are added.
 */

/* local functions */
static void medit_setup_new(struct descriptor_data *d);
static void init_mobile(struct char_data *mob);
static void medit_save_to_disk(zone_rnum zone_num);
static void medit_disp_positions(struct descriptor_data *d);
static void medit_disp_sex(struct descriptor_data *d);
static void medit_disp_attack_types(struct descriptor_data *d);
static bool medit_illegal_mob_flag(int fl);
static int  medit_get_mob_flag_by_number(int num);
static void medit_disp_mob_flags(struct descriptor_data *d);
static void medit_disp_aff_flags(struct descriptor_data *d);
static void medit_disp_menu(struct descriptor_data *d);
static void medit_disp_loadout_menu(struct descriptor_data *d);
static int medit_slot_required_wear_flag(int wear_pos);
static int medit_object_can_equip_slot(struct obj_data *obj, int wear_pos);
static int medit_parse_int_argument(const char *arg, int *value);
static int medit_arg_is_cancel(const char *arg);
static const char *medit_slot_label_by_wear_pos(int wear_pos);
static int medit_slot_from_picker_choice(int choice);
static void medit_disp_slot_picker(struct descriptor_data *d, const char *title, const char *prompt);
static const char *medit_required_wear_flag_desc(int wear_pos);
static void medit_disp_remove_inventory_picker(struct descriptor_data *d);
static void medit_disp_remove_loot_picker(struct descriptor_data *d);
static void medit_disp_combat_abilities_menu(struct descriptor_data *d);
static void medit_disp_combat_ability_field_menu(struct descriptor_data *d);
static void medit_disp_event_reactions_menu(struct descriptor_data *d);
static void medit_disp_event_reaction_field_menu(struct descriptor_data *d);
static int medit_validate_combat_ability(struct descriptor_data *d, struct mob_combat_ability *ab);
static int medit_validate_event_reaction(struct descriptor_data *d, struct mob_event_reaction *ev);

static const int medit_eq_picker_slots[] = {
  WEAR_HEAD, WEAR_EYES, WEAR_EAR_L, WEAR_EAR_R, WEAR_NECK_1, WEAR_ABOUT, WEAR_BODY, WEAR_ARMS, WEAR_WRIST_R,
  WEAR_WRIST_L, WEAR_HANDS, WEAR_FINGER_R, WEAR_FINGER_L, WEAR_WAIST,
  WEAR_LEGS, WEAR_FEET, WEAR_WIELD, WEAR_HOLD, WEAR_SHIELD, WEAR_LIGHT
};

static const char *medit_eq_picker_labels[] = {
  "Head", "Eyes", "Ear Left", "Ear Right", "Neck", "Around Body", "Body", "Arms", "Wrist Right", "Wrist Left",
  "Hands", "Finger Right", "Finger Left", "Waist", "Legs", "Feet",
  "Wield", "Hold", "Shield", "Light"
};

/*  utility functions */
ACMD(do_oasis_medit)
{
  int number = NOBODY, save = 0, real_num;
  struct descriptor_data *d;
  char buf1[MAX_STRING_LENGTH];
  char buf2[MAX_STRING_LENGTH];

  /* No building as a mob or while being forced. */
  if (IS_NPC(ch) || !ch->desc || STATE(ch->desc) != CON_PLAYING)
    return;

  /* Parse any arguments */
  two_arguments(argument, buf1, buf2);

  if (!*buf1) {
    send_to_char(ch, "Specify a mobile VNUM to edit.\r\n");
    return;
  } else if (!isdigit(*buf1)) {
    if (str_cmp("save", buf1) != 0) {
      send_to_char(ch, "Yikes!  Stop that, someone will get hurt!\r\n");
      return;
    }

    save = TRUE;

    if (is_number(buf2))
      number = atoi(buf2);
    else if (GET_OLC_ZONE(ch) > 0) {
      zone_rnum zlok;

      if ((zlok = real_zone(GET_OLC_ZONE(ch))) == NOWHERE)
        number = NOWHERE;
      else
        number = genolc_zone_bottom(zlok);
    }

    if (number == NOWHERE) {
      send_to_char(ch, "Save which zone?\r\n");
      return;
    }
  }

  /* If a numeric argument was given (like a room number), get it. */
  if (number == NOBODY)
    number = atoi(buf1);

  if (number < IDXTYPE_MIN || number > IDXTYPE_MAX) {
    send_to_char(ch, "That mobile VNUM can't exist.\r\n");
    return;
  }

  /* Check that whatever it is isn't already being edited. */
  for (d = descriptor_list; d; d = d->next) {
    if (STATE(d) == CON_MEDIT) {
      if (d->olc && OLC_NUM(d) == number) {
        send_to_char(ch, "That mobile is currently being edited by %s.\r\n",
          GET_NAME(d->character));
        return;
      }
    }
  }

  d = ch->desc;

  /* Give descriptor an OLC structure. */
  if (d->olc) {
    mudlog(BRF, LVL_IMMORT, TRUE,
      "SYSERR: do_oasis_medit: Player already had olc structure.");
    free(d->olc);
  }

  CREATE(d->olc, struct oasis_olc_data, 1);

  /* Find the zone. */
  OLC_ZNUM(d) = save ? real_zone(number) : real_zone_by_thing(number);
  if (OLC_ZNUM(d) == NOWHERE) {
    if (save) {
      send_to_char(ch, "Zone %d does not exist.\r\n", number);
    } else if (real_mobile(number) == NOBODY) {
      send_to_char(ch, "Mobile vnum %d does not exist and no zone owns that vnum.\r\n", number);
    } else {
      send_to_char(ch, "Mobile vnum %d exists but is not in any valid editable zone range.\r\n", number);
    }
    free(d->olc);
    d->olc = NULL;
    return;
  }

  /* Everyone but IMPLs can only edit zones they have been assigned. */
  if (!can_edit_zone(ch, OLC_ZNUM(d))) {
    send_cannot_edit(ch, zone_table[OLC_ZNUM(d)].number);
    /* Free the OLC structure. */
    free(d->olc);
    d->olc = NULL;
    return;
  }

  /* If save is TRUE, save the mobiles. */
  if (save) {
    send_to_char(ch, "Saving all mobiles in zone %d.\r\n",
      zone_table[OLC_ZNUM(d)].number);
    mudlog(CMP, MAX(LVL_BUILDER, GET_INVIS_LEV(ch)), TRUE,
      "OLC: %s saves mobile info for zone %d.",
      GET_NAME(ch), zone_table[OLC_ZNUM(d)].number);

    /* Save the mobiles. */
    save_mobiles(OLC_ZNUM(d));

    /* Free the olc structure stored in the descriptor. */
    free(d->olc);
    d->olc = NULL;
    return;
  }

  OLC_NUM(d) = number;

  /* If this is a new mobile, setup a new one, otherwise, setup the
     existing mobile. */
  if ((real_num = real_mobile(number)) == NOBODY)
    medit_setup_new(d);
  else
    medit_setup_existing(d, real_num);

  medit_disp_menu(d);
  STATE(d) = CON_MEDIT;

  /* Display the OLC messages to the players in the same room as the
     builder and also log it. */
  act("$n starts using OLC.", TRUE, d->character, 0, 0, TO_ROOM);
  SET_BIT_AR(PLR_FLAGS(ch), PLR_WRITING);

  mudlog(CMP, MAX(LVL_IMMORT, GET_INVIS_LEV(ch)), TRUE,"OLC: %s starts editing zone %d allowed zone %d",
    GET_NAME(ch), zone_table[OLC_ZNUM(d)].number, GET_OLC_ZONE(ch));
}

static void medit_save_to_disk(zone_rnum zone_num)
{
  save_mobiles(zone_num);
}

static void medit_setup_new(struct descriptor_data *d)
{
  struct char_data *mob;

  /* Allocate a scratch mobile structure. */
  CREATE(mob, struct char_data, 1);

  init_mobile(mob);

  GET_MOB_RNUM(mob) = NOBODY;
  /* Set up some default strings. */
  GET_ALIAS(mob) = strdup("mob unfinished");
  GET_SDESC(mob) = strdup("the unfinished mob");
  GET_LDESC(mob) = strdup("An unfinished mob stands here.\r\n");
  GET_DDESC(mob) = strdup("It looks unfinished.\r\n");
  SCRIPT(mob) = NULL;
  mob->proto_script = OLC_SCRIPT(d) = NULL;

  OLC_MOB(d) = mob;
  /* Has changed flag. (It hasn't so far, we just made it.) */
  OLC_VAL(d) = FALSE;
  OLC_ITEM_TYPE(d) = MOB_TRIGGER;
}

void medit_setup_existing(struct descriptor_data *d, int rmob_num)
{
  struct char_data *mob;

  /* Allocate a scratch mobile structure. */
  CREATE(mob, struct char_data, 1);

  copy_mobile(mob, mob_proto + rmob_num);

  OLC_MOB(d) = mob;
  OLC_ITEM_TYPE(d) = MOB_TRIGGER;
  dg_olc_script_copy(d);
  /*
   * The edited mob must not have a script.
   * It will be assigned to the updated mob later, after editing.
   */
  SCRIPT(mob) = NULL;
  OLC_MOB(d)->proto_script = NULL;
}

/* Ideally, this function should be in db.c, but I'll put it here for portability. */
static void init_mobile(struct char_data *mob)
{
  clear_char(mob);

  GET_HIT(mob) = GET_MANA(mob) = 1;
  GET_MAX_MANA(mob) = GET_MAX_MOVE(mob) = 100;
  GET_NDD(mob) = GET_SDD(mob) = 1;
  GET_WEIGHT(mob) = 200;
  GET_HEIGHT(mob) = 198;
  GET_PET_PRICE(mob) = 0;

  mob->real_abils.str = mob->real_abils.intel = mob->real_abils.wis = 11;
  mob->real_abils.dex = mob->real_abils.con = mob->real_abils.cha = 11;
  mob->aff_abils = mob->real_abils;

  GET_SAVE(mob, SAVING_PARA)   = 0;
  GET_SAVE(mob, SAVING_ROD)    = 0;
  GET_SAVE(mob, SAVING_PETRI)  = 0;
  GET_SAVE(mob, SAVING_BREATH) = 0;
  GET_SAVE(mob, SAVING_SPELL)  = 0;

  SET_BIT_AR(MOB_FLAGS(mob), MOB_ISNPC);
  mob->player_specials = &dummy_mob;
}

/* Save new/edited mob to memory. */
void medit_save_internally(struct descriptor_data *d)
{
  int i;
  mob_rnum new_rnum;
  struct descriptor_data *dsc;
  struct char_data *mob;

  i = (real_mobile(OLC_NUM(d)) == NOBODY);

  if ((new_rnum = add_mobile(OLC_MOB(d), OLC_NUM(d))) == NOBODY) {
    log("medit_save_internally: add_mobile failed.");
    return;
  }


  /* Update triggers and free old proto list */
  if (mob_proto[new_rnum].proto_script &&
      mob_proto[new_rnum].proto_script != OLC_SCRIPT(d))
    free_proto_script(&mob_proto[new_rnum], MOB_TRIGGER);

  mob_proto[new_rnum].proto_script = OLC_SCRIPT(d);

  /* this takes care of the mobs currently in-game */
  for (mob = character_list; mob; mob = mob->next) {
    if (GET_MOB_RNUM(mob) != new_rnum)
      continue;

    /* remove any old scripts */
    if (SCRIPT(mob))
      extract_script(mob, MOB_TRIGGER);

    free_proto_script(mob, MOB_TRIGGER);
    copy_proto_script(&mob_proto[new_rnum], mob, MOB_TRIGGER);
    assign_triggers(mob, MOB_TRIGGER);
  }
  /* end trigger update */

  if (!i)	/* Only renumber on new mobiles. */
    return;

  /* Update keepers in shops being edited and other mobs being edited. */
  for (dsc = descriptor_list; dsc; dsc = dsc->next) {
    if (STATE(dsc) == CON_SEDIT)
      S_KEEPER(OLC_SHOP(dsc)) += (S_KEEPER(OLC_SHOP(dsc)) != NOTHING && S_KEEPER(OLC_SHOP(dsc)) >= new_rnum);
    else if (STATE(dsc) == CON_MEDIT)
      GET_MOB_RNUM(OLC_MOB(dsc)) += (GET_MOB_RNUM(OLC_MOB(dsc)) != NOTHING && GET_MOB_RNUM(OLC_MOB(dsc)) >= new_rnum);
  }

  /* Update other people in zedit too. From: C.Raehl 4/27/99 */
  for (dsc = descriptor_list; dsc; dsc = dsc->next)
    if (STATE(dsc) == CON_ZEDIT)
      for (i = 0; OLC_ZONE(dsc)->cmd[i].command != 'S'; i++)
        if (OLC_ZONE(dsc)->cmd[i].command == 'M')
          if (OLC_ZONE(dsc)->cmd[i].arg1 >= new_rnum)
            OLC_ZONE(dsc)->cmd[i].arg1++;
}

/* Menu functions
   Display positions. (sitting, standing, etc) */
static void medit_disp_positions(struct descriptor_data *d)
{
  get_char_colors(d->character);
  clear_screen(d);
  column_list(d->character, 0, position_types, NUM_POSITIONS, TRUE);
  write_to_output(d, "Enter position number : ");
}

/* Display the gender of the mobile. */
static void medit_disp_sex(struct descriptor_data *d)
{
  get_char_colors(d->character);
  clear_screen(d);
  column_list(d->character, 0, genders, NUM_GENDERS, TRUE);
  write_to_output(d, "Enter gender number : ");
}

/* Display attack types menu. */
static void medit_disp_attack_types(struct descriptor_data *d)
{
  int i;

  get_char_colors(d->character);
  clear_screen(d);

  for (i = 0; i < NUM_ATTACK_TYPES; i++) {
    write_to_output(d, "%s%2d%s) %s\r\n", grn, i, nrm, attack_hit_text[i].singular);
  }
  write_to_output(d, "Enter attack type : ");
}

/* Find mob flags that shouldn't be set by builders */
static bool medit_illegal_mob_flag(int fl)
{
  int i;

  /* add any other flags you dont want them setting */
  const int illegal_flags[] = {
    MOB_ISNPC,
    MOB_NOTDEADYET,
  };

  const int num_illegal_flags = sizeof(illegal_flags)/sizeof(int);


  for (i=0; i < num_illegal_flags;i++)
    if (fl == illegal_flags[i])
      return (TRUE);

  return (FALSE);

}

/* Due to illegal mob flags not showing in the mob flags list,
   we need this to convert the list number back to flag value */
static int medit_get_mob_flag_by_number(int num)
{
  int i, count = 0;
  for (i = 0; i < NUM_MOB_FLAGS; i++) {
    if (medit_illegal_mob_flag(i)) continue;
    if ((++count) == num) return i;
  }
  /* Return 'illegal flag' value */
  return -1;
}

/* Display mob-flags menu. */
static void medit_disp_mob_flags(struct descriptor_data *d)
{
  int i, count = 0, columns = 0;
  char flags[MAX_STRING_LENGTH];

  get_char_colors(d->character);
  clear_screen(d);

  /* Mob flags has special handling to remove illegal flags from the list */
  for (i = 0; i < NUM_MOB_FLAGS; i++) {
    if (medit_illegal_mob_flag(i)) continue;
    write_to_output(d, "%s%2d%s) %-20.20s  %s", grn, ++count, nrm, action_bits[i],
                !(++columns % 2) ? "\r\n" : "");
  }

  sprintbitarray(MOB_FLAGS(OLC_MOB(d)), action_bits, AF_ARRAY_MAX, flags);
  write_to_output(d, "\r\nCurrent flags : %s%s%s\r\nEnter mob flags (0 to quit) : ", cyn, flags, nrm);
}

/* Display affection flags menu. */
static void medit_disp_aff_flags(struct descriptor_data *d)
{
  char flags[MAX_STRING_LENGTH];

  get_char_colors(d->character);
  clear_screen(d);
  /* +1/-1 antics needed because AFF_FLAGS doesn't start at 0. */
  column_list(d->character, 0, affected_bits + 1, NUM_AFF_FLAGS - 1, TRUE);
  sprintbitarray(AFF_FLAGS(OLC_MOB(d)), affected_bits, AF_ARRAY_MAX, flags);
  write_to_output(d, "\r\nCurrent flags   : %s%s%s\r\nEnter aff flags (0 to quit) : ",
                          cyn, flags, nrm);
}

static int medit_slot_required_wear_flag(int wear_pos)
{
  switch (wear_pos) {
    case WEAR_LIGHT:    return ITEM_WEAR_TAKE;
    case WEAR_FINGER_R:
    case WEAR_FINGER_L: return ITEM_WEAR_FINGER;
    case WEAR_NECK_1:   return ITEM_WEAR_NECK;
    case WEAR_BODY:     return ITEM_WEAR_BODY;
    case WEAR_HEAD:     return ITEM_WEAR_HEAD;
    case WEAR_EYES:     return ITEM_WEAR_EYES;
    case WEAR_EAR_L:
    case WEAR_EAR_R:    return ITEM_WEAR_EAR;
    case WEAR_LEGS:     return ITEM_WEAR_LEGS;
    case WEAR_FEET:     return ITEM_WEAR_FEET;
    case WEAR_HANDS:    return ITEM_WEAR_HANDS;
    case WEAR_ARMS:     return ITEM_WEAR_ARMS;
    case WEAR_SHIELD:   return ITEM_WEAR_SHIELD;
    case WEAR_ABOUT:    return ITEM_WEAR_ABOUT;
    case WEAR_WAIST:    return ITEM_WEAR_WAIST;
    case WEAR_WRIST_R:
    case WEAR_WRIST_L:  return ITEM_WEAR_WRIST;
    case WEAR_WIELD:    return ITEM_WEAR_WIELD;
    case WEAR_HOLD:     return ITEM_WEAR_TAKE;
    default:            return ITEM_WEAR_TAKE;
  }
}

static int medit_parse_int_argument(const char *arg, int *value)
{
  char *endptr = NULL;
  long parsed;

  if (!arg || !*arg)
    return FALSE;

  while (*arg && isspace((unsigned char)*arg))
    arg++;
  if (!*arg)
    return FALSE;

  parsed = strtol(arg, &endptr, 10);
  while (endptr && *endptr && isspace((unsigned char)*endptr))
    endptr++;
  if (endptr == arg || (endptr && *endptr != '\0'))
    return FALSE;

  *value = (int)parsed;
  return TRUE;
}

static int medit_arg_is_cancel(const char *arg)
{
  const char *p;

  if (!arg)
    return FALSE;
  while (*arg && isspace((unsigned char)*arg))
    arg++;
  if (*arg != 'q' && *arg != 'Q')
    return FALSE;
  p = arg + 1;
  while (*p && isspace((unsigned char)*p))
    p++;
  if (*p != '\0')
    return FALSE;
  return TRUE;
}

static const char *medit_slot_label_by_wear_pos(int wear_pos)
{
  int i;

  for (i = 0; i < (int)(sizeof(medit_eq_picker_slots) / sizeof(medit_eq_picker_slots[0])); i++)
    if (medit_eq_picker_slots[i] == wear_pos)
      return medit_eq_picker_labels[i];

  return "Unknown";
}

static int medit_slot_from_picker_choice(int choice)
{
  int idx = choice - 1;
  if (idx < 0 || idx >= (int)(sizeof(medit_eq_picker_slots) / sizeof(medit_eq_picker_slots[0])))
    return -1;
  return medit_eq_picker_slots[idx];
}

static void medit_disp_slot_picker(struct descriptor_data *d, const char *title, const char *prompt)
{
  int i;

  write_to_output(d, "%s\r\n", title);
  for (i = 0; i < (int)(sizeof(medit_eq_picker_slots) / sizeof(medit_eq_picker_slots[0])); i++)
    write_to_output(d, "%2d) %s\r\n", i + 1, medit_eq_picker_labels[i]);
  write_to_output(d, " Q) Cancel\r\n%s", prompt);
}

static const char *medit_required_wear_flag_desc(int wear_pos)
{
  switch (wear_pos) {
    case WEAR_HEAD:     return "wearable on head";
    case WEAR_NECK_1:   return "wearable around neck";
    case WEAR_ABOUT:    return "wearable on back/about body";
    case WEAR_EYES:     return "wearable on eyes";
    case WEAR_EAR_L:
    case WEAR_EAR_R:    return "wearable on ear";
    case WEAR_BODY:     return "wearable on body";
    case WEAR_ARMS:     return "wearable on arms";
    case WEAR_WRIST_R:
    case WEAR_WRIST_L:  return "wearable on wrist";
    case WEAR_HANDS:    return "wearable on hands";
    case WEAR_FINGER_R:
    case WEAR_FINGER_L: return "wearable on finger";
    case WEAR_WAIST:    return "wearable around waist";
    case WEAR_LEGS:     return "wearable on legs";
    case WEAR_FEET:     return "wearable on feet";
    case WEAR_WIELD:    return "wieldable";
    case WEAR_SHIELD:   return "wearable as shield";
    case WEAR_LIGHT:    return "takeable (light slot uses held/takeable items)";
    case WEAR_HOLD:
      return "holdable/takeable (or an offhand weapon)";
    default:
      return "wearable in that slot";
  }
}

static void medit_disp_remove_inventory_picker(struct descriptor_data *d)
{
  struct char_data *mob = OLC_MOB(d);
  int i;

  write_to_output(d, "Remove inventory item (choose visible list index):\r\n");
  for (i = 0; i < mob->mob_specials.inventory_loadout_count; i++) {
    obj_rnum ornum = real_object(mob->mob_specials.inventory_loadout[i].vnum);
    const char *sdesc = (ornum != NOTHING) ? obj_proto[ornum].short_description : "<missing object>";
    int count = MAX(1, mob->mob_specials.inventory_loadout[i].count);
    if (count > 1)
      write_to_output(d, "%2d) [%d] %s x%d\r\n", i + 1, mob->mob_specials.inventory_loadout[i].vnum, sdesc, count);
    else
      write_to_output(d, "%2d) [%d] %s\r\n", i + 1, mob->mob_specials.inventory_loadout[i].vnum, sdesc);
  }
  write_to_output(d, " Q) Cancel\r\nEnter visible list index to remove: ");
}

static void medit_disp_remove_loot_picker(struct descriptor_data *d)
{
  struct char_data *mob = OLC_MOB(d);
  int i;

  write_to_output(d, "Remove loot item (choose visible list index):\r\n");
  for (i = 0; i < mob->mob_specials.loot_table_count; i++) {
    obj_rnum ornum = real_object(mob->mob_specials.loot_table[i].vnum);
    const char *sdesc = (ornum != NOTHING) ? obj_proto[ornum].short_description : "<missing object>";
    write_to_output(d, "%2d) [%d] %-30s %3d%%\r\n", i + 1, mob->mob_specials.loot_table[i].vnum, sdesc, mob->mob_specials.loot_table[i].chance);
  }
  write_to_output(d, " Q) Cancel\r\nEnter visible list index to remove: ");
}

static int medit_object_can_equip_slot(struct obj_data *obj, int wear_pos)
{
  if (!obj || wear_pos < 0 || wear_pos >= NUM_WEARS)
    return FALSE;

  if (wear_pos == WEAR_HOLD && GET_OBJ_TYPE(obj) == ITEM_WEAPON && OBJ_FLAGGED(obj, ITEM_OFFHAND))
    return TRUE;

  return CAN_WEAR(obj, medit_slot_required_wear_flag(wear_pos));
}

static void medit_disp_loadout_menu(struct descriptor_data *d)
{
  struct char_data *mob = OLC_MOB(d);
  int i, j;

  get_char_colors(d->character);
  clear_screen(d);
  write_to_output(d, "-- LOADOUT / LOOT: [%d] %s\r\n\r\n", OLC_NUM(d), GET_SDESC(mob));

  write_to_output(d, "EQUIPPED ITEMS\r\n%s is using:\r\n", GET_SDESC(mob));
  for (i = 0; i < (int)(sizeof(medit_eq_picker_slots) / sizeof(medit_eq_picker_slots[0])); i++) {
    int slot = medit_eq_picker_slots[i];
    int found_idx = -1;
    for (j = 0; j < mob->mob_specials.equip_loadout_count; j++) {
      if (mob->mob_specials.equip_loadout[j].wear_pos == slot) {
        found_idx = j;
        break;
      }
    }

    if (found_idx < 0) {
      write_to_output(d, "%-14s [NOTHING]\r\n", medit_eq_picker_labels[i]);
    } else {
      obj_rnum ornum = real_object(mob->mob_specials.equip_loadout[found_idx].vnum);
      const char *sdesc = (ornum != NOTHING) ? obj_proto[ornum].short_description : "<missing object>";
      write_to_output(d, "%-14s [%d] %s\r\n",
        medit_eq_picker_labels[i], mob->mob_specials.equip_loadout[found_idx].vnum, sdesc);
    }
  }

  write_to_output(d, "\r\nINVENTORY ITEMS\r\n");
  if (mob->mob_specials.inventory_loadout_count <= 0)
    write_to_output(d, "  [NONE]\r\n");
  for (i = 0; i < mob->mob_specials.inventory_loadout_count; i++) {
    obj_rnum ornum = real_object(mob->mob_specials.inventory_loadout[i].vnum);
    const char *sdesc = (ornum != NOTHING) ? obj_proto[ornum].short_description : "<missing object>";
    int count = MAX(1, mob->mob_specials.inventory_loadout[i].count);
    if (count > 1)
      write_to_output(d, "  %2d) [%d] %s x%d\r\n", i + 1,
        mob->mob_specials.inventory_loadout[i].vnum, sdesc, count);
    else
      write_to_output(d, "  %2d) [%d] %s\r\n", i + 1,
        mob->mob_specials.inventory_loadout[i].vnum, sdesc);
  }

  write_to_output(d, "\r\nLOOT TABLE\r\n");
  if (mob->mob_specials.loot_table_count <= 0)
    write_to_output(d, "  [NONE]\r\n");
  for (i = 0; i < mob->mob_specials.loot_table_count; i++) {
    obj_rnum ornum = real_object(mob->mob_specials.loot_table[i].vnum);
    const char *sdesc = (ornum != NOTHING) ? obj_proto[ornum].short_description : "<missing object>";
    write_to_output(d, "  %2d) [%d] %-30s %3d%%\r\n", i + 1,
      mob->mob_specials.loot_table[i].vnum, sdesc, mob->mob_specials.loot_table[i].chance);
  }

  write_to_output(d,
    "\r\nA) Equip object\r\n"
    "B) Add inventory item\r\n"
    "C) Add loot item\r\n"
    "D) Remove equipped item\r\n"
    "E) Remove inventory item\r\n"
    "F) Remove loot item\r\n"
    "Q) Quit\r\n"
    "Enter choice : ");
  OLC_MODE(d) = MEDIT_LOADOUT_MENU;
}

/* Display main menu. */
static void medit_disp_menu(struct descriptor_data *d)
{
  struct char_data *mob;
  char flags[MAX_STRING_LENGTH], flag2[MAX_STRING_LENGTH];
  char price_buf[MAX_INPUT_LENGTH];

  mob = OLC_MOB(d);
  get_char_colors(d->character);
  clear_screen(d);

  if (GET_PET_PRICE(mob) > 0)
    snprintf(price_buf, sizeof(price_buf), "%d", GET_PET_PRICE(mob));
  else
    strlcpy(price_buf, "(default)", sizeof(price_buf));

  write_to_output(d,
  "-- Mob Number:  [%s%d%s]\r\n"
  "%s1%s) Sex: %s%-7.7s%s	         %s2%s) Keywords: %s%s\r\n"
  "%s3%s) S-Desc: %s%s\r\n"
  "%s4%s) L-Desc:-\r\n%s%s\r\n"
  "%s5%s) D-Desc:-\r\n%s%s\r\n",

	  cyn, OLC_NUM(d), nrm,
	  grn, nrm, yel, genders[LIMIT((int)GET_SEX(mob), 0, NUM_GENDERS - 1)], nrm,
	  grn, nrm, yel, GET_ALIAS(mob),
	  grn, nrm, yel, GET_SDESC(mob),
	  grn, nrm, yel, GET_LDESC(mob),
	  grn, nrm, yel, GET_DDESC(mob)
	  );

  sprintbitarray(MOB_FLAGS(mob), action_bits, AF_ARRAY_MAX, flags);
  sprintbitarray(AFF_FLAGS(mob), affected_bits, AF_ARRAY_MAX, flag2);
  write_to_output(d,
          "%s6%s) Position  : %s%s\r\n"
          "%s7%s) Default   : %s%s\r\n"
          "%s8%s) Attack    : %s%s\r\n"
      "%s9%s) Stats Menu...\r\n"
          "%sA%s) NPC Flags : %s%s\r\n"
          "%sB%s) AFF Flags : %s%s\r\n"
          "%sP%s) Pet Price : %s%s\r\n"
          "%sR%s) Loadout / Loot\r\n"
          "%sS%s) Script    : %s%s\r\n"
          "%sU%s) Combat Abilities\r\n"
          "%sV%s) Event Reactions\r\n"
          "%sW%s) Copy mob\r\n"
          "%sX%s) Delete mob\r\n"
	  "%sQ%s) Quit\r\n"
	  "Enter choice : ",

          grn, nrm, yel, position_types[(int)GET_POS(mob)],
          grn, nrm, yel, position_types[(int)GET_DEFAULT_POS(mob)],
          grn, nrm, yel, attack_hit_text[(int)GET_ATTACK(mob)].singular,
          grn, nrm,
          grn, nrm, cyn, flags,
          grn, nrm, cyn, flag2,
          grn, nrm, yel, price_buf,
          grn, nrm,
          grn, nrm, cyn, OLC_SCRIPT(d) ?"Set.":"Not Set.",
          grn, nrm,
          grn, nrm,
          grn, nrm,
          grn, nrm,
	  grn, nrm
	  );

  OLC_MODE(d) = MEDIT_MAIN_MENU;
}

/* Display main menu. */
static void medit_disp_stats_menu(struct descriptor_data *d)
{
  struct char_data *mob;
  char title[MAX_STRING_LENGTH];
  int hp_min, hp_max;
  int dmg_min, dmg_max;
  int base_xp_preview, total_xp_preview;

  mob = OLC_MOB(d);
  get_char_colors(d->character);
  clear_screen(d);

  hp_min = GET_HIT(mob) + GET_MOVE(mob);
  hp_max = (GET_HIT(mob) * GET_MANA(mob)) + GET_MOVE(mob);
  dmg_min = GET_NDD(mob) + GET_DAMROLL(mob);
  dmg_max = (GET_NDD(mob) * GET_SDD(mob)) + GET_DAMROLL(mob);
  base_xp_preview = mob_kill_base_xp_for_levels(GET_LEVEL(mob), GET_LEVEL(mob));
  total_xp_preview = LIMIT(base_xp_preview + mob->mob_specials.bonus_xp_min, 0, MAX_MOB_EXP);
  snprintf(title, sizeof(title), "MOB BUILD: [%d] %s", OLC_NUM(d), GET_SDESC(mob));

  write_to_output(d,
  "-------------------------------------------------------------------------------\r\n"
  "%-79.79s\r\n"
  "-------------------------------------------------------------------------------\r\n"
  "QUICK BUILD\r\n"
  "(%s1%s) Level:                     %s[%s%5d%s]%s\r\n"
  "(%s2%s) Reapply Recommended Stats\r\n"
  "\r\n"
  "Tip: Set the level first.\r\n"
  "     After changing level, accept the Y/N prompt to fill recommended stats.\r\n"
  "     Use option 2 later if you want to refresh recommended values again.\r\n"
  "-------------------------------------------------------------------------------\r\n"
  "HIT POINTS\r\n"
  "(%s3%s) HP NumDice:                %s[%s%5d%s]%s\r\n"
  "(%s4%s) HP SizeDice:               %s[%s%5d%s]%s\r\n"
  "(%s5%s) HP Addition:               %s[%s%5d%s]%s\r\n"
  "    HP Preview:                %s[%s%5d%s to %s%5d%s]%s\r\n"
  "-------------------------------------------------------------------------------\r\n"
  "DAMAGE\r\n"
  "(%s6%s) BHD NumDice:               %s[%s%5d%s]%s\r\n"
  "(%s7%s) BHD SizeDice:              %s[%s%5d%s]%s\r\n"
  "(%s8%s) Damroll:                   %s[%s%5d%s]%s\r\n"
  "    Damage Preview:            %s[%s%5d%s to %s%5d%s]%s\r\n"
  "-------------------------------------------------------------------------------\r\n"
  "COMBAT\r\n"
  "(%sA%s) Armor:                     %s[%s%5d%s]%s\r\n"
  "(%sB%s) Hitroll:                   %s[%s%5d%s]%s\r\n"
  "(%sC%s) Evasion:                   %s[%s%5d%s]%s\r\n"
  "(%sD%s) Alignment:                 %s[%s%5d%s]%s\r\n"
  "(%sE%s) Wimpy Threshold:           %s[%s%5d%s]%s\r\n"
  "-------------------------------------------------------------------------------\r\n"
  "REWARDS\r\n"
  "(%sF%s) Bonus XP:                  %s[ %s%d%s / %s%d%s ]%s\r\n"
  "(%sG%s) Gold Min/Max:              %s[%s%5lld%s / %s%5lld%s]%s\r\n"
  "    Base XP Preview:           %s[%s%5d%s]%s\r\n"
  "    Total XP Preview:          %s[%s%5d%s]%s\r\n"
  "    Note: Bonus XP is added on top of live kill XP.\r\n"
  "    Note: Rare Kill bonus may add extra XP when few live copies exist.\r\n"
  "-------------------------------------------------------------------------------\r\n"
  "ATTRIBUTES\r\n"
  "(%sH%s) Str: %s[%s%2d/%3d%s]%s   (%sI%s) Int: %s[%s%2d%s]%s   (%sJ%s) Wis: %s[%s%2d%s]%s\r\n"
  "(%sK%s) Dex: %s[%s%2d%s]%s     (%sL%s) Con: %s[%s%2d%s]%s   (%sM%s) Cha: %s[%s%2d%s]%s\r\n"
  "-------------------------------------------------------------------------------\r\n"
  "SAVING THROWS\r\n"
  "(%sN%s) Paralysis:               %s[%s%5d%s]%s\r\n"
  "(%sO%s) Rods/Staves:             %s[%s%5d%s]%s\r\n"
  "(%sP%s) Petrification:           %s[%s%5d%s]%s\r\n"
  "(%sR%s) Breath:                  %s[%s%5d%s]%s\r\n"
  "(%sS%s) Spells:                  %s[%s%5d%s]%s\r\n"
  "-------------------------------------------------------------------------------\r\n"
  "(%sQ%s) Quit to main menu\r\n"
  "Enter choice : ",
      title,
      cyn, nrm, cyn, yel, GET_LEVEL(mob), cyn, nrm,
      cyn, nrm,
      cyn, nrm, cyn, yel, GET_HIT(mob), cyn, nrm,
      cyn, nrm, cyn, yel, GET_MANA(mob), cyn, nrm,
      cyn, nrm, cyn, yel, GET_MOVE(mob), cyn, nrm,
      cyn, yel, hp_min, cyn, yel, hp_max, cyn, nrm,
      cyn, nrm, cyn, yel, GET_NDD(mob), cyn, nrm,
      cyn, nrm, cyn, yel, GET_SDD(mob), cyn, nrm,
      cyn, nrm, cyn, yel, GET_DAMROLL(mob), cyn, nrm,
      cyn, yel, dmg_min, cyn, yel, dmg_max, cyn, nrm,
      cyn, nrm, cyn, yel, GET_AC(mob), cyn, nrm,
      cyn, nrm, cyn, yel, GET_HITROLL(mob), cyn, nrm,
      cyn, nrm, cyn, yel, GET_EVASION(mob), cyn, nrm,
      cyn, nrm, cyn, yel, GET_ALIGNMENT(mob), cyn, nrm,
      cyn, nrm, cyn, yel, GET_MOB_WIMP_LEV(mob), cyn, nrm,
      cyn, nrm, cyn, yel, mob->mob_specials.bonus_xp_min, cyn, yel, mob->mob_specials.bonus_xp_max, cyn, nrm,
      cyn, nrm, cyn, yel, (long long)mob->mob_specials.gold_min, cyn, yel, (long long)mob->mob_specials.gold_max, cyn, nrm,
      cyn, yel, base_xp_preview, cyn, nrm,
      cyn, yel, total_xp_preview, cyn, nrm,
      cyn, nrm, cyn, yel, GET_STR(mob), GET_ADD(mob), cyn, nrm,
      cyn, nrm, cyn, yel, GET_INT(mob), cyn, nrm,
      cyn, nrm, cyn, yel, GET_WIS(mob), cyn, nrm,
      cyn, nrm, cyn, yel, GET_DEX(mob), cyn, nrm,
      cyn, nrm, cyn, yel, GET_CON(mob), cyn, nrm,
      cyn, nrm, cyn, yel, GET_CHA(mob), cyn, nrm,
      cyn, nrm, cyn, yel, GET_SAVE(mob, SAVING_PARA), cyn, nrm,
      cyn, nrm, cyn, yel, GET_SAVE(mob, SAVING_ROD), cyn, nrm,
      cyn, nrm, cyn, yel, GET_SAVE(mob, SAVING_PETRI), cyn, nrm,
      cyn, nrm, cyn, yel, GET_SAVE(mob, SAVING_BREATH), cyn, nrm,
      cyn, nrm, cyn, yel, GET_SAVE(mob, SAVING_SPELL), cyn, nrm,
      cyn, nrm);

  OLC_MODE(d) = MEDIT_STATS_MENU;
}

static void medit_disp_combat_abilities_menu(struct descriptor_data *d)
{
  int i;
  struct char_data *mob = OLC_MOB(d);
  clear_screen(d);
  write_to_output(d, "Combat Abilities (%d/%d)\r\n", mob->mob_specials.combat_ability_count, MAX_MOB_COMBAT_ABILITIES);
  if (mob->mob_specials.combat_ability_count <= 0)
    write_to_output(d, "  [NONE]\r\n");
  for (i = 0; i < mob->mob_specials.combat_ability_count; i++) {
    struct mob_combat_ability *ab = &mob->mob_specials.combat_abilities[i];
    write_to_output(d, "%2d) [%s] %-5s %-20s target:%-14s mode:%-20s prio:%d\r\n",
      i + 1, ab->enabled ? "EN" : "DIS",
      mob_behavior_ability_type_name(ab->ability_type),
      (ab->ability_vnum > 0 ? skill_name(ab->ability_vnum) : "<unset>"),
      mob_behavior_target_name(ab->target_type),
      mob_behavior_trigger_mode_name(ab->trigger_mode),
      ab->priority);
  }
  write_to_output(d,
    "\r\nA) Add  E) Edit  D) Delete  T) Toggle  Q) Quit\r\nEnter choice: ");
  OLC_MODE(d) = MEDIT_COMBAT_ABILITIES_MENU;
}

static void medit_disp_combat_ability_field_menu(struct descriptor_data *d)
{
  int idx = OLC(d)->behavior_slot;
  struct mob_combat_ability *ab = &OLC_MOB(d)->mob_specials.combat_abilities[idx];
  write_to_output(d,
    "\r\nEditing combat ability slot %d\r\n"
    "1) Type: %s\r\n"
    "2) Spell/Skill: %s\r\n"
    "3) Target: %s\r\n"
    "4) Trigger mode: %s\r\n"
    "5) Round min: %d\r\n"
    "6) Round max: %d\r\n"
    "7) Cooldown rounds: %d\r\n"
    "8) Priority (lower first): %d\r\n"
    "9) Once per fight: %d\r\n"
    "A) Max uses per fight (attempt cap): %d\r\n"
    "B) Require target not affected: %d\r\n"
    "C) Require self not affected: %d\r\n"
    "D) Self hp %% max: %d\r\n"
    "E) Target hp %% max: %d\r\n"
    "Q) Back\r\nEnter field: ",
    idx + 1,
    mob_behavior_ability_type_name(ab->ability_type),
    (ab->ability_vnum > 0 ? skill_name(ab->ability_vnum) : "<unset>"),
    mob_behavior_target_name(ab->target_type),
    mob_behavior_trigger_mode_name(ab->trigger_mode),
    ab->round_min, ab->round_max, ab->cooldown_rounds, ab->priority,
    ab->once_per_fight, ab->max_uses_per_fight, ab->require_target_not_affected,
    ab->require_self_not_affected, ab->self_hp_pct_max, ab->target_hp_pct_max);
  OLC_MODE(d) = MEDIT_COMBAT_ABILITY_FIELD_MENU;
}

static void medit_disp_event_reactions_menu(struct descriptor_data *d)
{
  int i;
  struct char_data *mob = OLC_MOB(d);
  clear_screen(d);
  write_to_output(d, "Event Reactions (%d/%d)\r\n", mob->mob_specials.event_reaction_count, MAX_MOB_EVENT_REACTIONS);
  if (mob->mob_specials.event_reaction_count <= 0)
    write_to_output(d, "  [NONE]\r\n");
  for (i = 0; i < mob->mob_specials.event_reaction_count; i++) {
    struct mob_event_reaction *ev = &mob->mob_specials.event_reactions[i];
    write_to_output(d, "%2d) [%s] event:%-18s action:%-12s data:%-18.18s chance:%d cooldown:%d\r\n",
      i + 1, ev->enabled ? "EN" : "DIS",
      mob_behavior_event_type_name(ev->event_type),
      mob_behavior_event_action_name(ev->action_type),
      (ev->action_type == MOB_EVENT_ACTION_CAST_SPELL || ev->action_type == MOB_EVENT_ACTION_USE_SKILL) ?
        (ev->ability_vnum > 0 ? skill_name(ev->ability_vnum) : "<unset>") : ev->argument,
      ev->chance_percent, ev->cooldown_pulses);
  }
  write_to_output(d, "\r\nA) Add  E) Edit  D) Delete  T) Toggle  Q) Quit\r\nEnter choice: ");
  OLC_MODE(d) = MEDIT_EVENT_REACTIONS_MENU;
}

static void medit_disp_event_reaction_field_menu(struct descriptor_data *d)
{
  int idx = OLC(d)->behavior_slot;
  struct mob_event_reaction *ev = &OLC_MOB(d)->mob_specials.event_reactions[idx];
  write_to_output(d,
    "\r\nEditing event reaction slot %d\r\n"
    "1) Event type: %s\r\n"
    "2) Action type: %s\r\n"
    "3) Spell/skill id: %d (%s)\r\n"
    "4) Target: %s\r\n"
    "5) Cooldown pulses: %d\r\n"
    "6) Chance %%: %d\r\n"
    "7) Once per reset: %d\r\n"
    "8) HP threshold %% (low hp): %d\r\n"
    "9) Message/argument: %s\r\n"
    "Q) Back\r\nEnter field: ",
    idx + 1,
    mob_behavior_event_type_name(ev->event_type),
    mob_behavior_event_action_name(ev->action_type),
    ev->ability_vnum, (ev->ability_vnum > 0 ? skill_name(ev->ability_vnum) : "<unset>"),
    mob_behavior_target_name(ev->target_type),
    ev->cooldown_pulses, ev->chance_percent, ev->once_per_reset,
    ev->hp_pct_threshold, ev->argument);
  OLC_MODE(d) = MEDIT_EVENT_REACTION_FIELD_MENU;
}

static int medit_validate_combat_ability(struct descriptor_data *d, struct mob_combat_ability *ab)
{
  if (ab->ability_type == MOB_ABILITY_SPELL) {
    if (ab->ability_vnum <= 0 || !IS_SPELL(ab->ability_vnum)) {
      write_to_output(d, "Invalid spell selection.\r\n");
      return 0;
    }
  } else if (ab->ability_type == MOB_ABILITY_SKILL) {
    if (ab->ability_vnum <= 0 || !IS_SKILL(ab->ability_vnum)) {
      write_to_output(d, "Invalid skill selection.\r\n");
      return 0;
    }
    if (!mob_behavior_validate_skill(ab->ability_vnum)) {
      write_to_output(d, "Unsupported skill for native mob behavior (supported: bash, kick).\r\n");
      return 0;
    }
  } else {
    write_to_output(d, "Ability type must be spell or skill.\r\n");
    return 0;
  }

  if (ab->trigger_mode == MOB_TRIGGER_RANDOM_ROUND_WINDOW && ab->round_min > ab->round_max) {
    write_to_output(d, "round_min must be <= round_max.\r\n");
    return 0;
  }
  ab->round_min = MAX(1, ab->round_min);
  ab->round_max = MAX(ab->round_min, ab->round_max);
  ab->cooldown_rounds = MAX(0, ab->cooldown_rounds);
  ab->max_uses_per_fight = MAX(0, ab->max_uses_per_fight);
  ab->self_hp_pct_max = LIMIT(ab->self_hp_pct_max, 0, 100);
  ab->target_hp_pct_max = LIMIT(ab->target_hp_pct_max, 0, 100);
  return 1;
}

static int medit_validate_event_reaction(struct descriptor_data *d, struct mob_event_reaction *ev)
{
  ev->chance_percent = LIMIT(ev->chance_percent, 0, 100);
  ev->cooldown_pulses = MAX(0, ev->cooldown_pulses);
  ev->hp_pct_threshold = LIMIT(ev->hp_pct_threshold, 1, 100);

  if (ev->action_type == MOB_EVENT_ACTION_CAST_SPELL) {
    if (!IS_SPELL(ev->ability_vnum)) {
      write_to_output(d, "Invalid spell for event reaction.\r\n");
      return 0;
    }
  } else if (ev->action_type == MOB_EVENT_ACTION_USE_SKILL) {
    if (!IS_SKILL(ev->ability_vnum) || !mob_behavior_validate_skill(ev->ability_vnum)) {
      write_to_output(d, "Invalid/unsupported skill for event reaction.\r\n");
      return 0;
    }
  }

  if ((ev->action_type == MOB_EVENT_ACTION_SAY_TEXT || ev->action_type == MOB_EVENT_ACTION_EMOTE_TEXT) &&
      !*ev->argument) {
    write_to_output(d, "Text argument cannot be empty.\r\n");
    return 0;
  }
  return 1;
}

void medit_parse(struct descriptor_data *d, char *arg)
{
  int i = -1, j;
  char *oldtext = NULL;

  if (OLC_MODE(d) == MEDIT_STATS_MENU ||
      OLC_MODE(d) == MEDIT_GOLD ||
      OLC_MODE(d) == MEDIT_BONUS_XP ||
      OLC_MODE(d) == MEDIT_LEVEL_AUTOFILL_CONFIRM ||
      OLC_MODE(d) == MEDIT_DELETE) {
    if (!genolc_checkstring(d, arg))
      return;
  } else if (OLC_MODE(d) > MEDIT_NUMERICAL_RESPONSE &&
             OLC_MODE(d) != MEDIT_LOADOUT_MENU &&
             OLC_MODE(d) != MEDIT_LOADOUT_EQUIP_VNUM &&
             OLC_MODE(d) != MEDIT_LOADOUT_EQUIP_SLOT &&
             OLC_MODE(d) != MEDIT_LOADOUT_EQUIP_REPLACE &&
             OLC_MODE(d) != MEDIT_LOADOUT_INV_VNUM &&
             OLC_MODE(d) != MEDIT_LOADOUT_INV_COUNT &&
             OLC_MODE(d) != MEDIT_LOADOUT_LOOT_VNUM &&
             OLC_MODE(d) != MEDIT_LOADOUT_LOOT_CHANCE &&
             OLC_MODE(d) != MEDIT_LOADOUT_REMOVE_EQUIP &&
             OLC_MODE(d) != MEDIT_LOADOUT_REMOVE_INV &&
             OLC_MODE(d) != MEDIT_LOADOUT_REMOVE_LOOT &&
             OLC_MODE(d) != MEDIT_COMBAT_ABILITIES_MENU &&
             OLC_MODE(d) != MEDIT_COMBAT_ABILITY_FIELD_MENU &&
             OLC_MODE(d) != MEDIT_COMBAT_ABILITY_FIELD_VALUE &&
             OLC_MODE(d) != MEDIT_EVENT_REACTIONS_MENU &&
             OLC_MODE(d) != MEDIT_EVENT_REACTION_FIELD_MENU &&
             OLC_MODE(d) != MEDIT_EVENT_REACTION_FIELD_VALUE) {
    char *endptr = NULL;
    long parsed;

    parsed = strtol(arg, &endptr, 10);
    while (endptr && *endptr && isspace((unsigned char)*endptr))
      endptr++;

    if (!*arg || endptr == arg || (endptr && *endptr != '\0')) {
      write_to_output(d, "Try again : ");
      return;
    }
    i = (int)parsed;
  } else {	/* String response. */
    if (!genolc_checkstring(d, arg))
      return;
  }
  switch (OLC_MODE(d)) {
  case MEDIT_CONFIRM_SAVESTRING:
    /* Ensure mob has MOB_ISNPC set. */
    SET_BIT_AR(MOB_FLAGS(OLC_MOB(d)), MOB_ISNPC);
    switch (*arg) {
    case 'y':
    case 'Y':
      /* Save the mob in memory and to disk. */
      medit_save_internally(d);
      mudlog(CMP, MAX(LVL_BUILDER, GET_INVIS_LEV(d->character)), TRUE, "OLC: %s edits mob %d", GET_NAME(d->character), OLC_NUM(d));
      if (CONFIG_OLC_SAVE) {
        medit_save_to_disk(OLC_ZNUM(d));
        write_to_output(d, "Mobile saved to disk.\r\n");
      } else
        write_to_output(d, "Mobile saved to memory.\r\n");
      cleanup_olc(d, CLEANUP_ALL);
      return;
    case 'n':
    case 'N':
      /* If not saving, we must free the script_proto list. We do so by
       * assigning it to the edited mob and letting free_mobile in
       * cleanup_olc handle it. */
      OLC_MOB(d)->proto_script = OLC_SCRIPT(d);
      cleanup_olc(d, CLEANUP_ALL);
      return;
    default:
      write_to_output(d, "Invalid choice!\r\n");
      write_to_output(d, "Do you wish to save your changes? : ");
      return;
    }

  case MEDIT_MAIN_MENU:
    i = 0;
    switch (*arg) {
    case 'q':
    case 'Q':
      if (OLC_VAL(d)) {	/* Anything been changed? */
	      write_to_output(d, "Do you wish to save your changes? : ");
	      OLC_MODE(d) = MEDIT_CONFIRM_SAVESTRING;
      } else
	cleanup_olc(d, CLEANUP_ALL);
      return;
    case '1':
      OLC_MODE(d) = MEDIT_SEX;
      medit_disp_sex(d);
      return;
    case '2':
      OLC_MODE(d) = MEDIT_KEYWORD;
      i--;
      break;
    case '3':
      OLC_MODE(d) = MEDIT_S_DESC;
      i--;
      break;
    case '4':
      OLC_MODE(d) = MEDIT_L_DESC;
      i--;
      break;
    case '5':
      OLC_MODE(d) = MEDIT_D_DESC;
      send_editor_help(d);
      write_to_output(d, "Enter mob description:\r\n\r\n");
      if (OLC_MOB(d)->player.description) {
	      write_to_output(d, "%s", OLC_MOB(d)->player.description);
	      oldtext = strdup(OLC_MOB(d)->player.description);
      }
      string_write(d, &OLC_MOB(d)->player.description, MAX_MOB_DESC, 0, oldtext);
      OLC_VAL(d) = 1;
      return;
    case '6':
      OLC_MODE(d) = MEDIT_POS;
      medit_disp_positions(d);
      return;
    case '7':
      OLC_MODE(d) = MEDIT_DEFAULT_POS;
      medit_disp_positions(d);
      return;
    case '8':
      OLC_MODE(d) = MEDIT_ATTACK;
      medit_disp_attack_types(d);
      return;
    case '9':
      OLC_MODE(d) = MEDIT_STATS_MENU;
      medit_disp_stats_menu(d);
      return;
    case 'a':
    case 'A':
      OLC_MODE(d) = MEDIT_NPC_FLAGS;
      medit_disp_mob_flags(d);
      return;
    case 'b':
    case 'B':
      OLC_MODE(d) = MEDIT_AFF_FLAGS;
      medit_disp_aff_flags(d);
      return;
    case 'p':
    case 'P':
      OLC_MODE(d) = MEDIT_PET_PRICE;
      write_to_output(d, "Enter pet price in gold (0 = automatic): ");
      return;
    case 'r':
    case 'R':
      medit_disp_loadout_menu(d);
      return;
    case 'w':
    case 'W':
      write_to_output(d, "Copy what mob? ");
      OLC_MODE(d) = MEDIT_COPY;
      return;
    case 'x':
    case 'X':
      write_to_output(d, "Are you sure you want to delete this mobile? ");
      OLC_MODE(d) = MEDIT_DELETE;
      return;
    case 's':
    case 'S':
      OLC_SCRIPT_EDIT_MODE(d) = SCRIPT_MAIN_MENU;
      dg_script_menu(d);
      return;
    case 'u':
    case 'U':
      medit_disp_combat_abilities_menu(d);
      return;
    case 'v':
    case 'V':
      medit_disp_event_reactions_menu(d);
      return;
    default:
      medit_disp_menu(d);
      return;
    }
    if (i == 0)
      break;
    else if (i == 1)
      write_to_output(d, "\r\nEnter new value : ");
    else if (i == -1)
      write_to_output(d, "\r\nEnter new text :\r\n] ");
    else
      write_to_output(d, "Oops...\r\n");
    return;

  case MEDIT_STATS_MENU:
    i=0;
    switch(*arg) {
    case 'q':
    case 'Q':
      medit_disp_menu(d);
      return;
    case '1':  /* Edit level */
      OLC_MODE(d) = MEDIT_LEVEL;
      i++;
      break;
    case '2':  /* Autoroll stats */
      medit_autoroll_stats(d);
      medit_disp_stats_menu(d);
      OLC_VAL(d) = TRUE;
      return;
    case '3':
      OLC_MODE(d) = MEDIT_NUM_HP_DICE;
      i++;
      break;
    case '4':
      OLC_MODE(d) = MEDIT_SIZE_HP_DICE;
      i++;
      break;
    case '5':
      OLC_MODE(d) = MEDIT_ADD_HP;
      i++;
      break;
    case '6':
      OLC_MODE(d) = MEDIT_NDD;
      i++;
      break;
    case '7':
      OLC_MODE(d) = MEDIT_SDD;
      i++;
      break;
    case '8':
      OLC_MODE(d) = MEDIT_DAMROLL;
      i++;
      break;
    case 'a':
    case 'A':
      OLC_MODE(d) = MEDIT_AC;
      i++;
      break;
    case 'b':
    case 'B':
      OLC_MODE(d) = MEDIT_HITROLL;
      i++;
      break;
    case 'd':
    case 'D':
      OLC_MODE(d) = MEDIT_ALIGNMENT;
      i++;
      break;
    case 'c':
    case 'C':
      OLC_MODE(d) = MEDIT_EVASION;
      i++;
      break;
    case 'e':
    case 'E':
      OLC_MODE(d) = MEDIT_WIMPY_THRESH;
      i++;
      break;
    case 'f':
    case 'F':
      OLC_MODE(d) = MEDIT_BONUS_XP;
      write_to_output(d, "Enter Bonus XP min and max (example: 7 13) or a single value: ");
      return;
    case 'g':
    case 'G':
      OLC_MODE(d) = MEDIT_GOLD;
      write_to_output(d, "Enter gold min and max (example: 10 50) or a single value: ");
      return;
    case 'h':
    case 'H':
      OLC_MODE(d) = MEDIT_STR;
      write_to_output(d, "\r\nEnter Strength base value [3-25]: ");
      return;
    case 'i':
    case 'I':
      OLC_MODE(d) = MEDIT_INT;
      i++;
      break;
    case 'j':
    case 'J':
      OLC_MODE(d) = MEDIT_WIS;
      i++;
      break;
    case 'k':
    case 'K':
      OLC_MODE(d) = MEDIT_DEX;
      i++;
      break;
    case 'l':
    case 'L':
      OLC_MODE(d) = MEDIT_CON;
      i++;
      break;
    case 'm':
    case 'M':
      OLC_MODE(d) = MEDIT_CHA;
      i++;
      break;
    case 'n':
    case 'N':
      OLC_MODE(d) = MEDIT_PARA;
      i++;
      break;
    case 'o':
    case 'O':
      OLC_MODE(d) = MEDIT_ROD;
      i++;
      break;
    case 'p':
    case 'P':
      OLC_MODE(d) = MEDIT_PETRI;
      i++;
      break;
    case 'r':
    case 'R':
      OLC_MODE(d) = MEDIT_BREATH;
      i++;
      break;
    case 's':
    case 'S':
      OLC_MODE(d) = MEDIT_SPELL;
      i++;
      break;
    default:
      medit_disp_stats_menu(d);
      return;
    }
    if (i == 0)
      break;
    else if (i == 1)
      write_to_output(d, "\r\nEnter new value : ");
    else if (i == -1)
      write_to_output(d, "\r\nEnter new text :\r\n] ");
    else
      write_to_output(d, "Oops...\r\n");
    return;

  case OLC_SCRIPT_EDIT:
    if (dg_script_edit_parse(d, arg)) return;
    break;

  case MEDIT_KEYWORD:
    smash_tilde(arg);
    if (GET_ALIAS(OLC_MOB(d)))
      free(GET_ALIAS(OLC_MOB(d)));
    GET_ALIAS(OLC_MOB(d)) = str_udup(arg);
    break;

  case MEDIT_S_DESC:
    smash_tilde(arg);
    if (GET_SDESC(OLC_MOB(d)))
      free(GET_SDESC(OLC_MOB(d)));
    GET_SDESC(OLC_MOB(d)) = str_udup(arg);
    break;

  case MEDIT_L_DESC:
    smash_tilde(arg);
    if (GET_LDESC(OLC_MOB(d)))
      free(GET_LDESC(OLC_MOB(d)));
    if (arg && *arg) {
      char buf[MAX_INPUT_LENGTH];
      snprintf(buf, sizeof(buf), "%s\r\n", arg);
      GET_LDESC(OLC_MOB(d)) = strdup(buf);
    } else
      GET_LDESC(OLC_MOB(d)) = strdup("undefined");

    break;

  case MEDIT_D_DESC:
    /*
     * We should never get here.
     */
    cleanup_olc(d, CLEANUP_ALL);
    mudlog(BRF, LVL_BUILDER, TRUE, "SYSERR: OLC: medit_parse(): Reached D_DESC case!");
    write_to_output(d, "Oops...\r\n");
    break;

  case MEDIT_NPC_FLAGS:
    if ((i = atoi(arg)) <= 0)
      break;
    else if ( (j = medit_get_mob_flag_by_number(i)) == -1) {
       write_to_output(d, "Invalid choice!\r\n");
       write_to_output(d, "Enter mob flags (0 to quit) :");
       return;
    } else if (j <= NUM_MOB_FLAGS) {
      TOGGLE_BIT_AR(MOB_FLAGS(OLC_MOB(d)), (j));
    }
    medit_disp_mob_flags(d);
    return;

  case MEDIT_AFF_FLAGS:
    if ((i = atoi(arg)) <= 0)
      break;
    else if (i < NUM_AFF_FLAGS)
      TOGGLE_BIT_AR(AFF_FLAGS(OLC_MOB(d)), i);

    /* Remove unwanted bits right away. */
    REMOVE_BIT_AR(AFF_FLAGS(OLC_MOB(d)), AFF_CHARM);
    REMOVE_BIT_AR(AFF_FLAGS(OLC_MOB(d)), AFF_POISON);
    REMOVE_BIT_AR(AFF_FLAGS(OLC_MOB(d)), AFF_SLEEP);
    medit_disp_aff_flags(d);
    return;

  case MEDIT_LOADOUT_MENU:
    switch (*arg) {
      case 'q':
      case 'Q':
        if (OLC_STORAGE(d)) {
          free(OLC_STORAGE(d));
          OLC_STORAGE(d) = NULL;
        }
        medit_disp_menu(d);
        return;
      case 'a':
      case 'A':
        OLC_MODE(d) = MEDIT_LOADOUT_EQUIP_VNUM;
        write_to_output(d, "Enter object vnum to equip (Q to cancel): ");
        return;
      case 'b':
      case 'B':
        OLC_MODE(d) = MEDIT_LOADOUT_INV_VNUM;
        write_to_output(d, "Enter object vnum to add to inventory (Q to cancel): ");
        return;
      case 'c':
      case 'C':
        OLC_MODE(d) = MEDIT_LOADOUT_LOOT_VNUM;
        write_to_output(d, "Enter object vnum to add to loot table (Q to cancel): ");
        return;
      case 'd':
      case 'D':
      {
        struct char_data *mob = OLC_MOB(d);
        OLC_MODE(d) = MEDIT_LOADOUT_REMOVE_EQUIP;
        if (mob->mob_specials.equip_loadout_count <= 0) {
          write_to_output(d, "There are no equipped items to remove.\r\n");
          medit_disp_loadout_menu(d);
          return;
        }
        medit_disp_slot_picker(d, "Choose equipped slot to remove:", "Enter visible slot choice to remove: ");
        return;
      }
      case 'e':
      case 'E':
      {
        struct char_data *mob = OLC_MOB(d);
        OLC_MODE(d) = MEDIT_LOADOUT_REMOVE_INV;
        if (mob->mob_specials.inventory_loadout_count <= 0) {
          write_to_output(d, "There are no inventory items to remove.\r\n");
          medit_disp_loadout_menu(d);
          return;
        }
        medit_disp_remove_inventory_picker(d);
        return;
      }
      case 'f':
      case 'F':
      {
        struct char_data *mob = OLC_MOB(d);
        OLC_MODE(d) = MEDIT_LOADOUT_REMOVE_LOOT;
        if (mob->mob_specials.loot_table_count <= 0) {
          write_to_output(d, "There are no loot entries to remove.\r\n");
          medit_disp_loadout_menu(d);
          return;
        }
        medit_disp_remove_loot_picker(d);
        return;
      }
      default:
        medit_disp_loadout_menu(d);
        return;
    }

  case MEDIT_LOADOUT_EQUIP_VNUM:
    if (medit_arg_is_cancel(arg)) {
      medit_disp_loadout_menu(d);
      return;
    }
    if (!medit_parse_int_argument(arg, &i)) {
      write_to_output(d, "Please enter an object vnum or Q to cancel: ");
      return;
    }
    if (real_object(i) == NOTHING) {
      write_to_output(d, "No object exists with vnum %d. Enter object vnum to equip (Q to cancel): ", i);
      return;
    }
    if (OLC_STORAGE(d))
      free(OLC_STORAGE(d));
    CREATE(OLC_STORAGE(d), char, 32);
    if (OLC_STORAGE(d))
      snprintf(OLC_STORAGE(d), 32, "%d", i);
    OLC_MODE(d) = MEDIT_LOADOUT_EQUIP_SLOT;
    medit_disp_slot_picker(d, "Choose equip slot:", "Enter visible slot choice (or Q to cancel): ");
    return;

  case MEDIT_LOADOUT_EQUIP_SLOT:
  {
    int slot, idx;
    obj_rnum ornum;
    struct obj_data *obj;
    struct char_data *mob = OLC_MOB(d);

    if (medit_arg_is_cancel(arg)) {
      if (OLC_STORAGE(d)) {
        free(OLC_STORAGE(d));
        OLC_STORAGE(d) = NULL;
      }
      medit_disp_loadout_menu(d);
      return;
    }

    if (!medit_parse_int_argument(arg, &i)) {
      write_to_output(d, "Please enter a visible slot choice number or Q to cancel: ");
      return;
    }

    slot = medit_slot_from_picker_choice(i);
    if (slot < 0) {
      write_to_output(d, "Invalid visible slot choice. Please enter a slot number shown above or Q to cancel: ");
      return;
    }

    ornum = real_object(OLC_STORAGE(d) ? atoi(OLC_STORAGE(d)) : NOTHING);
    if (ornum == NOTHING) {
      write_to_output(d, "Selected object no longer exists.\r\n");
      medit_disp_loadout_menu(d);
      return;
    }
    obj = &obj_proto[ornum];
    if (!medit_object_can_equip_slot(obj, slot)) {
      write_to_output(d,
        "Object [%d] %s cannot be equipped in %s because it lacks the required wear flag (%s).\r\n",
        obj_index[ornum].vnum, obj->short_description, medit_slot_label_by_wear_pos(slot),
        medit_required_wear_flag_desc(slot));
      if (OLC_STORAGE(d)) {
        free(OLC_STORAGE(d));
        OLC_STORAGE(d) = NULL;
      }
      medit_disp_loadout_menu(d);
      return;
    }

    for (idx = 0; idx < mob->mob_specials.equip_loadout_count; idx++) {
      if (mob->mob_specials.equip_loadout[idx].wear_pos == slot) {
        OLC_MODE(d) = MEDIT_LOADOUT_EQUIP_REPLACE;
        if (OLC_STORAGE(d))
          free(OLC_STORAGE(d));
        CREATE(OLC_STORAGE(d), char, 48);
        if (OLC_STORAGE(d))
          snprintf(OLC_STORAGE(d), 48, "%d %d", obj_index[ornum].vnum, slot);
        write_to_output(d, "Slot %s already contains [%d] %s. Replace it? (Y/N): ",
          medit_slot_label_by_wear_pos(slot),
          mob->mob_specials.equip_loadout[idx].vnum,
          real_object(mob->mob_specials.equip_loadout[idx].vnum) != NOTHING ?
            obj_proto[real_object(mob->mob_specials.equip_loadout[idx].vnum)].short_description :
            "<missing object>");
        return;
      }
    }

    if (mob->mob_specials.equip_loadout_count >= MAX_MOB_LOADOUT_ITEMS) {
      write_to_output(d, "Equip loadout is full (max %d entries).\r\n", MAX_MOB_LOADOUT_ITEMS);
      if (OLC_STORAGE(d)) {
        free(OLC_STORAGE(d));
        OLC_STORAGE(d) = NULL;
      }
      medit_disp_loadout_menu(d);
      return;
    }

    idx = mob->mob_specials.equip_loadout_count++;
    mob->mob_specials.equip_loadout[idx].vnum = obj_index[ornum].vnum;
    mob->mob_specials.equip_loadout[idx].wear_pos = slot;
    OLC_VAL(d) = TRUE;
    if (OLC_STORAGE(d)) {
      free(OLC_STORAGE(d));
      OLC_STORAGE(d) = NULL;
    }
    medit_disp_loadout_menu(d);
    return;
  }

  case MEDIT_LOADOUT_EQUIP_REPLACE:
  {
    int new_vnum = NOTHING, slot = -1;
    int idx = -1;
    struct char_data *mob = OLC_MOB(d);
    if (OLC_STORAGE(d))
      sscanf(OLC_STORAGE(d), "%d %d", &new_vnum, &slot);
    for (j = 0; j < mob->mob_specials.equip_loadout_count; j++) {
      if (mob->mob_specials.equip_loadout[j].wear_pos == slot) {
        idx = j;
        break;
      }
    }

    if ((*arg == 'y' || *arg == 'Y') &&
        idx >= 0 && idx < mob->mob_specials.equip_loadout_count) {
      mob->mob_specials.equip_loadout[idx].vnum = new_vnum;
      OLC_VAL(d) = TRUE;
    } else if (!(*arg == 'n' || *arg == 'N')) {
      write_to_output(d, "Please answer Y or N: ");
      return;
    }
    if (OLC_STORAGE(d)) {
      free(OLC_STORAGE(d));
      OLC_STORAGE(d) = NULL;
    }
    medit_disp_loadout_menu(d);
    return;
  }

  case MEDIT_LOADOUT_INV_VNUM:
    if (medit_arg_is_cancel(arg)) {
      medit_disp_loadout_menu(d);
      return;
    }
    if (!medit_parse_int_argument(arg, &i)) {
      write_to_output(d, "Please enter an object vnum or Q to cancel: ");
      return;
    }
    if (real_object(i) == NOTHING) {
      write_to_output(d, "No object exists with vnum %d. Enter object vnum to add to inventory (Q to cancel): ", i);
      return;
    }
    if (OLC_STORAGE(d))
      free(OLC_STORAGE(d));
    CREATE(OLC_STORAGE(d), char, 32);
    if (OLC_STORAGE(d))
      snprintf(OLC_STORAGE(d), 32, "%d", i);
    OLC_MODE(d) = MEDIT_LOADOUT_INV_COUNT;
    write_to_output(d, "Enter count (1+; Q to cancel): ");
    return;

  case MEDIT_LOADOUT_INV_COUNT:
  {
    struct char_data *mob = OLC_MOB(d);
    int idx;
    if (medit_arg_is_cancel(arg)) {
      if (OLC_STORAGE(d)) {
        free(OLC_STORAGE(d));
        OLC_STORAGE(d) = NULL;
      }
      medit_disp_loadout_menu(d);
      return;
    }
    if (!medit_parse_int_argument(arg, &i) || i <= 0) {
      write_to_output(d, "Invalid count. Enter a positive integer (1+) or Q to cancel: ");
      return;
    }
    if (mob->mob_specials.inventory_loadout_count >= MAX_MOB_LOADOUT_ITEMS) {
      write_to_output(d, "Inventory loadout is full (max %d entries).\r\n", MAX_MOB_LOADOUT_ITEMS);
      if (OLC_STORAGE(d)) {
        free(OLC_STORAGE(d));
        OLC_STORAGE(d) = NULL;
      }
      medit_disp_loadout_menu(d);
      return;
    }
    idx = mob->mob_specials.inventory_loadout_count++;
    mob->mob_specials.inventory_loadout[idx].vnum = OLC_STORAGE(d) ? atoi(OLC_STORAGE(d)) : NOTHING;
    mob->mob_specials.inventory_loadout[idx].count = i;
    OLC_VAL(d) = TRUE;
    if (OLC_STORAGE(d)) {
      free(OLC_STORAGE(d));
      OLC_STORAGE(d) = NULL;
    }
    medit_disp_loadout_menu(d);
    return;
  }

  case MEDIT_LOADOUT_LOOT_VNUM:
    if (medit_arg_is_cancel(arg)) {
      medit_disp_loadout_menu(d);
      return;
    }
    if (!medit_parse_int_argument(arg, &i)) {
      write_to_output(d, "Please enter an object vnum or Q to cancel: ");
      return;
    }
    if (real_object(i) == NOTHING) {
      write_to_output(d, "No object exists with vnum %d. Enter object vnum to add to loot table (Q to cancel): ", i);
      return;
    }
    if (OLC_STORAGE(d))
      free(OLC_STORAGE(d));
    CREATE(OLC_STORAGE(d), char, 32);
    if (OLC_STORAGE(d))
      snprintf(OLC_STORAGE(d), 32, "%d", i);
    OLC_MODE(d) = MEDIT_LOADOUT_LOOT_CHANCE;
    write_to_output(d, "Enter drop chance percent (1-100; Q to cancel): ");
    return;

  case MEDIT_LOADOUT_LOOT_CHANCE:
  {
    struct char_data *mob = OLC_MOB(d);
    int idx, target_vnum;
    if (medit_arg_is_cancel(arg)) {
      if (OLC_STORAGE(d)) {
        free(OLC_STORAGE(d));
        OLC_STORAGE(d) = NULL;
      }
      medit_disp_loadout_menu(d);
      return;
    }
    if (!medit_parse_int_argument(arg, &i) || i < 1 || i > 100) {
      write_to_output(d, "Invalid drop chance. Enter a percent from 1-100 or Q to cancel: ");
      return;
    }
    target_vnum = OLC_STORAGE(d) ? atoi(OLC_STORAGE(d)) : NOTHING;
    for (idx = 0; idx < mob->mob_specials.loot_table_count; idx++) {
      if (mob->mob_specials.loot_table[idx].vnum == target_vnum) {
        mob->mob_specials.loot_table[idx].chance = i;
        OLC_VAL(d) = TRUE;
        write_to_output(d, "Loot item [%d] already existed; updated its drop chance to %d%%.\r\n", target_vnum, i);
        if (OLC_STORAGE(d)) {
          free(OLC_STORAGE(d));
          OLC_STORAGE(d) = NULL;
        }
        medit_disp_loadout_menu(d);
        return;
      }
    }
    if (mob->mob_specials.loot_table_count >= MAX_MOB_LOOT_ITEMS) {
      write_to_output(d, "Loot table is full (max %d entries).\r\n", MAX_MOB_LOOT_ITEMS);
      if (OLC_STORAGE(d)) {
        free(OLC_STORAGE(d));
        OLC_STORAGE(d) = NULL;
      }
      medit_disp_loadout_menu(d);
      return;
    }
    idx = mob->mob_specials.loot_table_count++;
    mob->mob_specials.loot_table[idx].vnum = target_vnum;
    mob->mob_specials.loot_table[idx].chance = i;
    OLC_VAL(d) = TRUE;
    if (OLC_STORAGE(d)) {
      free(OLC_STORAGE(d));
      OLC_STORAGE(d) = NULL;
    }
    medit_disp_loadout_menu(d);
    return;
  }

  case MEDIT_LOADOUT_REMOVE_EQUIP:
  {
    struct char_data *mob = OLC_MOB(d);
    int slot, idx;

    if (medit_arg_is_cancel(arg)) {
      medit_disp_loadout_menu(d);
      return;
    }
    if (!medit_parse_int_argument(arg, &i)) {
      write_to_output(d, "Please enter a visible slot choice number or Q to cancel: ");
      return;
    }
    slot = medit_slot_from_picker_choice(i);
    if (slot < 0) {
      write_to_output(d, "Invalid visible slot choice. Please enter a slot number shown above or Q to cancel: ");
      return;
    }
    for (idx = 0; idx < mob->mob_specials.equip_loadout_count; idx++) {
      if (mob->mob_specials.equip_loadout[idx].wear_pos == slot) {
        for (; idx + 1 < mob->mob_specials.equip_loadout_count; idx++)
          mob->mob_specials.equip_loadout[idx] = mob->mob_specials.equip_loadout[idx + 1];
        mob->mob_specials.equip_loadout_count--;
        OLC_VAL(d) = TRUE;
        medit_disp_loadout_menu(d);
        return;
      }
    }
    write_to_output(d, "That slot is already empty.\r\n");
    medit_disp_loadout_menu(d);
    return;
  }

  case MEDIT_LOADOUT_REMOVE_INV:
  {
    struct char_data *mob = OLC_MOB(d);
    int idx;

    if (medit_arg_is_cancel(arg)) {
      medit_disp_loadout_menu(d);
      return;
    }
    if (!medit_parse_int_argument(arg, &i)) {
      write_to_output(d, "Please enter a visible list index number or Q to cancel: ");
      return;
    }
    idx = i - 1;
    if (idx < 0 || idx >= mob->mob_specials.inventory_loadout_count) {
      write_to_output(d, "Invalid visible list index. Enter a list index shown above or Q to cancel: ");
      return;
    }
    for (; idx + 1 < mob->mob_specials.inventory_loadout_count; idx++)
      mob->mob_specials.inventory_loadout[idx] = mob->mob_specials.inventory_loadout[idx + 1];
    mob->mob_specials.inventory_loadout_count--;
    OLC_VAL(d) = TRUE;
    medit_disp_loadout_menu(d);
    return;
  }

  case MEDIT_LOADOUT_REMOVE_LOOT:
  {
    struct char_data *mob = OLC_MOB(d);
    int idx;

    if (medit_arg_is_cancel(arg)) {
      medit_disp_loadout_menu(d);
      return;
    }
    if (!medit_parse_int_argument(arg, &i)) {
      write_to_output(d, "Please enter a visible list index number or Q to cancel: ");
      return;
    }
    idx = i - 1;
    if (idx < 0 || idx >= mob->mob_specials.loot_table_count) {
      write_to_output(d, "Invalid visible list index. Enter a list index shown above or Q to cancel: ");
      return;
    }
    for (; idx + 1 < mob->mob_specials.loot_table_count; idx++)
      mob->mob_specials.loot_table[idx] = mob->mob_specials.loot_table[idx + 1];
    mob->mob_specials.loot_table_count--;
    OLC_VAL(d) = TRUE;
    medit_disp_loadout_menu(d);
    return;

  case MEDIT_COMBAT_ABILITIES_MENU:
    switch (LOWER(*arg)) {
      case 'q': medit_disp_menu(d); return;
      case 'a':
        if (OLC_MOB(d)->mob_specials.combat_ability_count >= MAX_MOB_COMBAT_ABILITIES) {
          write_to_output(d, "No free combat ability slots.\r\n");
          medit_disp_combat_abilities_menu(d);
          return;
        }
        OLC(d)->behavior_slot = OLC_MOB(d)->mob_specials.combat_ability_count++;
        memset(&OLC_MOB(d)->mob_specials.combat_abilities[OLC(d)->behavior_slot], 0, sizeof(struct mob_combat_ability));
        OLC_MOB(d)->mob_specials.combat_abilities[OLC(d)->behavior_slot].enabled = 1;
        OLC_MOB(d)->mob_specials.combat_abilities[OLC(d)->behavior_slot].ability_type = MOB_ABILITY_SPELL;
        OLC_MOB(d)->mob_specials.combat_abilities[OLC(d)->behavior_slot].target_type = MOB_TARGET_FIGHTING;
        OLC_MOB(d)->mob_specials.combat_abilities[OLC(d)->behavior_slot].trigger_mode = MOB_TRIGGER_COOLDOWN;
        OLC_MOB(d)->mob_specials.combat_abilities[OLC(d)->behavior_slot].priority = 100;
        OLC_MOB(d)->mob_specials.combat_abilities[OLC(d)->behavior_slot].round_min = 1;
        OLC_MOB(d)->mob_specials.combat_abilities[OLC(d)->behavior_slot].round_max = 1;
        OLC_MOB(d)->mob_specials.combat_abilities[OLC(d)->behavior_slot].self_hp_pct_max = 100;
        OLC_MOB(d)->mob_specials.combat_abilities[OLC(d)->behavior_slot].target_hp_pct_max = 100;
        OLC_VAL(d) = 1;
        medit_disp_combat_ability_field_menu(d);
        return;
      case 'e':
        write_to_output(d, "Edit which slot? ");
        OLC_MODE(d) = MEDIT_COMBAT_ABILITIES_EDIT;
        return;
      case 'd':
        write_to_output(d, "Delete which slot? ");
        OLC_MODE(d) = MEDIT_COMBAT_ABILITIES_DELETE;
        return;
      case 't':
        write_to_output(d, "Toggle which slot? ");
        OLC_MODE(d) = MEDIT_COMBAT_ABILITIES_TOGGLE;
        return;
      default:
        medit_disp_combat_abilities_menu(d);
        return;
    }

  case MEDIT_COMBAT_ABILITIES_EDIT:
    i = atoi(arg) - 1;
    if (i < 0 || i >= OLC_MOB(d)->mob_specials.combat_ability_count) {
      write_to_output(d, "Invalid slot.\r\n");
      medit_disp_combat_abilities_menu(d);
      return;
    }
    OLC(d)->behavior_slot = i;
    medit_disp_combat_ability_field_menu(d);
    return;

  case MEDIT_COMBAT_ABILITIES_DELETE:
    i = atoi(arg) - 1;
    if (i < 0 || i >= OLC_MOB(d)->mob_specials.combat_ability_count) {
      write_to_output(d, "Invalid slot.\r\n");
      medit_disp_combat_abilities_menu(d);
      return;
    }
    for (j = i; j < OLC_MOB(d)->mob_specials.combat_ability_count - 1; j++)
      OLC_MOB(d)->mob_specials.combat_abilities[j] = OLC_MOB(d)->mob_specials.combat_abilities[j + 1];
    OLC_MOB(d)->mob_specials.combat_ability_count--;
    OLC_VAL(d) = 1;
    medit_disp_combat_abilities_menu(d);
    return;

  case MEDIT_COMBAT_ABILITIES_TOGGLE:
    i = atoi(arg) - 1;
    if (i < 0 || i >= OLC_MOB(d)->mob_specials.combat_ability_count) {
      write_to_output(d, "Invalid slot.\r\n");
      medit_disp_combat_abilities_menu(d);
      return;
    }
    OLC_MOB(d)->mob_specials.combat_abilities[i].enabled = !OLC_MOB(d)->mob_specials.combat_abilities[i].enabled;
    OLC_VAL(d) = 1;
    medit_disp_combat_abilities_menu(d);
    return;

  case MEDIT_COMBAT_ABILITY_FIELD_MENU: {
    struct mob_combat_ability *ab = &OLC_MOB(d)->mob_specials.combat_abilities[OLC(d)->behavior_slot];
    if (LOWER(*arg) == 'q') {
      if (medit_validate_combat_ability(d, ab))
        OLC_VAL(d) = 1;
      medit_disp_combat_abilities_menu(d);
      return;
    }
    OLC(d)->behavior_field = toupper(*arg);
    write_to_output(d, "Enter new value: ");
    OLC_MODE(d) = MEDIT_COMBAT_ABILITY_FIELD_VALUE;
    return;
  }

  case MEDIT_COMBAT_ABILITY_FIELD_VALUE: {
    struct mob_combat_ability *ab = &OLC_MOB(d)->mob_specials.combat_abilities[OLC(d)->behavior_slot];
    switch (OLC(d)->behavior_field) {
      case '1': ab->ability_type = LIMIT(atoi(arg), MOB_ABILITY_SPELL, MOB_ABILITY_SKILL); break;
      case '2': ab->ability_vnum = find_skill_num(arg); break;
      case '3': ab->target_type = LIMIT(atoi(arg), MOB_TARGET_SELF, MOB_TARGET_ALLY); break;
      case '4': ab->trigger_mode = LIMIT(atoi(arg), MOB_TRIGGER_OPENER, MOB_TRIGGER_TARGET_HP_THRESHOLD); break;
      case '5': ab->round_min = atoi(arg); break;
      case '6': ab->round_max = atoi(arg); break;
      case '7': ab->cooldown_rounds = atoi(arg); break;
      case '8': ab->priority = atoi(arg); break;
      case '9': ab->once_per_fight = atoi(arg) ? 1 : 0; break;
      case 'A': ab->max_uses_per_fight = atoi(arg); break;
      case 'B': ab->require_target_not_affected = atoi(arg) ? 1 : 0; break;
      case 'C': ab->require_self_not_affected = atoi(arg) ? 1 : 0; break;
      case 'D': ab->self_hp_pct_max = atoi(arg); break;
      case 'E': ab->target_hp_pct_max = atoi(arg); break;
      default: break;
    }
    if (medit_validate_combat_ability(d, ab))
      OLC_VAL(d) = 1;
    medit_disp_combat_ability_field_menu(d);
    return;
  }

  case MEDIT_EVENT_REACTIONS_MENU:
    switch (LOWER(*arg)) {
      case 'q': medit_disp_menu(d); return;
      case 'a':
        if (OLC_MOB(d)->mob_specials.event_reaction_count >= MAX_MOB_EVENT_REACTIONS) {
          write_to_output(d, "No free event reaction slots.\r\n");
          medit_disp_event_reactions_menu(d);
          return;
        }
        OLC(d)->behavior_slot = OLC_MOB(d)->mob_specials.event_reaction_count++;
        memset(&OLC_MOB(d)->mob_specials.event_reactions[OLC(d)->behavior_slot], 0, sizeof(struct mob_event_reaction));
        OLC_MOB(d)->mob_specials.event_reactions[OLC(d)->behavior_slot].enabled = 1;
        OLC_MOB(d)->mob_specials.event_reactions[OLC(d)->behavior_slot].event_type = MOB_EVENT_PLAYER_ENTERS_ROOM;
        OLC_MOB(d)->mob_specials.event_reactions[OLC(d)->behavior_slot].action_type = MOB_EVENT_ACTION_SAY_TEXT;
        OLC_MOB(d)->mob_specials.event_reactions[OLC(d)->behavior_slot].target_type = MOB_TARGET_FIGHTING;
        OLC_MOB(d)->mob_specials.event_reactions[OLC(d)->behavior_slot].chance_percent = 100;
        OLC_MOB(d)->mob_specials.event_reactions[OLC(d)->behavior_slot].hp_pct_threshold = 30;
        OLC_VAL(d) = 1;
        medit_disp_event_reaction_field_menu(d);
        return;
      case 'e':
        write_to_output(d, "Edit which slot? ");
        OLC_MODE(d) = MEDIT_EVENT_REACTIONS_EDIT;
        return;
      case 'd':
        write_to_output(d, "Delete which slot? ");
        OLC_MODE(d) = MEDIT_EVENT_REACTIONS_DELETE;
        return;
      case 't':
        write_to_output(d, "Toggle which slot? ");
        OLC_MODE(d) = MEDIT_EVENT_REACTIONS_TOGGLE;
        return;
      default:
        medit_disp_event_reactions_menu(d);
        return;
    }

  case MEDIT_EVENT_REACTIONS_EDIT:
    i = atoi(arg) - 1;
    if (i < 0 || i >= OLC_MOB(d)->mob_specials.event_reaction_count) {
      write_to_output(d, "Invalid slot.\r\n");
      medit_disp_event_reactions_menu(d);
      return;
    }
    OLC(d)->behavior_slot = i;
    medit_disp_event_reaction_field_menu(d);
    return;

  case MEDIT_EVENT_REACTIONS_DELETE:
    i = atoi(arg) - 1;
    if (i < 0 || i >= OLC_MOB(d)->mob_specials.event_reaction_count) {
      write_to_output(d, "Invalid slot.\r\n");
      medit_disp_event_reactions_menu(d);
      return;
    }
    for (j = i; j < OLC_MOB(d)->mob_specials.event_reaction_count - 1; j++)
      OLC_MOB(d)->mob_specials.event_reactions[j] = OLC_MOB(d)->mob_specials.event_reactions[j + 1];
    OLC_MOB(d)->mob_specials.event_reaction_count--;
    OLC_VAL(d) = 1;
    medit_disp_event_reactions_menu(d);
    return;

  case MEDIT_EVENT_REACTIONS_TOGGLE:
    i = atoi(arg) - 1;
    if (i < 0 || i >= OLC_MOB(d)->mob_specials.event_reaction_count) {
      write_to_output(d, "Invalid slot.\r\n");
      medit_disp_event_reactions_menu(d);
      return;
    }
    OLC_MOB(d)->mob_specials.event_reactions[i].enabled = !OLC_MOB(d)->mob_specials.event_reactions[i].enabled;
    OLC_VAL(d) = 1;
    medit_disp_event_reactions_menu(d);
    return;

  case MEDIT_EVENT_REACTION_FIELD_MENU:
    if (LOWER(*arg) == 'q') {
      if (medit_validate_event_reaction(d, &OLC_MOB(d)->mob_specials.event_reactions[OLC(d)->behavior_slot]))
        OLC_VAL(d) = 1;
      medit_disp_event_reactions_menu(d);
      return;
    }
    OLC(d)->behavior_field = toupper(*arg);
    write_to_output(d, "Enter new value: ");
    OLC_MODE(d) = MEDIT_EVENT_REACTION_FIELD_VALUE;
    return;

  case MEDIT_EVENT_REACTION_FIELD_VALUE: {
    struct mob_event_reaction *ev = &OLC_MOB(d)->mob_specials.event_reactions[OLC(d)->behavior_slot];
    switch (OLC(d)->behavior_field) {
      case '1': ev->event_type = LIMIT(atoi(arg), MOB_EVENT_PLAYER_ENTERS_ROOM, MOB_EVENT_DEATH); break;
      case '2': ev->action_type = LIMIT(atoi(arg), MOB_EVENT_ACTION_CAST_SPELL, MOB_EVENT_ACTION_EMOTE_TEXT); break;
      case '3': ev->ability_vnum = find_skill_num(arg); break;
      case '4': ev->target_type = LIMIT(atoi(arg), MOB_TARGET_SELF, MOB_TARGET_ALLY); break;
      case '5': ev->cooldown_pulses = atoi(arg); break;
      case '6': ev->chance_percent = atoi(arg); break;
      case '7': ev->once_per_reset = atoi(arg) ? 1 : 0; break;
      case '8': ev->hp_pct_threshold = atoi(arg); break;
      case '9': strlcpy(ev->argument, arg, sizeof(ev->argument)); break;
      default: break;
    }
    if (medit_validate_event_reaction(d, ev))
      OLC_VAL(d) = 1;
    medit_disp_event_reaction_field_menu(d);
    return;
  }
  }

/* Numerical responses. */

  case MEDIT_SEX:
    GET_SEX(OLC_MOB(d)) = LIMIT(i - 1, 0, NUM_GENDERS - 1);
    break;

  case MEDIT_HITROLL:
    GET_HITROLL(OLC_MOB(d)) = LIMIT(i, 0, 50);
    OLC_VAL(d) = TRUE;
    medit_disp_stats_menu(d);
    return;

  case MEDIT_WIMPY_THRESH:
    GET_MOB_WIMP_LEV(OLC_MOB(d)) = LIMIT(i, 0, 30000);
    OLC_VAL(d) = TRUE;
    medit_disp_stats_menu(d);
    return;

  case MEDIT_DAMROLL:
    GET_DAMROLL(OLC_MOB(d)) = LIMIT(i, 0, 50);
    OLC_VAL(d) = TRUE;
    medit_disp_stats_menu(d);
    return;

  case MEDIT_NDD:
    GET_NDD(OLC_MOB(d)) = LIMIT(i, 0, 30);
    OLC_VAL(d) = TRUE;
    medit_disp_stats_menu(d);
    return;

  case MEDIT_SDD:
    GET_SDD(OLC_MOB(d)) = LIMIT(i, 0, 127);
    OLC_VAL(d) = TRUE;
    medit_disp_stats_menu(d);
    return;

  case MEDIT_NUM_HP_DICE:
    GET_HIT(OLC_MOB(d)) = LIMIT(i, 0, 30);
    OLC_VAL(d) = TRUE;
    medit_disp_stats_menu(d);
    return;

  case MEDIT_SIZE_HP_DICE:
    GET_MANA(OLC_MOB(d)) = LIMIT(i, 0, 1000);
    OLC_VAL(d) = TRUE;
    medit_disp_stats_menu(d);
    return;

  case MEDIT_ADD_HP:
    GET_MOVE(OLC_MOB(d)) = LIMIT(i, 0, 30000);
    OLC_VAL(d) = TRUE;
    medit_disp_stats_menu(d);
    return;

  case MEDIT_AC:
    GET_AC(OLC_MOB(d)) = LIMIT(i, 0, 200);
    OLC_VAL(d) = TRUE;
    medit_disp_stats_menu(d);
    return;

  case MEDIT_EVASION:
    GET_EVASION(OLC_MOB(d)) = LIMIT(i, 0, 200);
    OLC_VAL(d) = TRUE;
    medit_disp_stats_menu(d);
    return;

  case MEDIT_EXP:
    GET_EXP(OLC_MOB(d)) = LIMIT(i, 0, MAX_MOB_EXP);
    OLC_MOB(d)->mob_specials.bonus_xp_min = GET_EXP(OLC_MOB(d));
    OLC_MOB(d)->mob_specials.bonus_xp_max = GET_EXP(OLC_MOB(d));
    OLC_VAL(d) = TRUE;
    medit_disp_stats_menu(d);
    return;

  case MEDIT_BONUS_XP: {
      int xmin = 0, xmax = 0;

      if (sscanf(arg, "%d %d", &xmin, &xmax) == 2) {
        /* ok */
      } else if (sscanf(arg, "%d", &xmin) == 1) {
        xmax = xmin;
      } else {
        write_to_output(d, "Enter Bonus XP min and max (example: 7 13) or a single value: ");
        return;
      }

      if (xmin < 0) xmin = 0;
      if (xmax < 0) xmax = 0;
      if (xmax < xmin) {
        int tmp = xmin;
        xmin = xmax;
        xmax = tmp;
      }

      OLC_MOB(d)->mob_specials.bonus_xp_min = LIMIT(xmin, 0, MAX_MOB_EXP);
      OLC_MOB(d)->mob_specials.bonus_xp_max = LIMIT(xmax, 0, MAX_MOB_EXP);
      GET_EXP(OLC_MOB(d)) = OLC_MOB(d)->mob_specials.bonus_xp_min; /* legacy fallback */
      OLC_VAL(d) = TRUE;
      medit_disp_stats_menu(d);
    }
    return;

  case MEDIT_GOLD: {
      long long gmin = 0, gmax = 0;

      /* Accept: "min max" OR a single value (sets both). */
      if (sscanf(arg, "%lld %lld", &gmin, &gmax) == 2) {
        /* ok */
      } else if (sscanf(arg, "%lld", &gmin) == 1) {
        gmax = gmin;
      } else {
        write_to_output(d, "Enter gold min and max (example: 10 50) or a single value: ");
        return;
      }

      if (gmin < 0) gmin = 0;
      if (gmax < 0) gmax = 0;
      if (gmax < gmin) {
        write_to_output(d, "Max must be >= min. Enter gold min and max (example: 10 50) or a single value: ");
        return;
      }

      OLC_MOB(d)->mob_specials.gold_min = gmin;
      OLC_MOB(d)->mob_specials.gold_max = gmax;


        OLC_VAL(d) = TRUE;
      medit_disp_stats_menu(d);
    }
      return;

  case MEDIT_PET_PRICE: {
    long long price = 0;

    if (sscanf(arg, "%lld", &price) != 1) {
      write_to_output(d, "Enter pet price in gold (0 = automatic): ");
      return;
    }

    if (price < 0)
      price = 0;
    if (price > 2000000000LL)
      price = 2000000000LL;

    GET_PET_PRICE(OLC_MOB(d)) = (int)price;
    OLC_VAL(d) = TRUE;
    medit_disp_menu(d);
    return;
  }

  case MEDIT_STR:
    GET_STR(OLC_MOB(d)) = LIMIT(i, 3, 25);
    OLC_MOB(d)->real_abils.str = GET_STR(OLC_MOB(d));
    OLC_MODE(d) = MEDIT_STR_ADD;
    OLC_VAL(d) = TRUE;
    write_to_output(d, "Enter Strength add value [0-100]: ");
    return;

  case MEDIT_STR_ADD:
    GET_ADD(OLC_MOB(d)) = LIMIT(i, 0, 100);
    OLC_MOB(d)->real_abils.str_add = GET_ADD(OLC_MOB(d));
    OLC_VAL(d) = TRUE;
    medit_disp_stats_menu(d);
    return;

  case MEDIT_INT:
    GET_INT(OLC_MOB(d)) = LIMIT(i, 3, 25);
    OLC_MOB(d)->real_abils.intel = GET_INT(OLC_MOB(d));
    OLC_VAL(d) = TRUE;
    medit_disp_stats_menu(d);
    return;

  case MEDIT_WIS:
    GET_WIS(OLC_MOB(d)) = LIMIT(i, 3, 25);
    OLC_MOB(d)->real_abils.wis = GET_WIS(OLC_MOB(d));
    OLC_VAL(d) = TRUE;
    medit_disp_stats_menu(d);
    return;

  case MEDIT_DEX:
    GET_DEX(OLC_MOB(d)) = LIMIT(i, 3, 25);
    OLC_MOB(d)->real_abils.dex = GET_DEX(OLC_MOB(d));
    OLC_VAL(d) = TRUE;
    medit_disp_stats_menu(d);
    return;

  case MEDIT_CON:
    GET_CON(OLC_MOB(d)) = LIMIT(i, 3, 25);
    OLC_MOB(d)->real_abils.con = GET_CON(OLC_MOB(d));
    OLC_VAL(d) = TRUE;
    medit_disp_stats_menu(d);
    return;

  case MEDIT_CHA:
    GET_CHA(OLC_MOB(d)) = LIMIT(i, 3, 25);
    OLC_MOB(d)->real_abils.cha = GET_CHA(OLC_MOB(d));
    OLC_VAL(d) = TRUE;
    medit_disp_stats_menu(d);
    return;

  case MEDIT_PARA:
    GET_SAVE(OLC_MOB(d), SAVING_PARA) = LIMIT(i, 0, 100);
    OLC_VAL(d) = TRUE;
    medit_disp_stats_menu(d);
    return;

  case MEDIT_ROD:
    GET_SAVE(OLC_MOB(d), SAVING_ROD) = LIMIT(i, 0, 100);
    OLC_VAL(d) = TRUE;
    medit_disp_stats_menu(d);
    return;

  case MEDIT_PETRI:
    GET_SAVE(OLC_MOB(d), SAVING_PETRI) = LIMIT(i, 0, 100);
    OLC_VAL(d) = TRUE;
    medit_disp_stats_menu(d);
    return;

  case MEDIT_BREATH:
    GET_SAVE(OLC_MOB(d), SAVING_BREATH) = LIMIT(i, 0, 100);
    OLC_VAL(d) = TRUE;
    medit_disp_stats_menu(d);
    return;

  case MEDIT_SPELL:
    GET_SAVE(OLC_MOB(d), SAVING_SPELL) = LIMIT(i, 0, 100);
    OLC_VAL(d) = TRUE;
    medit_disp_stats_menu(d);
    return;

  case MEDIT_POS:
    GET_POS(OLC_MOB(d)) = LIMIT(i - 1, 0, NUM_POSITIONS - 1);
    break;

  case MEDIT_DEFAULT_POS:
    GET_DEFAULT_POS(OLC_MOB(d)) = LIMIT(i - 1, 0, NUM_POSITIONS - 1);
    break;

  case MEDIT_ATTACK:
    GET_ATTACK(OLC_MOB(d)) = LIMIT(i, 0, NUM_ATTACK_TYPES - 1);
    break;

  case MEDIT_LEVEL:
  {
    int old_level = GET_LEVEL(OLC_MOB(d));
    int new_level = LIMIT(i, 1, LVL_IMPL);

    GET_LEVEL(OLC_MOB(d)) = new_level;
    OLC_VAL(d) = TRUE;
    if (new_level != old_level) {
      OLC_MODE(d) = MEDIT_LEVEL_AUTOFILL_CONFIRM;
      write_to_output(d, "Apply recommended stats for level %d? (Y/N): ", new_level);
      return;
    }
    medit_disp_stats_menu(d);
    return;
  }

  case MEDIT_LEVEL_AUTOFILL_CONFIRM:
    switch (*arg) {
    case 'y':
    case 'Y':
      medit_autoroll_stats(d);
      OLC_VAL(d) = TRUE;
      break;
    case 'n':
    case 'N':
      break;
    default:
      write_to_output(d, "Please answer Y or N: ");
      return;
    }
    medit_disp_stats_menu(d);
    return;

  case MEDIT_ALIGNMENT:
    GET_ALIGNMENT(OLC_MOB(d)) = LIMIT(i, -1000, 1000);
    OLC_VAL(d) = TRUE;
    medit_disp_stats_menu(d);
    return;

  case MEDIT_COPY:
    if ((i = real_mobile(atoi(arg))) != NOWHERE) {
      medit_setup_existing(d, i);
    } else
      write_to_output(d, "That mob does not exist.\r\n");
    break;

  case MEDIT_DELETE:
    if (*arg == 'y' || *arg == 'Y') {
      if (delete_mobile(GET_MOB_RNUM(OLC_MOB(d))) != NOBODY)
        write_to_output(d, "Mobile deleted.\r\n");
      else
        write_to_output(d, "Couldn't delete the mobile!\r\n");

      cleanup_olc(d, CLEANUP_ALL);
      return;
    } else if (*arg == 'n' || *arg == 'N') {
      medit_disp_menu(d);
      OLC_MODE(d) = MEDIT_MAIN_MENU;
      return;
    } else
      write_to_output(d, "Please answer 'Y' or 'N': ");
    break;

  default:
    /* We should never get here. */
    cleanup_olc(d, CLEANUP_ALL);
    mudlog(BRF, LVL_BUILDER, TRUE, "SYSERR: OLC: medit_parse(): Reached default case!");
    write_to_output(d, "Oops...\r\n");
    break;
  }

/* END OF CASE If we get here, we have probably changed something, and now want
   to return to main menu.  Use OLC_VAL as a 'has changed' flag */

  OLC_VAL(d) = TRUE;
  medit_disp_menu(d);
}

void medit_string_cleanup(struct descriptor_data *d, int terminator)
{
  switch (OLC_MODE(d)) {

  case MEDIT_D_DESC:
  default:
     medit_disp_menu(d);
     break;
  }
}

void medit_autoroll_stats(struct descriptor_data *d)
{
  int mob_lev;

  mob_lev = GET_LEVEL(OLC_MOB(d));
  mob_lev = GET_LEVEL(OLC_MOB(d)) = LIMIT(mob_lev, 1, LVL_IMPL);

  GET_MOVE(OLC_MOB(d))    = mob_lev * 5;                 /* HP addition baseline */
  GET_HIT(OLC_MOB(d))     = MAX(1, mob_lev / 6);         /* number of HP dice */
  GET_MANA(OLC_MOB(d))    = MAX(4, (mob_lev / 6) + 2);   /* size of HP dice */

  GET_NDD(OLC_MOB(d))     = MAX(1, (mob_lev + 4) / 8);   /* number of damage dice */
  GET_SDD(OLC_MOB(d))     = MAX(2, (mob_lev + 9) / 10);  /* size of damage dice */
  GET_DAMROLL(OLC_MOB(d)) = mob_lev / 12;                /* damage bonus */

  GET_HITROLL(OLC_MOB(d)) = mob_lev / 8;                 /* conservative early hit chance */
  OLC_MOB(d)->mob_specials.gold_min = MAX(0, mob_lev);
  OLC_MOB(d)->mob_specials.gold_max = MAX(OLC_MOB(d)->mob_specials.gold_min, mob_lev * 2);
  GET_AC(OLC_MOB(d))      = 20 + mob_lev;                /* gentler armor scaling */
  GET_EVASION(OLC_MOB(d)) = mob_lev / 3;                 /* conservative evasion */
  GET_MOB_WIMP_LEV(OLC_MOB(d)) = MAX(1, mob_lev / 2);    /* default flee threshold */

  /* 'Advanced' stats are only rolled if advanced options are enabled */
  if (CONFIG_MEDIT_ADVANCED) {
    GET_STR(OLC_MOB(d))     = LIMIT((mob_lev*2)/3, 11, 18); /* 2/3 level in range 11 to 18 */
    GET_INT(OLC_MOB(d))     = LIMIT((mob_lev*2)/3, 11, 18);
    GET_WIS(OLC_MOB(d))     = LIMIT((mob_lev*2)/3, 11, 18);
    GET_DEX(OLC_MOB(d))     = LIMIT((mob_lev*2)/3, 11, 18);
    GET_CON(OLC_MOB(d))     = LIMIT((mob_lev*2)/3, 11, 18);
    GET_CHA(OLC_MOB(d))     = LIMIT((mob_lev*2)/3, 11, 18);
    OLC_MOB(d)->real_abils.str   = GET_STR(OLC_MOB(d));
    OLC_MOB(d)->real_abils.intel = GET_INT(OLC_MOB(d));
    OLC_MOB(d)->real_abils.wis   = GET_WIS(OLC_MOB(d));
    OLC_MOB(d)->real_abils.dex   = GET_DEX(OLC_MOB(d));
    OLC_MOB(d)->real_abils.con   = GET_CON(OLC_MOB(d));
    OLC_MOB(d)->real_abils.cha   = GET_CHA(OLC_MOB(d));

    GET_SAVE(OLC_MOB(d), SAVING_PARA)   = mob_lev / 4;  /* All Saving throws */
    GET_SAVE(OLC_MOB(d), SAVING_ROD)    = mob_lev / 4;  /* set to a quarter  */
    GET_SAVE(OLC_MOB(d), SAVING_PETRI)  = mob_lev / 4;  /* of the mobs level */
    GET_SAVE(OLC_MOB(d), SAVING_BREATH) = mob_lev / 4;
    GET_SAVE(OLC_MOB(d), SAVING_SPELL)  = mob_lev / 4;
  }

}
