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

/* Needed for MOB_GUILD_MASTER auto-sync in medit_save_internally() */
SPECIAL(guild);
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
#include "ai_actor.h"

/* Builder-friendly NPC flags list:
 * action_bits[] ends with reserved "DEAD" which should not be exposed in OLC menus.
 * This wrapper omits that final entry while keeping numbering stable for builders.
 */
static const char *action_bits_olc[] = {
  "SPEC",
  "SENTINEL",
  "SCAVENGER",
  "ISNPC",
  "AWARE",
  "AGGR",
  "STAY-ZONE",
  "WIMPY",
  "AGGR_EVIL",
  "AGGR_GOOD",
  "AGGR_NEUTRAL",
  "MEMORY",
  "HELPER",
  "NO_CHARM",
  "NO_SUMMN",
  "NO_SLEEP",
  "NO_BASH",
  "NO_BLIND",
  "NO_KILL",
  "GUILD_MASTER",
  "RESERVED",
  "AI_ACTOR",
  "\n"
};

/* local functions */
static void medit_setup_new(struct descriptor_data *d);
static void init_mobile(struct char_data *mob);
static void medit_save_to_disk(zone_vnum zone_num);
static void medit_disp_positions(struct descriptor_data *d);
static void medit_disp_sex(struct descriptor_data *d);
static void medit_disp_attack_types(struct descriptor_data *d);
static bool medit_illegal_mob_flag(int fl);
static int  medit_get_mob_flag_by_number(int num);
static void medit_disp_mob_flags(struct descriptor_data *d);
static void medit_disp_aff_flags(struct descriptor_data *d);
static void medit_disp_menu(struct descriptor_data *d);
static void medit_disp_ai_menu(struct descriptor_data *d);
static void medit_disp_ai_perception(struct descriptor_data *d) { struct mob_ai_config*c=OLC_MOB(d)->ai_config; write_to_output(d,"\r\n                 AI Actor Perception\r\n1) Notice Entry: %s  2) Notice Departure: %s  3) Notice Speech: %s\r\n4) Notice Whispers: %s  5) Notice Emotes: %s  6) Notice Combat: %s\r\n7) Attacks Self: %s  8) Attacks Allies: %s  9) Corpses: %s\r\nA) Drops: %s  B) Gifts: %s  C) Crimes: %s\r\nD) Hearing: %d  E) Observation: %d  F) Suspicion: %d  G) Recognition: %d\r\nQ) Return\r\nChoice: ",c->notice_entry?"Yes":"No",c->notice_departure?"Yes":"No",c->notice_speech?"Yes":"No",c->notice_whispers?"Yes":"No",c->notice_emotes?"Yes":"No",c->notice_combat?"Yes":"No",c->notice_self_attack?"Yes":"No",c->notice_ally_attack?"Yes":"No",c->notice_corpses?"Yes":"No",c->notice_drops?"Yes":"No",c->notice_gifts?"Yes":"No",c->notice_crimes?"Yes":"No",c->hearing_sensitivity,c->observation_sensitivity,c->suspicion_threshold,c->recognition_confidence); OLC_MODE(d)=MEDIT_AI_PERCEPTION; }
static const char *threat_name(int n) { static const char *x[]={"Observe","Warn","Challenge","Call Help","Assist (unavailable)","Follow (unavailable)","Arrest (unavailable)","Attack","Flee","Surrender (unavailable)","Ignore"}; return n>=0&&n<AI_THREAT_RESPONSE_MAX?x[n]:"Invalid"; }
static void medit_disp_ai_threat(struct descriptor_data *d) { struct mob_ai_config*c=OLC_MOB(d)->ai_config; write_to_output(d,"\r\nAI Actor Threat Response\r\n1) Observe:%s 2) Warn:%s 3) Challenge:%s 4) Call Help:%s 5) Assist:%s\r\n6) Follow: Unavailable 7) Arrest: Unavailable 8) Attack:%s 9) Flee:%s A) Surrender: Unavailable\r\nB) Cooldown:%d C) Calm reset:%d D) Repeat window:%d\r\nE) Escalation sequence R) Reset Q) Return\r\nChoice: ",c->threat_enabled[0]?"Yes":"No",c->threat_enabled[1]?"Yes":"No",c->threat_enabled[2]?"Yes":"No",c->threat_enabled[3]?"Yes":"No",c->threat_enabled[4]?"Yes":"No",c->threat_enabled[7]?"Yes":"No",c->threat_enabled[8]?"Yes":"No",c->threat_cooldown,c->calm_reset_time,c->repeated_event_window); OLC_MODE(d)=MEDIT_AI_THREAT; }
static void medit_disp_ai_threat_sequence(struct descriptor_data *d) { struct mob_ai_config*c=OLC_MOB(d)->ai_config;int i;write_to_output(d,"\r\nThreat Escalation Sequence\r\n");for(i=0;i<c->threat_step_count;i++)write_to_output(d,"%d) %s Severity %d Cooldown %d Repeats %d Advance %s\r\n",i+1,threat_name(c->threat_steps[i].type),c->threat_steps[i].minimum_severity,c->threat_steps[i].cooldown,c->threat_steps[i].max_repetitions,c->threat_steps[i].advance_on_failure?"Yes":"No");write_to_output(d,"A <type severity cooldown repeats advance>; D <line>; U <line>; N <line>; Q) Return\r\nChoice: ");OLC_MODE(d)=MEDIT_AI_THREAT_SEQUENCE; }
static void medit_disp_ai_memory(struct descriptor_data *d) { struct mob_ai_config*c=OLC_MOB(d)->ai_config; write_to_output(d,"\r\n                   AI Actor Memory\r\n1) Enabled: %s  2) Maximum Actors: %d  3) Ordinary Duration: %d  4) Important Duration: %d\r\n5) Trust Gain: %d%%  6) Trust Loss: %d%%  7) Fear Gain: %d%%  8) Fear Decay: %d\r\n9) Hostility Gain: %d%% A) Hostility Decay: %d B) Familiarity Gain: %d%% C) Familiarity Decay: %d D) Forgiveness: %d\r\nE) Attacks:%s F) Assistance:%s G) Crimes:%s H) Gifts:%s I) Insults:%s J) Conversations:%s K) Threats:%s L) Last room:%s M) Deaths:%s\r\nQ) Return\r\nChoice: ",c->memory_enabled?"Yes":"No",c->memory_max_actors,c->memory_ordinary_duration,c->memory_important_duration,c->trust_gain,c->trust_loss,c->fear_gain,c->fear_decay,c->hostility_gain,c->hostility_decay,c->familiarity_gain,c->familiarity_decay,c->forgiveness,c->remember_attacks?"Yes":"No",c->remember_assistance?"Yes":"No",c->remember_crimes?"Yes":"No",c->remember_gifts?"Yes":"No",c->remember_insults?"Yes":"No",c->remember_conversations?"Yes":"No",c->remember_threats?"Yes":"No",c->remember_last_room?"Yes":"No",c->remember_deaths?"Yes":"No"); OLC_MODE(d)=MEDIT_AI_MEMORY; }
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

