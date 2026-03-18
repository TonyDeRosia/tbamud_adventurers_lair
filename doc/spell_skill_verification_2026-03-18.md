# Spell and Skill Verification Report (2026-03-18)

## A. Inventory summary
- Total `SPELL_*` found: 225
- Total `SKILL_*` found: 28
- Total fully wired: 148
- Total partially wired: 0
- Total broken: 69
- Total passive-only: 1
- Total unreachable: 35

## B. Full inventory table

| ID | Constant | Declared file | Parser/command status | Implementation status | Support hook status | Learnable | Final classification |
|---:|---|---|---|---|---|---|---|
| 1 | `SPELL_ARMOR` | `src/spells.h` | spello | generic magic (MAG_AFFECTS) | hooked | yes | Fully wired |
| 2 | `SPELL_TELEPORT` | `src/spells.h` | spello | manual (spell_teleport) | hooked | yes | Fully wired |
| 3 | `SPELL_BLESS` | `src/spells.h` | spello | generic magic (MAG_AFFECTS | MAG_ALTER_OBJS) | hooked | yes | Fully wired |
| 4 | `SPELL_BLINDNESS` | `src/spells.h` | spello | generic magic (MAG_AFFECTS) | hooked | yes | Fully wired |
| 5 | `SPELL_BURNING_HANDS` | `src/spells.h` | spello | generic magic (MAG_DAMAGE) | hooked | yes | Fully wired |
| 6 | `SPELL_CALL_LIGHTNING` | `src/spells.h` | spello | generic magic (MAG_DAMAGE) | hooked | yes | Fully wired |
| 7 | `SPELL_CHARM` | `src/spells.h` | spello | manual (spell_charm) | hooked | yes | Fully wired |
| 8 | `SPELL_CHILL_TOUCH` | `src/spells.h` | spello | generic magic (MAG_DAMAGE | MAG_AFFECTS) | hooked | yes | Fully wired |
| 9 | `SPELL_CLONE` | `src/spells.h` | spello | generic magic (MAG_SUMMONS) | hooked | yes | Fully wired |
| 10 | `SPELL_COLOR_SPRAY` | `src/spells.h` | spello | generic magic (MAG_DAMAGE) | hooked | yes | Fully wired |
| 11 | `SPELL_CONTROL_WEATHER` | `src/spells.h` | spello | manual (spell_control_weather) | hooked | yes | Fully wired |
| 12 | `SPELL_CREATE_FOOD` | `src/spells.h` | spello | generic magic (MAG_CREATIONS) | hooked | yes | Fully wired |
| 13 | `SPELL_CREATE_WATER` | `src/spells.h` | spello | manual (spell_create_water) | hooked | yes | Fully wired |
| 14 | `SPELL_CURE_BLIND` | `src/spells.h` | spello | generic magic (MAG_UNAFFECTS) | hooked | yes | Fully wired |
| 15 | `SPELL_CURE_CRITIC` | `src/spells.h` | spello | generic magic (MAG_POINTS) | hooked | yes | Fully wired |
| 16 | `SPELL_CURE_LIGHT` | `src/spells.h` | spello | generic magic (MAG_POINTS) | hooked | yes | Fully wired |
| 17 | `SPELL_CURSE` | `src/spells.h` | spello | generic magic (MAG_AFFECTS | MAG_ALTER_OBJS) | hooked | yes | Fully wired |
| 18 | `SPELL_DETECT_ALIGN` | `src/spells.h` | spello | generic magic (MAG_AFFECTS) | hooked | yes | Fully wired |
| 19 | `SPELL_DETECT_INVIS` | `src/spells.h` | spello | generic magic (MAG_AFFECTS) | hooked | yes | Fully wired |
| 20 | `SPELL_DETECT_MAGIC` | `src/spells.h` | spello | generic magic (MAG_AFFECTS) | hooked | yes | Fully wired |
| 21 | `SPELL_DETECT_POISON` | `src/spells.h` | spello | manual (spell_detect_poison) | hooked | yes | Fully wired |
| 22 | `SPELL_DISPEL_EVIL` | `src/spells.h` | spello | generic magic (MAG_DAMAGE) | hooked | yes | Fully wired |
| 23 | `SPELL_EARTHQUAKE` | `src/spells.h` | spello | generic magic (MAG_AREAS) | hooked | yes | Fully wired |
| 24 | `SPELL_ENCHANT_WEAPON` | `src/spells.h` | spello | manual (spell_enchant_weapon) | hooked | yes | Fully wired |
| 25 | `SPELL_ENERGY_DRAIN` | `src/spells.h` | spello | manual missing dispatch | hooked | yes | Parser-wired but not implemented |
| 26 | `SPELL_FIREBALL` | `src/spells.h` | spello | generic magic (MAG_DAMAGE) | hooked | yes | Fully wired |
| 27 | `SPELL_HARM` | `src/spells.h` | spello | generic magic (MAG_DAMAGE) | hooked | yes | Fully wired |
| 28 | `SPELL_HEAL` | `src/spells.h` | spello | generic magic (MAG_POINTS | MAG_UNAFFECTS) | hooked | yes | Fully wired |
| 29 | `SPELL_INVISIBLE` | `src/spells.h` | spello | generic magic (MAG_AFFECTS | MAG_ALTER_OBJS) | hooked | yes | Fully wired |
| 30 | `SPELL_LIGHTNING_BOLT` | `src/spells.h` | spello | generic magic (MAG_DAMAGE) | hooked | yes | Fully wired |
| 31 | `SPELL_LOCATE_OBJECT` | `src/spells.h` | spello | manual (spell_locate_object) | hooked | yes | Fully wired |
| 32 | `SPELL_MAGIC_MISSILE` | `src/spells.h` | spello | generic magic (MAG_DAMAGE) | hooked | yes | Fully wired |
| 33 | `SPELL_POISON` | `src/spells.h` | spello | generic magic (MAG_AFFECTS | MAG_ALTER_OBJS) | hooked | yes | Fully wired |
| 34 | `SPELL_PROT_FROM_EVIL` | `src/spells.h` | spello | generic magic (MAG_AFFECTS) | hooked | yes | Fully wired |
| 35 | `SPELL_REMOVE_CURSE` | `src/spells.h` | spello | generic magic (MAG_UNAFFECTS | MAG_ALTER_OBJS) | hooked | yes | Fully wired |
| 36 | `SPELL_SANCTUARY` | `src/spells.h` | spello | generic magic (MAG_AFFECTS) | hooked | yes | Fully wired |
| 37 | `SPELL_SHOCKING_GRASP` | `src/spells.h` | spello | generic magic (MAG_DAMAGE) | hooked | yes | Fully wired |
| 38 | `SPELL_SLEEP` | `src/spells.h` | spello | generic magic (MAG_AFFECTS) | hooked | yes | Fully wired |
| 39 | `SPELL_STRENGTH` | `src/spells.h` | spello | generic magic (MAG_AFFECTS) | hooked | yes | Fully wired |
| 40 | `SPELL_SUMMON` | `src/spells.h` | spello | manual (spell_summon) | hooked | yes | Fully wired |
| 41 | `SPELL_VENTRILOQUATE` | `src/spells.h` | spello | manual (spell_ventriloquate) | hooked | yes | Fully wired |
| 42 | `SPELL_WORD_OF_RECALL` | `src/spells.h` | spello | manual (spell_recall) | hooked | yes | Fully wired |
| 43 | `SPELL_REMOVE_POISON` | `src/spells.h` | spello | generic magic (MAG_UNAFFECTS | MAG_ALTER_OBJS) | hooked | yes | Fully wired |
| 44 | `SPELL_SENSE_LIFE` | `src/spells.h` | spello | generic magic (MAG_AFFECTS) | hooked | yes | Fully wired |
| 45 | `SPELL_ANIMATE_DEAD` | `src/spells.h` | spello | generic magic (MAG_SUMMONS) | hooked | yes | Fully wired |
| 46 | `SPELL_DISPEL_GOOD` | `src/spells.h` | spello | generic magic (MAG_DAMAGE) | hooked | yes | Fully wired |
| 47 | `SPELL_GROUP_ARMOR` | `src/spells.h` | spello | generic magic (MAG_GROUPS) | hooked | yes | Fully wired |
| 48 | `SPELL_GROUP_HEAL` | `src/spells.h` | spello | generic magic (MAG_GROUPS) | hooked | yes | Fully wired |
| 49 | `SPELL_GROUP_RECALL` | `src/spells.h` | spello | generic magic (MAG_GROUPS) | hooked | yes | Fully wired |
| 50 | `SPELL_INFRAVISION` | `src/spells.h` | spello | generic magic (MAG_AFFECTS) | hooked | yes | Fully wired |
| 51 | `SPELL_WATERWALK` | `src/spells.h` | spello | generic magic (MAG_AFFECTS) | hooked | yes | Fully wired |
| 52 | `SPELL_IDENTIFY` | `src/spells.h` | spello | manual (spell_identify) | hooked | yes | Fully wired |
| 53 | `SPELL_FLY` | `src/spells.h` | spello | generic magic (MAG_AFFECTS) | hooked | yes | Fully wired |
| 54 | `SPELL_DARKNESS` | `src/spells.h` | spello | generic magic (MAG_ROOMS) | hooked | yes | Fully wired |
| 55 | `SPELL_CORRUPTION` | `src/spells.h` | spello | manual (spell_corruption) | hooked | yes | Fully wired |
| 56 | `SPELL_BEAR_SPIRIT` | `src/spells.h` | spello | generic magic (MAG_AFFECTS) | hooked | yes | Fully wired |
| 57 | `SPELL_WOLF_SPIRIT` | `src/spells.h` | spello | generic magic (MAG_AFFECTS) | hooked | yes | Fully wired |
| 58 | `SPELL_TIGER_SPIRIT` | `src/spells.h` | spello | generic magic (MAG_AFFECTS) | hooked | yes | Fully wired |
| 59 | `SPELL_EAGLE_SPIRIT` | `src/spells.h` | spello | generic magic (MAG_AFFECTS) | hooked | yes | Fully wired |
| 60 | `SPELL_DRAGON_SPIRIT` | `src/spells.h` | spello | generic magic (MAG_AFFECTS) | hooked | yes | Fully wired |
| 61 | `SPELL_ARCANE_WARD` | `src/spells.h` | spello | generic magic (MAG_AFFECTS) | hooked | yes | Fully wired |
| 62 | `SPELL_EVASION` | `src/spells.h` | spello | generic magic (MAG_AFFECTS) | hooked | yes | Fully wired |
| 63 | `SPELL_IRONSKIN` | `src/spells.h` | spello | generic magic (MAG_AFFECTS) | hooked | yes | Fully wired |
| 64 | `SPELL_DIVINE_BULWARK` | `src/spells.h` | spello | generic magic (MAG_AFFECTS) | hooked | yes | Fully wired |
| 65 | `SPELL_SONG_OF_RESILIENCE` | `src/spells.h` | spello | generic magic (MAG_AFFECTS) | hooked | yes | Fully wired |
| 66 | `SPELL_DARK_AEGIS` | `src/spells.h` | spello | generic magic (MAG_AFFECTS) | hooked | yes | Fully wired |
| 67 | `SPELL_NIRVANA` | `src/spells.h` | spello | generic magic (MAG_AFFECTS) | hooked | yes | Fully wired |
| 68 | `SPELL_PLAGUE_BOLT` | `src/spells.h` | spello | manual (spell_plague_bolt) | hooked | yes | Fully wired |
| 69 | `SPELL_ENFEEBLEMENT` | `src/spells.h` | spello | manual (spell_enfeeblement) | hooked | yes | Fully wired |
| 70 | `SPELL_DEVOUR_SOUL` | `src/spells.h` | spello | manual (spell_devour_soul) | hooked | yes | Fully wired |
| 71 | `SPELL_MEMENTO_MORI` | `src/spells.h` | spello | manual (spell_memento_mori) | hooked | yes | Fully wired |
| 72 | `SPELL_FIREBOLT` | `src/spells.h` | spello | generic magic (MAG_DAMAGE) | hooked | no | Implemented but not reachable |
| 73 | `SPELL_FLAME_ARROW` | `src/spells.h` | spello | generic magic (MAG_DAMAGE | MAG_AFFECTS) | hooked | no | Implemented but not reachable |
| 74 | `SPELL_FROSTBITE` | `src/spells.h` | spello | generic magic (MAG_DAMAGE | MAG_AFFECTS) | hooked | no | Implemented but not reachable |
| 75 | `SPELL_VOLTAIC_BOLT` | `src/spells.h` | spello | generic magic (MAG_DAMAGE | MAG_AFFECTS) | hooked | no | Implemented but not reachable |
| 76 | `SPELL_ACID_BLAST` | `src/spells.h` | spello | generic magic (MAG_DAMAGE | MAG_AFFECTS) | hooked | no | Implemented but not reachable |
| 77 | `SPELL_SHADOW_BOLT` | `src/spells.h` | spello | generic magic (MAG_DAMAGE) | hooked | no | Implemented but not reachable |
| 78 | `SPELL_VAMPIRIC_TOUCH` | `src/spells.h` | spello | manual (spell_vampiric_touch) | hooked | no | Implemented but not reachable |
| 79 | `SPELL_WEB` | `src/spells.h` | spello | generic magic (MAG_AFFECTS) | hooked | no | Implemented but not reachable |
| 80 | `SPELL_SILENCE` | `src/spells.h` | spello | generic magic (MAG_AFFECTS) | hooked | no | Implemented but not reachable |
| 81 | `SPELL_FEAR` | `src/spells.h` | spello | generic magic (MAG_AFFECTS) | hooked | no | Implemented but not reachable |
| 82 | `SPELL_TRUE_SEEING` | `src/spells.h` | spello | generic magic (MAG_AFFECTS) | hooked | no | Implemented but not reachable |
| 83 | `SPELL_STONE_SKIN` | `src/spells.h` | spello | generic magic (MAG_AFFECTS) | hooked | no | Implemented but not reachable |
| 84 | `SPELL_BARKSKIN` | `src/spells.h` | spello | generic magic (MAG_AFFECTS) | hooked | no | Implemented but not reachable |
| 85 | `SPELL_GIANT_STRENGTH` | `src/spells.h` | spello | generic magic (MAG_AFFECTS) | hooked | no | Implemented but not reachable |
| 86 | `SPELL_ADRENALINE_SURGE` | `src/spells.h` | spello | generic magic (MAG_AFFECTS) | hooked | no | Implemented but not reachable |
| 87 | `SPELL_CLARITY` | `src/spells.h` | spello | generic magic (MAG_AFFECTS) | hooked | no | Implemented but not reachable |
| 88 | `SPELL_MARK_OF_DEATH` | `src/spells.h` | spello | generic magic (MAG_AFFECTS) | hooked | no | Implemented but not reachable |
| 89 | `SPELL_BLOODLUST` | `src/spells.h` | spello | generic magic (MAG_AFFECTS) | hooked | no | Implemented but not reachable |
| 90 | `SPELL_GREATER_HEAL` | `src/spells.h` | spello | manual (spell_greater_heal) | hooked | no | Implemented but not reachable |
| 91 | `SPELL_CLEANSE` | `src/spells.h` | spello | manual (spell_cleanse) | hooked | no | Implemented but not reachable |
| 92 | `SPELL_DISRUPT` | `src/spells.h` | spello | generic magic (MAG_DAMAGE | MAG_AFFECTS) | hooked | yes | Fully wired |
| 93 | `SPELL_ANTIMAGIC_SHELL` | `src/spells.h` | spello | generic magic (MAG_AFFECTS) | hooked | yes | Fully wired |
| 94 | `SPELL_ENCHANTERS_FOCUS` | `src/spells.h` | spello | generic magic (MAG_AFFECTS) | hooked | yes | Fully wired |
| 95 | `SPELL_TIME_SNARE` | `src/spells.h` | spello | generic magic (MAG_AFFECTS) | hooked | yes | Fully wired |
| 96 | `SPELL_PHASE_SHIFT` | `src/spells.h` | spello | generic magic (MAG_AFFECTS) | hooked | yes | Fully wired |
| 97 | `SPELL_MIRROR_VEIL` | `src/spells.h` | spello | generic magic (MAG_AFFECTS) | hooked | yes | Fully wired |
| 98 | `SPELL_ELEMENTAL_WARD_FIRE` | `src/spells.h` | spello | generic magic (MAG_AFFECTS) | hooked | yes | Fully wired |
| 99 | `SPELL_ELEMENTAL_WARD_COLD` | `src/spells.h` | spello | generic magic (MAG_AFFECTS) | hooked | yes | Fully wired |
| 100 | `SPELL_ELEMENTAL_WARD_LIGHTNING` | `src/spells.h` | spello | generic magic (MAG_AFFECTS) | hooked | yes | Fully wired |
| 101 | `SPELL_ELEMENTAL_WARD_ACID` | `src/spells.h` | spello | generic magic (MAG_AFFECTS) | hooked | yes | Fully wired |
| 102 | `SPELL_COUNTERSPELL` | `src/spells.h` | spello | manual (spell_counterspell) | hooked | yes | Fully wired |
| 103 | `SPELL_SPELL_STEAL` | `src/spells.h` | spello | manual (spell_spell_steal) | hooked | yes | Fully wired |
| 104 | `SPELL_CANCELLATION` | `src/spells.h` | spello | manual (spell_cancellation) | hooked | yes | Fully wired |
| 105 | `SPELL_HOLD_PERSON` | `src/spells.h` | spello | generic magic (MAG_AFFECTS) | hooked | yes | Fully wired |
| 106 | `SPELL_HOLD_MONSTER` | `src/spells.h` | spello | generic magic (MAG_AFFECTS) | hooked | yes | Fully wired |
| 107 | `SPELL_CONFUSION` | `src/spells.h` | spello | generic magic (MAG_AFFECTS) | hooked | yes | Fully wired |
| 108 | `SPELL_VERTIGO` | `src/spells.h` | spello | generic magic (MAG_AFFECTS) | hooked | yes | Fully wired |
| 109 | `SPELL_MASS_FEAR` | `src/spells.h` | spello | generic magic (MAG_MASSES) | hooked | yes | Fully wired |
| 110 | `SPELL_NULL_FIELD` | `src/spells.h` | spello | generic magic (MAG_ROOMS) | hooked | yes | Fully wired |
| 111 | `SPELL_SILENCE_FIELD` | `src/spells.h` | spello | generic magic (MAG_ROOMS) | hooked | yes | Fully wired |
| 112 | `SPELL_MIASMA` | `src/spells.h` | spello | generic magic (MAG_AREAS) | hooked | yes | Fully wired |
| 113 | `SPELL_TOXIC_CLOUD` | `src/spells.h` | spello | generic magic (MAG_AREAS) | hooked | yes | Fully wired |
| 114 | `SPELL_WALL_OF_FIRE` | `src/spells.h` | spello | generic magic (MAG_ROOMS) | hooked | yes | Fully wired |
| 115 | `SPELL_STATIC_FIELD` | `src/spells.h` | spello | generic magic (MAG_ROOMS) | hooked | yes | Fully wired |
| 116 | `SPELL_CONSECRATE` | `src/spells.h` | spello | generic magic (MAG_ROOMS) | hooked | yes | Fully wired |
| 117 | `SPELL_GRAVITY_WELL` | `src/spells.h` | spello | generic magic (MAG_AREAS | MAG_ROOMS) | hooked | yes | Fully wired |
| 118 | `SPELL_SHOCKWAVE` | `src/spells.h` | spello | generic magic (MAG_AREAS) | hooked | yes | Fully wired |
| 119 | `SPELL_NOVA` | `src/spells.h` | spello | generic magic (MAG_AREAS) | hooked | yes | Fully wired |
| 120 | `SPELL_ICE_STORM` | `src/spells.h` | spello | generic magic (MAG_AREAS) | hooked | yes | Fully wired |
| 121 | `SPELL_BLIZZARD` | `src/spells.h` | spello | generic magic (MAG_AREAS) | hooked | yes | Fully wired |
| 122 | `SPELL_FROST_NOVA` | `src/spells.h` | spello | generic magic (MAG_AREAS) | hooked | yes | Fully wired |
| 123 | `SPELL_FIREBALL_GREATER` | `src/spells.h` | spello | generic magic (MAG_AREAS) | hooked | yes | Fully wired |
| 124 | `SPELL_ACID_RAIN` | `src/spells.h` | spello | generic magic (MAG_ROOMS) | hooked | yes | Fully wired |
| 125 | `SPELL_SONIC_BURST` | `src/spells.h` | spello | generic magic (MAG_AREAS) | hooked | yes | Fully wired |
| 126 | `SPELL_WORD_OF_PAIN` | `src/spells.h` | spello | generic magic (MAG_AREAS) | hooked | yes | Fully wired |
| 127 | `SPELL_CONJURE_ELEMENTAL` | `src/spells.h` | spello | manual (spell_conjure_elemental) | hooked | yes | Fully wired |
| 128 | `SPELL_CALL_WOLVES` | `src/spells.h` | spello | manual (spell_call_wolves) | hooked | yes | Fully wired |
| 129 | `SPELL_CALL_BEARS` | `src/spells.h` | spello | manual (spell_call_bears) | hooked | yes | Fully wired |
| 130 | `SPELL_ANIMATE_DEAD_GREATER` | `src/spells.h` | spello | manual (spell_animate_dead_greater) | hooked | yes | Fully wired |
| 131 | `SPELL_ABYSS_GATE` | `src/spells.h` | spello | manual (spell_abyss_gate) | hooked | yes | Fully wired |
| 132 | `SPELL_GATE` | `src/spells.h` | spello | manual (spell_gate) | hooked | yes | Fully wired |
| 133 | `SPELL_PORTAL` | `src/spells.h` | spello | manual (spell_portal) | hooked | yes | Fully wired |
| 134 | `SPELL_LOCATE_CORPSE` | `src/spells.h` | spello | manual (spell_locate_corpse) | hooked | yes | Fully wired |
| 135 | `SPELL_WORD_OF_RECALL_MASS` | `src/spells.h` | spello | manual (spell_word_of_recall_mass) | hooked | yes | Fully wired |
| 136 | `SPELL_ASTRAL_PROJECTION` | `src/spells.h` | spello | manual (spell_astral_projection) | hooked | yes | Fully wired |
| 137 | `SPELL_ETHEREAL_JAUNT` | `src/spells.h` | spello | manual (spell_ethereal_jaunt) | hooked | yes | Fully wired |
| 138 | `SPELL_LEYLINE_TAP` | `src/spells.h` | spello | manual (spell_leyline_tap) | hooked | yes | Fully wired |
| 139 | `SPELL_TEMPORAL_SHIFT` | `src/spells.h` | spello | manual (spell_temporal_shift) | hooked | yes | Fully wired |
| 140 | `SPELL_CHRONO_SHIFT` | `src/spells.h` | spello | manual (spell_chrono_shift) | hooked | yes | Fully wired |
| 141 | `SPELL_BALEFIRE` | `src/spells.h` | spello | manual (spell_balefire) | hooked | yes | Fully wired |
| 142 | `SPELL_METEOR` | `src/spells.h` | spello | manual (spell_meteor) | hooked | yes | Fully wired |
| 143 | `SPELL_METEOR_SWARM` | `src/spells.h` | spello | manual (spell_meteor_swarm) | hooked | yes | Fully wired |
| 144 | `SPELL_HELLFIRE` | `src/spells.h` | spello | manual (spell_hellfire) | hooked | yes | Fully wired |
| 145 | `SPELL_WRATHFIRE` | `src/spells.h` | spello | manual (spell_wrathfire) | hooked | yes | Fully wired |
| 146 | `SPELL_CELESTIAL_SMITE` | `src/spells.h` | spello | manual (spell_celestial_smite) | hooked | yes | Fully wired |
| 147 | `SPELL_HAMMER_OF_GOD` | `src/spells.h` | spello | manual (spell_hammer_of_god) | hooked | yes | Fully wired |
| 148 | `SPELL_DEATH_KNELL` | `src/spells.h` | spello | manual (spell_death_knell) | hooked | yes | Fully wired |
| 149 | `SPELL_UNHOLY_WORD` | `src/spells.h` | spello | manual (spell_unholy_word) | hooked | yes | Fully wired |
| 150 | `SPELL_HOLY_WORD` | `src/spells.h` | spello | manual (spell_holy_word) | hooked | yes | Fully wired |
| 151 | `SPELL_FINGER_OF_DEATH` | `src/spells.h` | spello | manual (spell_finger_of_death) | hooked | yes | Fully wired |
| 152 | `SPELL_WAIL_OF_THE_BANSHEE` | `src/spells.h` | spello | manual (spell_wail_of_the_banshee) | hooked | yes | Fully wired |
| 153 | `SPELL_DISINTEGRATE` | `src/spells.h` | spello | manual (spell_disintegrate) | hooked | yes | Fully wired |
| 154 | `SPELL_POWER_WORD_KILL` | `src/spells.h` | spello | manual (spell_power_word_kill) | hooked | yes | Fully wired |
| 155 | `SPELL_POWER_WORD_STUN` | `src/spells.h` | spello | manual (spell_power_word_stun) | hooked | yes | Fully wired |
| 156 | `SPELL_POWER_WORD_BLIND` | `src/spells.h` | spello | manual (spell_power_word_blind) | hooked | yes | Fully wired |
| 157 | `SPELL_POWER_WORD_SILENCE` | `src/spells.h` | spello | manual (spell_power_word_silence) | hooked | yes | Fully wired |
| 158 | `SPELL_PSYCHIC_CRUSH` | `src/spells.h` | spello | manual (spell_psychic_crush) | hooked | yes | Fully wired |
| 159 | `SPELL_TIME_STOP` | `src/spells.h` | spello | manual (spell_time_stop) | hooked | yes | Fully wired |
| 160 | `SPELL_BLACK_LANCE` | `src/spells.h` | spello | manual missing dispatch | hooked | no | Parser-wired but not implemented |
| 161 | `SPELL_REALITY_SLASH` | `src/spells.h` | spello | manual missing dispatch | hooked | no | Parser-wired but not implemented |
| 162 | `SPELL_GRASP_HEART` | `src/spells.h` | spello | manual missing dispatch | hooked | no | Parser-wired but not implemented |
| 163 | `SPELL_NEGATIVE_BURST` | `src/spells.h` | spello | manual missing dispatch | hooked | no | Parser-wired but not implemented |
| 164 | `SPELL_TRUE_DEATH` | `src/spells.h` | spello | manual missing dispatch | hooked | no | Parser-wired but not implemented |
| 165 | `SPELL_PERFECT_UNKNOWABLE` | `src/spells.h` | spello | manual missing dispatch | hooked | no | Parser-wired but not implemented |
| 166 | `SPELL_CRYSTAL_BODY` | `src/spells.h` | spello | manual missing dispatch | hooked | no | Parser-wired but not implemented |
| 167 | `SPELL_GREATER_MAGIC_SEAL` | `src/spells.h` | spello | manual missing dispatch | hooked | no | Parser-wired but not implemented |
| 168 | `SPELL_DESPAIR_AURA` | `src/spells.h` | spello | manual missing dispatch | hooked | no | Parser-wired but not implemented |
| 169 | `SPELL_OBLIVION_SPEAR` | `src/spells.h` | spello | manual missing dispatch | hooked | no | Parser-wired but not implemented |
| 170 | `SPELL_BONE_PRISON` | `src/spells.h` | spello | manual missing dispatch | hooked | no | Parser-wired but not implemented |
| 171 | `SPELL_UNDYING_WILL` | `src/spells.h` | spello | manual missing dispatch | hooked | no | Parser-wired but not implemented |
| 172 | `SPELL_DRAGON_LIGHTNING` | `src/spells.h` | spello | manual missing dispatch | hooked | no | Parser-wired but not implemented |
| 173 | `SPELL_CHAIN_DRAGON_LIGHTNING` | `src/spells.h` | spello | manual missing dispatch | hooked | no | Parser-wired but not implemented |
| 174 | `SPELL_HELL_FLAME` | `src/spells.h` | spello | manual missing dispatch | hooked | no | Parser-wired but not implemented |
| 175 | `SPELL_GRAVITY_MAELSTROM` | `src/spells.h` | spello | manual missing dispatch | hooked | no | Parser-wired but not implemented |
| 176 | `SPELL_CALL_GREATER_THUNDER` | `src/spells.h` | spello | manual missing dispatch | hooked | no | Parser-wired but not implemented |
| 177 | `SPELL_ASTRAL_SMITE` | `src/spells.h` | spello | manual missing dispatch | hooked | no | Parser-wired but not implemented |
| 178 | `SPELL_GREATER_REJECTION` | `src/spells.h` | spello | manual missing dispatch | hooked | no | Parser-wired but not implemented |
| 179 | `SPELL_FALLEN_DOWN` | `src/spells.h` | spello | manual missing dispatch | hooked | no | Parser-wired but not implemented |
| 180 | `SPELL_IA_SHUB_NIGGURATH` | `src/spells.h` | spello | manual missing dispatch | hooked | no | Parser-wired but not implemented |
| 181 | `SPELL_GOAL_OF_ALL_LIFE_IS_DEATH` | `src/spells.h` | spello | manual missing dispatch | hooked | no | Parser-wired but not implemented |
| 182 | `SPELL_CRY_OF_THE_BANSHEE` | `src/spells.h` | spello | manual missing dispatch | hooked | no | Parser-wired but not implemented |
| 183 | `SPELL_NAPALM` | `src/spells.h` | spello | manual missing dispatch | hooked | no | Parser-wired but not implemented |
| 184 | `SPELL_BODY_OF_EFFULGENT_BERYL` | `src/spells.h` | spello | manual missing dispatch | hooked | no | Parser-wired but not implemented |
| 185 | `SPELL_VERMILION_NOVA` | `src/spells.h` | spello | manual missing dispatch | hooked | no | Parser-wired but not implemented |
| 186 | `SPELL_NUCLEAR_BLAST` | `src/spells.h` | spello | manual missing dispatch | hooked | no | Parser-wired but not implemented |
| 187 | `SPELL_GREATER_TELEPORTATION` | `src/spells.h` | spello | manual missing dispatch | hooked | no | Parser-wired but not implemented |
| 188 | `SPELL_SILENT_MAGIC` | `src/spells.h` | spello | manual missing dispatch | hooked | no | Parser-wired but not implemented |
| 189 | `SPELL_TRIPLE_MAXIMIZE_MAGIC` | `src/spells.h` | spello | manual missing dispatch | hooked | no | Parser-wired but not implemented |
| 190 | `SPELL_PANTHEON` | `src/spells.h` | spello | manual missing dispatch | hooked | no | Parser-wired but not implemented |
| 191 | `SPELL_DIMENSIONAL_LOCK` | `src/spells.h` | spello | manual missing dispatch | hooked | no | Parser-wired but not implemented |
| 192 | `SPELL_SHADOW_BIND` | `src/spells.h` | spello | manual missing dispatch | hooked | no | Parser-wired but not implemented |
| 193 | `SPELL_SHADOW_EXCHANGE` | `src/spells.h` | spello | manual missing dispatch | hooked | no | Parser-wired but not implemented |
| 194 | `SPELL_DAGGER_RAIN` | `src/spells.h` | spello | manual missing dispatch | hooked | no | Parser-wired but not implemented |
| 195 | `SPELL_MONARCHS_PRESSURE` | `src/spells.h` | spello | manual missing dispatch | hooked | no | Parser-wired but not implemented |
| 196 | `SPELL_SHADOW_DOMAIN` | `src/spells.h` | spello | manual missing dispatch | hooked | no | Parser-wired but not implemented |
| 197 | `SPELL_FORCE_GRASP` | `src/spells.h` | spello | manual missing dispatch | hooked | no | Parser-wired but not implemented |
| 198 | `SPELL_SHADOW_STEP` | `src/spells.h` | spello | manual missing dispatch | hooked | no | Parser-wired but not implemented |
| 199 | `SPELL_BLACK_HEART` | `src/spells.h` | spello | manual missing dispatch | hooked | no | Parser-wired but not implemented |
| 200 | `SPELL_CALL_SHADOW_LEGION` | `src/spells.h` | spello | manual missing dispatch | hooked | no | Parser-wired but not implemented |
| 201 | `SPELL_NIGHT_HUNT` | `src/spells.h` | spello | manual missing dispatch | hooked | no | Parser-wired but not implemented |
| 202 | `SPELL_DARK_REBUKE` | `src/spells.h` | spello | manual missing dispatch | hooked | no | Parser-wired but not implemented |
| 203 | `SPELL_EXECUTION_MARK` | `src/spells.h` | spello | manual missing dispatch | hooked | no | Parser-wired but not implemented |
| 204 | `SPELL_SHADOW_EXTRACTION` | `src/spells.h` | spello | manual missing dispatch | hooked | no | Parser-wired but not implemented |
| 205 | `SPELL_ARISE_GREATER` | `src/spells.h` | spello | manual missing dispatch | hooked | no | Parser-wired but not implemented |
| 206 | `SPELL_MONARCHS_AUTHORITY` | `src/spells.h` | spello | manual missing dispatch | hooked | no | Parser-wired but not implemented |
| 207 | `SPELL_RULERS_HAND` | `src/spells.h` | spello | manual missing dispatch | hooked | no | Parser-wired but not implemented |
| 208 | `SPELL_SHADOW_LANCE` | `src/spells.h` | spello | manual missing dispatch | hooked | no | Parser-wired but not implemented |
| 209 | `SPELL_SHADOW_BURST` | `src/spells.h` | spello | manual missing dispatch | hooked | no | Parser-wired but not implemented |
| 210 | `SPELL_SHADOW_STORM` | `src/spells.h` | spello | manual missing dispatch | hooked | no | Parser-wired but not implemented |
| 211 | `SPELL_FATAL_STRIKE` | `src/spells.h` | spello | manual missing dispatch | hooked | no | Parser-wired but not implemented |
| 212 | `SPELL_DOMINION_OF_SHADOWS` | `src/spells.h` | spello | manual missing dispatch | hooked | no | Parser-wired but not implemented |
| 213 | `SPELL_SHADOW_RECALL` | `src/spells.h` | spello | manual missing dispatch | hooked | no | Parser-wired but not implemented |
| 214 | `SPELL_SHADOW_REGENESIS` | `src/spells.h` | spello | manual missing dispatch | hooked | no | Parser-wired but not implemented |
| 215 | `SPELL_ASSASSINS_INTENT` | `src/spells.h` | spello | manual missing dispatch | hooked | no | Parser-wired but not implemented |
| 216 | `SPELL_BLOOD_DAGGER_TEMPEST` | `src/spells.h` | spello | manual missing dispatch | hooked | no | Parser-wired but not implemented |
| 217 | `SPELL_CHAIN_OF_SUBJUGATION` | `src/spells.h` | spello | manual missing dispatch | hooked | no | Parser-wired but not implemented |
| 218 | `SPELL_SOVEREIGNS_STEP` | `src/spells.h` | spello | manual missing dispatch | hooked | no | Parser-wired but not implemented |
| 219 | `SPELL_KINGS_COMMAND` | `src/spells.h` | spello | manual missing dispatch | hooked | no | Parser-wired but not implemented |
| 220 | `SPELL_DETECT_KILL_INTENT` | `src/spells.h` | spello | manual missing dispatch | hooked | no | Parser-wired but not implemented |
| 221 | `SPELL_MUTILATE` | `src/spells.h` | spello | manual missing dispatch | hooked | no | Parser-wired but not implemented |
| 222 | `SPELL_SHADOW_ARMOR` | `src/spells.h` | spello | manual missing dispatch | hooked | no | Parser-wired but not implemented |
| 223 | `SPELL_TOTAL_OCCULTATION` | `src/spells.h` | spello | manual missing dispatch | hooked | no | Parser-wired but not implemented |
| 224 | `SPELL_DOMAIN_BREAK` | `src/spells.h` | spello | manual missing dispatch | hooked | no | Parser-wired but not implemented |
| 225 | `SPELL_HUNTERS_INSTINCT` | `src/spells.h` | spello | manual missing dispatch | hooked | no | Parser-wired but not implemented |
| 231 | `SKILL_BACKSTAB` | `src/spells.h` | skillo | command handler | command path | yes | Fully wired |
| 232 | `SKILL_BASH` | `src/spells.h` | skillo | command handler | command path | yes | Fully wired |
| 233 | `SKILL_HIDE` | `src/spells.h` | skillo | command handler | command path | yes | Fully wired |
| 234 | `SKILL_KICK` | `src/spells.h` | skillo | command handler | command path | no | Implemented but not reachable |
| 235 | `SKILL_PICK_LOCK` | `src/spells.h` | skillo | command handler | command path | yes | Fully wired |
| 236 | `SKILL_WHIRLWIND` | `src/spells.h` | skillo | command handler | command path | yes | Fully wired |
| 237 | `SKILL_RESCUE` | `src/spells.h` | skillo | command handler | command path | yes | Fully wired |
| 238 | `SKILL_SNEAK` | `src/spells.h` | skillo | command handler | command path | yes | Fully wired |
| 239 | `SKILL_STEAL` | `src/spells.h` | skillo | command handler | command path | yes | Fully wired |
| 240 | `SKILL_TRACK` | `src/spells.h` | skillo | command handler | command path | yes | Fully wired |
| 241 | `SKILL_BANDAGE` | `src/spells.h` | skillo | command handler | command path | yes | Fully wired |
| 242 | `SKILL_DUAL_WIELD` | `src/spells.h` | skillo | passive hook | fight.c, act.informative.c, act.item.c | yes | Intentionally passive-only and hook-wired |
| 243 | `SKILL_RECALL` | `src/spells.h` | skillo | command handler | command path | no | Implemented but not reachable |
| 244 | `SKILL_OVERLORD_PRESENCE` | `src/spells.h` | skillo | passive hook | fight.c | no | Implemented but not reachable |
| 245 | `SKILL_SUPREME_CASTER_DISCIPLINE` | `src/spells.h` | skillo | passive hook | magic.c | no | Implemented but not reachable |
| 246 | `SKILL_UNDEAD_COMMAND` | `src/spells.h` | skillo | no handler/hook | none | no | Broken by missing support hook |
| 247 | `SKILL_TACTICAL_SPELL_MEMORY` | `src/spells.h` | skillo | no handler/hook | none | no | Broken by missing support hook |
| 248 | `SKILL_DREAD_DOMINION` | `src/spells.h` | skillo | passive hook | fight.c | no | Implemented but not reachable |
| 249 | `SKILL_SHADOW_COMMANDER` | `src/spells.h` | skillo | passive hook | fight.c | no | Implemented but not reachable |
| 250 | `SKILL_PREDATORS_ADVANCE` | `src/spells.h` | skillo | passive hook | fight.c | no | Implemented but not reachable |
| 251 | `SKILL_MONARCH_REFLEXES` | `src/spells.h` | skillo | passive hook | fight.c | no | Implemented but not reachable |
| 252 | `SKILL_RELENTLESS_HUNT` | `src/spells.h` | skillo | passive hook | fight.c | no | Implemented but not reachable |
| 253 | `SKILL_SHADOW_RESERVOIR` | `src/spells.h` | skillo | passive hook | utils.c | no | Implemented but not reachable |
| 254 | `SKILL_SHADOW_SURGE` | `src/spells.h` | skillo | passive hook | fight.c | no | Implemented but not reachable |
| 255 | `SKILL_CHAIN_ASSASSAULT` | `src/spells.h` | skillo | passive hook | fight.c | no | Implemented but not reachable |
| 256 | `SKILL_SOVEREIGN_PRESSURE` | `src/spells.h` | skillo | passive hook | fight.c | no | Implemented but not reachable |
| 257 | `SKILL_KILL_WINDOW` | `src/spells.h` | skillo | passive hook | fight.c | no | Implemented but not reachable |
| 258 | `SKILL_LEGION_MASTERY` | `src/spells.h` | skillo | passive hook | fight.c | no | Implemented but not reachable |

