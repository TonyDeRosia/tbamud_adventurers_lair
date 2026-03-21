/**************************************************************************
*  File: magic.c                                           Part of tbaMUD *
*  Usage: Low-level functions for magic; spell template code.             *
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
#include "interpreter.h"
#include "constants.h"
#include "dg_scripts.h"
#include "class.h"
#include "race.h"
#include "fight.h"
#include "mud_event.h"
#include "criticalhits.h"


/* local file scope function prototypes */
static int mag_materials(struct char_data *ch, IDXTYPE item0, IDXTYPE item1, IDXTYPE item2, int extract, int verbose);
static void perform_mag_groups(int level, struct char_data *ch, struct char_data *tch, int spellnum, int savetype);
bool is_spirit_spell(int spellnum);
int mystic_spirit_cap(struct char_data *ch);
static int active_spirit_count(struct char_data *ch);
bool can_bind_spirit(struct char_data *ch, int spellnum);
bool is_sanctuary_spell(int spellnum);
static void strip_sanctuary_effects(struct char_data *victim);
static void sanctuary_messages(int spellnum, const char **to_vict, const char **to_room);
static enum damage_type current_spell_damage_type = DAM_NONE;

static int spell_dur_short(int level) { return 2 + (level / 10); }
static int spell_dur_medium(int level) { return 4 + (level / 8); }
static int spell_dur_long(int level) { return 6 + (level / 6); }
static int spell_dmg_low(int level) { return (level * 2) + dice(2, MAX(1, level / 4)); }
static int spell_dmg_medium(int level) { return (level * 3) + dice(3, MAX(1, level / 3)); }
static int spell_dmg_high(int level) { return (level * 4) + dice(4, MAX(1, level / 2)); }
static int spell_dmg_extreme(int level) { return (level * 5) + dice(5, MAX(1, level / 2)); }

void set_spell_damage_type(enum damage_type type)
{
  current_spell_damage_type = type;
}

bool is_sanctuary_spell(int spellnum)
{
  switch (spellnum) {
  case SPELL_SANCTUARY:
  case SPELL_ARCANE_WARD:
  case SPELL_EVASION:
  case SPELL_IRONSKIN:
  case SPELL_DIVINE_BULWARK:
  case SPELL_SONG_OF_RESILIENCE:
  case SPELL_DARK_AEGIS:
  case SPELL_NIRVANA:
  case SPELL_BEAR_SPIRIT:
    return TRUE;
  default:
    return FALSE;
  }
}

static void strip_sanctuary_effects(struct char_data *victim)
{
  struct affected_type *af, *next;

  for (af = victim->affected; af; af = next) {
    next = af->next;

    if (IS_SET_AR(af->bitvector, AFF_SANCTUARY) &&
        af->spell > 0 && af->spell <= MAX_SPELLS &&
        is_sanctuary_spell(af->spell))
      affect_remove(victim, af);
  }
}

static void sanctuary_messages(int spellnum, const char **to_vict, const char **to_room)
{
  switch (spellnum) {
  case SPELL_ARCANE_WARD:
    *to_vict = "Arcane runes flare to life, warding you from harm.";
    *to_room = "$n is surrounded by shimmering arcane runes that bend incoming force.";
    break;
  case SPELL_SANCTUARY:
    *to_vict = "A divine presence shields you.";
    *to_room = "A soft divine glow surrounds $n, warding them from harm.";
    break;
  case SPELL_EVASION:
    *to_vict = "Your senses sharpen as you prepare to evade incoming attacks.";
    *to_room = "$n moves with heightened awareness, slipping instinctively away from danger.";
    break;
  case SPELL_IRONSKIN:
    *to_vict = "Your skin hardens, turning aside incoming blows.";
    *to_room = "$n's skin hardens like forged steel.";
    break;
  case SPELL_DIVINE_BULWARK:
    *to_vict = "Holy power forms a bulwark around you.";
    *to_room = "A radiant bulwark of holy power surrounds $n.";
    break;
  case SPELL_SONG_OF_RESILIENCE:
    *to_vict = "A resonant melody weaves resilience into your body.";
    *to_room = "A steady, resonant melody surrounds $n, dampening the force of attacks.";
    break;
  case SPELL_DARK_AEGIS:
    *to_vict = "Dark energies form a protective aegis around you.";
    *to_room = "Shadows coil tightly around $n, absorbing incoming harm.";
    break;
  case SPELL_NIRVANA:
    *to_vict = "Your eyes glow green as you enter a state of primal nirvana.";
    *to_room = "$n grows unnaturally still as $s eyes begin to glow green with primal serenity.";
    break;
  case SPELL_BEAR_SPIRIT:
    *to_vict = "The spirit of the bear fortifies your body.";
    *to_room = "$n's form thickens with primal strength as the spirit of the bear envelops $m.";
    break;
  default:
    *to_vict = "A white aura momentarily surrounds you.";
    *to_room = "$n is surrounded by a white aura.";
    break;
  }
}


/* Negative apply_saving_throw[] values make saving throws better! So do
 * negative modifiers.  Though people may be used to the reverse of that.
 * It's due to the code modifying the target saving throw instead of the
 * random number of the character as in some other systems. */
int mag_savingthrow(struct char_data *ch, int type, int modifier)
{
  /* NPCs use warrior tables according to some book */
  int class_sav = CLASS_WARRIOR;
  int save;

  if (!IS_NPC(ch))
    class_sav = GET_CLASS(ch);

  save = saving_throws(class_sav, type, GET_LEVEL(ch));
  save += GET_SAVE(ch, type);
  save += modifier;
  if (IN_ROOM(ch) != NOWHERE &&
      room_has_effect(&world[IN_ROOM(ch)], ROOM_EFFECT_CONSECRATE) &&
      IS_GOOD(ch))
    save -= 5;

  /* Throwing a 0 is always a failure. */
  if (MAX(1, save) < rand_number(0, 99))
    return (TRUE);

  /* Oops, failed. Sorry. */
  return (FALSE);
}

/* affect_update: called from comm.c (causes spells to wear off) */
void affect_update(void)
{
  struct affected_type *af, *next;
  struct char_data *i;

  for (i = character_list; i; i = i->next)
    for (af = i->affected; af; af = next) {
      next = af->next;
      if (af->duration >= 1)
	af->duration--;
      else if (af->duration == -1)	/* No action */
	;
      else {
        if ((af->spell > 0) && (af->spell <= MAX_SPELLS))
          if (!af->next || (af->next->spell != af->spell) ||
              (af->next->duration > 0))
            if (spell_info[af->spell].wear_off_msg) {
              if (is_sanctuary_spell(af->spell)) {
                act(spell_info[af->spell].wear_off_msg, TRUE, i, 0, 0, TO_ROOM);
                act(spell_info[af->spell].wear_off_msg, FALSE, i, 0, 0, TO_CHAR);
              } else
                send_to_char(i, "%s\r\n", spell_info[af->spell].wear_off_msg);
            }
	affect_remove(i, af);
      }
    }
}

/* Checks for up to 3 vnums (spell reagents) in the player's inventory. If
 * multiple vnums are passed in, the function ANDs the items together as
 * requirements (ie. if one or more are missing, the spell will not fail).
 * @param ch The caster of the spell.
 * @param item0 The first required item of the spell, NOTHING if not required.
 * @param item1 The second required item of the spell, NOTHING if not required.
 * @param item2 The third required item of the spell, NOTHING if not required.
 * @param extract TRUE if mag_materials should consume (destroy) the items in
 * the players inventory, FALSE if not. Items will only be removed on a
 * successful cast.
 * @param verbose TRUE to provide some generic failure or success messages,
 * FALSE to send no in game messages from this function.
 * @retval int TRUE if ch has all materials to cast the spell, FALSE if not.
 */
static int mag_materials(struct char_data *ch, IDXTYPE item0,
    IDXTYPE item1, IDXTYPE item2, int extract, int verbose)
{
  /* Begin Local variable definitions. */
  /*------------------------------------------------------------------------*/
  /* Used for object searches. */
  struct obj_data *tobj = NULL;
  /* Points to found reagents. */
  struct obj_data *obj0 = NULL, *obj1 = NULL, *obj2 = NULL;
  /*------------------------------------------------------------------------*/
  /* End Local variable definitions. */

  /* Begin success checks. Checks must pass to signal a success. */
  /*------------------------------------------------------------------------*/
  /* Check for the objects in the players inventory. */
  for (tobj = ch->carrying; tobj; tobj = tobj->next_content)
  {
    if ((item0 != NOTHING) && (GET_OBJ_VNUM(tobj) == item0))
    {
      obj0 = tobj;
      item0 = NOTHING;
    }
    else if ((item1 != NOTHING) && (GET_OBJ_VNUM(tobj) == item1))
    {
      obj1 = tobj;
      item1 = NOTHING;
    }
    else if ((item2 != NOTHING) && (GET_OBJ_VNUM(tobj) == item2))
    {
      obj2 = tobj;
      item2 = NOTHING;
    }
  }

  /* If we needed items, but didn't find all of them, then the spell is a
   * failure. */
  if ((item0 != NOTHING) || (item1 != NOTHING) || (item2 != NOTHING))
  {
    /* Generic spell failure messages. */
    if (verbose)
    {
      switch (rand_number(0, 2))
      {
      case 0:
        send_to_char(ch, "A wart sprouts on your nose.\r\n");
        break;
      case 1:
        send_to_char(ch, "Your hair falls out in clumps.\r\n");
        break;
      case 2:
        send_to_char(ch, "A huge corn develops on your big toe.\r\n");
        break;
      }
    }
    /* Return fales, the material check has failed. */
    return (FALSE);
  }
  /*------------------------------------------------------------------------*/
  /* End success checks. */

  /* From here on, ch has all required materials in their inventory and the
   * material check will return a success. */

  /* Begin Material Processing. */
  /*------------------------------------------------------------------------*/
  /* Extract (destroy) the materials, if so called for. */
  if (extract)
  {
    if (obj0 != NULL)
      extract_obj(obj0);
    if (obj1 != NULL)
      extract_obj(obj1);
    if (obj2 != NULL)
      extract_obj(obj2);
    /* Generic success messages that signals extracted objects. */
    if (verbose)
    {
      send_to_char(ch, "A puff of smoke rises from your pack.\r\n");
      act("A puff of smoke rises from $n's pack.", TRUE, ch, NULL, NULL, TO_ROOM);
    }
  }

  /* Don't extract the objects, but signal materials successfully found. */
  if(!extract && verbose)
  {
    send_to_char(ch, "Your pack rumbles.\r\n");
    act("Something rumbles in $n's pack.", TRUE, ch, NULL, NULL, TO_ROOM);
  }
  /*------------------------------------------------------------------------*/
  /* End Material Processing. */

  /* Signal to calling function that the materials were successfully found
   * and processed. */
  return (TRUE);
}


