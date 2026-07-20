#include "conf.h"
#include "sysdep.h"

#include "structs.h"
#include "utils.h"
#include "db.h"
#include "comm.h"
#include "handler.h"
#include "interpreter.h"
#include "shop.h"
#include "spec_procs.h"
#include "quest.h"
#include "mail.h"
#include "npc_social_ai.h"

ACMD(do_say);
#include "ai_actor.h"

#define CLAMP(v, lo, hi) ((v) < (lo) ? (lo) : ((v) > (hi) ? (hi) : (v)))
#define ARRAYSZ(a) (int)(sizeof(a) / sizeof((a)[0]))
#define NPC_SOCIAL_RECENT_MAX 5
#define NPC_ESCALATE_WINDOW 90
#define NPC_CATEGORY_REPEAT_WINDOW 20

enum npc_line_kind {
  LK_FIRST_GREET = 0,
  LK_REPEAT_GREET,
  LK_WARM_RECOG,
  LK_CAUTIOUS_RECOG,
  LK_POLITE_SERVICE,
  LK_TASK_PROMPT,
  LK_IDLE_WORK,
  LK_IDLE_OBSERVATION,
  LK_MILD_SUSPICION,
  LK_FIRM_SUSPICION,
  LK_MILD_ANNOY,
  LK_FIRM_ANNOY,
  LK_WARNING,
  LK_COMBAT_ALARM,
  LK_FEAR,
  LK_DANGER_ACTIVE,
  LK_RECOVERY,
  LK_APPROVAL,
  LK_DISMISSAL,
  LK_FAREWELL
};

struct npc_pool {
  const char *const *lines;
  int count;
};

static int has_kw(const char *text, const char *kw)
{
  if (!text || !kw || !*kw) return FALSE;
  return (strstr(text, kw) != NULL);
}

static unsigned long line_hash(const char *s)
{
  unsigned long h = 2166136261u;
  if (!s) return 0;
  while (*s) {
    h ^= (unsigned char)*s;
    h *= 16777619u;
    s++;
  }
  return (h == 0) ? 1 : h;
}

static int is_hash_recent(const int *arr, int count, int hash)
{
  int i;
  for (i = 0; i < count; i++)
    if (arr[i] == hash)
      return TRUE;
  return FALSE;
}

static void push_recent_hash(int *arr, int count, int hash)
{
  int i;
  for (i = count - 1; i > 0; i--) arr[i] = arr[i - 1];
  arr[0] = hash;
}

static const char *pick_from_pool(struct char_data *ch, const struct npc_pool *pool, int is_emote)
{
  int tries, idx, hash, i;
  int *recent_arr;
  int last_hash;
  int found_non_recent = -1;

  if (!ch || !ch->ai_state || !pool || pool->count <= 0 || !pool->lines) return NULL;

  recent_arr = is_emote ? ch->ai_state->recent_emote_hashes : ch->ai_state->recent_speech_hashes;
  last_hash = is_emote ? ch->ai_state->last_emote_hash : ch->ai_state->last_speech_hash;

  for (tries = 0; tries < pool->count * 2; tries++) {
    idx = rand_number(0, pool->count - 1);
    hash = (int)line_hash(pool->lines[idx]);
    if (hash == last_hash) continue;
    if (is_hash_recent(recent_arr, NPC_SOCIAL_RECENT_MAX, hash)) {
      if (found_non_recent < 0) found_non_recent = idx;
      continue;
    }
    if (is_emote) {
      ch->ai_state->last_emote_hash = hash;
      push_recent_hash(ch->ai_state->recent_emote_hashes, NPC_SOCIAL_RECENT_MAX, hash);
    } else {
      ch->ai_state->last_speech_hash = hash;
      push_recent_hash(ch->ai_state->recent_speech_hashes, NPC_SOCIAL_RECENT_MAX, hash);
    }
    return pool->lines[idx];
  }

  for (i = 0; i < pool->count; i++) {
    hash = (int)line_hash(pool->lines[i]);
    if (hash != last_hash && !is_hash_recent(recent_arr, NPC_SOCIAL_RECENT_MAX, hash)) {
      if (is_emote) {
        ch->ai_state->last_emote_hash = hash;
        push_recent_hash(ch->ai_state->recent_emote_hashes, NPC_SOCIAL_RECENT_MAX, hash);
      } else {
        ch->ai_state->last_speech_hash = hash;
        push_recent_hash(ch->ai_state->recent_speech_hashes, NPC_SOCIAL_RECENT_MAX, hash);
      }
      return pool->lines[i];
    }
  }

  if (!is_emote) return NULL;
  if (found_non_recent < 0) return NULL;
  hash = (int)line_hash(pool->lines[found_non_recent]);
  ch->ai_state->last_emote_hash = hash;
  push_recent_hash(ch->ai_state->recent_emote_hashes, NPC_SOCIAL_RECENT_MAX, hash);
  return pool->lines[found_non_recent];
}

static struct ai_actor_memory_entry *npc_mem_get(struct char_data *ch, long idnum)
{
  int i, limit, evict = -1;
  if (!ch || !ch->ai_state || idnum <= 0) return NULL;
  for (i = 0; i < ch->ai_state->mem_count; i++)
    if (ch->ai_state->mem[i].idnum == idnum)
      return &ch->ai_state->mem[i];
  limit = (ch->ai_prof && ch->ai_prof->memory_enabled) ? ch->ai_prof->memory_max_actors : AI_MEM_MAX;
  if (ch->ai_state->mem_count >= limit) { /* Evict the least relevant relationship, never randomly. */
    int score = 1000000;
    for (i=0;i<ch->ai_state->mem_count;i++) { int s=abs(ch->ai_state->mem[i].trust)+ch->ai_state->mem[i].fear+ch->ai_state->mem[i].hostility; if(s<score){score=s;evict=i;} }
    if (evict < 0) return NULL; i=evict;
  } else i = ch->ai_state->mem_count++;
  memset(&ch->ai_state->mem[i], 0, sizeof(ch->ai_state->mem[i]));
  ch->ai_state->mem[i].idnum = idnum;
  return &ch->ai_state->mem[i];
}

static void npc_copy_lower(const char *src, char *dst, size_t sz)
{
  size_t i;
  if (!dst || sz == 0) return;
  if (!src) {
    dst[0] = '\0';
    return;
  }
  for (i = 0; i + 1 < sz && src[i]; i++)
    dst[i] = LOWER(src[i]);
  dst[i] = '\0';
}

int npc_detect_say_intent(const char *msg)
{
  char lower[256];
  npc_copy_lower(msg, lower, sizeof(lower));
  if (!*lower) return SAY_INTENT_UNCLEAR;

  if (has_kw(lower, "kill you") || has_kw(lower, "fight me") || has_kw(lower, "attack"))
    return SAY_INTENT_THREAT;
  if (has_kw(lower, "touch me") || has_kw(lower, "lick me") || has_kw(lower, "kiss me"))
    return SAY_INTENT_INAPPROPRIATE;
  if (has_kw(lower, "idiot") || has_kw(lower, "stupid") || has_kw(lower, "shut up") || has_kw(lower, "eat one"))
    return SAY_INTENT_RUDE;
  if (has_kw(lower, "good job") || has_kw(lower, "nice") || has_kw(lower, "well done"))
    return SAY_INTENT_PRAISE;
  if (has_kw(lower, "train") || has_kw(lower, "buy") || has_kw(lower, "sell") || has_kw(lower, "heal") ||
      has_kw(lower, "job") || has_kw(lower, "quest"))
    return SAY_INTENT_SERVICE;
  if (has_kw(lower, "weather") || has_kw(lower, "day") || has_kw(lower, "busy") || has_kw(lower, "quiet"))
    return SAY_INTENT_SMALLTALK;
  if (has_kw(lower, "hi") || has_kw(lower, "hello") || has_kw(lower, "hey") || has_kw(lower, "greetings"))
    return SAY_INTENT_GREETING;
  if (has_kw(lower, "what") || has_kw(lower, "why") || has_kw(lower, "how") || has_kw(lower, "huh") || has_kw(lower, "help"))
    return SAY_INTENT_QUESTION;

  return SAY_INTENT_UNCLEAR;
}

int npc_ai_is_humanoid_social_candidate(struct char_data *ch)
{
  return (ch && IS_NPC(ch) && IN_ROOM(ch) != NOWHERE && MOB_FLAGGED(ch, MOB_AI_ACTOR));
}

void npc_ai_build_profile(struct char_data *ch, struct npc_social_profile *out)
{
  const char *name = (ch && ch->player.name) ? ch->player.name : "";
  const char *sdesc = (ch && ch->player.short_descr) ? ch->player.short_descr : "";
  int has_shop_spec = FALSE;
  int has_quest_spec = FALSE;
  int has_mayor_spec = FALSE;
  int has_guard_spec = FALSE;
  int has_guild_spec = FALSE;
  int has_service_spec = FALSE;

  if (!out) return;
  out->role = NPC_ROLE_GENERIC_TOWNSFOLK;
  out->temperament = NPC_TEMP_STEADY;
  out->social_style = NPC_SOCIAL_NEUTRAL;

  if (ch && GET_MOB_RNUM(ch) != NOBODY) {
    int (*spec)(struct char_data *, void *, int, char *) = mob_index[GET_MOB_RNUM(ch)].func;
    has_shop_spec = (spec == shop_keeper || spec == receptionist || spec == postmaster || spec == bank);
    has_quest_spec = (spec == questmaster);
    has_mayor_spec = (spec == mayor);
    has_guard_spec = (spec == guild_guard);
    has_guild_spec = (spec == guild || MOB_FLAGGED(ch, MOB_GUILD_MASTER));
    has_service_spec = has_shop_spec || has_quest_spec || has_mayor_spec || has_guild_spec;
  }

  if (has_shop_spec || has_kw(name, "merchant") || has_kw(name, "shopkeeper") || has_kw(name, "shop") ||
      has_kw(sdesc, "merchant") || has_kw(sdesc, "shopkeeper") || has_kw(sdesc, "shop")) {
    out->role = NPC_ROLE_MERCHANT;
  } else if (has_guild_spec || has_kw(name, "trainer") || has_kw(name, "instructor") || has_kw(name, "guildmaster") ||
             has_kw(sdesc, "trainer") || has_kw(sdesc, "instructor") || has_kw(sdesc, "guildmaster")) {
    out->role = NPC_ROLE_TRAINER;
  } else if (has_kw(name, "healer") || has_kw(name, "medic") || has_kw(name, "cleric") || has_kw(name, "priest") ||
             has_kw(sdesc, "healer") || has_kw(sdesc, "medic") || has_kw(sdesc, "cleric")) {
    out->role = NPC_ROLE_HEALER;
  } else if (has_quest_spec || has_kw(name, "quest") || has_kw(name, "hermit") || has_kw(sdesc, "quest") || has_kw(sdesc, "hermit")) {
    out->role = NPC_ROLE_QUESTGIVER;
  } else if (has_kw(name, "innkeeper") || has_kw(name, "inn") || has_kw(sdesc, "innkeeper") || has_kw(sdesc, "inn")) {
    out->role = NPC_ROLE_INNKEEPER;
  } else if (has_mayor_spec || has_kw(name, "mayor") || has_kw(name, "official") || has_kw(name, "advisor") ||
             has_kw(name, "council") || has_kw(sdesc, "mayor") || has_kw(sdesc, "official") || has_kw(sdesc, "advisor")) {
    out->role = NPC_ROLE_OFFICIAL;
  } else if (has_guard_spec || has_kw(name, "guard") || has_kw(name, "watch") || has_kw(name, "sentinel") ||
             has_kw(sdesc, "guard") || has_kw(sdesc, "watch") || has_kw(sdesc, "sentinel")) {
    out->role = NPC_ROLE_GUARD;
  } else if (has_kw(name, "bandit") || has_kw(name, "brigand") || has_kw(name, "outlaw") || has_kw(sdesc, "bandit")) {
    out->role = NPC_ROLE_BANDIT;
  } else if (has_service_spec || has_kw(name, "service") || has_kw(sdesc, "service")) {
    out->role = NPC_ROLE_GENERIC_SERVICE;
  } else if (has_kw(name, "civilian") || has_kw(name, "townsfolk") || has_kw(sdesc, "civilian") || has_kw(sdesc, "townsfolk")) {
    out->role = NPC_ROLE_CIVILIAN;
  } else {
    out->role = NPC_ROLE_GENERIC_TOWNSFOLK;
  }

  if (MOB_FLAGGED(ch, MOB_WIMPY)) out->temperament = NPC_TEMP_TIMID;
  if (MOB_FLAGGED(ch, MOB_AGGRESSIVE)) out->temperament = NPC_TEMP_AGGRESSIVE;

  if (has_kw(name, "loud") || has_kw(name, "chatty") || has_kw(sdesc, "boisterous")) out->social_style = NPC_SOCIAL_EXTROVERT;
  if (has_kw(name, "quiet") || has_kw(name, "silent") || has_kw(sdesc, "quiet")) out->social_style = NPC_SOCIAL_INTROVERT;
}

