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

#define AI_MEDIT_CLAMP(v, lo, hi) ((v) < (lo) ? (lo) : ((v) > (hi) ? (hi) : (v)))

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
static void medit_disp_ai_compatibility(struct descriptor_data *d)
{
  char report[MAX_STRING_LENGTH];
  ai_actor_compatibility_report(OLC_MOB(d), report, sizeof(report), FALSE);
  write_to_output(d, "%s", report);
  OLC_MODE(d) = MEDIT_AI_COMPATIBILITY;
}
static const char *medit_ai_state(struct descriptor_data *d, int enabled);
static void medit_disp_ai_mode(struct descriptor_data *d)
{
  struct mob_ai_config *c = OLC_MOB(d)->ai_config;
  write_to_output(d, "\r\nAI Actor Profile Mode\r\n0) Inferred\r\n   Compilation derives profile values from prototype data.\r\n1) Custom\r\n   Compilation uses stored configuration values directly.\r\n2) Overrides\r\n   Compilation starts inferred and applies only fields in the override mask.\r\nCurrent mode: %s\r\nStored values are preserved when changing modes; Reset discards them.\r\nH) Help  Q) Return\r\nChoice: ", c->mode == MOB_AI_CUSTOM ? "Custom" : c->mode == MOB_AI_INFERRED_OVERRIDES ? "Overrides" : "Inferred");
  OLC_MODE(d) = MEDIT_AI_MODE;
}
static void medit_disp_ai_role(struct descriptor_data *d)
{
  int i; struct mob_ai_config *c = OLC_MOB(d)->ai_config;
  write_to_output(d, "\r\nAI Actor Role\r\n");
  for (i = ROLE_UNKNOWN; i <= ROLE_BOSS; i++)
    write_to_output(d, "%d) %s\r\n   %s\r\n", i, ai_actor_config_role_name(i), ai_actor_config_role_summary(i));
  write_to_output(d, "Current role: %s\r\nH) Help  Q) Return\r\nChoice: ", ai_actor_config_role_name(c->role));
  OLC_MODE(d) = MEDIT_AI_ROLE;
}
static void medit_disp_ai_movement(struct descriptor_data *d)
{
  int i; struct mob_ai_config *c = OLC_MOB(d)->ai_config;
  write_to_output(d, "\r\nAI Actor Movement\r\n");
  for (i = AI_MOVE_STATIONARY; i <= AI_MOVE_RETURN_HOME; i++)
    write_to_output(d, "%d) %s\r\n   %s\r\n", i, ai_actor_config_movement_name(i), ai_actor_config_movement_summary(i));
  if (MOB_FLAGGED(OLC_MOB(d), MOB_SENTINEL) && c->movement == AI_MOVE_RANDOM)
    write_to_output(d, "Warning: SENTINEL prevents normal random movement.\r\n");
  write_to_output(d, "Current movement: %s\r\nH) Help  Q) Return\r\nChoice: ", ai_actor_config_movement_name(c->movement));
  OLC_MODE(d) = MEDIT_AI_MOVEMENT;
}
static int medit_parse_ai_integer(const char *arg, int minimum, int maximum, int *value);
static int medit_parse_ai_boolean(const char *arg, int *value);
static int medit_is_ai_mode(int mode);
static void medit_disp_ai_personality(struct descriptor_data *d);
static void medit_disp_ai_social(struct descriptor_data *d);
static void medit_disp_ai_dialogue(struct descriptor_data *d);
static void medit_disp_ai_perception(struct descriptor_data *d);
static void medit_disp_ai_memory(struct descriptor_data *d);
static void medit_disp_ai_threat(struct descriptor_data *d);
static void medit_disp_ai_combat(struct descriptor_data *d);
static void medit_disp_ai_schedule(struct descriptor_data *d);
static void medit_disp_ai_patrol_routes(struct descriptor_data *d);
static void medit_disp_ai_capabilities(struct descriptor_data *d);
static void medit_disp_ai_vocalizations(struct descriptor_data *d);
static const char *medit_ai_communication_summary(const struct mob_ai_config *c)
{
  return c->communication == AI_COMM_SPEAK ? "Speaks" :
         c->communication == AI_COMM_VOCALIZE ? "Creature sounds" : "Silent";
}
/* Older configurations have no separate preset field.  This deliberately derives
 * a label from their existing compiled inputs without rewriting those inputs. */
static const char *medit_ai_intelligence_summary(const struct mob_ai_config *c)
{
  if (c->archetype == AI_ARCH_MINDLESS || c->memory_style == AI_MEMORY_NONE) return "Mindless";
  if (c->archetype == AI_ARCH_BEAST) return "Animal";
  if (c->observation_sensitivity >= 80 || c->memory_style == AI_MEMORY_FULL_RELATIONSHIP) return "Brilliant";
  if (c->observation_sensitivity >= 65 || c->memory_style == AI_MEMORY_SOCIAL) return "Clever";
  if (c->observation_sensitivity >= 45 || c->memory_style == AI_MEMORY_BASIC_HOSTILE) return "Average";
  return "Simple";
}
static void medit_disp_ai_communication(struct descriptor_data *d)
{
  struct mob_ai_config *c = OLC_MOB(d)->ai_config;
  write_to_output(d, "\r\nCommunication\r\n-------------\r\n\r\n1) Silent\r\n   The NPC does not speak or make authored creature sounds.\r\n2) Creature Sounds\r\n   The NPC uses creature vocalization lines, not normal spoken dialogue.\r\n3) Speaks\r\n   The NPC may use dialogue, greetings, warnings, and replies.\r\n\r\nCurrent: %s\r\nH) Help  Q) Cancel\r\nChoice: ", medit_ai_communication_summary(c));
  OLC_MODE(d) = MEDIT_AI_COMMUNICATION;
}
static void medit_disp_ai_intelligence(struct descriptor_data *d)
{
  write_to_output(d, "\r\nIntelligence\r\n------------\r\n\r\n1) Mindless  - immediate instinct and minimal reasoning\r\n2) Animal    - danger, territory, and kindred allies\r\n3) Simple    - basic decisions and common interactions\r\n4) Average   - ordinary perception, memory, and judgment\r\n5) Clever    - stronger recognition and tactical decisions\r\n6) Brilliant - strong perception, long memory, and planning\r\n\r\nCurrent effective level: %s\r\nSelecting a level updates only AI thinking defaults; dialogue and schedules are preserved.\r\nH) Help  Q) Cancel\r\nChoice: ", medit_ai_intelligence_summary(OLC_MOB(d)->ai_config));
  OLC_MODE(d) = MEDIT_AI_INTELLIGENCE;
}
static void medit_disp_ai_advanced(struct descriptor_data *d)
{
  write_to_output(d, "\r\nAdvanced AI Brain\r\n-----------------\r\n\r\n1) Personality and Social Style\r\n2) Perception and Awareness\r\n3) Memory Details\r\n4) Threat Response\r\n5) Combat Reactions\r\n6) Assistance Rules\r\n7) Capability Overrides\r\n8) Movement Internals\r\n9) Profile and Inference\r\nP) Technical Compiled Profile\r\nV) Validation Information\r\nD) Technical Diagnostics\r\nR) Reset Inferred Defaults\r\n\r\nH) Help\r\nQ) Return\r\nChoice: ");
  OLC_MODE(d) = MEDIT_AI_ADVANCED;
}
static void medit_disp_ai_diagnostics(struct descriptor_data *d)
{
  struct mob_ai_config *c = OLC_MOB(d)->ai_config;
  int spoken = 0, i;
  for (i = 0; i < AI_DIALOGUE_CATEGORIES; i++) spoken += c->dialogue_count[i];
  write_to_output(d, "\r\nDiagnostics\r\n-----------\r\n\r\n");
  if (c->communication == AI_COMM_VOCALIZE && !c->vocalization_count) write_to_output(d, "INFO\r\nCommunication is set to Creature Sounds, but no creature sound lines exist.\r\n\r\n");
  if (c->communication == AI_COMM_NONE && (spoken || c->vocalization_count)) write_to_output(d, "WARNING\r\nCommunication is Silent, so authored dialogue and creature sounds will not run.\r\n\r\n");
  if (c->schedule_enabled && MOB_FLAGGED(OLC_MOB(d), MOB_SENTINEL)) write_to_output(d, "WARNING\r\nSENTINEL prevents the active schedule from moving this NPC.\r\n\r\n");
  if (!c->schedule_enabled) write_to_output(d, "INFO\r\nThis NPC has no active schedule and will use normal MEDIT movement behavior.\r\n\r\n");
  if (!ai_actor_compatibility_warning_count(OLC_MOB(d))) write_to_output(d, "Ready\r\nNo actionable configuration warnings.\r\n\r\n");
  write_to_output(d, "P) Preview Effective Behavior\r\nD) Advanced Technical Diagnostics\r\nH) Help\r\nQ) Return\r\nChoice: ");
  OLC_MODE(d) = MEDIT_AI_DIAGNOSTICS;
}
static void medit_disp_ai_help(struct descriptor_data *d, int return_mode, const char *title,
                               const char *explanation, const char *tips, const char *related)
{
  if (OLC_STORAGE(d)) free(OLC_STORAGE(d));
  CREATE(OLC_STORAGE(d), char, 24);
  snprintf(OLC_STORAGE(d), 24, "help %d", return_mode);
  write_to_output(d, "\r\n------------------------------\r\nAI Actor %s Help\r\n------------------------------\r\n\r\n%s\r\n\r\nEditing tips: %s\r\nRelated systems: %s\r\n\r\nPress ENTER to return.\r\n", title, explanation, tips, related);
  OLC_MODE(d) = MEDIT_AI_HELP;
}
static void medit_return_from_ai_help(struct descriptor_data *d)
{
  int mode = MEDIT_AI_MENU;
  if (OLC_STORAGE(d)) { sscanf(OLC_STORAGE(d), "help %d", &mode); free(OLC_STORAGE(d)); OLC_STORAGE(d) = NULL; }
  switch (mode) {
    case MEDIT_AI_MODE: medit_disp_ai_mode(d); break; case MEDIT_AI_ROLE: medit_disp_ai_role(d); break;
    case MEDIT_AI_MOVEMENT: medit_disp_ai_movement(d); break; case MEDIT_AI_PERSONALITY: medit_disp_ai_personality(d); break;
  case MEDIT_AI_SOCIAL: medit_disp_ai_social(d); break; case MEDIT_AI_DIALOGUE: medit_disp_ai_dialogue(d); break;
    case MEDIT_AI_PERCEPTION: medit_disp_ai_perception(d); break; case MEDIT_AI_MEMORY: medit_disp_ai_memory(d); break;
    case MEDIT_AI_THREAT: medit_disp_ai_threat(d); break; case MEDIT_AI_COMBAT: medit_disp_ai_combat(d); break;
    case MEDIT_AI_SCHEDULE: medit_disp_ai_schedule(d); break; case MEDIT_AI_PATROL_ROUTES: medit_disp_ai_patrol_routes(d); break;
    case MEDIT_AI_CAPABILITIES: medit_disp_ai_capabilities(d); break; case MEDIT_AI_VOCALIZATIONS: medit_disp_ai_vocalizations(d); break;
    case MEDIT_AI_COMMUNICATION: medit_disp_ai_communication(d); break;
    case MEDIT_AI_INTELLIGENCE: medit_disp_ai_intelligence(d); break;
    case MEDIT_AI_DIAGNOSTICS: medit_disp_ai_diagnostics(d); break;
    case MEDIT_AI_ADVANCED: medit_disp_ai_advanced(d); break;
    default: medit_disp_ai_menu(d); break;
  }
}
static const char *ai_perception_summary[] = {
  "Processes room-entry events.", "Processes room-departure events.",
  "Processes ordinary nearby speech.", "Processes whisper events.",
  "Processes emote/social events.", "Processes nearby combat events.",
  "Processes direct attacks on this NPC.", "Processes attacks on local allies.",
  "Processes corpse events.", "Processes nearby drop events.",
  "Processes give events addressed to this NPC.", "Processes recorded theft/crime events.",
  "Stored sensitivity; runtime consumers are limited.", "Stored sensitivity; runtime consumers are limited.",
  "Stored threshold used by threat escalation.", "Minimum identity confidence for targeted threat steps."
};
static const char *ai_memory_summary[] = {
  "Enables live per-actor memories; they are not saved across reboot.", "Maximum live actor records (shared pool; oldest expires first).",
  "Lifetime of ordinary records in seconds.", "Lifetime of important records in seconds.",
  "Trust score added by a positive memory.", "Trust score removed by a negative memory.",
  "Fear score added by a fearful event.", "Fear decay per lifecycle update.",
  "Hostility score added by a hostile event.", "Hostility decay per lifecycle update.",
  "Familiarity score added by contact.", "Familiarity decay per lifecycle update.", "Forgiveness applied during decay.",
  "Remember direct attackers.", "Remember assistance events.", "Remember recorded crimes.", "Remember gifts.",
  "Stored option; no insult event producer found.", "Remember conversation/help events.", "Remember threats.",
  "Stored option; no last-room consumer found.", "Remember defeats/deaths."
};
static void medit_disp_ai_perception(struct descriptor_data *d)
{
  struct mob_ai_config *c=OLC_MOB(d)->ai_config; int i; int *v=&c->notice_entry;
  write_to_output(d,"\r\nAI Actor Perception\r\n");
  for(i=0;i<12;i++) write_to_output(d,"%c) %-18s: %s\r\n   %s\r\n",i<9?'1'+i:'A'+i-9,
    (const char *[]){"Notice Entry","Notice Departure","Notice Speech","Notice Whispers","Notice Emotes","Notice Combat","Attacks Self","Attacks Allies","Corpses","Drops","Gifts","Crimes","Hearing","Observation","Suspicion","Recognition"}[i],
    v[i]?"Enabled":"Disabled",ai_perception_summary[i]);
  /* Numeric values need a separate line; avoid treating them as booleans. */
  for(i=12;i<16;i++) write_to_output(d,"%c) %-18s: %d (0-100 score)\r\n   %s\r\n",'D'+i-12,
    (const char *[]){"Hearing","Observation","Suspicion","Recognition"}[i-12],v[i],ai_perception_summary[i]);
  write_to_output(d,"H) Help (event gates, scores, and limitations)\r\nQ) Return\r\nChoice: "); OLC_MODE(d)=MEDIT_AI_PERCEPTION;
}
static const char *threat_name(int n) { static const char *x[]={"Observe","Warn","Challenge","Call Help","Assist (stored; no direct step consumer)","Follow (unsupported)","Arrest (unsupported)","Attack","Flee","Surrender (unsupported)","Ignore"}; return n>=0&&n<AI_THREAT_RESPONSE_MAX?x[n]:"Invalid"; }
static void medit_disp_ai_threat(struct descriptor_data *d)
{ struct mob_ai_config*c=OLC_MOB(d)->ai_config;int i;write_to_output(d,"\r\nAI Actor Threat Response\r\nEnabled responses gate escalation steps; Follow, Arrest, and Surrender are stored but unsupported.\r\n");for(i=0;i<10;i++)write_to_output(d,"%c) %s: %s\r\n   %s\r\n",i<9?'1'+i:'A',threat_name(i),c->threat_enabled[i]?"Enabled":"Disabled",i==0?"Observes severity before escalation.":i==3?"Dispatches eligible nearby allies.":i==7?"Starts combat when the step succeeds.":i==8?"Attempts configured combat flee behavior.":"Stored configuration; inspect runtime support before relying on it.");write_to_output(d,"B) Cooldown: %d seconds\r\nC) Calm reset: %d seconds\r\nD) Repeat window: %d seconds\r\nE) Escalation sequence\r\nH) Help  R) Reset  Q) Return\r\nChoice: ",c->threat_cooldown,c->calm_reset_time,c->repeated_event_window);OLC_MODE(d)=MEDIT_AI_THREAT; }
static void medit_disp_ai_threat_sequence(struct descriptor_data *d) { struct mob_ai_config*c=OLC_MOB(d)->ai_config;int i;write_to_output(d,"\r\nThreat Escalation Sequence\r\nEach step uses severity (0-100), cooldown seconds, and repetition limit.\r\n");for(i=0;i<c->threat_step_count;i++)write_to_output(d,"%d) %s Severity %d Cooldown %d seconds Repeats %d Advance %s\r\n",i+1,threat_name(c->threat_steps[i].type),c->threat_steps[i].minimum_severity,c->threat_steps[i].cooldown,c->threat_steps[i].max_repetitions,c->threat_steps[i].advance_on_failure?"Yes":"No");write_to_output(d,"A <type severity cooldown repeats advance>; E <line type severity cooldown repeats advance>; D/U/N <line>\r\nH) Help  Q) Return\r\nChoice: ");OLC_MODE(d)=MEDIT_AI_THREAT_SEQUENCE; }
static void medit_disp_ai_combat(struct descriptor_data *d) { struct mob_ai_config*c=OLC_MOB(d)->ai_config; write_to_output(d,"\r\nAI Actor Combat Reactions\r\n1) Style: %d\r\n   Selects the compiled combat style.\r\n2) Combat Enabled: %s\r\n   Gates combat decision processing.\r\n3) Initiate: %s  4) Assist Allies: %s  5) Call Help: %s  6) Flee: %s\r\n   Eligibility gates; they do not assign priority.\r\n7) Protect Trusted: %s  8) Protect Group: %s  9) Same Role: %s  A) Same Prototype: %s\r\nB) Retaliate Self: %s  C) Retaliate Ally: %s  D) Retaliate Hostile: %s  E) Switch Targets: %s\r\nF) Flee Health Threshold: %d%% current hit points\r\nG) Assist Severity: %d (0-100 threat score)\r\nH) Switch Threshold: %d (0-100 threat score)\r\nI) Decision Cooldown: %d seconds\r\nJ) Target weights\r\nK) Preview L) Validate\r\nH) Help  Q) Return\r\nChoice: ",c->combat_style,c->combat_enabled?"Enabled":"Disabled",c->may_initiate?"Enabled":"Disabled",c->may_assist?"Enabled":"Disabled",c->may_call_help?"Enabled":"Disabled",c->may_flee?"Enabled":"Disabled",c->protect_trusted?"Enabled":"Disabled",c->protect_group?"Enabled":"Disabled",c->protect_same_role?"Enabled":"Disabled",c->protect_same_prototype?"Enabled":"Disabled",c->retaliate_self?"Enabled":"Disabled",c->retaliate_ally?"Enabled":"Disabled",c->retaliate_hostile?"Enabled":"Disabled",c->switch_targets?"Enabled":"Disabled",c->flee_hp_percent,c->assist_severity,c->target_switch_threshold,c->combat_cooldown); OLC_MODE(d)=MEDIT_AI_COMBAT; }
static void medit_disp_ai_targets(struct descriptor_data *d) { static const char *names[] = { "Current attacker", "Attacker of trusted actor", "Attacker of group member", "Known hostile", "Lowest health", "Player character", "NPC", "Previous target" }; struct mob_ai_config*c=OLC_MOB(d)->ai_config; int i; write_to_output(d,"\r\nCombat Target Weights\r\nPositive weights make an eligible target more attractive; negative weights discourage it. This is scoring, not fixed ordering.\r\n"); for(i=0;i<AI_TARGET_WEIGHTS;i++) write_to_output(d,"%d) %s: %d (-100..100)\r\n   Eligibility/scoring input used by target selection.\r\n",i+1,names[i],c->target_weight[i]); write_to_output(d,"Unsupported by current runtime: spellcaster, healer, ranged attacker, criminal, feared actor, highest damage output.\r\nH) Help  Q) Return\r\nChoice: "); OLC_MODE(d)=MEDIT_AI_TARGETS; }
static void medit_disp_ai_memory(struct descriptor_data *d) { struct mob_ai_config*c=OLC_MOB(d)->ai_config;int i;int *v=&c->memory_enabled;write_to_output(d,"\r\nAI Actor Memory\r\nMemory records live NPC instances only; prototype settings persist, records do not.\r\n");for(i=0;i<22;i++)write_to_output(d,"%c) %s: %d\r\n   %s\r\n",i<9?'1'+i:'A'+i-9,(const char *[]){"Memory Enabled","Capacity","Ordinary Duration","Important Duration","Trust Gain","Trust Loss","Fear Gain","Fear Decay","Hostility Gain","Hostility Decay","Familiarity Gain","Familiarity Decay","Forgiveness","Remember Attacks","Remember Assistance","Remember Crimes","Remember Gifts","Remember Insults","Remember Conversations","Remember Threats","Remember Last Room","Remember Deaths"}[i],v[i],ai_memory_summary[i]);write_to_output(d,"Durations are seconds; capacity is actors (maximum %d). Scores are stored modifiers, not percentages.\r\nH) Help  Q) Return\r\nChoice: ",AI_MEM_MAX);OLC_MODE(d)=MEDIT_AI_MEMORY; }
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
static const char *ai_trait_summaries[AI_ACTOR_PERSONALITIES] = {
  "How likely the NPC is to respond with force instead of diplomacy.",
  "How willing the NPC is to continue fighting despite danger.",
  "How readily the NPC starts or joins conversations.",
  "How interested the NPC is in investigating unusual events.",
  "How closely the NPC follows routines and responsibilities.",
  "How truthful the NPC tends to be during dialogue.",
  "How strongly rewards influence decisions.",
  "How much concern the NPC shows toward others.",
  "How resistant the NPC is to betrayal.",
  "How long the NPC tolerates delays or annoyance.",
  "How quickly strangers become potential threats.",
  "How strongly insults and status affect reactions."
};
static const char *medit_ai_personality_summary(const struct mob_ai_config *c)
{
  int i, highest = 0, lowest = 0, deviation = 0;
  for (i = 1; i < AI_ACTOR_PERSONALITIES; i++) {
    if (c->personality[i] > c->personality[highest]) highest = i;
    if (c->personality[i] < c->personality[lowest]) lowest = i;
  }
  for (i = 0; i < AI_ACTOR_PERSONALITIES; i++) deviation += c->personality[i] >= 50 ? c->personality[i] - 50 : 50 - c->personality[i];
  if (deviation <= 24) return "Balanced profile — average personality values (50).";
  if (c->personality[AI_TRAIT_AGGRESSION] >= 70 && c->personality[AI_TRAIT_COMPASSION] <= 35)
    return "Aggressive personality — high aggression, low compassion.";
  if (c->personality[highest] >= 65)
    return (highest == AI_TRAIT_CURIOSITY) ? "Highly curious — curiosity is the dominant trait." : ai_trait_names[highest];
  return "Mixed personality profile — review the trait values below.";
}

