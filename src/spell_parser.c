/**************************************************************************
 *  File: spell_parser.c                                    Part of tbaMUD *
 *  Usage: Top-level magic routines; outside points of entry to magic sys. *
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
#include "interpreter.h"
#include "spells.h"
#include "class.h"
#include "race.h"
#include "handler.h"
#include "comm.h"
#include "db.h"
#include "dg_scripts.h"
#include "fight.h"  /* for hit() */

#define SINFO spell_info[spellnum]

/* Global Variables definitions, used elsewhere */
struct spell_info_type spell_info[TOP_SPELL_DEFINE + 1];
char cast_arg2[MAX_INPUT_LENGTH];
const char *unused_spellname = "!UNUSED!"; /* So we can get &unused_spellname */

/* Local (File Scope) Function Prototypes */
static void say_spell(struct char_data *ch, int spellnum, struct char_data *tch,
    struct obj_data *tobj);
static void spello(int spl, const char *name, int max_mana, int min_mana,
    int mana_change, int minpos, int targets, int violent, int routines,
    const char *wearoff);
static int mag_manacost(struct char_data *ch, int spellnum);
static bool is_spellup_beneficial_spell(int spellnum);
static int reflect_suppressed = 0;

int spell_on_cooldown(struct char_data *ch, int spellnum)
{
  if (!ch || IS_NPC(ch) || spellnum < 0 || spellnum > MAX_SKILLS)
    return 0;
  return GET_SPELL_COOLDOWN(ch, spellnum) > 0;
}

void set_spell_cooldown(struct char_data *ch, int spellnum, int rounds)
{
  if (!ch || IS_NPC(ch) || spellnum < 0 || spellnum > MAX_SKILLS)
    return;
  GET_SPELL_COOLDOWN(ch, spellnum) = MAX(0, rounds);
}

void tick_spell_cooldowns(struct char_data *ch)
{
  int i;
  if (!ch || IS_NPC(ch))
    return;

  for (i = 0; i <= MAX_SKILLS; i++)
    if (GET_SPELL_COOLDOWN(ch, i) > 0)
      GET_SPELL_COOLDOWN(ch, i)--;
}

int room_has_effect(struct room_data *room, int effect_type)
{
  struct room_effect_data *eff;
  if (!room)
    return 0;

  for (eff = room->effects; eff; eff = eff->next)
    if (eff->effect_type == effect_type)
      return 1;
  return 0;
}

void room_add_effect(struct room_data *room, int effect_type, int duration, int modifier)
{
  struct room_effect_data *eff;
  if (!room || duration <= 0)
    return;

  for (eff = room->effects; eff; eff = eff->next) {
    if (eff->effect_type == effect_type) {
      eff->duration = MAX(eff->duration, duration);
      eff->modifier = modifier;
      return;
    }
  }

  CREATE(eff, struct room_effect_data, 1);
  eff->effect_type = effect_type;
  eff->duration = duration;
  eff->modifier = modifier;
  eff->next = room->effects;
  room->effects = eff;
}

void room_tick_effects(struct room_data *room)
{
  struct room_effect_data *eff, *next, *prev = NULL;
  if (!room)
    return;

  for (eff = room->effects; eff; eff = next) {
    struct char_data *tch, *next_tch;
    next = eff->next;

    for (tch = room->people; tch; tch = next_tch) {
      struct affected_type af;
      int saved = FALSE;
      next_tch = tch->next_in_room;

      if (!tch || (!IS_NPC(tch) && GET_LEVEL(tch) >= LVL_IMMORT))
        continue;

      switch (eff->effect_type) {
        case ROOM_EFFECT_WALL_OF_FIRE:
          set_next_damage_type(DAM_FIRE);
          if (damage(tch, tch, 8, TYPE_SUFFERING) == -1)
            continue;
          if (!AFF_FLAGGED(tch, AFF_BURNING)) {
            saved = mag_savingthrow(tch, SAVING_SPELL, 0);
            if (!saved) {
              new_affect(&af);
              af.spell = SPELL_WALL_OF_FIRE;
              af.duration = 2;
              af.location = APPLY_NONE;
              SET_BIT_AR(af.bitvector, AFF_BURNING);
              affect_join(tch, &af, FALSE, FALSE, FALSE, FALSE);
            }
          }
          break;
        case ROOM_EFFECT_STATIC_FIELD:
          set_next_damage_type(DAM_LIGHTNING);
          if (damage(tch, tch, AFF_FLAGGED(tch, AFF_STATIC) ? 8 : 6, TYPE_SUFFERING) == -1)
            continue;
          if (!AFF_FLAGGED(tch, AFF_STATIC)) {
            saved = mag_savingthrow(tch, SAVING_SPELL, 0);
            if (!saved) {
              new_affect(&af);
              af.spell = SPELL_STATIC_FIELD;
              af.duration = 2;
              af.location = APPLY_NONE;
              SET_BIT_AR(af.bitvector, AFF_STATIC);
              affect_join(tch, &af, FALSE, FALSE, FALSE, FALSE);
            }
          }
          break;
        case ROOM_EFFECT_CONSECRATE:
          if (IS_EVIL(tch) && !affected_by_spell(tch, SPELL_CONSECRATE)) {
            new_affect(&af);
            af.spell = SPELL_CONSECRATE;
            af.duration = 1;
            af.location = APPLY_HITROLL;
            af.modifier = -2;
            affect_join(tch, &af, FALSE, FALSE, FALSE, FALSE);
          }
          if (GET_RACE(tch) == RACE_VAMPIRE) {
            set_next_damage_type(DAM_HOLY);
            if (damage(tch, tch, 5, TYPE_SUFFERING) == -1)
              continue;
          }
          break;
        case ROOM_EFFECT_GRAVITY_WELL:
          set_next_damage_type(DAM_FORCE);
          if (damage(tch, tch, 6, TYPE_SUFFERING) == -1)
            continue;
          break;
        case ROOM_EFFECT_ACID_RAIN:
          set_next_damage_type(DAM_ACID);
          if (damage(tch, tch, 7, TYPE_SUFFERING) == -1)
            continue;
          if (!AFF_FLAGGED(tch, AFF_CORRODED)) {
            saved = mag_savingthrow(tch, SAVING_SPELL, 0);
            if (!saved) {
              new_affect(&af);
              af.spell = SPELL_ACID_RAIN;
              af.duration = MAX(1, eff->duration);
              af.location = APPLY_AC;
              af.modifier = 20;
              SET_BIT_AR(af.bitvector, AFF_CORRODED);
              affect_join(tch, &af, FALSE, FALSE, FALSE, FALSE);
            }
          }
          break;
      }
    }

    if (eff->duration > 0)
      eff->duration--;
    if (eff->duration <= 0) {
      switch (eff->effect_type) {
        case ROOM_EFFECT_NULL_FIELD:
          send_to_room(real_room(room->number), "The null field collapses.\r\n");
          break;
        case ROOM_EFFECT_SILENCE_FIELD:
          send_to_room(real_room(room->number), "The field of silence dissipates.\r\n");
          break;
        case ROOM_EFFECT_WALL_OF_FIRE:
          send_to_room(real_room(room->number), "The wall of fire gutters out.\r\n");
          break;
        case ROOM_EFFECT_STATIC_FIELD:
          send_to_room(real_room(room->number), "The static field finally disperses.\r\n");
          break;
        case ROOM_EFFECT_CONSECRATE:
          send_to_room(real_room(room->number), "The consecrated ground returns to normal.\r\n");
          break;
        case ROOM_EFFECT_GRAVITY_WELL:
          send_to_room(real_room(room->number), "The crushing gravity well releases its grip.\r\n");
          break;
        case ROOM_EFFECT_ACID_RAIN:
          send_to_room(real_room(room->number), "The acid rain ceases.\r\n");
          break;
      }
      if (prev)
        prev->next = next;
      else
        room->effects = next;
      free(eff);
      continue;
    }
    prev = eff;
  }
}

void set_temp_summon_timer(struct char_data *mob, int rounds)
{
  if (!mob || !IS_NPC(mob))
    return;
  GET_SUMMON_TIMER(mob) = MAX(0, rounds);
}

struct cast_message {
  const char *to_caster;
  const char *to_room;
  const char *to_target;
};