static void npc_say(struct char_data *ch, const char *line)
{
  if (!ch || !line || !*line) return;
  do_say(ch, (char *)line, 0, 0);
}

/* -- Dialogue content pools -- */
static const char *const guard_first[] = {
  "Keep your step steady and your hands honest.",
  "The peace holds today. See that it stays that way.",
  "Mind the law and we will have no trouble.",
  "Walk clean and you will be left in peace.",
  "The streets are watched. Remember that.",
  "Another traveler. Keep your purpose plain.",
  "Trouble has a way of finding fools. Do not be one.",
  "You are free to pass, so long as you pass peacefully.",
  "The watch is on duty. Conduct yourself well.",
  "Keep your temper sheathed with your blade."
};
static const char *const guard_repeat[] = {
  "There you are again. Still keeping to the straight road?",
  "You return often. I will take that as a good sign.",
  "Back on the street again, are you?",
  "I remember your face. Keep giving me no reason to object.",
  "You know the order here by now.",
  "Seen anything worth reporting?",
  "Still among the living, I see.",
  "You carry yourself better than some. Keep it that way."
};
static const char *const guard_warm[] = {
  "You have caused less trouble than most. That counts for something.",
  "I know you. Makes watch duty easier.",
  "Good to see a familiar face without a complaint attached.",
  "You have earned a little trust. Do not spend it poorly.",
  "You have been steady enough so far.",
  "I have seen worse folk pass these roads than you.",
  "Well met. The streets are calmer with known faces.",
  "You again. That sits well enough with me."
};
static const char *const guard_cautious[] = {
  "I know you, but I am still watching.",
  "You are familiar, not above suspicion.",
  "Back again. Keep your business clean.",
  "I remember you. Do not test how much.",
  "You have been here before. That does not excuse anything.",
  "You are known, but not unchecked.",
  "I have seen you often enough to notice patterns.",
  "Familiarity is not permission. Remember that."
};
static const char *const guard_idle_obs[] = {
  "Quiet streets are a gift that rarely lasts.",
  "A calm road is still a road worth watching.",
  "Most trouble starts with someone thinking no one is looking.",
  "Order lives in small choices.",
  "The city keeps its peace only while people keep theirs."
};
static const char *const guard_mild_susp[] = {
  "You seem to be circling with no purpose.", "What keeps you lingering here?",
  "You have the look of someone measuring a boundary.", "State your business and be done with it.",
  "That glance of yours is wandering too much.", "You are watching more than you are walking.",
  "Something on your mind, or someone else's purse?", "You are drawing the sort of notice you do not want."
};
static const char *const guard_firm_susp[] = {
  "You have lingered long enough.", "I have asked once already. Explain yourself.",
  "You are nearing the end of my patience.", "Step back and speak clearly.",
  "You are acting like someone who wants trouble.", "I do not like the look of this.",
  "You will answer, or you will move.", "You are one bad choice from a hard lesson."
};
static const char *const guard_mild_annoy[] = {
  "I am not here for idle pestering.", "Save my time and yours.", "You have asked enough for the moment.",
  "Do not wear out your welcome.", "Take the hint and move along.", "You are beginning to grate.",
  "I have other matters than repeating myself.", "You test patience more than wisdom."
};
static const char *const guard_firm_annoy[] = {
  "Enough.", "I said move along.", "My patience is spent.", "You have pushed this far enough.",
  "One more nuisance and this changes tone.", "I will not warn you forever.",
  "You are making this harder than it should be.", "Stand down and be quiet."
};
static const char *const guard_warning[] = {
  "Back away. Now.", "Keep your distance.", "Do not make me put hand to steel.",
  "Choose calm while you still can.", "That is your last gentle warning.",
  "Leave off before this turns ugly.", "Take your temper elsewhere.", "One more step and I answer in force."
};
static const char *const guard_combat[] = {
  "Steel up!", "Watch to me!", "Hold them!", "Drive them back!",
  "On me, now!", "Form up!", "No bloodshed in these streets!", "By the watch, stop them!"
};
static const char *const guard_fear[] = {
  "Gods, not here.", "Keep the civilians back!", "This is turning bad fast.", "Make room and keep clear!",
  "Too many blades out at once.", "Seal this area off!", "Protect the innocents!", "This street is no place for panic."
};
static const char *const guard_recovery[] = {
  "Back to order, then.", "Clean this mess before it festers.", "That could have gone worse.",
  "Stay alert. Trouble rarely dies alone.", "Peace again, for now.", "The street is ours again.",
  "That ended bloodier than I would like.", "Take a breath and return to your business."
};
static const char *const guard_approval[] = {
  "You handled yourself with sense.", "That was the right choice.", "Good. More folk should think that clearly.",
  "You did well enough there.", "A measured hand is worth respecting.", "You chose restraint. Remember the value of it.",
  "Well done.", "I will mark that in your favor."
};
static const char *const guard_dismiss[] = {
  "Move along.", "That is all from me.", "Off with you, then.", "You have your answer.",
  "Enough talk. Keep moving.", "Do what you came to do and go.", "We are finished here.", "Take care and cause none."
};
static const char *const guard_farewell[] = {
  "Keep the peace.", "Walk safely.", "Stay clear of foolishness.", "May your road stay quiet.",
  "Keep your blade sheathed and your head cool.", "Go on, then.", "Watch your back out there.",
  "Do not make me regret letting you pass."
};
static const char *const guard_emotes[] = {
  "folds their arms across a polished breastplate", "rests their thumb near the guard of their weapon",
  "scans the road with a patient, practiced stare", "angles their body to keep the street in view",
  "straightens a strap and resumes their watch", "narrows their eyes at a distant movement",
  "plants their boots and squares their shoulders", "glances from face to face, measuring the room",
  "lets out a slow breath through the nose", "checks the edge of their scabbard by habit",
  "tilts their head as if listening for trouble", "shifts into a firmer stance without a word"
};

/* Merchant */
static const char *const merchant_first[] = {
  "Welcome in. Coin spends well here.", "If you have sense and silver, we will get along.",
  "A fair look costs nothing. Breaking something does.", "Goods for sale and no nonsense with them.",
  "Take your time, but not my whole day.", "You look like someone in need of proper supplies.",
  "I have stock enough for wise buyers.", "Look with your eyes before your hands.",
  "Trade is open. Speak plain and pay fair.", "Come in, then. Let us see what brings you."
};
static const char *const merchant_repeat[] = {
  "Back again? Good taste usually returns.", "I thought my stock might draw you twice.",
  "You have a habit of finding your way back to my counter.", "See something you missed the first time?",
  "I kept the shelves standing just for you.", "Returned already? Then something caught your eye.",
  "You again. That is rarely bad for business.", "Welcome back. I have kept busy since last time."
};
static const char *const merchant_warm[] = {
  "You are one of the easier customers to deal with.", "I know your face, and that helps.",
  "Always good to see a familiar buyer.", "You have a respectable eye for decent goods.",
  "You return often enough to be remembered kindly.", "I would rather trade with known folk than fools.",
  "You have been fair with me. I value that.", "Ah, a customer I can speak to without bracing myself."
};
static const char *const merchant_cautious[] = {
  "I know you, but I still watch my shelves.", "You have returned. Good. Keep your hands honest.",
  "I remember you. Let us keep this civil.", "You are familiar enough. That is not the same as trusted.",
  "I know your face. I still count my stock.", "Back again. Try not to test my patience today.",
  "You are known here. Stay respectable.", "We have traded before. Let us not ruin that."
};
static const char *const merchant_service[] = {
  "Have a proper look. There is quality here.", "Say what you need and I will point you to it.",
  "If you are buying, I am listening.", "Let us keep this simple. Need, price, purchase.",
  "Tell me what you lack and I may solve it.", "Coin first, confusion second.",
  "I can help if you know what you want.", "A fair trade begins with clear words."
};
static const char *const merchant_work[] = {
  "Stock does not sort itself.", "There is always one more ledger line to chase.",
  "The honest shopkeeper works while others drift.", "A shelf left dusty invites poor business.",
  "Count once, count twice, and still someone argues price.", "Good trade comes to those who stay prepared.",
  "Busy hands keep profit from thinning out.", "A tidy counter sells better than a cluttered one."
};
static const char *const merchant_obs[] = {
  "Market traffic tells you what sort of day it will be.", "You can hear prosperity in the right kind of footsteps.",
  "Quiet streets are either a blessing or a warning.", "Coin moves like weather. You learn to feel it.",
  "The city breathes differently on a good trade day.", "There is always someone shopping for trouble instead of goods.",
  "A patient seller outlasts a frantic one.", "People reveal themselves by what they reach for."
};
static const char *const merchant_mild_susp[] = {
  "Looking is one thing. Lurking is another.", "You have been circling that shelf a while.",
  "Can I help you, or are you just testing my nerves?", "Eyes sharp, hands sharper, is that it?",
  "You are studying the shop more than the wares.", "If you need something, speak. If not, drift elsewhere.",
  "I dislike uncertainty near expensive stock.", "You have the air of someone considering a bad idea."
};
static const char *const merchant_firm_susp[] = {
  "Step away from the goods.", "I have watched you long enough.", "Either buy or leave.",
  "I will not ask again. Back from the shelf.", "You are one twitch from being thrown out.",
  "I know the look of a thief before the hand moves.", "Keep where I can see you.", "This is a shop, not a place for schemes."
};
static const char *const merchant_mild_annoy[] = {
  "If you are not buying, do not turn this into a pastime.", "You are wearing grooves into my patience.",
  "I answer questions. I do not entertain nonsense.", "My counter is not improved by endless hovering.",
  "Make your decision before daylight dies.", "There are customers, and there are burdens. Choose which you are.",
  "You have had enough of my time for free.", "Do not paw at my mood the way you paw at my stock."
};
static const char *const merchant_firm_annoy[] = {
  "Enough dithering.", "My patience is not part of the inventory.", "Buy something or leave my doorway clear.",
  "I am done repeating myself.", "You have crossed from nuisance into problem.",
  "I will not let you spoil the shop for paying customers.", "Leave off, now.", "I have had my fill of you today."
};
static const char *const merchant_fear[] = {
  "Not among the wares!", "Take that steel out of my shop!", "I will not lose stock to your madness!",
  "Outside! Fight outside!", "Gods spare my shelves!", "Do you know what broken stock costs?",
  "Keep blood off my floor!", "I want no killing under my roof!"
};
static const char *const merchant_danger[] = {
  "Someone stop them before the place is wrecked!", "Mind the shelves!", "Get clear of the counter!",
  "I am not paid enough for this kind of day!", "Keep them away from the stock!", "This is ruinous!",
  "By all coins, not here!", "My livelihood is standing in the path of fools!"
};
static const char *const merchant_recovery[] = {
  "Look at this mess.", "There goes half a day to panic and repairs.", "Back to trade, if trade still lives.",
  "I swear the city breeds chaos on purpose.", "Let me count what survived.", "Peace, finally. My poor shelves.",
  "That was bad for business in every possible way.", "I would like one quiet hour before I die."
};
static const char *const merchant_approval[] = {
  "That was handled better than I expected.", "A careful hand is always welcome here.",
  "You have some sense after all.", "Good. I prefer customers who think.", "That was decent of you.",
  "I notice restraint when I see it.", "You did right there.", "There is value in a level head."
};
static const char *const merchant_dismiss[] = {
  "If you are done, so am I.", "Take care of yourself and your purse.", "That is all I have for you.",
  "Out with you, then.", "Return with coin, preferably.", "We are finished for now.",
  "Mind the door on your way out.", "Do not forget what you owe, whether silver or manners."
};
static const char *const merchant_farewell[] = {
  "Spend wisely.", "Come back with coin.", "May your purse stay heavier than your troubles.",
  "Good trade to you.", "Walk carefully and spend carefully.", "May your next bargain be a sound one.",
  "Keep your pack dry and your money hidden.", "Until next time."
};
static const char *const merchant_emotes[] = {
  "straightens a row of wares with exacting fingers", "checks a ledger and mutters over a figure",
  "runs a cloth along the edge of the counter", "recounts a stack of coins under a watchful eye",
  "shifts a crate with a practiced grunt", "adjusts a hanging display to sit just right",
  "taps the cover of a ledger with one finger", "glances at the door each time it opens",
  "dusts a shelf and inspects the result", "squares two boxes so their edges align",
  "slides a wrapped parcel farther from wandering hands", "looks over the stock as if measuring profit by sight"
};

