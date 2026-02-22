#include "conf.h"
#include "sysdep.h"

#include <ctype.h>

#include "structs.h"
#include "utils.h"
#include "db.h"
#include "comm.h"
#include "handler.h"
#include "act.h"
#include "ai_actor.h"
#include "ai_reactions.h"

#define AI_RX_MOB_CD_MAX 1024
#define AI_RX_ROOM_CD_MAX 512
#define AI_RX_ROOM_PLAYER_CD_MAX 1024
#define AI_RX_ROOM_PLAYER_HASH_CD_MAX 2048
#define AI_RX_ROOM_EVENT_MAX 512
#define AI_RX_HARASS_MAX 1024
#ifndef AI_RX_DEBUG_PLAN
#define AI_RX_DEBUG_PLAN 1
#endif

enum ai_rx_kind { AI_RX_KIND_EMOTE = 0, AI_RX_KIND_TINY_UTTERANCE, AI_RX_KIND_SHORT_LINE, AI_RX_KIND_ONE_LINER };
enum ai_rx_category {
  AI_RX_PROCESSING_BEAT = 0, AI_RX_EVALUATION_BEAT, AI_RX_MORAL_JUDGMENT, AI_RX_SOCIAL_TENSION,
  AI_RX_DEFENSIVE_POSTURE, AI_RX_CURIOUS_INTEREST, AI_RX_DISMISSAL, AI_RX_AMUSED, AI_RX_DISGUST,
  AI_RX_RESPECT, AI_RX_CONFUSION, AI_RX_WARNING, AI_RX_ESCALATION, AI_RX_DE_ESCALATION,
  AI_RX_INTRUSION_RESPONSE, AI_RX_BOUNDARY_RESPONSE, AI_RX_SERVICE_REFUSAL_REINFORCER,
  AI_RX_ROOM_ATMOSPHERE, AI_RX_SILENT_STARE, AI_RX_CAT_MAX
};

struct ai_reaction_result {
  int will_fire;
  int output_kind;
  int category;
  const char *selected_text;
  const char *debug_reason;
};

struct ai_rx_cd_mob { int mob_id; time_t next_allowed; };
struct ai_rx_cd_room { room_rnum room; int event_type; time_t next_allowed; };
struct ai_rx_cd_room_player { room_rnum room; long player_idnum; int event_type; time_t next_allowed; };
struct ai_rx_cd_room_player_hash { room_rnum room; long player_idnum; int mob_id; unsigned long text_hash; int event_type; time_t next_allowed; };
struct ai_rx_room_event { room_rnum room; int event_type; int count; time_t updated_at; };
struct ai_rx_harass { room_rnum room; long actor_idnum; int boundary_hits; time_t expires_at; };
struct ai_rx_room_voice { room_rnum room; time_t last_spoken; time_t next_ambience; };

static struct ai_rx_cd_mob ai_rx_mob_cd[AI_RX_MOB_CD_MAX];
static struct ai_rx_cd_room ai_rx_room_cd[AI_RX_ROOM_CD_MAX];
static struct ai_rx_cd_room_player ai_rx_room_player_cd[AI_RX_ROOM_PLAYER_CD_MAX];
static struct ai_rx_cd_room_player_hash ai_rx_room_player_hash_cd[AI_RX_ROOM_PLAYER_HASH_CD_MAX];
static struct ai_rx_room_event ai_rx_room_event[AI_RX_ROOM_EVENT_MAX];
static struct ai_rx_harass ai_rx_harass[AI_RX_HARASS_MAX];
static struct ai_rx_room_voice ai_rx_room_voice[AI_RX_ROOM_CD_MAX];

static int ai_text_has_sub_ci_local(const char *hay, const char *needle) {
  size_t nl, i, j; if (!hay || !needle || !*needle) return FALSE; nl = strlen(needle);
  for (i = 0; hay[i]; i++) { for (j = 0; j < nl; j++) { if (!hay[i + j]) return FALSE; if (tolower((unsigned char)hay[i + j]) != tolower((unsigned char)needle[j])) break; } if (j == nl) return TRUE; }
  return FALSE;
}

static unsigned long ai_rx_hash_mix(unsigned long h, unsigned long v) { h ^= v + 0x9e3779b9UL + (h << 6) + (h >> 2); return h; }
static int ai_rx_seed_pick(unsigned long seed, int n) { if (n <= 0) return 0; return (int)(seed % (unsigned long)n); }
static const char *ai_rx_pick_seeded(const char *const *pool, unsigned long seed) {
  int n = 0; while (pool && pool[n]) n++; if (n <= 0) return NULL; return pool[ai_rx_seed_pick(seed, n)];
}