/* Every spell that does damage comes through here.  This calculates the amount
 * of damage, adds in any modifiers, determines what the saves are, tests for
 * save and calls damage(). -1 = dead, otherwise the amount of damage done. */
int mag_damage(int level, struct char_data *ch, struct char_data *victim,
		     int spellnum, int savetype)
{
  int dam = 0;
  int save_modifier = 0;
  enum damage_type local_damage_type = current_spell_damage_type;

  if (victim == NULL || ch == NULL)
    return (0);

  current_spell_damage_type = DAM_NONE;

  switch (spellnum) {
    /* Mostly mages */
  case SPELL_MAGIC_MISSILE:
  case SPELL_CHILL_TOUCH:	/* chill touch also has an affect */
    if (IS_MAGIC_USER(ch))
      dam = dice(1, 8) + 1;
    else
      dam = dice(1, 6) + 1;
    dam += MAX(0, level / 12);
    break;
  case SPELL_BURNING_HANDS:
    if (local_damage_type == DAM_NONE) local_damage_type = DAM_FIRE;
    if (IS_MAGIC_USER(ch))
      dam = dice(3, 8) + 3;
    else
      dam = dice(3, 6) + 3;
    break;
  case SPELL_SHOCKING_GRASP:
    if (local_damage_type == DAM_NONE) local_damage_type = DAM_LIGHTNING;
    if (IS_MAGIC_USER(ch))
      dam = dice(5, 8) + 5;
    else
      dam = dice(5, 6) + 5;
    break;
  case SPELL_LIGHTNING_BOLT:
    if (local_damage_type == DAM_NONE) local_damage_type = DAM_LIGHTNING;
    if (IS_MAGIC_USER(ch))
      dam = dice(7, 8) + 7;
    else
      dam = dice(7, 6) + 7;
    break;
  case SPELL_COLOR_SPRAY:
    if (IS_MAGIC_USER(ch))
      dam = dice(9, 8) + 9;
    else
      dam = dice(9, 6) + 9;
    break;
  case SPELL_FIREBALL:
    if (local_damage_type == DAM_NONE) local_damage_type = DAM_FIRE;
    if (IS_MAGIC_USER(ch))
      dam = dice(11, 8) + 11;
    else
      dam = dice(11, 6) + 11;
    break;

    /* Mostly clerics */
  case SPELL_DISPEL_EVIL:
    dam = dice(6, 8) + 6;
    if (IS_EVIL(ch)) {
      victim = ch;
      dam = GET_HIT(ch) - 1;
    } else if (IS_GOOD(victim)) {
      act("The gods protect $N.", FALSE, ch, 0, victim, TO_CHAR);
      return (0);
    }
    break;
  case SPELL_DISPEL_GOOD:
    dam = dice(6, 8) + 6;
    if (IS_GOOD(ch)) {
      victim = ch;
      dam = GET_HIT(ch) - 1;
    } else if (IS_EVIL(victim)) {
      act("The gods protect $N.", FALSE, ch, 0, victim, TO_CHAR);
      return (0);
    }
    break;


  case SPELL_CALL_LIGHTNING:
    if (local_damage_type == DAM_NONE) local_damage_type = DAM_LIGHTNING;
    dam = dice(7, 8) + 7;
    break;

  case SPELL_HARM:
    dam = dice(8, 8) + 8;
    break;

  case SPELL_ENERGY_DRAIN:
    if (local_damage_type == DAM_NONE) local_damage_type = DAM_NECROTIC;
    if (GET_LEVEL(victim) <= 2)
      dam = 100;
    else
      dam = dice(1, 10);
    break;
  case SPELL_FIREBOLT:
    if (local_damage_type == DAM_NONE) local_damage_type = DAM_FIRE;
    dam = spell_dmg_low(level);
    break;
  case SPELL_FLAME_ARROW:
    if (local_damage_type == DAM_NONE) local_damage_type = DAM_FIRE;
    dam = spell_dmg_medium(level);
    break;
  case SPELL_FROSTBITE:
    if (local_damage_type == DAM_NONE) local_damage_type = DAM_COLD;
    dam = spell_dmg_low(level);
    break;
  case SPELL_VOLTAIC_BOLT:
    if (local_damage_type == DAM_NONE) local_damage_type = DAM_LIGHTNING;
    dam = spell_dmg_medium(level);
    break;
  case SPELL_ACID_BLAST:
    if (local_damage_type == DAM_NONE) local_damage_type = DAM_ACID;
    dam = spell_dmg_medium(level);
    break;
  case SPELL_SHADOW_BOLT:
    if (local_damage_type == DAM_NONE) local_damage_type = DAM_SHADOW;
    dam = spell_dmg_medium(level);
    break;
  case SPELL_VAMPIRIC_TOUCH:
    if (local_damage_type == DAM_NONE) local_damage_type = DAM_NECROTIC;
    dam = spell_dmg_medium(level);
    break;
  case SPELL_DISRUPT:
    if (local_damage_type == DAM_NONE) local_damage_type = DAM_ARCANE;
    dam = spell_dmg_low(level);
    break;

    /* Area spells */
  case SPELL_EARTHQUAKE:
    if (local_damage_type == DAM_NONE) local_damage_type = DAM_EARTH;
    dam = dice(2, 8) + level;
    break;

  } /* switch(spellnum) */


  if (victim) {
    int level_gap = GET_LEVEL(ch) - GET_LEVEL(victim);
    if (spellnum == SPELL_MAGIC_MISSILE || spellnum == SPELL_CHILL_TOUCH) {
      if (level_gap >= 8)
        save_modifier -= 8 + MIN(10, level_gap / 3);
      else if (level_gap < 0)
        save_modifier += MIN(6, (-level_gap) / 4);
    }
  }

  /* divide damage by two if victim makes his saving throw */
  if (mag_savingthrow(victim, savetype, save_modifier))
    dam /= 2;

  if (spellnum == SPELL_MAGIC_MISSILE && victim) {
    int level_gap = GET_LEVEL(ch) - GET_LEVEL(victim);
    if (level_gap >= 10 && dam > 0)
      dam = MAX(2, dam);
  }

  if (spellnum == SPELL_MAGIC_MISSILE && CONFIG_DEBUG_MODE >= NRM &&
      GET_LEVEL(ch) >= LVL_BUILDER)
    send_to_char(ch,
      "\t1Combat Debug:\r\n"
      "   \t2Magic Missile Caster Lvl:\t3%d\r\n"
      "   \t2Magic Missile Victim Lvl:\t3%d\r\n"
      "   \t2Magic Missile Save Adj:\t3%d\r\n"
      "   \t2Magic Missile Final Damage:\t3%d\tn\r\n",
      GET_LEVEL(ch), GET_LEVEL(victim), save_modifier, dam);

  if (spellnum == SPELL_SHADOW_BOLT && IS_GOOD(victim))
    dam = (dam * 125) / 100;

  if (dam > 0 && AFF_FLAGGED(ch, AFF_EMPOWERED))
    dam = (dam * 110) / 100;
  if (dam > 0 && ch && GET_SKILL(ch, SKILL_SUPREME_CASTER_DISCIPLINE) > 0) {
    int bonus_pct = 5;
    if (affected_by_spell(ch, SPELL_ENCHANTERS_FOCUS))
      bonus_pct += 10;
    if (bonus_pct > 20)
      bonus_pct = 20;
    dam = (dam * (100 + bonus_pct)) / 100;
  }

  /* and finally, inflict the damage */

  /* Spell crits (mag_damage) */
  if (dam > 0 && ch && victim) {
    int mult = 200;
    if (crit_check_spell(ch, &mult)) {
      dam = (dam * mult) / 100;
      crit_show_banner(ch, victim, mult);
    }
  }

  set_next_damage_type(local_damage_type);
  return (damage(ch, victim, dam, spellnum));
}

/* Every spell that does an affect comes through here.  This determines the
 * effect, whether it is added or replacement, whether it is legal or not, etc.
 * affect_join(vict, aff, add_dur, avg_dur, add_mod, avg_mod) */
#define MAX_SPELL_AFFECTS 5	/* change if more needed */