/* trainer, healer, questgiver, civilian, bandit, official, generic townsfolk/service pools omitted from comment: present below */
/* To keep file manageable these pools are fully included for runtime variety. */
static const char *const trainer_first[] = {
  "If you came for comfort, keep walking.", "Discipline begins before the first lesson.",
  "You look capable of learning, if not yet of doing.", "Stand straight and speak your aim.",
  "Training is earned one correction at a time.", "Effort matters more than excuses here.",
  "If you want strength, be ready to sweat for it.", "I can teach. I cannot make you endure.",
  "Every skill begins with being willing to be corrected.", "Tell me what you seek to improve."
};
static const char *const trainer_repeat[] = {
  "Back again. Good. Consistency matters.", "You return. That already sets you above many.",
  "Ready to work, or ready to complain?", "I was wondering whether discipline would bring you back.",
  "There is always another flaw to refine.", "You again. Good. Progress favors repetition.",
  "You have not quit yet. Promising.", "Back for another lesson in humility, are you?"
};
static const char *const trainer_warm[] = {
  "You have shown more grit than most.", "I have seen improvement in you.", "You are becoming easier to teach.",
  "Repeated effort leaves a mark. You are proving that.", "You come back, and that matters.",
  "I remember students who listen.", "You have earned a little professional respect.",
  "You learn best when you stop fighting the lesson."
};
static const char *const trainer_cautious[] = {
  "I know you, but that does not excuse sloppy work.", "You return. Good. Do not waste it.",
  "I have seen your habits. Some still need breaking.", "You are familiar, not finished.",
  "You know the routine. Follow it.", "I remember your strengths and your laziness.",
  "Back again. That means I can push you further.", "I know you well enough to expect better."
};
static const char *const trainer_prompt[] = {
  "Tell me what you want sharpened.", "If it is skill you want, start by listening.",
  "Choose your weakness and we will begin there.", "State your discipline and stand ready.",
  "You want improvement? Then commit to it.", "Speak plainly. What are you training for?",
  "There is always something to refine.", "Show me your aim and I will show you the work."
};
static const char *const trainer_work[] = {
  "Balance before force. Always.", "A weak stance ruins a strong strike.", "Technique outlives enthusiasm.",
  "The body learns what the mind repeats.", "Bad habits harden faster than good ones.",
  "Practice makes patterns, not miracles.", "Strength without control is waste.",
  "A lesson ignored today becomes a weakness tomorrow."
};
static const char *const trainer_obs[] = {
  "You can tell a fighter by how they carry stillness.", "Most people fail before they ever start paying attention.",
  "A room reveals posture before it reveals character.", "Restlessness is often fear wearing another face.",
  "The impatient strike hardest at empty air.", "Skill is often quieter than pride.",
  "True readiness rarely needs boasting.", "People show their discipline when they think no one is measuring it."
};
static const char *const trainer_mild_susp[] = {
  "You are watching like someone looking for a shortcut.", "Do you mean to learn, or only to linger?",
  "Stand with purpose or move aside.", "Indecision wastes more than time.",
  "You have the look of someone avoiding effort.", "Say what you want.",
  "If you are here for nonsense, go elsewhere.", "Do not hover in my line of work."
};
static const char *const trainer_firm_susp[] = {
  "You are one step from wasting my time entirely.", "Either commit or leave.",
  "I will not tolerate posturing without purpose.", "Back off and decide yourself.",
  "I do not train gawkers.", "You are here under false intention. Correct that.",
  "Enough circling. Speak or go.", "You will not loiter here indefinitely."
};
static const char *const trainer_mild_annoy[] = {
  "Focus.", "I already answered you.", "Do not make me repeat the obvious.", "You are drifting.",
  "Attention is part of the lesson.", "Every distraction is self-inflicted.",
  "You came for guidance. Listen when it is given.", "Stop pecking at the edges and do the work."
};
static const char *const trainer_firm_annoy[] = {
  "Enough fidgeting.", "I will not teach through nonsense.", "You are wasting my effort.",
  "Correct yourself now.", "That attitude has no place here.", "I am finished tolerating sloppiness of mind.",
  "If you want progress, stop resisting it.", "Stand properly or leave."
};
static const char *const trainer_fear[] = {
  "This is a training ground, not a slaughter pit.", "Control yourselves!",
  "Take this violence elsewhere unless you mean it.", "Clear the reckless from the floor.",
  "This is why discipline matters.", "Keep back if you cannot hold your nerve.",
  "Too much panic, not enough control.", "Steel and fear make poor teachers together."
};
static const char *const trainer_combat[] = {
  "Set your feet!", "Guard up!", "Do not give them the line!", "Hold your balance!",
  "Read the opening!", "Stay sharp!", "Back them off!", "No sloppy movement now!"
};
static const char *const trainer_recovery[] = {
  "Now breathe and learn from that.", "That was uglier than it needed to be.",
  "Chaos always exposes poor habits.", "Back to form. Back to discipline.", "If you survived, then learn.",
  "The lesson is not over just because the noise stopped.", "Order yourselves.", "Good. Now recover properly."
};
static const char *const trainer_approval[] = {
  "Better.", "That showed control.", "You corrected well.", "That was disciplined work.",
  "Good. Do it that way again.", "There. That is the shape of progress.", "You are learning.",
  "Well done, but do not become satisfied."
};
static const char *const trainer_dismiss[] = {
  "Return when you are ready to work.", "We are done for now.", "Take that lesson and practice it.",
  "Enough for today.", "Go put effort where your mouth has been.", "Off with you. Train what you were told.",
  "That is your instruction.", "Do not let the lesson go stale."
};
static const char *const trainer_farewell[] = {
  "Train hard.", "Keep your footing.", "Do not let idleness undo your gains.",
  "Return with effort, not excuses.", "Practice before pride.", "Stay disciplined.", "Hold your form.", "Come back sharper."
};
static const char *const trainer_emotes[] = {
  "rolls one shoulder and then the other", "studies your stance with a critical eye",
  "paces a short measured line", "demonstrates a precise strike through empty air",
  "folds their hands behind their back", "tilts their head in silent appraisal",
  "sets their feet and checks their own balance", "gestures sharply for better posture",
  "watches movement in the room like a lesson unfolding", "breathes out slowly through the nose",
  "gives a brief dissatisfied shake of the head", "taps two fingers against their arm in thought"
};

/* Healer */
static const char *const healer_first[] = {
  "Peace to you. Are you hurt, weary, or both?", "Come gently. Healing favors calm.",
  "If you bear pain, do not bear it proudly.", "Sit if you need to. I have seen worse.",
  "You look travel-worn. That can be mended.", "Bring me truth and I will do what I can.",
  "Rest is the first medicine many refuse.", "If you are wounded, let me see it.",
  "Pain speaks plainly if you allow it.", "You have the look of someone who has pushed too far."
};
static const char *const healer_repeat[] = {
  "You return. Then either the road is hard, or your habits are.", "Back again. Sit, and let us see what needs mending now.",
  "I remember you. Has the world been rough again?", "You again. Breathe first, speak second.",
  "Welcome back. Keep still a moment.", "You have a familiar weariness about you.",
  "I hoped your last visit would hold longer.", "Come in. Let us tend what has frayed."
};
static const char *const healer_warm[] = {
  "I know your face well enough to worry when I see it strained.", "You return, and I am glad you made it back whole enough.",
  "It is good to see you standing.", "You have been through much and still walk.", "You are remembered kindly here.",
  "I know your burdens a little now.", "You return often enough to make concern natural.", "Be at ease. You are among known hands."
};
static const char *const healer_cautious[] = {
  "I know you, though I still need to know what happened.", "You are familiar, but wounds change faster than faces.",
  "I remember you. Sit, and do not hide the truth of it.", "You return. Let us hope with less damage this time.",
  "I know your face, though not yet your good sense.", "Back again. Be honest with me.",
  "I remember you well enough to worry.", "You are known, but not beyond needing care."
};
static const char *const healer_service[] = {
  "Show me where it hurts.", "Sit still and let me work.", "If there is injury, speak of it plainly.",
  "Calm yourself and I can help more quickly.", "Do not make me guess at hidden wounds.",
  "Healing begins when stubbornness ends.", "Tell me what happened and where the pain lies.", "Let me see what can be mended."
};
static const char *const healer_work[] = {
  "Bandages folded now save panic later.", "Clean hands prevent ugly endings.", "Herbs keep better when they are respected.",
  "A little preparation spares a great deal of suffering.", "Most healing is patience wearing practical clothes.",
  "There is mercy in small, careful habits.", "The body often wants to heal if we stop hindering it.", "Quiet work keeps worse work from arriving."
};
static const char *const healer_obs[] = {
  "You can see strain in the way someone lowers themselves to sit.", "Pain teaches quickly, though not kindly.",
  "The weary often speak more honestly than the proud.", "Fear enters the breath before it enters the voice.",
  "Most people bleed long before the wound reaches skin.", "Calm is easier to prescribe than to keep.",
  "A room carries tension the way flesh carries bruises.", "Relief is sometimes only being seen clearly."
};
static const char *const healer_mild_susp[] = {
  "You are restless in a place meant for calm.", "What is it you need, truly?", "You seem more agitated than injured.",
  "If you have business, speak it gently.", "This room is for care, not schemes.", "You are carrying something heavier than your words.",
  "Do not bring hidden trouble to a healing space.", "I can feel unease before I can name it."
};
static const char *const healer_firm_susp[] = {
  "Enough wandering. Speak your need or leave.", "You are disturbing the peace of this place.",
  "I will not allow tension to spread unchecked here.", "If you bring trouble, turn back with it.",
  "You are pressing on sacred patience now.", "This is not a room for games.", "Back away and settle yourself.", "I need calm or I need distance."
};
static const char *const healer_mild_annoy[] = {
  "Please stop making my work harder.", "I need stillness, not agitation.", "You are not the only one who may need care.",
  "If you keep interrupting, no one is helped.", "Peace, for a moment.", "I have heard you. Do not peck at me.",
  "Let me finish one thing before ten others.", "Your impatience heals nothing."
};
static const char *const healer_firm_annoy[] = {
  "Enough.", "This is a place of healing. Behave accordingly.", "I will not be hounded in my own work.",
  "Be still, or be gone.", "My patience is not endless.", "Do not turn suffering into spectacle.", "Silence now.", "You are fraying the room."
};
static const char *const healer_fear[] = {
  "No bloodshed here!", "Take your violence outside these walls!", "There are already enough wounds in the world!",
  "Stop this at once!", "This place is not for killing!", "Have you no shame, drawing blood here?",
  "Keep blades from the sick!", "Do not make me tend fresh ruin!"
};
static const char *const healer_danger[] = {
  "Keep back and give the injured room!", "Move, move, let the frightened breathe!",
  "This panic will wound more than steel!", "Stand clear if you cannot help!", "Gods preserve us from reckless hands!",
  "There will be enough dead without stupidity added!", "Hold yourselves together!", "Make space for the vulnerable!"
};
static const char *const healer_recovery[] = {
  "At last. Let us begin putting things right.", "Bring me the hurt and keep the shouting away.",
  "The worst seems passed.", "Now we mend what panic has torn open.", "Enough noise. Back to care.",
  "Peace, at last. Hold onto it.", "Let the room breathe again.", "We return now to what actually saves lives."
};
static const char *const healer_approval[] = {
  "That was mercifully done.", "You chose restraint, and that matters.", "Good. Fewer wounds for all of us.",
  "You helped more than you know.", "There is kindness in that choice.", "Well done. Compassion is not weakness.",
  "You acted with care.", "I am glad to have seen that."
};
static const char *const healer_dismiss[] = {
  "Go gently.", "Take care of your body before it forces the issue.", "Do not ignore what I have told you.",
  "Rest when you can.", "That is all I can do for now.", "Be on your way, but not recklessly.", "Go with care.", "Return if the hurt worsens."
};
static const char *const healer_farewell[] = {
  "May you keep whole.", "Walk in peace.", "Mind your limits.", "Go carefully.", "May your road be kinder than the last.",
  "Keep yourself from needless harm.", "Rest when the chance comes.", "Do not wait too long before returning, if you must."
};
static const char *const healer_emotes[] = {
  "arranges a stack of folded bandages", "sorts herbs by scent and leaf", "bows their head in a brief quiet prayer",
  "checks a small satchel of remedies", "lights a fresh bit of incense", "washes their hands with deliberate care",
  "smooths a wrinkle from the edge of a cloth", "looks over a row of bottles against the light",
  "mixes dried herbs with patient fingers", "listens to the room with a healer's practiced stillness",
  "sets a clean cloth within easy reach", "breathes slowly, as if steadying the room itself"
};