static const char *const rx_processing_generic[] = {
  "$n pauses, weighing your words.", "$n tilts $s head, considering.", "$n breathes out, thinking.",
  "$n studies you in silence.", "$n glances aside, then back.", "$n keeps still for a beat.",
  "$n folds $s hands and listens.", "$n nods once, slowly.", "$n narrows $s gaze in thought.",
  "$n taps two fingers, mulling it over.", NULL
};
static const char *const rx_eval_guard[] = {
  "$n scans the room and rechecks you.", "$n squares $s shoulders and evaluates the risk.",
  "$n shifts stance, watchful.", "$n watches your hands, not your smile.", "$n marks exits with a brief glance.", NULL
};
static const char *const rx_eval_merchant[] = {
  "$n sizes you up like a hard bargain.", "$n eyes your pack, then your face.", "$n drums fingers on the counter, appraising.",
  "$n checks your tone before your coin.", "$n weighs your request with trader calm.", NULL
};
static const char *const rx_tension_guard[] = {
  "$n rests a hand near $s weapon.", "$n plants $s feet and holds the line.", "$n gives a warning look.",
  "$n shifts to block a clean approach.", "$n stares until the room stills.", NULL
};
static const char *const rx_tension_bandit[] = {
  "$n smirks without warmth.", "$n leans in, testing your nerve.", "$n rolls $s shoulders, coiled.",
  "$n watches like a wolf at dusk.", "$n cracks knuckles and waits.", NULL
};
static const char *const rx_moral_good[] = {
  "$n frowns at the cruelty in your words.", "$n shakes $s head in disapproval.", "$n gives a look that says enough.",
  "$n's jaw tightens at that.", "$n refuses with calm certainty.", NULL
};
static const char *const rx_moral_evil[] = {
  "$n smiles thinly at the threat.", "$n seems entertained by the tension.", "$n gives a cold, approving nod.",
  "$n watches with hungry patience.", "$n looks pleased by the discord.", NULL
};
static const char *const rx_boundary[] = {
  "$n gives you a flat look.", "$n steps back, unimpressed.", "$n raises a hand in refusal.",
  "$n's expression hardens: no.", "$n turns the conversation away.", "$n does not indulge that.",
  "$n's answer is firm and final.", "$n keeps a respectful distance.", NULL
};
static const char *const rx_confusion[] = {
  "$n blinks, uncertain.", "$n tilts $s head, puzzled.", "$n mouths a quiet, 'what?'", "$n squints at the nonsense.",
  "$n waits for a clearer question.", NULL
};
static const char *const rx_escalation[] = {
  "$n's posture hardens.", "$n points to the door, stern.", "$n's patience runs out.", "$n stands taller, warning clear.", NULL
};
static const char *const rx_de_escalation[] = {
  "$n eases $s stance a little.", "$n lowers $s voice and steadies the room.", "$n gestures for calm.", "$n nods, tension fading.", NULL
};
static const char *const rx_intrusion[] = {
  "$n shields $s belongings with a sharp look.", "$n guards the counter with both hands.", "$n gives a hard no to that idea.", NULL
};
static const char *const rx_service_refusal[] = {
  "No.", "Not happening.", "Ask something lawful.", "Try a different request.", NULL
};
static const char *const rx_silent_stare[] = { "$n just stares.", "$n says nothing.", "$n remains unreadable.", NULL };
static const char *const rx_tiny[] = { "Hm.", "Tch.", "Hmph.", "No.", NULL };

static const char *const rx_one_liner[] = {
  "Evening.", "I can help if you ask clean.", "What do you need.", "That depends.", "Say it plain.", NULL
};

static const char *const rx_plan_neutral_greet[] = {"Hello.", "Yes?", "What do you need?", NULL};
static const char *const rx_plan_guard_greet[] = {"State your business.", "Keep it brief.", "Move along if you have no business.", NULL};
static const char *const rx_plan_constable_greet[] = {"Constable on duty. Speak plain.", "Order first. What is your business?", NULL};
static const char *const rx_plan_merchant_greet[] = {"Welcome. Need wares?", "Trade fair and speak plain.", NULL};
static const char *const rx_plan_innkeeper_greet[] = {"Well met. Need food or a bed?", "Welcome in. What can I get you?", NULL};
static const char *const rx_plan_bandit_greet[] = {"Move it.", "Talk fast.", "You lost, traveler?", NULL};
static const char *const rx_plan_instructor_greet[] = {"Ready to train? Ask clearly.", "Discipline first. What lesson do you seek?", NULL};