void mag_affects(int level, struct char_data *ch, struct char_data *victim,
                      int spellnum, int savetype)
{
  struct affected_type af[MAX_SPELL_AFFECTS];
  bool accum_affect = FALSE, accum_duration = FALSE;
  bool refresh_on_recast = FALSE;
  const char *to_vict = NULL, *to_room = NULL;
  int i, j, buff_duration;


  if (victim == NULL || ch == NULL)
    return;

  if (is_spirit_spell(spellnum) && !can_bind_spirit(ch, spellnum))
    return;

  if (is_sanctuary_spell(spellnum))
    strip_sanctuary_effects(victim);

  for (i = 0; i < MAX_SPELL_AFFECTS; i++) {
    new_affect(&(af[i]));
    af[i].spell = spellnum;
  }

  switch (spellnum) {

  case SPELL_CHILL_TOUCH:
    af[0].location = APPLY_STR;
    if (mag_savingthrow(victim, savetype, 0))
      af[0].duration = 1;
    else
      af[0].duration = 4;
    af[0].modifier = -1;
    accum_duration = TRUE;
    to_vict = "You feel your strength wither!";
    break;

  case SPELL_ARMOR:
    af[0].location = APPLY_AC;
    af[0].modifier = 20;
    af[0].duration = 12 + (level / 6);
    if (af[0].duration > 18)
      af[0].duration = 18;
    refresh_on_recast = TRUE;
    to_vict = "You feel someone protecting you.";
    break;

  case SPELL_BLESS:
    buff_duration = 12 + (level / 6);
    if (buff_duration > 18)
      buff_duration = 18;

    af[0].location = APPLY_HITROLL;
    af[0].modifier = 2;
    af[0].duration = buff_duration;

    af[1].location = APPLY_SAVING_SPELL;
    af[1].modifier = -1;
    af[1].duration = buff_duration;

    refresh_on_recast = TRUE;
    to_vict = "You feel righteous.";
    break;

  case SPELL_BLINDNESS:
    if (MOB_FLAGGED(victim, MOB_NOBLIND) || GET_LEVEL(victim) >= LVL_IMMORT || mag_savingthrow(victim, savetype, 0)) {
      send_to_char(ch, "You fail.\r\n");
      return;
    }

    af[0].location = APPLY_HITROLL;
    af[0].modifier = -4;
    af[0].duration = 2;
    SET_BIT_AR(af[0].bitvector, AFF_BLIND);

    af[1].location = APPLY_AC;
    af[1].modifier = -40;
    af[1].duration = 2;
    SET_BIT_AR(af[1].bitvector, AFF_BLIND);

    to_room = "$n seems to be blinded!";
    to_vict = "You have been blinded!";
    break;

  case SPELL_CURSE:
    if (mag_savingthrow(victim, savetype, 0)) {
      send_to_char(ch, "%s", CONFIG_NOEFFECT);
      return;
    }

    af[0].location = APPLY_HITROLL;
    af[0].duration = 1 + (GET_LEVEL(ch) / 2);
    af[0].modifier = -1;
    SET_BIT_AR(af[0].bitvector, AFF_CURSE);

    af[1].location = APPLY_DAMROLL;
    af[1].duration = 1 + (GET_LEVEL(ch) / 2);
    af[1].modifier = -1;
    SET_BIT_AR(af[1].bitvector, AFF_CURSE);

    accum_duration = TRUE;
    accum_affect = TRUE;
    to_room = "$n briefly glows red!";
    to_vict = "You feel very uncomfortable.";
    break;

  case SPELL_DETECT_ALIGN:
    af[0].duration = 12 + (level / 6);
    if (af[0].duration > 18)
      af[0].duration = 18;
    SET_BIT_AR(af[0].bitvector, AFF_DETECT_ALIGN);
    refresh_on_recast = TRUE;
    to_vict = "Your eyes tingle.";
    break;

  case SPELL_DETECT_INVIS:
    af[0].duration = 12 + (level / 6);
    if (af[0].duration > 18)
      af[0].duration = 18;
    SET_BIT_AR(af[0].bitvector, AFF_DETECT_INVIS);
    refresh_on_recast = TRUE;
    to_vict = "Your eyes tingle.";
    break;

  case SPELL_DETECT_MAGIC:
    af[0].duration = 12 + (level / 6);
    if (af[0].duration > 18)
      af[0].duration = 18;
    SET_BIT_AR(af[0].bitvector, AFF_DETECT_MAGIC);
    refresh_on_recast = TRUE;
    to_vict = "Your eyes tingle.";
    break;

  case SPELL_FLY:
    af[0].duration = 15 + (level / 5);
    if (af[0].duration > 24)
      af[0].duration = 24;
    SET_BIT_AR(af[0].bitvector, AFF_FLYING);
    refresh_on_recast = TRUE;
    to_vict = "You float above the ground.";
    break;

  case SPELL_INFRAVISION:
    af[0].duration = 12 + (level / 6);
    if (af[0].duration > 18)
      af[0].duration = 18;
    SET_BIT_AR(af[0].bitvector, AFF_INFRAVISION);
    refresh_on_recast = TRUE;
    to_vict = "Your eyes glow red.";
    to_room = "$n's eyes glow red.";
    break;

  case SPELL_INVISIBLE:
    if (!victim)
      victim = ch;

    af[0].duration = 12 + (GET_LEVEL(ch) / 6);
    if (af[0].duration > 20)
      af[0].duration = 20;
    af[0].modifier = 40;
    af[0].location = APPLY_AC;
    SET_BIT_AR(af[0].bitvector, AFF_INVISIBLE);
    refresh_on_recast = TRUE;
    to_vict = "You vanish.";
    to_room = "$n slowly fades out of existence.";
    break;

  case SPELL_POISON:
    if (mag_savingthrow(victim, savetype, 0)) {
      send_to_char(ch, "%s", CONFIG_NOEFFECT);
      return;
    }

    af[0].location = APPLY_STR;
    af[0].duration = GET_LEVEL(ch);
    af[0].modifier = -2;
    SET_BIT_AR(af[0].bitvector, AFF_POISON);
    to_vict = "You feel very sick.";
    to_room = "$n gets violently ill!";
    break;

  case SPELL_PROT_FROM_EVIL:
    af[0].duration = 15 + (level / 5);
    if (af[0].duration > 24)
      af[0].duration = 24;
    SET_BIT_AR(af[0].bitvector, AFF_PROTECT_EVIL);
    refresh_on_recast = TRUE;
    to_vict = "You feel invulnerable!";
    break;

  case SPELL_SANCTUARY:
  case SPELL_ARCANE_WARD:
  case SPELL_EVASION:
  case SPELL_IRONSKIN:
  case SPELL_DIVINE_BULWARK:
  case SPELL_SONG_OF_RESILIENCE:
  case SPELL_DARK_AEGIS:
  case SPELL_NIRVANA:
    sanctuary_messages(spellnum, &to_vict, &to_room);
    af[0].duration = 18 + (level / 4);
    if (af[0].duration > 30)
      af[0].duration = 30;
    SET_BIT_AR(af[0].bitvector, AFF_SANCTUARY);
    refresh_on_recast = TRUE;
    break;

  case SPELL_SLEEP:
    if (!CONFIG_PK_ALLOWED && !IS_NPC(ch) && !IS_NPC(victim))
      return;
    if (MOB_FLAGGED(victim, MOB_NOSLEEP))
      return;
    if (mag_savingthrow(victim, savetype, 0))
      return;

    af[0].duration = 4 + (GET_LEVEL(ch) / 4);
    SET_BIT_AR(af[0].bitvector, AFF_SLEEP);

    if (GET_POS(victim) > POS_SLEEPING) {
      send_to_char(victim, "You feel very sleepy...  Zzzz......\r\n");
      act("$n goes to sleep.", TRUE, victim, 0, 0, TO_ROOM);
      GET_POS(victim) = POS_SLEEPING;
    }
    break;

  case SPELL_STRENGTH:
    if (GET_ADD(victim) == 100)
      return;

    af[0].location = APPLY_STR;
    af[0].duration = (GET_LEVEL(ch) / 2) + 4;
    af[0].modifier = 1 + (level > 18);
    accum_duration = TRUE;
    accum_affect = TRUE;
    to_vict = "You feel stronger!";
    break;

  case SPELL_SENSE_LIFE:
    to_vict = "Your feel your awareness improve.";
    af[0].duration = 12 + (level / 6);
    if (af[0].duration > 18)
      af[0].duration = 18;
    SET_BIT_AR(af[0].bitvector, AFF_SENSE_LIFE);
    refresh_on_recast = TRUE;
    break;

  case SPELL_WATERWALK:
    af[0].duration = 15 + (level / 5);
    if (af[0].duration > 24)
      af[0].duration = 24;
    SET_BIT_AR(af[0].bitvector, AFF_WATERWALK);
    refresh_on_recast = TRUE;
    to_vict = "You feel webbing between your toes.";
    break;

  case SPELL_BEAR_SPIRIT:
    sanctuary_messages(spellnum, &to_vict, &to_room);
    buff_duration = 18 + (level / 4);
    if (buff_duration > 30)
      buff_duration = 30;
    af[0].duration = buff_duration;
    SET_BIT_AR(af[0].bitvector, AFF_SANCTUARY);

    af[1].location = APPLY_AC;
    af[1].modifier = 15;
    af[1].duration = buff_duration;

    af[2].location = APPLY_HIT;
    af[2].modifier = 10;
    af[2].duration = buff_duration;

    af[3].location = APPLY_SAVING_BREATH;
    af[3].modifier = -1;
    af[3].duration = buff_duration;

    refresh_on_recast = TRUE;
    break;

  case SPELL_WOLF_SPIRIT:
    buff_duration = 18 + (level / 4);
    if (buff_duration > 30)
      buff_duration = 30;

    af[0].location = APPLY_HITROLL;
    af[0].modifier = 2;
    af[0].duration = buff_duration;

    af[1].location = APPLY_DEX;
    af[1].modifier = 1;
    af[1].duration = buff_duration;

    af[2].location = APPLY_MELEE_CRIT;
    af[2].modifier = 1;
    af[2].duration = buff_duration;

    refresh_on_recast = TRUE;
    to_vict = "A wolf spirit sharpens your predatory instincts.";
    break;

  case SPELL_TIGER_SPIRIT:
    buff_duration = 18 + (level / 4);
    if (buff_duration > 30)
      buff_duration = 30;

    af[0].location = APPLY_DAMROLL;
    af[0].modifier = 2;
    af[0].duration = buff_duration;

    af[1].location = APPLY_HITROLL;
    af[1].modifier = 1;
    af[1].duration = buff_duration;

    af[2].location = APPLY_MELEE_CRIT_MULT;
    af[2].modifier = 1;
    af[2].duration = buff_duration;

    refresh_on_recast = TRUE;
    to_vict = "A stalking tiger spirit urges you to strike from the shadows.";
    break;

  case SPELL_EAGLE_SPIRIT:
    buff_duration = 18 + (level / 4);
    if (buff_duration > 30)
      buff_duration = 30;

    af[0].duration = buff_duration;
    SET_BIT_AR(af[0].bitvector, AFF_DETECT_INVIS);

    af[1].duration = buff_duration;
    SET_BIT_AR(af[1].bitvector, AFF_SENSE_LIFE);

    af[2].location = APPLY_AC;
    af[2].modifier = 10;
    af[2].duration = buff_duration;

    af[3].duration = buff_duration;
    SET_BIT_AR(af[3].bitvector, AFF_FLYING);

    refresh_on_recast = TRUE;
    to_vict = "An eagle spirit guides your sight from above.";
    break;

  case SPELL_DRAGON_SPIRIT:
    buff_duration = 18 + (level / 4);
    if (buff_duration > 30)
      buff_duration = 30;

    af[0].location = APPLY_STR;
    af[0].modifier = 1;
    af[0].duration = buff_duration;

    af[1].location = APPLY_CON;
    af[1].modifier = 1;
    af[1].duration = buff_duration;

    af[2].location = APPLY_MOVE;
    af[2].modifier = 15;
    af[2].duration = buff_duration;

    af[3].location = APPLY_SAVING_BREATH;
    af[3].modifier = -2;
    af[3].duration = buff_duration;

    refresh_on_recast = TRUE;
    to_vict = "A venerable dragon spirit coils protectively around you.";
    break;

  case SPELL_FLAME_ARROW:
    af[0].duration = spell_dur_short(level);
    af[0].modifier = 5;
    af[0].location = APPLY_NONE;
    SET_BIT_AR(af[0].bitvector, AFF_BURNING);
    refresh_on_recast = TRUE;
    break;

  case SPELL_FROSTBITE:
    if (mag_savingthrow(victim, savetype, 0))
      return;
    af[0].duration = spell_dur_short(level);
    af[0].modifier = -2;
    af[0].location = APPLY_DEX;
    SET_BIT_AR(af[0].bitvector, AFF_FROZEN);
    refresh_on_recast = TRUE;
    break;

  case SPELL_VOLTAIC_BOLT:
    af[0].duration = spell_dur_short(level);
    af[0].location = APPLY_NONE;
    SET_BIT_AR(af[0].bitvector, AFF_STATIC);
    refresh_on_recast = TRUE;
    break;

  case SPELL_ACID_BLAST:
    if (mag_savingthrow(victim, savetype, 0))
      return;
    af[0].duration = spell_dur_medium(level);
    af[0].modifier = -30;
    af[0].location = APPLY_AC;
    SET_BIT_AR(af[0].bitvector, AFF_CORRODED);
    refresh_on_recast = TRUE;
    break;

  case SPELL_WEB:
    if (mag_savingthrow(victim, savetype, 0)) {
      af[0].duration = spell_dur_short(level);
      af[0].modifier = -2;
    } else {
      af[0].duration = spell_dur_medium(level);
      af[0].modifier = -4;
    }
    af[0].location = APPLY_DEX;
    SET_BIT_AR(af[0].bitvector, AFF_WEBBED);
    refresh_on_recast = TRUE;
    break;

  case SPELL_SILENCE:
    if (mag_savingthrow(victim, savetype, 0))
      return;
    af[0].duration = spell_dur_short(level);
    af[0].location = APPLY_NONE;
    SET_BIT_AR(af[0].bitvector, AFF_SILENCED);
    refresh_on_recast = TRUE;
    break;

  case SPELL_FEAR:
    if (mag_savingthrow(victim, savetype, 0)) {
      af[0].duration = 1;
      af[0].modifier = -4;
    } else {
      af[0].duration = spell_dur_short(level);
      af[0].modifier = -10;
    }
    af[0].location = APPLY_HITROLL;
    SET_BIT_AR(af[0].bitvector, AFF_FEARFUL);
    refresh_on_recast = TRUE;
    break;

  case SPELL_TRUE_SEEING:
    af[0].duration = spell_dur_long(level);
    SET_BIT_AR(af[0].bitvector, AFF_DETECT_INVIS);
    af[1].duration = spell_dur_long(level);
    SET_BIT_AR(af[1].bitvector, AFF_SENSE_LIFE);
    af[2].duration = spell_dur_long(level);
    SET_BIT_AR(af[2].bitvector, AFF_TRUESIGHT);
    refresh_on_recast = TRUE;
    break;

  case SPELL_STONE_SKIN:
    af[0].duration = spell_dur_medium(level);
    af[0].location = APPLY_AC;
    af[0].modifier = 40;
    SET_BIT_AR(af[0].bitvector, AFF_STONESKIN);
    refresh_on_recast = TRUE;
    break;

  case SPELL_BARKSKIN:
    af[0].duration = spell_dur_long(level);
    af[0].location = APPLY_AC;
    af[0].modifier = 20;
    SET_BIT_AR(af[0].bitvector, AFF_BARKSKIN);
    af[1].duration = spell_dur_long(level);
    af[1].location = APPLY_CON;
    af[1].modifier = 1;
    SET_BIT_AR(af[1].bitvector, AFF_BARKSKIN);
    refresh_on_recast = TRUE;
    break;

  case SPELL_GIANT_STRENGTH:
    af[0].duration = spell_dur_long(level);
    af[0].location = APPLY_STR;
    af[0].modifier = 4;
    af[1].duration = spell_dur_long(level);
    af[1].location = APPLY_DAMROLL;
    af[1].modifier = 3;
    refresh_on_recast = TRUE;
    break;

  case SPELL_ADRENALINE_SURGE:
    af[0].duration = 3;
    af[0].location = APPLY_STR;
    af[0].modifier = 3;
    SET_BIT_AR(af[0].bitvector, AFF_ADRENALINE);
    af[1].duration = 3;
    af[1].location = APPLY_DEX;
    af[1].modifier = 1;
    SET_BIT_AR(af[1].bitvector, AFF_ADRENALINE);
    af[2].duration = 3;
    af[2].location = APPLY_AC;
    af[2].modifier = 2;
    SET_BIT_AR(af[2].bitvector, AFF_ADRENALINE);
    refresh_on_recast = TRUE;
    break;

  case SPELL_CLARITY:
    af[0].duration = spell_dur_medium(level);
    af[0].location = APPLY_INT;
    af[0].modifier = 3;
    SET_BIT_AR(af[0].bitvector, AFF_CLARITY);
    af[1].duration = spell_dur_medium(level);
    af[1].location = APPLY_WIS;
    af[1].modifier = 2;
    SET_BIT_AR(af[1].bitvector, AFF_CLARITY);
    af[2].duration = spell_dur_medium(level);
    af[2].location = APPLY_NONE;
    af[2].modifier = 5;
    SET_BIT_AR(af[2].bitvector, AFF_CLARITY);
    refresh_on_recast = TRUE;
    break;

  case SPELL_MARK_OF_DEATH:
    if (mag_savingthrow(victim, savetype, 0))
      return;
    af[0].duration = spell_dur_medium(level);
    af[0].location = APPLY_NONE;
    SET_BIT_AR(af[0].bitvector, AFF_MARKED);
    refresh_on_recast = TRUE;
    break;

  case SPELL_BLOODLUST:
    af[0].duration = spell_dur_medium(level);
    af[0].location = APPLY_DAMROLL;
    af[0].modifier = 10;
    SET_BIT_AR(af[0].bitvector, AFF_BLOODLUST);
    refresh_on_recast = TRUE;
    break;

  case SPELL_DISRUPT:
    af[0].duration = spell_dur_short(level);
    af[0].location = APPLY_NONE;
    SET_BIT_AR(af[0].bitvector, AFF_SPELLLOCK);
    refresh_on_recast = TRUE;
    break;

  case SPELL_ANTIMAGIC_SHELL:
    af[0].duration = spell_dur_medium(level);
    af[0].location = APPLY_SAVING_SPELL;
    af[0].modifier = 20;
    SET_BIT_AR(af[0].bitvector, AFF_WARDED);
    refresh_on_recast = TRUE;
    break;

  case SPELL_ENCHANTERS_FOCUS:
    af[0].duration = spell_dur_medium(level);
    af[0].location = APPLY_NONE;
    SET_BIT_AR(af[0].bitvector, AFF_EMPOWERED);
    refresh_on_recast = TRUE;
    break;

  case SPELL_TIME_SNARE:
    if (mag_savingthrow(victim, savetype, 0))
      return;
    af[0].duration = spell_dur_short(level);
    af[0].location = APPLY_NONE;
    SET_BIT_AR(af[0].bitvector, AFF_TIME_SNARE);
    refresh_on_recast = TRUE;
    break;

  case SPELL_PHASE_SHIFT:
    af[0].duration = spell_dur_short(level);
    af[0].location = APPLY_NONE;
    SET_BIT_AR(af[0].bitvector, AFF_PHASE);
    refresh_on_recast = TRUE;
    break;

  case SPELL_MIRROR_VEIL:
    af[0].duration = spell_dur_medium(level);
    af[0].location = APPLY_NONE;
    SET_BIT_AR(af[0].bitvector, AFF_MIRROR_IMAGE);
    refresh_on_recast = TRUE;
    break;

  case SPELL_ELEMENTAL_WARD_FIRE:
    af[0].duration = spell_dur_medium(level);
    af[0].location = APPLY_NONE;
    SET_BIT_AR(af[0].bitvector, AFF_ELEMENTAL_WARD_FIRE);
    refresh_on_recast = TRUE;
    break;

  case SPELL_ELEMENTAL_WARD_COLD:
    af[0].duration = spell_dur_medium(level);
    af[0].location = APPLY_NONE;
    SET_BIT_AR(af[0].bitvector, AFF_ELEMENTAL_WARD_COLD);
    refresh_on_recast = TRUE;
    break;

  case SPELL_ELEMENTAL_WARD_LIGHTNING:
    af[0].duration = spell_dur_medium(level);
    af[0].location = APPLY_NONE;
    SET_BIT_AR(af[0].bitvector, AFF_ELEMENTAL_WARD_LIGHTNING);
    refresh_on_recast = TRUE;
    break;

  case SPELL_ELEMENTAL_WARD_ACID:
    af[0].duration = spell_dur_medium(level);
    af[0].location = APPLY_NONE;
    SET_BIT_AR(af[0].bitvector, AFF_ELEMENTAL_WARD_ACID);
    refresh_on_recast = TRUE;
    break;

  case SPELL_HOLD_PERSON:
    if (IS_NPC(victim) && GET_RACE(victim) > RACE_HUMAN) {
      send_to_char(ch, "That target is not humanoid enough for hold person.\r\n");
      return;
    }
    if (mag_savingthrow(victim, SAVING_PARA, 0)) {
      af[0].duration = 1;
      af[0].location = APPLY_NONE;
      SET_BIT_AR(af[0].bitvector, AFF_TIME_SNARE);
    } else {
      af[0].duration = spell_dur_short(level);
      af[0].location = APPLY_NONE;
      SET_BIT_AR(af[0].bitvector, AFF_STUNNED);
    }
    refresh_on_recast = TRUE;
    break;

  case SPELL_HOLD_MONSTER:
    if (GET_RACE(victim) == RACE_VAMPIRE) {
      send_to_char(ch, "Undead are resistant to this hold.\r\n");
      return;
    }
    if (mag_savingthrow(victim, SAVING_PARA, 0)) {
      af[0].duration = spell_dur_short(level);
      af[0].location = APPLY_NONE;
      SET_BIT_AR(af[0].bitvector, AFF_TIME_SNARE);
    } else {
      af[0].duration = spell_dur_medium(level);
      af[0].location = APPLY_NONE;
      SET_BIT_AR(af[0].bitvector, AFF_STUNNED);
    }
    refresh_on_recast = TRUE;
    break;

  case SPELL_CONFUSION:
    if (mag_savingthrow(victim, savetype, 0)) {
      af[0].duration = 1;
      af[0].location = APPLY_HITROLL;
      af[0].modifier = -2;
    } else {
      af[0].duration = spell_dur_short(level);
      af[0].location = APPLY_HITROLL;
      af[0].modifier = -4;
    }
    refresh_on_recast = TRUE;
    break;

  case SPELL_VERTIGO:
    if (mag_savingthrow(victim, savetype, 0)) {
      af[0].duration = 1;
      af[0].location = APPLY_DEX;
      af[0].modifier = -1;
    } else {
      af[0].duration = spell_dur_short(level);
      af[0].location = APPLY_DEX;
      af[0].modifier = -3;
      af[1].duration = spell_dur_short(level);
      af[1].location = APPLY_HITROLL;
      af[1].modifier = -4;
      af[2].duration = 1;
      af[2].location = APPLY_NONE;
      SET_BIT_AR(af[2].bitvector, AFF_STUNNED);
    }
    refresh_on_recast = TRUE;
    break;
  }

  /* If this is a mob that has this affect set in its mob file, do not perform
   * the affect.  This prevents people from un-sancting mobs by sancting them
   * and waiting for it to fade, for example. */
  if (IS_NPC(victim) && !affected_by_spell(victim, spellnum)) {
    for (i = 0; i < MAX_SPELL_AFFECTS; i++) {
      for (j=1; j<NUM_AFF_FLAGS; j++) {
        if (IS_SET_AR(af[i].bitvector, j) && AFF_FLAGGED(victim, j)) {
          send_to_char(ch, "%s", CONFIG_NOEFFECT);
          return;
        }
      }
    }
  }

  if (refresh_on_recast && affected_by_spell(victim, spellnum))
    affect_from_char(victim, spellnum);

  /* If the victim is already affected by this spell, and the spell does not
   * have an accumulative effect, then fail the spell. */
  if (affected_by_spell(victim,spellnum) && !(accum_duration||accum_affect)) {
    send_to_char(ch, "%s", CONFIG_NOEFFECT);
    return;
  }

  for (i = 0; i < MAX_SPELL_AFFECTS; i++)
    if (af[i].bitvector[0] || af[i].bitvector[1] ||
        af[i].bitvector[2] || af[i].bitvector[3] ||
        (af[i].location != APPLY_NONE))
      affect_join(victim, af+i, accum_duration, FALSE, accum_affect, FALSE);

  if (to_vict != NULL)
    act(to_vict, FALSE, victim, 0, ch, TO_CHAR);
  if (to_room != NULL)
    act(to_room, TRUE, victim, 0, ch, TO_ROOM);
}