/* Questgiver */
static const char *const quest_first[] = {
  "There is work to be done, if you are equal to hearing it.", "I may have use for capable hands.",
  "You look like someone who could be pointed at a problem.", "If you seek purpose, I have no shortage of need.",
  "Listen first. Decide after.", "I have a matter that has waited too long already.", "There are tasks suited to steady folk.",
  "You have arrived at a useful moment, perhaps.", "If you came seeking idle chatter, you came poorly.",
  "There may yet be a purpose for your presence."
};
static const char *const quest_repeat[] = {
  "You return. Good.", "I was beginning to wonder whether you had vanished.", "Back again, then. Do you bring progress?",
  "I have been expecting news.", "You return with either answers or excuses.", "Ah. A familiar face and, I hope, useful news.",
  "Well? Have you made headway?", "There you are. Speak."
};
static const char *const quest_warm[] = {
  "I trust you more than when we first spoke.", "You have proven at least somewhat reliable.",
  "It is easier to speak plainly with someone who returns.", "You have given me reason to remember you favorably.",
  "Known hands make difficult work lighter.", "I have come to expect follow-through from you.",
  "You have earned a little confidence.", "It is good to see someone who does not vanish at the first hardship."
};
static const char *const quest_cautious[] = {
  "I know you, but I still require results.", "You return. Good. Familiarity is not completion.",
  "I remember you. Do not waste that.", "You are known, though the work remains.",
  "I know your face well enough to expect an answer.", "You have returned before. Return usefully now.",
  "I recognize you. Speak clearly.", "I know you, but I still need proof, not presence."
};
static const char *const quest_prompt[] = {
  "There is a matter requiring attention.", "I need someone willing to finish what they start.",
  "A problem waits unresolved.", "There is news I need brought, or brought back.", "I have a task for careful hands.",
  "If you seek a purpose, I can supply one.", "There is unfinished business that needs a spine.", "I could set you to useful work."
};
static const char *const quest_work[] = {
  "Problems multiply faster than helpers.", "There is always another loose end needing a steady hand.",
  "A sealed letter carries more weight than most swords.", "Every task ignored becomes two.", "Waiting is the dull half of duty.",
  "Someone must keep account of what remains undone.", "Work never truly ends. It only changes shape.",
  "The world sheds trouble like trees shed leaves."
};
static const char *const quest_obs[] = {
  "Most people want reward without responsibility.", "Silence usually means either peace or gathering trouble.",
  "You can learn a great deal from who arrives in a hurry.", "Delay has a smell to it.",
  "People reveal their reliability by how they return.", "A calm street can still hide an urgent need.",
  "Few listen well the first time.", "Purpose is rarer than ambition."
};
static const char *const quest_mild_susp[] = {
  "You linger like someone deciding whether to commit.", "Have you come to help, or only to hover?",
  "If you have words, use them.", "You seem uncertain. Uncertainty rarely finishes anything.",
  "Do you need direction, or courage?", "You have the posture of someone avoiding a hard answer.",
  "Do not waste both our time by circling.", "Speak plainly if you mean to be useful."
};
static const char *const quest_firm_susp[] = {
  "Enough of this hovering.", "I need action or absence, not indecision.", "Either you have business here, or you do not.",
  "I am not here to indulge dithering.", "Speak, commit, or leave.", "You are testing patience better spent elsewhere.",
  "Do not linger uselessly in front of me.", "This is no place for half-intent."
};
static const char *const quest_mild_annoy[] = {
  "I do not enjoy repeating myself.", "If you were listening, you would not ask that again.", "Time is already thinner than I prefer.",
  "Do not peck at details while the larger matter waits.", "You are edging toward nuisance.", "I need focus, not fragments.",
  "There is too much to do for this sort of delay.", "Let us proceed with a little more sense."
};
static const char *const quest_firm_annoy[] = {
  "Enough.", "You are wasting valuable time.", "I will not be pestered into nonsense.",
  "Return when you can listen properly.", "My patience has sharper limits than my duties.",
  "We are done until you regain focus.", "Do not test me further.", "I require usefulness, not noise."
};
static const char *const quest_fear[] = {
  "Not now. Not in the middle of this.", "Gods, must chaos answer every plan?", "Keep clear unless you mean to help!",
  "This is turning worse by the breath!", "There goes order for the day!", "Hold yourselves together!",
  "Trouble has found us quickly enough.", "Protect the messenger and the papers!"
};
static const char *const quest_recovery[] = {
  "Now then. Where were we?", "Chaos done? Good. The work remains.", "Back to the matter at hand.",
  "At last. Let us restore order to this.", "Very well. We continue.", "The disruption is over. The obligation is not.",
  "Good. Now speak with purpose.", "That wasted time we did not have."
};
static const char *const quest_approval[] = {
  "Good. That is the sort of answer I prefer.", "You have done well enough to be trusted further.",
  "Reliable work is always remembered.", "That was useful. More useful than most.", "You followed through. I value that.",
  "Well done. The matter is better for it.", "You have proven yourself capable.", "I will remember this favorably."
};
static const char *const quest_dismiss[] = {
  "Go, then. There is work to do.", "Return when you have something worth bringing.", "That is enough for now.",
  "Be about it.", "We are finished until there is progress.", "Do not delay without reason.", "Take the task seriously.", "Off with you, then."
};
static const char *const quest_farewell[] = {
  "Return with results.", "Go carefully and come back useful.", "May the work not outrun you.",
  "Do not waste time on the road.", "Take the task seriously.", "Be steady.", "Keep your head.", "Come back with something worth hearing."
};
static const char *const quest_emotes[] = {
  "unrolls a worn parchment and studies it again", "taps a finger against a sealed letter",
  "sorts papers into a neater stack", "glances over a written notice with narrowed eyes",
  "folds their hands while waiting for a response", "checks a wax seal for cracks",
  "slides a parchment into place with care", "reads a line twice before looking up",
  "presses thumb and finger against a tired brow", "lets out a small breath at the weight of unfinished work",
  "sets one document aside and reaches for another", "measures you with a look that weighs reliability"
};

/* Civilian, bandit, official, generic townsfolk, generic service */
static const char *const civilian_first[] = {"Oh, hello there.","Good day to you.","Well met, I suppose.","Another passerby, then.","Hope the road has treated you kindly.","Fair enough day for walking.","You seem in one piece. That is something.","Hello, then."};
static const char *const civilian_repeat[] = {"You again.","Oh, I remember you.","Back around this way, are you?","There you are again.","We do seem to cross paths.","I have seen you before.","You return often enough.","Back again, then."};
static const char *const civilian_warm[] = {"You are becoming a familiar comfort.","It is good seeing a known face.","You are easier to greet now than before.","I am glad it is you and not trouble.","You have a way of returning without causing alarm.","I have grown used to seeing you about.","You seem decent enough company.","Known folk make a place feel steadier."};
static const char *const civilian_cautious[] = {"I know you, though I still keep my wits.","You are familiar, but the world teaches caution.","I have seen you about. That is all.","You are known, though not beyond worry.","I remember you. I still hope today stays quiet.","You are familiar enough, at least.","I know your face, if not always your intent.","Back again. Let us hope for calm."};
static const char *const civilian_obs[] = {"Quiet days feel rare lately.","The street seems calmer than it was.","You can tell a lot by how hurried people walk.","I prefer ordinary days.","There is comfort in routine, if you let there be.","Some days feel heavy before anything even happens.","The city sounds different when trouble is near.","Not every silence is peaceful, but some are."};
static const char *const civilian_mild_susp[] = {"You look like you are waiting for something.","Can I help you with something?","You have been standing there a while.","Is there a reason you keep looking about?","You seem uneasy. It makes me uneasy too.","What is it you need?","You are drawing a little too much notice.","I would rather know your purpose than guess at it."};
static const char *const civilian_mild_annoy[] = {"I would rather not be bothered further.","Please leave off.","I have little taste for this.","You are making a simple moment unpleasant.","That is enough from you.","I would prefer some peace.","You are pressing too much.","Please, let that be enough."};
static const char *const civilian_firm_annoy[] = {"Leave me be.","I have had enough of this.","Do not trouble me again.","Back away.","I want no more of your attention.","Enough. Go on.","I said enough.","You are making me regret stopping."};
static const char *const civilian_fear[] = {"Please do not hurt me!","I want no part of this!","Keep away from me!","Guards! Please!","Someone help!","Not here, please!","I just want to get clear!","Gods save us!"};
static const char *const civilian_danger[] = {"Where can I get clear?","Move, move!","This is turning awful!","I do not want to die here!","Someone stop them!","Keep back from me!","There is too much panic!","This is madness!"};
static const char *const civilian_recovery[] = {"Is it over?","Thank the gods.","I thought that would never end.","I would like one quiet moment now.","Back to breathing, then.","I hate days like this.","Peace again, if only briefly.","I need my nerves back in order."};
static const char *const civilian_approval[] = {"That was kind of you.","Thank you for that.","You handled that better than most would.","I am glad someone kept a clear head.","That helped more than you know.","You did right.","I will remember that kindness.","You made this place feel safer."};
static const char *const civilian_dismiss[] = {"I should be getting on.","That is enough for me.","I would rather move along now.","Take care, then.","We are done here.","I have said what I can.","Best we leave it there.","Go well."};
static const char *const civilian_farewell[] = {"Stay safe.","Good day.","May the road be kind.","Take care out there.","Walk easy.","I hope the day treats you well.","Go gently.","Farewell for now."};
static const char *const civilian_emotes[] = {"glances over one shoulder before relaxing again","adjusts their cloak or sleeves","murmurs something too quiet to catch","steps a little aside to leave room to pass","folds their hands together for a moment","looks down the street as if expecting someone","lets out a quiet sigh","rubs at their palms absentmindedly","checks the area with a brief uneasy glance","shifts weight from one foot to the other","smooths a wrinkle from their clothes","lowers their voice when the room feels tense"};

static const char *const bandit_first[] = {"Careful where you plant your feet.","You look like you still think roads are honest.","Lost, are you?","Keep your purse close. Or do not.","This stretch favors the bold and the foolish.","You have wandered into rough company.","Roads like these take payment in one form or another.","You carry yourself like someone worth noticing."};
static const char *const bandit_repeat[] = {"Back again? Brave or stupid.","You return. Interesting.","There you are again.","I thought you might wander back my way.","You keep showing your face in risky places.","Again? You must like danger.","You have not learned caution yet.","Back to tempt fortune, are you?"};
static const char *const bandit_warm[] = {"You have a nerve I can almost respect.","I know you now. Makes this more interesting.","You return often for someone with sense to lose.","You have grit. I will give you that.","I remember you. Not everyone leaves an impression.","You walk these roads like they owe you passage.","You have lasted longer than I expected.","I have seen softer folk break sooner."};
static const char *const bandit_cautious[] = {"I know you, but I do not trust you.","You are familiar. That only sharpens interest.","I remember you. Keep that in mind.","You have been here before. Dangerous habit.","I know your face well enough to watch it closely.","Back again. That can go badly.","You are known, not safe.","I have not forgotten you."};
static const char *const bandit_mild_susp[] = {"You are looking around too carefully.","What are you counting with those eyes?","You seem to be measuring exits.","You carry questions in your posture.","Thinking of running before trouble starts?","You have the look of someone checking odds.","I do not like people who study too much.","You are too alert to be comfortable."};
static const char *const bandit_firm_susp[] = {"You are one twitch from a bad time.","Stop weighing your chances and speak.","I see the doubt in you. Do not turn it into action.","You are close to making me impatient.","Either stand easy or stand ready.","I will not be studied like prey.","You are making this sharper than it needs to be.","Mind yourself before I do it for you."};
static const char *const bandit_mild_annoy[] = {"You talk too much.","I am losing interest in your voice.","Do not peck at me.","Enough circling.","Your nerve is getting thin.","You are close to becoming work.","I have only so much patience, and it was not much.","You are pressing where you should not."};
static const char *const bandit_firm_annoy[] = {"That is enough.","You are done testing me.","I will not say it again.","Back off or bleed for it.","You have gone past nuisance.","My mood is gone.","Choose silence or consequence.","Enough from you."};
static const char *const bandit_warning[] = {"Hand it over and keep breathing.","Do not make me work harder for what I want.","You are standing where loss finds people.","Choose the easy pain.","Drop the pride before I take more than coin.","This road is expensive today.","Do not make me prove myself.","I can turn this ugly faster than you think."};
static const char *const bandit_combat[] = {"Take them!","Cut off the road!","Do not let them breathe!","Drive them down!","Now!","Break their stance!","Hit hard!","Finish it!"};
static const char *const bandit_fear[] = {"This is going too wide!","Too many moving pieces!","Watch the flanks!","Do not lose control of it!","This is messier than I wanted!","Keep your head!","Too much noise, not enough sense!","Hold the line or run!"};
static const char *const bandit_recovery[] = {"That got lively.","Could have gone cleaner.","Back to business, then.","I prefer my danger profitable.","That spilled further than I planned.","Everyone still breathing enough to matter?","Fine. Let us reset the mood.","Messy work leaves bad memories."};
static const char *const bandit_approval[] = {"Hah. There is iron in you.","That was not foolish.","You chose well for once.","I can respect that much.","You did better than I expected.","Not bad.","You have a useful streak.","That was sharp."};
static const char *const bandit_dismiss[] = {"Be gone while you still can.","Take your luck and leave.","Off with you.","We are done.","Move before my mood changes again.","Go on.","You have had enough of my time.","Best you keep walking."};
static const char *const bandit_farewell[] = {"Walk fast.","Guard your purse.","Keep your hand near your blade.","May the next road be kinder than this one.","Try not to die somewhere dull.","Go while fortune still favors you.","Stay wary.","Until the road crosses again."};
static const char *const bandit_emotes[] = {"smirks as though holding back a crueler thought","rolls their neck until it cracks softly","eyes your belt, purse, and boots in turn","spits to the side without apology","rests a hand near a weapon in easy habit","tilts their head like a wolf considering distance","leans just enough to crowd your space","watches the exits with practiced awareness","shows a brief humorless grin","drums fingers once against their belt","shifts weight like they are always ready to move","looks you over as though pricing a mistake"};