static void medit_disp_ai_personality(struct descriptor_data *d)
{
  struct mob_ai_config *c = OLC_MOB(d)->ai_config; int i;
  write_to_output(d, "\r\nAI Actor Personality (0-100)\r\n%s\r\n", medit_ai_personality_summary(c));
  for (i = 0; i < AI_ACTOR_PERSONALITIES; i++)
    write_to_output(d, "%c) %-12s: %d\r\n   %s\r\n", i < 9 ? '1' + i : 'A' + i - 9,
                    ai_trait_names[i], c->personality[i], ai_trait_summaries[i]);
  write_to_output(d, "P) Apply Preset\r\nH) Help  Q) Return\r\nChoice: "); OLC_MODE(d)=MEDIT_AI_PERSONALITY;
}
static void medit_disp_ai_social(struct descriptor_data *d)
{
  struct mob_ai_config *c = OLC_MOB(d)->ai_config;
  write_to_output(d, "\r\n%s                    AI Actor Social Behavior%s\r\n\r\n%sStyle%s\r\n  %s1)%s Social Style       : %s%s%s\r\n\r\n%sResponses%s\r\n  %s2)%s Greeting           : %s\r\n  %s5)%s Whisper Replies    : %s\r\n  %s6)%s Respond to Stranger: %s\r\n  %s7)%s Respond to Trusted : %s\r\n  %s8)%s Respond to Feared  : %s\r\n  %s9)%s Respond to Hostile : %s\r\n\r\n%sIdle Behavior%s\r\n  %s3)%s Ambient Speech     : %s%s\r\n  %s4)%s Ambient Emotes     : %s%s\r\n\r\n%sCooldowns%s\r\n  %sA)%s Speech Cooldown    : %s%d seconds%s\r\n  %sB)%s Room Cooldown      : %s%d seconds%s\r\n  %sC)%s Emote Cooldown     : %s%d seconds%s\r\n\r\n%sH)%s Help   %sQ)%s Return\r\nChoice: ", CCCYN(d->character,C_NRM),CCNRM(d->character,C_NRM),CCCYN(d->character,C_NRM),CCNRM(d->character,C_NRM),CCGRN(d->character,C_NRM),CCNRM(d->character,C_NRM),CCYEL(d->character,C_NRM),ai_social_style_name(c->social),CCNRM(d->character,C_NRM),CCCYN(d->character,C_NRM),CCNRM(d->character,C_NRM),CCGRN(d->character,C_NRM),CCNRM(d->character,C_NRM),medit_ai_state(d,c->greeting_enabled),CCGRN(d->character,C_NRM),CCNRM(d->character,C_NRM),medit_ai_state(d,c->whisper_enabled),CCGRN(d->character,C_NRM),CCNRM(d->character,C_NRM),medit_ai_state(d,c->respond_strangers),CCGRN(d->character,C_NRM),CCNRM(d->character,C_NRM),medit_ai_state(d,c->respond_trusted),CCGRN(d->character,C_NRM),CCNRM(d->character,C_NRM),medit_ai_state(d,c->respond_feared),CCGRN(d->character,C_NRM),CCNRM(d->character,C_NRM),medit_ai_state(d,c->respond_hostile),CCCYN(d->character,C_NRM),CCNRM(d->character,C_NRM),CCGRN(d->character,C_NRM),CCNRM(d->character,C_NRM),medit_ai_state(d,c->ambient_speech_enabled),c->ambient_speech_enabled && !c->dialogue_count[4] ? " [Warning: no ambient speech lines]" : "",CCGRN(d->character,C_NRM),CCNRM(d->character,C_NRM),medit_ai_state(d,c->ambient_emotes_enabled),c->ambient_emotes_enabled && !c->dialogue_count[5] ? " [Warning: no ambient emote lines]" : "",CCCYN(d->character,C_NRM),CCNRM(d->character,C_NRM),CCGRN(d->character,C_NRM),CCNRM(d->character,C_NRM),CCYEL(d->character,C_NRM),c->speech_cooldown,CCNRM(d->character,C_NRM),CCGRN(d->character,C_NRM),CCNRM(d->character,C_NRM),CCYEL(d->character,C_NRM),c->room_speech_cooldown,CCNRM(d->character,C_NRM),CCGRN(d->character,C_NRM),CCNRM(d->character,C_NRM),CCYEL(d->character,C_NRM),c->emote_cooldown,CCNRM(d->character,C_NRM),CCGRN(d->character,C_NRM),CCNRM(d->character,C_NRM),CCGRN(d->character,C_NRM),CCNRM(d->character,C_NRM));
  OLC_MODE(d)=MEDIT_AI_SOCIAL;
}

/* Counts are authored entries out of the maximum; requires authored ambient speech lines.
 * Minimum seconds between speech remains documented in Social Help. */
static const char *ai_dialogue_summaries[AI_DIALOGUE_CATEGORIES] = {
 "Entry greeting.", "Positive social response.", "Suspicious social response.", "Hostile social response.", "Idle spoken line.", "Idle descriptive action.", "Departure response.", "Warning response.", "Challenge response.", "Threat response.", "Call-for-help response.", "Fear response.", "Schedule departure line.", "Schedule arrival line.", "Schedule work line.", "Schedule guard line.", "Schedule patrol line.", "Schedule sleep line.", "Schedule wake line.", "Schedule failure line."
};
static void medit_disp_ai_dialogue_lines(struct descriptor_data *d, int category)
{ struct mob_ai_config *c=OLC_MOB(d)->ai_config; int i; write_to_output(d,"\r\n%s             %s Dialogue%s\r\n\r\n%s\r\nEntries: %d/%d\r\n\r\n",CCCYN(d->character,C_NRM),ai_dialogue_category_name(category),CCNRM(d->character,C_NRM),ai_dialogue_summaries[category],c->dialogue_count[category],AI_DIALOGUE_MAX_LINES); for(i=0;i<c->dialogue_count[category];i++) write_to_output(d,"  %d) %s\r\n",i+1,c->dialogue[category][i]); write_to_output(d,"\r\nCommands\r\n  A) Add a line  E) Edit a line  D) Delete a line\r\n  U) Move a line up  N) Move a line down\r\n  H) Help  Q) Return to categories\r\nChoice: "); OLC_MODE(d)=MEDIT_AI_DIALOGUE; }
static void medit_disp_ai_dialogue(struct descriptor_data *d) { struct mob_ai_config*c=OLC_MOB(d)->ai_config; int i,spoken=0; for(i=0;i<AI_DIALOGUE_CATEGORIES;i++) spoken+=c->dialogue_count[i]; write_to_output(d,"\r\nDialogue\r\n--------\r\n\r\nCommunication\r\n  %s\r\nSpoken Dialogue\r\n  %d authored lines%s\r\nCreature Sounds\r\n  %d authored lines%s\r\n\r\n1) Greetings\r\n2) Ambient Dialogue\r\n3) Replies and Questions\r\n4) Warnings and Threats\r\n5) Combat and Help Lines\r\n6) Farewells\r\n7) Creature Sounds\r\n8) Preview Dialogue\r\n\r\nH) Help\r\nQ) Return\r\nChoice: ",medit_ai_communication_summary(c),spoken,c->communication==AI_COMM_NONE?" (inactive)":"",c->vocalization_count,c->communication==AI_COMM_NONE?" (inactive)":""); OLC_MODE(d)=MEDIT_AI_DIALOGUE; }