static const char *const rx_plan_guard_demand[] = {"No.", "That is not happening.", "Stand down.", NULL};
static const char *const rx_plan_merchant_demand[] = {"Coin first.", "No coin, no deal.", NULL};
static const char *const rx_plan_innkeeper_demand[] = {"Pay first, then service.", "We do not do charity at knifepoint.", NULL};
static const char *const rx_plan_bandit_demand[] = {"Try it and bleed for it.", "You demand too much.", NULL};
static const char *const rx_plan_instructor_demand[] = {"Discipline before demands.", "Ask properly or leave.", NULL};
static const char *const fallback_guard[] = {
  "State your need clearly.","Speak plainly.","One request at a time.","Keep it lawful and concise.",
  "Report the matter in plain words.","Stay calm and be specific.","I need a clear request.","Keep to one topic.",
  "Say exactly what service you need.","Brief and lawful, citizen.","No riddles. State your business.","Make your request direct.",NULL
};
static const char *const fallback_constable[] = {
  "File your request properly.","Specify the service required.","Submit one clear request.","State the matter for the record.",
  "Clarify your petition in formal terms.","One issue per request.","I require a precise statement.","Identify the exact service sought.",
  "Present your request in order.","Keep the report concise.","Define your need without ambiguity.","Proceed with a proper request.",NULL
};
static const char *const fallback_merchant[] = {
  "What category are you after?","Weapons, armor, or something finer?","Name the goods and I'll quote coin.","Are you buying, selling, or browsing?",
  "Pick a wares category and we can trade.","Tell me the item class you want.","Coin talks-what exactly do you need?","Give me one product line at a time.",
  "Trade starts with specifics.","State the merchandise clearly.","Need supplies, steel, or sundries?","Choose the goods and we'll deal.",NULL
};
static const char *const fallback_innkeeper[] = {
  "Room, meal, or ale?","Say if you need a bed or a bowl.","Pick one: lodging, food, or drink.","Travel's easier with a clear order.",
  "Bed, bread, or brew?","Name your comfort and I'll serve it.","One tab at a time-room or refreshment.","You after rest, rations, or a mug?",
  "Tell me if it's cot, stew, or cask.","Make it simple: room, meal, or ale.","What can I pour or prepare for you?","Choose your inn service plainly.",NULL
};
static const char *const fallback_bandit[] = {
  "Talk straight or keep walking.","One ask, quick.","Don't waste my time with foggy words.","Pick a lane and spit it out.",
  "You want coin, cover, or directions?","Say it clean before my patience snaps.","One hustle at a time.","Make your angle obvious.",
  "Clear asks get answers. Maybe.","Keep it short and useful.","Stop circling-state your play.","One request, then move.",NULL
};
static const char *const fallback_instructor[] = {
  "Discipline your question.","State one training objective.","Specify the lesson you seek.","One drill request at a time.",
  "Name the skill to improve.","Clarity is the first exercise.","Choose one technique to discuss.","Ask with precision and focus.",
  "Define your need like a student.","Pick a single training goal.","Refine your request and continue.","Instruction begins with a clear ask.",NULL
};
static const char *const fallback_neutral[] = {
  "I need a clearer request.","Ask one thing at a time.","Say that more plainly.","Clarify what you need.",
  "Give me one specific question.","Keep your request concise.","I can help when it's clearer.","Try a direct question.",
  "Name the service or topic.","Let's keep this simple.","State your request directly.","What exactly are you asking?",NULL
};

int ai_rx_infer_targeted_to_mob(struct char_data *mob, const char *norm_text) {
  if (!mob || !norm_text) return FALSE;
  if (ai_text_has_sub_ci_local(norm_text, "you") || ai_text_has_sub_ci_local(norm_text, "@")) return TRUE;
  return ai_text_has_sub_ci_local(norm_text, GET_NAME(mob));
}
int ai_rx_is_service_style_request(const char *norm_text) {
  if (!norm_text || !*norm_text) return FALSE;
  return ai_text_has_sub_ci_local(norm_text, "food") || ai_text_has_sub_ci_local(norm_text, "water") || ai_text_has_sub_ci_local(norm_text, "rest") ||
         ai_text_has_sub_ci_local(norm_text, "inn") || ai_text_has_sub_ci_local(norm_text, "training") || ai_text_has_sub_ci_local(norm_text, "directions");
}
int ai_rx_is_explicit_sexual_request(const char *norm_text) {
  if (!norm_text || !*norm_text) return FALSE;
  return ai_text_has_sub_ci_local(norm_text, "sex") || ai_text_has_sub_ci_local(norm_text, "fuck") || ai_text_has_sub_ci_local(norm_text, "nude") ||
         ai_text_has_sub_ci_local(norm_text, "bed me") || ai_text_has_sub_ci_local(norm_text, "sleep with me");
}
void ai_rx_clean_sentence(char *s) { (void)s; }