static const char *const official_first[] = {"State your business clearly.","If you have a matter, present it properly.","Order is easier when people speak plainly.","You stand before civic duty. Respect it.","If there is business to settle, begin with clarity.","Proceed in an orderly fashion.","Do not waste words where simple truth will do.","This office exists for purpose, not confusion."};
static const char *const official_repeat[] = {"You return. Very well.","Back again. Let us be efficient.","I remember this matter, or at least the burden of it.","You again. Speak plainly and save us both time.","Return visits are best accompanied by progress.","You are becoming familiar in these halls.","Very well. Continue.","You have returned. See that it is worth the interruption."};
static const char *const official_warm[] = {"You have been easier to deal with than many.","I know your face and, increasingly, your reliability.","It is good to see someone who understands procedure.","You have earned a measure of professional regard.","You return with less confusion than most.","You are known here in a favorable enough light.","I appreciate consistency when I see it.","You have proven at least somewhat dependable."};
static const char *const official_cautious[] = {"I know you, though I still expect precision.","You are familiar, not exempt.","I remember you. Let us keep matters orderly.","You are known here, but not beyond scrutiny.","Familiarity does not replace process.","You return often. Be concise.","I know your face well enough. Make your words equally useful.","Yes, I remember you. That does not shorten procedure."};
static const char *const official_service[] = {"Present the matter.","If you require assistance, explain the details.","Speak in order and I can address the problem.","State the issue plainly.","Let us proceed one fact at a time.","If there is business, there is a method to it.","Begin with what matters most.","I can help, provided you remain clear."};
static const char *const official_work[] = {"Records do not keep themselves.","Order survives only where someone maintains it.","A missing detail becomes a larger problem by evening.","Procedure may be dull, but chaos is worse.","Someone must keep count of what others ignore.","Paperwork is simply consequence written down.","Small inaccuracies grow teeth if left alone.","Careful records prevent foolish arguments later."};
static const char *const official_obs[] = {"You can tell the state of a city by how it waits.","Disorder often announces itself in impatience.","Most confusion begins with people refusing simple process.","A quiet office is either a gift or a warning.","People resent structure most when they need it most.","Routine is the spine of public peace.","Some days require ten times the order they are given.","A city frays first in small corners."};
static const char *const official_mild_susp[] = {"You seem reluctant to state your purpose.","If you have business, why hesitate?","You are lingering in a place that prefers clarity.","Say what you need.","You have the look of someone deciding how much truth to use.","This is not a hall for vague intention.","You are drawing unnecessary notice.","If there is a matter, present it now."};
static const char *const official_firm_susp[] = {"Enough hesitation.","I require a clear answer.","You are nearing removal, not resolution.","Speak plainly or leave.","This is not a place for circling behavior.","You have had ample chance to explain yourself.","I will not indulge this further.","Either become useful or become absent."};
static const char *const official_mild_annoy[] = {"You are trying my patience.","I have already answered that.","Do not turn a simple matter into a slow disaster.","This would proceed faster with less noise.","You are edging past acceptable persistence.","Please restrain your confusion.","I need precision, not repetition.","You are making this more difficult than necessary."};
static const char *const official_firm_annoy[] = {"Enough.","I will not repeat myself again.","This conversation has degraded.","You will either compose yourself or depart.","My patience for this matter is at an end.","We are finished unless you improve immediately.","Do not test procedural patience further.","You have exhausted the grace available here."};
static const char *const official_fear[] = {"This office is not a battleground!","Keep violence out of civic space!","Stand back and preserve order!","This is unacceptable!","Clear the area!","There are officials and records here, you fools!","Contain this immediately!","No bloodshed in these halls!"};
static const char *const official_recovery[] = {"Restore order at once.","Very well. We resume.","That interruption was intolerable.","Back to proper business.","Let the record show chaos has ended, for now.","Order again. Good.","Now then, compose yourselves.","We continue, though against my preference."};
static const char *const official_approval[] = {"That was handled properly.","A sensible decision.","You showed discipline there.","Good. That is how matters should proceed.","I appreciate clear conduct.","That was efficient and correct.","You did well.","That reflects well on you."};
static const char *const official_dismiss[] = {"That concludes the matter.","You may go.","We are finished here.","Proceed with your responsibilities.","Take your leave.","The matter is settled for now.","That is all.","Go on, then."};
static const char *const official_farewell[] = {"Carry yourself properly.","Go with order.","Mind your duties.","Keep to the lawful path.","May your affairs remain uncomplicated.","Good day.","Proceed carefully.","Until next time."};
static const char *const official_emotes[] = {"smooths a stack of papers into perfect alignment","checks a seal before setting a document aside","folds hands neatly while listening","glances down a line of text without visible emotion","adjusts a ledger to square it with the desk","taps one finger lightly against a written report","sets a document atop another with precise care","looks over the room with measured restraint","draws in a slow breath through the nose","lifts their chin slightly before speaking","repositions a writing tool exactly where it belongs","waits with the patience of someone used to disorder"};

static const char *const town_first[] = {"Hello there.","Well met.","Good enough day for walking.","Another face on the street.","Hope your day has held together so far.","Fair weather for passing through.","You look like you have somewhere to be.","Good day."};
static const char *const town_repeat[] = {"You again.","There is that familiar face.","We keep crossing paths.","Back again, then.","I have seen you about.","You do get around.","Another pass through, I see.","Well met once more."};
static const char *const town_warm[] = {"You are becoming part of the scenery in a good way.","It is pleasant seeing a familiar face.","You are known about here now.","I have grown used to your passing.","You seem to fit more easily each time.","It is good seeing someone recognizable.","You have become familiar company.","You bring less uncertainty than strangers do."};
static const char *const town_cautious[] = {"I know your face, if not your whole story.","You are familiar enough.","I have seen you about, yes.","You are not entirely a stranger now.","I know you by sight at least.","You have become recognizable, for what that is worth.","You are known, though I still keep my wits.","Back again. Let us hope the day stays mild."};
static const char *const town_obs[] = {"The city has its own rhythm, if you listen for it.","Some days pass softly. Others drag their boots.","It is strange how quickly a street can change its mood.","Routine holds people together more than they admit.","Quiet is often worth noticing.","The weather tells on everyone eventually.","Most days I ask only for ordinary.","There is comfort in familiar noise."};
static const char *const town_mild_susp[] = {"You seem to be waiting for something.","Can I help you find your way?","You look a little too watchful.","You have been standing there a while.","What is it you are after?","I would rather know than guess.","You seem uneasy.","You are making the air feel tighter."};
static const char *const town_mild_annoy[] = {"That is enough, I think.","You are pressing too much.","I would rather not continue this.","Please let the matter rest.","You are trying my patience.","Leave off now.","I have little taste for more of this.","That will do."};
static const char *const town_fear[] = {"Not here!","Keep clear!","Please, no!","I want no part of this!","Someone help!","Gods, not again!","Get away from me!","This has gone wrong!"};
static const char *const town_recovery[] = {"Well. That was awful.","At least it is over.","I would prefer the day quiet from here on.","My nerves will be slow to settle.","Back to ordinary, if we are lucky.","That was more than enough excitement.","Thank the gods for silence again.","I need a calmer hour than this one."};
static const char *const town_dismiss[] = {"Take care, then.","That is enough for me.","Best we leave it there.","I should move on.","Go well.","That will do.","Until next time.","Off you go."};
static const char *const town_emotes[] = {"glances up the street and back again","adjusts their sleeves or cloak","lets out a quiet breath","shifts aside to make room to pass","looks over the weather with a thoughtful squint","rests hands loosely at their sides","murmurs to themselves for a moment","tilts their head at passing activity","checks the street with a brief curious glance","squares their posture and relaxes again","rubs thumb against fingertips absentmindedly","takes one small step aside from the flow of traffic"};

static const char *const service_first[] = {"Yes? What do you need?","If you have business, say it.","I can help, if you are direct.","State your need and we will begin there.","What is it you are after?","If you require service, speak plainly.","You look like someone needing direction.","Go on, then."};
static const char *const service_repeat[] = {"Back again?","You return. What now?","I see you have more to ask.","There you are again.","Still in need of something?","You do come back often.","Again, then. Speak.","Back with another matter?"};
static const char *const service_warm[] = {"I know you well enough to skip some confusion.","You are easier to help than most.","I remember you. That helps.","You have become a familiar sort of trouble.","You are known here now.","I can work with familiar faces.","You return often enough to be expected.","You are at least predictable, which I value."};
static const char *const service_cautious[] = {"I know you, though I still prefer clear words.","You are familiar enough. Do not make that harder than it needs to be.","I remember you. Keep this simple.","You are known, but I still need you to be precise.","I have seen you before. Let us not waste time.","Back again. Fine. Speak clearly.","You are familiar, not effortless.","I know your face, if not your efficiency."};
static const char *const service_prompt[] = {"What do you need handled?","If there is a task, name it.","Tell me the matter.","Speak the need plainly.","I can only help if you stop circling.","Let us begin with the actual problem.","If you need a service, request it properly.","Enough preamble. What is it?"};
static const char *const service_work[] = {"There is always one more small task to finish.","Order survives by steady hands.","Work seldom runs out. It only queues itself.","Most problems improve when addressed promptly.","Routine holds the whole place together.","Quiet work is still work.","Someone must keep things moving.","A tidy task now prevents a larger one later."};
static const char *const service_mild_annoy[] = {"I have heard you.","Do not make this slower than it needs to be.","You are starting to wear on me.","Ask properly or not at all.","I do not enjoy repeating myself.","Enough circling.","You are spending patience needlessly.","Please move this along."};
static const char *const service_firm_annoy[] = {"Enough.","I am done repeating myself.","State it cleanly or leave.","You are wasting time now.","My patience for this is spent.","That will do.","Either proceed properly or be gone.","I will not continue like this."};
static const char *const service_fear[] = {"Not here!","Take this elsewhere!","I want no part of this disorder!","Keep back!","This is not the place for violence!","Enough of this!","Someone bring this under control!","Not in the middle of work!"};
static const char *const service_recovery[] = {"Very well. We continue.","Back to work.","That interruption helped no one.","Order again, at last.","Now then, where were we?","Good. Let the work resume.","Enough chaos. Back to the matter.","We return now to useful things."};
static const char *const service_dismiss[] = {"That is all.","Go on, then.","We are finished for now.","Take care.","Return if you must.","That concludes it.","Off with you.","Best be on your way."};
static const char *const service_emotes[] = {"checks a nearby surface for order","folds hands while waiting","straightens a small stack of items","glances at you expectantly","adjusts something purely to keep it neat","breathes out and resumes their task","looks up only briefly before returning to work","sets one item in its proper place","waits with practical patience","tilts their head as if urging you to get to the point","smooths out a wrinkle from cloth or paper","keeps working with steady, efficient motions"};