static const struct cast_message cast_messages[] = {
  [SPELL_ARMOR] = {
    "Your skin takes on a faint, hardened sheen.",
    "A dull protective sheen settles over $n.",
    NULL
  },
  [SPELL_TELEPORT] = {
    "Reality folds and you vanish in a blink.",
    "Space ripples and $n disappears.",
    NULL
  },
  [SPELL_BLESS] = {
    "You speak a quiet blessing and strength answers.",
    "A warm glow briefly rests on $n.",
    NULL
  },
  [SPELL_BLINDNESS] = {
    "You gesture sharply and sight is stolen away.",
    "$n makes a cutting motion toward $N\u2019s eyes.",
    "Darkness swallows your vision."
  },
  [SPELL_BURNING_HANDS] = {
    "Flames spill from your palms in a hungry rush.",
    "Fire bursts from $n\u2019s hands toward $N.",
    "Fire scorches across your skin."
  },
  [SPELL_CALL_LIGHTNING] = {
    "You call upward and the sky answers with wrath.",
    "A crack of lightning answers $n\u2019s call.",
    "Lightning rips through you."
  },
  [SPELL_CHARM] = {
    "Your voice turns velvet and command slips in.",
    "$n speaks softly to $N with unsettling certainty.",
    "A strange warmth makes $n feel trustworthy."
  },
  [SPELL_CHILL_TOUCH] = {
    "Cold clings to your fingers as you reach out.",
    "Frosty darkness trails $n\u2019s hand toward $N.",
    "A dead cold grips your body."
  },
  [SPELL_CLONE] = {
    "You trace a mirror sigil and life imitates life.",
    "A wavering duplicate forms near $n.",
    NULL
  },
  [SPELL_COLOR_SPRAY] = {
    "You fling a burst of prismatic light.",
    "A fan of colors explodes from $n toward $N.",
    "Light fractures your senses."
  },
  [SPELL_CONTROL_WEATHER] = {
    "You whisper to the air and the world listens.",
    "The air shifts as if obeying $n\u2019s will.",
    NULL
  },
  [SPELL_CREATE_FOOD] = {
    "Simple words become a simple meal.",
    "Food appears as $n finishes a short chant.",
    NULL
  },
  [SPELL_CREATE_WATER] = {
    "You call forth water, clean and cold.",
    "Fresh water gathers at $n\u2019s gesture.",
    NULL
  },
  [SPELL_CURE_BLIND] = {
    "You brush away the dark and sight returns.",
    "$n\u2019s hand passes over $N\u2019s eyes with a soft glow.",
    "Warmth clears your eyes."
  },
  [SPELL_CURE_CRITIC] = {
    "You bind deep wounds with steady purpose.",
    "A strong healing glow wraps around $N.",
    "Pain pulls back as your body knits."
  },
  [SPELL_CURE_LIGHT] = {
    "You close minor wounds with a calm touch.",
    "A soft glow settles over $N.",
    "Your cuts seal and the sting fades."
  },
  [SPELL_CURSE] = {
    "You lay a bitter word that clings like ash.",
    "A shadowed hush follows $n\u2019s curse upon $N.",
    "Misfortune settles on you like a weight."
  },
  [SPELL_DETECT_ALIGN] = {
    "You focus, reading the shape of a soul.",
    "$n\u2019s eyes narrow as if seeing too much.",
    NULL
  },
  [SPELL_DETECT_INVIS] = {
    "Your sight sharpens beyond the veil.",
    "$n blinks slowly, gaze turning keen.",
    NULL
  },
  [SPELL_DETECT_MAGIC] = {
    "The world\u2019s hidden threads begin to glow.",
    "$n studies the air as if reading it.",
    NULL
  },
  [SPELL_DETECT_POISON] = {
    "Your senses turn to bitterness and danger.",
    "$n inhales carefully, eyes intent.",
    NULL
  },
  [SPELL_DISPEL_EVIL] = {
    "You drive out darkness with a fierce prayer.",
    "Light flares from $n toward $N.",
    "A searing purity burns at you."
  },
  [SPELL_EARTHQUAKE] = {
    "You slam your will into the ground.",
    "The earth heaves violently around $n.",
    NULL
  },
  [SPELL_ENCHANT_WEAPON] = {
    "You bind a sharp promise into the steel.",
    "$n\u2019s weapon glints with a brief, hungry light.",
    NULL
  },
  [SPELL_ENERGY_DRAIN] = {
    "You reach into life and pull.",
    "A dark pull radiates from $n toward $N.",
    "Strength bleeds from you in a cold rush."
  },
  [SPELL_FIREBALL] = {
    "You hurl a roaring sphere of flame.",
    "A fireball streaks from $n toward $N.",
    "Exploding heat slams into you."
  },
  [SPELL_HARM] = {
    "You speak ruin and it answers.",
    "A brutal, blackened pulse strikes $N.",
    "Agony tears through you."
  },
  [SPELL_HEAL] = {
    "You call wholeness back into flesh.",
    "Radiant light floods $N for a heartbeat.",
    "Relief washes through you as wounds vanish."
  },
  [SPELL_INVISIBLE] = {
    "You blur, then vanish from easy sight.",
    "$n shimmers and fades from view.",
    NULL
  },
  [SPELL_LIGHTNING_BOLT] = {
    "You snap your hand and lightning obeys.",
    "A bolt of lightning lashes out from $n.",
    "Lightning punches through you."
  },
  [SPELL_LOCATE_OBJECT] = {
    "Your mind reaches, seeking a familiar weight.",
    "$n grows still, listening with $s eyes.",
    NULL
  },
  [SPELL_MAGIC_MISSILE] = {
    "You flick your fingers and force darts fly.",
    "Arcane missiles snap from $n toward $N.",
    "Invisible force strikes you hard."
  },
  [SPELL_POISON] = {
    "Your words sour the air with venom.",
    "$n\u2019s curse turns the color of sickness on $N.",
    "Your blood turns cold and foul."
  },
  [SPELL_PROT_FROM_EVIL] = {
    "A firm barrier settles around you.",
    "A pale ward circles $n for a moment.",
    NULL
  },
  [SPELL_REMOVE_CURSE] = {
    "You tear away the clinging malice.",
    "A dark haze lifts from $N.",
    "The weight of the curse falls away."
  },
  [SPELL_SANCTUARY] = {
    "A gentle aura settles around you like mercy.",
    "A soft protective glow surrounds $n.",
    NULL
  },
  [SPELL_SHOCKING_GRASP] = {
    "Electricity crawls over your hand as you strike.",
    "$n grabs at $N with crackling power.",
    "Your muscles lock as shock tears through you."
  },
  [SPELL_SLEEP] = {
    "You murmur a lull and will becomes fog.",
    "$n gestures and drowsiness rolls over $N.",
    "Your eyelids grow heavy and the world slips away."
  },
  [SPELL_STRENGTH] = {
    "Power pours into your limbs like heat.",
    "$n\u2019s posture steadies with sudden might.",
    NULL
  },
  [SPELL_SUMMON] = {
    "You pull on a name and space gives way.",
    "The air twists as $n calls someone through.",
    NULL
  },
  [SPELL_VENTRILOQUATE] = {
    "You throw your voice like a hidden knife.",
    "A voice speaks from the wrong place.",
    NULL
  },
  [SPELL_WORD_OF_RECALL] = {
    "You speak the word that leads you home.",
    "$n fades away on a whispered word.",
    NULL
  },
  [SPELL_REMOVE_POISON] = {
    "You draw the toxin out and cast it aside.",
    "A sickly tinge drains from $N.",
    "The poison\u2019s grip loosens and fades."
  },
  [SPELL_SENSE_LIFE] = {
    "You feel the pulse of living things nearby.",
    "$n breathes in slowly, sensing the unseen.",
    NULL
  },
  [SPELL_ANIMATE_DEAD] = {
    "You call to the stillness and it answers.",
    "A grave chill rises as $n stirs the dead.",
    NULL
  },
  [SPELL_DISPEL_GOOD] = {
    "You tear at holy light with spiteful force.",
    "Darkness flares from $n toward $N.",
    "Something cold snuffs at your virtue."
  },
  [SPELL_GROUP_ARMOR] = {
    "Your ward spreads outward to your allies.",
    "A protective sheen settles over $n\u2019s group.",
    NULL
  },
  [SPELL_GROUP_HEAL] = {
    "You release a wave of restoring light.",
    "Warm radiance washes over $n\u2019s group.",
    NULL
  },
  [SPELL_GROUP_RECALL] = {
    "You call your allies back by shared bond.",
    "The air pulls tight as $n\u2019s group vanishes.",
    NULL
  },
  [SPELL_INFRAVISION] = {
    "Heat and shadow sharpen into clear sight.",
    "$n\u2019s eyes take on a faint, eerie glow.",
    NULL
  },
  [SPELL_WATERWALK] = {
    "Your feet grow light as if the world forgives weight.",
    "$n\u2019s steps seem strangely certain.",
    NULL
  },
  [SPELL_IDENTIFY] = {
    "You trace the thing\u2019s story with your mind.",
    "$n studies $p with intense focus.",
    NULL
  },
  [SPELL_FLY] = {
    "Air gathers beneath you and lifts.",
    "$n rises as if carried by unseen hands.",
    NULL
  },
  [SPELL_DARKNESS] = {
    "You snuff the light with a cold gesture.",
    "Shadows thicken around $n, swallowing the room.",
    NULL
  },
  [SPELL_FIREBOLT] = {
    "You hurl a bolt of fire at $N!",
    "$n hurls a bolt of fire at $N!",
    "A bolt of fire slams into you!"
  },
  [SPELL_FLAME_ARROW] = {
    "You conjure a blazing arrow of fire and hurl it at $N!",
    "$n hurls a blazing arrow of fire at $N!",
    "A blazing arrow of fire pierces you and leaves you burning!"
  },
  [SPELL_FROSTBITE] = {
    "You drain heat from $N's body in a flash of killing frost!",
    "$n freezes the moisture around $N with killing frost!",
    "Extreme cold bites into you, numbing your limbs!"
  },
  [SPELL_VOLTAIC_BOLT] = {
    "A crackling bolt of electricity arcs from your fingers at $N!",
    "$n fires a crackling bolt of electricity at $N!",
    "A voltaic bolt slams into you, leaving you charged with static!"
  },
  [SPELL_ACID_BLAST] = {
    "You unleash a blast of corrosive acid at $N!",
    "$n launches a sizzling blast of acid at $N!",
    "Searing acid eats through your armor and flesh!"
  },
  [SPELL_SHADOW_BOLT] = {
    "You hurl a bolt of pure shadow energy at $N!",
    "$n hurls a bolt of shadow at $N!",
    "A bolt of shadow crashes into you!"
  },
  [SPELL_VAMPIRIC_TOUCH] = {
    "You plunge necrotic energy into $N, draining life from the wound!",
    "$n drains life from $N with necrotic power!",
    "$n tears at your vitality with necrotic power!"
  },
  [SPELL_WEB] = {
    "You hurl a mass of sticky webs at $N!",
    "Sticky webs shoot from $n's hands and wrap around $N!",
    "Thick strands of webbing wrap around you!"
  },
  [SPELL_SILENCE] = {
    "You weave silence around $N's throat!",
    "$n seals $N's voice with a gesture!",
    "Your voice is stolen away by magical silence!"
  },
  [SPELL_FEAR] = {
    "You project an aura of supernatural dread at $N!",
    "$n unleashes supernatural fear at $N!",
    "Terror seizes your heart as magical dread overwhelms you!"
  },
  [SPELL_TRUE_SEEING] = {
    "Your vision sharpens to supernatural clarity.",
    "$n's eyes glow with supernatural clarity.",
    "A shimmer of arcane light washes over your eyes."
  },
  [SPELL_STONE_SKIN] = {
    "Your skin hardens to the consistency of granite!",
    "$n's skin hardens to a stony texture!",
    "$n's skin takes on a rough, stony texture."
  },
  [SPELL_BARKSKIN] = {
    "Your skin hardens into rough, bark-like armor!",
    "$n's skin hardens to a bark-like texture!",
    "$n's skin hardens to a bark-like texture."
  },
  [SPELL_GIANT_STRENGTH] = {
    "Your muscles swell with giant-like strength!",
    "$n swells with magical strength!",
    "$n's muscles swell with magical strength!"
  },
  [SPELL_ADRENALINE_SURGE] = {
    "Adrenaline surges through you!",
    "$n surges with adrenaline!",
    NULL
  },
  [SPELL_CLARITY] = {
    "Your mind sharpens to perfect clarity!",
    "$n's eyes clear with magical clarity!",
    "$n's eyes clear as magical clarity takes hold."
  },
  [SPELL_MARK_OF_DEATH] = {
    "You brand $N with the Mark of Death!",
    "$n brands $N with a burning sigil!",
    "A burning sigil sears itself into your flesh!"
  },
  [SPELL_BLOODLUST] = {
    "Bloodlust seizes you as your body burns for violence!",
    "$n's eyes go red with bloodlust!",
    NULL
  },
  [SPELL_GREATER_HEAL] = {
    "You channel powerful healing into $N!",
    "$n channels powerful healing into $N!",
    "Waves of powerful healing wash over you!"
  },
  [SPELL_CLEANSE] = {
    "Holy light cleanses all afflictions from $N!",
    "$n calls holy light to cleanse $N of all afflictions!",
    "Holy light washes away all your afflictions!"
  },
  [SPELL_DISRUPT] = {
    "You slam a bolt of disruptive force into $N!",
    "$n blasts $N with a crackling bolt of disruption!",
    "A jarring force rattles your concentration!"
  },
  [SPELL_ANTIMAGIC_SHELL] = {
    "You weave a shell of anti-magical energy around yourself!",
    "$n disappears momentarily inside a flickering shell of energy!",
    "A shell of anti-magical energy surrounds you."
  },
  [SPELL_ENCHANTERS_FOCUS] = {
    "You draw ambient arcane energy into perfect focus.",
    "Arcane light coalesces around $n in a focused halo.",
    NULL
  },
  [SPELL_TIME_SNARE] = {
    "You snare $N in a fold of slowed time!",
    "$n traces a rune in the air and $N seems to move sluggishly!",
    "Time thickens around you, slowing your every action!"
  },
  [SPELL_PHASE_SHIFT] = {
    "You phase partially out of the material plane!",
    "$n's form shimmers and becomes ghostly!",
    NULL
  },
  [SPELL_MIRROR_VEIL] = {
    "Shimmering mirror images of yourself surround you!",
    "$n summons a veil of mirror images!",
    NULL
  },
  [SPELL_ELEMENTAL_WARD_FIRE] = {
    "A ward of fire resistance wraps around you.",
    "$n glows briefly with a warm orange light.",
    NULL
  },
  [SPELL_ELEMENTAL_WARD_COLD] = {
    "A ward of cold resistance wraps around you.",
    "$n glows briefly with a pale blue light.",
    NULL
  },
  [SPELL_ELEMENTAL_WARD_LIGHTNING] = {
    "A ward of lightning resistance wraps around you.",
    "$n crackles with a brief mantle of electric light.",
    NULL
  },
  [SPELL_ELEMENTAL_WARD_ACID] = {
    "A ward of acid resistance wraps around you.",
    "$n is briefly coated in a slick green sheen.",
    NULL
  },
  [SPELL_COUNTERSPELL] = {
    "You reach into the weave and unravel $N's spell!",
    "$n disrupts $N's spellcasting with a counterspell!",
    "Your spell is torn apart by $n's counterspell!"
  },
  [SPELL_SPELL_STEAL] = {
    "You steal magical energy from $N and draw it into yourself!",
    "$n steals a magical effect from $N!",
    "You feel one of your magical protections ripped away!"
  },
  [SPELL_CANCELLATION] = {
    "You channel raw anti-magic into $N, unraveling enchantments!",
    "$n gestures sharply and $N's magical aura crackles and dims!",
    "You feel your magical protections unraveling!"
  },
  [SPELL_HOLD_PERSON] = {
    "You command $N to be still, and they obey!",
    "$n holds $N motionless with a command!",
    "You are held motionless by magical compulsion!"
  },
  [SPELL_HOLD_MONSTER] = {
    "Your powerful hold magic freezes $N in place!",
    "$n freezes $N solid with hold magic!",
    "Powerful magic holds your body immobile!"
  },
  [SPELL_CONFUSION] = {
    "Confusion floods $N's mind!",
    "$n clouds $N's mind with confusion!",
    "Confusion floods your mind and scrambles your thoughts!"
  },
  [SPELL_VERTIGO] = {
    "You induce violent vertigo in $N!",
    "$n causes $N to stagger with vertigo!",
    "The world spins violently as vertigo seizes you!"
  },
  [SPELL_MASS_FEAR] = {
    "You project waves of supernatural terror at all your enemies!",
    "$n projects waves of supernatural terror!",
    "Terror overwhelms you!"
  },
  [SPELL_NULL_FIELD] = {
    "You create a null field that suppresses all magic in the area!",
    "$n creates a null field that suppresses all magic in the area!",
    NULL
  },
  [SPELL_SILENCE_FIELD] = {
    "A field of magical silence falls over the entire area!",
    "$n silences the entire area with a wave of a hand!",
    NULL
  },
  [SPELL_MIASMA] = {
    "You exhale a billowing cloud of toxic miasma!",
    "$n exhales a billowing cloud of toxic miasma!",
    NULL
  },
  [SPELL_TOXIC_CLOUD] = {
    "You summon a billowing cloud of toxic gas!",
    "$n summons a billowing cloud of toxic gas!",
    NULL
  },
  [SPELL_WALL_OF_FIRE] = {
    "You conjure a wall of magical fire!",
    "$n conjures a wall of magical fire!",
    NULL
  },
  [SPELL_STATIC_FIELD] = {
    "You charge the air with crackling static electricity!",
    "$n charges the air with crackling static electricity!",
    NULL
  },
  [SPELL_CONSECRATE] = {
    "You consecrate this ground in the name of your deity!",
    "$n consecrates this ground in the name of $s deity!",
    NULL
  },
  [SPELL_GRAVITY_WELL] = {
    "You create a crushing gravity well that pins your enemies!",
    "$n creates a crushing gravity well that pins enemies in place!",
    NULL
  },
  [SPELL_SHOCKWAVE] = {
    "A shockwave of force erupts from you!",
    "A shockwave of force erupts from $n!",
    NULL
  },
  [SPELL_NOVA] = {
    "You explode with a blinding nova of pure magical energy!",
    "$n explodes with a blinding nova of pure magical energy!",
    NULL
  },
  [SPELL_EARTHQUAKE] = {
    "You call a violent earthquake that shakes the ground!",
    "$n calls a violent earthquake that shakes the ground!",
    NULL
  },
  [SPELL_ICE_STORM] = {
    "You call a violent storm of razor ice into the room!",
    "$n calls a violent storm of razor ice into the room!",
    NULL
  },
  [SPELL_BLIZZARD] = {
    "You unleash a full blizzard upon your enemies!",
    "$n unleashes a full blizzard upon $s enemies!",
    NULL
  },
  [SPELL_FROST_NOVA] = {
    "Frost explodes outward from you in a shattering nova!",
    "Frost explodes outward from $n in a shattering nova!",
    NULL
  },
  [SPELL_FIREBALL_GREATER] = {
    "A massive fireball erupts from your hands, filling the room with fire!",
    "A massive fireball erupts from $n's hands, filling the room with fire!",
    NULL
  },
  [SPELL_ACID_RAIN] = {
    "You call a rain of burning acid from above!",
    "$n calls a rain of burning acid from above!",
    NULL
  },
  [SPELL_SONIC_BURST] = {
    "A sonic burst explodes outward from you!",
    "A sonic burst explodes outward from $n!",
    NULL
  },
  [SPELL_WORD_OF_PAIN] = {
    "You speak the Word of Pain into existence!",
    "$n speaks the Word of Pain into existence!",
    NULL
  },
  [SPELL_CONJURE_ELEMENTAL] = {
    "You tear open a rift to the elemental planes!",
    "$n tears open a rift to the elemental planes!",
    NULL
  },
  [SPELL_CALL_WOLVES] = {
    "You call wolves to your side!",
    "$n calls wolves to $s side!",
    NULL
  },
  [SPELL_CALL_BEARS] = {
    "You call a bear to fight for you!",
    "$n calls a bear to fight!",
    NULL
  },
  [SPELL_ANIMATE_DEAD_GREATER] = {
    "Dark power flows from you, raising the dead!",
    "Dark power flows from $n, raising the dead!",
    NULL
  },
  [SPELL_ABYSS_GATE] = {
    "You tear open a gate to the Abyss!",
    "$n tears open a gate to the Abyss!",
    NULL
  },
  [SPELL_GATE] = {
    "You tear open a gate to summon a mighty entity!",
    "$n tears open a gate to summon a mighty entity!",
    NULL
  },
  [SPELL_PORTAL] = {
    "A shimmering portal tears open in the air here!",
    "A shimmering portal tears open in the air here!",
    NULL
  },
  [SPELL_WORD_OF_RECALL_MASS] = {
    "You call your group home with mass recall!",
    "$n calls $s group home with mass recall!",
    NULL
  },
  [SPELL_ASTRAL_PROJECTION] = {
    "Your astral form separates from your body and flies toward $N!",
    "$n's astral form separates and flies toward $N!",
    NULL
  },
  [SPELL_ETHEREAL_JAUNT] = {
    "You jaunt through the ethereal plane and reappear elsewhere!",
    "$n jaunts through the ethereal plane and vanishes!",
    NULL
  },
  [SPELL_LEYLINE_TAP] = {
    "You tap into a leyline of raw arcane energy, restoring your mana!",
    "$n draws in a surge of raw arcane leyline energy!",
    NULL
  },
  [SPELL_TEMPORAL_SHIFT] = {
    "You shift $N's place in time!",
    "$n shifts $N's place in time!",
    "Time stutters around you for a heartbeat!"
  },
  [SPELL_CHRONO_SHIFT] = {
    "You shift yourself back in time one round, undoing the damage!",
    "$n flickers as time rewinds around $m!",
    NULL
  }
};