static enum ai_rx_intent ai_rx_classify_intent(enum ai_event_type type, const char *norm_text) {
  if (type == AI_EVENT_COMBAT_START) return RX_INTENT_COMBAT_START;
  if (!norm_text || !*norm_text) return RX_INTENT_NONE;
  if (ai_rx_is_explicit_sexual_request(norm_text)) return RX_INTENT_FLIRT_LEWD;
  if (ai_text_has_sub_ci_local(norm_text, "buy") || ai_text_has_sub_ci_local(norm_text, "shop") || ai_text_has_sub_ci_local(norm_text, "purchase") || ai_text_has_sub_ci_local(norm_text, "market") || ai_text_has_sub_ci_local(norm_text, "stuff")) return RX_INTENT_REQUEST_TRADE;
  if (ai_text_has_sub_ci_local(norm_text, "help") || ai_text_has_sub_ci_local(norm_text, "assist") || ai_text_has_sub_ci_local(norm_text, "guide") || ai_text_has_sub_ci_local(norm_text, "where")) return RX_INTENT_REQUEST_HELP;
  if (ai_text_has_sub_ci_local(norm_text, "give me") || ai_text_has_sub_ci_local(norm_text, "gold") || ai_text_has_sub_ci_local(norm_text, "money") || ai_text_has_sub_ci_local(norm_text, "coins")) return RX_INTENT_REQUEST_TRADE;
  if (ai_text_has_sub_ci_local(norm_text, "hello") || ai_text_has_sub_ci_local(norm_text, "hi") || ai_text_has_sub_ci_local(norm_text, "greetings")) return RX_INTENT_GREETING;
  if (ai_text_has_sub_ci_local(norm_text, "kiss") || ai_text_has_sub_ci_local(norm_text, "love") || ai_text_has_sub_ci_local(norm_text, "flirt")) return RX_INTENT_FRIENDLY_SOCIAL;
  if (ai_text_has_sub_ci_local(norm_text, "thanks")) return RX_INTENT_PRAISE;
  if (ai_text_has_sub_ci_local(norm_text, "idiot") || ai_text_has_sub_ci_local(norm_text, "stupid") || ai_text_has_sub_ci_local(norm_text, "moron") || ai_text_has_sub_ci_local(norm_text, "fool")) return RX_INTENT_INSULT;
  if (ai_text_has_sub_ci_local(norm_text, "steal") || ai_text_has_sub_ci_local(norm_text, "rob") || ai_text_has_sub_ci_local(norm_text, "mug")) return RX_INTENT_LOOT_THEFT;
  if (ai_text_has_sub_ci_local(norm_text, "threat") || ai_text_has_sub_ci_local(norm_text, "kill") || ai_text_has_sub_ci_local(norm_text, "hurt")) return RX_INTENT_THREATEN;
  if (ai_text_has_sub_ci_local(norm_text, "help")) return RX_INTENT_REQUEST_HELP;
  if (strchr(norm_text, '?')) return RX_INTENT_QUESTION;
  return RX_INTENT_SMALLTALK;
}

int ai_rx_process_event(const struct ai_rx_event *ev, struct ai_rx_result *out) {
  enum ai_rx_intent it;
  if (!out) return FALSE;
  memset(out, 0, sizeof(*out));
  if (!ev || !ev->mob) return FALSE;
  it = ai_rx_classify_intent((enum ai_event_type)ev->event_type, ev->normalized_text);
  out->intent = (int)it;
  out->threat = (it == RX_INTENT_THREATEN || it == RX_INTENT_LOOT_THEFT) ? 3 : 1;
  out->urgency = (it == RX_INTENT_THREATEN) ? 70 : (it == RX_INTENT_FLIRT_LEWD ? 55 : 20);
  return FALSE;
}

static struct ai_rx_room_event *ai_rx_room_event_get(room_rnum room, int event_type, int create) {
  int i, oldest = 0;
  for (i = 0; i < AI_RX_ROOM_EVENT_MAX; i++) if (ai_rx_room_event[i].room == room && ai_rx_room_event[i].event_type == event_type) return &ai_rx_room_event[i];
  if (!create) return NULL;
  for (i = 0; i < AI_RX_ROOM_EVENT_MAX; i++) {
    if (ai_rx_room_event[i].updated_at == 0) return &ai_rx_room_event[i];
    if (ai_rx_room_event[i].updated_at < ai_rx_room_event[oldest].updated_at) oldest = i;
  }
  return &ai_rx_room_event[oldest];
}

void ai_reactions_room_event_reset(room_rnum room, int event_type) {
  struct ai_rx_room_event *e = ai_rx_room_event_get(room, event_type, 1);
  if (!e) return;
  e->room = room; e->event_type = event_type; e->count = 0; e->updated_at = time(0);
}

static struct ai_rx_room_voice *ai_rx_room_voice_get(room_rnum room, int create) {
  int i, oldest = 0;
  for (i = 0; i < AI_RX_ROOM_CD_MAX; i++) {
    if (ai_rx_room_voice[i].room == room)
      return &ai_rx_room_voice[i];
    if (ai_rx_room_voice[i].room == NOWHERE && !create)
      continue;
    if (ai_rx_room_voice[i].last_spoken < ai_rx_room_voice[oldest].last_spoken)
      oldest = i;
  }
  if (!create) return NULL;
  ai_rx_room_voice[oldest].room = room;
  return &ai_rx_room_voice[oldest];
}