static struct npc_pool get_pool(const struct npc_social_profile *p, enum npc_line_kind kind)
{
  struct npc_pool z = { NULL, 0 };
  if (!p) return z;
  switch (p->role) {
    case NPC_ROLE_GUARD:
      switch (kind) {
        case LK_FIRST_GREET: z.lines = guard_first; z.count = ARRAYSZ(guard_first); break;
        case LK_REPEAT_GREET: z.lines = guard_repeat; z.count = ARRAYSZ(guard_repeat); break;
        case LK_WARM_RECOG: z.lines = guard_warm; z.count = ARRAYSZ(guard_warm); break;
        case LK_CAUTIOUS_RECOG: z.lines = guard_cautious; z.count = ARRAYSZ(guard_cautious); break;
        case LK_IDLE_OBSERVATION: z.lines = guard_idle_obs; z.count = ARRAYSZ(guard_idle_obs); break;
        case LK_MILD_SUSPICION: z.lines = guard_mild_susp; z.count = ARRAYSZ(guard_mild_susp); break;
        case LK_FIRM_SUSPICION: z.lines = guard_firm_susp; z.count = ARRAYSZ(guard_firm_susp); break;
        case LK_MILD_ANNOY: z.lines = guard_mild_annoy; z.count = ARRAYSZ(guard_mild_annoy); break;
        case LK_FIRM_ANNOY: z.lines = guard_firm_annoy; z.count = ARRAYSZ(guard_firm_annoy); break;
        case LK_WARNING: z.lines = guard_warning; z.count = ARRAYSZ(guard_warning); break;
        case LK_COMBAT_ALARM: z.lines = guard_combat; z.count = ARRAYSZ(guard_combat); break;
        case LK_FEAR: z.lines = guard_fear; z.count = ARRAYSZ(guard_fear); break;
        case LK_RECOVERY: z.lines = guard_recovery; z.count = ARRAYSZ(guard_recovery); break;
        case LK_APPROVAL: z.lines = guard_approval; z.count = ARRAYSZ(guard_approval); break;
        case LK_DISMISSAL: z.lines = guard_dismiss; z.count = ARRAYSZ(guard_dismiss); break;
        case LK_FAREWELL: z.lines = guard_farewell; z.count = ARRAYSZ(guard_farewell); break;
        default: break;
      }
      break;
    case NPC_ROLE_MERCHANT:
      switch (kind) {
        case LK_FIRST_GREET: z.lines = merchant_first; z.count = ARRAYSZ(merchant_first); break;
        case LK_REPEAT_GREET: z.lines = merchant_repeat; z.count = ARRAYSZ(merchant_repeat); break;
        case LK_WARM_RECOG: z.lines = merchant_warm; z.count = ARRAYSZ(merchant_warm); break;
        case LK_CAUTIOUS_RECOG: z.lines = merchant_cautious; z.count = ARRAYSZ(merchant_cautious); break;
        case LK_POLITE_SERVICE: z.lines = merchant_service; z.count = ARRAYSZ(merchant_service); break;
        case LK_IDLE_WORK: z.lines = merchant_work; z.count = ARRAYSZ(merchant_work); break;
        case LK_IDLE_OBSERVATION: z.lines = merchant_obs; z.count = ARRAYSZ(merchant_obs); break;
        case LK_MILD_SUSPICION: z.lines = merchant_mild_susp; z.count = ARRAYSZ(merchant_mild_susp); break;
        case LK_FIRM_SUSPICION: z.lines = merchant_firm_susp; z.count = ARRAYSZ(merchant_firm_susp); break;
        case LK_MILD_ANNOY: z.lines = merchant_mild_annoy; z.count = ARRAYSZ(merchant_mild_annoy); break;
        case LK_FIRM_ANNOY: z.lines = merchant_firm_annoy; z.count = ARRAYSZ(merchant_firm_annoy); break;
        case LK_FEAR: z.lines = merchant_fear; z.count = ARRAYSZ(merchant_fear); break;
        case LK_DANGER_ACTIVE: z.lines = merchant_danger; z.count = ARRAYSZ(merchant_danger); break;
        case LK_RECOVERY: z.lines = merchant_recovery; z.count = ARRAYSZ(merchant_recovery); break;
        case LK_APPROVAL: z.lines = merchant_approval; z.count = ARRAYSZ(merchant_approval); break;
        case LK_DISMISSAL: z.lines = merchant_dismiss; z.count = ARRAYSZ(merchant_dismiss); break;
        case LK_FAREWELL: z.lines = merchant_farewell; z.count = ARRAYSZ(merchant_farewell); break;
        default: break;
      }
      break;
    case NPC_ROLE_TRAINER:
      switch (kind) {
        case LK_FIRST_GREET: z.lines = trainer_first; z.count = ARRAYSZ(trainer_first); break;
        case LK_REPEAT_GREET: z.lines = trainer_repeat; z.count = ARRAYSZ(trainer_repeat); break;
        case LK_WARM_RECOG: z.lines = trainer_warm; z.count = ARRAYSZ(trainer_warm); break;
        case LK_CAUTIOUS_RECOG: z.lines = trainer_cautious; z.count = ARRAYSZ(trainer_cautious); break;
        case LK_TASK_PROMPT: z.lines = trainer_prompt; z.count = ARRAYSZ(trainer_prompt); break;
        case LK_IDLE_WORK: z.lines = trainer_work; z.count = ARRAYSZ(trainer_work); break;
        case LK_IDLE_OBSERVATION: z.lines = trainer_obs; z.count = ARRAYSZ(trainer_obs); break;
        case LK_MILD_SUSPICION: z.lines = trainer_mild_susp; z.count = ARRAYSZ(trainer_mild_susp); break;
        case LK_FIRM_SUSPICION: z.lines = trainer_firm_susp; z.count = ARRAYSZ(trainer_firm_susp); break;
        case LK_MILD_ANNOY: z.lines = trainer_mild_annoy; z.count = ARRAYSZ(trainer_mild_annoy); break;
        case LK_FIRM_ANNOY: z.lines = trainer_firm_annoy; z.count = ARRAYSZ(trainer_firm_annoy); break;
        case LK_FEAR: z.lines = trainer_fear; z.count = ARRAYSZ(trainer_fear); break;
        case LK_COMBAT_ALARM: z.lines = trainer_combat; z.count = ARRAYSZ(trainer_combat); break;
        case LK_RECOVERY: z.lines = trainer_recovery; z.count = ARRAYSZ(trainer_recovery); break;
        case LK_APPROVAL: z.lines = trainer_approval; z.count = ARRAYSZ(trainer_approval); break;
        case LK_DISMISSAL: z.lines = trainer_dismiss; z.count = ARRAYSZ(trainer_dismiss); break;
        case LK_FAREWELL: z.lines = trainer_farewell; z.count = ARRAYSZ(trainer_farewell); break;
        default: break;
      }
      break;
    case NPC_ROLE_HEALER:
      switch (kind) {
        case LK_FIRST_GREET: z.lines = healer_first; z.count = ARRAYSZ(healer_first); break;
        case LK_REPEAT_GREET: z.lines = healer_repeat; z.count = ARRAYSZ(healer_repeat); break;
        case LK_WARM_RECOG: z.lines = healer_warm; z.count = ARRAYSZ(healer_warm); break;
        case LK_CAUTIOUS_RECOG: z.lines = healer_cautious; z.count = ARRAYSZ(healer_cautious); break;
        case LK_POLITE_SERVICE: z.lines = healer_service; z.count = ARRAYSZ(healer_service); break;
        case LK_IDLE_WORK: z.lines = healer_work; z.count = ARRAYSZ(healer_work); break;
        case LK_IDLE_OBSERVATION: z.lines = healer_obs; z.count = ARRAYSZ(healer_obs); break;
        case LK_MILD_SUSPICION: z.lines = healer_mild_susp; z.count = ARRAYSZ(healer_mild_susp); break;
        case LK_FIRM_SUSPICION: z.lines = healer_firm_susp; z.count = ARRAYSZ(healer_firm_susp); break;
        case LK_MILD_ANNOY: z.lines = healer_mild_annoy; z.count = ARRAYSZ(healer_mild_annoy); break;
        case LK_FIRM_ANNOY: z.lines = healer_firm_annoy; z.count = ARRAYSZ(healer_firm_annoy); break;
        case LK_FEAR: z.lines = healer_fear; z.count = ARRAYSZ(healer_fear); break;
        case LK_DANGER_ACTIVE: z.lines = healer_danger; z.count = ARRAYSZ(healer_danger); break;
        case LK_RECOVERY: z.lines = healer_recovery; z.count = ARRAYSZ(healer_recovery); break;
        case LK_APPROVAL: z.lines = healer_approval; z.count = ARRAYSZ(healer_approval); break;
        case LK_DISMISSAL: z.lines = healer_dismiss; z.count = ARRAYSZ(healer_dismiss); break;
        case LK_FAREWELL: z.lines = healer_farewell; z.count = ARRAYSZ(healer_farewell); break;
        default: break;
      }
      break;
    case NPC_ROLE_QUESTGIVER:
      switch (kind) {
        case LK_FIRST_GREET: z.lines = quest_first; z.count = ARRAYSZ(quest_first); break;
        case LK_REPEAT_GREET: z.lines = quest_repeat; z.count = ARRAYSZ(quest_repeat); break;
        case LK_WARM_RECOG: z.lines = quest_warm; z.count = ARRAYSZ(quest_warm); break;
        case LK_CAUTIOUS_RECOG: z.lines = quest_cautious; z.count = ARRAYSZ(quest_cautious); break;
        case LK_TASK_PROMPT: z.lines = quest_prompt; z.count = ARRAYSZ(quest_prompt); break;
        case LK_IDLE_WORK: z.lines = quest_work; z.count = ARRAYSZ(quest_work); break;
        case LK_IDLE_OBSERVATION: z.lines = quest_obs; z.count = ARRAYSZ(quest_obs); break;
        case LK_MILD_SUSPICION: z.lines = quest_mild_susp; z.count = ARRAYSZ(quest_mild_susp); break;
        case LK_FIRM_SUSPICION: z.lines = quest_firm_susp; z.count = ARRAYSZ(quest_firm_susp); break;
        case LK_MILD_ANNOY: z.lines = quest_mild_annoy; z.count = ARRAYSZ(quest_mild_annoy); break;
        case LK_FIRM_ANNOY: z.lines = quest_firm_annoy; z.count = ARRAYSZ(quest_firm_annoy); break;
        case LK_FEAR: z.lines = quest_fear; z.count = ARRAYSZ(quest_fear); break;
        case LK_RECOVERY: z.lines = quest_recovery; z.count = ARRAYSZ(quest_recovery); break;
        case LK_APPROVAL: z.lines = quest_approval; z.count = ARRAYSZ(quest_approval); break;
        case LK_DISMISSAL: z.lines = quest_dismiss; z.count = ARRAYSZ(quest_dismiss); break;
        case LK_FAREWELL: z.lines = quest_farewell; z.count = ARRAYSZ(quest_farewell); break;
        default: break;
      }
      break;
    case NPC_ROLE_CIVILIAN:
      switch (kind) {
        case LK_FIRST_GREET: z.lines = civilian_first; z.count = ARRAYSZ(civilian_first); break;
        case LK_REPEAT_GREET: z.lines = civilian_repeat; z.count = ARRAYSZ(civilian_repeat); break;
        case LK_WARM_RECOG: z.lines = civilian_warm; z.count = ARRAYSZ(civilian_warm); break;
        case LK_CAUTIOUS_RECOG: z.lines = civilian_cautious; z.count = ARRAYSZ(civilian_cautious); break;
        case LK_IDLE_OBSERVATION: z.lines = civilian_obs; z.count = ARRAYSZ(civilian_obs); break;
        case LK_MILD_SUSPICION: z.lines = civilian_mild_susp; z.count = ARRAYSZ(civilian_mild_susp); break;
        case LK_MILD_ANNOY: z.lines = civilian_mild_annoy; z.count = ARRAYSZ(civilian_mild_annoy); break;
        case LK_FIRM_ANNOY: z.lines = civilian_firm_annoy; z.count = ARRAYSZ(civilian_firm_annoy); break;
        case LK_FEAR: z.lines = civilian_fear; z.count = ARRAYSZ(civilian_fear); break;
        case LK_DANGER_ACTIVE: z.lines = civilian_danger; z.count = ARRAYSZ(civilian_danger); break;
        case LK_RECOVERY: z.lines = civilian_recovery; z.count = ARRAYSZ(civilian_recovery); break;
        case LK_APPROVAL: z.lines = civilian_approval; z.count = ARRAYSZ(civilian_approval); break;
        case LK_DISMISSAL: z.lines = civilian_dismiss; z.count = ARRAYSZ(civilian_dismiss); break;
        case LK_FAREWELL: z.lines = civilian_farewell; z.count = ARRAYSZ(civilian_farewell); break;
        default: break;
      }
      break;
    case NPC_ROLE_BANDIT:
      switch (kind) {
        case LK_FIRST_GREET: z.lines = bandit_first; z.count = ARRAYSZ(bandit_first); break;
        case LK_REPEAT_GREET: z.lines = bandit_repeat; z.count = ARRAYSZ(bandit_repeat); break;
        case LK_WARM_RECOG: z.lines = bandit_warm; z.count = ARRAYSZ(bandit_warm); break;
        case LK_CAUTIOUS_RECOG: z.lines = bandit_cautious; z.count = ARRAYSZ(bandit_cautious); break;
        case LK_MILD_SUSPICION: z.lines = bandit_mild_susp; z.count = ARRAYSZ(bandit_mild_susp); break;
        case LK_FIRM_SUSPICION: z.lines = bandit_firm_susp; z.count = ARRAYSZ(bandit_firm_susp); break;
        case LK_MILD_ANNOY: z.lines = bandit_mild_annoy; z.count = ARRAYSZ(bandit_mild_annoy); break;
        case LK_FIRM_ANNOY: z.lines = bandit_firm_annoy; z.count = ARRAYSZ(bandit_firm_annoy); break;
        case LK_WARNING: z.lines = bandit_warning; z.count = ARRAYSZ(bandit_warning); break;
        case LK_COMBAT_ALARM: z.lines = bandit_combat; z.count = ARRAYSZ(bandit_combat); break;
        case LK_FEAR: z.lines = bandit_fear; z.count = ARRAYSZ(bandit_fear); break;
        case LK_RECOVERY: z.lines = bandit_recovery; z.count = ARRAYSZ(bandit_recovery); break;
        case LK_APPROVAL: z.lines = bandit_approval; z.count = ARRAYSZ(bandit_approval); break;
        case LK_DISMISSAL: z.lines = bandit_dismiss; z.count = ARRAYSZ(bandit_dismiss); break;
        case LK_FAREWELL: z.lines = bandit_farewell; z.count = ARRAYSZ(bandit_farewell); break;
        default: break;
      }
      break;
    case NPC_ROLE_OFFICIAL:
      switch (kind) {
        case LK_FIRST_GREET: z.lines = official_first; z.count = ARRAYSZ(official_first); break;
        case LK_REPEAT_GREET: z.lines = official_repeat; z.count = ARRAYSZ(official_repeat); break;
        case LK_WARM_RECOG: z.lines = official_warm; z.count = ARRAYSZ(official_warm); break;
        case LK_CAUTIOUS_RECOG: z.lines = official_cautious; z.count = ARRAYSZ(official_cautious); break;
        case LK_POLITE_SERVICE: z.lines = official_service; z.count = ARRAYSZ(official_service); break;
        case LK_IDLE_WORK: z.lines = official_work; z.count = ARRAYSZ(official_work); break;
        case LK_IDLE_OBSERVATION: z.lines = official_obs; z.count = ARRAYSZ(official_obs); break;
        case LK_MILD_SUSPICION: z.lines = official_mild_susp; z.count = ARRAYSZ(official_mild_susp); break;
        case LK_FIRM_SUSPICION: z.lines = official_firm_susp; z.count = ARRAYSZ(official_firm_susp); break;
        case LK_MILD_ANNOY: z.lines = official_mild_annoy; z.count = ARRAYSZ(official_mild_annoy); break;
        case LK_FIRM_ANNOY: z.lines = official_firm_annoy; z.count = ARRAYSZ(official_firm_annoy); break;
        case LK_FEAR: z.lines = official_fear; z.count = ARRAYSZ(official_fear); break;
        case LK_RECOVERY: z.lines = official_recovery; z.count = ARRAYSZ(official_recovery); break;
        case LK_APPROVAL: z.lines = official_approval; z.count = ARRAYSZ(official_approval); break;
        case LK_DISMISSAL: z.lines = official_dismiss; z.count = ARRAYSZ(official_dismiss); break;
        case LK_FAREWELL: z.lines = official_farewell; z.count = ARRAYSZ(official_farewell); break;
        default: break;
      }
      break;
    case NPC_ROLE_GENERIC_SERVICE:
      switch (kind) {
        case LK_FIRST_GREET: z.lines = service_first; z.count = ARRAYSZ(service_first); break;
        case LK_REPEAT_GREET: z.lines = service_repeat; z.count = ARRAYSZ(service_repeat); break;
        case LK_WARM_RECOG: z.lines = service_warm; z.count = ARRAYSZ(service_warm); break;
        case LK_CAUTIOUS_RECOG: z.lines = service_cautious; z.count = ARRAYSZ(service_cautious); break;
        case LK_TASK_PROMPT: z.lines = service_prompt; z.count = ARRAYSZ(service_prompt); break;
        case LK_IDLE_WORK: z.lines = service_work; z.count = ARRAYSZ(service_work); break;
        case LK_MILD_ANNOY: z.lines = service_mild_annoy; z.count = ARRAYSZ(service_mild_annoy); break;
        case LK_FIRM_ANNOY: z.lines = service_firm_annoy; z.count = ARRAYSZ(service_firm_annoy); break;
        case LK_FEAR: z.lines = service_fear; z.count = ARRAYSZ(service_fear); break;
        case LK_RECOVERY: z.lines = service_recovery; z.count = ARRAYSZ(service_recovery); break;
        case LK_DISMISSAL: z.lines = service_dismiss; z.count = ARRAYSZ(service_dismiss); break;
        default: break;
      }
      break;
    case NPC_ROLE_GENERIC_TOWNSFOLK:
    default:
      switch (kind) {
        case LK_FIRST_GREET: z.lines = town_first; z.count = ARRAYSZ(town_first); break;
        case LK_REPEAT_GREET: z.lines = town_repeat; z.count = ARRAYSZ(town_repeat); break;
        case LK_WARM_RECOG: z.lines = town_warm; z.count = ARRAYSZ(town_warm); break;
        case LK_CAUTIOUS_RECOG: z.lines = town_cautious; z.count = ARRAYSZ(town_cautious); break;
        case LK_IDLE_OBSERVATION: z.lines = town_obs; z.count = ARRAYSZ(town_obs); break;
        case LK_MILD_SUSPICION: z.lines = town_mild_susp; z.count = ARRAYSZ(town_mild_susp); break;
        case LK_MILD_ANNOY: z.lines = town_mild_annoy; z.count = ARRAYSZ(town_mild_annoy); break;
        case LK_FEAR: z.lines = town_fear; z.count = ARRAYSZ(town_fear); break;
        case LK_RECOVERY: z.lines = town_recovery; z.count = ARRAYSZ(town_recovery); break;
        case LK_DISMISSAL: z.lines = town_dismiss; z.count = ARRAYSZ(town_dismiss); break;
        default: break;
      }
      break;
  }
  return z;
}