bool is_spirit_spell(int spellnum)
{
  switch (spellnum) {
  case SPELL_BEAR_SPIRIT:
  case SPELL_WOLF_SPIRIT:
  case SPELL_TIGER_SPIRIT:
  case SPELL_EAGLE_SPIRIT:
  case SPELL_DRAGON_SPIRIT:
    return TRUE;
  default:
    return FALSE;
  }
}

int mystic_spirit_cap(struct char_data *ch)
{
  int level;

  if (ch == NULL || IS_NPC(ch) || GET_CLASS(ch) != CLASS_MYSTIC)
    return 0;

  level = GET_LEVEL(ch);

  if (level >= 80)
    return 5;
  if (level >= 60)
    return 4;
  if (level >= 40)
    return 3;
  if (level >= 20)
    return 2;
  return 1;
}

static int active_spirit_count(struct char_data *ch)
{
  int spirits = 0;

  if (ch == NULL)
    return 0;

  spirits += affected_by_spell(ch, SPELL_BEAR_SPIRIT) ? 1 : 0;
  spirits += affected_by_spell(ch, SPELL_WOLF_SPIRIT) ? 1 : 0;
  spirits += affected_by_spell(ch, SPELL_TIGER_SPIRIT) ? 1 : 0;
  spirits += affected_by_spell(ch, SPELL_EAGLE_SPIRIT) ? 1 : 0;
  spirits += affected_by_spell(ch, SPELL_DRAGON_SPIRIT) ? 1 : 0;

  return spirits;
}