static const char *medit_schedule_room_name(int vnum) { room_rnum r; if (!vnum) return "None"; r=real_room(vnum); return r==NOWHERE ? "INVALID" : world[r].name; }
static const char *medit_sched_activity(int v) { static const char *n[]={"Remain","Travel","Patrol","Idle Socially","Guard","Work","Sleep","Rest","Return Home"}; return v>=0&&v<AI_SCHEDULE_ACTIVITY_MAX?n[v]:"INVALID"; }
static const char *medit_sched_dest(int v) { static const char *n[]={"Current Room","Specific Room","Home Room","Work Room","Sleep Room","Guard Room","Fallback Room","Patrol Route","Spawn Room"}; return v>=0&&v<AI_DESTINATION_MAX?n[v]:"INVALID"; }
static const char *medit_sched_action(int v) { static const char *n[]={"None","Speak","Emote","Sit","Rest","Sleep","Stand","Wake","Begin Patrol","Guard Silently"}; return v>=0&&v<AI_SCHEDULE_ACTION_MAX?n[v]:"INVALID"; }
static const char *medit_sched_failure(int v) { static const char *n[]={"Wait and Retry","Skip Entry","Restart Activity","Use Fallback","Abort Until Next Activation","Disable Until Schedule Changes"}; return v>=0&&v<AI_FAILURE_MAX?n[v]:"INVALID"; }
static const char *medit_sched_interrupt(int v) { static const char *n[]={"Ignore Minor","Pause and Resume","Restart Activity","Skip Entry","Abort Until Next Activation","Return to Fallback"}; return v>=0&&v<AI_INTERRUPT_MAX?n[v]:"INVALID"; }
static const char *medit_sched_days(int m) { if(m==AI_DAY_MASK_ALL)return "Every Day"; if(m==0x1f)return "Weekdays"; if(m==0x60)return "Weekends"; return m>0&&m<=AI_DAY_MASK_ALL?"Custom":"INVALID"; }
static const char *medit_patrol_mode(int v) { static const char *n[]={"Loop","Ping-pong","Once"}; return v>=AI_PATROL_LOOP&&v<=AI_PATROL_ONCE?n[v]:"Invalid/Unavailable"; }
static void medit_schedule_store(struct descriptor_data *d, int route, int entry, int waypoint) { if(OLC_STORAGE(d)) free(OLC_STORAGE(d)); CREATE(OLC_STORAGE(d),char,48); snprintf(OLC_STORAGE(d),48,"%d %d %d",route,entry,waypoint); }
static void medit_schedule_load(struct descriptor_data *d,int *route,int *entry,int *waypoint) { *route=*entry=*waypoint=-1; if(OLC_STORAGE(d)) sscanf(OLC_STORAGE(d),"%d %d %d",route,entry,waypoint); }
static struct ai_schedule_entry *medit_selected_entry(struct descriptor_data *d) { int r,id,w,i; medit_schedule_load(d,&r,&id,&w); for(i=0;i<OLC_MOB(d)->ai_config->schedule_count;i++)if(OLC_MOB(d)->ai_config->schedules[i].id==id)return &OLC_MOB(d)->ai_config->schedules[i]; return NULL; }
static struct ai_patrol_route *medit_selected_route(struct descriptor_data *d) { int id,e,w,i; medit_schedule_load(d,&id,&e,&w); for(i=0;i<OLC_MOB(d)->ai_config->patrol_count;i++)if(OLC_MOB(d)->ai_config->patrols[i].id==id)return &OLC_MOB(d)->ai_config->patrols[i]; return NULL; }
static void medit_disp_ai_schedule(struct descriptor_data *d) { struct mob_ai_config*c=OLC_MOB(d)->ai_config; write_to_output(d,"\r\nSchedule\r\n--------\r\n\r\nStatus\r\n  %s\r\nEntries\r\n  %d\r\nPatrol Routes\r\n  %d\r\n\r\n1) Enable or Disable Schedule\r\n2) Edit Daily Schedule\r\n3) Edit Patrol Routes\r\n4) Preview Today's Routine\r\n5) Validate Destinations\r\n\r\nNormal MEDIT movement flags remain authoritative outside active routine entries.\r\nH) Help\r\nQ) Return\r\nChoice: ",c->schedule_enabled?"Enabled":"Disabled",c->schedule_count,c->patrol_count); OLC_MODE(d)=MEDIT_AI_SCHEDULE; }
static void medit_disp_ai_schedule_entries(struct descriptor_data *d) { struct mob_ai_config*c=OLC_MOB(d)->ai_config;int i;write_to_output(d,"\r\n%s                    AI Actor Schedule Entries%s\r\n\r\n #  State     Time       Days        Priority  Activity\r\n",CCCYN(d->character,C_NRM),CCNRM(d->character,C_NRM));for(i=0;i<c->schedule_count;i++){struct ai_schedule_entry*e=&c->schedules[i];write_to_output(d," %d  %-8s  %-9s %-11s %-9d %s\r\n",i+1,e->enabled?"Enabled":"Disabled",e->start_hour==0&&e->end_hour==0?"All Day":"game time",medit_sched_days(e->day_mask),e->priority,medit_sched_activity(e->activity));}write_to_output(d,"\r\nCommands\r\n  A) Add Entry  E) Edit Entry  D) Delete Entry  U) Move Entry Up\r\n  N) Move Entry Down  C) Duplicate Entry  T) Enable/Disable Entry\r\n  H) Help  Q) Return\r\nChoice: ");OLC_MODE(d)=MEDIT_AI_SCHEDULE_ENTRIES; }

static void medit_disp_ai_schedule_entry(struct descriptor_data*d) { struct ai_schedule_entry*e=medit_selected_entry(d);room_rnum r;if(!e){write_to_output(d,"Invalid entry index.\r\n");medit_disp_ai_schedule_entries(d);return;}r=real_room(e->destination_value);write_to_output(d,"\r\n                 AI Actor Schedule Entry\r\n\r\n  Stable ID                  : %d\r\n\r\n  1) Enabled                : %s\r\n  2) Start Hour             : %02d\r\n  3) End Hour               : %02d\r\n  4) Day Mask               : %s\r\n  5) Priority               : %d\r\n  6) Activity               : %s\r\n  7) Destination Type       : %s\r\n  8) Destination Value      : %s\r\n  9) Patrol Route           : %d\r\n  A) Arrival Action         : %s\r\n  B) Departure Action       : %s\r\n  C) Interruption Policy    : %s\r\n  D) Failure Policy         : %s\r\n  E) Maximum Travel Time    : %d\r\n  F) Maximum Attempts       : %d\r\n  G) Wait/Retry Duration    : %d\r\n  H) Dialogue Category      : Schedule Arrival\r\n  V) Preview Entry  X) Validate Entry  Q) Return\r\nChoice: ",e->id,e->enabled?"Yes":"No",e->start_hour,e->end_hour,medit_sched_days(e->day_mask),e->priority,medit_sched_activity(e->activity),medit_sched_dest(e->destination),e->destination==AI_DEST_ROOM_VNUM?(r==NOWHERE?"INVALID":world[r].name):"Not Applicable",e->route_id,medit_sched_action(e->arrival_action),medit_sched_action(e->departure_action),medit_sched_interrupt(e->interruption_policy),medit_sched_failure(e->failure_policy),e->max_travel_time,e->max_attempts,e->wait_duration);OLC_MODE(d)=MEDIT_AI_SCHEDULE_ENTRY; }
static void medit_disp_ai_patrol_routes(struct descriptor_data*d) {struct mob_ai_config*c=OLC_MOB(d)->ai_config;int i;write_to_output(d,"\r\n                   AI Actor Patrol Routes\r\n\r\n");for(i=0;i<c->patrol_count;i++){struct ai_patrol_route*r=&c->patrols[i];write_to_output(d,"  %d) [%s] ID %d  %-20s %-18s %d waypoints\r\n",i+1,r->enabled?"Enabled":"Disabled",r->id,r->label,medit_patrol_mode(r->loop_mode),r->waypoint_count);}write_to_output(d,"\r\n  A) Add Route  E) Edit Route  D) Delete Route  U) Move Route Up\r\n  N) Move Route Down  C) Duplicate Route  T) Toggle Enabled\r\n  H) Help  Q) Return\r\nChoice: ");OLC_MODE(d)=MEDIT_AI_PATROL_ROUTES;}
static void medit_disp_ai_patrol_route(struct descriptor_data*d) {struct ai_patrol_route*r=medit_selected_route(d);if(!r){write_to_output(d,"Invalid route index.\r\n");medit_disp_ai_patrol_routes(d);return;}write_to_output(d,"\r\n                   AI Actor Patrol Route\r\n\r\n  Stable Route ID           : %d\r\n\r\n  1) Builder Label          : %s\r\n  2) Enabled                : %s\r\n  3) Route Mode             : %s\r\n  4) Failure Policy         : %s\r\n  5) Waypoints              : %d / %d\r\n  V) Preview Route  X) Validate Route  Q) Return\r\nChoice: ",r->id,r->label,r->enabled?"Yes":"No",medit_patrol_mode(r->loop_mode),medit_sched_failure(r->failure_policy),r->waypoint_count,AI_PATROL_WAYPOINT_MAX);OLC_MODE(d)=MEDIT_AI_PATROL_ROUTE;}
static void medit_disp_ai_patrol_waypoints(struct descriptor_data*d){struct ai_patrol_route*r=medit_selected_route(d);int i; if(!r){medit_disp_ai_patrol_routes(d);return;}write_to_output(d,"\r\n              Patrol Waypoints: %s\r\n\r\n",r->label);for(i=0;i<r->waypoint_count;i++)write_to_output(d,"  %d) Room %d - %s  Wait %d\r\n",i+1,r->waypoints[i].room_vnum,medit_schedule_room_name(r->waypoints[i].room_vnum),r->waypoints[i].wait_duration);write_to_output(d,"\r\n  A) Add Waypoint  E) Edit Waypoint  D) Delete Waypoint  U) Move Waypoint Up\r\n  N) Move Waypoint Down  C) Duplicate Waypoint\r\n  H) Help  Q) Return\r\nChoice: ");OLC_MODE(d)=MEDIT_AI_PATROL_WAYPOINTS;}
static void medit_disp_ai_patrol_waypoint(struct descriptor_data*d){struct ai_patrol_route*r=medit_selected_route(d);int rid,e,w;if(!r){medit_disp_ai_patrol_routes(d);return;}medit_schedule_load(d,&rid,&e,&w);if(w<0||w>=r->waypoint_count){write_to_output(d,"Invalid waypoint index.\r\n");medit_disp_ai_patrol_waypoints(d);return;}write_to_output(d,"\r\n                 AI Actor Patrol Waypoint\r\n\r\n  1) Room                   : %d - %s\r\n  2) Wait Duration          : %d\r\n  3) Arrival Action         : %s\r\n  Q) Return\r\nChoice: ",r->waypoints[w].room_vnum,medit_schedule_room_name(r->waypoints[w].room_vnum),r->waypoints[w].wait_duration,medit_sched_action(r->waypoints[w].arrival_action));OLC_MODE(d)=MEDIT_AI_PATROL_WAYPOINT;}

/* Display main menu. */
static void medit_disp_menu(struct descriptor_data *d)
{
  struct char_data *mob;
  char flags[MAX_STRING_LENGTH], flag2[MAX_STRING_LENGTH];
  char price_buf[MAX_INPUT_LENGTH];
  char ai_status[96];

  mob = OLC_MOB(d);
  get_char_colors(d->character);
  clear_screen(d);

  if (GET_PET_PRICE(mob) > 0)
    snprintf(price_buf, sizeof(price_buf), "%d", GET_PET_PRICE(mob));
  else
    strlcpy(price_buf, "(default)", sizeof(price_buf));
  if (MOB_FLAGGED(mob, MOB_AI_ACTOR)) {
    int warnings = ai_actor_compatibility_warning_count(mob);
    snprintf(ai_status, sizeof(ai_status), "Enabled [%s]", warnings ? "compatibility warnings present" : "No compatibility warnings");
    if (warnings) snprintf(ai_status, sizeof(ai_status), "Enabled [%d compatibility warnings]", warnings);
  } else strlcpy(ai_status, "Disabled -- select I to enable AI Actor behavior", sizeof(ai_status));

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
          grn, nrm, yel, ai_status, nrm,
          grn, nrm, cyn, OLC_SCRIPT(d) ?"Set.":"Not Set.",
          grn, nrm,
          grn, nrm,
	  grn, nrm
	  );

  OLC_MODE(d) = MEDIT_MAIN_MENU;
}

static const char *medit_ai_state(struct descriptor_data *d, int enabled)
{
  (void)d;
  return enabled ? "Enabled" : "Disabled";
}

static const char *medit_ai_capability_source(const struct mob_ai_config *c, int which)
{
  int value = which == 1 ? c->archetype : which == 2 ? c->communication :
              which == 3 ? c->memory_style : c->assistance_style;
  return value < 0 ? "Inferred from the NPC profile" : "Builder selection";
}

static void medit_disp_ai_capability_picker(struct descriptor_data *d, int which)
{
  static const char *archetype[] = { "Humanoid — ordinary speaking person.", "Beast — animal-like creature.", "Monster — large hostile creature.", "Mindless — instinct only; no social relationships.", "Construct — artificial and predictable.", "Undead — dead creature with creature-like speech.", "Service NPC — guard, merchant, or other service role." };
  static const char *communication[] = { "None — never communicates.", "Vocalize — uses authored creature sounds instead of speech.", "Speak — uses ordinary dialogue.", "Telepathy — selected profile; delivery is not available." };
  static const char *memory[] = { "None — forgets everyone.", "Basic Hostile Memory — remembers threats only.", "Social Memory — remembers relationships.", "Full Relationship Memory — preserves the richest relationship detail." };
  static const char *assistance[] = { "None — never helps allies.", "Same Kind — helps matching creatures.", "Faction — helps faction allies.", "Any Ally — helps eligible nearby allies." };
  const char **choices = which == 1 ? archetype : which == 2 ? communication : which == 3 ? memory : assistance;
  const char *title = which == 1 ? "Archetype" : which == 2 ? "Communication" : which == 3 ? "Memory Style" : "Assistance";
  int count = which == 1 ? 7 : 4, i;

  write_to_output(d, "\r\n----------------------------------\r\nChoose %s\r\n\r\n1) Inferred\r\n   Let the NPC's role and identity choose this value.\r\n", title);
  for (i = 0; i < count; i++)
    write_to_output(d, "%d) %s\r\n", i + 2, choices[i]);
  write_to_output(d, "\r\nQ) Cancel\r\n----------------------------------\r\nChoice: ");
  OLC_VAL(d) = which;
  OLC_MODE(d) = MEDIT_AI_CAPABILITY_VALUE;
}