static int ai_rx_harass_level(room_rnum room, long actor, int bump) {
  int i, oldest = 0; time_t now = time(0);
  for (i = 0; i < AI_RX_HARASS_MAX; i++) {
    if (ai_rx_harass[i].expires_at < now) { memset(&ai_rx_harass[i], 0, sizeof(ai_rx_harass[i])); }
    if (ai_rx_harass[i].room == room && ai_rx_harass[i].actor_idnum == actor) {
      if (bump) ai_rx_harass[i].boundary_hits++;
      ai_rx_harass[i].expires_at = now + 120;
      return ai_rx_harass[i].boundary_hits;
    }
    if (ai_rx_harass[i].expires_at < ai_rx_harass[oldest].expires_at) oldest = i;
  }
  ai_rx_harass[oldest].room = room; ai_rx_harass[oldest].actor_idnum = actor; ai_rx_harass[oldest].boundary_hits = bump ? 1 : 0; ai_rx_harass[oldest].expires_at = now + 120;
  return ai_rx_harass[oldest].boundary_hits;
}

static int ai_rx_can_fire(struct char_data *mob, const struct ai_reaction_ctx *ctx, const char **why) {
  int i, mob_id = GET_IDNUM(mob); time_t now = time(0);
  struct ai_rx_room_event *ev;
  if (!ctx || !mob) { *why = "bad_ctx"; return FALSE; }
  if (ctx->event_type != AI_EVENT_PLAYER_SAY) { *why = "event_not_player_say"; return FALSE; }
  if (!ctx->can_act || ctx->is_sleeping || ctx->is_stunned || ctx->is_charmed || ctx->is_fighting) { *why = "cannot_act"; return FALSE; }
  if (mob_id <= 0) mob_id = GET_MOB_VNUM(mob);
  for (i = 0; i < AI_RX_MOB_CD_MAX; i++) if (ai_rx_mob_cd[i].mob_id == mob_id && ai_rx_mob_cd[i].next_allowed > now) { *why = "mob_cooldown"; return FALSE; }
  for (i = 0; i < AI_RX_ROOM_CD_MAX; i++) if (ai_rx_room_cd[i].room == ctx->room_rnum && ai_rx_room_cd[i].event_type == ctx->event_type && ai_rx_room_cd[i].next_allowed > now) {
    struct ai_rx_room_voice *rv = ai_rx_room_voice_get(ctx->room_rnum, 1);
    if (rv && (now - rv->last_spoken) > 8 && now >= rv->next_ambience) {
      rv->next_ambience = now + 8;
      *why = "allow_ambience";
      log("AI_RX_RULE_ALLOW_AMBIENCE reason=no_speech_recent");
      return TRUE;
    }
    *why = "room_cooldown"; return FALSE;
  }
  for (i = 0; i < AI_RX_ROOM_PLAYER_HASH_CD_MAX; i++) if (ai_rx_room_player_hash_cd[i].room == ctx->room_rnum && ai_rx_room_player_hash_cd[i].player_idnum == ctx->actor_idnum && ai_rx_room_player_hash_cd[i].mob_id == mob_id && ai_rx_room_player_hash_cd[i].event_type == ctx->event_type && ai_rx_room_player_hash_cd[i].text_hash == ctx->normalized_hash && ai_rx_room_player_hash_cd[i].next_allowed > now) { *why = "same_mob_hash_cooldown"; return FALSE; }
  ev = ai_rx_room_event_get(ctx->room_rnum, ctx->event_type, 1);
  if (ev && ev->updated_at && (now - ev->updated_at) > 5) ev->count = 0;
  *why = "ok";
  return TRUE;
}


static const char *ai_rx_persona_name(enum ai_actor_persona p) {
  switch (p) {
    case AI_PERSONA_GUARD: return "guard";
    case AI_PERSONA_CONSTABLE: return "constable";
    case AI_PERSONA_MERCHANT: return "merchant";
    case AI_PERSONA_INNKEEPER: return "innkeeper";
    case AI_PERSONA_BANDIT: return "bandit";
    case AI_PERSONA_INSTRUCTOR: return "instructor";
    default: return "neutral";
  }
}