static bool send_cast_message(struct char_data *ch, struct char_data *tch, struct obj_data *tobj, int spellnum)
{
  const struct cast_message *msg;

  if (spellnum < 0 || spellnum >= (int) (sizeof(cast_messages) / sizeof(cast_messages[0])))
    return FALSE;

  msg = &cast_messages[spellnum];

  if (!msg->to_caster && !msg->to_room && !msg->to_target)
    return FALSE;

  if (msg->to_caster)
    act(msg->to_caster, FALSE, ch, tobj, tch, TO_CHAR);

  if (msg->to_room) {
    int audience = (tch && msg->to_target) ? TO_NOTVICT : TO_ROOM;
    act(msg->to_room, TRUE, ch, tobj, tch, audience);
  }

  if (tch && msg->to_target)
    act(msg->to_target, FALSE, ch, tobj, tch, TO_VICT);

  return TRUE;
}

/* Local (File Scope) Variables */
struct syllable {
  const char *org;
  const char *news;
};
static struct syllable syls[] = { { " ", " " }, { "ar", "abra" },
    { "ate", "i" }, { "cau", "kada" }, { "blind", "nose" }, { "bur", "mosa" }, {
        "cu", "judi" }, { "de", "oculo" }, { "dis", "mar" },
    { "ect", "kamina" }, { "en", "uns" }, { "gro", "cra" }, { "light", "dies" },
    { "lo", "hi" }, { "magi", "kari" }, { "mon", "bar" }, { "mor", "zak" }, {
        "move", "sido" }, { "ness", "lacri" }, { "ning", "illa" }, { "per",
        "duda" }, { "ra", "gru" }, { "re", "candus" }, { "son", "sabru" }, {
        "tect", "infra" }, { "tri", "cula" }, { "ven", "nofo" }, { "word of",
        "inset" }, { "a", "i" }, { "b", "v" }, { "c", "q" }, { "d", "m" }, {
        "e", "o" }, { "f", "y" }, { "g", "t" }, { "h", "p" }, { "i", "u" }, {
        "j", "y" }, { "k", "t" }, { "l", "r" }, { "m", "w" }, { "n", "b" }, {
        "o", "a" }, { "p", "s" }, { "q", "d" }, { "r", "f" }, { "s", "g" }, {
        "t", "h" }, { "u", "e" }, { "v", "z" }, { "w", "x" }, { "x", "n" }, {
        "y", "l" }, { "z", "k" }, { "", "" } };

static int mag_manacost(struct char_data *ch, int spellnum) {
  int mana = MAX(SINFO.mana_max - (SINFO.mana_change *
      (GET_LEVEL(ch) - SINFO.min_level[(int) GET_CLASS(ch)])),
  SINFO.mana_min);

  if (ch && AFF_FLAGGED(ch, AFF_EMPOWERED))
    mana = MAX(1, (mana * 9) / 10);

  return mana;
}

static char *obfuscate_spell(const char *unobfuscated) {
  static char obfuscated[200];
  int maxlen = 200;

  int j, ofs = 0;

  *obfuscated = '\0';

  while (unobfuscated[ofs]) {
    for (j = 0; *(syls[j].org); j++) {
      if (!strncmp(syls[j].org, unobfuscated + ofs, strlen(syls[j].org))) {
        if (strlen(syls[j].news) < maxlen) {
          strncat(obfuscated, syls[j].news, maxlen);
          maxlen -= strlen(syls[j].news);
        } else {
          log("No room in obfuscated version of '%s' (currently obfuscated to '%s') to add syllable '%s'.",
              unobfuscated, obfuscated, syls[j].news);
        }
        ofs += strlen(syls[j].org);
        break;
      }
    }
    /* i.e., we didn't find a match in syls[] */
    if (!*syls[j].org) {
      log("No entry in syllable table for substring of '%s' starting at '%s'.", unobfuscated, unobfuscated + ofs);
      ofs++;
    }
  }
  return obfuscated;
}

static void say_spell(struct char_data *ch, int spellnum, struct char_data *tch,
    struct obj_data *tobj) {
  const char *format, *spell = skill_name(spellnum);
  char act_buf_original[256], act_buf_obfuscated[256], *obfuscated = obfuscate_spell(spell);


  struct char_data *i;

  if (tch != NULL && IN_ROOM(tch) == IN_ROOM(ch)) {
    if (tch == ch)
      format = "$n closes $s eyes and utters the words, '%s'.";
    else
      format = "$n stares at $N and utters the words, '%s'.";
  } else if (tobj != NULL
      && ((IN_ROOM(tobj) == IN_ROOM(ch)) || (tobj->carried_by == ch)))
    format = "$n stares at $p and utters the words, '%s'.";
  else
    format = "$n utters the words, '%s'.";

  snprintf(act_buf_original, sizeof(act_buf_original), format, spell);
  snprintf(act_buf_obfuscated, sizeof(act_buf_obfuscated), format, obfuscated);

  for (i = world[IN_ROOM(ch)].people; i; i = i->next_in_room) {
    if (i == ch || i == tch || !i->desc || !AWAKE(i))
      continue;
    if (GET_CLASS(ch) == GET_CLASS(i))
      perform_act(act_buf_original, ch, tobj, tch, i);
    else
      perform_act(act_buf_obfuscated, ch, tobj, tch, i);
  }

  if (tch != NULL && tch != ch && IN_ROOM(tch) == IN_ROOM(ch)) {
    snprintf(act_buf_original, sizeof(act_buf_original), "$n stares at you and utters the words, '%s'.",
    GET_CLASS(ch) == GET_CLASS(tch) ? spell : obfuscated);
    act(act_buf_original, FALSE, ch, NULL, tch, TO_VICT);
  }
}

/* This function should be used anytime you are not 100% sure that you have
 * a valid spell/skill number.  A typical for() loop would not need to use
 * this because you can guarantee > 0 and <= TOP_SPELL_DEFINE. */
const char *skill_name(int num) {
  if (num > 0 && num <= TOP_SPELL_DEFINE)
    return (spell_info[num].name);
  else if (num == -1)
    return ("UNUSED");
  else
    return ("UNDEFINED");
}

static bool is_available_spell(int spellnum) {
  return (spellnum > 0 && spellnum <= MAX_SPELLS && spell_info[spellnum].name
      && str_cmp(spell_info[spellnum].name, unused_spellname) != 0);
}

static bool is_available_ability(int ability) {
  return (ability > 0 && ability <= TOP_SPELL_DEFINE && spell_info[ability].name
      && str_cmp(spell_info[ability].name, unused_spellname) != 0);
}

static bool is_spellup_beneficial_spell(int spellnum)
{
  if (!is_available_spell(spellnum))
    return FALSE;

  /* Hard safety gate for spellup: only explicitly approved support buffs. */
  switch (spellnum) {
    case SPELL_ARMOR:
    case SPELL_BLESS:
    case SPELL_DETECT_ALIGN:
    case SPELL_DETECT_INVIS:
    case SPELL_DETECT_MAGIC:
    case SPELL_FLY:
    case SPELL_INFRAVISION:
    case SPELL_INVISIBLE:
    case SPELL_PROT_FROM_EVIL:
    case SPELL_SANCTUARY:
    case SPELL_ARCANE_WARD:
    case SPELL_EVASION:
    case SPELL_IRONSKIN:
    case SPELL_DIVINE_BULWARK:
    case SPELL_SONG_OF_RESILIENCE:
    case SPELL_DARK_AEGIS:
    case SPELL_NIRVANA:
    case SPELL_SENSE_LIFE:
    case SPELL_STRENGTH:
    case SPELL_BEAR_SPIRIT:
    case SPELL_WOLF_SPIRIT:
    case SPELL_TIGER_SPIRIT:
    case SPELL_EAGLE_SPIRIT:
    case SPELL_DRAGON_SPIRIT:
    case SPELL_WATERWALK:
      break;
    default:
      return FALSE;
  }

  /* Defensive invariants: even whitelisted spells must be non-violent affects. */
  if (SINFO.violent || !IS_SET(SINFO.routines, MAG_AFFECTS))
    return FALSE;
  if (!IS_SET(SINFO.targets, TAR_CHAR_ROOM | TAR_CHAR_WORLD | TAR_SELF_ONLY))
    return FALSE;
  if (IS_SET(SINFO.targets, TAR_IGNORE))
    return FALSE;

  return TRUE;
}

static void normalize_ability_input(const char *input, char *output,
    size_t output_len) {
  const char *start = input;
  char quote;
  size_t len;

  if (!input || output_len == 0) {
    if (output_len > 0)
      *output = '\0';
    return;
  }

  while (*start && isspace((unsigned char)*start))
    start++;

  quote = *start;
  if (quote == '\'' || quote == '"') {
    const char *end = strchr(start + 1, quote);
    start++;
    len = end ? (size_t)(end - start) : strlen(start);
    if (len >= output_len)
      len = output_len - 1;
    memcpy(output, start, len);
    output[len] = '\0';
  } else {
    strlcpy(output, start, output_len);
  }

  len = strlen(output);
  while (len > 0 && isspace((unsigned char)output[len - 1])) {
    output[len - 1] = '\0';
    len--;
  }
}

static void append_match(char *buffer, size_t buf_size, const char *name,
    int *count) {
  size_t offset = strlen(buffer);

  if (*count > 0 && offset + 1 < buf_size) {
    strlcpy(buffer + offset, ", ", buf_size - offset);
    offset = strlen(buffer);
  }

  if (offset < buf_size)
    strlcpy(buffer + offset, name, buf_size - offset);
  (*count)++;
}

static bool ability_matches_input(const char *input, const char *ability_name,
    bool allow_partial_name, bool allow_extra_input, int *name_tokens,
    int *input_tokens) {
  char input_buf[MAX_INPUT_LENGTH];
  char name_buf[MAX_INPUT_LENGTH];
  char input_token[MAX_INPUT_LENGTH];
  char name_token[MAX_INPUT_LENGTH];
  char *input_ptr = input_buf;
  char *name_ptr = name_buf;
  int matched_name_tokens = 0;
  int matched_input_tokens = 0;

  if (name_tokens)
    *name_tokens = 0;
  if (input_tokens)
    *input_tokens = 0;

  if (!input || !*input || !ability_name || !*ability_name)
    return FALSE;

  strlcpy(input_buf, input, sizeof(input_buf));
  strlcpy(name_buf, ability_name, sizeof(name_buf));

  input_ptr = any_one_arg(input_ptr, input_token);
  name_ptr = any_one_arg(name_ptr, name_token);

  while (*input_token && *name_token) {
    if (!is_abbrev(input_token, name_token))
      return FALSE;
    matched_name_tokens++;
    matched_input_tokens++;
    input_ptr = any_one_arg(input_ptr, input_token);
    name_ptr = any_one_arg(name_ptr, name_token);
  }

  if (!*input_token && *name_token) {
    if (!allow_partial_name)
      return FALSE;
    while (*name_token) {
      matched_name_tokens++;
      name_ptr = any_one_arg(name_ptr, name_token);
    }
  } else if (*input_token && !*name_token) {
    if (!allow_extra_input)
      return FALSE;
  }

  if (name_tokens)
    *name_tokens = matched_name_tokens;
  if (input_tokens)
    *input_tokens = matched_input_tokens;

  return TRUE;
}

static int find_spell_by_tokens(const char *name, char *ambig_buf,
    size_t ambig_len, int *matched_tokens, bool allow_partial_name,
    bool allow_extra_input) {
  /* Adventurers Lair compatibility aliases:
   * Keep these lightweight so legacy/variant ability names can reuse stable
   * implementations without duplicating spell logic. */
  static const struct {
    const char *alias;
    int spellnum;
  } spell_aliases[] = {
    { "colour spray", SPELL_COLOR_SPRAY },
    { "detect invis", SPELL_DETECT_INVIS },
    { "invis", SPELL_INVISIBLE },
    { "night vision", SPELL_INFRAVISION },
    { "shield", SPELL_ARMOR },
    { "underwater breathing", SPELL_WATERWALK },
    { NULL, 0 }
  };
  int best_spell = -1;
  int best_tokens = 0;
  int best_input_tokens = 0;
  int match_count = 0;
  int spellnum;

  if (matched_tokens)
    *matched_tokens = 0;

  *ambig_buf = '\0';

  for (spellnum = 0; spell_aliases[spellnum].alias; spellnum++) {
    if (ability_matches_input(name, spell_aliases[spellnum].alias,
        allow_partial_name, allow_extra_input, NULL, NULL)) {
      if (matched_tokens)
        *matched_tokens = 1;
      return spell_aliases[spellnum].spellnum;
    }
  }

  for (spellnum = 1; spellnum <= MAX_SPELLS; spellnum++) {
    int token_count = 0;
    int input_token_count = 0;

    if (!is_available_spell(spellnum))
      continue;

    if (!ability_matches_input(name, spell_info[spellnum].name,
        allow_partial_name, allow_extra_input, &token_count,
        &input_token_count))
      continue;

    if (token_count > best_tokens) {
      best_tokens = token_count;
      best_input_tokens = input_token_count;
      best_spell = spellnum;
      match_count = 0;
      *ambig_buf = '\0';
      append_match(ambig_buf, ambig_len, spell_info[spellnum].name,
          &match_count);
    } else if (token_count == best_tokens) {
      append_match(ambig_buf, ambig_len, spell_info[spellnum].name,
          &match_count);
    }
  }

  if (matched_tokens)
    *matched_tokens = best_input_tokens;

  if (match_count == 1)
    return best_spell;
  if (match_count > 1)
    return -2;

  return -1;
}

static int find_spell_by_prefix(const char *name, char *ambig_buf,
    size_t ambig_len) {
  return find_spell_by_tokens(name, ambig_buf, ambig_len, NULL, TRUE, FALSE);
}

static struct char_data *find_char_prefix(struct char_data *ch,
    const char *name, int number, bool include_fighting, char *ambig_buf,
    size_t ambig_len) {
  struct char_data *i, *vict = NULL;
  int count = 0;

  *ambig_buf = '\0';

  if (number < 1)
    number = 1;

  if (include_fighting && FIGHTING(ch) && CAN_SEE(ch, FIGHTING(ch))
      && is_abbrev(name, GET_NAME(FIGHTING(ch)))) {
    append_match(ambig_buf, ambig_len, GET_NAME(FIGHTING(ch)), &count);
    if (count == number)
      vict = FIGHTING(ch);
  }

  for (i = world[IN_ROOM(ch)].people; i; i = i->next_in_room) {
    if (!CAN_SEE(ch, i))
      continue;
    if (include_fighting && i == FIGHTING(ch))
      continue;
    if (is_abbrev(name, GET_NAME(i))) {
      append_match(ambig_buf, ambig_len, GET_NAME(i), &count);
      if (count == number)
        vict = i;
    }
  }

  if (vict && (number > 1 || count == 1))
    return vict;

  if (count > 1)
    return NULL;

  *ambig_buf = '\0';
  return NULL;
}

