/**
* @file spells.h
* Constants and function prototypes for the spell system.
*
* Part of the core tbaMUD source code distribution, which is a derivative
* of, and continuation of, CircleMUD.
*
* All rights reserved.  See license for complete information.
* Copyright (C) 1993, 94 by the Trustees of the Johns Hopkins University
* CircleMUD is based on DikuMUD, Copyright (C) 1990, 1991.
*/
#ifndef _SPELLS_H_
#define _SPELLS_H_

#define DEFAULT_STAFF_LVL	12
#define DEFAULT_WAND_LVL	12

#define CAST_UNDEFINED	(-1)
#define CAST_SPELL	0
#define CAST_POTION	1
#define CAST_WAND	2
#define CAST_STAFF	3
#define CAST_SCROLL	4

#define MAG_DAMAGE	(1 << 0)
#define MAG_AFFECTS	(1 << 1)
#define MAG_UNAFFECTS	(1 << 2)
#define MAG_POINTS	(1 << 3)
#define MAG_ALTER_OBJS	(1 << 4)
#define MAG_GROUPS	(1 << 5)
#define MAG_MASSES	(1 << 6)
#define MAG_AREAS	(1 << 7)
#define MAG_SUMMONS	(1 << 8)
#define MAG_CREATIONS	(1 << 9)
#define MAG_MANUAL	(1 << 10)
#define MAG_ROOMS   (1 << 11)

#define TYPE_UNDEFINED               (-1)
#define SPELL_RESERVED_DBC            0  /* SKILL NUMBER ZERO -- RESERVED */

