#include "conf.h"
#include "sysdep.h"

#include <ctype.h>

#include "structs.h"
#include "utils.h"
#include "db.h"
#include "handler.h"
#include "ai_actor.h"
#include "ai_reactions.h"

#define AI_REL_CACHE_MAX 256
#define AI_REL_PER_MOB_MAX 12
#define AI_ROOM_COMBAT_CACHE_MAX 128

enum ai_ext_archetype {
  AI_ARCHX_PALADIN_HOLY = 0, AI_ARCHX_HEALER_SUPPORT, AI_ARCHX_GUARD_LAW,
  AI_ARCHX_CIVILIAN, AI_ARCHX_MERCHANT, AI_ARCHX_INSTRUCTOR, AI_ARCHX_BANDIT_CRIMINAL,
  AI_ARCHX_CULTIST_FANATIC, AI_ARCHX_BEAST_ANIMAL, AI_ARCHX_UNDEAD_MINDLESS,
  AI_ARCHX_SPIRIT_ETHEREAL, AI_ARCHX_COMMANDER
};

struct ai_room_context { int threat_level, fights_count, has_fight, has_multiple_fights, bloodshed_recent, law_breaking_recent; };
struct ai_moral_context { int victim_is_good, aggressor_is_evil, victim_is_innocent, player_is_known, urgency; };
struct ai_personality_ext { int bravery, empathy, honor, aggression, lawfulness, piety, wit, patience, fearfulness; };
struct ai_rel_entry { long player_idnum; int trust,respect,fear,gratitude; time_t last_seen,last_interaction; unsigned int recent_flags; };
struct ai_rel_bucket { struct char_data *mob; struct ai_rel_entry entries[AI_REL_PER_MOB_MAX]; time_t updated_at; };
struct ai_room_combat_seen { room_rnum room; int had_fight_last_check; time_t last_check_time; };

static struct ai_rel_bucket ai_rel_cache[AI_REL_CACHE_MAX];
static struct ai_room_combat_seen ai_room_combat_cache[AI_ROOM_COMBAT_CACHE_MAX];

static int ai_text_has_sub_ci_local(const char *hay, const char *needle)
{
  size_t nl, i, j;
  if (!hay || !needle || !*needle) return FALSE;
  nl = strlen(needle);
  for (i = 0; hay[i]; i++) {
    for (j = 0; j < nl; j++) {
      if (!hay[i + j]) return FALSE;
      if (tolower((unsigned char)hay[i + j]) != tolower((unsigned char)needle[j]))
        break;
    }
    if (j == nl) return TRUE;
  }
  return FALSE;
}

static const char *ai_rx_actor_display_name(struct char_data *mob, char *out, size_t outsz)
{
  char tmp[256];
  char *p;
  size_t len;

  if (!out || outsz == 0)
    return "someone";
  out[0] = '\0';
  if (!mob) {
    strlcpy(out, "someone", outsz);
    return out;
  }

  if (GET_NAME(mob) && isupper((unsigned char)GET_NAME(mob)[0]))
    strlcpy(tmp, GET_NAME(mob), sizeof(tmp));
  else
    strlcpy(tmp, mob->player.short_descr ? mob->player.short_descr : GET_NAME(mob), sizeof(tmp));

  p = tmp;
  while (*p && isspace((unsigned char)*p))
    p++;
  if (!strncasecmp(p, "a ", 2))
    p += 2;
  else if (!strncasecmp(p, "an ", 3))
    p += 3;
  else if (!strncasecmp(p, "the ", 4))
    p += 4;

  strlcpy(out, p, outsz);
  len = strlen(out);
  while (len > 0 && (isspace((unsigned char)out[len - 1]) || out[len - 1] == '.' || out[len - 1] == '!' || out[len - 1] == '?' || out[len - 1] == ','))
    out[--len] = '\0';

  if (!out[0])
    strlcpy(out, "someone", outsz);
  return out;
}

int ai_rx_infer_targeted_to_mob(struct char_data *mob, const char *norm_text)
{
  char dn[128];

  if (!mob || !norm_text)
    return FALSE;

  ai_rx_actor_display_name(mob, dn, sizeof(dn));
  if (ai_text_has_sub_ci_local(norm_text, "you") || ai_text_has_sub_ci_local(norm_text, "@"))
    return TRUE;
  return ai_text_has_sub_ci_local(norm_text, dn);
}