static int find_ability_by_tokens(const char *name, char *ambig_buf,
    size_t ambig_len, int *matched_tokens, bool allow_partial_name,
    bool allow_extra_input) {
  static const struct {
    const char *alias;
    int ability;
  } ability_aliases[] = {
    { "colour spray", SPELL_COLOR_SPRAY },
    { "detect invis", SPELL_DETECT_INVIS },
    { "hunt", SKILL_TRACK },
    { "invis", SPELL_INVISIBLE },
    { "night vision", SPELL_INFRAVISION },
    { "shield", SPELL_ARMOR },
    { "underwater breathing", SPELL_WATERWALK },
    { NULL, 0 }
  };
  int best_ability = -1;
  int best_tokens = 0;
  int best_input_tokens = 0;
  int match_count = 0;
  int ability;

  if (matched_tokens)
    *matched_tokens = 0;

  *ambig_buf = '\0';

  for (ability = 0; ability_aliases[ability].alias; ability++) {
    if (ability_matches_input(name, ability_aliases[ability].alias,
        allow_partial_name, allow_extra_input, NULL, NULL)) {
      if (matched_tokens)
        *matched_tokens = 1;
      return ability_aliases[ability].ability;
    }
  }

  for (ability = 1; ability <= TOP_SPELL_DEFINE; ability++) {
    int token_count = 0;
    int input_token_count = 0;

    if (!is_available_ability(ability))
      continue;

    if (!ability_matches_input(name, spell_info[ability].name,
        allow_partial_name, allow_extra_input, &token_count,
        &input_token_count))
      continue;

    if (token_count > best_tokens) {
      best_tokens = token_count;
      best_input_tokens = input_token_count;
      best_ability = ability;
      match_count = 0;
      *ambig_buf = '\0';
      append_match(ambig_buf, ambig_len, spell_info[ability].name,
          &match_count);
    } else if (token_count == best_tokens) {
      append_match(ambig_buf, ambig_len, spell_info[ability].name,
          &match_count);
    }
  }

  if (matched_tokens)
    *matched_tokens = best_input_tokens;

  if (match_count == 1)
    return best_ability;
  if (match_count > 1)
    return -2;

  return -1;
}

int find_skill_num(char *name) {
  char cleaned[MAX_INPUT_LENGTH];
  int skill_num;
  static const struct {
    const char *alias;
    int ability;
  } direct_aliases[] = {
    { "colour spray", SPELL_COLOR_SPRAY },
    { "detect invis", SPELL_DETECT_INVIS },
    { "hunt", SKILL_TRACK },
    { "invis", SPELL_INVISIBLE },
    { "night vision", SPELL_INFRAVISION },
    { "shield", SPELL_ARMOR },
    { "underwater breathing", SPELL_WATERWALK },
    { NULL, 0 }
  };

  normalize_ability_input(name, cleaned, sizeof(cleaned));
  if (!*cleaned)
    return (-1);

  for (skill_num = 0; direct_aliases[skill_num].alias; skill_num++)
    if (ability_matches_input(cleaned, direct_aliases[skill_num].alias,
        TRUE, FALSE, NULL, NULL))
      return direct_aliases[skill_num].ability;

  for (skill_num = 1; skill_num <= TOP_SPELL_DEFINE; skill_num++) {
    if (!is_available_ability(skill_num))
      continue;
    if (ability_matches_input(cleaned, spell_info[skill_num].name,
        TRUE, FALSE, NULL, NULL))
      return skill_num;
  }

  return (-1);
}

int find_skill_num_with_ambig(const char *name, char *ambig_buf,
    size_t ambig_len) {
  char cleaned[MAX_INPUT_LENGTH];

  normalize_ability_input(name, cleaned, sizeof(cleaned));
  if (!*cleaned) {
    if (ambig_buf)
      *ambig_buf = '\0';
    return (-1);
  }

  return find_ability_by_tokens(cleaned, ambig_buf, ambig_len, NULL,
      TRUE, FALSE);
}

/* This function is the very heart of the entire magic system.  All invocations
 * of all types of magic -- objects, spoken and unspoken PC and NPC spells, the
 * works -- all come through this function eventually. This is also the entry
 * point for non-spoken or unrestricted spells. Spellnum 0 is legal but silently
 * ignored here, to make callers simpler. */
int call_magic(struct char_data *caster, struct char_data *cvict,
    struct obj_data *ovict, int spellnum, int level, int casttype) {
  int savetype;

  if (spellnum < 1 || spellnum > TOP_SPELL_DEFINE)
    return (0);

  if (!cast_wtrigger(caster, cvict, ovict, spellnum))
    return 0;
  if (!cast_otrigger(caster, ovict, spellnum))
    return 0;
  if (!cast_mtrigger(caster, cvict, spellnum))
    return 0;

  if (ROOM_FLAGGED(IN_ROOM(caster), ROOM_NOMAGIC) && spellnum != SPELL_IDENTIFY) {
    send_to_char(caster, "Your magic fizzles out and dies.\r\n");
    act("$n's magic fizzles out and dies.", FALSE, caster, 0, 0, TO_ROOM);
    return (0);
  }
  if (room_has_effect(&world[IN_ROOM(caster)], ROOM_EFFECT_NULL_FIELD) &&
      spellnum != SPELL_NULL_FIELD) {
    send_to_char(caster, "The null field suppresses your spell completely!\r\n");
    act("$n's spell collapses against the null field.", FALSE, caster, 0, 0, TO_ROOM);
    return (0);
  }
  if (room_has_effect(&world[IN_ROOM(caster)], ROOM_EFFECT_SILENCE_FIELD) &&
      spellnum != SPELL_SILENCE_FIELD) {
    send_to_char(caster, "The silence field swallows your incantation.\r\n");
    act("$n mouths arcane words, but the silence field devours them.", FALSE, caster, 0, 0, TO_ROOM);
    return (0);
  }
  if (ROOM_FLAGGED(IN_ROOM(caster), ROOM_PEACEFUL) && (SINFO.violent || IS_SET(SINFO.routines, MAG_DAMAGE))) {
    send_to_char(caster, "A flash of white light fills the room, dispelling your violent magic!\r\n");
    act("White light from no particular source suddenly fills the room, then vanishes.", FALSE, caster, 0, 0, TO_ROOM);
    return (0);
  }
  if (cvict && MOB_FLAGGED(cvict, MOB_NOKILL)) {
    send_to_char(caster, "This mob is protected.\r\n");
    return (0);
  }

  if (!reflect_suppressed &&
      cvict && cvict != caster &&
      SINFO.violent &&
      (IS_SET(SINFO.targets, TAR_CHAR_ROOM) || IS_SET(SINFO.targets, TAR_CHAR_WORLD) ||
       IS_SET(SINFO.targets, TAR_FIGHT_VICT) || IS_SET(SINFO.targets, TAR_FIGHT_SELF)) &&
      AFF_FLAGGED(cvict, AFF_SPELL_REFLECT) &&
      rand_number(1, 100) <= 20) {
    act("A reflective ward around $N redirects your spell!", FALSE, caster, 0, cvict, TO_CHAR);
    act("Your reflective ward flares and redirects $n's spell!", FALSE, caster, 0, cvict, TO_VICT);
    act("$N's reflective ward flares and redirects $n's spell!", FALSE, caster, 0, cvict, TO_NOTVICT);
    reflect_suppressed++;
    call_magic(cvict, caster, NULL, spellnum, level, casttype);
    reflect_suppressed--;
    return 0;
  }
  /* determine the type of saving throw */
  switch (casttype) {
  case CAST_STAFF:
  case CAST_SCROLL:
  case CAST_POTION:
  case CAST_WAND:
    savetype = SAVING_ROD;
    break;
  case CAST_SPELL:
    savetype = SAVING_SPELL;
    break;
  default:
    savetype = SAVING_BREATH;
    break;
  }

  if (IS_SET(SINFO.routines, MAG_DAMAGE))
    if (mag_damage(level, caster, cvict, spellnum, savetype) == -1)
      return (-1); /* Successful and target died, don't cast again. */

  if (IS_SET(SINFO.routines, MAG_AFFECTS))
    mag_affects(level, caster, cvict, spellnum, savetype);

  if (IS_SET(SINFO.routines, MAG_UNAFFECTS))
    mag_unaffects(level, caster, cvict, spellnum, savetype);

  if (IS_SET(SINFO.routines, MAG_POINTS))
    mag_points(level, caster, cvict, spellnum, savetype);

  if (IS_SET(SINFO.routines, MAG_ALTER_OBJS))
    mag_alter_objs(level, caster, ovict, spellnum, savetype);

  if (IS_SET(SINFO.routines, MAG_GROUPS))
    mag_groups(level, caster, spellnum, savetype);

  if (IS_SET(SINFO.routines, MAG_MASSES))
    mag_masses(level, caster, spellnum, savetype);

  if (IS_SET(SINFO.routines, MAG_AREAS))
    mag_areas(level, caster, spellnum, savetype);

  if (IS_SET(SINFO.routines, MAG_SUMMONS))
    mag_summons(level, caster, ovict, spellnum, savetype);

  if (IS_SET(SINFO.routines, MAG_CREATIONS))
    mag_creations(level, caster, spellnum);

  if (IS_SET(SINFO.routines, MAG_ROOMS))
    mag_rooms(level, caster, spellnum);

  if (IS_SET(SINFO.routines, MAG_MANUAL))
    switch (spellnum) {
    case SPELL_CHARM:
      MANUAL_SPELL(spell_charm)
      ;
      break;
    case SPELL_CREATE_WATER:
      MANUAL_SPELL(spell_create_water)
      ;
      break;
    case SPELL_DETECT_POISON:
      MANUAL_SPELL(spell_detect_poison)
      ;
      break;
    case SPELL_ENCHANT_WEAPON:
      MANUAL_SPELL(spell_enchant_weapon)
      ;
      break;
    case SPELL_IDENTIFY:
      MANUAL_SPELL(spell_identify)
      ;
      break;
    case SPELL_LOCATE_OBJECT:
      MANUAL_SPELL(spell_locate_object)
      ;
      break;
    case SPELL_SUMMON:
      MANUAL_SPELL(spell_summon)
      ;
      break;
    case SPELL_WORD_OF_RECALL:
      MANUAL_SPELL(spell_recall)
      ;
      break;
    case SPELL_TELEPORT:
      MANUAL_SPELL(spell_teleport)
      ;
      break;
    case SPELL_CORRUPTION:
      MANUAL_SPELL(spell_corruption)
      ;
      break;
    case SPELL_PLAGUE_BOLT:
      MANUAL_SPELL(spell_plague_bolt)
      ;
      break;
    case SPELL_ENFEEBLEMENT:
      MANUAL_SPELL(spell_enfeeblement)
      ;
      break;
    case SPELL_DEVOUR_SOUL:
      MANUAL_SPELL(spell_devour_soul)
      ;
      break;
    case SPELL_MEMENTO_MORI:
      MANUAL_SPELL(spell_memento_mori)
      ;
      break;
    case SPELL_VAMPIRIC_TOUCH:
      MANUAL_SPELL(spell_vampiric_touch)
      ;
      break;
    case SPELL_GREATER_HEAL:
      MANUAL_SPELL(spell_greater_heal)
      ;
      break;
    case SPELL_CLEANSE:
      MANUAL_SPELL(spell_cleanse)
      ;
      break;
    case SPELL_COUNTERSPELL:
      MANUAL_SPELL(spell_counterspell)
      ;
      break;
    case SPELL_SPELL_STEAL:
      MANUAL_SPELL(spell_spell_steal)
      ;
      break;
    case SPELL_CANCELLATION:
      MANUAL_SPELL(spell_cancellation)
      ;
      break;
    case SPELL_CONJURE_ELEMENTAL:
      MANUAL_SPELL(spell_conjure_elemental)
      ;
      break;
    case SPELL_CALL_WOLVES:
      MANUAL_SPELL(spell_call_wolves)
      ;
      break;
    case SPELL_CALL_BEARS:
      MANUAL_SPELL(spell_call_bears)
      ;
      break;
    case SPELL_ANIMATE_DEAD_GREATER:
      MANUAL_SPELL(spell_animate_dead_greater)
      ;
      break;
    case SPELL_ABYSS_GATE:
      MANUAL_SPELL(spell_abyss_gate)
      ;
      break;
    case SPELL_GATE:
      MANUAL_SPELL(spell_gate)
      ;
      break;
    case SPELL_PORTAL:
      MANUAL_SPELL(spell_portal)
      ;
      break;
    case SPELL_LOCATE_CORPSE:
      MANUAL_SPELL(spell_locate_corpse)
      ;
      break;
    case SPELL_WORD_OF_RECALL_MASS:
      MANUAL_SPELL(spell_word_of_recall_mass)
      ;
      break;
    case SPELL_ASTRAL_PROJECTION:
      MANUAL_SPELL(spell_astral_projection)
      ;
      break;
    case SPELL_ETHEREAL_JAUNT:
      MANUAL_SPELL(spell_ethereal_jaunt)
      ;
      break;
    case SPELL_LEYLINE_TAP:
      MANUAL_SPELL(spell_leyline_tap)
      ;
      break;
    case SPELL_TEMPORAL_SHIFT:
      MANUAL_SPELL(spell_temporal_shift)
      ;
      break;
    case SPELL_CHRONO_SHIFT:
      MANUAL_SPELL(spell_chrono_shift)
      ;
      break;
    }

  return (1);
}

/* mag_objectmagic: This is the entry-point for all magic items.  This should
 * only be called by the 'quaff', 'use', 'recite', etc. routines.
 * For reference, object values 0-3:
 * staff  - [0]	level	[1] max charges	[2] num charges	[3] spell num
 * wand   - [0]	level	[1] max charges	[2] num charges	[3] spell num
 * scroll - [0]	level	[1] spell num	[2] spell num	[3] spell num
 * potion - [0] level	[1] spell num	[2] spell num	[3] spell num
 * Staves and wands will default to level 14 if the level is not specified; the
 * DikuMUD format did not specify staff and wand levels in the world files */