bool can_bind_spirit(struct char_data *ch, int spellnum)
{
  int cap, active;

  if (!is_spirit_spell(spellnum))
    return TRUE;

  if (ch == NULL)
    return FALSE;

  if (GET_CLASS(ch) != CLASS_MYSTIC && GET_LEVEL(ch) < LVL_IMMORT) {
    send_to_char(ch, "Only mystics may bind this spirit.\r\n");
    return FALSE;
  }

  cap = mystic_spirit_cap(ch);

  if (cap == 0)
    return TRUE;

  if (affected_by_spell(ch, spellnum))
    return TRUE;

  active = active_spirit_count(ch);

  if (active >= cap) {
    send_to_char(ch, "You cannot bind another spirit; you have reached your limit of %d.\r\n", cap);
    return FALSE;
  }

  return TRUE;
}

/* This function is used to provide services to mag_groups.  This function is
 * the one you should change to add new group spells. */
static void perform_mag_groups(int level, struct char_data *ch,
			struct char_data *tch, int spellnum, int savetype)
{
  switch (spellnum) {
    case SPELL_GROUP_HEAL:
    mag_points(level, ch, tch, SPELL_HEAL, savetype);
    break;
  case SPELL_GROUP_ARMOR:
    mag_affects(level, ch, tch, SPELL_ARMOR, savetype);
    break;
  case SPELL_GROUP_RECALL:
    spell_recall(level, ch, tch, NULL);
    break;
  }
}

/* Every spell that affects the group should run through here perform_mag_groups
 * contains the switch statement to send us to the right magic. Group spells
 * affect everyone grouped with the caster who is in the room, caster last. To
 * add new group spells, you shouldn't have to change anything in mag_groups.
 * Just add a new case to perform_mag_groups. */
void mag_groups(int level, struct char_data *ch, int spellnum, int savetype)
{
  struct char_data *tch;

  if (ch == NULL)
    return;

  if (!GROUP(ch))
    return;
    
  while ((tch = (struct char_data *) simple_list(GROUP(ch)->members)) != NULL) {
    if (IN_ROOM(tch) != IN_ROOM(ch))
      continue;
    if (tch == ch)
      continue;
    perform_mag_groups(level, ch, tch, spellnum, savetype);
  }
  perform_mag_groups(level, ch, ch, spellnum, savetype);
}


/* Mass spells affect every creature in the room except the caster. No spells
 * of this class currently implemented. */