int ai_rx_is_service_style_request(const char *norm_text)
{
  if (!norm_text || !*norm_text)
    return FALSE;

  return (ai_text_has_sub_ci_local(norm_text, "food") ||
          ai_text_has_sub_ci_local(norm_text, "water") ||
          ai_text_has_sub_ci_local(norm_text, "rest") ||
          ai_text_has_sub_ci_local(norm_text, "inn") ||
          ai_text_has_sub_ci_local(norm_text, "training") ||
          ai_text_has_sub_ci_local(norm_text, "train") ||
          ai_text_has_sub_ci_local(norm_text, "practice") ||
          ai_text_has_sub_ci_local(norm_text, "directions") ||
          ai_text_has_sub_ci_local(norm_text, "where") ||
          ai_text_has_sub_ci_local(norm_text, "money") ||
          ai_text_has_sub_ci_local(norm_text, "job"));
}

int ai_rx_is_explicit_sexual_request(const char *norm_text)
{
  if (!norm_text || !*norm_text)
    return FALSE;

  return (ai_text_has_sub_ci_local(norm_text, "sex") ||
          ai_text_has_sub_ci_local(norm_text, "fuck") ||
          ai_text_has_sub_ci_local(norm_text, "nude") ||
          ai_text_has_sub_ci_local(norm_text, "horny") ||
          ai_text_has_sub_ci_local(norm_text, "bed me") ||
          ai_text_has_sub_ci_local(norm_text, "sleep with me") ||
          ai_text_has_sub_ci_local(norm_text, "kiss me"));
}

void ai_rx_clean_sentence(char *s)
{
  size_t i, len;
  if (!s || !*s) return;
  for (i = 0; s[i]; i++) if (isspace((unsigned char)s[i])) { s[i] = ' '; while (s[i+1] && isspace((unsigned char)s[i+1])) memmove(s+i+1, s+i+2, strlen(s+i+2)+1); }
  while (*s == ' ') memmove(s, s + 1, strlen(s));
  len = strlen(s);
  while (len > 0 && s[len - 1] == ' ') s[--len] = '\0';
  if (len > 0 && s[len-1] != '.' && s[len-1] != '!' && s[len-1] != '?') { if (len + 1 < 512) { s[len] = '.'; s[len+1] = '\0'; } }
}

static enum ai_rx_intent ai_rx_classify_intent(enum ai_event_type type, const char *norm_text)
{
  if (type == AI_EVENT_COMBAT_START) return RX_INTENT_COMBAT_START;
  if (!norm_text) return RX_INTENT_NONE;
  if (ai_text_has_sub_ci_local(norm_text, "help") || ai_text_has_sub_ci_local(norm_text, "save me")) return RX_INTENT_REQUEST_HELP;
  if (ai_text_has_sub_ci_local(norm_text, "heal") || ai_text_has_sub_ci_local(norm_text, "cure")) return RX_INTENT_REQUEST_HEAL;
  if (ai_text_has_sub_ci_local(norm_text, "buy") || ai_text_has_sub_ci_local(norm_text, "sell") || ai_text_has_sub_ci_local(norm_text, "trade")) return RX_INTENT_REQUEST_TRADE;
  if (ai_text_has_sub_ci_local(norm_text, "train") || ai_text_has_sub_ci_local(norm_text, "practice")) return RX_INTENT_REQUEST_TRAIN;
  if (ai_text_has_sub_ci_local(norm_text, "sorry") || ai_text_has_sub_ci_local(norm_text, "forgive")) return RX_INTENT_APOLOGY;
  if (ai_text_has_sub_ci_local(norm_text, "praise") || ai_text_has_sub_ci_local(norm_text, "thank")) return RX_INTENT_PRAISE;
  if (ai_text_has_sub_ci_local(norm_text, "kill") || ai_text_has_sub_ci_local(norm_text, "die") || ai_text_has_sub_ci_local(norm_text, "murder")) return RX_INTENT_THREATEN;
  if (ai_text_has_sub_ci_local(norm_text, "idiot") || ai_text_has_sub_ci_local(norm_text, "stupid") || ai_text_has_sub_ci_local(norm_text, "fool")) return RX_INTENT_INSULT;
  if (ai_text_has_sub_ci_local(norm_text, "steal") || ai_text_has_sub_ci_local(norm_text, "rob") || ai_text_has_sub_ci_local(norm_text, "mug")) return RX_INTENT_LOOT_THEFT;
  if (ai_text_has_sub_ci_local(norm_text, "pray") || ai_text_has_sub_ci_local(norm_text, "bless") || ai_text_has_sub_ci_local(norm_text, "mercy")) return RX_INTENT_CONFESSION_PRAYER;
  if (strchr(norm_text, '?')) return RX_INTENT_QUESTION;
  if (ai_text_has_sub_ci_local(norm_text, "hello") || ai_text_has_sub_ci_local(norm_text, "hi ")) return RX_INTENT_GREETING;
  return RX_INTENT_SMALLTALK;
}

