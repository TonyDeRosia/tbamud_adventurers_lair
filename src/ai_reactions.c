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

enum ai_rx_kind { AI_RX_KIND_EMOTE = 0, AI_RX_KIND_TINY_UTTERANCE, AI_RX_KIND_SHORT_LINE };
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
struct ai_rx_cd_room_player_hash { room_rnum room; long player_idnum; unsigned long text_hash; int event_type; time_t next_allowed; };
struct ai_rx_room_event { room_rnum room; int event_type; int count; time_t updated_at; };
struct ai_rx_harass { room_rnum room; long actor_idnum; int boundary_hits; time_t expires_at; };

static struct ai_rx_cd_mob ai_rx_mob_cd[AI_RX_MOB_CD_MAX];
static struct ai_rx_cd_room ai_rx_room_cd[AI_RX_ROOM_CD_MAX];
static struct ai_rx_cd_room_player ai_rx_room_player_cd[AI_RX_ROOM_PLAYER_CD_MAX];
static struct ai_rx_cd_room_player_hash ai_rx_room_player_hash_cd[AI_RX_ROOM_PLAYER_HASH_CD_MAX];
static struct ai_rx_room_event ai_rx_room_event[AI_RX_ROOM_EVENT_MAX];
static struct ai_rx_harass ai_rx_harass[AI_RX_HARASS_MAX];

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
  if (ai_text_has_sub_ci_local(norm_text, "steal") || ai_text_has_sub_ci_local(norm_text, "rob") || ai_text_has_sub_ci_local(norm_text, "mug")) return RX_INTENT_LOOT_THEFT;
  if (ai_text_has_sub_ci_local(norm_text, "threat") || ai_text_has_sub_ci_local(norm_text, "kill") || ai_text_has_sub_ci_local(norm_text, "hurt")) return RX_INTENT_THREATEN;
  if (ai_text_has_sub_ci_local(norm_text, "help")) return RX_INTENT_REQUEST_HELP;
  if (ai_text_has_sub_ci_local(norm_text, "hello") || ai_text_has_sub_ci_local(norm_text, "hi ")) return RX_INTENT_GREETING;
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
  for (i = 0; i < AI_RX_ROOM_CD_MAX; i++) if (ai_rx_room_cd[i].room == ctx->room_rnum && ai_rx_room_cd[i].event_type == ctx->event_type && ai_rx_room_cd[i].next_allowed > now) { *why = "room_cooldown"; return FALSE; }
  for (i = 0; i < AI_RX_ROOM_PLAYER_HASH_CD_MAX; i++) if (ai_rx_room_player_hash_cd[i].room == ctx->room_rnum && ai_rx_room_player_hash_cd[i].player_idnum == ctx->actor_idnum && ai_rx_room_player_hash_cd[i].event_type == ctx->event_type && ai_rx_room_player_hash_cd[i].text_hash == ctx->normalized_hash && ai_rx_room_player_hash_cd[i].next_allowed > now) { *why = "room_player_hash_cooldown"; return FALSE; }
  ev = ai_rx_room_event_get(ctx->room_rnum, ctx->event_type, 1);
  if (ev && ev->updated_at && (now - ev->updated_at) > 5) ev->count = 0;
  *why = "ok";
  return TRUE;
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
  seed = 1469598103UL;
  seed = ai_rx_hash_mix(seed, (unsigned long)ctx->room_rnum);
  seed = ai_rx_hash_mix(seed, (unsigned long)ctx->mob_vnum);
  seed = ai_rx_hash_mix(seed, (unsigned long)ctx->actor_idnum);
  seed = ai_rx_hash_mix(seed, (unsigned long)ctx->normalized_hash);
  seed = ai_rx_hash_mix(seed, (unsigned long)cat);

  switch (cat) {
    case AI_RX_BOUNDARY_RESPONSE: out->selected_text = ai_rx_pick_seeded(rx_boundary, seed); out->debug_reason = "boundary"; break;
    case AI_RX_ESCALATION: out->selected_text = ai_rx_pick_seeded(rx_escalation, seed); out->debug_reason = "escalation"; break;
    case AI_RX_DEFENSIVE_POSTURE: out->selected_text = ai_rx_pick_seeded(rx_tension_guard, seed); out->debug_reason = "guard_posture"; break;
    case AI_RX_SOCIAL_TENSION: out->selected_text = ai_rx_pick_seeded(rx_tension_bandit, seed); out->debug_reason = "bandit_tension"; break;
    case AI_RX_EVALUATION_BEAT: out->selected_text = ai_rx_pick_seeded((ctx->mob_role == ROLE_MERCHANT) ? rx_eval_merchant : rx_eval_guard, seed); out->debug_reason = "evaluation"; break;
    case AI_RX_WARNING: out->selected_text = ai_rx_pick_seeded(rx_intrusion, seed); out->debug_reason = "warning"; break;
    case AI_RX_CONFUSION: out->selected_text = ai_rx_pick_seeded(rx_confusion, seed); out->debug_reason = "confusion"; break;
    case AI_RX_SILENT_STARE: out->selected_text = ai_rx_pick_seeded(rx_silent_stare, seed); out->debug_reason = "degraded_cap"; break;
    case AI_RX_MORAL_JUDGMENT: out->selected_text = ai_rx_pick_seeded(IS_GOOD(mob) ? rx_moral_good : rx_moral_evil, seed); out->debug_reason = "moral"; break;
    default: out->selected_text = ai_rx_pick_seeded(rx_processing_generic, seed); out->debug_reason = "processing"; break;
  }
  if (!out->selected_text) { out->will_fire = FALSE; out->debug_reason = "empty_pool"; }
}