static const char *ai_rx_pick_plan_line(enum ai_actor_persona persona, int intent_id, unsigned long seed, const char **out_pool)
{
  const char *const *pool = rx_plan_neutral_greet;
  int demand = (intent_id == AI_INTENT_THREAT || intent_id == AI_INTENT_INSULT);
  int greet = (intent_id == AI_INTENT_GREET || intent_id == AI_INTENT_SMALLTALK);

  if (demand) {
    switch (persona) {
      case AI_PERSONA_GUARD:
      case AI_PERSONA_CONSTABLE: pool = rx_plan_guard_demand; *out_pool = "guard_demand"; break;
      case AI_PERSONA_MERCHANT: pool = rx_plan_merchant_demand; *out_pool = "merchant_demand"; break;
      case AI_PERSONA_INNKEEPER: pool = rx_plan_innkeeper_demand; *out_pool = "innkeeper_demand"; break;
      case AI_PERSONA_BANDIT: pool = rx_plan_bandit_demand; *out_pool = "bandit_demand"; break;
      case AI_PERSONA_INSTRUCTOR: pool = rx_plan_instructor_demand; *out_pool = "instructor_demand"; break;
      default: pool = rx_plan_guard_demand; *out_pool = "neutral_demand"; break;
    }
  } else if (greet) {
    switch (persona) {
      case AI_PERSONA_GUARD: pool = rx_plan_guard_greet; *out_pool = "guard_greet"; break;
      case AI_PERSONA_CONSTABLE: pool = rx_plan_constable_greet; *out_pool = "constable_greet"; break;
      case AI_PERSONA_MERCHANT: pool = rx_plan_merchant_greet; *out_pool = "merchant_greet"; break;
      case AI_PERSONA_INNKEEPER: pool = rx_plan_innkeeper_greet; *out_pool = "innkeeper_greet"; break;
      case AI_PERSONA_BANDIT: pool = rx_plan_bandit_greet; *out_pool = "bandit_greet"; break;
      case AI_PERSONA_INSTRUCTOR: pool = rx_plan_instructor_greet; *out_pool = "instructor_greet"; break;
      default: pool = rx_plan_neutral_greet; *out_pool = "neutral_greet"; break;
    }
  } else {
    switch (persona) {
      case AI_PERSONA_BANDIT: pool = fallback_bandit; *out_pool = "fallback_bandit"; break;
      case AI_PERSONA_GUARD: pool = fallback_guard; *out_pool = "fallback_guard"; break;
      case AI_PERSONA_CONSTABLE: pool = fallback_constable; *out_pool = "fallback_constable"; break;
      case AI_PERSONA_MERCHANT: pool = fallback_merchant; *out_pool = "fallback_merchant"; break;
      case AI_PERSONA_INNKEEPER: pool = fallback_innkeeper; *out_pool = "fallback_innkeeper"; break;
      case AI_PERSONA_INSTRUCTOR: pool = fallback_instructor; *out_pool = "fallback_instructor"; break;
      default: pool = fallback_neutral; *out_pool = "fallback_neutral"; break;
    }
  }

  return ai_rx_pick_seeded(pool, seed);
}

static int ai_reaction_category_from_intent(int intent_id, const struct ai_reaction_ctx *ctx) {
  if (ai_rx_is_explicit_sexual_request(ctx ? ctx->normalized_text : NULL)) return AI_RX_BOUNDARY_RESPONSE;
  if (intent_id == AI_INTENT_THREAT || intent_id == AI_INTENT_INSULT) return AI_RX_WARNING;
  if (intent_id == AI_INTENT_ASK_SERVICE) return AI_RX_EVALUATION_BEAT;
  if (intent_id == AI_INTENT_GIBBERISH || intent_id == AI_INTENT_CONFUSION) return AI_RX_CONFUSION;
  if (intent_id == AI_INTENT_GREET || intent_id == AI_INTENT_SMALLTALK) return AI_RX_PROCESSING_BEAT;
  return AI_RX_PROCESSING_BEAT;
}