static void ai_actor_build_room_context(struct room_data *room, const char *norm_text, enum ai_rx_intent intent, struct ai_room_context *out)
{
  struct char_data *ch;
  room_rnum rr;
  int i, oldest = 0;
  time_t now = time(0);
  if (!out) return;
  memset(out, 0, sizeof(*out));
  if (!room) return;
  rr = (room_rnum)(room - world);
  for (ch = room->people; ch; ch = ch->next_in_room) if (FIGHTING(ch)) out->fights_count++;
  out->has_fight = (out->fights_count > 0);
  out->has_multiple_fights = (out->fights_count > 2);
  if (out->has_fight) out->threat_level = 2;
  if (out->has_multiple_fights) out->threat_level = 3;
  if (intent == RX_INTENT_DAMAGE_EVENT || intent == RX_INTENT_THREATEN || (norm_text && (ai_text_has_sub_ci_local(norm_text, "bleed") || ai_text_has_sub_ci_local(norm_text, "slashes") || ai_text_has_sub_ci_local(norm_text, "hits")))) out->bloodshed_recent = 1;
  if (intent == RX_INTENT_LOOT_THEFT || (norm_text && (ai_text_has_sub_ci_local(norm_text, "steal") || ai_text_has_sub_ci_local(norm_text, "rob")))) out->law_breaking_recent = 1;
  if (out->bloodshed_recent && out->threat_level < 2) out->threat_level = 2;
  for (i = 0; i < AI_ROOM_COMBAT_CACHE_MAX; i++) {
    if (ai_room_combat_cache[i].room == rr) {
      ai_room_combat_cache[i].had_fight_last_check = out->has_fight;
      ai_room_combat_cache[i].last_check_time = now;
      return;
    }
    if (ai_room_combat_cache[i].last_check_time < ai_room_combat_cache[oldest].last_check_time)
      oldest = i;
  }
  ai_room_combat_cache[oldest].room = rr;
  ai_room_combat_cache[oldest].had_fight_last_check = out->has_fight;
  ai_room_combat_cache[oldest].last_check_time = now;
}

static void ai_rx_build_moral_context(struct char_data *mob, struct char_data *actor, enum ai_rx_intent intent, const struct ai_room_context *rc, struct ai_moral_context *mc)
{
  if (!mc) return;
  memset(mc, 0, sizeof(*mc));
  if (!mob || !actor) return;
  mc->victim_is_good = IS_GOOD(actor);
  mc->aggressor_is_evil = IS_EVIL(actor);
  mc->victim_is_innocent = (intent == RX_INTENT_REQUEST_HELP || intent == RX_INTENT_REQUEST_HEAL);
  mc->player_is_known = (GET_IDNUM(actor) > 0);
  if (intent == RX_INTENT_REQUEST_HELP || intent == RX_INTENT_REQUEST_HEAL || intent == RX_INTENT_PLAYER_DOWN) mc->urgency += 25;
  if (intent == RX_INTENT_COMBAT_START || intent == RX_INTENT_COMBAT_ONGOING || intent == RX_INTENT_DAMAGE_EVENT) mc->urgency += 20;
  if (rc && rc->threat_level >= 2) mc->urgency += 15;
  if (rc && rc->has_multiple_fights) mc->urgency += 10;
  if (rc && rc->bloodshed_recent) mc->urgency += 10;
  if (rc && rc->law_breaking_recent) mc->urgency += 8;
  if (IS_GOOD(mob) && (mc->victim_is_innocent || mc->aggressor_is_evil)) mc->urgency += 8;
  mc->urgency = URANGE(0, mc->urgency, 100);
}