/* PLAYER SPELLS -- Numbered from 1 to MAX_SPELLS */
#define SPELL_ARMOR                   1 /* Reserved Skill[] DO NOT CHANGE */
#define SPELL_TELEPORT                2 /* Reserved Skill[] DO NOT CHANGE */
#define SPELL_BLESS                   3 /* Reserved Skill[] DO NOT CHANGE */
#define SPELL_BLINDNESS               4 /* Reserved Skill[] DO NOT CHANGE */
#define SPELL_BURNING_HANDS           5 /* Reserved Skill[] DO NOT CHANGE */
#define SPELL_CALL_LIGHTNING          6 /* Reserved Skill[] DO NOT CHANGE */
#define SPELL_CHARM                   7 /* Reserved Skill[] DO NOT CHANGE */
#define SPELL_CHILL_TOUCH             8 /* Reserved Skill[] DO NOT CHANGE */
#define SPELL_CLONE                   9 /* Reserved Skill[] DO NOT CHANGE */
#define SPELL_COLOR_SPRAY            10 /* Reserved Skill[] DO NOT CHANGE */
#define SPELL_CONTROL_WEATHER        11 /* Reserved Skill[] DO NOT CHANGE */
#define SPELL_CREATE_FOOD            12 /* Reserved Skill[] DO NOT CHANGE */
#define SPELL_CREATE_WATER           13 /* Reserved Skill[] DO NOT CHANGE */
#define SPELL_CURE_BLIND             14 /* Reserved Skill[] DO NOT CHANGE */
#define SPELL_CURE_CRITIC            15 /* Reserved Skill[] DO NOT CHANGE */
#define SPELL_CURE_LIGHT             16 /* Reserved Skill[] DO NOT CHANGE */
#define SPELL_CURSE                  17 /* Reserved Skill[] DO NOT CHANGE */
#define SPELL_DETECT_ALIGN           18 /* Reserved Skill[] DO NOT CHANGE */
#define SPELL_DETECT_INVIS           19 /* Reserved Skill[] DO NOT CHANGE */
#define SPELL_DETECT_MAGIC           20 /* Reserved Skill[] DO NOT CHANGE */
#define SPELL_DETECT_POISON          21 /* Reserved Skill[] DO NOT CHANGE */
#define SPELL_DISPEL_EVIL            22 /* Reserved Skill[] DO NOT CHANGE */
#define SPELL_EARTHQUAKE             23 /* Reserved Skill[] DO NOT CHANGE */
#define SPELL_ENCHANT_WEAPON         24 /* Reserved Skill[] DO NOT CHANGE */
#define SPELL_ENERGY_DRAIN           25 /* Reserved Skill[] DO NOT CHANGE */
#define SPELL_FIREBALL               26 /* Reserved Skill[] DO NOT CHANGE */
#define SPELL_HARM                   27 /* Reserved Skill[] DO NOT CHANGE */
#define SPELL_HEAL                   28 /* Reserved Skill[] DO NOT CHANGE */
#define SPELL_INVISIBLE              29 /* Reserved Skill[] DO NOT CHANGE */
#define SPELL_LIGHTNING_BOLT         30 /* Reserved Skill[] DO NOT CHANGE */
#define SPELL_LOCATE_OBJECT          31 /* Reserved Skill[] DO NOT CHANGE */
#define SPELL_MAGIC_MISSILE          32 /* Reserved Skill[] DO NOT CHANGE */
#define SPELL_POISON                 33 /* Reserved Skill[] DO NOT CHANGE */
#define SPELL_PROT_FROM_EVIL         34 /* Reserved Skill[] DO NOT CHANGE */
#define SPELL_REMOVE_CURSE           35 /* Reserved Skill[] DO NOT CHANGE */
#define SPELL_SANCTUARY              36 /* Reserved Skill[] DO NOT CHANGE */
#define SPELL_SHOCKING_GRASP         37 /* Reserved Skill[] DO NOT CHANGE */
#define SPELL_SLEEP                  38 /* Reserved Skill[] DO NOT CHANGE */
#define SPELL_STRENGTH               39 /* Reserved Skill[] DO NOT CHANGE */
#define SPELL_SUMMON                 40 /* Reserved Skill[] DO NOT CHANGE */
#define SPELL_VENTRILOQUATE          41 /* Reserved Skill[] DO NOT CHANGE */
#define SPELL_WORD_OF_RECALL         42 /* Reserved Skill[] DO NOT CHANGE */
#define SPELL_REMOVE_POISON          43 /* Reserved Skill[] DO NOT CHANGE */
#define SPELL_SENSE_LIFE             44 /* Reserved Skill[] DO NOT CHANGE */
#define SPELL_ANIMATE_DEAD           45 /* Reserved Skill[] DO NOT CHANGE */
#define SPELL_DISPEL_GOOD            46 /* Reserved Skill[] DO NOT CHANGE */
#define SPELL_GROUP_ARMOR            47 /* Reserved Skill[] DO NOT CHANGE */
#define SPELL_GROUP_HEAL             48 /* Reserved Skill[] DO NOT CHANGE */
#define SPELL_GROUP_RECALL           49 /* Reserved Skill[] DO NOT CHANGE */
#define SPELL_INFRAVISION            50 /* Reserved Skill[] DO NOT CHANGE */
#define SPELL_WATERWALK              51 /* Reserved Skill[] DO NOT CHANGE */
#define SPELL_IDENTIFY               52 /* Reserved Skill[] DO NOT CHANGE */
#define SPELL_FLY                    53 /* Reserved Skill[] DO NOT CHANGE */
#define SPELL_DARKNESS               54
#define SPELL_CORRUPTION             55
#define SPELL_BEAR_SPIRIT            56
#define SPELL_WOLF_SPIRIT            57
#define SPELL_TIGER_SPIRIT           58
#define SPELL_EAGLE_SPIRIT           59
#define SPELL_DRAGON_SPIRIT          60
#define SPELL_ARCANE_WARD            61
#define SPELL_EVASION                62
#define SPELL_IRONSKIN               63
#define SPELL_DIVINE_BULWARK         64
#define SPELL_SONG_OF_RESILIENCE     65
#define SPELL_DARK_AEGIS             66
#define SPELL_NIRVANA                67
#define SPELL_PLAGUE_BOLT            68
#define SPELL_ENFEEBLEMENT           69
#define SPELL_DEVOUR_SOUL            70
#define SPELL_MEMENTO_MORI           71
#define SPELL_FIREBOLT               72
#define SPELL_FLAME_ARROW            73
#define SPELL_FROSTBITE              74
#define SPELL_VOLTAIC_BOLT           75
#define SPELL_ACID_BLAST             76
#define SPELL_SHADOW_BOLT            77
#define SPELL_VAMPIRIC_TOUCH         78
#define SPELL_WEB                    79
#define SPELL_SILENCE                80
#define SPELL_FEAR                   81
#define SPELL_TRUE_SEEING            82
#define SPELL_STONE_SKIN             83
#define SPELL_BARKSKIN               84
#define SPELL_GIANT_STRENGTH         85
#define SPELL_ADRENALINE_SURGE       86
#define SPELL_CLARITY                87
#define SPELL_MARK_OF_DEATH          88
#define SPELL_BLOODLUST              89
#define SPELL_GREATER_HEAL           90
#define SPELL_CLEANSE                91
#define SPELL_DISRUPT                92
#define SPELL_ANTIMAGIC_SHELL        93
#define SPELL_ENCHANTERS_FOCUS       94
#define SPELL_TIME_SNARE             95
#define SPELL_PHASE_SHIFT            96
#define SPELL_MIRROR_VEIL            97
#define SPELL_ELEMENTAL_WARD_FIRE    98
#define SPELL_ELEMENTAL_WARD_COLD    99
#define SPELL_ELEMENTAL_WARD_LIGHTNING 100
#define SPELL_ELEMENTAL_WARD_ACID    101
#define SPELL_COUNTERSPELL           102
#define SPELL_SPELL_STEAL            103
#define SPELL_CANCELLATION           104
#define SPELL_HOLD_PERSON            105
#define SPELL_HOLD_MONSTER           106
#define SPELL_CONFUSION              107
#define SPELL_VERTIGO                108
#define SPELL_MASS_FEAR              109
#define SPELL_NULL_FIELD             110
#define SPELL_SILENCE_FIELD          111
#define SPELL_MIASMA                 112
#define SPELL_TOXIC_CLOUD            113
#define SPELL_WALL_OF_FIRE           114
#define SPELL_STATIC_FIELD           115
#define SPELL_CONSECRATE             116
#define SPELL_GRAVITY_WELL           117
#define SPELL_SHOCKWAVE              118
#define SPELL_NOVA                   119
#define SPELL_ICE_STORM              120
#define SPELL_BLIZZARD               121
#define SPELL_FROST_NOVA             122
#define SPELL_FIREBALL_GREATER       123
#define SPELL_ACID_RAIN              124
#define SPELL_SONIC_BURST            125
#define SPELL_WORD_OF_PAIN           126
#define SPELL_CONJURE_ELEMENTAL      127
#define SPELL_CALL_WOLVES            128
#define SPELL_CALL_BEARS             129
#define SPELL_ANIMATE_DEAD_GREATER   130
#define SPELL_ABYSS_GATE             131
#define SPELL_GATE                   132
#define SPELL_PORTAL                 133
#define SPELL_LOCATE_CORPSE          134
#define SPELL_WORD_OF_RECALL_MASS    135
#define SPELL_ASTRAL_PROJECTION      136
#define SPELL_ETHEREAL_JAUNT         137
#define SPELL_LEYLINE_TAP            138
#define SPELL_TEMPORAL_SHIFT         139
#define SPELL_CHRONO_SHIFT           140
#define SPELL_BALEFIRE               141
#define SPELL_METEOR                 142
#define SPELL_METEOR_SWARM           143
#define SPELL_HELLFIRE               144
#define SPELL_WRATHFIRE              145
#define SPELL_CELESTIAL_SMITE        146
#define SPELL_HAMMER_OF_GOD          147
#define SPELL_DEATH_KNELL            148
#define SPELL_UNHOLY_WORD            149
#define SPELL_HOLY_WORD              150
#define SPELL_FINGER_OF_DEATH        151
#define SPELL_WAIL_OF_THE_BANSHEE    152
#define SPELL_DISINTEGRATE           153
#define SPELL_POWER_WORD_KILL        154
#define SPELL_POWER_WORD_STUN        155
#define SPELL_POWER_WORD_BLIND       156
#define SPELL_POWER_WORD_SILENCE     157
#define SPELL_PSYCHIC_CRUSH          158
#define SPELL_TIME_STOP              159
#define SPELL_BLACK_LANCE            160
#define SPELL_REALITY_SLASH          161
#define SPELL_GRASP_HEART            162
#define SPELL_NEGATIVE_BURST         163
#define SPELL_TRUE_DEATH             164
#define SPELL_PERFECT_UNKNOWABLE     165
#define SPELL_CRYSTAL_BODY           166
#define SPELL_GREATER_MAGIC_SEAL     167
#define SPELL_DESPAIR_AURA           168
#define SPELL_OBLIVION_SPEAR         169
#define SPELL_BONE_PRISON            170
#define SPELL_UNDYING_WILL           171
#define SPELL_DRAGON_LIGHTNING       172
#define SPELL_CHAIN_DRAGON_LIGHTNING 173
#define SPELL_HELL_FLAME             174
#define SPELL_GRAVITY_MAELSTROM      175
#define SPELL_CALL_GREATER_THUNDER   176
#define SPELL_ASTRAL_SMITE           177
#define SPELL_GREATER_REJECTION      178
#define SPELL_FALLEN_DOWN            179
#define SPELL_IA_SHUB_NIGGURATH      180
#define SPELL_GOAL_OF_ALL_LIFE_IS_DEATH 181
#define SPELL_CRY_OF_THE_BANSHEE     182
#define SPELL_NAPALM                 183
#define SPELL_BODY_OF_EFFULGENT_BERYL 184
#define SPELL_VERMILION_NOVA         185
#define SPELL_NUCLEAR_BLAST          186
#define SPELL_GREATER_TELEPORTATION  187
#define SPELL_SILENT_MAGIC           188
#define SPELL_TRIPLE_MAXIMIZE_MAGIC  189
#define SPELL_PANTHEON               190
#define SPELL_DIMENSIONAL_LOCK       191
#define SPELL_SHADOW_BIND            192
#define SPELL_SHADOW_EXCHANGE        193
#define SPELL_DAGGER_RAIN            194
#define SPELL_MONARCHS_PRESSURE      195
#define SPELL_SHADOW_DOMAIN          196
#define SPELL_FORCE_GRASP            197
#define SPELL_SHADOW_STEP            198
#define SPELL_BLACK_HEART            199
#define SPELL_CALL_SHADOW_LEGION     200
#define SPELL_NIGHT_HUNT             201
#define SPELL_DARK_REBUKE            202
#define SPELL_EXECUTION_MARK         203
#define SPELL_SHADOW_EXTRACTION      204
#define SPELL_ARISE_GREATER          205
#define SPELL_MONARCHS_AUTHORITY     206
#define SPELL_RULERS_HAND            207
#define SPELL_SHADOW_LANCE           208
#define SPELL_SHADOW_BURST           209
#define SPELL_SHADOW_STORM           210
#define SPELL_FATAL_STRIKE           211
#define SPELL_DOMINION_OF_SHADOWS    212
#define SPELL_SHADOW_RECALL          213
#define SPELL_SHADOW_REGENESIS       214
#define SPELL_ASSASSINS_INTENT       215
#define SPELL_BLOOD_DAGGER_TEMPEST   216
#define SPELL_CHAIN_OF_SUBJUGATION   217
#define SPELL_SOVEREIGNS_STEP        218
#define SPELL_KINGS_COMMAND          219
#define SPELL_DETECT_KILL_INTENT     220
#define SPELL_MUTILATE               221
#define SPELL_SHADOW_ARMOR           222
#define SPELL_TOTAL_OCCULTATION      223
#define SPELL_DOMAIN_BREAK           224
#define SPELL_HUNTERS_INSTINCT       225
/** Total Number of defined spells */
#define NUM_SPELLS                   225

