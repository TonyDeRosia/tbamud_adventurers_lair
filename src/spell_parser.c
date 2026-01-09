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
static bool is_buff_spell(int spellnum);

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
  return MAX(SINFO.mana_max - (SINFO.mana_change *
      (GET_LEVEL(ch) - SINFO.min_level[(int) GET_CLASS(ch)])),
  SINFO.mana_min);
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

static bool is_buff_spell(int spellnum)
{
  if (!is_available_spell(spellnum))
    return FALSE;
  if (SINFO.violent)
    return FALSE;
  if (!IS_SET(SINFO.routines, MAG_AFFECTS))
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
  int best_spell = -1;
  int best_tokens = 0;
  int best_input_tokens = 0;
  int match_count = 0;
  int spellnum;

  if (matched_tokens)
    *matched_tokens = 0;

  *ambig_buf = '\0';

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
  int best_ability = -1;
  int best_tokens = 0;
  int best_input_tokens = 0;
  int match_count = 0;
  int ability;

  if (matched_tokens)
    *matched_tokens = 0;

  *ambig_buf = '\0';

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

  normalize_ability_input(name, cleaned, sizeof(cleaned));
  if (!*cleaned)
    return (-1);

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

  if (ROOM_FLAGGED(IN_ROOM(caster), ROOM_NOMAGIC)) {
    send_to_char(caster, "Your magic fizzles out and dies.\r\n");
    act("$n's magic fizzles out and dies.", FALSE, caster, 0, 0, TO_ROOM);
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

  for (spellnum = 1; spellnum <= MAX_SPELLS; spellnum++) {
    if (!is_buff_spell(spellnum))
      continue;
    if (GET_LEVEL(ch) < SINFO.min_level[(int) GET_CLASS(ch)])
      continue;
    if (GET_SKILL(ch, spellnum) == 0)
      continue;
    if (tch != ch && IS_SET(SINFO.targets, TAR_SELF_ONLY))
      continue;
    if (tch == ch && IS_SET(SINFO.targets, TAR_NOT_SELF))
      continue;

    any_eligible = TRUE;
    mana = mag_manacost(ch, spellnum);
    if ((mana > 0) && (GET_MANA(ch) < mana) && (GET_LEVEL(ch) < LVL_IMMORT))
      continue;

    any_attempted = TRUE;
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
    number = get_number(&targp);
    if (!target && (IS_SET(SINFO.targets, TAR_CHAR_ROOM))) {
      if ((tch = get_char_vis(ch, target_argument, &number, FIND_CHAR_ROOM)) != NULL)
        target = TRUE;
    }
    if (!target && IS_SET(SINFO.targets, TAR_CHAR_WORLD))
      if ((tch = get_char_vis(ch, target_argument, &number, FIND_CHAR_WORLD)) != NULL)
        target = TRUE;

    if (!target && IS_SET(SINFO.targets, TAR_OBJ_INV))
      if ((tobj = get_obj_in_list_vis(ch, target_argument, &number, ch->carrying)) != NULL)
        target = TRUE;

    if (!target && IS_SET(SINFO.targets, TAR_OBJ_EQUIP)) {
      for (i = 0; !target && i < NUM_WEARS; i++)
        if (GET_EQ(ch, i) && isname(target_argument, GET_EQ(ch, i)->name)) {
          tobj = GET_EQ(ch, i);
          target = TRUE;
        }
    }
    if (!target && IS_SET(SINFO.targets, TAR_OBJ_ROOM))
      if ((tobj = get_obj_in_list_vis(ch, target_argument, &number,
          world[IN_ROOM(ch)].contents)) != NULL)
        target = TRUE;

    if (!target && IS_SET(SINFO.targets, TAR_OBJ_WORLD))
      if ((tobj = get_obj_vis(ch, target_argument, &number)) != NULL)
        target = TRUE;

    if (!target && (IS_SET(SINFO.targets, TAR_CHAR_ROOM) ||
        IS_SET(SINFO.targets, TAR_CHAR_WORLD))) {
      bool include_fighting = IS_SET(SINFO.targets, TAR_FIGHT_VICT);
      tch = find_char_prefix(ch, target_argument, number, include_fighting,
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
  mana = mag_manacost(ch, spellnum);
  if ((mana > 0) && (GET_MANA(ch) < mana) && (GET_LEVEL(ch) < LVL_IMMORT)) {
    send_to_char(ch, "You haven't the energy to cast that spell!\r\n");
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
  TAR_CHAR_ROOM | TAR_OBJ_INV | TAR_OBJ_ROOM, FALSE, MAG_MANUAL, NULL);

  /* NON-castable spells should appear below here. */
  spello(SPELL_IDENTIFY, "identify", 0, 0, 0, 0,
  TAR_CHAR_ROOM | TAR_OBJ_INV | TAR_OBJ_ROOM, FALSE, MAG_MANUAL, NULL);

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