static int ai_rx_guess_ext_archetype(struct char_data *mob, int role, int style)
{
  const char *sd = (mob && mob->player.short_descr) ? mob->player.short_descr : "";
  (void)style;
  if (!mob) return AI_ARCHX_CIVILIAN;
  if (role == ROLE_GUARD) return AI_ARCHX_GUARD_LAW;
  if (role == ROLE_MERCHANT) return AI_ARCHX_MERCHANT;
  if (role == ROLE_BANDIT) return AI_ARCHX_BANDIT_CRIMINAL;
  if (role == ROLE_CULTIST) return AI_ARCHX_CULTIST_FANATIC;
  if (ai_text_has_sub_ci_local(sd, "paladin") || ai_text_has_sub_ci_local(sd, "templar")) return AI_ARCHX_PALADIN_HOLY;
  if (ai_text_has_sub_ci_local(sd, "healer") || ai_text_has_sub_ci_local(sd, "priest")) return AI_ARCHX_HEALER_SUPPORT;
  if (ai_text_has_sub_ci_local(sd, "instructor") || ai_text_has_sub_ci_local(sd, "trainer") || MOB_FLAGGED(mob, MOB_GUILD_MASTER)) return AI_ARCHX_INSTRUCTOR;
  if (ai_text_has_sub_ci_local(sd, "commander") || ai_text_has_sub_ci_local(sd, "captain")) return AI_ARCHX_COMMANDER;
  if (ai_text_has_sub_ci_local(sd, "wolf") || ai_text_has_sub_ci_local(sd, "bear") || ai_text_has_sub_ci_local(sd, "beast")) return AI_ARCHX_BEAST_ANIMAL;
  if (ai_text_has_sub_ci_local(sd, "skeleton") || ai_text_has_sub_ci_local(sd, "zombie")) return AI_ARCHX_UNDEAD_MINDLESS;
  if (ai_text_has_sub_ci_local(sd, "spirit") || ai_text_has_sub_ci_local(sd, "wraith") || ai_text_has_sub_ci_local(sd, "ghost")) return AI_ARCHX_SPIRIT_ETHEREAL;
  return AI_ARCHX_CIVILIAN;
}

static unsigned long ai_rx_seed_mob(struct char_data *mob)
{
  unsigned long s = 1469598103UL;
  const char *n = mob ? GET_NAME(mob) : "";
  int i;
  for (i = 0; n && n[i]; i++) s = (s ^ (unsigned long)(unsigned char)n[i]) * 16777619UL;
  s ^= (unsigned long)(mob ? GET_MOB_VNUM(mob) : 0);
  s ^= (unsigned long)(mob ? GET_ALIGNMENT(mob) : 0);
  return s;
}

static void ai_rx_personality(struct char_data *mob, int ext_arch, struct ai_personality_ext *out)
{
  unsigned long seed;
  if (!out) return;
  memset(out, 0, sizeof(*out));
  seed = ai_rx_seed_mob(mob);
  out->bravery = 40 + (int)(seed % 41);
  out->empathy = 30 + (int)((seed >> 3) % 51);
  out->honor = 25 + (int)((seed >> 7) % 61);
  out->aggression = 20 + (int)((seed >> 11) % 71);
  out->lawfulness = 20 + (int)((seed >> 13) % 71);
  out->piety = 10 + (int)((seed >> 17) % 81);
  out->wit = 15 + (int)((seed >> 19) % 71);
  out->patience = 20 + (int)((seed >> 23) % 71);
  out->fearfulness = 10 + (int)((seed >> 27) % 71);
  if (ext_arch == AI_ARCHX_PALADIN_HOLY || ext_arch == AI_ARCHX_GUARD_LAW) { out->honor += 18; out->lawfulness += 20; }
  if (ext_arch == AI_ARCHX_HEALER_SUPPORT) { out->empathy += 22; out->aggression -= 10; }
  if (ext_arch == AI_ARCHX_BANDIT_CRIMINAL) { out->aggression += 20; out->honor -= 14; out->lawfulness -= 18; }
  out->bravery = URANGE(0, out->bravery, 100); out->empathy = URANGE(0, out->empathy, 100); out->honor = URANGE(0, out->honor, 100);
  out->aggression = URANGE(0, out->aggression, 100); out->lawfulness = URANGE(0, out->lawfulness, 100); out->piety = URANGE(0, out->piety, 100);
  out->wit = URANGE(0, out->wit, 100); out->patience = URANGE(0, out->patience, 100); out->fearfulness = URANGE(0, out->fearfulness, 100);
}