/* Insert new spells here, up to MAX_SPELLS */
#define MAX_SPELLS		    230

/* PLAYER SKILLS - Numbered from MAX_SPELLS+1 to MAX_SKILLS */
#define SKILL_BACKSTAB              231 /* Reserved Skill[] DO NOT CHANGE */
#define SKILL_BASH                  232 /* Reserved Skill[] DO NOT CHANGE */
#define SKILL_HIDE                  233 /* Reserved Skill[] DO NOT CHANGE */
#define SKILL_KICK                  234 /* Reserved Skill[] DO NOT CHANGE */
#define SKILL_PICK_LOCK             235 /* Reserved Skill[] DO NOT CHANGE */
#define SKILL_WHIRLWIND             236 
#define SKILL_RESCUE                237 /* Reserved Skill[] DO NOT CHANGE */
#define SKILL_SNEAK                 238 /* Reserved Skill[] DO NOT CHANGE */
#define SKILL_STEAL                 239 /* Reserved Skill[] DO NOT CHANGE */
#define SKILL_TRACK                 240 /* Reserved Skill[] DO NOT CHANGE */
#define SKILL_BANDAGE               241 /* Reserved Skill[] DO NOT CHANGE */

#define SKILL_DUAL_WIELD            242 /* Reserved Skill[] DO NOT CHANGE */
#define SKILL_RECALL                243 /* Reserved Skill[] DO NOT CHANGE */
#define SKILL_OVERLORD_PRESENCE     244
#define SKILL_SUPREME_CASTER_DISCIPLINE 245
#define SKILL_UNDEAD_COMMAND        246
#define SKILL_TACTICAL_SPELL_MEMORY 247
#define SKILL_DREAD_DOMINION        248
#define SKILL_SHADOW_COMMANDER      249
#define SKILL_PREDATORS_ADVANCE     250
#define SKILL_MONARCH_REFLEXES      251
#define SKILL_RELENTLESS_HUNT       252
#define SKILL_SHADOW_RESERVOIR      253
#define SKILL_SHADOW_SURGE          254
#define SKILL_CHAIN_ASSASSAULT      255
#define SKILL_SOVEREIGN_PRESSURE    256
#define SKILL_KILL_WINDOW           257
#define SKILL_LEGION_MASTERY        258
#define SKILL_APPRAISE_ENEMY        259
#define SKILL_STUDY                 260
/* New skills may be added here up to MAX_SKILLS (see structs.h). */