void mag_objectmagic(struct char_data *ch, struct obj_data *obj, char *argument) {
  char arg[MAX_INPUT_LENGTH];
  int i, k;
  struct char_data *tch = NULL, *next_tch;
  struct obj_data *tobj = NULL;

  one_argument(argument, arg);

  k = generic_find(arg, FIND_CHAR_ROOM | FIND_OBJ_INV | FIND_OBJ_ROOM |
  FIND_OBJ_EQUIP, ch, &tch, &tobj);

  switch (GET_OBJ_TYPE(obj)) {
  case ITEM_STAFF:
    act("You tap $p three times on the ground.", FALSE, ch, obj, 0, TO_CHAR);
    if (obj->action_description)
      act(obj->action_description, FALSE, ch, obj, 0, TO_ROOM);
    else
      act("$n taps $p three times on the ground.", FALSE, ch, obj, 0, TO_ROOM);

    if (GET_OBJ_VAL(obj, 2) <= 0) {
      send_to_char(ch, "It seems powerless.\r\n");
      act("Nothing seems to happen.", FALSE, ch, obj, 0, TO_ROOM);
    } else {
      GET_OBJ_VAL(obj, 2)--;
      WAIT_STATE(ch, PULSE_VIOLENCE);
      /* Level to cast spell at. */
      k = GET_OBJ_VAL(obj, 0) ? GET_OBJ_VAL(obj, 0) : DEFAULT_STAFF_LVL;

      /* Area/mass spells on staves can cause crashes. So we use special cases
       * for those spells spells here. */
      if (HAS_SPELL_ROUTINE(GET_OBJ_VAL(obj, 3), MAG_MASSES | MAG_AREAS)) {
        for (i = 0, tch = world[IN_ROOM(ch)].people; tch;
            tch = tch->next_in_room)
          i++;
        while (i-- > 0)
          call_magic(ch, NULL, NULL, GET_OBJ_VAL(obj, 3), k, CAST_STAFF);
      } else {
        for (tch = world[IN_ROOM(ch)].people; tch; tch = next_tch) {
          next_tch = tch->next_in_room;
          if (ch != tch)
            call_magic(ch, tch, NULL, GET_OBJ_VAL(obj, 3), k, CAST_STAFF);
        }
      }
    }
    break;
  case ITEM_WAND:
    if (k == FIND_CHAR_ROOM) {
      if (tch == ch) {
        act("You point $p at yourself.", FALSE, ch, obj, 0, TO_CHAR);
        act("$n points $p at $mself.", FALSE, ch, obj, 0, TO_ROOM);
      } else {
        act("You point $p at $N.", FALSE, ch, obj, tch, TO_CHAR);
        if (obj->action_description)
          act(obj->action_description, FALSE, ch, obj, tch, TO_ROOM);
        else
          act("$n points $p at $N.", TRUE, ch, obj, tch, TO_ROOM);
      }
    } else if (tobj != NULL) {
      act("You point $p at $P.", FALSE, ch, obj, tobj, TO_CHAR);
      if (obj->action_description)
        act(obj->action_description, FALSE, ch, obj, tobj, TO_ROOM);
      else
        act("$n points $p at $P.", TRUE, ch, obj, tobj, TO_ROOM);
    } else if (IS_SET(spell_info[GET_OBJ_VAL(obj, 3)].routines,
        MAG_AREAS | MAG_MASSES)) {
      /* Wands with area spells don't need to be pointed. */
      act("You point $p outward.", FALSE, ch, obj, NULL, TO_CHAR);
      act("$n points $p outward.", TRUE, ch, obj, NULL, TO_ROOM);
    } else {
      act("At what should $p be pointed?", FALSE, ch, obj, NULL, TO_CHAR);
      return;
    }

    if (GET_OBJ_VAL(obj, 2) <= 0) {
      send_to_char(ch, "It seems powerless.\r\n");
      act("Nothing seems to happen.", FALSE, ch, obj, 0, TO_ROOM);
      return;
    }
    GET_OBJ_VAL(obj, 2)--;
    WAIT_STATE(ch, PULSE_VIOLENCE);
    if (GET_OBJ_VAL(obj, 0))
      call_magic(ch, tch, tobj, GET_OBJ_VAL(obj, 3), GET_OBJ_VAL(obj, 0),
          CAST_WAND);
    else
      call_magic(ch, tch, tobj, GET_OBJ_VAL(obj, 3),
      DEFAULT_WAND_LVL, CAST_WAND);
    break;
  case ITEM_SCROLL:
    if (*arg) {
      if (!k) {
        act("There is nothing to here to affect with $p.", FALSE, ch, obj, NULL,
            TO_CHAR);
        return;
      }
    } else
      tch = ch;

    act("You recite $p which dissolves.", TRUE, ch, obj, 0, TO_CHAR);
    if (obj->action_description)
      act(obj->action_description, FALSE, ch, obj, tch, TO_ROOM);
    else
      act("$n recites $p.", FALSE, ch, obj, NULL, TO_ROOM);

    WAIT_STATE(ch, PULSE_VIOLENCE);
    for (i = 1; i <= 3; i++)
      if (call_magic(ch, tch, tobj, GET_OBJ_VAL(obj, i), GET_OBJ_VAL(obj, 0),
          CAST_SCROLL) <= 0)
        break;

    if (obj != NULL)
      extract_obj(obj);
    break;
  case ITEM_POTION:
    tch = ch;

    if (!consume_otrigger(obj, ch, OCMD_QUAFF)) /* check trigger */
      return;

    act("You quaff $p.", FALSE, ch, obj, NULL, TO_CHAR);
    if (obj->action_description)
      act(obj->action_description, FALSE, ch, obj, NULL, TO_ROOM);
    else
      act("$n quaffs $p.", TRUE, ch, obj, NULL, TO_ROOM);

    WAIT_STATE(ch, PULSE_VIOLENCE);
    for (i = 1; i <= 3; i++)
      if (call_magic(ch, ch, NULL, GET_OBJ_VAL(obj, i), GET_OBJ_VAL(obj, 0),
          CAST_POTION) <= 0)
        break;

    if (obj != NULL)
      extract_obj(obj);
    break;
  default:
    log("SYSERR: Unknown object_type %d in mag_objectmagic.",
        GET_OBJ_TYPE(obj));
    break;
  }
}

/* cast_spell is used generically to cast any spoken spell, assuming we already
 * have the target char/obj and spell number.  It checks all restrictions,
 * prints the words, etc. Entry point for NPC casts.  Recommended entry point
 * for spells cast by NPCs via specprocs. */
int cast_spell(struct char_data *ch, struct char_data *tch,
    struct obj_data *tobj, int spellnum) {
  if (spellnum < 0 || spellnum > TOP_SPELL_DEFINE) {
    log("SYSERR: cast_spell trying to call spellnum %d/%d.", spellnum,
    TOP_SPELL_DEFINE);
    return (0);
  }

  if (AFF_FLAGGED(ch, AFF_STUNNED)) {
    send_to_char(ch, "You are too stunned to cast!\r\n");
    return (0);
  }

  if (GET_POS(ch) < SINFO.min_position) {
    switch (GET_POS(ch)) {
      case POS_SLEEPING:
        send_to_char(ch, "You dream about great magical powers.\r\n");
        break;
      case POS_RESTING:
        send_to_char(ch, "You cannot concentrate while resting.\r\n");
        break;
      case POS_SITTING:
        send_to_char(ch, "You can't do this sitting!\r\n");
        break;
      case POS_FIGHTING:
        send_to_char(ch, "Impossible!  You can't concentrate enough!\r\n");
        break;
      default:
        send_to_char(ch, "You can't do much of anything like this!\r\n");
        break;
    }
    return (0);
  }
  if (AFF_FLAGGED(ch, AFF_CHARM) && (ch->master == tch)) {
    send_to_char(ch, "You are afraid you might hurt your master!\r\n");
    return (0);
  }
  if ((tch != ch) && IS_SET(SINFO.targets, TAR_SELF_ONLY)) {
    if (is_sanctuary_spell(spellnum))
      send_to_char(ch, "You can only invoke this protection on yourself.\r\n");
    else
      send_to_char(ch, "You can only cast this spell upon yourself!\r\n");
    return (0);
  }
  if ((tch == ch) && IS_SET(SINFO.targets, TAR_NOT_SELF)) {
    send_to_char(ch, "You cannot cast this spell upon yourself!\r\n");
    return (0);
  }
  if (IS_SET(SINFO.routines, MAG_GROUPS) && !GROUP(ch)) {
    send_to_char(ch, "You can't cast this spell if you're not in a group!\r\n");
    return (0);
  }
  if (is_spirit_spell(spellnum) && !can_bind_spirit(ch, spellnum))
    return (0);
  send_to_char(ch, "%s", CONFIG_OK);

  if (!send_cast_message(ch, tch, tobj, spellnum)) {
    /* fallback feedback */
    if (!IS_NPC(ch)) {
      if (tch) {
        send_to_char(ch, "You cast %s on %s.\r\n", skill_name(spellnum), (tch == ch) ? "yourself" : GET_NAME(tch));
      } else if (tobj) {
        send_to_char(ch, "You cast %s on %s.\r\n", skill_name(spellnum), GET_OBJ_SHORT(tobj));
      } else {
        send_to_char(ch, "You cast %s.\r\n", skill_name(spellnum));
      }
    }
  }

  say_spell(ch, spellnum, tch, tobj);

  return (call_magic(ch, tch, tobj, spellnum, GET_LEVEL(ch), CAST_SPELL));
}

ACMD(do_spellup)
{
  struct char_data *tch = NULL;
  char arg[MAX_INPUT_LENGTH];
  int spellnum;
  int mana;
  bool any_eligible = FALSE;
  bool any_attempted = FALSE;
  int cast_count = 0, skipped_active = 0, skipped_mana = 0, skipped_combat = 0;

  if (IS_NPC(ch))
    return;

  one_argument(argument, arg);

  if (*arg) {
    char *argp = arg;
    int number = get_number(&argp);
    if ((tch = get_char_vis(ch, arg, &number, FIND_CHAR_ROOM)) == NULL) {
      send_to_char(ch, "You don't see that person here.\r\n");
      return;
    }
  } else {
    tch = ch;
  }

  if (FIGHTING(ch) && tch != ch) {
    send_to_char(ch, "You are too busy fighting to buff someone else right now.\r\n");
    return;
  }

  for (spellnum = 1; spellnum <= MAX_SPELLS; spellnum++) {
    if (!is_spellup_beneficial_spell(spellnum))
      continue;
    if (GET_LEVEL(ch) < SINFO.min_level[(int) GET_CLASS(ch)])
      continue;
    if (GET_SKILL(ch, spellnum) == 0)
      continue;
    if (tch != ch && IS_SET(SINFO.targets, TAR_SELF_ONLY))
      continue;
    if (tch == ch && IS_SET(SINFO.targets, TAR_NOT_SELF))
      continue;
    if (FIGHTING(ch) && (SINFO.min_position > POS_FIGHTING || !IS_SET(SINFO.targets, TAR_FIGHT_SELF))) {
      skipped_combat++;
      continue;
    }
    if (affected_by_spell(tch, spellnum)) {
      skipped_active++;
      continue;
    }

    any_eligible = TRUE;
    if (AFF_FLAGGED(ch, AFF_SILENCED)) {
      send_to_char(ch, "You open your mouth but no words come out!\r\n");
      continue;
    }
    if (spell_on_cooldown(ch, spellnum)) {
      send_to_char(ch, "That spell is still recovering.\r\n");
      continue;
    }
    mana = mag_manacost(ch, spellnum);
    if ((mana > 0) && (GET_MANA(ch) < mana) && (GET_LEVEL(ch) < LVL_IMMORT)) {
      skipped_mana++;
      continue;
    }

    any_attempted = TRUE;
    if (AFF_FLAGGED(ch, AFF_SPELLLOCK) && rand_number(1, 100) <= 40) {
      send_to_char(ch, "Your concentration shatters and the spell fizzles!\r\n");
      WAIT_STATE(ch, PULSE_VIOLENCE);
      if (mana > 0)
        GET_MANA(ch) = MAX(0, MIN(effective_max_mana(ch), GET_MANA(ch) - mana));
      continue;
    }
    if (rand_number(0, 101) > GET_SKILL(ch, spellnum)) {
      WAIT_STATE(ch, PULSE_VIOLENCE);
      if (!tch || !skill_message(0, ch, tch, spellnum))
        send_to_char(ch, "You lost your concentration!\r\n");
      if (mana > 0)
        GET_MANA(ch) = MAX(0, MIN(effective_max_mana(ch), GET_MANA(ch) - (mana / 2)));
      if (SINFO.violent && tch && IS_NPC(tch))
        hit(tch, ch, TYPE_UNDEFINED);
      improve_ability_from_use(ch, spellnum, 0);
    } else {
      if (cast_spell(ch, tch, NULL, spellnum)) {
        improve_ability_from_use(ch, spellnum, 1);
        WAIT_STATE(ch, PULSE_VIOLENCE);
        if (mana > 0)
          GET_MANA(ch) = MAX(0, MIN(effective_max_mana(ch), GET_MANA(ch) - mana));
        cast_count++;
      }
    }
  }

  if (!any_eligible) {
    if (tch == ch)
      send_to_char(ch, "You don't know any buff spells to cast.\r\n");
    else
      send_to_char(ch, "You don't know any buff spells to cast on them.\r\n");
  } else if (!any_attempted) {
    send_to_char(ch, "You don't have the energy to cast any buffs right now.\r\n");
  } else {
    send_to_char(ch, "Spellup complete: %d cast, %d already active, %d low mana, %d blocked in combat.\r\n",
                 cast_count, skipped_active, skipped_mana, skipped_combat);
  }
}

/* do_cast is the entry point for PC-casted spells.  It parses the arguments,
 * determines the spell number and finds a target, throws the die to see if
 * the spell can be cast, checks for sufficient mana and subtracts it, and
 * passes control to cast_spell(). */