static struct ai_rel_bucket *ai_rel_bucket_get(struct char_data *mob, int create)
{
  int i, oldest = 0;
  for (i = 0; i < AI_REL_CACHE_MAX; i++) if (ai_rel_cache[i].mob == mob) return &ai_rel_cache[i];
  if (!create) return NULL;
  for (i = 0; i < AI_REL_CACHE_MAX; i++) {
    if (!ai_rel_cache[i].mob) { memset(&ai_rel_cache[i], 0, sizeof(ai_rel_cache[i])); ai_rel_cache[i].mob = mob; return &ai_rel_cache[i]; }
    if (ai_rel_cache[i].updated_at < ai_rel_cache[oldest].updated_at) oldest = i;
  }
  memset(&ai_rel_cache[oldest], 0, sizeof(ai_rel_cache[oldest])); ai_rel_cache[oldest].mob = mob; return &ai_rel_cache[oldest];
}

static struct ai_rel_entry *ai_rel_get(struct char_data *mob, long player_idnum, int create, time_t now)
{
  struct ai_rel_bucket *b = ai_rel_bucket_get(mob, create);
  int i, oldest = 0;
  if (!b || player_idnum <= 0) return NULL;
  for (i = 0; i < AI_REL_PER_MOB_MAX; i++) {
    if (b->entries[i].player_idnum == player_idnum) { b->entries[i].last_seen = now; b->updated_at = now; return &b->entries[i]; }
    if (b->entries[i].player_idnum == 0) oldest = i;
    else if (b->entries[i].last_seen < b->entries[oldest].last_seen) oldest = i;
  }
  if (!create) return NULL;
  memset(&b->entries[oldest], 0, sizeof(b->entries[oldest]));
  b->entries[oldest].player_idnum = player_idnum; b->entries[oldest].last_seen = now; b->updated_at = now;
  return &b->entries[oldest];
}

static void ai_rel_apply_event(struct char_data *mob, long player_idnum, enum ai_rx_intent intent, time_t now)
{
  struct ai_rel_entry *re = ai_rel_get(mob, player_idnum, 1, now);
  if (!re) return;
  if (intent == RX_INTENT_PRAISE || intent == RX_INTENT_APOLOGY || intent == RX_INTENT_FRIENDLY_SOCIAL) { re->trust += 5; re->gratitude += 4; re->respect += 2; }
  if (intent == RX_INTENT_INSULT || intent == RX_INTENT_THREATEN || intent == RX_INTENT_HOSTILE_SOCIAL) { re->trust -= 7; re->respect -= 6; re->fear += 4; }
  if (intent == RX_INTENT_REQUEST_HELP || intent == RX_INTENT_REQUEST_HEAL) re->trust += 1;
  re->trust = URANGE(-100, re->trust, 100); re->respect = URANGE(-100, re->respect, 100); re->fear = URANGE(-100, re->fear, 100); re->gratitude = URANGE(-100, re->gratitude, 100);
  re->last_interaction = now;
}