/* NON-PLAYER AND OBJECT SPELLS AND SKILLS: The practice levels for the spells
 * and skills below are _not_ recorded in the players file; therefore, the
 * intended use is for spells and skills associated with objects (such as
 * SPELL_IDENTIFY used with scrolls of identify) or non-players (such as NPC
 * only spells). */

/* To make an affect induced by dg_affect look correct on 'stat' we need to
 * define it with a 'spellname'. */
#define SPELL_DG_AFFECT              298

#define TOP_SPELL_DEFINE	     299
/* NEW NPC/OBJECT SPELLS can be inserted here up to 299 */

/* WEAPON ATTACK TYPES */
#define TYPE_HIT        300
#define TYPE_STING      301
#define TYPE_WHIP       302
#define TYPE_SLASH      303
#define TYPE_BITE       304
#define TYPE_BLUDGEON   305
#define TYPE_CRUSH      306
#define TYPE_POUND      307
#define TYPE_CLAW       308
#define TYPE_MAUL       309
#define TYPE_THRASH     310
#define TYPE_PIERCE     311
#define TYPE_BLAST		  312
#define TYPE_PUNCH		  313
#define TYPE_STAB		    314
/** The total number of attack types */
#define NUM_ATTACK_TYPES  15

/* new attack types can be added here - up to TYPE_SUFFERING */
#define TYPE_SUFFERING		     399