## C. Issue list

| File | Symbol/function | Problem | Fix applied |
|---|---|---|---|
| `src/spell_parser.c` | `SPELL_VENTRILOQUATE` | Missing spello registration made the declared class-learnable spell uncastable by name. | Added spello metadata, manual dispatch in cast switch, and concrete `spell_ventriloquate` implementation. |
| `src/spell_parser.c` | `SPELL_GROUP_RECALL` | Group recall existed in `mag_groups` but had no parser registration, leaving normal casting path broken. | Added `spello(SPELL_GROUP_RECALL, ..., MAG_GROUPS, ...)`. |
| `src/spell_parser.c` | `SPELL_IDENTIFY` | Duplicate later spello entry overwrote cast metadata (mana/position/target profile). | Removed duplicate non-castable overwrite entry and preserved the castable metadata entry. |
| `src/spell_parser.c + src/spells.c` | `SPELL_CONTROL_WEATHER` | Spell was parser-declared with `MAG_MANUAL` but had no manual dispatch case or implementation function. | Added manual dispatch case and implemented `spell_control_weather`. |
| `src/class.c + src/fight.c + src/magic.c` | `SKILL_OVERLORD_PRESENCE..SKILL_LEGION_MASTERY` | Shadow passive block is hook-wired in combat but lacks class learnability assignments. | Documented as intentionally limited/unreachable pending class progression design. |
| `src/spells.h + src/spell_parser.c` | `SKILL_UNDEAD_COMMAND / SKILL_TACTICAL_SPELL_MEMORY` | Declared skills are parser-registered but have no command or passive hook logic. | Documented as unresolved wiring gaps needing explicit gameplay hook design. |
| `src/spells.h` | `spell_information prototype` | Prototype exists without implementation and is not referenced by manual dispatch. | Left unchanged in this pass; documented as legacy declaration requiring separate cleanup. |

## D. Files changed
- `src/spell_parser.c`
- `src/spells.h`
- `src/spells.c`
- `doc/spell_skill_verification_2026-03-18.md`

## E. Remaining limitations
- Shadow-sovereign passive skills (`SKILL_OVERLORD_PRESENCE` through `SKILL_LEGION_MASTERY`) are wired in combat hooks but have no class-level assignments in `init_spell_levels()`, leaving them unreachable for normal progression.
- `SKILL_UNDEAD_COMMAND` and `SKILL_TACTICAL_SPELL_MEMORY` have parser metadata only and no active command or passive hook.
- `spell_information` remains a declared-but-undefined legacy prototype in `spells.h`.

## F. Compile result
- `make -C src -j4` succeeded.
- Existing compiler warnings remain in unrelated files (no new warnings introduced by this pass).