void mag_masses(int level, struct char_data *ch, int spellnum, int savetype)
{
  struct char_data *tch, *tch_next;

  for (tch = world[IN_ROOM(ch)].people; tch; tch = tch_next) {
    tch_next = tch->next_in_room;
    if (tch == ch)
      continue;
    if (!IS_NPC(tch) && GET_LEVEL(tch) >= LVL_IMMORT)
      continue;
    if (!CONFIG_PK_ALLOWED && !IS_NPC(ch) && !IS_NPC(tch))
      continue;
    if (!IS_NPC(tch) && GROUP(tch) && GROUP(ch) && GROUP(ch) == GROUP(tch))
      continue;

    switch (spellnum) {
      case SPELL_MASS_FEAR:
        if (mag_savingthrow(tch, SAVING_SPELL, 0)) {
          struct affected_type af;
          new_affect(&af);
          af.spell = spellnum;
          af.duration = 1;
          af.location = APPLY_HITROLL;
          af.modifier = -3;
          affect_join(tch, &af, FALSE, FALSE, FALSE, FALSE);
        } else {
          struct affected_type af;
          new_affect(&af);
          af.spell = spellnum;
          af.duration = spell_dur_short(level);
          af.location = APPLY_HITROLL;
          af.modifier = -10;
          SET_BIT_AR(af.bitvector, AFF_FEARFUL);
          affect_join(tch, &af, FALSE, FALSE, FALSE, FALSE);
        }
        break;
    }
  }
}

/* Every spell that affects an area (room) runs through here.  These are
 * generally offensive spells.  This calls mag_damage to do the actual damage.
 * All spells listed here must also have a case in mag_damage() in order for
 * them to work. Area spells have limited targets within the room. */
void mag_areas(int level, struct char_data *ch, int spellnum, int savetype)
{
  struct char_data *tch, *next_tch;
  const char *to_char = NULL, *to_room = NULL;
  struct char_data *primary = NULL;

  if (ch == NULL)
    return;

  /* to add spells just add the message here plus an entry in mag_damage for
   * the damaging part of the spell.   */
  switch (spellnum) {
  case SPELL_EARTHQUAKE:
    to_char = "You call a violent earthquake that shakes the ground!";
    to_room ="$n calls a violent earthquake that shakes the ground!";
    break;
  case SPELL_MIASMA:
    to_char = "You exhale a billowing cloud of toxic miasma!";
    to_room = "$n exhales a billowing cloud of toxic miasma!";
    break;
  case SPELL_TOXIC_CLOUD:
    to_char = "You summon a billowing cloud of toxic gas!";
    to_room = "$n summons a billowing cloud of toxic gas!";
    break;
  case SPELL_SHOCKWAVE:
    to_char = "A shockwave of force erupts from you!";
    to_room = "A shockwave of force erupts from $n!";
    break;
  case SPELL_NOVA:
    to_char = "You explode with a blinding nova of pure magical energy!";
    to_room = "$n explodes with a blinding nova of pure magical energy!";
    break;
  case SPELL_ICE_STORM:
    to_char = "You call a violent storm of razor ice into the room!";
    to_room = "$n calls a violent storm of razor ice into the room!";
    break;
  case SPELL_BLIZZARD:
    to_char = "You unleash a full blizzard upon your enemies!";
    to_room = "$n unleashes a full blizzard upon $s enemies!";
    break;
  case SPELL_FROST_NOVA:
    to_char = "Frost explodes outward from you in a shattering nova!";
    to_room = "Frost explodes outward from $n in a shattering nova!";
    break;
  case SPELL_FIREBALL_GREATER:
    to_char = "A massive fireball erupts from your hands, filling the room with fire!";
    to_room = "A massive fireball erupts from $n's hands, filling the room with fire!";
    break;
  case SPELL_SONIC_BURST:
    to_char = "A sonic burst explodes outward from you!";
    to_room = "A sonic burst explodes outward from $n!";
    break;
  case SPELL_WORD_OF_PAIN:
    to_char = "You speak the Word of Pain into existence!";
    to_room = "$n speaks the Word of Pain into existence!";
    break;
  }

  if (to_char != NULL)
    act(to_char, FALSE, ch, 0, 0, TO_CHAR);
  if (to_room != NULL)
    act(to_room, FALSE, ch, 0, 0, TO_ROOM);


  for (tch = world[IN_ROOM(ch)].people; tch; tch = next_tch) {
    next_tch = tch->next_in_room;

    /* The skips: 1: the caster
     *            2: immortals
     *            3: if no pk on this mud, skips over all players
     *            4: pets (charmed NPCs)
     *            5: other players in the same group (if the spell is 'violent') 
     *            6: Flying people if earthquake is the spell                         */
    if (tch == ch)
      continue;
    if (!IS_NPC(tch) && GET_LEVEL(tch) >= LVL_IMMORT)
      continue;
    if (!CONFIG_PK_ALLOWED && !IS_NPC(ch) && !IS_NPC(tch))
      continue;
    if (!IS_NPC(ch) && IS_NPC(tch) && AFF_FLAGGED(tch, AFF_CHARM))
      continue;
    if (!IS_NPC(tch) && spell_info[spellnum].violent && GROUP(tch) && GROUP(ch) && GROUP(ch) == GROUP(tch))
      continue;
	if ((spellnum == SPELL_EARTHQUAKE) && AFF_FLAGGED(tch, AFF_FLYING))
	  continue;
    if (spellnum == SPELL_GRAVITY_WELL) {
      int dam = spell_dmg_medium(level);
      if (mag_savingthrow(tch, SAVING_SPELL, 0))
        dam /= 2;
      set_next_damage_type(DAM_FORCE);
      if (damage(ch, tch, dam, spellnum) == -1)
        continue;
      continue;
    }

    if (spellnum == SPELL_MIASMA) {
      struct affected_type af;
      int dur = mag_savingthrow(tch, SAVING_SPELL, 0) ? spell_dur_short(level) : spell_dur_medium(level);
      new_affect(&af);
      af.spell = SPELL_MIASMA;
      af.duration = dur;
      af.location = APPLY_HITROLL;
      af.modifier = -5;
      affect_join(tch, &af, FALSE, FALSE, FALSE, FALSE);
      continue;
    }

    if (spellnum == SPELL_TOXIC_CLOUD) {
      struct affected_type af;
      int dur = mag_savingthrow(tch, SAVING_SPELL, 0) ? spell_dur_short(level) : spell_dur_medium(level);
      new_affect(&af);
      af.spell = SPELL_TOXIC_CLOUD;
      af.duration = dur;
      af.location = APPLY_HITROLL;
      af.modifier = 0;
      affect_join(tch, &af, FALSE, FALSE, FALSE, FALSE);
      continue;
    }

    if (spellnum == SPELL_FIREBALL_GREATER && !primary)
      primary = FIGHTING(ch) ? FIGHTING(ch) : tch;

    {
      int saved = mag_savingthrow(tch, SAVING_SPELL, 0);
      int dam = 0;
      enum damage_type dtype = DAM_FORCE;
      struct affected_type af;

      switch (spellnum) {
        case SPELL_EARTHQUAKE:
          dam = spell_dmg_medium(level);
          dtype = DAM_EARTH;
          break;
        case SPELL_SHOCKWAVE:
          dam = spell_dmg_medium(level);
          dtype = DAM_FORCE;
          break;
        case SPELL_NOVA:
          dam = spell_dmg_high(level);
          dtype = DAM_ARCANE;
          break;
        case SPELL_ICE_STORM:
          dam = spell_dmg_medium(level);
          dtype = DAM_COLD;
          break;
        case SPELL_BLIZZARD:
          dam = spell_dmg_high(level);
          dtype = DAM_COLD;
          break;
        case SPELL_FROST_NOVA:
          dam = spell_dmg_medium(level);
          dtype = DAM_COLD;
          break;
        case SPELL_FIREBALL_GREATER:
          dam = (tch == primary) ? spell_dmg_high(level) : spell_dmg_medium(level);
          dtype = DAM_FIRE;
          break;
        case SPELL_SONIC_BURST:
          dam = spell_dmg_medium(level);
          dtype = DAM_SONIC;
          break;
        case SPELL_WORD_OF_PAIN:
          dam = (GET_RACE(tch) == RACE_VAMPIRE) ? spell_dmg_extreme(level) : spell_dmg_high(level);
          dtype = DAM_SONIC;
          break;
      }

      if (saved)
        dam /= 2;
      set_next_damage_type(dtype);
      if (damage(ch, tch, dam, spellnum) == -1)
        continue;

      if (spellnum == SPELL_SHOCKWAVE && !saved) {
        new_affect(&af);
        af.spell = spellnum;
        af.duration = 1;
        af.location = APPLY_NONE;
        SET_BIT_AR(af.bitvector, AFF_STUNNED);
        affect_join(tch, &af, FALSE, FALSE, FALSE, FALSE);
        if (FIGHTING(tch))
          stop_fighting(tch);
      } else if (spellnum == SPELL_EARTHQUAKE && !saved) {
        new_affect(&af);
        af.spell = spellnum;
        af.duration = 1;
        af.location = APPLY_NONE;
        SET_BIT_AR(af.bitvector, AFF_STUNNED);
        affect_join(tch, &af, FALSE, FALSE, FALSE, FALSE);
      } else if (spellnum == SPELL_ICE_STORM && !saved) {
        new_affect(&af);
        af.spell = spellnum;
        af.duration = spell_dur_short(level);
        af.location = APPLY_DEX;
        af.modifier = -1;
        affect_join(tch, &af, FALSE, FALSE, FALSE, FALSE);
      } else if (spellnum == SPELL_BLIZZARD) {
        if (!saved) {
          new_affect(&af);
          af.spell = spellnum;
          af.duration = spell_dur_medium(level);
          af.location = APPLY_NONE;
          SET_BIT_AR(af.bitvector, AFF_TIME_SNARE);
          affect_join(tch, &af, FALSE, FALSE, FALSE, FALSE);
          new_affect(&af);
          af.spell = spellnum;
          af.duration = spell_dur_medium(level);
          af.location = APPLY_DEX;
          af.modifier = -3;
          affect_join(tch, &af, FALSE, FALSE, FALSE, FALSE);
        } else {
          new_affect(&af);
          af.spell = spellnum;
          af.duration = spell_dur_short(level);
          af.location = APPLY_DEX;
          af.modifier = -1;
          affect_join(tch, &af, FALSE, FALSE, FALSE, FALSE);
        }
      } else if (spellnum == SPELL_FROST_NOVA && !saved) {
        new_affect(&af);
        af.spell = spellnum;
        af.duration = spell_dur_short(level);
        af.location = APPLY_NONE;
        SET_BIT_AR(af.bitvector, AFF_ROOTED);
        affect_join(tch, &af, FALSE, FALSE, FALSE, FALSE);
      } else if (spellnum == SPELL_SONIC_BURST && !saved) {
        new_affect(&af);
        af.spell = spellnum;
        af.duration = 1;
        af.location = APPLY_NONE;
        SET_BIT_AR(af.bitvector, AFF_STUNNED);
        affect_join(tch, &af, FALSE, FALSE, FALSE, FALSE);
      } else if (spellnum == SPELL_WORD_OF_PAIN && !saved) {
        new_affect(&af);
        af.spell = spellnum;
        af.duration = 2;
        af.location = APPLY_NONE;
        SET_BIT_AR(af.bitvector, AFF_STUNNED);
        affect_join(tch, &af, FALSE, FALSE, FALSE, FALSE);
        new_affect(&af);
        af.spell = spellnum;
        af.duration = spell_dur_short(level);
        af.location = APPLY_NONE;
        SET_BIT_AR(af.bitvector, AFF_SILENCED);
        affect_join(tch, &af, FALSE, FALSE, FALSE, FALSE);
      }
    }
  }
}