static void ai_reaction_fire(struct char_data *mob, const struct ai_reaction_result *r) {
  char buf[256];
  if (!mob || !r || !r->selected_text || !*r->selected_text) return;
  if (r->output_kind == AI_RX_KIND_EMOTE || !strncmp(r->selected_text, "$n ", 3)) {
    snprintf(buf, sizeof(buf), "%.*s", (int)sizeof(buf)-1, (!strncmp(r->selected_text, "$n ", 3) ? r->selected_text + 3 : r->selected_text));
    do_echo(mob, buf, 0, SCMD_EMOTE);
  } else {
    snprintf(buf, sizeof(buf), "%.*s", (int)sizeof(buf)-1, r->selected_text);
    do_say(mob, buf, 0, 0);
  }
}

int ai_reaction_try(struct char_data *mob, const struct ai_reaction_ctx *ctx) {
  const char *why = "";
  struct ai_reaction_result r;
  int i, mob_id = GET_IDNUM(mob), oldest = 0;
  time_t now = time(0);
  struct ai_rx_room_event *ev;

  if (!ai_rx_can_fire(mob, ctx, &why)) {
    log("AI_RX_SUPPRESS reason=%s", why ? why : "unknown");
    return 0;
  }

  ai_reaction_pick(mob, ctx, &r);
  if (!r.will_fire || !r.selected_text) {
    log("AI_RX_SUPPRESS reason=no_pick");
    return 0;
  }

  log("AI_RX_PICK category=%d kind=%d text=\"%s\"", r.category, r.output_kind, r.selected_text);
  ai_reaction_fire(mob, &r);
  log("AI_RX_FIRE vnum=%d role=%d trigger=%d", GET_MOB_VNUM(mob), ctx ? ctx->mob_role : -1, ctx ? ctx->trigger_reason : -1);

  if (mob_id <= 0) mob_id = GET_MOB_VNUM(mob);
  for (i = 0; i < AI_RX_MOB_CD_MAX; i++) { if (ai_rx_mob_cd[i].mob_id == mob_id || ai_rx_mob_cd[i].next_allowed == 0) { ai_rx_mob_cd[i].mob_id = mob_id; ai_rx_mob_cd[i].next_allowed = now + rand_number(8, 20); break; } }
  for (i = 0; i < AI_RX_ROOM_CD_MAX; i++) { if ((ai_rx_room_cd[i].room == ctx->room_rnum && ai_rx_room_cd[i].event_type == ctx->event_type) || ai_rx_room_cd[i].next_allowed == 0) { ai_rx_room_cd[i].room = ctx->room_rnum; ai_rx_room_cd[i].event_type = ctx->event_type; ai_rx_room_cd[i].next_allowed = now + rand_number(2, 4); break; } }
  for (i = 0; i < AI_RX_ROOM_PLAYER_CD_MAX; i++) { if ((ai_rx_room_player_cd[i].room == ctx->room_rnum && ai_rx_room_player_cd[i].player_idnum == ctx->actor_idnum && ai_rx_room_player_cd[i].event_type == ctx->event_type) || ai_rx_room_player_cd[i].next_allowed == 0) { ai_rx_room_player_cd[i].room = ctx->room_rnum; ai_rx_room_player_cd[i].player_idnum = ctx->actor_idnum; ai_rx_room_player_cd[i].event_type = ctx->event_type; ai_rx_room_player_cd[i].next_allowed = now + 6; break; } }
  for (i = 0; i < AI_RX_ROOM_PLAYER_HASH_CD_MAX; i++) { if ((ai_rx_room_player_hash_cd[i].room == ctx->room_rnum && ai_rx_room_player_hash_cd[i].player_idnum == ctx->actor_idnum && ai_rx_room_player_hash_cd[i].event_type == ctx->event_type && ai_rx_room_player_hash_cd[i].text_hash == ctx->normalized_hash) || ai_rx_room_player_hash_cd[i].next_allowed == 0) { ai_rx_room_player_hash_cd[i].room = ctx->room_rnum; ai_rx_room_player_hash_cd[i].player_idnum = ctx->actor_idnum; ai_rx_room_player_hash_cd[i].event_type = ctx->event_type; ai_rx_room_player_hash_cd[i].text_hash = ctx->normalized_hash; ai_rx_room_player_hash_cd[i].next_allowed = now + 20; break; } }

  ev = ai_rx_room_event_get(ctx->room_rnum, ctx->event_type, 1);
  if (ev) { ev->room = ctx->room_rnum; ev->event_type = ctx->event_type; ev->count++; ev->updated_at = now; }

  (void)oldest;
  return 1;
}