static struct npc_pool get_emote_pool(const struct npc_social_profile *p)
{
  struct npc_pool z = { NULL, 0 };
  if (!p) return z;
  switch (p->role) {
    case NPC_ROLE_GUARD: z.lines = guard_emotes; z.count = ARRAYSZ(guard_emotes); break;
    case NPC_ROLE_MERCHANT: z.lines = merchant_emotes; z.count = ARRAYSZ(merchant_emotes); break;
    case NPC_ROLE_TRAINER: z.lines = trainer_emotes; z.count = ARRAYSZ(trainer_emotes); break;
    case NPC_ROLE_HEALER: z.lines = healer_emotes; z.count = ARRAYSZ(healer_emotes); break;
    case NPC_ROLE_QUESTGIVER: z.lines = quest_emotes; z.count = ARRAYSZ(quest_emotes); break;
    case NPC_ROLE_CIVILIAN: z.lines = civilian_emotes; z.count = ARRAYSZ(civilian_emotes); break;
    case NPC_ROLE_BANDIT: z.lines = bandit_emotes; z.count = ARRAYSZ(bandit_emotes); break;
    case NPC_ROLE_OFFICIAL: z.lines = official_emotes; z.count = ARRAYSZ(official_emotes); break;
    case NPC_ROLE_GENERIC_SERVICE: z.lines = service_emotes; z.count = ARRAYSZ(service_emotes); break;
    default: z.lines = town_emotes; z.count = ARRAYSZ(town_emotes); break;
  }
  return z;
}

static const char *npc_pick_line_for_kind(struct char_data *ch, const struct npc_social_profile *p, enum npc_line_kind kind)
{
  struct npc_pool pool = get_pool(p, kind);
  return pick_from_pool(ch, &pool, FALSE);
}

const char *npc_ai_get_dialogue_line(const struct npc_social_profile *profile, enum npc_priority prio, int repeat)
{
  (void)repeat;
  if (!profile) return NULL;
  if (prio == NPC_PRIO_WARN) return "Choose calm while you still can.";
  if (prio == NPC_PRIO_ENGAGE) return "Hold the line!";
  if (prio == NPC_PRIO_WORK) return "State what you need and we will proceed.";
  if (profile->role == NPC_ROLE_CIVILIAN) return "Good day to you.";
  return "Another day, another duty.";
}

void npc_ai_do_emote(struct char_data *ch, const struct npc_social_profile *profile, time_t now)
{
  char buf[MAX_STRING_LENGTH];
  const char *emote;
  struct npc_pool pool;
  (void)now;
  if (!ch || !profile || !ch->ai_state) return;
  pool = get_emote_pool(profile);
  emote = pick_from_pool(ch, &pool, TRUE);
  if (!emote) return;
  snprintf(buf, sizeof(buf), "$n %s.", emote);
  act(buf, TRUE, ch, 0, 0, TO_ROOM);
}

void npc_ai_update_memory(struct char_data *ch, struct char_data *player, int trust_delta, int annoyance_delta, int fear_delta, time_t now)
{
  struct ai_actor_memory_entry *m;
  if (!ch || !player || IS_NPC(player) || !ch->ai_prof || !ch->ai_prof->memory_enabled) return;
  m = npc_mem_get(ch, GET_IDNUM(player));
  if (!m) return;
  m->trust = CLAMP(m->trust + trust_delta * (trust_delta >= 0 ? ch->ai_prof->trust_gain : ch->ai_prof->trust_loss) / 100, -100, 100);
  m->attitude = CLAMP(m->attitude - annoyance_delta * ch->ai_prof->hostility_gain / 100, -100, 100);
  m->hostility = CLAMP(m->hostility + annoyance_delta * ch->ai_prof->hostility_gain / 100, 0, 100);
  m->fear = CLAMP(m->fear + fear_delta * ch->ai_prof->fear_gain / 100, 0, 100);
  m->familiarity = CLAMP(m->familiarity + (trust_delta > 0 ? ch->ai_prof->familiarity_gain / 20 : 0), 0, 100);
  m->identity_confidence = 85; m->belief_confidence = 85.0f;
  m->last_seen_time = now;
  m->last_interaction_time = now;
}

static int npc_role_supports_combat_alarm(const struct npc_social_profile *p)
{
  return (p && (p->role == NPC_ROLE_GUARD || p->role == NPC_ROLE_TRAINER || p->role == NPC_ROLE_BANDIT));
}

static int npc_should_respond_to_speech(const struct npc_social_profile *p, int intent, int is_first_greeting)
{
  int chance = 60;
  if (intent == SAY_INTENT_RUDE || intent == SAY_INTENT_INAPPROPRIATE || intent == SAY_INTENT_THREAT) return TRUE;
  if (intent == SAY_INTENT_GREETING && is_first_greeting) return TRUE;

  if (p) {
    if (p->social_style == NPC_SOCIAL_EXTROVERT) chance = 70;
    else if (p->social_style == NPC_SOCIAL_INTROVERT) chance = 50;
  }
  return (rand_number(1, 100) <= chance);
}

static void npc_decay_escalation(struct ai_actor_memory_entry *m, time_t now)
{
  if (!m) return;
  if (m->last_reply_time <= 0 || (now - m->last_reply_time) > NPC_ESCALATE_WINDOW) m->last_intent = 0;
  if (m->last_topic_time <= 0 || (now - m->last_topic_time) > NPC_ESCALATE_WINDOW) m->last_topic = 0;
}

static enum npc_line_kind npc_kind_for_service_role(const struct npc_social_profile *p)
{
  if (!p) return LK_TASK_PROMPT;
  if (p->role == NPC_ROLE_MERCHANT || p->role == NPC_ROLE_HEALER || p->role == NPC_ROLE_OFFICIAL)
    return LK_POLITE_SERVICE;
  if (p->role == NPC_ROLE_TRAINER || p->role == NPC_ROLE_QUESTGIVER || p->role == NPC_ROLE_GENERIC_SERVICE)
    return LK_TASK_PROMPT;
  return LK_TASK_PROMPT;
}

static enum npc_line_kind npc_kind_for_question_role(const struct npc_social_profile *p)
{
  if (!p) return LK_TASK_PROMPT;
  if (p->role == NPC_ROLE_MERCHANT || p->role == NPC_ROLE_HEALER || p->role == NPC_ROLE_OFFICIAL ||
      p->role == NPC_ROLE_GENERIC_SERVICE)
    return LK_POLITE_SERVICE;
  return LK_TASK_PROMPT;
}

static int npc_try_speech_with_fallback(struct char_data *ch, const struct npc_social_profile *p, struct ai_actor_memory_entry *m, enum npc_line_kind kind, time_t now)
{
  const char *line = NULL;

  if (m && m->last_topic_key[0] == (char)kind && (now - m->last_reaction) < NPC_CATEGORY_REPEAT_WINDOW)
    line = NULL;
  else
    line = npc_pick_line_for_kind(ch, p, kind);

  if (!line && kind != LK_IDLE_OBSERVATION)
    line = npc_pick_line_for_kind(ch, p, LK_IDLE_OBSERVATION);
  if (!line && kind != LK_DISMISSAL)
    line = npc_pick_line_for_kind(ch, p, LK_DISMISSAL);
  if (!line) {
    if (rand_number(1, 100) <= 65) npc_ai_do_emote(ch, p, now);
    return FALSE;
  }

  npc_say(ch, line);
  ch->ai_state->last_spoke = now;
  return TRUE;
}