#define SAVING_PARA   0
#define SAVING_ROD    1
#define SAVING_PETRI  2
#define SAVING_BREATH 3
#define SAVING_SPELL  4
#define SAVING_DEATH  SAVING_PARA

/* Reserved summon/portal VNUM constants for spell expansion. */
#define OBJVNUM_SPELL_PORTAL          9300
#define MOBVNUM_LESSER_ELEMENTAL      9301
#define MOBVNUM_ELEMENTAL             9302
#define MOBVNUM_GREATER_ELEMENTAL     9303
#define MOBVNUM_ELDER_ELEMENTAL       9304
#define MOBVNUM_SUMMONED_WOLF         9305
#define MOBVNUM_SUMMONED_BEAR         9306
#define MOBVNUM_LESSER_DEMON          9307
#define MOBVNUM_DEMON                 9308
#define MOBVNUM_GREATER_DEMON         9309
#define MOBVNUM_DEMON_LORD            9310
#define MOBVNUM_CELESTIAL_GUARDIAN    9311
#define MOBVNUM_ELEMENTAL_TITAN       9312
#define MOBVNUM_GREATER_UNDEAD        9313
#define MOBVNUM_SHADOW_SOLDIER        9314
#define MOBVNUM_GREATER_SHADOW        9315
#define MOBVNUM_SHADOW_ELITE          9316