/*----------------------------------------------------------------------------*/
/* Begin Magic Summoning - Generic Routines and Local Globals */
/*----------------------------------------------------------------------------*/

/* Every spell which summons/gates/conjours a mob comes through here. */
/* These use act(), don't put the \r\n. */
static const char *mag_summon_msgs[] = {
  "\r\n",
  "$n makes a strange magical gesture; you feel a strong breeze!",
  "$n animates a corpse!",
  "$N appears from a cloud of thick blue smoke!",
  "$N appears from a cloud of thick green smoke!",
  "$N appears from a cloud of thick red smoke!",
  "$N disappears in a thick black cloud!"
  "As $n makes a strange magical gesture, you feel a strong breeze.",
  "As $n makes a strange magical gesture, you feel a searing heat.",
  "As $n makes a strange magical gesture, you feel a sudden chill.",
  "As $n makes a strange magical gesture, you feel the dust swirl.",
  "$n magically divides!",
  "$n animates a corpse!"
};

/* Keep the \r\n because these use send_to_char. */
static const char *mag_summon_fail_msgs[] = {
  "\r\n",
  "There are no such creatures.\r\n",
  "Uh oh...\r\n",
  "Oh dear.\r\n",
  "Gosh durnit!\r\n",
  "The elements resist!\r\n",
  "You failed.\r\n",
  "There is no corpse!\r\n"
};

/* Defines for Mag_Summons */
#define MOB_CLONE            10   /**< vnum for the clone mob. */
#define OBJ_CLONE            161  /**< vnum for clone material. */
#define MOB_ZOMBIE           11   /**< vnum for the zombie mob. */

void mag_summons(int level, struct char_data *ch, struct obj_data *obj,
		      int spellnum, int savetype)
{
  struct char_data *mob = NULL;
  struct obj_data *tobj, *next_obj;
  int pfail = 0, msg = 0, fmsg = 0, num = 1, handle_corpse = FALSE, i;
  mob_vnum mob_num;

  if (ch == NULL)
    return;

  switch (spellnum) {
  case SPELL_CLONE:
    msg = 10;
    fmsg = rand_number(2, 6);	/* Random fail message. */
    mob_num = MOB_CLONE;
    /*
     * We have designated the clone spell as the example for how to use the
     * mag_materials function.
     * In stock tbaMUD it checks to see if the character has item with
     * vnum 161 which is a set of sacrificial entrails. If we have the entrails
     * the spell will succeed,  and if not, the spell will fail 102% of the time
     * (prevents random success... see below).
     * The object is extracted and the generic cast messages are displayed.
     */
    if( !mag_materials(ch, OBJ_CLONE, NOTHING, NOTHING, TRUE, TRUE) )
      pfail = 102; /* No materials, spell fails. */
    else
      pfail = 0;	/* We have the entrails, spell is successfully cast. */
    break;

  case SPELL_ANIMATE_DEAD:
    if (obj == NULL || !IS_CORPSE(obj)) {
      act(mag_summon_fail_msgs[7], FALSE, ch, 0, 0, TO_CHAR);
      return;
    }
    handle_corpse = TRUE;
    msg = 11;
    fmsg = rand_number(2, 6);	/* Random fail message. */
    mob_num = MOB_ZOMBIE;
    pfail = 10;	/* 10% failure, should vary in the future. */
    break;

  default:
    return;
  }

  if (AFF_FLAGGED(ch, AFF_CHARM)) {
    send_to_char(ch, "You are too giddy to have any followers!\r\n");
    return;
  }
  if (rand_number(0, 101) < pfail) {
    send_to_char(ch, "%s", mag_summon_fail_msgs[fmsg]);
    return;
  }
  for (i = 0; i < num; i++) {
    if (!(mob = read_mobile(mob_num, VIRTUAL))) {
      send_to_char(ch, "You don't quite remember how to make that creature.\r\n");
      return;
    }
    char_to_room(mob, IN_ROOM(ch));
    IS_CARRYING_W(mob) = 0;
    IS_CARRYING_N(mob) = 0;
    SET_BIT_AR(AFF_FLAGS(mob), AFF_CHARM);
    if (spellnum == SPELL_CLONE) {
      /* Don't mess up the prototype; use new string copies. */
      mob->player.name = strdup(GET_NAME(ch));
      mob->player.short_descr = strdup(GET_NAME(ch));
    }
    act(mag_summon_msgs[msg], FALSE, ch, 0, mob, TO_ROOM);
    load_mtrigger(mob);
    add_follower(mob, ch);
    
    if (GROUP(ch) && GROUP_LEADER(GROUP(ch)) == ch)
      join_group(mob, GROUP(ch));    
  }
  if (handle_corpse) {
    for (tobj = obj->contains; tobj; tobj = next_obj) {
      next_obj = tobj->next_content;
      obj_from_obj(tobj);
      obj_to_char(tobj, mob);
    }
    extract_obj(obj);
  }
}

/* Clean up the defines used for mag_summons. */
#undef MOB_CLONE
#undef OBJ_CLONE
#undef MOB_ZOMBIE

/*----------------------------------------------------------------------------*/
/* End Magic Summoning - Generic Routines and Local Globals */
/*----------------------------------------------------------------------------*/


void mag_points(int level, struct char_data *ch, struct char_data *victim,
		     int spellnum, int savetype)
{
  int healing = 0, move = 0;

  if (victim == NULL)
    return;

  switch (spellnum) {
  case SPELL_CURE_LIGHT:
    healing = dice(1, 8) + 1 + (level / 4);
    send_to_char(victim, "You feel better.\r\n");
    break;
  case SPELL_CURE_CRITIC:
    healing = dice(3, 8) + 3 + (level / 4);
    send_to_char(victim, "You feel a lot better!\r\n");
    break;
  case SPELL_HEAL:
    healing = 100 + dice(3, 8);
    send_to_char(victim, "A warm feeling floods your body.\r\n");
    break;
  }
  
  /* Heal crits (mag_points), even out of combat */
  if (healing > 0 && ch && victim) {
    int mult = 200;
    if (crit_check_heal(ch, &mult)) {
      healing = (healing * mult) / 100;
      crit_show_banner(ch, victim, mult);
    }
  }

GET_HIT(victim) = MIN(GET_MAX_HIT(victim), GET_HIT(victim) + healing);
  GET_MOVE(victim) = MIN(GET_MAX_MOVE(victim), GET_MOVE(victim) + move);
  update_pos(victim);
}

void mag_unaffects(int level, struct char_data *ch, struct char_data *victim,
		        int spellnum, int type)
{
  int spell = 0, msg_not_affected = TRUE;
  const char *to_vict = NULL, *to_room = NULL;

  if (victim == NULL)
    return;

  switch (spellnum) {
  case SPELL_HEAL:
    /* Heal also restores health, so don't give the "no effect" message if the
     * target isn't afflicted by the 'blindness' spell. */
    msg_not_affected = FALSE;
    /* fall-through */
  case SPELL_CURE_BLIND:
    spell = SPELL_BLINDNESS;
    to_vict = "Your vision returns!";
    to_room = "There's a momentary gleam in $n's eyes.";
    break;
  case SPELL_REMOVE_POISON: {
    int removed = 0;
    struct affected_type *af, *next_af;

    /* Remove poison affect by spell id */
    if (affected_by_spell(victim, SPELL_POISON)) {
      affect_from_char(victim, SPELL_POISON);
      removed = 1;
    }

    /* Remove any affects that set AFF_POISON, regardless of spell id */
    for (af = victim->affected; af; af = next_af) {
      next_af = af->next;
      if (IS_SET_AR(af->bitvector, AFF_POISON)) {
        affect_remove(victim, af);
        removed = 1;
      }
    }

    /* Clear the flag if it was set directly */
    if (AFF_FLAGGED(victim, AFF_POISON)) {
      REMOVE_BIT_AR(AFF_FLAGS(victim), AFF_POISON);
      removed = 1;
    }

    if (!removed) {
      act("Nothing seems to happen.", FALSE, ch, 0, victim, TO_CHAR);
    } else {
      act("You feel less sick.", FALSE, victim, 0, 0, TO_CHAR);
      act("$n looks less sick.", TRUE, victim, 0, 0, TO_ROOM);
    }

    /* Message handling */
    if (removed) {
      to_vict = "You feel less sick.";
      to_room = "$n looks less sick.";
    } else {
      to_vict = "Nothing seems to happen.";
    }

    break;
  }
  case SPELL_REMOVE_CURSE:
    spell = SPELL_CURSE;
    to_vict = "You don't feel so unlucky.";
    break;
  default:
    log("SYSERR: unknown spellnum %d passed to mag_unaffects.", spellnum);
    return;
  }
  /* remove poison handled here: avoid generic 'Nothing seems to happen.' */
  if (spellnum == SPELL_REMOVE_POISON) {
    if (to_vict != NULL)
      act(to_vict, FALSE, ch, 0, victim, TO_VICT);
    if (to_room != NULL)
      act(to_room, FALSE, ch, 0, victim, TO_NOTVICT);
    return;
  }


  if (!affected_by_spell(victim, spell)) {
    if (msg_not_affected)
      send_to_char(ch, "%s", CONFIG_NOEFFECT);
    return;
  }

  affect_from_char(victim, spell);
  if (to_vict != NULL)
    act(to_vict, FALSE, victim, 0, ch, TO_CHAR);
  if (to_room != NULL)
    act(to_room, TRUE, victim, 0, ch, TO_ROOM);
}