static void ai_reaction_pick(struct char_data *mob, const struct ai_reaction_ctx *ctx, struct ai_reaction_result *out) {
  int cat, threaty, harassment; unsigned long seed;
  struct ai_rx_room_event *ev;
  memset(out, 0, sizeof(*out));
  if (!mob || !ctx) return;
  ev = ai_rx_room_event_get(ctx->room_rnum, ctx->event_type, 1);
  cat = ai_reaction_category_from_intent(ctx->intent_id, ctx);
  threaty = (ctx->threat >= 2 || ctx->suspicion > 0.65f || ctx->intent_id == AI_INTENT_THREAT);
  if (threaty && ctx->mob_role == ROLE_GUARD) cat = AI_RX_DEFENSIVE_POSTURE;
  else if (threaty && ctx->mob_role == ROLE_BANDIT) cat = AI_RX_SOCIAL_TENSION;
  else if (ctx->suspicion > 0.45f) cat = AI_RX_EVALUATION_BEAT;
  harassment = ai_rx_harass_level(ctx->room_rnum, ctx->actor_idnum, cat == AI_RX_BOUNDARY_RESPONSE ? 1 : 0);
  if (harassment >= 3 && cat == AI_RX_BOUNDARY_RESPONSE) cat = AI_RX_ESCALATION;

  if (ev && ev->count >= 3) cat = AI_RX_SILENT_STARE;

  out->will_fire = TRUE;
  out->category = cat;
  out->output_kind = (cat == AI_RX_SERVICE_REFUSAL_REINFORCER) ? AI_RX_KIND_TINY_UTTERANCE : AI_RX_KIND_EMOTE;
  if (ctx->trigger_reason == AI_RX_TRIG_ARB_SLOT_DENIED_EARLY && (ctx->mob_alignment >= -350) && (ctx->mob_role == ROLE_GUARD || ctx->mob_role == ROLE_MERCHANT || ctx->mob_role == ROLE_CIVILIAN))
    out->output_kind = AI_RX_KIND_ONE_LINER;
  if (ctx->intent_id == AI_INTENT_INSULT || ctx->intent_id == AI_INTENT_THREAT) {
    if (ctx->mob_alignment >= -350 && (ctx->mob_role == ROLE_GUARD || ctx->mob_role == ROLE_MERCHANT))
      cat = AI_RX_DE_ESCALATION;
    else if (ctx->mob_role == ROLE_MERCHANT)
      cat = AI_RX_SERVICE_REFUSAL_REINFORCER;
  }
  if (ctx->trigger_reason == AI_RX_TRIG_NON_SPEAK_ACTION_SELECTED)
    out->output_kind = AI_RX_KIND_TINY_UTTERANCE;
  seed = 1469598103UL;
  seed = ai_rx_hash_mix(seed, (unsigned long)ctx->room_rnum);
  seed = ai_rx_hash_mix(seed, (unsigned long)ctx->mob_vnum);
  seed = ai_rx_hash_mix(seed, (unsigned long)ctx->actor_idnum);
  seed = ai_rx_hash_mix(seed, (unsigned long)ctx->normalized_hash);
  seed = ai_rx_hash_mix(seed, (unsigned long)cat);

  if (out->output_kind == AI_RX_KIND_ONE_LINER || ctx->event_type == AI_EVENT_PLAYER_SAY) {
    enum ai_actor_persona persona = get_actor_persona(mob);
    const char *pool_name = "";
    out->output_kind = AI_RX_KIND_ONE_LINER;
    out->selected_text = ai_rx_pick_plan_line(persona, ctx->intent_id, seed, &pool_name);
    out->debug_reason = pool_name;
#if AI_RX_DEBUG_PLAN
    log("AI_RX_PLAN intent=%d persona=%s pool=%s", ctx->intent_id, ai_rx_persona_name(persona), pool_name ? pool_name : "none");
#endif
    if (!out->selected_text) {
      out->will_fire = FALSE;
      out->debug_reason = "empty_plan";
    }
    return;
  }

  switch (cat) {
    case AI_RX_BOUNDARY_RESPONSE: out->selected_text = ai_rx_pick_seeded(rx_boundary, seed); out->debug_reason = "boundary"; break;
    case AI_RX_ESCALATION: out->selected_text = ai_rx_pick_seeded(rx_escalation, seed); out->debug_reason = "escalation"; break;
    case AI_RX_DEFENSIVE_POSTURE: out->selected_text = ai_rx_pick_seeded(rx_tension_guard, seed); out->debug_reason = "guard_posture"; break;
    case AI_RX_SOCIAL_TENSION: out->selected_text = ai_rx_pick_seeded(rx_tension_bandit, seed); out->debug_reason = "bandit_tension"; break;
    case AI_RX_EVALUATION_BEAT: out->selected_text = ai_rx_pick_seeded((ctx->mob_role == ROLE_MERCHANT) ? rx_eval_merchant : rx_eval_guard, seed); out->debug_reason = "evaluation"; break;
    case AI_RX_WARNING: out->selected_text = ai_rx_pick_seeded(rx_intrusion, seed); out->debug_reason = "warning"; break;
    case AI_RX_CONFUSION: out->selected_text = ai_rx_pick_seeded(rx_confusion, seed); out->debug_reason = "confusion"; break;
    case AI_RX_SILENT_STARE: out->selected_text = ai_rx_pick_seeded(rx_tiny, seed); out->debug_reason = "degraded_cap"; break;
    case AI_RX_MORAL_JUDGMENT: out->selected_text = ai_rx_pick_seeded(IS_GOOD(mob) ? rx_moral_good : rx_moral_evil, seed); out->debug_reason = "moral"; break;
    case AI_RX_DE_ESCALATION: out->selected_text = ai_rx_pick_seeded(rx_de_escalation, seed); out->debug_reason = "de_escalation"; break;
    case AI_RX_SERVICE_REFUSAL_REINFORCER: out->selected_text = ai_rx_pick_seeded(rx_service_refusal, seed); out->debug_reason = "service_refusal"; break;
    default: out->selected_text = ai_rx_pick_seeded(rx_processing_generic, seed); out->debug_reason = "processing"; break;
  }
  if (out->output_kind == AI_RX_KIND_ONE_LINER)
    out->selected_text = ai_rx_pick_seeded(rx_one_liner, seed);
  if (!out->selected_text) { out->will_fire = FALSE; out->debug_reason = "empty_pool"; }
}

static void ai_reaction_fire(struct char_data *mob, const struct ai_reaction_result *r) {
  char buf[256];
  if (!mob || !r || !r->selected_text || !*r->selected_text) return;
  if (r->output_kind == AI_RX_KIND_EMOTE || !strncmp(r->selected_text, "$n ", 3)) {
    snprintf(buf, sizeof(buf), "%.*s", (int)sizeof(buf)-1, (!strncmp(r->selected_text, "$n ", 3) ? r->selected_text + 3 : r->selected_text));
    do_echo(mob, buf, 0, SCMD_EMOTE);
  } else {
    struct ai_rx_room_voice *rv = ai_rx_room_voice_get(IN_ROOM(mob), 1);
    snprintf(buf, sizeof(buf), "%.*s", (int)sizeof(buf)-1, r->selected_text);
    do_say(mob, buf, 0, 0);
    if (rv) rv->last_spoken = time(0);
  }
}