/***
 **Possible Targets:
 **  TAR_IGNORE    : IGNORE TARGET.
 **  TAR_CHAR_ROOM : PC/NPC in room.
 **  TAR_CHAR_WORLD: PC/NPC in world.
 **  TAR_FIGHT_SELF: If fighting, and no argument, select tar_char as self.
 **  TAR_FIGHT_VICT: If fighting, and no argument, select tar_char as victim (fighting).
 **  TAR_SELF_ONLY : If no argument, select self, if argument check that it IS self.
 **  TAR_NOT_SELF  : Target is anyone else besides self.
 **  TAR_OBJ_INV   : Object in inventory.
 **  TAR_OBJ_ROOM  : Object in room.
 **  TAR_OBJ_WORLD : Object in world.
 **  TAR_OBJ_EQUIP : Object held.
 ***/
#define TAR_IGNORE      (1 << 0)
#define TAR_CHAR_ROOM   (1 << 1)
#define TAR_CHAR_WORLD  (1 << 2)
#define TAR_FIGHT_SELF  (1 << 3)
#define TAR_FIGHT_VICT  (1 << 4)
#define TAR_SELF_ONLY   (1 << 5) /* Only a check, use with i.e. TAR_CHAR_ROOM */
#define TAR_NOT_SELF   	(1 << 6) /* Only a check, use with i.e. TAR_CHAR_ROOM */
#define TAR_OBJ_INV     (1 << 7)
#define TAR_OBJ_ROOM    (1 << 8)
#define TAR_OBJ_WORLD   (1 << 9)
#define TAR_OBJ_EQUIP	  (1 << 10)

struct spell_info_type {
   byte min_position;	/* Position for caster	 */
   int mana_min;	/* Min amount of mana used by a spell (highest lev) */
   int mana_max;	/* Max amount of mana used by a spell (lowest lev) */
   int mana_change;	/* Change in mana used by spell from lev to lev */

   int min_level[MAX_CLASSES];
   int routines;
   byte violent;
   int targets;         /* See below for use with TAR_XXX  */
   const char *name;	/* Input size not limited. Originates from string constants. */
   const char *wear_off_msg;	/* Input size not limited. Originates from string constants. */
};

/* Possible Targets:
   bit 0 : IGNORE TARGET
   bit 1 : PC/NPC in room
   bit 2 : PC/NPC in world
   bit 3 : Object held
   bit 4 : Object in inventory
   bit 5 : Object in room
   bit 6 : Object in world
   bit 7 : If fighting, and no argument, select tar_char as self
   bit 8 : If fighting, and no argument, select tar_char as victim (fighting)
   bit 9 : If no argument, select self, if argument check that it IS self. */
#define SPELL_TYPE_SPELL   0
#define SPELL_TYPE_POTION  1
#define SPELL_TYPE_WAND    2
#define SPELL_TYPE_STAFF   3
#define SPELL_TYPE_SCROLL  4

#define ASPELL(spellname) \
void	spellname(int level, struct char_data *ch, \
		  struct char_data *victim, struct obj_data *obj)

#define MANUAL_SPELL(spellname)	spellname(level, caster, cvict, ovict);

enum identify_detail_level {
  IDENTIFY_BASIC = 0,
  IDENTIFY_FULL = 1
};

enum damage_type {
  DAM_NONE = 0,
  DAM_FIRE,
  DAM_COLD,
  DAM_LIGHTNING,
  DAM_ACID,
  DAM_NECROTIC,
  DAM_HOLY,
  DAM_SHADOW,
  DAM_ARCANE,
  DAM_FORCE,
  DAM_SONIC,
  DAM_POISON,
  DAM_PSYCHIC,
  DAM_EARTH
};

enum room_effect_type {
  ROOM_EFFECT_NONE = 0,
  ROOM_EFFECT_WALL_OF_FIRE,
  ROOM_EFFECT_SILENCE_FIELD,
  ROOM_EFFECT_NULL_FIELD,
  ROOM_EFFECT_CONSECRATE,
  ROOM_EFFECT_STATIC_FIELD,
  ROOM_EFFECT_TOXIC_CLOUD,
  ROOM_EFFECT_MIASMA,
  ROOM_EFFECT_GRAVITY_WELL,
  ROOM_EFFECT_ACID_RAIN,
  ROOM_EFFECT_DIMENSIONAL_LOCK,
  ROOM_EFFECT_SHADOW_DOMAIN,
  ROOM_EFFECT_SHADOW_STORM
};

