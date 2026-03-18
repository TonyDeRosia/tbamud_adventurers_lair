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

static int has_kw(const char *text, const char *kw)
{
  if (!text || !kw || !*kw) return FALSE;
  return (strstr(text, kw) != NULL);
}

static struct ai_actor_memory_entry *npc_mem_get(struct char_data *ch, long idnum)
{
  int i;
  if (!ch || !ch->ai_state || idnum <= 0) return NULL;
  for (i = 0; i < ch->ai_state->mem_count; i++)
    if (ch->ai_state->mem[i].idnum == idnum)
      return &ch->ai_state->mem[i];
  if (ch->ai_state->mem_count >= AI_MEM_MAX) return NULL;
  i = ch->ai_state->mem_count++;
  memset(&ch->ai_state->mem[i], 0, sizeof(ch->ai_state->mem[i]));
  ch->ai_state->mem[i].idnum = idnum;
  return &ch->ai_state->mem[i];
}

int npc_ai_is_humanoid_social_candidate(struct char_data *ch)
{
  /* Builder-controlled entry gate: AI_ACTOR means this NPC opts into social AI. */
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

  /* 1) Explicit service/gameplay roles first. */
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
  /* 2) Guard / civic enforcement role. */
  } else if (has_guard_spec || has_kw(name, "guard") || has_kw(name, "watch") || has_kw(name, "sentinel") ||
             has_kw(sdesc, "guard") || has_kw(sdesc, "watch") || has_kw(sdesc, "sentinel")) {
    out->role = NPC_ROLE_GUARD;
  /* 3) Bandit / hostile social role if explicitly suited. */
  } else if (has_kw(name, "bandit") || has_kw(name, "brigand") || has_kw(name, "outlaw") || has_kw(sdesc, "bandit")) {
    out->role = NPC_ROLE_BANDIT;
  /* 4) Safe fallback: service or townsfolk. */
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

const char *npc_ai_get_dialogue_line(const struct npc_social_profile *profile, enum npc_priority prio, int repeat)
{
  if (!profile) return NULL;
  switch (profile->role) {
    case NPC_ROLE_GUARD:
      if (prio == NPC_PRIO_WARN) return "Stand down before this gets ugly.";
      if (prio == NPC_PRIO_ENGAGE) return "To arms!";
      if (prio == NPC_PRIO_OBSERVE) return "What are you doing lurking about?";
      return repeat ? "You again. Still keeping clean, I hope." : "Move along and keep the peace.";
    case NPC_ROLE_MERCHANT:
      if (prio == NPC_PRIO_WORK) return "Have a look at the goods.";
      if (prio == NPC_PRIO_WARN) return "If you are not buying, do not waste my daylight.";
      if (prio == NPC_PRIO_FLEE) return "Gods preserve me, not in my shop!";
      return repeat ? "Back for more, are you?" : "Welcome. Take a look, but do not break anything.";
    case NPC_ROLE_TRAINER:
      if (prio == NPC_PRIO_WORK) return "Again. Faster this time.";
      if (prio == NPC_PRIO_WARN) return "Stop fidgeting and focus.";
      return "If you seek discipline, you are in the right place.";
    case NPC_ROLE_HEALER:
      if (prio == NPC_PRIO_FLEE || prio == NPC_PRIO_WARN) return "Please, no bloodshed here.";
      return "Peace be with you. Are you wounded?";
    case NPC_ROLE_QUESTGIVER:
      return repeat ? "Have you come about the matter I mentioned?" : "There may be work for one with steady hands.";
    case NPC_ROLE_CIVILIAN:
      if (prio == NPC_PRIO_FLEE) return "Please do not hurt me!";
      return "Good day to you.";
    case NPC_ROLE_BANDIT:
      if (prio == NPC_PRIO_WARN || prio == NPC_PRIO_ENGAGE) return "Do not make me ask twice.";
      return "Careful where you wander.";
    default:
      return "Another day in town.";
  }
}

void npc_ai_do_emote(struct char_data *ch, const struct npc_social_profile *profile, time_t now)
{
  const char *emote = NULL;
  (void)now;
  if (!ch || !profile) return;
  switch (profile->role) {
    case NPC_ROLE_GUARD: emote = "$n folds their arms and scans the area."; break;
    case NPC_ROLE_MERCHANT: emote = "$n sorts a few items on the counter."; break;
    case NPC_ROLE_TRAINER: emote = "$n rolls their shoulders and studies your stance."; break;
    case NPC_ROLE_HEALER: emote = "$n arranges clean bandages neatly."; break;
    case NPC_ROLE_QUESTGIVER: emote = "$n unrolls a worn parchment."; break;
    case NPC_ROLE_CIVILIAN: emote = "$n glances around uneasily."; break;
    case NPC_ROLE_BANDIT: emote = "$n smirks without warmth."; break;
    default: emote = "$n watches the room in silence."; break;
  }
  act(emote, TRUE, ch, 0, 0, TO_ROOM);
}

void npc_ai_update_memory(struct char_data *ch, struct char_data *player, int trust_delta, int annoyance_delta, int fear_delta, time_t now)
{
  struct ai_actor_memory_entry *m;
  if (!ch || !player || IS_NPC(player)) return;
  m = npc_mem_get(ch, GET_IDNUM(player));
  if (!m) return;
  m->trust = CLAMP(m->trust + trust_delta, -100, 100);
  m->attitude = CLAMP(m->attitude - annoyance_delta, -100, 100);
  m->fear = CLAMP(m->fear + fear_delta, 0, 100);
  m->last_seen_time = now;
  m->last_interaction_time = now;
}

enum npc_priority npc_ai_choose_priority(struct char_data *ch, const struct npc_social_profile *profile, time_t now)
{
  (void)now;
  if (!ch || !profile) return NPC_PRIO_IDLE;
  if (FIGHTING(ch)) return NPC_PRIO_ENGAGE;
  if (profile->role == NPC_ROLE_MERCHANT || profile->role == NPC_ROLE_TRAINER || profile->role == NPC_ROLE_HEALER)
    return NPC_PRIO_WORK;
  return NPC_PRIO_OBSERVE;
}

void npc_ai_handle_player_enter(struct char_data *ch, struct char_data *player, time_t now)
{
  struct npc_social_profile p;
  struct ai_actor_memory_entry *m;
  int repeat = 0;
  if (!npc_ai_is_humanoid_social_candidate(ch) || !player || IS_NPC(player)) return;
  npc_ai_build_profile(ch, &p);
  m = npc_mem_get(ch, GET_IDNUM(player));
  if (m && m->last_seen_time > 0 && (now - m->last_seen_time) < (12 * 3600)) repeat = 1;
  if (ch->ai_state && (now - ch->ai_state->last_spoke) < ((p.social_style == NPC_SOCIAL_EXTROVERT) ? 8 : 15)) return;
  npc_say(ch, npc_ai_get_dialogue_line(&p, NPC_PRIO_SOCIALIZE, repeat));
  if (ch->ai_state) ch->ai_state->last_spoke = now;
  npc_ai_update_memory(ch, player, 1, 0, 0, now);
}

void npc_ai_handle_player_leave(struct char_data *ch, struct char_data *player, time_t now)
{
  (void)now;
  if (!npc_ai_is_humanoid_social_candidate(ch) || !player || IS_NPC(player)) return;
  npc_ai_update_memory(ch, player, 0, 0, 0, now);
}

void npc_ai_handle_speech_event(struct char_data *ch, struct char_data *player, const char *text, time_t now)
{
  struct npc_social_profile p;
  enum npc_priority prio = NPC_PRIO_OBSERVE;
  if (!npc_ai_is_humanoid_social_candidate(ch) || !player || IS_NPC(player)) return;
  npc_ai_build_profile(ch, &p);
  if (text && (has_kw(text, "idiot") || has_kw(text, "stupid") || has_kw(text, "hate"))) {
    prio = NPC_PRIO_WARN;
    npc_ai_update_memory(ch, player, -1, 8, 0, now);
  } else if (text && (has_kw(text, "help") || has_kw(text, "quest") || has_kw(text, "buy") || has_kw(text, "trade"))) {
    prio = NPC_PRIO_WORK;
    npc_ai_update_memory(ch, player, 2, 0, 0, now);
  } else {
    npc_ai_update_memory(ch, player, 1, 0, 0, now);
  }
  if (ch->ai_state && (now - ch->ai_state->last_spoke) < 4) return;
  npc_say(ch, npc_ai_get_dialogue_line(&p, prio, FALSE));
  if (ch->ai_state) ch->ai_state->last_spoke = now;
}

void npc_ai_handle_room_danger(struct char_data *ch, struct char_data *actor, time_t now)
{
  struct npc_social_profile p;
  if (!npc_ai_is_humanoid_social_candidate(ch)) return;
  npc_ai_build_profile(ch, &p);
  if (p.role == NPC_ROLE_GUARD)
    npc_say(ch, "Guards, here!");
  else if (p.role == NPC_ROLE_MERCHANT)
    npc_say(ch, "Take the fighting outside!");
  else if (p.role == NPC_ROLE_HEALER)
    npc_say(ch, "This is a place of healing, not murder.");
  else if (p.role == NPC_ROLE_CIVILIAN)
    npc_say(ch, "Guards! Someone call the guards!");
  else
    npc_say(ch, "Is it over?");
  if (actor && !IS_NPC(actor))
    npc_ai_update_memory(ch, actor, -3, 5, 15, now);
  if (ch->ai_state) ch->ai_state->last_spoke = now;
}

void npc_ai_maybe_do_ambient_action(struct char_data *ch, const struct npc_social_profile *profile, time_t now)
{
  int emote_cd;
  if (!ch || !profile || !ch->ai_state) return;
  emote_cd = (profile->social_style == NPC_SOCIAL_EXTROVERT) ? 15 : (profile->social_style == NPC_SOCIAL_INTROVERT ? 45 : 25);
  if ((now - ch->ai_state->last_emote_time) < emote_cd) return;
  npc_ai_do_emote(ch, profile, now);
  ch->ai_state->last_emote_time = now;
}