static void medit_disp_ai_capabilities(struct descriptor_data *d)
{
  struct mob_ai_config *c = OLC_MOB(d)->ai_config;
  struct ai_actor_profile *p;
  ai_actor_refresh_profile(OLC_MOB(d), TRUE);
  p = OLC_MOB(d)->ai_prof;
  const char *delay_status = c->movement == AI_MOVE_RANDOM ? "Active — Random movement is enabled." : "Inactive — current movement mode is not Random.";
  write_to_output(d,
      "\r\n                    AI Actor Capabilities\r\n\r\nIdentity\r\n  1) Archetype\r\n     Authored: %s\r\n     Effective: %s\r\n     Source: %s\r\n\r\nCommunication\r\n  2) Communication Type\r\n     Authored: %s\r\n     Effective: %s\r\n     Source: %s\r\n  B) Creature Vocalizations\r\n     Authored creature sounds used by Vocalize.\r\n\r\nMemory\r\n  3) Memory Style\r\n     Authored: %s\r\n     Effective: %s\r\n     Source: %s\r\n\r\nRelationships\r\n  4) Assistance\r\n     Authored: %s\r\n     Effective: %s\r\n     Source: %s\r\n\r\nMovement\r\n  5) Random Move Delay\r\n     %d seconds\r\n     Status: %s\r\n\r\nH) Help   Q) Return\r\nChoice: ",
      c->archetype < 0 ? "Inferred" : ai_actor_archetype_name(c->archetype), ai_actor_archetype_name(p->archetype), medit_ai_capability_source(c, 1),
      ai_actor_communication_name(c->communication), ai_actor_communication_name(p->communication), medit_ai_capability_source(c, 2),
      ai_actor_memory_style_name(c->memory_style), ai_actor_memory_style_name(p->memory_style), medit_ai_capability_source(c, 3),
      ai_actor_assistance_style_name(c->assistance_style), ai_actor_assistance_style_name(p->assistance_style), medit_ai_capability_source(c, 4), c->movement_delay, delay_status);
  OLC_MODE(d) = MEDIT_AI_CAPABILITIES;
}

static void medit_disp_ai_vocalizations(struct descriptor_data *d)
{
  struct mob_ai_config *c = OLC_MOB(d)->ai_config;
  int i;
  write_to_output(d, "\r\nCreature Vocalizations (%d/%d)\r\n", c->vocalization_count, AI_VOCALIZATION_MAX_LINES);
  for (i = 0; i < c->vocalization_count; i++)
    write_to_output(d, "  %d) %s\r\n", i + 1, c->vocalization[i]);
  write_to_output(d, "\r\nA) Add  E) Edit  D) Delete  U) Move up  N) Move down\r\nP) Preview  H) Help  Q) Return\r\nChoice: ");
  OLC_MODE(d) = MEDIT_AI_VOCALIZATIONS;
}

static void medit_disp_ai_preview(struct descriptor_data *d)
{
  struct mob_ai_config *c = OLC_MOB(d)->ai_config;
  int spoken = 0, i;
  for (i = 0; i < AI_DIALOGUE_CATEGORIES; i++) spoken += c->dialogue_count[i];
  write_to_output(d, "\r\nEffective NPC Behavior\r\n----------------------\r\n\r\nCommunication\r\n  %s\r\n\r\nIntelligence\r\n  %s\r\n\r\nMemory\r\n  %s\r\n\r\nAssistance\r\n  %s\r\n\r\nMovement\r\n  Uses normal MEDIT movement rules%s\r\n\r\nSchedule\r\n  %s\r\n\r\nDialogue\r\n  %d spoken lines\r\n  %d creature sounds\r\n\r\nCombat\r\n  %s\r\n\r\nRestrictions\r\n  %s%s\r\n\r\nWarnings\r\n  %s\r\n\r\nPress ENTER to return. ",
    medit_ai_communication_summary(c), medit_ai_intelligence_summary(c),
    c->memory_enabled ? "Remembers attackers and familiar threats" : "No AI memory is enabled",
    c->may_assist ? "Helps eligible allies" : "Defends itself without ally assistance",
    c->schedule_enabled ? "; schedule overrides movement during active routine entries" : "",
    c->schedule_enabled ? "Enabled" : "Disabled", spoken, c->vocalization_count,
    c->combat_enabled ? "Defends itself and uses configured reactions" : "Combat reactions are disabled",
    MOB_FLAGGED(OLC_MOB(d), MOB_SENTINEL) ? "SENTINEL active" : "No SENTINEL restriction", MOB_FLAGGED(OLC_MOB(d), MOB_STAY_ZONE) ? "; STAY_ZONE active" : "",
    ai_actor_compatibility_warning_count(OLC_MOB(d)) ? "See Diagnostics for actionable warnings" : "None");
  OLC_MODE(d) = MEDIT_AI_HELP;
  if (OLC_STORAGE(d)) free(OLC_STORAGE(d));
  CREATE(OLC_STORAGE(d), char, 24); snprintf(OLC_STORAGE(d), 24, "help %d", MEDIT_AI_MENU);
}

static void medit_disp_ai_menu(struct descriptor_data *d)
{
  struct mob_ai_config *c = OLC_MOB(d)->ai_config;
  int spoken = 0, i;
  if (!MOB_FLAGGED(OLC_MOB(d), MOB_AI_ACTOR)) { write_to_output(d, "Enable AI_ACTOR for this mob? (Y/N): "); OLC_MODE(d) = MEDIT_AI_ENABLE_CONFIRM; return; }
  if (!c) OLC_MOB(d)->ai_config = c = mob_ai_config_new();
  for (i = 0; i < AI_DIALOGUE_CATEGORIES; i++) spoken += c->dialogue_count[i];
  write_to_output(d, "\r\n                         AI Actor\r\n\r\n1) Communication : %s\r\n2) Intelligence  : %s\r\n3) Schedule      : %s\r\n4) Dialogue      : %s\r\n5) Preview       : Show effective NPC behavior\r\n6) Diagnostics   : %s\r\n\r\nA) Advanced AI Brain\r\nH) Help\r\nQ) Return\r\nChoice: ",
    medit_ai_communication_summary(c), medit_ai_intelligence_summary(c),
    c->schedule_enabled ? "Daily routine enabled" : (c->schedule_count || c->patrol_count ? "Configured, disabled" : "None"),
    spoken || c->vocalization_count ? "authored dialogue available" : "No authored lines",
    ai_actor_compatibility_warning_count(OLC_MOB(d)) ? "Warnings present" : "Ready");
  OLC_MODE(d) = MEDIT_AI_MENU;
}

/* AI modes accept textual commands.  Keep them out of Oasis's legacy numeric
 * pre-parser; individual modes below decide whether a value is numeric. */
static int medit_is_ai_mode(int mode)
{
  return mode >= MEDIT_AI_MENU && mode <= MEDIT_AI_DIAGNOSTICS;
}

static int medit_parse_ai_integer(const char *arg, int minimum, int maximum, int *value)
{
  char *end;
  long parsed;
  if (!arg || !*arg)
    return FALSE;
  parsed = strtol(arg, &end, 10);
  while (*end && isspace((unsigned char)*end))
    end++;
  if (end == arg || *end || parsed < minimum || parsed > maximum)
    return FALSE;
  *value = (int)parsed;
  return TRUE;
}