ASPELL(spell_create_water);
ASPELL(spell_control_weather);
ASPELL(spell_ventriloquate);
ASPELL(spell_recall);
ASPELL(spell_teleport);
ASPELL(spell_summon);
ASPELL(spell_locate_object);
ASPELL(spell_charm);
ASPELL(spell_energy_drain);
ASPELL(spell_identify);
ASPELL(spell_enchant_weapon);
ASPELL(spell_detect_poison);
ASPELL(spell_corruption);
ASPELL(spell_plague_bolt);
ASPELL(spell_enfeeblement);
ASPELL(spell_devour_soul);
ASPELL(spell_memento_mori);
ASPELL(spell_vampiric_touch);
ASPELL(spell_greater_heal);
ASPELL(spell_cleanse);
ASPELL(spell_counterspell);
ASPELL(spell_spell_steal);
ASPELL(spell_cancellation);
ASPELL(spell_conjure_elemental);
ASPELL(spell_call_wolves);
ASPELL(spell_call_bears);
ASPELL(spell_animate_dead_greater);
ASPELL(spell_abyss_gate);
ASPELL(spell_gate);
ASPELL(spell_portal);
ASPELL(spell_locate_corpse);
ASPELL(spell_word_of_recall_mass);
ASPELL(spell_astral_projection);
ASPELL(spell_ethereal_jaunt);
ASPELL(spell_leyline_tap);
ASPELL(spell_temporal_shift);
ASPELL(spell_chrono_shift);
ASPELL(spell_balefire);
ASPELL(spell_meteor);
ASPELL(spell_meteor_swarm);
ASPELL(spell_hellfire);
ASPELL(spell_wrathfire);
ASPELL(spell_celestial_smite);
ASPELL(spell_hammer_of_god);
ASPELL(spell_death_knell);
ASPELL(spell_unholy_word);
ASPELL(spell_holy_word);
ASPELL(spell_finger_of_death);
ASPELL(spell_wail_of_the_banshee);
ASPELL(spell_disintegrate);
ASPELL(spell_power_word_kill);
ASPELL(spell_power_word_stun);
ASPELL(spell_power_word_blind);
ASPELL(spell_power_word_silence);
ASPELL(spell_psychic_crush);
ASPELL(spell_time_stop);
ASPELL(spell_black_lance);
ASPELL(spell_reality_slash);
ASPELL(spell_grasp_heart);
ASPELL(spell_negative_burst);
ASPELL(spell_true_death);
ASPELL(spell_perfect_unknowable);
ASPELL(spell_crystal_body);
ASPELL(spell_greater_magic_seal);
ASPELL(spell_despair_aura);
ASPELL(spell_oblivion_spear);
ASPELL(spell_bone_prison);
ASPELL(spell_undying_will);
ASPELL(spell_dragon_lightning);
ASPELL(spell_chain_dragon_lightning);
ASPELL(spell_hell_flame);
ASPELL(spell_gravity_maelstrom);
ASPELL(spell_call_greater_thunder);
ASPELL(spell_astral_smite);
ASPELL(spell_greater_rejection);
ASPELL(spell_fallen_down);
ASPELL(spell_ia_shub_niggurath);
ASPELL(spell_goal_of_all_life_is_death);
ASPELL(spell_cry_of_the_banshee);
ASPELL(spell_napalm);
ASPELL(spell_body_of_effulgent_beryl);
ASPELL(spell_vermilion_nova);
ASPELL(spell_nuclear_blast);
ASPELL(spell_greater_teleportation);
ASPELL(spell_silent_magic);
ASPELL(spell_triple_maximize_magic);
ASPELL(spell_pantheon);
ASPELL(spell_dimensional_lock);
ASPELL(spell_shadow_bind);
ASPELL(spell_shadow_exchange);
ASPELL(spell_dagger_rain);
ASPELL(spell_monarchs_pressure);
ASPELL(spell_shadow_domain);
ASPELL(spell_force_grasp);
ASPELL(spell_shadow_step);
ASPELL(spell_black_heart);
ASPELL(spell_call_shadow_legion);
ASPELL(spell_night_hunt);
ASPELL(spell_dark_rebuke);
ASPELL(spell_execution_mark);
ASPELL(spell_shadow_extraction);
ASPELL(spell_arise_greater);
ASPELL(spell_monarchs_authority);
ASPELL(spell_rulers_hand);
ASPELL(spell_shadow_lance);
ASPELL(spell_shadow_burst);
ASPELL(spell_shadow_storm);
ASPELL(spell_fatal_strike);
ASPELL(spell_dominion_of_shadows);
ASPELL(spell_shadow_recall);
ASPELL(spell_shadow_regenesis);
ASPELL(spell_assassins_intent);
ASPELL(spell_blood_dagger_tempest);
ASPELL(spell_chain_of_subjugation);
ASPELL(spell_sovereigns_step);
ASPELL(spell_kings_command);
ASPELL(spell_detect_kill_intent);
ASPELL(spell_mutilate);
ASPELL(spell_shadow_armor);