void mag_alter_objs(int level, struct char_data *ch, struct obj_data *obj,
		         int spellnum, int savetype)
{
  const char *to_char = NULL, *to_room = NULL;

  if (obj == NULL)
    return;

  switch (spellnum) {
    case SPELL_BLESS:
      if (!OBJ_FLAGGED(obj, ITEM_BLESS) &&
	  (GET_OBJ_WEIGHT(obj) <= 5 * GET_LEVEL(ch))) {
	SET_BIT_AR(GET_OBJ_EXTRA(obj), ITEM_BLESS);
	to_char = "$p glows briefly.";
      }
      break;
    case SPELL_CURSE:
      if (!OBJ_FLAGGED(obj, ITEM_NODROP)) {
	SET_BIT_AR(GET_OBJ_EXTRA(obj), ITEM_NODROP);
	if (GET_OBJ_TYPE(obj) == ITEM_WEAPON)
	  GET_OBJ_VAL(obj, 2)--;
	to_char = "$p briefly glows red.";
      }
      break;
    case SPELL_INVISIBLE:
      if (!OBJ_FLAGGED(obj, ITEM_NOINVIS) && !OBJ_FLAGGED(obj, ITEM_INVISIBLE)) {
        SET_BIT_AR(GET_OBJ_EXTRA(obj), ITEM_INVISIBLE);
        to_char = "$p vanishes.";
      }
      break;
    case SPELL_POISON:
      if (((GET_OBJ_TYPE(obj) == ITEM_DRINKCON) ||
         (GET_OBJ_TYPE(obj) == ITEM_FOUNTAIN) ||
         (GET_OBJ_TYPE(obj) == ITEM_FOOD)) && !GET_OBJ_VAL(obj, 3)) {
      GET_OBJ_VAL(obj, 3) = 1;
      to_char = "$p steams briefly.";
      }
      break;
    case SPELL_REMOVE_CURSE:
      if (OBJ_FLAGGED(obj, ITEM_NODROP)) {
        REMOVE_BIT_AR(GET_OBJ_EXTRA(obj), ITEM_NODROP);
        if (GET_OBJ_TYPE(obj) == ITEM_WEAPON)
          GET_OBJ_VAL(obj, 2)++;
        to_char = "$p briefly glows blue.";
      }
      break;
    case SPELL_REMOVE_POISON:
      if (((GET_OBJ_TYPE(obj) == ITEM_DRINKCON) ||
         (GET_OBJ_TYPE(obj) == ITEM_FOUNTAIN) ||
         (GET_OBJ_TYPE(obj) == ITEM_FOOD)) && GET_OBJ_VAL(obj, 3)) {
        GET_OBJ_VAL(obj, 3) = 0;
        to_char = "$p steams briefly.";
      }
      break;
  }

  if (to_char == NULL)
    send_to_char(ch, "%s", CONFIG_NOEFFECT);
  else
    act(to_char, TRUE, ch, obj, 0, TO_CHAR);

  if (to_room != NULL)
    act(to_room, TRUE, ch, obj, 0, TO_ROOM);
  else if (to_char != NULL)
    act(to_char, TRUE, ch, obj, 0, TO_ROOM);
}

void mag_creations(int level, struct char_data *ch, int spellnum)
{
  struct obj_data *tobj;
  obj_vnum z;

  if (ch == NULL)
    return;
  /* level = MAX(MIN(level, LVL_IMPL), 1); - Hm, not used. */

  if (spellnum != SPELL_CREATE_FOOD) {
    send_to_char(ch, "Nothing happens.\r\n");
    log("SYSERR: mag_creations: unsupported spell routing (spell %d, caster %s).",
        spellnum, GET_NAME(ch));
    return;
  }

  z = 10;

  if (!(tobj = read_object(z, VIRTUAL))) {
    send_to_char(ch, "I seem to have goofed.\r\n");
    log("SYSERR: spell_creations, spell %d, obj %d: obj not found",
	    spellnum, z);
    return;
  }
  obj_to_char(tobj, ch);
  act("$n creates $p.", FALSE, ch, tobj, 0, TO_ROOM);
  act("You create $p.", FALSE, ch, tobj, 0, TO_CHAR);
  load_otrigger(tobj);
}

void mag_rooms(int level, struct char_data *ch, int spellnum)
{
  room_rnum rnum;
  int duration = 0;
  bool failure = FALSE;
  event_id IdNum = eNULL;
  const char *msg = NULL;
  const char *room = NULL;
  
  rnum = IN_ROOM(ch);
  
  if (ROOM_FLAGGED(rnum, ROOM_NOMAGIC))
    failure = TRUE;
  
  switch (spellnum) {
    case SPELL_DARKNESS:
      IdNum = eSPL_DARKNESS;
      if (ROOM_FLAGGED(rnum, ROOM_DARK))
        failure = TRUE;
        
      duration = 5;
      SET_BIT_AR(ROOM_FLAGS(rnum), ROOM_DARK);
        
      msg = "You cast a shroud of darkness upon the area.";
      room = "$n casts a shroud of darkness upon this area.";
    break;
    case SPELL_NULL_FIELD:
      if (room_has_effect(&world[rnum], ROOM_EFFECT_NULL_FIELD))
        failure = TRUE;
      duration = spell_dur_medium(level);
      room_add_effect(&world[rnum], ROOM_EFFECT_NULL_FIELD, duration, 0);
      msg = "You create a null field that suppresses all magic in the area!";
      room = "$n creates a null field that suppresses all magic in the area!";
      break;
    case SPELL_SILENCE_FIELD:
      if (room_has_effect(&world[rnum], ROOM_EFFECT_SILENCE_FIELD))
        failure = TRUE;
      duration = spell_dur_short(level);
      room_add_effect(&world[rnum], ROOM_EFFECT_SILENCE_FIELD, duration, 0);
      msg = "A field of magical silence falls over the entire area!";
      room = "$n silences the entire area with a wave of a hand!";
      break;
    case SPELL_WALL_OF_FIRE:
      if (room_has_effect(&world[rnum], ROOM_EFFECT_WALL_OF_FIRE))
        failure = TRUE;
      duration = spell_dur_medium(level);
      room_add_effect(&world[rnum], ROOM_EFFECT_WALL_OF_FIRE, duration, 0);
      msg = "You conjure a wall of magical fire!";
      room = "$n conjures a wall of magical fire!";
      break;
    case SPELL_STATIC_FIELD:
      if (room_has_effect(&world[rnum], ROOM_EFFECT_STATIC_FIELD))
        failure = TRUE;
      duration = spell_dur_medium(level);
      room_add_effect(&world[rnum], ROOM_EFFECT_STATIC_FIELD, duration, 0);
      msg = "You charge the air with crackling static electricity!";
      room = "$n charges the air with crackling static electricity!";
      break;
    case SPELL_CONSECRATE:
      if (room_has_effect(&world[rnum], ROOM_EFFECT_CONSECRATE))
        failure = TRUE;
      duration = spell_dur_long(level);
      room_add_effect(&world[rnum], ROOM_EFFECT_CONSECRATE, duration, 0);
      msg = "You consecrate this ground in the name of your deity!";
      room = "$n consecrates this ground in the name of $s deity!";
      break;
    case SPELL_GRAVITY_WELL:
      if (room_has_effect(&world[rnum], ROOM_EFFECT_GRAVITY_WELL))
        failure = TRUE;
      duration = spell_dur_short(level);
      room_add_effect(&world[rnum], ROOM_EFFECT_GRAVITY_WELL, duration, 0);
      msg = "You create a crushing gravity well that pins your enemies!";
      room = "$n creates a crushing gravity well that pins enemies in place!";
      break;
    case SPELL_ACID_RAIN:
      if (room_has_effect(&world[rnum], ROOM_EFFECT_ACID_RAIN))
        failure = TRUE;
      duration = spell_dur_medium(level);
      room_add_effect(&world[rnum], ROOM_EFFECT_ACID_RAIN, duration, 0);
      msg = "You call a rain of burning acid from above!";
      room = "$n calls a rain of burning acid from above!";
      break;
  
  }
  
  if (failure || msg == NULL || room == NULL) {
    send_to_char(ch, "You failed!\r\n");
    return;
  }
  
  send_to_char(ch, "%s\r\n", msg);
  act(room, FALSE, ch, 0, 0, TO_ROOM);
  
  if (IdNum != eNULL)
    NEW_EVENT(IdNum, &world[rnum], NULL, duration * PASSES_PER_SEC);
}