static int ai_rx_decide_override_line(struct char_data *mob, struct char_data *actor, const char *norm_text, enum ai_rx_intent intent, const struct ai_room_context *rc, const struct ai_moral_context *mc, char *out, size_t outsz, int *out_force_emote, time_t now)
{
  struct ai_personality_ext px;
  struct ai_rel_entry *re;
  int arch, roll, w_warn = 0, w_help = 0, w_heal = 0, w_call = 0, w_taunt = 0, w_flee = 0, total;
  (void)norm_text;
  if (!mob || !actor || !out || outsz == 0) return FALSE;
  arch = ai_rx_guess_ext_archetype(mob, mob->ai_prof ? mob->ai_prof->role : ROLE_UNKNOWN, mob->ai_prof ? mob->ai_prof->style : 0);
  ai_rx_personality(mob, arch, &px);
  re = ai_rel_get(mob, GET_IDNUM(actor), 1, now);
  roll = rand_number(1, 100);
  if (rc && rc->threat_level >= 2) { w_warn += 20; w_call += 20; }
  if (intent == RX_INTENT_INSULT || intent == RX_INTENT_THREATEN || intent == RX_INTENT_LOOT_THEFT) w_warn += 25;
  if (intent == RX_INTENT_REQUEST_HELP || intent == RX_INTENT_DAMAGE_EVENT || intent == RX_INTENT_COMBAT_START || intent == RX_INTENT_COMBAT_ONGOING) { w_help += 20; w_heal += 15; }
  if (arch == AI_ARCHX_PALADIN_HOLY || arch == AI_ARCHX_GUARD_LAW || arch == AI_ARCHX_COMMANDER) { w_warn += 20; w_call += 25; w_help += 15; }
  if (arch == AI_ARCHX_HEALER_SUPPORT) { w_heal += 35; w_help += 10; }
  if (arch == AI_ARCHX_MERCHANT || arch == AI_ARCHX_CIVILIAN) { w_call += 20; w_flee += 15; }
  if (arch == AI_ARCHX_BANDIT_CRIMINAL || IS_EVIL(mob)) { w_taunt += 25; w_help -= 10; }
  if (px.fearfulness > 65) w_flee += 20;
  if (re && re->trust > 20) { w_help += 10; w_heal += 8; }
  if (re && re->respect < -20) w_warn += 8;
  if (mc && mc->urgency > 60) { w_help += 12; w_heal += 12; w_call += 10; }
  total = MAX(1, w_warn + w_help + w_heal + w_call + w_taunt + w_flee);
  roll = rand_number(1, total);
  if (out_force_emote) *out_force_emote = 0;
  if ((roll -= MAX(0,w_heal)) <= 0) snprintf(out, outsz, "Hold still, I will help you.");
  else if ((roll -= MAX(0,w_help)) <= 0) snprintf(out, outsz, "Stand behind me, I'll intervene.");
  else if ((roll -= MAX(0,w_call)) <= 0) snprintf(out, outsz, "Guards! Trouble in this room!");
  else if ((roll -= MAX(0,w_warn)) <= 0) snprintf(out, outsz, "Enough. Keep the peace or face consequences.");
  else if ((roll -= MAX(0,w_flee)) <= 0) { snprintf(out, outsz, "backs away from the violence"); if (out_force_emote) *out_force_emote = 1; }
  else snprintf(out, outsz, "I've seen worse. Carry on, if you dare.");
  ai_rx_clean_sentence(out);
  return (intent == RX_INTENT_REQUEST_HELP || intent == RX_INTENT_REQUEST_HEAL || intent == RX_INTENT_THREATEN || intent == RX_INTENT_INSULT || (rc && rc->threat_level >= 2));
}

int ai_rx_process_event(const struct ai_rx_event *ev, struct ai_rx_result *out)
{
  struct ai_room_context room_ctx;
  struct ai_moral_context moral_ctx;
  enum ai_rx_intent rx_intent;
  time_t now = time(0);
  if (!out) return FALSE;
  memset(out, 0, sizeof(*out));
  if (!ev || !ev->mob || !MOB_FLAGGED(ev->mob, MOB_AI_ACTOR)) return FALSE;

  rx_intent = ai_rx_classify_intent((enum ai_event_type)ev->event_type, ev->normalized_text);
  ai_actor_build_room_context(ev->room, ev->normalized_text, rx_intent, &room_ctx);
  ai_rx_build_moral_context(ev->mob, ev->speaker, rx_intent, &room_ctx, &moral_ctx);
  if (ev->speaker)
    ai_rel_apply_event(ev->mob, GET_IDNUM(ev->speaker), rx_intent, now);

  out->intent = (int)rx_intent;
  out->threat = room_ctx.threat_level;
  out->urgency = moral_ctx.urgency;
  out->force_emote = 0;
  out->handled = ai_rx_decide_override_line(ev->mob, ev->speaker, ev->normalized_text, rx_intent, &room_ctx, &moral_ctx, out->line, sizeof(out->line), &out->force_emote, now);
  return out->handled;
}