int summon_stored_shadow(struct char_data *ch, int slot);
int shadow_active_count(struct char_data *ch);
int shadow_max_active_count(struct char_data *ch);
bool shadow_can_summon_more(struct char_data *ch, char *reason, size_t reason_sz);
ASPELL(spell_total_occultation);
ASPELL(spell_domain_break);
ASPELL(spell_hunters_instinct);
void show_identify_item(struct char_data *ch, struct obj_data *obj, enum identify_detail_level detail);

/* basic magic calling functions */

int find_skill_num(char *name);
int find_skill_num_with_ambig(const char *name, char *ambig_buf,
    size_t ambig_len);

int mag_damage(int level, struct char_data *ch, struct char_data *victim,
  int spellnum, int savetype);

void mag_affects(int level, struct char_data *ch, struct char_data *victim,
  int spellnum, int savetype);
bool is_spirit_spell(int spellnum);
bool can_bind_spirit(struct char_data *ch, int spellnum);
int mystic_spirit_cap(struct char_data *ch);

void mag_groups(int level, struct char_data *ch, int spellnum, int savetype);

void mag_masses(int level, struct char_data *ch, int spellnum, int savetype);

void mag_areas(int level, struct char_data *ch, int spellnum, int savetype);

void mag_rooms(int level, struct char_data *ch, int spellnum);

void mag_summons(int level, struct char_data *ch, struct obj_data *obj,
 int spellnum, int savetype);

int shadow_return_active_to_storage(struct char_data *owner, int quiet_mode);
void handle_followers_after_owner_teleport_or_recall(struct char_data *ch);

void mag_points(int level, struct char_data *ch, struct char_data *victim,
 int spellnum, int savetype);

void mag_unaffects(int level, struct char_data *ch, struct char_data *victim,
  int spellnum, int type);

void mag_alter_objs(int level, struct char_data *ch, struct obj_data *obj,
  int spellnum, int type);

void mag_creations(int level, struct char_data *ch, int spellnum);

int	call_magic(struct char_data *caster, struct char_data *cvict,
  struct obj_data *ovict, int spellnum, int level, int casttype);
void set_spell_damage_type(enum damage_type type);
int spell_on_cooldown(struct char_data *ch, int spellnum);
void set_spell_cooldown(struct char_data *ch, int spellnum, int rounds);
void tick_spell_cooldowns(struct char_data *ch);

int room_has_effect(struct room_data *room, int effect_type);
void room_add_effect(struct room_data *room, int effect_type, int duration, int modifier);
void room_tick_effects(struct room_data *room);

void set_temp_summon_timer(struct char_data *mob, int rounds);

void	mag_objectmagic(struct char_data *ch, struct obj_data *obj,
			char *argument);
int player_knows_identified_item(struct char_data *ch, struct obj_data *obj);
void player_record_identified_item(struct char_data *ch, struct obj_data *obj);

int	cast_spell(struct char_data *ch, struct char_data *tch,
  struct obj_data *tobj, int spellnum);

/* other prototypes */
void spell_level(int spell, int chclass, int level);
void init_spell_levels(void);
const char *skill_name(int num);

/* From magic.c */
int mag_savingthrow(struct char_data *ch, int type, int modifier);
void affect_update(void);
bool is_sanctuary_spell(int spellnum);

/* from spell_parser.c */
ACMD(do_cast);
ACMD(do_spellup);
void unused_spell(int spl);
void mag_assign_spells(void);

/* Global variables */
extern struct spell_info_type spell_info[];
extern char cast_arg2[];
extern const char *unused_spellname;

#endif /* _SPELLS_H_ */

/* Ability type helpers */
#define FIRST_SKILL   SKILL_BACKSTAB
#define IS_SKILL(num) ((num) >= FIRST_SKILL)
#define IS_SPELL(num) ((num) > 0 && (num) < FIRST_SKILL)