static int medit_parse_ai_boolean(const char *arg, int *value)
{
  if (!str_cmp(arg, "1") || !str_cmp(arg, "y") || !str_cmp(arg, "yes")) {
    *value = TRUE;
    return TRUE;
  }
  if (!str_cmp(arg, "toggle") || !str_cmp(arg, "t")) {
    *value = -1;
    return TRUE;
  }
  if (!str_cmp(arg, "0") || !str_cmp(arg, "n") || !str_cmp(arg, "no")) {
    *value = FALSE;
    return TRUE;
  }
  return FALSE;
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
             !medit_is_ai_mode(OLC_MODE(d)) &&
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
             OLC_MODE(d) != MEDIT_AI_ENABLE_CONFIRM &&
             OLC_MODE(d) != MEDIT_AI_MENU && OLC_MODE(d) != MEDIT_AI_SCHEDULE && (OLC_MODE(d) < MEDIT_AI_SCHEDULE_ROOM || OLC_MODE(d) > MEDIT_AI_PATROL_WAYPOINT_DELETE) && OLC_MODE(d) != MEDIT_AI_PERSONALITY && OLC_MODE(d) != MEDIT_AI_SOCIAL && OLC_MODE(d) != MEDIT_AI_DIALOGUE && OLC_MODE(d) != MEDIT_AI_DIALOGUE_ADD) {
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
  /* AI handlers accept commands as well as values.  Parse a complete number
   * opportunistically without rejecting text before the mode handles it. */
  if (medit_is_ai_mode(OLC_MODE(d)))
    (void)medit_parse_ai_integer(arg, INT_MIN, INT_MAX, &i);
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

  case MEDIT_AI_HELP: medit_return_from_ai_help(d); return;
  case MEDIT_AI_COMPATIBILITY:
    if (LOWER(*arg) == 'q') { medit_disp_ai_menu(d); return; }
    if (LOWER(*arg) == 'h') { char report[MAX_STRING_LENGTH]; ai_actor_compatibility_report(OLC_MOB(d), report, sizeof(report), TRUE); medit_disp_ai_help(d, MEDIT_AI_COMPATIBILITY, "Diagnostics", report, "This reference is read-only; it does not repair flags or routes.", "Legacy flags, profile modes, schedules, scripts, memory, and combat."); return; }
    medit_disp_ai_compatibility(d); return;
  case MEDIT_AI_DIAGNOSTICS:
    if (LOWER(*arg) == 'q') { medit_disp_ai_menu(d); return; }
    if (LOWER(*arg) == 'h') { medit_disp_ai_help(d, MEDIT_AI_DIAGNOSTICS, "Diagnostics", "Diagnostics lists only issues a builder can act on, such as inactive lines, schedules, and movement restrictions.", "Use Preview to understand effective behavior.", "Technical runtime detail is available only in Advanced AI Brain."); return; }
    if (LOWER(*arg) == 'p') { medit_disp_ai_preview(d); return; }
    if (LOWER(*arg) == 'd') { medit_disp_ai_compatibility(d); return; }
    medit_disp_ai_diagnostics(d); return;
  case MEDIT_AI_MENU:
    switch (LOWER(*arg)) {
      case 'q': medit_disp_menu(d); return;
      case 'h': medit_disp_ai_help(d, MEDIT_AI_MENU, "Builder Workflow", "1. Choose Communication. 2. Choose Intelligence. 3. Add a Schedule if needed. 4. Add Dialogue or Creature Sounds. 5. Preview behavior. 6. Resolve Diagnostics warnings. 7. Use Advanced AI Brain only for fine-tuning.", "Blank input redisplays this menu; Q always returns to MEDIT.", "Normal MEDIT owns flags and ordinary movement restrictions."); return;
      case '1': medit_disp_ai_communication(d); return;
      case '2': medit_disp_ai_intelligence(d); return;
      case '3': medit_disp_ai_schedule(d); return;
      case '4': medit_disp_ai_dialogue(d); return;
      case '5': medit_disp_ai_preview(d); return;
      case '6': medit_disp_ai_diagnostics(d); return;
      case 'a': medit_disp_ai_advanced(d); return;
      default: medit_disp_ai_menu(d); return;
    }
  case MEDIT_AI_COMMUNICATION:
    if (LOWER(*arg) == 'q') { medit_disp_ai_menu(d); return; }
    if (LOWER(*arg) == 'h') { medit_disp_ai_help(d, MEDIT_AI_COMMUNICATION, "Communication", "Silent disables authored speech and sounds. Creature Sounds uses vocalizations. Speaks enables normal dialogue.", "Choose one practical communication style.", "Telepathy remains an advanced partial capability."); return; }
    if (!medit_parse_ai_integer(arg, 1, 3, &i)) { write_to_output(d, "Choose Silent, Creature Sounds, or Speaks.\r\n"); medit_disp_ai_communication(d); return; }
    OLC_MOB(d)->ai_config->communication = i - 1; OLC_VAL(d) = 1; medit_disp_ai_menu(d); return;
  case MEDIT_AI_INTELLIGENCE: {
    struct mob_ai_config *c = OLC_MOB(d)->ai_config;
    if (LOWER(*arg) == 'q') { medit_disp_ai_menu(d); return; }
    if (LOWER(*arg) == 'h') { medit_disp_ai_help(d, MEDIT_AI_INTELLIGENCE, "Intelligence", "This applies documented defaults to existing perception, memory, assistance, and social fields. Advanced screens can fine-tune them afterwards.", "Selecting intelligence never changes dialogue, schedules, patrol routes, or combat authored data.", "Existing Advanced Configuration is derived until you choose a level."); return; }
    if (!medit_parse_ai_integer(arg, 1, 6, &i)) { write_to_output(d, "Choose an intelligence level from 1 to 6.\r\n"); medit_disp_ai_intelligence(d); return; }
    c->archetype = i == 1 ? AI_ARCH_MINDLESS : i == 2 ? AI_ARCH_BEAST : AI_ARCH_HUMANOID;
    c->memory_style = i == 1 ? AI_MEMORY_NONE : i == 2 ? AI_MEMORY_BASIC_HOSTILE : i < 5 ? AI_MEMORY_SOCIAL : AI_MEMORY_FULL_RELATIONSHIP;
    c->assistance_style = i < 2 ? AI_ASSIST_NONE : i == 2 ? AI_ASSIST_SAME_KIND : AI_ASSIST_FACTION;
    c->memory_enabled = i > 1; c->observation_sensitivity = (int[]){15,30,40,55,70,85}[i-1]; c->hearing_sensitivity = c->observation_sensitivity;
    c->recognition_confidence = c->observation_sensitivity; c->suspicion_threshold = 100 - c->observation_sensitivity; OLC_VAL(d)=1; medit_disp_ai_menu(d); return;
  }
  case MEDIT_AI_ADVANCED:
    switch (LOWER(*arg)) {
      case 'q': medit_disp_ai_menu(d); return; case 'h': medit_disp_ai_help(d,MEDIT_AI_ADVANCED,"Advanced AI Brain","Fine-tune engine-level AI systems here. These controls preserve the normal builder workflow.","Q returns to AI Actor; each detailed editor retains its existing values.","Profile modes, capabilities, movement internals, technical preview, validation, and diagnostics."); return;
      case '1': medit_disp_ai_personality(d); return; case '2': medit_disp_ai_perception(d); return; case '3': medit_disp_ai_memory(d); return; case '4': medit_disp_ai_threat(d); return; case '5': medit_disp_ai_combat(d); return; case '6': medit_disp_ai_social(d); return; case '7': medit_disp_ai_capabilities(d); return; case '8': medit_disp_ai_movement(d); return; case '9': medit_disp_ai_mode(d); return;
      case 'p': medit_disp_ai_preview(d); return; case 'v': medit_disp_ai_compatibility(d); return; case 'd': medit_disp_ai_compatibility(d); return;
      case 'r': mob_ai_config_free(OLC_MOB(d)->ai_config); OLC_MOB(d)->ai_config=mob_ai_config_new(); OLC_VAL(d)=1; medit_disp_ai_advanced(d); return;
      default: write_to_output(d,"Invalid advanced choice.\r\n"); medit_disp_ai_advanced(d); return;
    }
  case MEDIT_AI_PERSONALITY:
    if (LOWER(*arg)=='q') { medit_disp_ai_menu(d); return; }
    if (LOWER(*arg)=='h') { medit_disp_ai_help(d, MEDIT_AI_PERSONALITY, "Personality", "Traits shape the actor's social and threat tendencies.", "Each score ranges from 0 to 100; presets replace all trait values.", "Social, Threat, and Combat use personality values."); return; }
    if (LOWER(*arg)=='p') { OLC_MODE(d)=MEDIT_AI_PRESET; write_to_output(d, "\r\nPersonality Presets\r\n0) Balanced          : Even trait values.\r\n1) Noble Knight      : Brave, disciplined, loyal.\r\n2) Friendly Merchant : Sociable, honest, compassionate.\r\n3) Coward            : Low bravery; wary of danger.\r\n4) Fanatical Cultist : Proud, loyal, aggressive.\r\n5) Bandit            : Greedy and suspicious.\r\n6) Aggressive Beast  : Aggressive and territorial.\r\n7) Disciplined Soldier: Brave, patient, dutiful.\r\nChoose a preset (0-7, Q to cancel): "); return; }
    i=(LOWER(*arg)>='a'&&LOWER(*arg)<='c')?9+LOWER(*arg)-'a':atoi(arg)-1; if(i<0||i>=12){medit_disp_ai_personality(d);return;} OLC_MODE(d)=MEDIT_AI_TRAIT; OLC_VAL(d)=i; write_to_output(d,"%s\r\nCurrent value: %d\r\nEnter new value (0-100): ", ai_trait_names[i], OLC_MOB(d)->ai_config->personality[i]); return;
  case MEDIT_AI_TRAIT: if(i<0||i>100){write_to_output(d,"Value must be 0-100: ");return;} if(OLC_MOB(d)->ai_config->personality[OLC_VAL(d)]!=i){OLC_MOB(d)->ai_config->personality[OLC_VAL(d)]=i;OLC_MOB(d)->ai_config->override_mask|=AI_OVERRIDE_TRAITS;OLC_VAL(d)=1;} medit_disp_ai_personality(d);return;
  case MEDIT_AI_PRESET: if(i<0||i>7){medit_disp_ai_personality(d);return;} {static const int p[8][12]={{50,50,50,50,50,50,50,50,50,50,50,50},{25,70,30,20,90,75,10,55,85,65,45,40},{10,55,85,55,60,75,55,75,60,70,20,45},{15,20,30,65,35,55,20,65,40,25,75,20},{75,80,45,35,85,25,15,20,90,45,65,80},{35,55,55,30,75,45,95,20,75,55,60,70},{85,65,15,55,25,20,20,10,25,20,85,60},{45,75,45,25,95,70,20,55,90,80,45,50}};memcpy(OLC_MOB(d)->ai_config->personality,p[i],sizeof(p[i]));OLC_MOB(d)->ai_config->override_mask|=AI_OVERRIDE_TRAITS;OLC_VAL(d)=1;}medit_disp_ai_personality(d);return;
  case MEDIT_AI_CAPABILITIES:
    if (LOWER(*arg) == 'q') { medit_disp_ai_menu(d); return; }
    if (LOWER(*arg) == 'h') { medit_disp_ai_help(d, MEDIT_AI_CAPABILITIES, "Capabilities", "Choose clear behavior labels instead of engine enum values. Inferred values follow the NPC's role and identity.", "Select a category, then choose its named option. Q always leaves a picker without changing anything.", "Creature Vocalizations supplies the sounds used by Vocalize; Random Move Delay is active only in Random movement mode."); return; }
    if (!medit_parse_ai_integer(arg, 1, 5, &i)) { medit_disp_ai_capabilities(d); return; }
    if (i < 5) { medit_disp_ai_capability_picker(d, i); return; }
    OLC_VAL(d) = 5; OLC_MODE(d) = MEDIT_AI_CAPABILITY_VALUE;
    write_to_output(d, "Random Move Delay (1-60 seconds; Q to cancel): "); return;
  case MEDIT_AI_CAPABILITY_VALUE: {
    struct mob_ai_config *c = OLC_MOB(d)->ai_config;
    int which = OLC_VAL(d), count = which == 1 ? 7 : 4;
    if (LOWER(*arg) == 'q') { medit_disp_ai_capabilities(d); return; }
    if (!*arg) { which == 5 ? write_to_output(d, "Random Move Delay (1-60 seconds; Q to cancel): ") : medit_disp_ai_capability_picker(d, which); return; }
    if (which == 5) {
      if (!medit_parse_ai_integer(arg, 1, 60, &i)) { write_to_output(d, "Choose a delay from 1 to 60 seconds, or Q to cancel: "); return; }
      c->movement_delay=i;
    } else {
      if (!medit_parse_ai_integer(arg, 1, count + 1, &i)) { medit_disp_ai_capability_picker(d, which); return; }
      i = i == 1 ? -1 : i - 2;
      if (which == 1) c->archetype=i; else if (which == 2) c->communication=i; else if (which == 3) c->memory_style=i; else c->assistance_style=i;
    }
    OLC_VAL(d)=1; medit_disp_ai_capabilities(d); return;
  }
  case MEDIT_AI_VOCALIZATIONS: {
    struct mob_ai_config *c=OLC_MOB(d)->ai_config; char cmd=LOWER(*arg); int line=atoi(arg+1)-1;
    if(cmd=='q'){medit_disp_ai_menu(d);return;} if(cmd=='h'){medit_disp_ai_help(d,MEDIT_AI_VOCALIZATIONS,"Creature Vocalizations","Vocalizations are full-room act strings and never use normal say dialogue.","Use A/E/D/U/N plus a line number, for example E 2.","Communication=Vocalize delivers these lines; preview is read-only.");return;} if(cmd=='p'){write_to_output(d,"\r\nVocalization Preview\r\nCommunication effective: %s\r\n",ai_actor_communication_name(c->communication));for(i=0;i<c->vocalization_count;i++)write_to_output(d,"  %s\r\n",c->vocalization[i]);medit_disp_ai_vocalizations(d);return;} if(cmd=='a'){if(c->vocalization_count>=AI_VOCALIZATION_MAX_LINES){write_to_output(d,"Maximum reached.\r\n");medit_disp_ai_vocalizations(d);return;}OLC_MODE(d)=MEDIT_AI_VOCALIZATION_ADD;write_to_output(d,"Vocalization line: ");return;} if((cmd=='e'||cmd=='d'||cmd=='u'||cmd=='n')&&(line<0||line>=c->vocalization_count)){write_to_output(d,"Use %c <line number>.\r\n",cmd);medit_disp_ai_vocalizations(d);return;} if(cmd=='e'){OLC_VAL(d)=line;OLC_MODE(d)=MEDIT_AI_VOCALIZATION_EDIT;write_to_output(d,"Replacement vocalization: ");return;} if(cmd=='d'){mob_ai_vocalization_delete(c,line);OLC_VAL(d)=1;medit_disp_ai_vocalizations(d);return;} if(cmd=='u'||cmd=='n'){if(!mob_ai_vocalization_move(c,line,cmd=='u'?line-1:line+1))write_to_output(d,"That line cannot move further.\r\n");else OLC_VAL(d)=1;medit_disp_ai_vocalizations(d);return;}medit_disp_ai_vocalizations(d);return;
  }
  case MEDIT_AI_VOCALIZATION_ADD: if(!*arg||strlen(arg)>=AI_VOCALIZATION_LINE_MAX||!mob_ai_vocalization_set(OLC_MOB(d)->ai_config,OLC_MOB(d)->ai_config->vocalization_count,arg))write_to_output(d,"Invalid or too-long vocalization.\r\n");else OLC_VAL(d)=1;medit_disp_ai_vocalizations(d);return;
  case MEDIT_AI_VOCALIZATION_EDIT: if(!*arg||strlen(arg)>=AI_VOCALIZATION_LINE_MAX||!mob_ai_vocalization_set(OLC_MOB(d)->ai_config,OLC_VAL(d),arg))write_to_output(d,"Invalid or too-long vocalization.\r\n");else OLC_VAL(d)=1;medit_disp_ai_vocalizations(d);return;
  case MEDIT_AI_SOCIAL: if(LOWER(*arg)=='q'){medit_disp_ai_menu(d);return;} if(LOWER(*arg)=='h'){medit_disp_ai_help(d, MEDIT_AI_SOCIAL, "Social Behavior", "Social settings control greetings, replies, and ambient behavior.", "Cooldowns are in seconds; toggles enable individual responses.", "Dialogue provides authored lines for these events.");return;} i=(LOWER(*arg)>='a'&&LOWER(*arg)<='c')?10+LOWER(*arg)-'a':atoi(arg); if(i<1||i>12){medit_disp_ai_social(d);return;} OLC_MODE(d)=MEDIT_AI_SOCIAL_VALUE;OLC_VAL(d)=i;write_to_output(d,"Current value: %d\r\nEnter new value: ", *(&OLC_MOB(d)->ai_config->social+i-1));return;
  case MEDIT_AI_SOCIAL_VALUE: {struct mob_ai_config*c=OLC_MOB(d)->ai_config;int *v; switch(OLC_VAL(d)){case 1:v=&c->social;break;case 2:v=&c->greeting_enabled;break;case 3:v=&c->ambient_speech_enabled;break;case 4:v=&c->ambient_emotes_enabled;break;case 5:v=&c->whisper_enabled;break;case 6:v=&c->respond_strangers;break;case 7:v=&c->respond_trusted;break;case 8:v=&c->respond_feared;break;case 9:v=&c->respond_hostile;break;case 10:v=&c->speech_cooldown;break;case 11:v=&c->room_speech_cooldown;break;default:v=&c->emote_cooldown;}if(i<0||(OLC_VAL(d)==1&&i>10)||(OLC_VAL(d)>1&&OLC_VAL(d)<10&&i>1)||(OLC_VAL(d)>=10&&(i<1||i>300))){write_to_output(d,"Invalid value: ");return;}if(*v!=i){*v=i;OLC_VAL(d)=1;}medit_disp_ai_social(d);return;}
  case MEDIT_AI_DIALOGUE:
    if (LOWER(*arg)=='q') { medit_disp_ai_menu(d); return; }
    if (OLC_VAL(d) >= 0 && OLC_VAL(d) < AI_DIALOGUE_CATEGORIES && !isdigit((unsigned char)*arg)) {
      int category = OLC_VAL(d), line;
      char cmd = LOWER(*arg);
      if (cmd == 'h') { write_to_output(d,"%s\r\n",ai_dialogue_summaries[category]); medit_disp_ai_dialogue_lines(d,category); return; }
      if (cmd == 'a') { if (OLC_MOB(d)->ai_config->dialogue_count[category] >= AI_DIALOGUE_MAX_LINES) { write_to_output(d,"This category already contains the maximum of %d lines.\r\n",AI_DIALOGUE_MAX_LINES); medit_disp_ai_dialogue_lines(d,category); return; } OLC_MODE(d)=MEDIT_AI_DIALOGUE_ADD; write_to_output(d,"Line to add (maximum %d characters): ",AI_DIALOGUE_LINE_MAX-1); return; }
      line = atoi(arg + 1) - 1;
      if (cmd=='e' || cmd=='d' || cmd=='u' || cmd=='n') {
        if (!OLC_MOB(d)->ai_config->dialogue_count[category]) { write_to_output(d,"There are no lines to %s.\r\n",cmd=='e'?"edit":cmd=='d'?"delete":"move"); medit_disp_ai_dialogue_lines(d,category); return; }
        if (!*(arg + 1)) { OLC_VAL(d)=category*10+(cmd=='e'?1:cmd=='d'?2:cmd=='u'?3:4); OLC_MODE(d)=MEDIT_AI_DIALOGUE_INDEX; write_to_output(d,"Line number to %s (1-%d, or Q to cancel): ",cmd=='e'?"edit":cmd=='d'?"delete":cmd=='u'?"move up":"move down",OLC_MOB(d)->ai_config->dialogue_count[category]); return; }
        if (line < 0 || line >= OLC_MOB(d)->ai_config->dialogue_count[category]) { write_to_output(d,"Entry %d does not exist. Valid lines: 1-%d.\r\n",line+1,OLC_MOB(d)->ai_config->dialogue_count[category]); medit_disp_ai_dialogue_lines(d,category); return; }
        if (cmd=='e') { OLC_VAL(d)=category*AI_DIALOGUE_MAX_LINES+line; OLC_MODE(d)=MEDIT_AI_DIALOGUE_EDIT; write_to_output(d,"Replacement line: "); return; }
        if (cmd=='d') mob_ai_dialogue_delete(OLC_MOB(d)->ai_config,category,line);
        else if (cmd=='u' && line == 0) { write_to_output(d,"Dialogue line 1 cannot move up because it is already first.\r\n"); medit_disp_ai_dialogue_lines(d,category); return; }
        else if (cmd=='n' && line + 1 >= OLC_MOB(d)->ai_config->dialogue_count[category]) { write_to_output(d,"Dialogue line %d cannot move down because it is already last.\r\n",line+1); medit_disp_ai_dialogue_lines(d,category); return; }
        else mob_ai_dialogue_move(OLC_MOB(d)->ai_config,category,line,cmd=='u'?line-1:line+1);
        OLC_VAL(d)=1; write_to_output(d,"Dialogue line %d %s.\r\n",line+1,cmd=='d'?"deleted":cmd=='u'?"moved up":cmd=='n'?"moved down":"updated"); medit_disp_ai_dialogue_lines(d,category); return;
      }
      write_to_output(d,"Invalid command. Enter H for help.\r\n"); medit_disp_ai_dialogue_lines(d,category); return;
    }
    if (LOWER(*arg)=='h') { medit_disp_ai_help(d, MEDIT_AI_DIALOGUE, "Dialogue", "Add greetings, ambient speech, replies, warnings, combat lines, farewells, and creature sounds here.", "Choose an area, then add, edit, delete, or reorder lines.", "Communication determines which authored lines can run."); return; }
    if (LOWER(*arg)=='7') { medit_disp_ai_vocalizations(d); return; }
    if (LOWER(*arg)=='8') { write_to_output(d,"\r\nDialogue Preview\r\nSpoken lines and creature sounds are listed in their editors.\r\n"); medit_disp_ai_dialogue(d); return; }
    i=atoi(arg); if(i>=1&&i<=6) { static const int categories[]={AI_DIALOGUE_GREETING,AI_DIALOGUE_AMBIENT_SPEECH,AI_DIALOGUE_FRIENDLY,AI_DIALOGUE_WARNING,AI_DIALOGUE_CALL_HELP,AI_DIALOGUE_FAREWELL}; medit_disp_ai_dialogue_lines(d,categories[i-1]); return; }
    if (!medit_parse_ai_integer(arg,0,AI_DIALOGUE_CATEGORIES-1,&i)) { write_to_output(d,"Invalid category. Enter H for help.\r\n"); medit_disp_ai_dialogue(d); return; } OLC_VAL(d)=i;medit_disp_ai_dialogue_lines(d,i);return;
  case MEDIT_AI_DIALOGUE_INDEX: { int category=OLC_VAL(d)/10, operation=OLC_VAL(d)%10, line; if (LOWER(*arg)=='q') { OLC_VAL(d)=category; medit_disp_ai_dialogue_lines(d,category); return; } if (!medit_parse_ai_integer(arg,1,OLC_MOB(d)->ai_config->dialogue_count[category],&line)) { write_to_output(d,"Please enter a line number from 1 to %d, or Q to cancel: ",OLC_MOB(d)->ai_config->dialogue_count[category]); return; } line--; if(operation==1){OLC_VAL(d)=category*AI_DIALOGUE_MAX_LINES+line;OLC_MODE(d)=MEDIT_AI_DIALOGUE_EDIT;write_to_output(d,"Replacement line: ");return;} if(operation==2){mob_ai_dialogue_delete(OLC_MOB(d)->ai_config,category,line);write_to_output(d,"Dialogue line %d deleted.\r\n",line+1);} else if(operation==3&&line==0){write_to_output(d,"Dialogue line 1 cannot move up because it is already first.\r\n");} else if(operation==4&&line+1>=OLC_MOB(d)->ai_config->dialogue_count[category]){write_to_output(d,"Dialogue line %d cannot move down because it is already last.\r\n",line+1);} else {mob_ai_dialogue_move(OLC_MOB(d)->ai_config,category,line,operation==3?line-1:line+1);write_to_output(d,"Dialogue line %d moved %s.\r\n",line+1,operation==3?"up":"down");} OLC_VAL(d)=1;medit_disp_ai_dialogue_lines(d,category);return; }

  case MEDIT_AI_DIALOGUE_ADD: { int category=OLC_VAL(d), count=OLC_MOB(d)->ai_config->dialogue_count[category]; size_t len=strlen(arg); if (!*arg) write_to_output(d,"Dialogue line cannot be empty.\r\n"); else if (len >= AI_DIALOGUE_LINE_MAX) write_to_output(d,"Dialogue line is too long. Maximum: %d characters.\r\n",AI_DIALOGUE_LINE_MAX-1); else if (count >= AI_DIALOGUE_MAX_LINES) write_to_output(d,"This category already contains the maximum of %d lines.\r\n",AI_DIALOGUE_MAX_LINES); else if (!mob_ai_dialogue_set(OLC_MOB(d)->ai_config,category,count,arg)) write_to_output(d,"Unable to allocate memory for the dialogue line.\r\n"); else { OLC_VAL(d)=1; write_to_output(d,"Dialogue line added as entry %d.\r\n",count+1); } medit_disp_ai_dialogue_lines(d,category);return; }
  case MEDIT_AI_DIALOGUE_EDIT: { int category=OLC_VAL(d)/AI_DIALOGUE_MAX_LINES, line=OLC_VAL(d)%AI_DIALOGUE_MAX_LINES; if(!*arg) write_to_output(d,"Dialogue line cannot be empty.\r\n"); else if(strlen(arg)>=AI_DIALOGUE_LINE_MAX) write_to_output(d,"Dialogue line is too long. Maximum: %d characters.\r\n",AI_DIALOGUE_LINE_MAX-1); else if(mob_ai_dialogue_set(OLC_MOB(d)->ai_config,category,line,arg)){OLC_VAL(d)=1;write_to_output(d,"Dialogue line %d updated.\r\n",line+1);} else write_to_output(d,"Dialogue line was not saved.\r\n");medit_disp_ai_dialogue_lines(d,category);return; }
  case MEDIT_AI_PERCEPTION:
    if (LOWER(*arg)=='q') { medit_disp_ai_menu(d); return; }
    if (LOWER(*arg)=='h') { medit_disp_ai_help(d, MEDIT_AI_PERCEPTION, "Perception", "Event switches control which nearby events this actor notices.", "Numeric sensitivities accept values from 0 to 100.", "Memory and Threat use noticed events."); return; }
    i=(LOWER(*arg)>='a'&&LOWER(*arg)<='g')?10+LOWER(*arg)-'a':atoi(arg);
    if(i<1||i>16){medit_disp_ai_perception(d);return;}
    OLC_VAL(d)=i; OLC_MODE(d)=MEDIT_AI_PERCEPTION_VALUE;
    if (i <= 12) write_to_output(d,"Current: %s\r\nY) Enable  N) Disable  T) Toggle  Q) Cancel  H) Help\r\nChoice: ", *(&OLC_MOB(d)->ai_config->notice_entry+i-1) ? "Yes" : "No");
    else write_to_output(d,"Current value: %d\r\nValid range: 0-100\r\nEnter a new value, Q to cancel, or H for help: ", *(&OLC_MOB(d)->ai_config->notice_entry+i-1));
    return;
  case MEDIT_AI_PERCEPTION_VALUE: { struct mob_ai_config*c=OLC_MOB(d)->ai_config; int *v=&c->notice_entry+OLC_VAL(d)-1;
    if (LOWER(*arg)=='q') { medit_disp_ai_perception(d); return; }
    if (LOWER(*arg)=='h') { write_to_output(d, "This value is validated before it is saved; invalid input leaves the previous value unchanged.\r\n"); return; }
    if (OLC_VAL(d)<=12) { if (!medit_parse_ai_boolean(arg,&i)) { write_to_output(d,"Use Y) Enable, N) Disable, T) Toggle, H) Help, or Q) Cancel: "); return; } if (i == -1) i=!*v; }
    else if (!medit_parse_ai_integer(arg,0,100,&i)) { write_to_output(d,"Enter a whole number from 0 to 100: "); return; }
    if(*v!=i){*v=i;OLC_VAL(d)=1;} medit_disp_ai_perception(d);return; }
  case MEDIT_AI_MEMORY: if(LOWER(*arg)=='q'){medit_disp_ai_menu(d);return;} if(LOWER(*arg)=='h'){medit_disp_ai_help(d, MEDIT_AI_MEMORY, "Memory", "Memory stores relationships and familiarity for live NPC instances.", "Durations are seconds; use toggles for remembered event types.", "Perception supplies memory events; Threat reads them.");return;} if(!medit_parse_ai_integer(arg,1,22,&i)){medit_disp_ai_memory(d);return;} OLC_VAL(d)=i;OLC_MODE(d)=MEDIT_AI_MEMORY_VALUE;write_to_output(d,"Memory field %d\r\nCurrent value: %d\r\nUse Y/N/T for a toggle, or enter the documented whole-number range. H) Help  Q) Cancel\r\nChoice: ",i,(i<=13?(&OLC_MOB(d)->ai_config->memory_enabled)[i-1]:(&OLC_MOB(d)->ai_config->remember_attacks)[i-14]));return;
  case MEDIT_AI_MEMORY_VALUE: { struct mob_ai_config*c=OLC_MOB(d)->ai_config; int *v; int maximum; if(LOWER(*arg)=='q'){medit_disp_ai_memory(d);return;} if(LOWER(*arg)=='h'){write_to_output(d,"Memory numeric fields use seconds, actor capacity, or score units as described in the menu; invalid input preserves the current value.\r\n");return;} if(OLC_VAL(d)<=13) v=&c->memory_enabled+OLC_VAL(d)-1; else v=&c->remember_attacks+OLC_VAL(d)-14; if(OLC_VAL(d)==1||OLC_VAL(d)>=14){if(!medit_parse_ai_boolean(arg,&i)){write_to_output(d,"Use Y) Enable, N) Disable, T) Toggle, H) Help, or Q) Cancel: ");return;}if(i==-1)i=!*v;}else {maximum=OLC_VAL(d)==2?AI_MEM_MAX:((OLC_VAL(d)>=5&&OLC_VAL(d)<=7)||OLC_VAL(d)==9||OLC_VAL(d)==11?200:10080);if(!medit_parse_ai_integer(arg,0,maximum,&i)){write_to_output(d,"Enter a whole number in the documented range: ");return;}} if(*v!=i){*v=i;OLC_VAL(d)=1;}medit_disp_ai_memory(d);return; }
  case MEDIT_AI_COMBAT: { struct mob_ai_config*c=OLC_MOB(d)->ai_config; char report[MAX_STRING_LENGTH]; if(LOWER(*arg)=='h'){medit_disp_ai_help(d, MEDIT_AI_COMBAT, "Combat", "Combat options select eligibility, reactions, and target scoring.", "Use Preview and Validate to inspect combat settings.", "Threat response can call combat actions.");return;} if(LOWER(*arg)=='q'){medit_disp_ai_menu(d);return;} if(LOWER(*arg)=='j'){medit_disp_ai_targets(d);return;} if(LOWER(*arg)=='k'){ai_actor_combat_preview(c,report,sizeof(report));write_to_output(d,"\r\n%s\r\n",report);medit_disp_ai_combat(d);return;} if(LOWER(*arg)=='l'){ai_actor_combat_validate(c,report,sizeof(report));write_to_output(d,"\r\n%s\r\n",report);medit_disp_ai_combat(d);return;} if(!medit_parse_ai_integer(arg,1,18,&i) && !(LOWER(*arg)>='a'&&LOWER(*arg)<='j')){medit_disp_ai_combat(d);return;} if(LOWER(*arg)>='a'&&LOWER(*arg)<='j')i=10+LOWER(*arg)-'a'; if(i>=2&&i<=14){int *v=&c->combat_enabled+i-2;OLC_VAL(d)=i;OLC_MODE(d)=MEDIT_AI_COMBAT_VALUE;write_to_output(d,"Current value: %s\r\nY) Enable N) Disable T) Toggle H) Help Q) Cancel\r\nChoice: ",*v?"Enabled":"Disabled");return;} if(i==1||(i>=15&&i<=18)){OLC_VAL(d)=i;OLC_MODE(d)=MEDIT_AI_COMBAT_VALUE;write_to_output(d,"Combat value %d: enter a whole number, H for help, or Q to cancel: ",i);return;}medit_disp_ai_combat(d);return;}
  case MEDIT_AI_COMBAT_VALUE: {struct mob_ai_config*c=OLC_MOB(d)->ai_config;int *v=OLC_VAL(d)==1?&c->combat_style:OLC_VAL(d)==15?&c->flee_hp_percent:OLC_VAL(d)==16?&c->assist_severity:OLC_VAL(d)==17?&c->target_switch_threshold:OLC_VAL(d)==18?&c->combat_cooldown:&c->combat_enabled+OLC_VAL(d)-2;int maximum=OLC_VAL(d)==1?AI_COMBAT_STYLE_MAX-1:OLC_VAL(d)==18?300:100;if(LOWER(*arg)=='q'){medit_disp_ai_combat(d);return;}if(LOWER(*arg)=='h'){write_to_output(d,"This editor rejects partial or out-of-range values and keeps the old value on error.\r\n");return;}if(OLC_VAL(d)>=2&&OLC_VAL(d)<=14){if(!medit_parse_ai_boolean(arg,&i)){write_to_output(d,"Use Y/N/T, H, or Q: ");return;}if(i==-1)i=!*v;}else if(!medit_parse_ai_integer(arg,0,maximum,&i)){write_to_output(d,"Enter a whole number in range: ");return;}if(*v!=i){*v=i;OLC_VAL(d)=1;}medit_disp_ai_combat(d);return;}
  case MEDIT_AI_TARGETS: if(LOWER(*arg)=='q'){medit_disp_ai_combat(d);return;}i=atoi(arg);if(i<1||i>AI_TARGET_WEIGHTS){medit_disp_ai_targets(d);return;}OLC_VAL(d)=i-1;OLC_MODE(d)=MEDIT_AI_TARGET_VALUE;write_to_output(d,"Weight (-100..100): ");return;
  case MEDIT_AI_TARGET_VALUE: if(i < -100 || i > 100){write_to_output(d,"Weight (-100..100): ");return;}OLC_MOB(d)->ai_config->target_weight[OLC_VAL(d)]=i;OLC_VAL(d)=1;medit_disp_ai_targets(d);return;
  case MEDIT_AI_THREAT:
    if(LOWER(*arg)=='q'){medit_disp_ai_menu(d);return;} if(LOWER(*arg)=='h'){medit_disp_ai_help(d, MEDIT_AI_THREAT, "Threat Response", "Threat response determines the escalation steps available to this actor.", "Cooldowns and windows are measured in seconds.", "Perception and Memory provide threat context.");return;} if(LOWER(*arg)=='e'){medit_disp_ai_threat_sequence(d);return;} if(LOWER(*arg)=='r'){mob_ai_config_free(OLC_MOB(d)->ai_config);OLC_MOB(d)->ai_config=mob_ai_config_new();OLC_VAL(d)=1;medit_disp_ai_threat(d);return;} i=(LOWER(*arg)>='a'&&LOWER(*arg)<='d')?10+LOWER(*arg)-'a':atoi(arg); if(i>=1&&i<=5){if(i==5){medit_disp_ai_threat(d);return;} OLC_MOB(d)->ai_config->threat_enabled[i-1]=!OLC_MOB(d)->ai_config->threat_enabled[i-1];OLC_VAL(d)=1;medit_disp_ai_threat(d);return;}if(i==8||i==9){OLC_MOB(d)->ai_config->threat_enabled[i-1]=!OLC_MOB(d)->ai_config->threat_enabled[i-1];OLC_VAL(d)=1;medit_disp_ai_threat(d);return;}if(i>=11&&i<=13){OLC_VAL(d)=i;OLC_MODE(d)=MEDIT_AI_THREAT_VALUE;write_to_output(d,"Value: ");return;}medit_disp_ai_threat(d);return;
  case MEDIT_AI_THREAT_VALUE: if(i<1||i>(OLC_VAL(d)==12?3600:600)){write_to_output(d,"Invalid value: ");return;} if(OLC_VAL(d)==11)OLC_MOB(d)->ai_config->threat_cooldown=i;else if(OLC_VAL(d)==12)OLC_MOB(d)->ai_config->calm_reset_time=i;else OLC_MOB(d)->ai_config->repeated_event_window=i;OLC_VAL(d)=1;medit_disp_ai_threat(d);return;
  case MEDIT_AI_THREAT_SEQUENCE: { struct mob_ai_config*c=OLC_MOB(d)->ai_config; int a,b,cc,e,f,n=atoi(arg)-1; if(LOWER(*arg)=='q'){medit_disp_ai_threat(d);return;} if(sscanf(arg,"a %d %d %d %d %d",&a,&b,&cc,&e,&f)==5&&c->threat_step_count<AI_THREAT_STEP_MAX){struct ai_threat_step z={a,AI_MEDIT_CLAMP(b,0,100),AI_MEDIT_CLAMP(cc,0,300),AI_MEDIT_CLAMP(e,1,10),!!f};if(ai_threat_step_valid(&z,c->threat_enabled)){c->threat_steps[c->threat_step_count++]=z;OLC_VAL(d)=1;}medit_disp_ai_threat_sequence(d);return;}if(sscanf(arg,"e %d %d %d %d %d %d",&n,&a,&b,&cc,&e,&f)==6){n--;if(n>=0&&n<c->threat_step_count){struct ai_threat_step z={a,AI_MEDIT_CLAMP(b,0,100),AI_MEDIT_CLAMP(cc,0,300),AI_MEDIT_CLAMP(e,1,10),!!f};if(ai_threat_step_valid(&z,c->threat_enabled)){c->threat_steps[n]=z;OLC_VAL(d)=1;}}medit_disp_ai_threat_sequence(d);return;}if(LOWER(*arg)=='d'&&n>=0&&n<c->threat_step_count){memmove(&c->threat_steps[n],&c->threat_steps[n+1],sizeof(c->threat_steps[0])*(c->threat_step_count-n-1));c->threat_step_count--;OLC_VAL(d)=1;}else if(LOWER(*arg)=='u'&&n>0&&n<c->threat_step_count){struct ai_threat_step z=c->threat_steps[n];c->threat_steps[n]=c->threat_steps[n-1];c->threat_steps[n-1]=z;OLC_VAL(d)=1;}else if(LOWER(*arg)=='n'&&n>=0&&n+1<c->threat_step_count){struct ai_threat_step z=c->threat_steps[n];c->threat_steps[n]=c->threat_steps[n+1];c->threat_steps[n+1]=z;OLC_VAL(d)=1;}medit_disp_ai_threat_sequence(d);return; }
  case MEDIT_AI_SCHEDULE: { struct mob_ai_config*c=OLC_MOB(d)->ai_config; char cmd=LOWER(*arg), report[MAX_STRING_LENGTH]; if(cmd=='q'){medit_disp_ai_menu(d);return;} if(cmd=='h'){medit_disp_ai_help(d, MEDIT_AI_SCHEDULE, "Schedule", "Schedules describe routines; normal MEDIT flags remain the source of ordinary movement restrictions.", "Preview and validation do not modify the NPC. Preview: Show the day's effective activity timeline.", "Daily schedule and patrol route editors retain detailed activity settings.");return;} if(cmd=='1'){c->schedule_enabled=!c->schedule_enabled;OLC_VAL(d)=1;} else if(cmd=='2'){medit_disp_ai_schedule_entries(d);return;} else if(cmd=='3'){medit_disp_ai_patrol_routes(d);return;} else if(cmd=='4'){ai_actor_schedule_preview(c,((35*time_info.month)+(time_info.day+1))%7,time_info.hours,report,sizeof(report));write_to_output(d,"%s",report);} else if(cmd=='5'){ai_actor_schedule_validate(c,report,sizeof(report));write_to_output(d,"%s",report);} else write_to_output(d,"Invalid schedule choice.\r\n");medit_disp_ai_schedule(d);return; }
  case MEDIT_AI_SCHEDULE_ROOM: { int *v,candidate=0; if(!str_cmp(arg,"none")||!str_cmp(arg,"0"))candidate=0;else if(sscanf(arg,"%d",&candidate)!=1||candidate<=0||real_room(candidate)==NOWHERE){write_to_output(d,"Invalid room VNUM.\r\n");medit_disp_ai_schedule(d);return;}v=OLC_VAL(d)==0?&OLC_MOB(d)->ai_config->home_room_vnum:OLC_VAL(d)==1?&OLC_MOB(d)->ai_config->work_room_vnum:OLC_VAL(d)==2?&OLC_MOB(d)->ai_config->sleep_room_vnum:OLC_VAL(d)==3?&OLC_MOB(d)->ai_config->guard_room_vnum:&OLC_MOB(d)->ai_config->fallback_room_vnum;if(*v!=candidate){*v=candidate;OLC_VAL(d)=1;}medit_disp_ai_schedule(d);return; }
  case MEDIT_AI_SCHEDULE_FAILURE: { struct ai_schedule_entry *e=medit_selected_entry(d); i=atoi(arg);if(i<0||i>=AI_FAILURE_MAX)write_to_output(d,"Invalid policy.\r\n");else if(e){if(e->failure_policy!=i){e->failure_policy=i;OLC_VAL(d)=1;}medit_disp_ai_schedule_entry(d);return;}else if(OLC_MOB(d)->ai_config->default_failure_policy!=i){OLC_MOB(d)->ai_config->default_failure_policy=i;OLC_VAL(d)=1;}medit_disp_ai_schedule(d);return; }
  case MEDIT_AI_SCHEDULE_ENTRIES: {struct mob_ai_config*c=OLC_MOB(d)->ai_config;char cmd=LOWER(*arg);int n;if(cmd=='q'){medit_disp_ai_schedule(d);return;}if(cmd=='h'){write_to_output(d,"Entries use game time; All Day is 00:00-00:00. Priority selects among overlapping entries.\r\n");medit_disp_ai_schedule_entries(d);return;}if(cmd=='a'){struct ai_schedule_entry z;memset(&z,0,sizeof(z));z.enabled=TRUE;z.day_mask=AI_DAY_MASK_ALL;z.activity=AI_SCHEDULE_REMAIN;z.destination=AI_DEST_CURRENT_ROOM;z.max_attempts=3;if(ai_schedule_add(c,&z)){OLC_VAL(d)=1;write_to_output(d,"Schedule entry added as entry %d.\r\n",c->schedule_count);}else write_to_output(d,"Maximum entry count reached.\r\n");medit_disp_ai_schedule_entries(d);return;}if(cmd!='e'&&cmd!='d'&&cmd!='u'&&cmd!='n'&&cmd!='c'&&cmd!='t'){write_to_output(d,"Invalid command. Enter H for help.\r\n");medit_disp_ai_schedule_entries(d);return;}if(!c->schedule_count){write_to_output(d,"There are no schedule entries to %s.\r\n",cmd=='e'?"edit":cmd=='d'?"delete":"change");medit_disp_ai_schedule_entries(d);return;}if(!*(arg+1)){OLC_VAL(d)=cmd;OLC_MODE(d)=MEDIT_AI_SCHEDULE_INDEX;write_to_output(d,"Entry number to %s (1-%d, or Q to cancel): ",cmd=='e'?"edit":cmd=='d'?"delete":cmd=='u'?"move up":cmd=='n'?"move down":cmd=='c'?"duplicate":"toggle",c->schedule_count);return;}n=atoi(arg+1)-1;if(n<0||n>=c->schedule_count){write_to_output(d,"Entry %d does not exist. Valid entries: 1-%d.\r\n",n+1,c->schedule_count);medit_disp_ai_schedule_entries(d);return;}if(cmd=='e'){medit_schedule_store(d,-1,c->schedules[n].id,-1);medit_disp_ai_schedule_entry(d);return;}if(cmd=='d'){medit_schedule_store(d,-1,c->schedules[n].id,-1);OLC_MODE(d)=MEDIT_AI_SCHEDULE_DELETE;write_to_output(d,"Delete entry? (Y/N): ");return;}if(cmd=='u'&&n==0)write_to_output(d,"Entry 1 cannot move up because it is already first.\r\n");else if(cmd=='n'&&n+1==c->schedule_count)write_to_output(d,"Entry %d cannot move down because it is already last.\r\n",n+1);else if(cmd=='u'||cmd=='n'){ai_schedule_move(c,n,cmd=='u'?n-1:n+1);OLC_VAL(d)=1;write_to_output(d,"Schedule entry %d moved %s.\r\n",n+1,cmd=='u'?"up":"down");}else if(cmd=='c'){if(ai_schedule_duplicate(c,n)){OLC_VAL(d)=1;write_to_output(d,"Schedule entry duplicated.\r\n");}else write_to_output(d,"Maximum entry count reached.\r\n");}else{c->schedules[n].enabled=!c->schedules[n].enabled;OLC_VAL(d)=1;write_to_output(d,"Schedule entry %d %s.\r\n",n+1,c->schedules[n].enabled?"enabled":"disabled");}medit_disp_ai_schedule_entries(d);return;}
  case MEDIT_AI_SCHEDULE_INDEX: {char command=OLC_VAL(d);int n;if(LOWER(*arg)=='q'){medit_disp_ai_schedule_entries(d);return;}if(!medit_parse_ai_integer(arg,1,OLC_MOB(d)->ai_config->schedule_count,&n)){write_to_output(d,"Please enter an entry number from 1 to %d, or Q to cancel: ",OLC_MOB(d)->ai_config->schedule_count);return;}OLC_MODE(d)=MEDIT_AI_SCHEDULE_ENTRIES;snprintf(arg,MAX_INPUT_LENGTH,"%c %d",command,n);medit_parse(d,arg);return;}

  case MEDIT_AI_SCHEDULE_DELETE: {struct ai_schedule_entry*e=medit_selected_entry(d);if(LOWER(*arg)=='y'&&e){int k;for(k=0;k<OLC_MOB(d)->ai_config->schedule_count;k++)if(&OLC_MOB(d)->ai_config->schedules[k]==e&&ai_schedule_delete(OLC_MOB(d)->ai_config,k)){OLC_VAL(d)=1;break;}}medit_disp_ai_schedule_entries(d);return;}
  case MEDIT_AI_SCHEDULE_ENTRY: {struct ai_schedule_entry*e=medit_selected_entry(d);char cmd=LOWER(*arg),report[MAX_STRING_LENGTH];if(!e){medit_disp_ai_schedule_entries(d);return;}if(cmd=='q'){medit_disp_ai_schedule_entries(d);return;}if(cmd=='1'){e->enabled=!e->enabled;OLC_VAL(d)=1;}else if(cmd=='4'){write_to_output(d,"\r\nDay Mask: %s\r\nA) Every day  W) Weekdays  E) Weekends  1-7) Toggle day  C) Clear (not permitted)  Q) Return\r\nChoice: ",medit_sched_days(e->day_mask));OLC_MODE(d)=MEDIT_AI_SCHEDULE_DAYS;return;}else if(cmd=='6'){OLC_MODE(d)=MEDIT_AI_SCHEDULE_ACTIVITY;write_to_output(d,"Activity (0-%d): ",AI_SCHEDULE_ACTIVITY_MAX-1);return;}else if(cmd=='7'){OLC_MODE(d)=MEDIT_AI_SCHEDULE_DESTINATION;write_to_output(d,"Destination (0-%d): ",AI_DESTINATION_MAX-1);return;}else if(cmd=='8'){if(e->destination==AI_DEST_ROOM_VNUM){OLC_VAL(d)='r';OLC_MODE(d)=MEDIT_AI_SCHEDULE_ENTRY_VALUE;write_to_output(d,"Room VNUM (0 clears): ");return;}if(e->destination==AI_DEST_PATROL){write_to_output(d,"Route stable ID (0 or none clears): ");OLC_MODE(d)=MEDIT_AI_SCHEDULE_ROUTE;return;}write_to_output(d,"Destination value is Not Applicable.\r\n");}else if(cmd=='9'){write_to_output(d,"Route stable ID (0 or none clears): ");OLC_MODE(d)=MEDIT_AI_SCHEDULE_ROUTE;return;}else if(cmd=='a'||cmd=='b'){OLC_VAL(d)=cmd=='a'?0:1;OLC_MODE(d)=MEDIT_AI_SCHEDULE_ACTION;write_to_output(d,"Action (0-%d): ",AI_SCHEDULE_ACTION_MAX-1);return;}else if(cmd=='c'){OLC_MODE(d)=MEDIT_AI_SCHEDULE_INTERRUPT;write_to_output(d,"Interruption policy (0-%d): ",AI_INTERRUPT_MAX-1);return;}else if(cmd=='d'){OLC_MODE(d)=MEDIT_AI_SCHEDULE_FAILURE;write_to_output(d,"Failure policy (0-%d): ",AI_FAILURE_MAX-1);return;}else if(cmd=='v'){ai_actor_schedule_preview(OLC_MOB(d)->ai_config,((35*time_info.month)+(time_info.day+1))%7,time_info.hours,report,sizeof(report));write_to_output(d,"%s",report);}else if(cmd=='x'){ai_actor_schedule_validate(OLC_MOB(d)->ai_config,report,sizeof(report));write_to_output(d,"%s",report);}else if(cmd=='2'||cmd=='3'||cmd=='5'||cmd=='e'||cmd=='f'||cmd=='g'){OLC_VAL(d)=cmd;OLC_MODE(d)=MEDIT_AI_SCHEDULE_ENTRY_VALUE;write_to_output(d,"Value: ");return;}else write_to_output(d,"Invalid entry choice.\r\n");medit_disp_ai_schedule_entry(d);return;}
  case MEDIT_AI_SCHEDULE_ENTRY_VALUE: {struct ai_schedule_entry*e=medit_selected_entry(d);int *v=NULL,max=0,min=0;if(!e){medit_disp_ai_schedule_entries(d);return;}if(OLC_VAL(d)=='r'){i=atoi(arg);if(i<0||(i&&real_room(i)==NOWHERE))write_to_output(d,"Invalid room VNUM.\r\n");else if(e->destination_value!=i){e->destination_value=i;OLC_VAL(d)=1;}medit_disp_ai_schedule_entry(d);return;}if(OLC_VAL(d)=='2'){v=&e->start_hour;max=23;}else if(OLC_VAL(d)=='3'){v=&e->end_hour;max=23;}else if(OLC_VAL(d)=='5'){v=&e->priority;min=-100;max=100;}else if(OLC_VAL(d)=='e'){v=&e->max_travel_time;max=3600;}else if(OLC_VAL(d)=='f'){v=&e->max_attempts;min=1;max=100;}else {v=&e->wait_duration;max=3600;}i=atoi(arg);if(i<min||i>max)write_to_output(d,"Invalid value.\r\n");else if(*v!=i){*v=i;OLC_VAL(d)=1;}medit_disp_ai_schedule_entry(d);return;}

  case MEDIT_AI_SCHEDULE_DAYS: {struct ai_schedule_entry*e=medit_selected_entry(d);char cmd=LOWER(*arg);int bit=atoi(arg)-1;if(!e){medit_disp_ai_schedule_entries(d);return;}if(cmd=='q'){medit_disp_ai_schedule_entry(d);return;}if(cmd=='a')i=AI_DAY_MASK_ALL;else if(cmd=='w')i=0x1f;else if(cmd=='e')i=0x60;else if(cmd=='c')i=0;else if(bit>=0&&bit<7)i=e->day_mask^(1<<bit);else{i=-1;write_to_output(d,"Invalid day mask.\r\n");}if(i==0)write_to_output(d,"Invalid day mask.\r\n");else if(i>0&&e->day_mask!=i){e->day_mask=i;OLC_VAL(d)=1;}write_to_output(d,"\r\nDay Mask: %s\r\nA) Every day  W) Weekdays  E) Weekends  1-7) Toggle day  C) Clear (not permitted)  Q) Return\r\nChoice: ",medit_sched_days(e->day_mask));OLC_MODE(d)=MEDIT_AI_SCHEDULE_DAYS;return;}
  case MEDIT_AI_SCHEDULE_ACTIVITY: {struct ai_schedule_entry*e=medit_selected_entry(d);i=atoi(arg);if(!e){medit_disp_ai_schedule_entries(d);return;}if(i<0||i>=AI_SCHEDULE_ACTIVITY_MAX)write_to_output(d,"Unsupported activity.\r\n");else if(e->activity!=i){e->activity=i;OLC_VAL(d)=1;}medit_disp_ai_schedule_entry(d);return;}
  case MEDIT_AI_SCHEDULE_DESTINATION: {struct ai_schedule_entry*e=medit_selected_entry(d);i=atoi(arg);if(!e){medit_disp_ai_schedule_entries(d);return;}if(i<0||i>=AI_DESTINATION_MAX)write_to_output(d,"Unsupported destination.\r\n");else if(e->destination!=i){e->destination=i;OLC_VAL(d)=1;}medit_disp_ai_schedule_entry(d);return;}
  case MEDIT_AI_SCHEDULE_ROUTE: {struct ai_schedule_entry*e=medit_selected_entry(d);int id=atoi(arg),k; if(!e){medit_disp_ai_schedule_entries(d);return;}if(!str_cmp(arg,"0")||!str_cmp(arg,"none"))id=0;else{for(k=0;k<OLC_MOB(d)->ai_config->patrol_count;k++)if(OLC_MOB(d)->ai_config->patrols[k].id==id)break;if(k==OLC_MOB(d)->ai_config->patrol_count){write_to_output(d,"Invalid route index.\r\n");medit_disp_ai_schedule_entry(d);return;}}if(e->route_id!=id){e->route_id=id;OLC_VAL(d)=1;}medit_disp_ai_schedule_entry(d);return;}
  case MEDIT_AI_SCHEDULE_ACTION: {struct ai_schedule_entry*e=medit_selected_entry(d);i=atoi(arg);if(!e){medit_disp_ai_schedule_entries(d);return;}if(i<0||i>=AI_SCHEDULE_ACTION_MAX)write_to_output(d,"Invalid action.\r\n");else if(OLC_VAL(d)==0&&e->arrival_action!=i){e->arrival_action=i;OLC_VAL(d)=1;}else if(OLC_VAL(d)==1&&e->departure_action!=i){e->departure_action=i;OLC_VAL(d)=1;}medit_disp_ai_schedule_entry(d);return;}
  case MEDIT_AI_SCHEDULE_INTERRUPT: {struct ai_schedule_entry*e=medit_selected_entry(d);i=atoi(arg);if(!e){medit_disp_ai_schedule_entries(d);return;}if(i<0||i>=AI_INTERRUPT_MAX)write_to_output(d,"Invalid policy.\r\n");else if(e->interruption_policy!=i){e->interruption_policy=i;OLC_VAL(d)=1;}medit_disp_ai_schedule_entry(d);return;}
  case MEDIT_AI_PATROL_ROUTES: {struct mob_ai_config*c=OLC_MOB(d)->ai_config;char cmd=LOWER(*arg);int n=atoi(arg+1)-1;if(cmd=='q'){medit_disp_ai_schedule(d);return;}if(cmd=='a'){struct ai_patrol_route z;memset(&z,0,sizeof(z));z.enabled=TRUE;z.loop_mode=AI_PATROL_LOOP;if(ai_patrol_add(c,&z))OLC_VAL(d)=1;else write_to_output(d,"Maximum route count reached.\r\n");}else if(cmd=='e'){if(n<0||n>=c->patrol_count)write_to_output(d,"Invalid route index.\r\n");else{medit_schedule_store(d,c->patrols[n].id,-1,-1);medit_disp_ai_patrol_route(d);return;}}else if(cmd=='d'){if(n<0||n>=c->patrol_count)write_to_output(d,"Invalid route index.\r\n");else{medit_schedule_store(d,c->patrols[n].id,-1,-1);OLC_MODE(d)=MEDIT_AI_PATROL_DELETE;write_to_output(d,"Delete route? (Y/N): ");return;}}else if(cmd=='u'||cmd=='n'||cmd=='c'||cmd=='t'){if(n<0||n>=c->patrol_count)write_to_output(d,"Invalid route index.\r\n");else if(cmd=='u'&&ai_patrol_move(c,n,n-1))OLC_VAL(d)=1;else if(cmd=='n'&&ai_patrol_move(c,n,n+1))OLC_VAL(d)=1;else if(cmd=='c'){if(ai_patrol_duplicate(c,n))OLC_VAL(d)=1;else write_to_output(d,"Maximum route count reached.\r\n");}else if(cmd=='t'){c->patrols[n].enabled=!c->patrols[n].enabled;OLC_VAL(d)=1;}else write_to_output(d,"Invalid route index.\r\n");}else write_to_output(d,"Invalid route command.\r\n");medit_disp_ai_patrol_routes(d);return;}
  case MEDIT_AI_PATROL_DELETE: {struct ai_patrol_route*r=medit_selected_route(d);if(LOWER(*arg)=='y'&&r){int k;for(k=0;k<OLC_MOB(d)->ai_config->patrol_count;k++)if(&OLC_MOB(d)->ai_config->patrols[k]==r){if(ai_patrol_delete(OLC_MOB(d)->ai_config,k))OLC_VAL(d)=1;else write_to_output(d,"Referenced route cannot be deleted.\r\n");break;}}medit_disp_ai_patrol_routes(d);return;}
  case MEDIT_AI_PATROL_ROUTE: {struct ai_patrol_route*r=medit_selected_route(d);char cmd=LOWER(*arg),report[MAX_STRING_LENGTH];if(!r){medit_disp_ai_patrol_routes(d);return;}if(cmd=='q'){medit_disp_ai_patrol_routes(d);return;}if(cmd=='1'){OLC_MODE(d)=MEDIT_AI_PATROL_LABEL;write_to_output(d,"Builder label: ");return;}if(cmd=='2'){r->enabled=!r->enabled;OLC_VAL(d)=1;}else if(cmd=='3'){OLC_MODE(d)=MEDIT_AI_PATROL_MODE;write_to_output(d,"Route mode (0 Loop, 1 Ping-pong, 2 Once): ");return;}else if(cmd=='4'){OLC_MODE(d)=MEDIT_AI_PATROL_FAILURE;write_to_output(d,"Failure policy (0-%d): ",AI_FAILURE_MAX-1);return;}else if(cmd=='5'){medit_disp_ai_patrol_waypoints(d);return;}else if(cmd=='v'){ai_actor_patrol_preview(OLC_MOB(d)->ai_config,r->id,report,sizeof(report));write_to_output(d,"%s",report);}else if(cmd=='x'){ai_actor_schedule_validate(OLC_MOB(d)->ai_config,report,sizeof(report));write_to_output(d,"%s",report);}else write_to_output(d,"Invalid route choice.\r\n");medit_disp_ai_patrol_route(d);return;}
  case MEDIT_AI_PATROL_LABEL: {struct ai_patrol_route*r=medit_selected_route(d);if(r&&strcmp(r->label,arg)){strlcpy(r->label,arg,sizeof(r->label));OLC_VAL(d)=1;}medit_disp_ai_patrol_route(d);return;}
  case MEDIT_AI_PATROL_MODE: {struct ai_patrol_route*r=medit_selected_route(d);i=atoi(arg);if(!r){medit_disp_ai_patrol_routes(d);return;}if(i<AI_PATROL_LOOP||i>AI_PATROL_ONCE)write_to_output(d,"Unsupported route mode.\r\n");else if(r->loop_mode!=i){r->loop_mode=i;OLC_VAL(d)=1;}medit_disp_ai_patrol_route(d);return;}
  case MEDIT_AI_PATROL_FAILURE: {struct ai_patrol_route*r=medit_selected_route(d);i=atoi(arg);if(!r){medit_disp_ai_patrol_routes(d);return;}if(i<0||i>=AI_FAILURE_MAX)write_to_output(d,"Invalid policy.\r\n");else if(r->failure_policy!=i){r->failure_policy=i;OLC_VAL(d)=1;}medit_disp_ai_patrol_route(d);return;}
  case MEDIT_AI_PATROL_WAYPOINTS: {struct ai_patrol_route*r=medit_selected_route(d);char cmd=LOWER(*arg);int n=atoi(arg+1)-1;if(!r){medit_disp_ai_patrol_routes(d);return;}if(cmd=='q'){medit_disp_ai_patrol_route(d);return;}if(cmd=='a'){struct ai_patrol_waypoint z;memset(&z,0,sizeof(z));if(ai_patrol_waypoint_add(r,&z))OLC_VAL(d)=1;else write_to_output(d,"Maximum waypoint count reached.\r\n");}else if(cmd=='e'){if(n<0||n>=r->waypoint_count)write_to_output(d,"Invalid waypoint index.\r\n");else{medit_schedule_store(d,r->id,-1,n);medit_disp_ai_patrol_waypoint(d);return;}}else if(cmd=='d'){if(n<0||n>=r->waypoint_count)write_to_output(d,"Invalid waypoint index.\r\n");else{medit_schedule_store(d,r->id,-1,n);OLC_MODE(d)=MEDIT_AI_PATROL_WAYPOINT_DELETE;write_to_output(d,"Delete waypoint? (Y/N): ");return;}}else if(cmd=='u'||cmd=='n'||cmd=='c'){if(n<0||n>=r->waypoint_count)write_to_output(d,"Invalid waypoint index.\r\n");else if(cmd=='u'&&ai_patrol_waypoint_move(r,n,n-1))OLC_VAL(d)=1;else if(cmd=='n'&&ai_patrol_waypoint_move(r,n,n+1))OLC_VAL(d)=1;else if(cmd=='c'){if(ai_patrol_waypoint_duplicate(r,n))OLC_VAL(d)=1;else write_to_output(d,"Maximum waypoint count reached.\r\n");}else write_to_output(d,"Invalid waypoint index.\r\n");}else write_to_output(d,"Invalid waypoint command.\r\n");medit_disp_ai_patrol_waypoints(d);return;}
  case MEDIT_AI_PATROL_WAYPOINT_DELETE: {struct ai_patrol_route*r=medit_selected_route(d);int rid,e,w;if(r&&LOWER(*arg)=='y'){medit_schedule_load(d,&rid,&e,&w);if(ai_patrol_waypoint_delete(r,w))OLC_VAL(d)=1;}medit_disp_ai_patrol_waypoints(d);return;}
  case MEDIT_AI_PATROL_WAYPOINT: {struct ai_patrol_route*r=medit_selected_route(d);int rid,e,w;char cmd=LOWER(*arg);if(!r){medit_disp_ai_patrol_routes(d);return;}medit_schedule_load(d,&rid,&e,&w);if(cmd=='q'){medit_disp_ai_patrol_waypoints(d);return;}if(w<0||w>=r->waypoint_count){medit_disp_ai_patrol_waypoints(d);return;}if(cmd>='1'&&cmd<='3'){OLC_VAL(d)=cmd;OLC_MODE(d)=MEDIT_AI_PATROL_WAYPOINT_VALUE;write_to_output(d,cmd=='1'?"Room VNUM: ":cmd=='2'?"Wait duration: ":"Action (0-9): ");return;}medit_disp_ai_patrol_waypoint(d);return;}
  case MEDIT_AI_PATROL_WAYPOINT_VALUE: {struct ai_patrol_route*r=medit_selected_route(d);int rid,e,w;if(!r){medit_disp_ai_patrol_routes(d);return;}medit_schedule_load(d,&rid,&e,&w);if(w<0||w>=r->waypoint_count){medit_disp_ai_patrol_waypoints(d);return;}i=atoi(arg);if(OLC_VAL(d)=='1'){if(i<=0||real_room(i)==NOWHERE)write_to_output(d,"Invalid room VNUM.\r\n");else if(r->waypoints[w].room_vnum!=i){r->waypoints[w].room_vnum=i;OLC_VAL(d)=1;}}else if(OLC_VAL(d)=='2'){if(i<0||i>3600)write_to_output(d,"Invalid value.\r\n");else if(r->waypoints[w].wait_duration!=i){r->waypoints[w].wait_duration=i;OLC_VAL(d)=1;}}else {if(i<0||i>=AI_SCHEDULE_ACTION_MAX)write_to_output(d,"Invalid action.\r\n");else if(r->waypoints[w].arrival_action!=i){r->waypoints[w].arrival_action=i;OLC_VAL(d)=1;}}medit_disp_ai_patrol_waypoint(d);return;}

  case MEDIT_AI_ENABLE_CONFIRM:
    if (!str_cmp(arg, "y") || !str_cmp(arg, "yes")) { SET_BIT_AR(MOB_FLAGS(OLC_MOB(d)), MOB_AI_ACTOR); if (!OLC_MOB(d)->ai_config) OLC_MOB(d)->ai_config=mob_ai_config_new(); OLC_VAL(d)=1; medit_disp_ai_menu(d); return; }
    if (!str_cmp(arg, "n") || !str_cmp(arg, "no")) { medit_disp_menu(d); return; }
    write_to_output(d,"Please answer Y or N: "); return;
  case MEDIT_AI_MODE:
  case MEDIT_AI_ROLE:
  case MEDIT_AI_MOVEMENT:
    if (LOWER(*arg) == 'q') { medit_disp_ai_menu(d); return; }
    if (LOWER(*arg) == 'h') { write_to_output(d, "Select a listed numeric option. Changes preserve other builder-authored fields and override masks.\r\n"); return; }
    if (!OLC_MOB(d)->ai_config) OLC_MOB(d)->ai_config = mob_ai_config_new();
    if (OLC_MODE(d) == MEDIT_AI_MODE) { if (!medit_parse_ai_integer(arg, MOB_AI_INFERRED, MOB_AI_INFERRED_OVERRIDES, &i)) { write_to_output(d, "Choose 0, 1, or 2: "); return; } OLC_MOB(d)->ai_config->mode = i; }
    else if (OLC_MODE(d) == MEDIT_AI_ROLE) { if (!medit_parse_ai_integer(arg, ROLE_UNKNOWN, ROLE_BOSS, &i)) { write_to_output(d, "Choose a listed role: "); return; } OLC_MOB(d)->ai_config->role = i; OLC_MOB(d)->ai_config->override_mask |= AI_OVERRIDE_ROLE; }
    else { if (!medit_parse_ai_integer(arg, AI_MOVE_STATIONARY, AI_MOVE_RETURN_HOME, &i)) { write_to_output(d, "Choose a listed movement mode: "); return; } OLC_MOB(d)->ai_config->movement = i; OLC_MOB(d)->ai_config->override_mask |= AI_OVERRIDE_MOVEMENT; }
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