ACMD(do_cast) {
  struct char_data *tch = NULL;
  struct obj_data *tobj = NULL;
  char *target_argument;
  char *targp = NULL;
  char spell_input[MAX_INPUT_LENGTH], target_input[MAX_INPUT_LENGTH];
  char work[MAX_INPUT_LENGTH];
  char target_name[MAX_INPUT_LENGTH];
  char ambiguity[MAX_STRING_LENGTH];
  int number, mana, spellnum, i, target = 0;
  int matched_tokens = 0;

  if (IS_NPC(ch))
    return;

  /* get: blank, spell name, target name */
  target_argument = NULL;
  skip_spaces(&argument);

  if (*argument == '\0') {
    send_to_char(ch, "Cast what where?\r\n");
    return;
  }

  if (*argument == '\'' || *argument == '"') {
    char quote = *argument++;
    const char *closing = strchr(argument, quote);
    size_t len = closing ? (size_t)(closing - argument) : strlen(argument);

    if (len >= sizeof(spell_input))
      len = sizeof(spell_input) - 1;
    memcpy(spell_input, argument, len);
    spell_input[len] = '\0';

    target_argument = closing ? (char *)closing + 1 : NULL;
    if (target_argument) {
      char *target_ptr = (char *)target_argument;
      skip_spaces(&target_ptr);
      target_argument = target_ptr;
    }

    spellnum = find_spell_by_tokens(spell_input, ambiguity, sizeof(ambiguity),
        &matched_tokens, FALSE, FALSE);
  } else {
    int best_cut = -1;
    int best_spellnum = -1;
    char ambiguity_local[MAX_STRING_LENGTH];

    strlcpy(work, argument, sizeof(work));

    char *p;

    for (p = work; *p; p++) {
      if (*p == ' ') {
        char saved = *p;
        *p = '\0';

        if (*work) {
          int sn = find_spell_by_prefix(work, ambiguity_local,
              sizeof(ambiguity_local));
          if (sn > 0) {
            best_spellnum = sn;
            best_cut = (int)(p - work);
          }
        }

        *p = saved;
      }
    }

    if (*work) {
      int snfull = find_spell_by_prefix(work, ambiguity_local,
          sizeof(ambiguity_local));
      if (snfull > 0) {
        best_spellnum = snfull;
        best_cut = -1;
      }
    }

    if (best_spellnum > 0) {
      if (best_cut >= 0) {
        work[best_cut] = '\0';
        target_argument = work + best_cut + 1;
        skip_spaces(&target_argument);
      } else {
        target_argument = NULL;
      }
      spellnum = best_spellnum;
    } else {
      spellnum = find_spell_by_tokens(argument, ambiguity, sizeof(ambiguity),
          &matched_tokens, TRUE, TRUE);
      if (spellnum > 0) {
        char *target_ptr = argument;
        for (i = 0; i < matched_tokens && *target_ptr; i++)
          target_ptr = any_one_arg(target_ptr, spell_input);
        skip_spaces(&target_ptr);
        target_argument = target_ptr;
      }
    }
  }

  if (spellnum == -2) {
    send_to_char(ch, "Ambiguous spell name. Did you mean: %s?\r\n",
        ambiguity);
    return;
  }

  if ((spellnum < 1) || (spellnum > MAX_SPELLS)) {
    send_to_char(ch, "Cast what?!?\r\n");
    return;
  }
  if (GET_LEVEL(ch) < SINFO.min_level[(int) GET_CLASS(ch)]) {
    send_to_char(ch, "You do not know that spell!\r\n");
    return;
  }
  if (GET_SKILL(ch, spellnum) == 0) {
    send_to_char(ch, "You are unfamiliar with that spell.\r\n");
    return;
  }
  /* Find the target */
  if (target_argument != NULL) {
    strlcpy(target_input, target_argument, sizeof(target_input));
    strlcpy(target_name, target_input, sizeof(target_name));
    one_argument(target_name, target_name);
    target_argument = target_name;
    targp = target_argument;
    skip_spaces(&targp);
    target_argument = targp;

    /* Copy target to global cast_arg2, for use in spells like locate object */
    strcpy(cast_arg2, target_argument);
  }
  if (IS_SET(SINFO.targets, TAR_IGNORE)) {
    target = TRUE;
  } else if (target_argument != NULL && *target_argument) {
    char *target_lookup;
    number = get_number(&targp);
    target_lookup = (targp && *targp) ? targp : target_argument;
    if (!target && (IS_SET(SINFO.targets, TAR_CHAR_ROOM))) {
      if ((tch = get_char_vis(ch, target_lookup, &number, FIND_CHAR_ROOM)) != NULL)
        target = TRUE;
    }
    if (!target && IS_SET(SINFO.targets, TAR_CHAR_WORLD))
      if ((tch = get_char_vis(ch, target_lookup, &number, FIND_CHAR_WORLD)) != NULL)
        target = TRUE;

    if (!target && IS_SET(SINFO.targets, TAR_OBJ_INV))
      if ((tobj = get_obj_in_list_vis(ch, target_lookup, &number, ch->carrying)) != NULL)
        target = TRUE;

    if (!target && IS_SET(SINFO.targets, TAR_OBJ_EQUIP)) {
      if ((tobj = get_obj_in_equip_vis(ch, target_lookup, &number, ch->equipment)) != NULL)
        target = TRUE;
    }
    if (!target && IS_SET(SINFO.targets, TAR_OBJ_ROOM))
      if ((tobj = get_obj_in_list_vis(ch, target_lookup, &number,
          world[IN_ROOM(ch)].contents)) != NULL)
        target = TRUE;

    if (!target && IS_SET(SINFO.targets, TAR_OBJ_WORLD))
      if ((tobj = get_obj_vis(ch, target_lookup, &number)) != NULL)
        target = TRUE;

    if (!target && (IS_SET(SINFO.targets, TAR_CHAR_ROOM) ||
        IS_SET(SINFO.targets, TAR_CHAR_WORLD))) {
      bool include_fighting = IS_SET(SINFO.targets, TAR_FIGHT_VICT);
      tch = find_char_prefix(ch, target_lookup, number, include_fighting,
          ambiguity, sizeof(ambiguity));
      if (tch)
        target = TRUE;
      else if (*ambiguity) {
        send_to_char(ch, "Ambiguous target. Did you mean: %s?\r\n",
            ambiguity);
        return;
      }
    }

  } else { /* if target string is empty */
    if (!target && IS_SET(SINFO.targets, TAR_FIGHT_VICT))
      if (FIGHTING(ch) != NULL) {
        tch = FIGHTING(ch);
        target = TRUE;
      }
    if (!target && SINFO.violent && IS_SET(SINFO.targets, TAR_CHAR_ROOM)
        && !IS_SET(SINFO.targets, TAR_SELF_ONLY) && FIGHTING(ch) != NULL) {
      tch = FIGHTING(ch);
      target = TRUE;
    }
    if (!target && IS_SET(SINFO.targets, TAR_FIGHT_SELF))
      if (FIGHTING(ch) != NULL) {
        tch = ch;
        target = TRUE;
      }
    if (!target && (IS_SET(SINFO.targets, TAR_CHAR_ROOM) ||
        IS_SET(SINFO.targets, TAR_CHAR_WORLD)) &&
        (!SINFO.violent || IS_SET(SINFO.targets, TAR_SELF_ONLY))) {
      tch = ch;
      target = TRUE;
    }
    if (!target) {
      send_to_char(ch, "Upon %s should the spell be cast?\r\n",
          IS_SET(SINFO.targets, TAR_OBJ_ROOM | TAR_OBJ_INV | TAR_OBJ_WORLD | TAR_OBJ_EQUIP) ?
              "what" : "who");
      return;
    }
  }

  if (target && (tch == ch) && SINFO.violent) {
    send_to_char(ch, "You shouldn't cast that on yourself -- could be bad for your health!\r\n");
    return;
  }
  if (!target) {
    send_to_char(ch, "Cannot find the target of your spell!\r\n");
    return;
  }
  if (AFF_FLAGGED(ch, AFF_SILENCED)) {
    send_to_char(ch, "You open your mouth but no words come out!\r\n");
    return;
  }
  if (spell_on_cooldown(ch, spellnum)) {
    send_to_char(ch, "That spell is still recovering.\r\n");
    return;
  }
  mana = mag_manacost(ch, spellnum);
  if ((mana > 0) && (GET_MANA(ch) < mana) && (GET_LEVEL(ch) < LVL_IMMORT)) {
    send_to_char(ch, "You haven't the energy to cast that spell!\r\n");
    return;
  }

  if (AFF_FLAGGED(ch, AFF_SPELLLOCK) && rand_number(1, 100) <= 40) {
    send_to_char(ch, "Your concentration shatters and the spell fizzles!\r\n");
    WAIT_STATE(ch, PULSE_VIOLENCE);
    if (mana > 0)
      GET_MANA(ch) = MAX(0, MIN(effective_max_mana(ch), GET_MANA(ch) - mana));
    return;
  }

  /* You throws the dice and you takes your chances.. 101% is total failure */
  if (rand_number(1, 100) > GET_SKILL(ch, spellnum)) {
    WAIT_STATE(ch, PULSE_VIOLENCE);
    if (!tch || !skill_message(0, ch, tch, spellnum))
      send_to_char(ch, "You lost your concentration!\r\n");
    if (mana > 0)
      GET_MANA(ch) = MAX(0, MIN(effective_max_mana(ch), GET_MANA(ch) - (mana / 2)));
    if (SINFO.violent && tch && IS_NPC(tch))
    hit(tch, ch, TYPE_UNDEFINED);
    improve_ability_from_use(ch, spellnum, 0);
  } else { /* cast spell returns 1 on success; subtract mana & set waitstate */
    if (cast_spell(ch, tch, tobj, spellnum)) {
      improve_ability_from_use(ch, spellnum, 1);
      WAIT_STATE(ch, PULSE_VIOLENCE);
      if (mana > 0)
        GET_MANA(ch) = MAX(0, MIN(effective_max_mana(ch), GET_MANA(ch) - mana));
    }
  }
}

void spell_level(int spell, int chclass, int level) {
  int bad = 0;
  int class_count = num_pc_classes();

  if (spell < 0 || spell > TOP_SPELL_DEFINE) {
    log("SYSERR: attempting assign to illegal spellnum %d/%d", spell,
        TOP_SPELL_DEFINE);
    return;
  }

  if (chclass < 0 || chclass >= MAX_CLASSES) {
    log("SYSERR: assigning '%s' to illegal class %d/%d.", skill_name(spell),
        chclass, MAX_CLASSES - 1);
    bad = 1;
  } else if (chclass >= class_count) {
    log("SYSERR: assigning '%s' to class %d, which is outside num_pc_classes (%d).",
        skill_name(spell), chclass, class_count);
  }

  if (level < 1 || level > LVL_IMPL) {
    log("SYSERR: assigning '%s' to illegal level %d/%d.", skill_name(spell),
        level, LVL_IMPL);
    bad = 1;
  }

  if (!bad)
    spell_info[spell].min_level[chclass] = level;
}

/* Assign the spells on boot up */
static void spello(int spl, const char *name, int max_mana, int min_mana,
    int mana_change, int minpos, int targets, int violent, int routines,
    const char *wearoff) {
  int i;

  for (i = 0; i < NUM_CLASSES; i++)
    spell_info[spl].min_level[i] = LVL_IMMORT;
  spell_info[spl].mana_max = max_mana;
  spell_info[spl].mana_min = min_mana;
  spell_info[spl].mana_change = mana_change;
  spell_info[spl].min_position = minpos;
  spell_info[spl].targets = targets;
  spell_info[spl].violent = violent;
  spell_info[spl].routines = routines;
  spell_info[spl].name = name;
  spell_info[spl].wear_off_msg = wearoff;
}

void unused_spell(int spl) {
  int i;

  for (i = 0; i < NUM_CLASSES; i++)
    spell_info[spl].min_level[i] = LVL_IMPL + 1;
  spell_info[spl].mana_max = 0;
  spell_info[spl].mana_min = 0;
  spell_info[spl].mana_change = 0;
  spell_info[spl].min_position = 0;
  spell_info[spl].targets = 0;
  spell_info[spl].violent = 0;
  spell_info[spl].routines = 0;
  spell_info[spl].name = unused_spellname;
}

/* Skills use MOVE costs. We store the cost in spell_info[].mana_* fields
 * and your cast_skill / cast_spell logic decides whether to charge MOVE or MANA.
 */
#define SKILL_DEFAULT_COST 10
#define skillo(skill, name) spello(skill, name, SKILL_DEFAULT_COST, SKILL_DEFAULT_COST, 0, 0, 0, 0, 0, NULL);
#define skillo_cost(skill, name, cost) spello(skill, name, (cost), (cost), 0, 0, 0, 0, 0, NULL);

/* Arguments for spello calls:
 * spellnum, maxmana, minmana, manachng, minpos, targets, violent?, routines.
 * spellnum:  Number of the spell.  Usually the symbolic name as defined in
 *  spells.h (such as SPELL_HEAL).
 * spellname: The name of the spell.
 * maxmana :  The maximum mana this spell will take (i.e., the mana it
 *  will take when the player first gets the spell).
 * minmana :  The minimum mana this spell will take, no matter how high
 *  level the caster is.
 * manachng:  The change in mana for the spell from level to level.  This
 *  number should be positive, but represents the reduction in mana cost as
 *  the caster's level increases.
 * minpos  :  Minimum position the caster must be in for the spell to work
 *  (usually fighting or standing). targets :  A "list" of the valid targets
 *  for the spell, joined with bitwise OR ('|').
 * violent :  TRUE or FALSE, depending on if this is considered a violent
 *  spell and should not be cast in PEACEFUL rooms or on yourself.  Should be
 *  set on any spell that inflicts damage, is considered aggressive (i.e.
 *  charm, curse), or is otherwise nasty.
 * routines:  A list of magic routines which are associated with this spell
 *  if the spell uses spell templates.  Also joined with bitwise OR ('|').
 * See the documentation for a more detailed description of these fields. You
 * only need a spello() call to define a new spell; to decide who gets to use
 * a spell or skill, look in class.c.  -JE */