int ai_reaction_try(struct char_data *mob, const struct ai_reaction_ctx *ctx) {
  const char *why = "";
  struct ai_reaction_result r;
  int i, mob_id = GET_IDNUM(mob), oldest = 0;
  time_t now = time(0);
  struct ai_rx_room_event *ev;

  if (!ai_rx_can_fire(mob, ctx, &why)) {
    log("AI_RX_SUPPRESS reason=%s one_line_rule=%d", why ? why : "unknown", (why && !strcmp(why, "room_player_hash_cooldown")) ? 1 : 0);
    return 0;
  }

  ai_reaction_pick(mob, ctx, &r);
  if (!r.will_fire || !r.selected_text) {
    log("AI_RX_SUPPRESS reason=no_pick");
    return 0;
  }

  log("AI_RX_PICK kind=%s category=%d text=\"%s\"", (r.output_kind == AI_RX_KIND_ONE_LINER ? "ONE_LINER" : (r.output_kind == AI_RX_KIND_EMOTE ? "EMOTE" : (r.output_kind == AI_RX_KIND_TINY_UTTERANCE ? "TINY" : "SHORT"))), r.category, r.selected_text);
  ai_reaction_fire(mob, &r);
  log("AI_RX_FIRE vnum=%d role=%d trigger=%d", GET_MOB_VNUM(mob), ctx ? ctx->mob_role : -1, ctx ? ctx->trigger_reason : -1);

  if (mob_id <= 0) mob_id = GET_MOB_VNUM(mob);
  for (i = 0; i < AI_RX_MOB_CD_MAX; i++) { if (ai_rx_mob_cd[i].mob_id == mob_id || ai_rx_mob_cd[i].next_allowed == 0) { ai_rx_mob_cd[i].mob_id = mob_id; ai_rx_mob_cd[i].next_allowed = now + rand_number(8, 20); break; } }
  for (i = 0; i < AI_RX_ROOM_CD_MAX; i++) { if ((ai_rx_room_cd[i].room == ctx->room_rnum && ai_rx_room_cd[i].event_type == ctx->event_type) || ai_rx_room_cd[i].next_allowed == 0) { int cd = rand_number(2, 4); if (ctx->trigger_reason == AI_RX_TRIG_NON_SPEAK_ACTION_SELECTED) cd = rand_number(1, 2); else if (ctx->trigger_reason == AI_RX_TRIG_ARB_SLOT_DENIED_EARLY) cd = rand_number(2, 3); else if (ctx->event_type == AI_EVENT_PLAYER_SAY) cd = rand_number(3, 5); ai_rx_room_cd[i].room = ctx->room_rnum; ai_rx_room_cd[i].event_type = ctx->event_type; ai_rx_room_cd[i].next_allowed = now + cd; break; } }
  for (i = 0; i < AI_RX_ROOM_PLAYER_CD_MAX; i++) { if ((ai_rx_room_player_cd[i].room == ctx->room_rnum && ai_rx_room_player_cd[i].player_idnum == ctx->actor_idnum && ai_rx_room_player_cd[i].event_type == ctx->event_type) || ai_rx_room_player_cd[i].next_allowed == 0) { ai_rx_room_player_cd[i].room = ctx->room_rnum; ai_rx_room_player_cd[i].player_idnum = ctx->actor_idnum; ai_rx_room_player_cd[i].event_type = ctx->event_type; ai_rx_room_player_cd[i].next_allowed = now + 6; break; } }
  for (i = 0; i < AI_RX_ROOM_PLAYER_HASH_CD_MAX; i++) { if ((ai_rx_room_player_hash_cd[i].room == ctx->room_rnum && ai_rx_room_player_hash_cd[i].player_idnum == ctx->actor_idnum && ai_rx_room_player_hash_cd[i].mob_id == mob_id && ai_rx_room_player_hash_cd[i].event_type == ctx->event_type && ai_rx_room_player_hash_cd[i].text_hash == ctx->normalized_hash) || ai_rx_room_player_hash_cd[i].next_allowed == 0) { ai_rx_room_player_hash_cd[i].room = ctx->room_rnum; ai_rx_room_player_hash_cd[i].player_idnum = ctx->actor_idnum; ai_rx_room_player_hash_cd[i].mob_id = mob_id; ai_rx_room_player_hash_cd[i].event_type = ctx->event_type; ai_rx_room_player_hash_cd[i].text_hash = ctx->normalized_hash; ai_rx_room_player_hash_cd[i].next_allowed = now + 3; break; } }

  ev = ai_rx_room_event_get(ctx->room_rnum, ctx->event_type, 1);
  if (ev) { ev->room = ctx->room_rnum; ev->event_type = ctx->event_type; ev->count++; ev->updated_at = now; }

  (void)oldest;
  return 1;
}