static const int medit_eq_picker_slots[] = {
  WEAR_HEAD, WEAR_NECK_1, WEAR_ABOUT, WEAR_BODY, WEAR_ARMS, WEAR_WRIST_R,
  WEAR_WRIST_L, WEAR_HANDS, WEAR_FINGER_R, WEAR_FINGER_L, WEAR_WAIST,
  WEAR_LEGS, WEAR_FEET, WEAR_WIELD, WEAR_HOLD, WEAR_SHIELD, WEAR_LIGHT
};

static const char *medit_eq_picker_labels[] = {
  "Head", "Neck", "Back", "Body", "Arms", "Wrist Right", "Wrist Left",
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

static void medit_save_to_disk(zone_vnum foo)
{
  save_mobiles(real_zone(foo));
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


  /* Auto-sync GUILD_MASTER flag with the guild spec-proc. */
  if (IS_SET_AR(MOB_FLAGS(OLC_MOB(d)), MOB_GUILD_MASTER)) {
    mob_index[new_rnum].func = guild;
  } else if (mob_index[new_rnum].func == guild) {
    mob_index[new_rnum].func = NULL;
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

  ai_actor_refresh_live_mobs_by_vnum(OLC_NUM(d));

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
    20,
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

  sprintbitarray(MOB_FLAGS(OLC_MOB(d)), action_bits_olc, AF_ARRAY_MAX, flags);
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

static const char *ai_trait_names[AI_ACTOR_PERSONALITIES] = { "Aggression", "Bravery", "Sociability", "Curiosity", "Discipline", "Honesty", "Greed", "Compassion", "Loyalty", "Patience", "Suspicion", "Pride" };
static void medit_disp_ai_personality(struct descriptor_data *d) { struct mob_ai_config *c=OLC_MOB(d)->ai_config; int i; write_to_output(d,"\r\nAI Actor Personality\r\n"); for(i=0;i<12;i++) write_to_output(d,"%c) %-12s : %d\r\n",i<9?'1'+i:'A'+i-9,ai_trait_names[i],c->personality[i]); write_to_output(d,"P) Apply Preset  Q) Return\r\nChoice: "); OLC_MODE(d)=MEDIT_AI_PERSONALITY; }
static void medit_disp_ai_social(struct descriptor_data *d) { struct mob_ai_config*c=OLC_MOB(d)->ai_config; write_to_output(d,"\r\nAI Actor Social Behavior\r\n1) Style: %s\r\n2) Greeting: %s\r\n3) Ambient speech: %s\r\n4) Ambient emotes: %s\r\n5) Whisper replies: %s\r\n6) Respond strangers: %s\r\n7) Respond trusted: %s\r\n8) Respond feared: %s\r\n9) Respond hostile: %s\r\nA) Speech cooldown: %d\r\nB) Room cooldown: %d\r\nC) Emote cooldown: %d\r\nQ) Return\r\nChoice: ",ai_social_style_name(c->social),c->greeting_enabled?"Yes":"No",c->ambient_speech_enabled?"Yes":"No",c->ambient_emotes_enabled?"Yes":"No",c->whisper_enabled?"Yes":"No",c->respond_strangers?"Yes":"No",c->respond_trusted?"Yes":"No",c->respond_feared?"Yes":"No",c->respond_hostile?"Yes":"No",c->speech_cooldown,c->room_speech_cooldown,c->emote_cooldown); OLC_MODE(d)=MEDIT_AI_SOCIAL; }
static void medit_disp_ai_dialogue_lines(struct descriptor_data *d, int category) { struct mob_ai_config *c=OLC_MOB(d)->ai_config; int i; write_to_output(d,"\r\n%s dialogue (%d/%d)\r\n",ai_dialogue_category_name(category),c->dialogue_count[category],AI_DIALOGUE_MAX_LINES); for(i=0;i<c->dialogue_count[category];i++) write_to_output(d," %d) %s\r\n",i+1,c->dialogue[category][i]); write_to_output(d,"A) Add  E <line>) Edit  D <line>) Delete  U <line>) Move up  N <line>) Move down  Q) Categories\r\nChoice: "); OLC_MODE(d)=MEDIT_AI_DIALOGUE; }
static void medit_disp_ai_dialogue(struct descriptor_data *d) { struct mob_ai_config*c=OLC_MOB(d)->ai_config; write_to_output(d,"\r\nAI Actor Dialogue\r\n"); for(i=0;i<AI_DIALOGUE_CATEGORIES;i++) write_to_output(d,"%d) %s: %d/%d\r\n",i,ai_dialogue_category_name(i),c->dialogue_count[i],AI_DIALOGUE_MAX_LINES); write_to_output(d,"Select category; Q) Return\r\nChoice: "); OLC_MODE(d)=MEDIT_AI_DIALOGUE; }


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

  sprintbitarray(MOB_FLAGS(mob), action_bits_olc, AF_ARRAY_MAX, flags);
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
          "%sI%s) AI Actor Configuration: %s%s%s\r\n"
          "%sS%s) Script    : %s%s\r\n"
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
          grn, nrm, yel, MOB_FLAGGED(mob, MOB_AI_ACTOR) ? (mob->ai_config ? "Configured" : "Inferred") : "disabled (enable AI_ACTOR flag first)", nrm,
          grn, nrm, cyn, OLC_SCRIPT(d) ?"Set.":"Not Set.",
          grn, nrm,
          grn, nrm,
	  grn, nrm
	  );

  OLC_MODE(d) = MEDIT_MAIN_MENU;
}

static void medit_disp_ai_menu(struct descriptor_data *d)
{
  struct mob_ai_config *c = OLC_MOB(d)->ai_config;
  if (!MOB_FLAGGED(OLC_MOB(d), MOB_AI_ACTOR)) {
    write_to_output(d, "Enable AI_ACTOR for this mob? (Y/N): ");
    OLC_MODE(d) = MEDIT_AI_ENABLE_CONFIRM; return;
  }
  if (!c) OLC_MOB(d)->ai_config = c = mob_ai_config_new();
  write_to_output(d,
    "\r\nAI Actor Configuration\r\n"
    "1) Profile mode: %s\r\n2) Role: %d\r\n3) Movement: %d\r\n4) Personality\r\n5) Social behavior\r\n6) Dialogue lines\r\n7) Perception\r\n8) Memory\r\n9) Threat response\r\n"
    "P) Preview compiled profile\r\nR) Reset to inferred defaults\r\nQ) Return\r\nChoice: ",
    c->mode == MOB_AI_CUSTOM ? "Custom" : c->mode == MOB_AI_INFERRED_OVERRIDES ? "Inferred with overrides" : "Inferred", c->role, c->movement);
  OLC_MODE(d) = MEDIT_AI_MENU;
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
  total_xp_preview = LIMIT(base_xp_preview + GET_EXP(mob), 0, MAX_MOB_EXP);
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
  "(%sF%s) Bonus XP:                  %s[%s%5d%s]%s\r\n"
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
      cyn, nrm, cyn, yel, GET_EXP(mob), cyn, nrm,
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

void medit_parse(struct descriptor_data *d, char *arg)
{
  int i = -1, j;
  char *oldtext = NULL;

  if (OLC_MODE(d) == MEDIT_STATS_MENU ||
      OLC_MODE(d) == MEDIT_GOLD ||
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
             OLC_MODE(d) != MEDIT_AI_MENU && OLC_MODE(d) != MEDIT_AI_PERSONALITY && OLC_MODE(d) != MEDIT_AI_SOCIAL && OLC_MODE(d) != MEDIT_AI_DIALOGUE && OLC_MODE(d) != MEDIT_AI_DIALOGUE_ADD) {
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
        medit_save_to_disk(zone_table[real_zone_by_thing(OLC_NUM(d))].number);
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
    case 'i':
    case 'I':
      medit_disp_ai_menu(d);
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
      OLC_MODE(d) = MEDIT_EXP;
      i++;
      break;
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

  case MEDIT_AI_MENU:
    switch (LOWER(*arg)) {
      case 'q': medit_disp_menu(d); return;
      case '1': OLC_MODE(d) = MEDIT_AI_MODE; write_to_output(d, "Mode (0 Inferred, 1 Custom, 2 Overrides): "); return;
      case '2': OLC_MODE(d) = MEDIT_AI_ROLE; write_to_output(d, "Role (0-9): "); return;
      case '3': OLC_MODE(d) = MEDIT_AI_MOVEMENT; write_to_output(d, "Movement (0-5): "); return;
      case '4': medit_disp_ai_personality(d); return;
      case '5': medit_disp_ai_social(d); return;
      case '6': medit_disp_ai_dialogue(d); return;
      case '7': medit_disp_ai_perception(d); return;
      case '8': medit_disp_ai_memory(d); return;
      case '9': medit_disp_ai_threat(d); return;
      case 'r': mob_ai_config_free(OLC_MOB(d)->ai_config); OLC_MOB(d)->ai_config = NULL; OLC_VAL(d) = 1; medit_disp_ai_menu(d); return;
      case 'p': { struct ai_actor_profile *p; int n; ai_actor_refresh_profile(OLC_MOB(d), TRUE); p=OLC_MOB(d)->ai_prof; write_to_output(d, "Compiled profile: role=%d movement=%d social=%s\r\nSpeech: greet=%s ambient=%s emotes=%s whisper=%s cooldowns=%d/%d/%d\r\nResponses: strangers=%s trusted=%s feared=%s hostile=%s; response modifier=%+d\r\nTraits:", p->role, p->movement, ai_social_style_name(p->social), p->greeting_enabled?"on":"off",p->ambient_speech_enabled?"on":"off",p->ambient_emotes_enabled?"on":"off",p->whisper_enabled?"on":"off",p->talk_cooldown_secs,p->room_talk_cooldown_secs,p->emote_cooldown_secs,p->respond_strangers?"on":"off",p->respond_trusted?"on":"off",p->respond_feared?"on":"off",p->respond_hostile?"on":"off",ai_actor_personality_response_modifier(p->personality)); for(n=0;n<AI_ACTOR_PERSONALITIES;n++) write_to_output(d," %s=%d",ai_trait_names[n],p->personality[n]); write_to_output(d,"\r\nDialogue pools:"); for(n=0;n<AI_DIALOGUE_CATEGORIES;n++) write_to_output(d," %s=%d",ai_dialogue_category_name(n),p->dialogue_count[n]); write_to_output(d,"\r\n"); medit_disp_ai_menu(d); return; }
      default: medit_disp_ai_menu(d); return;
    }
  case MEDIT_AI_PERSONALITY:
    if (LOWER(*arg)=='q') { medit_disp_ai_menu(d); return; }
    if (LOWER(*arg)=='p') { OLC_MODE(d)=MEDIT_AI_PRESET; write_to_output(d,"Preset (0 Neutral, 1 Stoic Guard, 2 Friendly Merchant, 3 Nervous Civilian, 4 Fanatical Cultist, 5 Greedy Official, 6 Territorial Beast, 7 Disciplined Soldier): "); return; }
    i=(LOWER(*arg)>='a'&&LOWER(*arg)<='c')?9+LOWER(*arg)-'a':atoi(arg)-1; if(i<0||i>=12){medit_disp_ai_personality(d);return;} OLC_MODE(d)=MEDIT_AI_TRAIT; OLC_VAL(d)=i; write_to_output(d,"Value (0-100): "); return;
  case MEDIT_AI_TRAIT: if(i<0||i>100){write_to_output(d,"Value must be 0-100: ");return;} if(OLC_MOB(d)->ai_config->personality[OLC_VAL(d)]!=i){OLC_MOB(d)->ai_config->personality[OLC_VAL(d)]=i;OLC_MOB(d)->ai_config->override_mask|=AI_OVERRIDE_TRAITS;OLC_VAL(d)=1;} medit_disp_ai_personality(d);return;
  case MEDIT_AI_PRESET: if(i<0||i>7){medit_disp_ai_personality(d);return;} {static const int p[8][12]={{50,50,50,50,50,50,50,50,50,50,50,50},{25,70,30,20,90,75,10,55,85,65,45,40},{10,55,85,55,60,75,55,75,60,70,20,45},{15,20,30,65,35,55,20,65,40,25,75,20},{75,80,45,35,85,25,15,20,90,45,65,80},{35,55,55,30,75,45,95,20,75,55,60,70},{85,65,15,55,25,20,20,10,25,20,85,60},{45,75,45,25,95,70,20,55,90,80,45,50}};memcpy(OLC_MOB(d)->ai_config->personality,p[i],sizeof(p[i]));OLC_MOB(d)->ai_config->override_mask|=AI_OVERRIDE_TRAITS;OLC_VAL(d)=1;}medit_disp_ai_personality(d);return;
  case MEDIT_AI_SOCIAL: if(LOWER(*arg)=='q'){medit_disp_ai_menu(d);return;} i=(LOWER(*arg)>='a'&&LOWER(*arg)<='c')?10+LOWER(*arg)-'a':atoi(arg); if(i<1||i>12){medit_disp_ai_social(d);return;} OLC_MODE(d)=MEDIT_AI_SOCIAL_VALUE;OLC_VAL(d)=i;write_to_output(d,"Value: ");return;
  case MEDIT_AI_SOCIAL_VALUE: {struct mob_ai_config*c=OLC_MOB(d)->ai_config;int *v; switch(OLC_VAL(d)){case 1:v=&c->social;break;case 2:v=&c->greeting_enabled;break;case 3:v=&c->ambient_speech_enabled;break;case 4:v=&c->ambient_emotes_enabled;break;case 5:v=&c->whisper_enabled;break;case 6:v=&c->respond_strangers;break;case 7:v=&c->respond_trusted;break;case 8:v=&c->respond_feared;break;case 9:v=&c->respond_hostile;break;case 10:v=&c->speech_cooldown;break;case 11:v=&c->room_speech_cooldown;break;default:v=&c->emote_cooldown;}if(i<0||(OLC_VAL(d)==1&&i>10)||(OLC_VAL(d)>1&&OLC_VAL(d)<10&&i>1)||(OLC_VAL(d)>=10&&(i<1||i>300))){write_to_output(d,"Invalid value: ");return;}if(*v!=i){*v=i;OLC_VAL(d)=1;}medit_disp_ai_social(d);return;}
  case MEDIT_AI_DIALOGUE:
    if (LOWER(*arg)=='q') { medit_disp_ai_menu(d); return; }
    if (OLC_VAL(d) >= 0 && OLC_VAL(d) < AI_DIALOGUE_CATEGORIES && !isdigit((unsigned char)*arg)) {
      int line = atoi(arg+1) - 1; int category = OLC_VAL(d);
      if (LOWER(*arg)=='a') { OLC_MODE(d)=MEDIT_AI_DIALOGUE_ADD; write_to_output(d,"Line to add: "); return; }
      if (LOWER(*arg)=='e' && line>=0 && line<OLC_MOB(d)->ai_config->dialogue_count[category]) { OLC_VAL(d)=category*AI_DIALOGUE_MAX_LINES+line; OLC_MODE(d)=MEDIT_AI_DIALOGUE_EDIT; write_to_output(d,"Replacement line: "); return; }
      if (LOWER(*arg)=='d' && mob_ai_dialogue_delete(OLC_MOB(d)->ai_config,category,line)) { OLC_VAL(d)=1; medit_disp_ai_dialogue_lines(d,category); return; }
      if (LOWER(*arg)=='u' && mob_ai_dialogue_move(OLC_MOB(d)->ai_config,category,line,line-1)) { OLC_VAL(d)=1; medit_disp_ai_dialogue_lines(d,category); return; }
      if (LOWER(*arg)=='n' && mob_ai_dialogue_move(OLC_MOB(d)->ai_config,category,line,line+1)) { OLC_VAL(d)=1; medit_disp_ai_dialogue_lines(d,category); return; }
      medit_disp_ai_dialogue_lines(d,category); return;
    }
    i=atoi(arg);if(i<0||i>=AI_DIALOGUE_CATEGORIES){medit_disp_ai_dialogue(d);return;} OLC_VAL(d)=i;medit_disp_ai_dialogue_lines(d,i);return;
  case MEDIT_AI_DIALOGUE_ADD: if(*arg&&mob_ai_dialogue_set(OLC_MOB(d)->ai_config,OLC_VAL(d),OLC_MOB(d)->ai_config->dialogue_count[OLC_VAL(d)],arg))OLC_VAL(d)=1;else write_to_output(d,"Line rejected or pool full.\r\n");medit_disp_ai_dialogue_lines(d,OLC_VAL(d));return;
  case MEDIT_AI_DIALOGUE_EDIT: { int category=OLC_VAL(d)/AI_DIALOGUE_MAX_LINES, line=OLC_VAL(d)%AI_DIALOGUE_MAX_LINES; if(*arg&&mob_ai_dialogue_set(OLC_MOB(d)->ai_config,category,line,arg))OLC_VAL(d)=1;else write_to_output(d,"Line rejected.\r\n");medit_disp_ai_dialogue_lines(d,category);return; }
  case MEDIT_AI_PERCEPTION: if(LOWER(*arg)=='q'){medit_disp_ai_menu(d);return;} i=(LOWER(*arg)>='a'&&LOWER(*arg)<='g')?10+LOWER(*arg)-'a':atoi(arg); if(i<1||i>16){medit_disp_ai_perception(d);return;} OLC_VAL(d)=i; OLC_MODE(d)=MEDIT_AI_PERCEPTION_VALUE; write_to_output(d,"Value (0-100; toggles 0/1): ");return;
  case MEDIT_AI_PERCEPTION_VALUE: { struct mob_ai_config*c=OLC_MOB(d)->ai_config; int *v=&c->notice_entry+OLC_VAL(d)-1; if(i<0||i>100||(OLC_VAL(d)<=12&&i>1)){write_to_output(d,"Invalid value: ");return;} if(*v!=i){*v=i;OLC_VAL(d)=1;} medit_disp_ai_perception(d);return; }
  case MEDIT_AI_MEMORY: if(LOWER(*arg)=='q'){medit_disp_ai_menu(d);return;} i=(LOWER(*arg)>='a'&&LOWER(*arg)<='m')?10+LOWER(*arg)-'a':atoi(arg);if(i<1||i>22){medit_disp_ai_memory(d);return;}OLC_VAL(d)=i;OLC_MODE(d)=MEDIT_AI_MEMORY_VALUE;write_to_output(d,"Value: ");return;
  case MEDIT_AI_MEMORY_VALUE: { struct mob_ai_config*c=OLC_MOB(d)->ai_config; int *v; if(OLC_VAL(d)<=13) v=&c->memory_enabled+OLC_VAL(d)-1; else v=&c->remember_attacks+OLC_VAL(d)-14; if(i<0||i>((OLC_VAL(d)==2)?AI_MEM_MAX:((OLC_VAL(d)>=5&&OLC_VAL(d)<=7||OLC_VAL(d)==9||OLC_VAL(d)==11)?200:((OLC_VAL(d)>=14)?1:100)))){write_to_output(d,"Invalid value: ");return;}if(*v!=i){*v=i;OLC_VAL(d)=1;}medit_disp_ai_memory(d);return; }
  case MEDIT_AI_THREAT:
    if(LOWER(*arg)=='q'){medit_disp_ai_menu(d);return;} if(LOWER(*arg)=='e'){medit_disp_ai_threat_sequence(d);return;} if(LOWER(*arg)=='r'){mob_ai_config_free(OLC_MOB(d)->ai_config);OLC_MOB(d)->ai_config=mob_ai_config_new();OLC_VAL(d)=1;medit_disp_ai_threat(d);return;} i=(LOWER(*arg)>='a'&&LOWER(*arg)<='d')?10+LOWER(*arg)-'a':atoi(arg); if(i>=1&&i<=5){if(i==5){medit_disp_ai_threat(d);return;} OLC_MOB(d)->ai_config->threat_enabled[i-1]=!OLC_MOB(d)->ai_config->threat_enabled[i-1];OLC_VAL(d)=1;medit_disp_ai_threat(d);return;}if(i==8||i==9){OLC_MOB(d)->ai_config->threat_enabled[i-1]=!OLC_MOB(d)->ai_config->threat_enabled[i-1];OLC_VAL(d)=1;medit_disp_ai_threat(d);return;}if(i>=11&&i<=13){OLC_VAL(d)=i;OLC_MODE(d)=MEDIT_AI_THREAT_VALUE;write_to_output(d,"Value: ");return;}medit_disp_ai_threat(d);return;
  case MEDIT_AI_THREAT_VALUE: if(i<1||i>(OLC_VAL(d)==12?3600:600)){write_to_output(d,"Invalid value: ");return;} if(OLC_VAL(d)==11)OLC_MOB(d)->ai_config->threat_cooldown=i;else if(OLC_VAL(d)==12)OLC_MOB(d)->ai_config->calm_reset_time=i;else OLC_MOB(d)->ai_config->repeated_event_window=i;OLC_VAL(d)=1;medit_disp_ai_threat(d);return;
  case MEDIT_AI_THREAT_SEQUENCE: { struct mob_ai_config*c=OLC_MOB(d)->ai_config; int a,b,cc,e,f,n=atoi(arg)-1; if(LOWER(*arg)=='q'){medit_disp_ai_threat(d);return;} if(sscanf(arg,"a %d %d %d %d %d",&a,&b,&cc,&e,&f)==5&&c->threat_step_count<AI_THREAT_STEP_MAX){struct ai_threat_step z={a,b,cc,e,f};if(ai_threat_step_valid(&z,c->threat_enabled)){c->threat_steps[c->threat_step_count++]=z;OLC_VAL(d)=1;}medit_disp_ai_threat_sequence(d);return;}if(LOWER(*arg)=='d'&&n>=0&&n<c->threat_step_count){memmove(&c->threat_steps[n],&c->threat_steps[n+1],sizeof(c->threat_steps[0])*(c->threat_step_count-n-1));c->threat_step_count--;OLC_VAL(d)=1;}medit_disp_ai_threat_sequence(d);return; }
  case MEDIT_AI_ENABLE_CONFIRM: if (LOWER(*arg)=='y') { SET_BIT_AR(MOB_FLAGS(OLC_MOB(d)), MOB_AI_ACTOR); OLC_VAL(d)=1; medit_disp_ai_menu(d); return; } if (LOWER(*arg)=='n') { medit_disp_menu(d); return; } write_to_output(d,"Please answer Y or N: "); return;
  case MEDIT_AI_MODE:
  case MEDIT_AI_ROLE:
  case MEDIT_AI_MOVEMENT:
    if (!OLC_MOB(d)->ai_config) OLC_MOB(d)->ai_config = mob_ai_config_new();
    if (OLC_MODE(d) == MEDIT_AI_MODE) OLC_MOB(d)->ai_config->mode = atoi(arg);
    else if (OLC_MODE(d) == MEDIT_AI_ROLE) { OLC_MOB(d)->ai_config->role = atoi(arg); OLC_MOB(d)->ai_config->override_mask |= AI_OVERRIDE_ROLE; }
    else { OLC_MOB(d)->ai_config->movement = atoi(arg); OLC_MOB(d)->ai_config->override_mask |= AI_OVERRIDE_MOVEMENT; }
    mob_ai_config_validate(OLC_MOB(d)->ai_config); OLC_VAL(d) = 1; medit_disp_ai_menu(d); return;

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
    OLC_VAL(d) = TRUE;
    medit_disp_stats_menu(d);
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