enum npc_priority npc_ai_choose_priority(struct char_data *ch, const struct npc_social_profile *profile, time_t now)
{
  (void)now;
  if (!ch || !profile) return NPC_PRIO_IDLE;
  if (FIGHTING(ch)) return NPC_PRIO_ENGAGE;
  if (profile->role == NPC_ROLE_MERCHANT || profile->role == NPC_ROLE_TRAINER ||
      profile->role == NPC_ROLE_HEALER || profile->role == NPC_ROLE_QUESTGIVER ||
      profile->role == NPC_ROLE_OFFICIAL || profile->role == NPC_ROLE_GENERIC_SERVICE)
    return NPC_PRIO_WORK;
  return NPC_PRIO_OBSERVE;
}

void npc_ai_handle_player_enter(struct char_data *ch, struct char_data *player, time_t now)
{
  struct npc_social_profile p;
  struct ai_actor_memory_entry *m;
  enum npc_line_kind kind = LK_FIRST_GREET;
  const char *line = NULL;
  int familiarity = 0;
  int annoyance = 0;
  int repeat_window = 0;

  if (!npc_ai_is_humanoid_social_candidate(ch) || !player || IS_NPC(player) || !ch->ai_state) return;
  npc_ai_build_profile(ch, &p);
  m = npc_mem_get(ch, GET_IDNUM(player));
  if (m) {
    if (m->belief_confidence < 1000.0f) m->belief_confidence += 1.0f;
    familiarity = (int)m->belief_confidence;
    annoyance = -m->attitude;
    repeat_window = (m->last_seen_time > 0 && (now - m->last_seen_time) < (12 * 3600));
  }

  if ((p.role == NPC_ROLE_GUARD || p.role == NPC_ROLE_OFFICIAL) && repeat_window && (annoyance > 20 || (m && m->fear > 25))) {
    kind = LK_MILD_SUSPICION;
  } else if (familiarity < 2) {
    kind = LK_FIRST_GREET;
  } else if (familiarity < 6) {
    kind = LK_REPEAT_GREET;
  } else if (m && m->trust >= 20 && annoyance < 20 && m->fear < 25) {
    kind = (rand_number(1, 100) <= 65) ? LK_WARM_RECOG : LK_REPEAT_GREET;
  } else if (m && (annoyance >= 20 || m->fear >= 25)) {
    kind = (rand_number(1, 100) <= 70) ? LK_CAUTIOUS_RECOG : LK_MILD_SUSPICION;
  } else {
    kind = LK_REPEAT_GREET;
  }

  if (familiarity >= 3 && rand_number(1, 100) <= 24)
    kind = LK_IDLE_OBSERVATION;

  if ((now - ch->ai_state->last_spoke) < ((p.social_style == NPC_SOCIAL_EXTROVERT) ? 8 : 15)) {
    if (rand_number(1, 100) <= 55) npc_ai_do_emote(ch, &p, now);
    return;
  }

  line = npc_pick_line_for_kind(ch, &p, kind);
  if (!line && kind != LK_IDLE_OBSERVATION)
    line = npc_pick_line_for_kind(ch, &p, LK_IDLE_OBSERVATION);
  if (!line) {
    if (rand_number(1, 100) <= 70) npc_ai_do_emote(ch, &p, now);
    return;
  }

  npc_say(ch, line);
  ch->ai_state->last_spoke = now;
  npc_ai_update_memory(ch, player, 1, 0, 0, now);
}

void npc_ai_handle_player_leave(struct char_data *ch, struct char_data *player, time_t now)
{
  struct npc_social_profile p;
  const char *line;
  if (!npc_ai_is_humanoid_social_candidate(ch) || !player || IS_NPC(player) || !ch->ai_state) return;
  npc_ai_update_memory(ch, player, 0, 0, 0, now);
  if ((now - ch->ai_state->last_spoke) < 4) return;
  npc_ai_build_profile(ch, &p);
  line = npc_pick_line_for_kind(ch, &p, (rand_number(1, 100) <= 50) ? LK_DISMISSAL : LK_FAREWELL);
  if (line && rand_number(1, 100) <= 45) {
    npc_say(ch, line);
    ch->ai_state->last_spoke = now;
  }
}

void npc_ai_handle_speech_event(struct char_data *ch, struct char_data *player, const char *text, time_t now)
{
  struct npc_social_profile p;
  enum npc_line_kind kind = LK_IDLE_OBSERVATION;
  struct ai_actor_memory_entry *m;
  int intent = SAY_INTENT_UNCLEAR;
  int is_first_greeting = FALSE;
  int responded;
  int annoyance;

  if (!npc_ai_is_humanoid_social_candidate(ch) || !player || IS_NPC(player) || !ch->ai_state) return;
  npc_ai_build_profile(ch, &p);
  m = npc_mem_get(ch, GET_IDNUM(player));
  annoyance = m ? -m->attitude : 0;
  intent = npc_detect_say_intent(text);
  is_first_greeting = (intent == SAY_INTENT_GREETING && m && m->belief_confidence < 2.0f);

  if (text && (has_kw(text, "bye") || has_kw(text, "farewell") || has_kw(text, "goodbye"))) {
    kind = (rand_number(1, 100) <= 60) ? LK_FAREWELL : LK_DISMISSAL;
    npc_ai_update_memory(ch, player, 1, 0, 0, now);
  } else if (text && (has_kw(text, "thank") || has_kw(text, "appreciate"))) {
    kind = LK_APPROVAL;
    npc_ai_update_memory(ch, player, 2, 0, -1, now);
  } else {
    npc_decay_escalation(m, now);
    switch (intent) {
      case SAY_INTENT_GREETING:
        kind = (m && m->trust >= 20) ? LK_WARM_RECOG : ((m && m->belief_confidence >= 2.0f) ? LK_REPEAT_GREET : LK_FIRST_GREET);
        npc_ai_update_memory(ch, player, 2, 0, 0, now);
        break;
      case SAY_INTENT_QUESTION:
        kind = npc_kind_for_question_role(&p);
        npc_ai_update_memory(ch, player, 1, 0, 0, now);
        break;
      case SAY_INTENT_SERVICE:
        kind = npc_kind_for_service_role(&p);
        npc_ai_update_memory(ch, player, 2, 0, 0, now);
        break;
      case SAY_INTENT_SMALLTALK:
        kind = LK_IDLE_OBSERVATION;
        npc_ai_update_memory(ch, player, 1, 0, 0, now);
        break;
      case SAY_INTENT_PRAISE:
        kind = (m && m->trust >= 12) ? LK_WARM_RECOG : LK_APPROVAL;
        npc_ai_update_memory(ch, player, 2, 0, -1, now);
        break;
      case SAY_INTENT_RUDE:
        if (m) {
          m->last_intent = CLAMP(m->last_intent + 1, 1, 3);
          m->last_reply_time = now;
        }
        kind = (m && m->last_intent >= 2) ? LK_FIRM_ANNOY : LK_MILD_ANNOY;
        if (p.role == NPC_ROLE_GUARD && m && m->last_intent >= 2) kind = LK_WARNING;
        else if (p.role == NPC_ROLE_BANDIT) kind = (m && m->last_intent >= 2) ? LK_WARNING : LK_FIRM_ANNOY;
        else if (p.role == NPC_ROLE_HEALER) kind = LK_MILD_ANNOY;
        else if (p.role == NPC_ROLE_MERCHANT || p.role == NPC_ROLE_TRAINER) kind = LK_FIRM_ANNOY;
        npc_ai_update_memory(ch, player, -1, 9, 1, now);
        break;
      case SAY_INTENT_INAPPROPRIATE:
        if (m) {
          m->last_topic = CLAMP(m->last_topic + 1, 1, 3);
          m->last_topic_time = now;
        }
        kind = (m && m->last_topic >= 2) ? LK_WARNING : LK_FIRM_ANNOY;
        if (p.role == NPC_ROLE_GUARD) kind = LK_WARNING;
        npc_ai_update_memory(ch, player, -2, 10, 2, now);
        break;
      case SAY_INTENT_THREAT:
        if (p.role == NPC_ROLE_HEALER)
          kind = (rand_number(1, 100) <= 65) ? LK_FEAR : LK_WARNING;
        else if (npc_role_supports_combat_alarm(&p))
          kind = LK_COMBAT_ALARM;
        else
          kind = LK_WARNING;
        if (p.role == NPC_ROLE_GUARD) kind = LK_WARNING;
        npc_ai_update_memory(ch, player, -3, 12, 4, now);
        break;
      case SAY_INTENT_UNCLEAR:
      default:
        if (annoyance > 30) kind = LK_FIRM_ANNOY;
        else if (annoyance > 15) kind = LK_MILD_ANNOY;
        else kind = (rand_number(1, 100) <= 50) ? LK_IDLE_OBSERVATION : LK_TASK_PROMPT;
        npc_ai_update_memory(ch, player, 0, 0, 0, now);
        break;
    }
  }

  if ((now - ch->ai_state->last_spoke) < 4) {
    if (intent == SAY_INTENT_RUDE || intent == SAY_INTENT_INAPPROPRIATE || intent == SAY_INTENT_THREAT)
      npc_ai_do_emote(ch, &p, now);
    return;
  }
  if (!npc_should_respond_to_speech(&p, intent, is_first_greeting)) return;

  responded = npc_try_speech_with_fallback(ch, &p, m, kind, now);
  if (responded && m) {
    m->last_topic_key[0] = (char)kind;
    m->last_topic_key[1] = '\0';
    m->last_reaction = now;
  }
}

void npc_ai_handle_room_danger(struct char_data *ch, struct char_data *actor, time_t now)
{
  struct npc_social_profile p;
  const char *line = NULL;
  int roll;

  if (!npc_ai_is_humanoid_social_candidate(ch) || !ch->ai_state) return;
  npc_ai_build_profile(ch, &p);
  roll = rand_number(1, 100);

  switch (p.role) {
    case NPC_ROLE_GUARD: line = npc_pick_line_for_kind(ch, &p, (roll <= 55) ? LK_COMBAT_ALARM : LK_WARNING); break;
    case NPC_ROLE_CIVILIAN: line = npc_pick_line_for_kind(ch, &p, (roll <= 50) ? LK_FEAR : LK_DANGER_ACTIVE); break;
    case NPC_ROLE_HEALER: line = npc_pick_line_for_kind(ch, &p, (roll <= 60) ? LK_FEAR : LK_DANGER_ACTIVE); break;
    case NPC_ROLE_MERCHANT: line = npc_pick_line_for_kind(ch, &p, (roll <= 60) ? LK_FEAR : LK_DANGER_ACTIVE); break;
    case NPC_ROLE_TRAINER: line = npc_pick_line_for_kind(ch, &p, (roll <= 55) ? LK_COMBAT_ALARM : LK_FEAR); break;
    case NPC_ROLE_OFFICIAL: line = npc_pick_line_for_kind(ch, &p, LK_FEAR); break;
    case NPC_ROLE_BANDIT: line = npc_pick_line_for_kind(ch, &p, (roll <= 50) ? LK_COMBAT_ALARM : LK_FEAR); break;
    default: line = npc_pick_line_for_kind(ch, &p, LK_FEAR); break;
  }

  if (line) npc_say(ch, line);
  if (actor && !IS_NPC(actor)) npc_ai_update_memory(ch, actor, -3, 5, 15, now);
  ch->ai_state->last_spoke = now;
  strlcpy(ch->ai_state->last_speak_reason, "danger", sizeof(ch->ai_state->last_speak_reason));
}

void npc_ai_maybe_do_ambient_action(struct char_data *ch, const struct npc_social_profile *profile, time_t now)
{
  int emote_cd;
  const char *line;
  if (!ch || !profile || !ch->ai_state) return;

  if (!strcmp(ch->ai_state->last_speak_reason, "danger") && (now - ch->ai_state->last_spoke) >= 9) {
    line = npc_pick_line_for_kind(ch, profile, LK_RECOVERY);
    if (line) {
      npc_say(ch, line);
      ch->ai_state->last_spoke = now;
      strlcpy(ch->ai_state->last_speak_reason, "recovery", sizeof(ch->ai_state->last_speak_reason));
      return;
    }
  }

  emote_cd = (profile->social_style == NPC_SOCIAL_EXTROVERT) ? 14 : (profile->social_style == NPC_SOCIAL_INTROVERT ? 42 : 24);
  if ((now - ch->ai_state->last_emote_time) >= emote_cd) {
    npc_ai_do_emote(ch, profile, now);
    ch->ai_state->last_emote_time = now;
  }

  if ((now - ch->ai_state->last_spoke) > 35 && rand_number(1, 100) <= 28) {
    line = npc_pick_line_for_kind(ch, profile, (rand_number(1, 100) <= 60) ? LK_IDLE_WORK : LK_IDLE_OBSERVATION);
    if (line) {
      npc_say(ch, line);
      ch->ai_state->last_spoke = now;
      strlcpy(ch->ai_state->last_speak_reason, "ambient", sizeof(ch->ai_state->last_speak_reason));
    }
  }
}