void mag_assign_spells(void) {
  int i;

  /* Do not change the loop below. */
  for (i = 0; i <= TOP_SPELL_DEFINE; i++)
    unused_spell(i);
  /* Do not change the loop above. */

  spello(SPELL_ANIMATE_DEAD, "animate dead", 35, 10, 3, POS_STANDING,
  TAR_OBJ_ROOM, FALSE, MAG_SUMMONS, NULL);

  spello(SPELL_ARMOR, "armor", 30, 15, 3, POS_FIGHTING,
  TAR_CHAR_ROOM, FALSE, MAG_AFFECTS, "You feel less protected.");

  spello(SPELL_BLESS, "bless", 35, 5, 3, POS_STANDING,
  TAR_CHAR_ROOM | TAR_OBJ_INV, FALSE, MAG_AFFECTS | MAG_ALTER_OBJS,
      "You feel less righteous.");

  spello(SPELL_BLINDNESS, "blindness", 35, 25, 1, POS_STANDING,
  TAR_CHAR_ROOM | TAR_NOT_SELF, FALSE, MAG_AFFECTS,
      "You feel a cloak of blindness dissolve.");

  spello(SPELL_BURNING_HANDS, "burning hands", 30, 10, 3, POS_FIGHTING,
  TAR_CHAR_ROOM | TAR_FIGHT_VICT, TRUE, MAG_DAMAGE, NULL);

  spello(SPELL_CALL_LIGHTNING, "call lightning", 40, 25, 3, POS_FIGHTING,
  TAR_CHAR_ROOM | TAR_FIGHT_VICT, TRUE, MAG_DAMAGE, NULL);

  spello(SPELL_CHARM, "charm person", 75, 50, 2, POS_FIGHTING,
  TAR_CHAR_ROOM | TAR_NOT_SELF, TRUE, MAG_MANUAL,
      "You feel more self-confident.");

  spello(SPELL_CHILL_TOUCH, "chill touch", 12, 6, 1, POS_FIGHTING,
  TAR_CHAR_ROOM | TAR_FIGHT_VICT, TRUE, MAG_DAMAGE | MAG_AFFECTS,
      "You feel your strength return.");

  spello(SPELL_CLONE, "clone", 80, 65, 5, POS_STANDING,
  TAR_IGNORE, FALSE, MAG_SUMMONS, NULL);

  spello(SPELL_COLOR_SPRAY, "color spray", 30, 15, 3, POS_FIGHTING,
  TAR_CHAR_ROOM | TAR_FIGHT_VICT, TRUE, MAG_DAMAGE, NULL);

  spello(SPELL_CONTROL_WEATHER, "control weather", 75, 25, 5, POS_STANDING,
  TAR_IGNORE, FALSE, MAG_MANUAL, NULL);

  spello(SPELL_CORRUPTION, "corruption", 14, 7, 1, POS_FIGHTING,
  TAR_CHAR_ROOM | TAR_FIGHT_VICT, TRUE, MAG_MANUAL,
      "The lingering corruption fades away.");

  spello(SPELL_CREATE_FOOD, "create food", 30, 5, 4, POS_STANDING,
  TAR_IGNORE, FALSE, MAG_CREATIONS, NULL);

  spello(SPELL_CREATE_WATER, "create water", 30, 5, 4, POS_STANDING,
  TAR_OBJ_INV | TAR_OBJ_EQUIP, FALSE, MAG_MANUAL, NULL);

  spello(SPELL_CURE_BLIND, "cure blind", 30, 5, 2, POS_STANDING,
  TAR_CHAR_ROOM, FALSE, MAG_UNAFFECTS, NULL);

  spello(SPELL_CURE_CRITIC, "cure critic", 30, 10, 2, POS_FIGHTING,
  TAR_CHAR_ROOM, FALSE, MAG_POINTS, NULL);

  spello(SPELL_CURE_LIGHT, "cure light", 30, 10, 2, POS_FIGHTING,
  TAR_CHAR_ROOM, FALSE, MAG_POINTS, NULL);

  spello(SPELL_CURSE, "curse", 12, 6, 1, POS_STANDING,
  TAR_CHAR_ROOM | TAR_OBJ_INV, TRUE, MAG_AFFECTS | MAG_ALTER_OBJS,
      "You feel more optimistic.");

  spello(SPELL_DARKNESS, "darkness", 30, 5, 4, POS_STANDING,
  TAR_IGNORE, FALSE, MAG_ROOMS, NULL);

  spello(SPELL_DETECT_ALIGN, "detect alignment", 20, 10, 2, POS_STANDING,
  TAR_CHAR_ROOM | TAR_SELF_ONLY, FALSE, MAG_AFFECTS, "You feel less aware.");

  spello(SPELL_DETECT_INVIS, "detect invisibility", 20, 10, 2, POS_STANDING,
  TAR_CHAR_ROOM | TAR_SELF_ONLY, FALSE, MAG_AFFECTS,
      "Your eyes stop tingling.");

  spello(SPELL_DETECT_MAGIC, "detect magic", 10, 5, 1, POS_STANDING,
  TAR_CHAR_ROOM | TAR_SELF_ONLY, FALSE, MAG_AFFECTS,
      "The detect magic wears off.");

  spello(SPELL_DETECT_POISON, "detect poison", 15, 5, 1, POS_STANDING,
  TAR_CHAR_ROOM | TAR_OBJ_INV | TAR_OBJ_ROOM, FALSE, MAG_MANUAL,
      "The detect poison wears off.");

  spello(SPELL_DISPEL_EVIL, "dispel evil", 40, 25, 3, POS_FIGHTING,
  TAR_CHAR_ROOM | TAR_FIGHT_VICT, TRUE, MAG_DAMAGE, NULL);

  spello(SPELL_DISPEL_GOOD, "dispel good", 40, 25, 3, POS_FIGHTING,
  TAR_CHAR_ROOM | TAR_FIGHT_VICT, TRUE, MAG_DAMAGE, NULL);

  spello(SPELL_EARTHQUAKE, "earthquake", 40, 25, 3, POS_FIGHTING,
  TAR_IGNORE, TRUE, MAG_AREAS, NULL);

  spello(SPELL_ENCHANT_WEAPON, "enchant weapon", 150, 100, 10, POS_STANDING,
  TAR_OBJ_INV, FALSE, MAG_MANUAL, NULL);

  spello(SPELL_ENERGY_DRAIN, "energy drain", 40, 25, 1, POS_FIGHTING,
  TAR_CHAR_ROOM | TAR_FIGHT_VICT, TRUE, MAG_DAMAGE | MAG_MANUAL, NULL);

  spello(SPELL_GROUP_ARMOR, "group armor", 50, 30, 2, POS_STANDING,
  TAR_IGNORE, FALSE, MAG_GROUPS, NULL);

  spello(SPELL_FIREBALL, "fireball", 40, 30, 2, POS_FIGHTING,
  TAR_CHAR_ROOM | TAR_FIGHT_VICT, TRUE, MAG_DAMAGE, NULL);

  spello(SPELL_FLY, "fly", 40, 20, 2, POS_FIGHTING,
  TAR_CHAR_ROOM, FALSE, MAG_AFFECTS, "You drift slowly to the ground.");

  spello(SPELL_GROUP_HEAL, "group heal", 80, 60, 5, POS_STANDING,
  TAR_IGNORE, FALSE, MAG_GROUPS, NULL);

  spello(SPELL_HARM, "harm", 75, 45, 3, POS_FIGHTING,
  TAR_CHAR_ROOM | TAR_FIGHT_VICT, TRUE, MAG_DAMAGE, NULL);

  spello(SPELL_HEAL, "heal", 60, 40, 3, POS_FIGHTING,
  TAR_CHAR_ROOM, FALSE, MAG_POINTS | MAG_UNAFFECTS, NULL);

  spello(SPELL_INFRAVISION, "infravision", 25, 10, 1, POS_STANDING,
  TAR_CHAR_ROOM | TAR_SELF_ONLY, FALSE, MAG_AFFECTS,
      "Your night vision seems to fade.");

  spello(SPELL_INVISIBLE, "invisibility", 35, 25, 1, POS_STANDING,
  TAR_CHAR_ROOM | TAR_OBJ_INV | TAR_OBJ_ROOM, FALSE,
      MAG_AFFECTS | MAG_ALTER_OBJS, "You feel yourself exposed.");

  spello(SPELL_LIGHTNING_BOLT, "lightning bolt", 30, 15, 1, POS_FIGHTING,
  TAR_CHAR_ROOM | TAR_FIGHT_VICT, TRUE, MAG_DAMAGE, NULL);

  spello(SPELL_LOCATE_OBJECT, "locate object", 25, 20, 1, POS_STANDING,
  TAR_OBJ_WORLD, FALSE, MAG_MANUAL, NULL);

  spello(SPELL_MAGIC_MISSILE, "magic missile", 25, 10, 3, POS_FIGHTING,
  TAR_CHAR_ROOM | TAR_FIGHT_VICT, TRUE, MAG_DAMAGE, NULL);

  spello(SPELL_POISON, "poison", 50, 20, 3, POS_STANDING,
  TAR_CHAR_ROOM | TAR_NOT_SELF | TAR_OBJ_INV, TRUE,
  MAG_AFFECTS | MAG_ALTER_OBJS, "You feel less sick.");

  spello(SPELL_PROT_FROM_EVIL, "protection from evil", 40, 10, 3, POS_STANDING,
  TAR_CHAR_ROOM | TAR_SELF_ONLY, FALSE, MAG_AFFECTS,
      "You feel less protected.");

  spello(SPELL_REMOVE_CURSE, "remove curse", 45, 25, 5, POS_STANDING,
  TAR_CHAR_ROOM | TAR_OBJ_INV | TAR_OBJ_EQUIP, FALSE,
  MAG_UNAFFECTS | MAG_ALTER_OBJS, NULL);

  spello(SPELL_REMOVE_POISON, "remove poison", 40, 8, 4, POS_STANDING,
  TAR_CHAR_ROOM | TAR_OBJ_INV | TAR_OBJ_ROOM, FALSE,
      MAG_UNAFFECTS | MAG_ALTER_OBJS, NULL);

  spello(SPELL_SANCTUARY, "sanctuary", 110, 85, 5, POS_STANDING,
  TAR_CHAR_ROOM, FALSE, MAG_AFFECTS, "The divine glow around you fades away.");

  spello(SPELL_ARCANE_WARD, "arcane ward", 110, 85, 5, POS_STANDING,
  TAR_CHAR_ROOM | TAR_SELF_ONLY, FALSE, MAG_AFFECTS,
  "The arcane runes around you flicker and fade.");

  spello(SPELL_EVASION, "evasion", 110, 85, 5, POS_STANDING,
  TAR_CHAR_ROOM | TAR_SELF_ONLY, FALSE, MAG_AFFECTS,
  "Your heightened awareness fades.");

  spello(SPELL_IRONSKIN, "ironskin", 110, 85, 5, POS_STANDING,
  TAR_CHAR_ROOM | TAR_SELF_ONLY, FALSE, MAG_AFFECTS,
  "Your skin softens back to normal.");

  spello(SPELL_DIVINE_BULWARK, "divine bulwark", 110, 85, 5, POS_STANDING,
  TAR_CHAR_ROOM | TAR_SELF_ONLY, FALSE, MAG_AFFECTS,
  "The holy bulwark around you dissipates.");

  spello(SPELL_SONG_OF_RESILIENCE, "song of resilience", 110, 85, 5, POS_STANDING,
  TAR_CHAR_ROOM, FALSE, MAG_AFFECTS,
  "The resonant melody around you fades.");

  spello(SPELL_DARK_AEGIS, "dark aegis", 110, 85, 5, POS_STANDING,
  TAR_CHAR_ROOM | TAR_SELF_ONLY, FALSE, MAG_AFFECTS,
  "The shadows around you unravel and disperse.");

  spello(SPELL_NIRVANA, "nirvana", 110, 85, 5, POS_STANDING,
  TAR_CHAR_ROOM | TAR_SELF_ONLY, FALSE, MAG_AFFECTS,
  "Your eyes dim as the primal serenity fades.");

  spello(SPELL_PLAGUE_BOLT, "plague bolt", 14, 7, 1, POS_FIGHTING,
  TAR_CHAR_ROOM | TAR_FIGHT_VICT, TRUE, MAG_MANUAL,
  "The \tGplague\tn in your veins subsides.\tn");

  spello(SPELL_ENFEEBLEMENT, "enfeeblement", 45, 20, 2, POS_FIGHTING,
  TAR_CHAR_ROOM | TAR_FIGHT_VICT | TAR_NOT_SELF, TRUE, MAG_MANUAL,
  "Your strength and agility return.\tn");

  spello(SPELL_MEMENTO_MORI, "memento mori", 10, 5, 2, POS_STANDING,
  TAR_CHAR_ROOM | TAR_FIGHT_VICT | TAR_NOT_SELF, TRUE, MAG_MANUAL,
  "The \tDdeath omen\tn lifts.\tn");

  spello(SPELL_DEVOUR_SOUL, "devour soul", 120, 90, 3, POS_FIGHTING,
  TAR_CHAR_ROOM | TAR_FIGHT_VICT, TRUE, MAG_MANUAL,
  "The hollow ache in your soul fades.\tn");

  spello(SPELL_FIREBOLT, "firebolt", 12, 12, 0, POS_FIGHTING,
  TAR_CHAR_ROOM | TAR_FIGHT_VICT, TRUE, MAG_DAMAGE, NULL);

  spello(SPELL_FLAME_ARROW, "flame arrow", 16, 16, 0, POS_FIGHTING,
  TAR_CHAR_ROOM | TAR_FIGHT_VICT, TRUE, MAG_DAMAGE | MAG_AFFECTS,
  "The flames consuming you gutter out.");

  spello(SPELL_FROSTBITE, "frostbite", 14, 14, 0, POS_FIGHTING,
  TAR_CHAR_ROOM | TAR_FIGHT_VICT, TRUE, MAG_DAMAGE | MAG_AFFECTS,
  "Feeling returns to your limbs.");

  spello(SPELL_VOLTAIC_BOLT, "voltaic bolt", 18, 18, 0, POS_FIGHTING,
  TAR_CHAR_ROOM | TAR_FIGHT_VICT, TRUE, MAG_DAMAGE | MAG_AFFECTS,
  "The static charge around you dissipates.");

  spello(SPELL_ACID_BLAST, "acid blast", 20, 20, 0, POS_FIGHTING,
  TAR_CHAR_ROOM | TAR_FIGHT_VICT, TRUE, MAG_DAMAGE | MAG_AFFECTS,
  "The acid corroding your armor neutralizes.");

  spello(SPELL_SHADOW_BOLT, "shadow bolt", 16, 16, 0, POS_FIGHTING,
  TAR_CHAR_ROOM | TAR_FIGHT_VICT, TRUE, MAG_DAMAGE, NULL);

  spello(SPELL_VAMPIRIC_TOUCH, "vampiric touch", 20, 20, 0, POS_FIGHTING,
  TAR_CHAR_ROOM | TAR_FIGHT_VICT, TRUE, MAG_MANUAL, NULL);

  spello(SPELL_WEB, "web", 15, 15, 0, POS_FIGHTING,
  TAR_CHAR_ROOM | TAR_FIGHT_VICT | TAR_NOT_SELF, TRUE, MAG_AFFECTS,
  "The webs binding you dissolve away.");

  spello(SPELL_SILENCE, "silence", 15, 15, 0, POS_FIGHTING,
  TAR_CHAR_ROOM | TAR_FIGHT_VICT | TAR_NOT_SELF, TRUE, MAG_AFFECTS,
  "The magical silence lifts from your throat.");

  spello(SPELL_FEAR, "fear", 18, 18, 0, POS_FIGHTING,
  TAR_CHAR_ROOM | TAR_FIGHT_VICT | TAR_NOT_SELF, TRUE, MAG_AFFECTS,
  "The supernatural terror drains from you.");

  spello(SPELL_TRUE_SEEING, "true seeing", 20, 20, 0, POS_STANDING,
  TAR_CHAR_ROOM | TAR_SELF_ONLY, FALSE, MAG_AFFECTS,
  "Your magically enhanced vision fades.");

  spello(SPELL_STONE_SKIN, "stone skin", 20, 20, 0, POS_STANDING,
  TAR_CHAR_ROOM, FALSE, MAG_AFFECTS,
  "Your stone skin crumbles away.");

  spello(SPELL_BARKSKIN, "barkskin", 15, 15, 0, POS_STANDING,
  TAR_CHAR_ROOM, FALSE, MAG_AFFECTS,
  "Your barkskin softens back to normal.");

  spello(SPELL_GIANT_STRENGTH, "giant strength", 15, 15, 0, POS_STANDING,
  TAR_CHAR_ROOM, FALSE, MAG_AFFECTS,
  "Your giant-like strength fades.");

  spello(SPELL_ADRENALINE_SURGE, "adrenaline surge", 15, 15, 0, POS_STANDING,
  TAR_CHAR_ROOM | TAR_SELF_ONLY, FALSE, MAG_AFFECTS,
  "The adrenaline rush leaves your body.");

  spello(SPELL_CLARITY, "clarity", 20, 20, 0, POS_STANDING,
  TAR_CHAR_ROOM, FALSE, MAG_AFFECTS,
  "Your perfect clarity fades.");

  spello(SPELL_MARK_OF_DEATH, "mark of death", 25, 25, 0, POS_FIGHTING,
  TAR_CHAR_ROOM | TAR_FIGHT_VICT | TAR_NOT_SELF, TRUE, MAG_AFFECTS,
  "The Mark of Death fades from your flesh.");

  spello(SPELL_BLOODLUST, "bloodlust", 20, 20, 0, POS_STANDING,
  TAR_CHAR_ROOM | TAR_SELF_ONLY, FALSE, MAG_AFFECTS,
  "The bloodlust drains from you.");

  spello(SPELL_GREATER_HEAL, "greater heal", 40, 40, 0, POS_FIGHTING,
  TAR_CHAR_ROOM, FALSE, MAG_MANUAL, NULL);

  spello(SPELL_CLEANSE, "cleanse", 20, 20, 0, POS_STANDING,
  TAR_CHAR_ROOM, FALSE, MAG_MANUAL, NULL);

  spello(SPELL_DISRUPT, "disrupt", 18, 18, 0, POS_FIGHTING,
  TAR_CHAR_ROOM | TAR_FIGHT_VICT | TAR_NOT_SELF, TRUE, MAG_DAMAGE | MAG_AFFECTS,
  "Your concentration returns to normal.");

  spello(SPELL_ANTIMAGIC_SHELL, "antimagic shell", 30, 30, 0, POS_STANDING,
  TAR_CHAR_ROOM, FALSE, MAG_AFFECTS,
  "Your anti-magic shell dissolves.");

  spello(SPELL_ENCHANTERS_FOCUS, "enchanters focus", 25, 25, 0, POS_STANDING,
  TAR_CHAR_ROOM | TAR_SELF_ONLY, FALSE, MAG_AFFECTS,
  "Your arcane focus dissipates.");

  spello(SPELL_TIME_SNARE, "time snare", 20, 20, 0, POS_FIGHTING,
  TAR_CHAR_ROOM | TAR_FIGHT_VICT | TAR_NOT_SELF, TRUE, MAG_AFFECTS,
  "The temporal snare releases you.");

  spello(SPELL_PHASE_SHIFT, "phase shift", 25, 25, 0, POS_STANDING,
  TAR_CHAR_ROOM | TAR_SELF_ONLY, FALSE, MAG_AFFECTS,
  "You solidify back into the material plane.");

  spello(SPELL_MIRROR_VEIL, "mirror veil", 20, 20, 0, POS_STANDING,
  TAR_CHAR_ROOM, FALSE, MAG_AFFECTS,
  "Your mirror images wink out.");

  spello(SPELL_ELEMENTAL_WARD_FIRE, "elemental ward fire", 15, 15, 0, POS_STANDING,
  TAR_CHAR_ROOM, FALSE, MAG_AFFECTS,
  "Your fire ward fades.");

  spello(SPELL_ELEMENTAL_WARD_COLD, "elemental ward cold", 15, 15, 0, POS_STANDING,
  TAR_CHAR_ROOM, FALSE, MAG_AFFECTS,
  "Your cold ward fades.");

  spello(SPELL_ELEMENTAL_WARD_LIGHTNING, "elemental ward lightning", 15, 15, 0, POS_STANDING,
  TAR_CHAR_ROOM, FALSE, MAG_AFFECTS,
  "Your lightning ward fades.");

  spello(SPELL_ELEMENTAL_WARD_ACID, "elemental ward acid", 15, 15, 0, POS_STANDING,
  TAR_CHAR_ROOM, FALSE, MAG_AFFECTS,
  "Your acid ward fades.");

  spello(SPELL_COUNTERSPELL, "counterspell", 30, 30, 0, POS_FIGHTING,
  TAR_CHAR_ROOM | TAR_FIGHT_VICT | TAR_NOT_SELF, TRUE, MAG_MANUAL, NULL);

  spello(SPELL_SPELL_STEAL, "spell steal", 45, 45, 0, POS_FIGHTING,
  TAR_CHAR_ROOM | TAR_FIGHT_VICT | TAR_NOT_SELF, TRUE, MAG_MANUAL, NULL);

  spello(SPELL_CANCELLATION, "cancellation", 25, 25, 0, POS_FIGHTING,
  TAR_CHAR_ROOM | TAR_FIGHT_VICT | TAR_NOT_SELF, TRUE, MAG_MANUAL, NULL);

  spello(SPELL_HOLD_PERSON, "hold person", 20, 20, 0, POS_FIGHTING,
  TAR_CHAR_ROOM | TAR_FIGHT_VICT | TAR_NOT_SELF, TRUE, MAG_AFFECTS,
  "You can move freely again.");

  spello(SPELL_HOLD_MONSTER, "hold monster", 30, 30, 0, POS_FIGHTING,
  TAR_CHAR_ROOM | TAR_FIGHT_VICT | TAR_NOT_SELF, TRUE, MAG_AFFECTS,
  "The hold magic finally releases you.");

  spello(SPELL_CONFUSION, "confusion", 22, 22, 0, POS_FIGHTING,
  TAR_CHAR_ROOM | TAR_FIGHT_VICT | TAR_NOT_SELF, TRUE, MAG_AFFECTS,
  "Your thoughts settle back into order.");

  spello(SPELL_VERTIGO, "vertigo", 18, 18, 0, POS_FIGHTING,
  TAR_CHAR_ROOM | TAR_FIGHT_VICT | TAR_NOT_SELF, TRUE, MAG_AFFECTS,
  "The spinning sensation finally fades.");

  spello(SPELL_MASS_FEAR, "mass fear", 45, 45, 0, POS_FIGHTING,
  TAR_IGNORE, TRUE, MAG_MASSES, "Your terror loosens its grip.");

  spello(SPELL_NULL_FIELD, "null field", 60, 60, 0, POS_STANDING,
  TAR_IGNORE, FALSE, MAG_ROOMS, NULL);

  spello(SPELL_SILENCE_FIELD, "silence field", 40, 40, 0, POS_STANDING,
  TAR_IGNORE, FALSE, MAG_ROOMS, NULL);

  spello(SPELL_MIASMA, "miasma", 28, 28, 0, POS_FIGHTING,
  TAR_IGNORE, TRUE, MAG_AREAS, "The choking miasma finally clears.");

  spello(SPELL_TOXIC_CLOUD, "toxic cloud", 32, 32, 0, POS_FIGHTING,
  TAR_IGNORE, TRUE, MAG_AREAS, "You finally cough out the lingering poison gas.");

  spello(SPELL_WALL_OF_FIRE, "wall of fire", 40, 40, 0, POS_STANDING,
  TAR_IGNORE, TRUE, MAG_ROOMS, NULL);

  spello(SPELL_STATIC_FIELD, "static field", 35, 35, 0, POS_STANDING,
  TAR_IGNORE, TRUE, MAG_ROOMS, NULL);

  spello(SPELL_CONSECRATE, "consecrate", 40, 40, 0, POS_STANDING,
  TAR_IGNORE, FALSE, MAG_ROOMS, NULL);

  spello(SPELL_GRAVITY_WELL, "gravity well", 55, 55, 0, POS_FIGHTING,
  TAR_IGNORE, TRUE, MAG_AREAS | MAG_ROOMS, NULL);

  spello(SPELL_SHOCKWAVE, "shockwave", 35, 35, 0, POS_FIGHTING,
  TAR_IGNORE, TRUE, MAG_AREAS, NULL);

  spello(SPELL_NOVA, "nova", 40, 40, 0, POS_FIGHTING,
  TAR_IGNORE, TRUE, MAG_AREAS, NULL);

  spello(SPELL_ICE_STORM, "ice storm", 35, 35, 0, POS_FIGHTING,
  TAR_IGNORE, TRUE, MAG_AREAS, NULL);

  spello(SPELL_BLIZZARD, "blizzard", 55, 55, 0, POS_FIGHTING,
  TAR_IGNORE, TRUE, MAG_AREAS, NULL);

  spello(SPELL_FROST_NOVA, "frost nova", 40, 40, 0, POS_FIGHTING,
  TAR_IGNORE, TRUE, MAG_AREAS, NULL);

  spello(SPELL_FIREBALL_GREATER, "greater fireball", 45, 45, 0, POS_FIGHTING,
  TAR_IGNORE, TRUE, MAG_AREAS, NULL);

  spello(SPELL_ACID_RAIN, "acid rain", 38, 38, 0, POS_STANDING,
  TAR_IGNORE, TRUE, MAG_ROOMS, NULL);

  spello(SPELL_SONIC_BURST, "sonic burst", 28, 28, 0, POS_FIGHTING,
  TAR_IGNORE, TRUE, MAG_AREAS, NULL);

  spello(SPELL_WORD_OF_PAIN, "word of pain", 65, 65, 0, POS_FIGHTING,
  TAR_IGNORE, TRUE, MAG_AREAS, NULL);

  spello(SPELL_SENSE_LIFE, "sense life", 20, 10, 2, POS_STANDING,
  TAR_CHAR_ROOM | TAR_SELF_ONLY, FALSE, MAG_AFFECTS,
      "You feel less aware of your surroundings.");

  spello(SPELL_SHOCKING_GRASP, "shocking grasp", 30, 15, 3, POS_FIGHTING,
  TAR_CHAR_ROOM | TAR_FIGHT_VICT, TRUE, MAG_DAMAGE, NULL);

  spello(SPELL_SLEEP, "sleep", 40, 25, 5, POS_STANDING,
  TAR_CHAR_ROOM, TRUE, MAG_AFFECTS, "You feel less tired.");

  spello(SPELL_STRENGTH, "strength", 35, 30, 1, POS_STANDING,
  TAR_CHAR_ROOM, FALSE, MAG_AFFECTS, "You feel weaker.");

  spello(SPELL_SUMMON, "summon", 75, 50, 3, POS_STANDING,
  TAR_CHAR_WORLD | TAR_NOT_SELF, FALSE, MAG_MANUAL, NULL);

  spello(SPELL_TELEPORT, "teleport", 75, 50, 3, POS_STANDING,
  TAR_CHAR_ROOM, FALSE, MAG_MANUAL, NULL);

  spello(SPELL_BEAR_SPIRIT, "bear spirit", 40, 20, 2, POS_STANDING,
  TAR_CHAR_ROOM, FALSE, MAG_AFFECTS, "The spirit of the bear withdraws from you.");

  spello(SPELL_WOLF_SPIRIT, "wolf spirit", 40, 20, 2, POS_STANDING,
  TAR_CHAR_ROOM, FALSE, MAG_AFFECTS, "The keen focus of the wolf spirit fades.");

  spello(SPELL_TIGER_SPIRIT, "tiger spirit", 45, 25, 2, POS_STANDING,
  TAR_CHAR_ROOM, FALSE, MAG_AFFECTS, "The ferocity of the tiger spirit ebbs.");

  spello(SPELL_EAGLE_SPIRIT, "eagle spirit", 50, 25, 2, POS_STANDING,
  TAR_CHAR_ROOM, FALSE, MAG_AFFECTS, "The watchful eagle spirit drifts away.");

  spello(SPELL_DRAGON_SPIRIT, "dragon spirit", 60, 40, 2, POS_STANDING,
  TAR_CHAR_ROOM, FALSE, MAG_AFFECTS, "The presence of the dragon spirit withdraws.");

  spello(SPELL_WATERWALK, "waterwalk", 40, 20, 2, POS_STANDING,
  TAR_CHAR_ROOM, FALSE, MAG_AFFECTS, "Your feet seem less buoyant.");

  spello(SPELL_WORD_OF_RECALL, "word of recall", 20, 10, 2, POS_FIGHTING,
  TAR_CHAR_ROOM, FALSE, MAG_MANUAL, NULL);

  spello(SPELL_IDENTIFY, "identify", 50, 25, 5, POS_STANDING,
  TAR_CHAR_ROOM | TAR_OBJ_INV | TAR_OBJ_ROOM | TAR_OBJ_EQUIP, FALSE, MAG_MANUAL, NULL);

  spello(SPELL_CONJURE_ELEMENTAL, "conjure elemental", 60, 60, 0, POS_STANDING,
  TAR_IGNORE, FALSE, MAG_MANUAL, NULL);
  spello(SPELL_CALL_WOLVES, "call wolves", 35, 35, 0, POS_STANDING,
  TAR_IGNORE, FALSE, MAG_MANUAL, NULL);
  spello(SPELL_CALL_BEARS, "call bears", 50, 50, 0, POS_STANDING,
  TAR_IGNORE, FALSE, MAG_MANUAL, NULL);
  spello(SPELL_ANIMATE_DEAD_GREATER, "animate dead greater", 55, 55, 0, POS_STANDING,
  TAR_OBJ_ROOM, FALSE, MAG_MANUAL, NULL);
  spello(SPELL_ABYSS_GATE, "abyss gate", 80, 80, 0, POS_STANDING,
  TAR_IGNORE, TRUE, MAG_MANUAL, NULL);
  spello(SPELL_GATE, "gate", 100, 100, 0, POS_STANDING,
  TAR_IGNORE, FALSE, MAG_MANUAL, NULL);
  spello(SPELL_PORTAL, "portal", 50, 50, 0, POS_STANDING,
  TAR_CHAR_WORLD | TAR_NOT_SELF, FALSE, MAG_MANUAL, NULL);
  spello(SPELL_LOCATE_CORPSE, "locate corpse", 20, 20, 0, POS_STANDING,
  TAR_IGNORE, FALSE, MAG_MANUAL, NULL);
  spello(SPELL_WORD_OF_RECALL_MASS, "word of recall mass", 75, 75, 0, POS_STANDING,
  TAR_IGNORE, FALSE, MAG_MANUAL, NULL);
  spello(SPELL_ASTRAL_PROJECTION, "astral projection", 40, 40, 0, POS_STANDING,
  TAR_CHAR_WORLD | TAR_NOT_SELF, FALSE, MAG_MANUAL, NULL);
  spello(SPELL_ETHEREAL_JAUNT, "ethereal jaunt", 40, 40, 0, POS_STANDING,
  TAR_IGNORE, FALSE, MAG_MANUAL, NULL);
  spello(SPELL_LEYLINE_TAP, "leyline tap", 0, 0, 0, POS_STANDING,
  TAR_IGNORE, FALSE, MAG_MANUAL, NULL);
  spello(SPELL_TEMPORAL_SHIFT, "temporal shift", 40, 40, 0, POS_FIGHTING,
  TAR_CHAR_ROOM | TAR_FIGHT_SELF | TAR_FIGHT_VICT, TRUE, MAG_MANUAL, NULL);
  spello(SPELL_CHRONO_SHIFT, "chrono shift", 50, 50, 0, POS_FIGHTING,
  TAR_SELF_ONLY, FALSE, MAG_MANUAL, NULL);

  /* NON-castable spells should appear below here. */
  spello(SPELL_IDENTIFY, "identify", 0, 0, 0, 0,
  TAR_CHAR_ROOM | TAR_OBJ_INV | TAR_OBJ_ROOM | TAR_OBJ_EQUIP, FALSE, MAG_MANUAL, NULL);

  /* you might want to name this one something more fitting to your theme -Welcor*/
  spello(SPELL_DG_AFFECT, "Script-inflicted", 0, 0, 0, POS_SITTING,
  TAR_IGNORE, TRUE, 0, NULL);

  /* Declaration of skills - this actually doesn't do anything except set it up
   * so that immortals can use these skills by default.  The min level to use
   * the skill for other classes is set up in class.c. */
  skillo_cost(SKILL_BACKSTAB, "backstab", 20);
  skillo_cost(SKILL_BASH, "bash", 15);
  skillo_cost(SKILL_HIDE, "hide", 5);
  skillo_cost(SKILL_KICK, "kick", 10);
  skillo_cost(SKILL_PICK_LOCK, "pick lock", 5);
  skillo_cost(SKILL_RESCUE, "rescue", 10);
  skillo_cost(SKILL_SNEAK, "sneak", 5);
  skillo_cost(SKILL_STEAL, "steal", 5);
  skillo_cost(SKILL_TRACK, "track", 5);
  skillo_cost(SKILL_WHIRLWIND, "whirlwind", 20);
  skillo_cost(SKILL_BANDAGE, "bandage", 8);

  skillo_cost(SKILL_DUAL_WIELD, "dual wield", 0);
  skillo_cost(SKILL_RECALL, "recall", 0);
}
