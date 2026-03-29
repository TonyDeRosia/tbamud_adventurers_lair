#include "conf.h"
#include "sysdep.h"

#include "mob_behavior.h"
#include "utils.h"
#include "comm.h"
#include "db.h"
#include "handler.h"
#include "fight.h"
#include "spells.h"
#include "act.h"
#include "interpreter.h"
#include <stdarg.h>

static long g_mob_behavior_pulse = 0;

static struct char_data *mob_behavior_pick_target(struct char_data *mob, const struct mob_combat_ability *ab, struct char_data *event_actor);
static int mob_behavior_schedule_random_round(struct char_data *mob, int idx, const struct mob_combat_ability *ab);
static int mob_behavior_can_use_combat_ability(struct char_data *mob, int idx, struct char_data **out_target, const char **reason_out);
static int mob_behavior_use_combat_ability(struct char_data *mob, int idx, struct char_data *target);
static int mob_behavior_execute_reaction(struct char_data *mob, const struct mob_event_reaction *ev, struct char_data *actor);
static void mob_behavior_debug_notify(struct char_data *mob, const char *fmt, ...);
static int mob_behavior_use_skill(struct char_data *mob, struct char_data *target, int skillnum);
static int mob_behavior_skill_has_specific_message(int skillnum);

void mob_behavior_advance_pulse(void)
{
  g_mob_behavior_pulse++;
}

const char *mob_behavior_ability_type_name(int type)
{
  switch (type) {
    case MOB_ABILITY_SPELL: return "spell";
    case MOB_ABILITY_SKILL: return "skill";
    default: return "none";
  }
}

const char *mob_behavior_target_name(int target)
{
  switch (target) {
    case MOB_TARGET_SELF: return "self";
    case MOB_TARGET_FIGHTING: return "fighting target";
    case MOB_TARGET_RANDOM_ENEMY: return "random enemy";
    case MOB_TARGET_ALLY: return "ally";
    default: return "unknown";
  }
}

const char *mob_behavior_trigger_mode_name(int mode)
{
  switch (mode) {
    case MOB_TRIGGER_OPENER: return "opener";
    case MOB_TRIGGER_COOLDOWN: return "cooldown";
    case MOB_TRIGGER_RANDOM_ROUND_WINDOW: return "random round window";
    case MOB_TRIGGER_SELF_HP_THRESHOLD: return "self hp threshold";
    case MOB_TRIGGER_TARGET_HP_THRESHOLD: return "target hp threshold";
    default: return "unknown";
  }
}

const char *mob_behavior_event_type_name(int type)
{
  switch (type) {
    case MOB_EVENT_PLAYER_ENTERS_ROOM: return "player enters room";
    case MOB_EVENT_COMBAT_STARTS: return "combat starts";
    case MOB_EVENT_LOW_HP: return "low hp";
    case MOB_EVENT_ATTACKED: return "attacked";
    case MOB_EVENT_DEATH: return "death";
    default: return "unknown";
  }
}

const char *mob_behavior_event_action_name(int type)
{
  switch (type) {
    case MOB_EVENT_ACTION_CAST_SPELL: return "cast spell";
    case MOB_EVENT_ACTION_USE_SKILL: return "use skill";
    case MOB_EVENT_ACTION_SAY_TEXT: return "say text";
    case MOB_EVENT_ACTION_EMOTE_TEXT: return "emote text";
    default: return "unknown";
  }
}

int mob_behavior_validate_skill(int skillnum)
{
  switch (skillnum) {
    case SKILL_BASH:
    case SKILL_KICK:
#ifdef SKILL_DISARM
    case SKILL_DISARM:
#endif
#ifdef SKILL_DIRT_KICK
    case SKILL_DIRT_KICK:
#endif
#ifdef SKILL_TRIP
    case SKILL_TRIP:
#endif
      return 1;
    default:
      return 0;
  }
}

void mob_behavior_reset_fight_state(struct char_data *mob)
{
  int i;
  if (!mob)
    return;

  mob->mob_behavior_in_combat = 0;
  mob->mob_behavior_fight_round = 0;
  mob->mob_behavior_round_marker = 0;

  for (i = 0; i < MAX_MOB_COMBAT_ABILITIES; i++) {
    mob->mob_behavior_uses[i] = 0;
    mob->mob_behavior_last_used_round[i] = -9999;
    mob->mob_behavior_random_round[i] = -1;
    mob->mob_behavior_random_spent[i] = 0;
    mob->mob_behavior_opener_attempted[i] = 0;
  }
}

void mob_behavior_on_combat_start(struct char_data *mob, struct char_data *opponent)
{
  int i;
  if (!mob || !IS_NPC(mob))
    return;

  mob_behavior_reset_fight_state(mob);
  mob->mob_behavior_in_combat = 1;
  mob_behavior_debug_notify(mob, "combat start vs %s", opponent ? GET_NAME(opponent) : "<none>");

  for (i = 0; i < mob->mob_specials.combat_ability_count && i < MAX_MOB_COMBAT_ABILITIES; i++) {
    const struct mob_combat_ability *ab = &mob->mob_specials.combat_abilities[i];
    if (ab->trigger_mode == MOB_TRIGGER_RANDOM_ROUND_WINDOW)
      mob_behavior_schedule_random_round(mob, i, ab);
  }

  mob_behavior_handle_event(mob, MOB_EVENT_COMBAT_STARTS, opponent);
}

void mob_behavior_on_combat_end(struct char_data *mob)
{
  mob_behavior_reset_fight_state(mob);
}

static int mob_behavior_schedule_random_round(struct char_data *mob, int idx, const struct mob_combat_ability *ab)
{
  int rmin = MAX(1, ab->round_min);
  int rmax = MAX(rmin, ab->round_max);
  mob->mob_behavior_random_round[idx] = rand_number(rmin, rmax);
  mob->mob_behavior_random_spent[idx] = 0;
  mob_behavior_debug_notify(mob, "combat start/cycle schedule: slot=%d round=%d range=[%d,%d]",
                            idx + 1, mob->mob_behavior_random_round[idx], rmin, rmax);
  return mob->mob_behavior_random_round[idx];
}

static int mob_behavior_target_has_effect(struct char_data *target, int ability_id)
{
  if (!target || ability_id <= 0)
    return 0;
  return affected_by_spell(target, ability_id);
}

static struct char_data *mob_behavior_pick_target(struct char_data *mob, const struct mob_combat_ability *ab, struct char_data *event_actor)
{
  struct char_data *tch, *list[128];
  int count = 0;

  if (!mob)
    return NULL;

  switch (ab ? ab->target_type : MOB_TARGET_SELF) {
    case MOB_TARGET_SELF:
      return mob;
    case MOB_TARGET_FIGHTING:
      return FIGHTING(mob);
    case MOB_TARGET_RANDOM_ENEMY:
      for (tch = world[IN_ROOM(mob)].people; tch; tch = tch->next_in_room)
        if (tch != mob && FIGHTING(tch) == mob && count < 128)
          list[count++] = tch;
      if (count == 0)
        return FIGHTING(mob);
      return list[rand_number(0, count - 1)];
    case MOB_TARGET_ALLY:
      for (tch = world[IN_ROOM(mob)].people; tch; tch = tch->next_in_room)
        if (IS_NPC(tch) && tch != mob && count < 128)
          list[count++] = tch;
      if (count == 0)
        return mob;
      return list[rand_number(0, count - 1)];
    default:
      return event_actor ? event_actor : FIGHTING(mob);
  }
}

static int mob_behavior_can_use_combat_ability(struct char_data *mob, int idx, struct char_data **out_target, const char **reason_out)
{
  struct mob_combat_ability *ab;
  struct char_data *target;
#define FAIL_REASON(msg) do { if (reason_out) *reason_out = (msg); return 0; } while (0)

  if (!mob || !IS_NPC(mob) || !FIGHTING(mob))
    FAIL_REASON("not in combat");
  if (idx < 0 || idx >= mob->mob_specials.combat_ability_count)
    FAIL_REASON("bad slot");

  ab = &mob->mob_specials.combat_abilities[idx];
  if (!ab->enabled)
    FAIL_REASON("disabled");
  if (ab->ability_id <= 0)
    FAIL_REASON("missing ability id");
  if (ab->ability_type == MOB_ABILITY_SKILL && !mob_behavior_validate_skill(ab->ability_id)) {
    log("SYSERR: Native mob behavior unsupported skill id %d on mob vnum %d.",
        ab->ability_id, GET_MOB_VNUM(mob));
    FAIL_REASON("unsupported skill");
  }

  if (ab->once_per_fight && mob->mob_behavior_uses[idx] > 0)
    FAIL_REASON("once_per_fight already used");
  if (ab->max_uses_per_fight > 0 && mob->mob_behavior_uses[idx] >= ab->max_uses_per_fight)
    FAIL_REASON("max uses reached");
  if (ab->cooldown_rounds > 0 &&
      mob->mob_behavior_fight_round - mob->mob_behavior_last_used_round[idx] < ab->cooldown_rounds)
    FAIL_REASON("cooldown");

  switch (ab->trigger_mode) {
    case MOB_TRIGGER_OPENER:
      if (mob->mob_behavior_opener_attempted[idx])
        FAIL_REASON("opener already attempted");
      if (mob->mob_behavior_fight_round != 1)
        FAIL_REASON("not opener round");
      break;
    case MOB_TRIGGER_RANDOM_ROUND_WINDOW:
      if (mob->mob_behavior_random_spent[idx])
        FAIL_REASON("random window already spent");
      if (mob->mob_behavior_random_round[idx] <= 0)
        mob_behavior_schedule_random_round(mob, idx, ab);
      if (mob->mob_behavior_fight_round != mob->mob_behavior_random_round[idx])
        FAIL_REASON("scheduled round not reached");
      break;
    case MOB_TRIGGER_SELF_HP_THRESHOLD:
      if (GET_MAX_HIT(mob) <= 0)
        FAIL_REASON("invalid self max hp");
      if (((GET_HIT(mob) * 100) / GET_MAX_HIT(mob)) > MAX(0, ab->self_hp_pct_max))
        FAIL_REASON("hp threshold not met");
      break;
    case MOB_TRIGGER_TARGET_HP_THRESHOLD:
      if (!FIGHTING(mob) || GET_MAX_HIT(FIGHTING(mob)) <= 0)
        FAIL_REASON("target missing");
      if (((GET_HIT(FIGHTING(mob)) * 100) / GET_MAX_HIT(FIGHTING(mob))) > MAX(0, ab->target_hp_pct_max))
        FAIL_REASON("target hp threshold not met");
      break;
    case MOB_TRIGGER_COOLDOWN:
    default:
      break;
  }

  target = mob_behavior_pick_target(mob, ab, NULL);
  if (!target)
    FAIL_REASON("target missing");

  if (ab->require_target_not_affected && mob_behavior_target_has_effect(target, ab->ability_id))
    FAIL_REASON("target already affected");
  if (ab->require_self_not_affected && mob_behavior_target_has_effect(mob, ab->ability_id))
    FAIL_REASON("self already affected");

  if (out_target)
    *out_target = target;
  if (reason_out)
    *reason_out = "eligible";
  return 1;
#undef FAIL_REASON
}

static int mob_behavior_use_combat_ability(struct char_data *mob, int idx, struct char_data *target)
{
  struct mob_combat_ability *ab = &mob->mob_specials.combat_abilities[idx];
  int attempted = 0;

  if (ab->trigger_mode == MOB_TRIGGER_OPENER)
    mob->mob_behavior_opener_attempted[idx] = 1;

  if (ab->ability_type == MOB_ABILITY_SPELL) {
    if (cast_spell(mob, target, NULL, ab->ability_id))
      attempted = 1;
  } else if (ab->ability_type == MOB_ABILITY_SKILL) {
    if (!target)
      return 0;
    attempted = mob_behavior_use_skill(mob, target, ab->ability_id);
  }

  if (ab->trigger_mode == MOB_TRIGGER_RANDOM_ROUND_WINDOW)
    mob->mob_behavior_random_spent[idx] = 1;

  if (attempted) {
    mob->mob_behavior_uses[idx]++;
    mob->mob_behavior_last_used_round[idx] = mob->mob_behavior_fight_round;

    if (ab->trigger_mode == MOB_TRIGGER_RANDOM_ROUND_WINDOW &&
        ab->max_uses_per_fight > 1 &&
        (ab->max_uses_per_fight == 0 || mob->mob_behavior_uses[idx] < ab->max_uses_per_fight)) {
      if (ab->cooldown_rounds <= 0 ||
          mob->mob_behavior_fight_round - mob->mob_behavior_last_used_round[idx] >= ab->cooldown_rounds)
        mob_behavior_schedule_random_round(mob, idx, ab);
    }
  }

  if (attempted)
    mob_behavior_debug_notify(mob, "ability used: slot=%d ability=%s target=%s",
                              idx + 1, skill_name(ab->ability_id), target ? GET_NAME(target) : "<none>");
  else
    mob_behavior_debug_notify(mob, "ability attempt failed: slot=%d ability=%s",
                              idx + 1, skill_name(ab->ability_id));

  return attempted;
}

void mob_behavior_eval_combat_round(struct char_data *mob)
{
  int i, best = -1, best_priority = 999999;
  struct char_data *target = NULL;

  if (!mob || !IS_NPC(mob) || !FIGHTING(mob))
    return;

  if (!mob->mob_behavior_in_combat)
    mob_behavior_on_combat_start(mob, FIGHTING(mob));

  mob->mob_behavior_fight_round++;
  mob_behavior_debug_notify(mob, "combat round=%d", mob->mob_behavior_fight_round);

  for (i = 0; i < mob->mob_specials.combat_ability_count && i < MAX_MOB_COMBAT_ABILITIES; i++) {
    struct char_data *tmp_target = NULL;
    const char *reason = "ineligible";
    struct mob_combat_ability *ab = &mob->mob_specials.combat_abilities[i];
    if (!mob_behavior_can_use_combat_ability(mob, i, &tmp_target, &reason)) {
      if (ab->trigger_mode == MOB_TRIGGER_RANDOM_ROUND_WINDOW &&
          !mob->mob_behavior_random_spent[i] &&
          mob->mob_behavior_random_round[i] > 0 &&
          mob->mob_behavior_fight_round == mob->mob_behavior_random_round[i]) {
        /* Random round window semantics:
         * - exactly one scheduled round is chosen per cycle;
         * - evaluation happens at that scheduled round once;
         * - if conditions fail on that exact round, the scheduled attempt is spent;
         * - no retries on later rounds in the same cycle.
         */
        mob->mob_behavior_random_spent[i] = 1;
        mob_behavior_debug_notify(mob, "random window spent: slot=%d scheduled=%d reached=%d reason=%s",
                                  i + 1, mob->mob_behavior_random_round[i], mob->mob_behavior_fight_round, reason);
      } else if (ab->trigger_mode == MOB_TRIGGER_RANDOM_ROUND_WINDOW) {
        mob_behavior_debug_notify(mob, "random window waiting: slot=%d scheduled=%d current=%d reason=%s",
                                  i + 1, mob->mob_behavior_random_round[i], mob->mob_behavior_fight_round, reason);
      }
      continue;
    }
    if (ab->priority < best_priority) {
      best_priority = ab->priority;
      best = i;
      target = tmp_target;
    }
  }

  if (best >= 0) {
    mob_behavior_debug_notify(mob, "selected slot=%d priority=%d (lower acts first)", best + 1, best_priority);
    mob_behavior_use_combat_ability(mob, best, target);
  }

  mob_behavior_handle_event(mob, MOB_EVENT_LOW_HP, FIGHTING(mob));
}

static int mob_behavior_execute_reaction(struct char_data *mob, const struct mob_event_reaction *ev, struct char_data *actor)
{
  struct char_data *target = actor ? actor : FIGHTING(mob);
  char arg[MAX_INPUT_LENGTH + 16];

  switch (ev->action_type) {
    case MOB_EVENT_ACTION_CAST_SPELL:
      if (!target)
        target = mob;
      return cast_spell(mob, target, NULL, ev->ability_id) ? 1 : 0;
    case MOB_EVENT_ACTION_USE_SKILL:
      if (!target)
        return 0;
      if (!mob_behavior_validate_skill(ev->ability_id)) {
        log("SYSERR: Native mob behavior unsupported reaction skill id %d on mob vnum %d.",
            ev->ability_id, GET_MOB_VNUM(mob));
        return 0;
      }
      if (!mob_behavior_use_skill(mob, target, ev->ability_id))
        return 0;
      return 1;
    case MOB_EVENT_ACTION_SAY_TEXT:
      if (!ev->argument[0])
        return 0;
      snprintf(arg, sizeof(arg), "say %s", ev->argument);
      command_interpreter(mob, arg);
      return 1;
    case MOB_EVENT_ACTION_EMOTE_TEXT:
      if (!ev->argument[0])
        return 0;
      snprintf(arg, sizeof(arg), "emote %s", ev->argument);
      command_interpreter(mob, arg);
      return 1;
    default:
      return 0;
  }
}

void mob_behavior_handle_event(struct char_data *mob, int event_type, struct char_data *actor)
{
  int i;

  if (!mob || !IS_NPC(mob))
    return;

  for (i = 0; i < mob->mob_specials.event_reaction_count && i < MAX_MOB_EVENT_REACTIONS; i++) {
    struct mob_event_reaction *ev = &mob->mob_specials.event_reactions[i];

    if (!ev->enabled || ev->event_type != event_type)
      continue;
    if (ev->once_per_reset && mob->mob_behavior_event_used_this_reset[i]) {
      mob_behavior_debug_notify(mob, "reaction suppressed: slot=%d reason=once_per_reset", i + 1);
      continue;
    }
    if (mob->mob_behavior_event_cooldown_until[i] > (int)g_mob_behavior_pulse) {
      mob_behavior_debug_notify(mob, "reaction suppressed: slot=%d reason=cooldown", i + 1);
      continue;
    }
    if (ev->chance_percent < 100 && rand_number(1, 100) > MAX(0, ev->chance_percent))
      continue;

    if (event_type == MOB_EVENT_PLAYER_ENTERS_ROOM) {
      if (!actor || IS_NPC(actor))
        continue;
      if (mob->mob_behavior_event_last_actor_id[i] == GET_IDNUM(actor) &&
          ev->cooldown_pulses > 0 &&
          ((int)g_mob_behavior_pulse - mob->mob_behavior_event_last_trigger_pulse[i]) < ev->cooldown_pulses) {
        mob_behavior_debug_notify(mob, "reaction suppressed: slot=%d reason=duplicate actor within cooldown", i + 1);
        continue;
      }
    }

    if (event_type == MOB_EVENT_LOW_HP) {
      int hp_pct;
      if (GET_MAX_HIT(mob) <= 0)
        continue;
      hp_pct = (GET_HIT(mob) * 100) / GET_MAX_HIT(mob);
      if (hp_pct > MAX(1, ev->hp_pct_threshold)) {
        mob_behavior_debug_notify(mob, "reaction suppressed: slot=%d reason=hp threshold not met", i + 1);
        continue;
      }
    }

    if (!mob_behavior_execute_reaction(mob, ev, actor)) {
      mob_behavior_debug_notify(mob, "reaction suppressed: slot=%d reason=execution failed", i + 1);
      continue;
    }

    mob->mob_behavior_event_used_this_reset[i] = 1;
    mob->mob_behavior_event_cooldown_until[i] = (int)g_mob_behavior_pulse + MAX(0, ev->cooldown_pulses);
    mob->mob_behavior_event_last_actor_id[i] = actor ? GET_IDNUM(actor) : 0;
    mob->mob_behavior_event_last_trigger_pulse[i] = (int)g_mob_behavior_pulse;
    mob_behavior_debug_notify(mob, "reaction fired: slot=%d event=%s action=%s",
                              i + 1, mob_behavior_event_type_name(ev->event_type), mob_behavior_event_action_name(ev->action_type));
  }
}

static int mob_behavior_use_skill(struct char_data *mob, struct char_data *target, int skillnum)
{
  char arg[MAX_INPUT_LENGTH];

  if (!mob || !target)
    return 0;

  snprintf(arg, sizeof(arg), "%s", GET_NAME(target));

  if (!mob_behavior_skill_has_specific_message(skillnum)) {
    if (target == mob) {
      act("$n uses $t on $mself!", FALSE, mob, (struct obj_data *)skill_name(skillnum), NULL, TO_ROOM);
    } else {
      act("$n uses $t on $N!", FALSE, mob, (struct obj_data *)skill_name(skillnum), target, TO_NOTVICT);
      act("$n uses $t on you!", FALSE, mob, (struct obj_data *)skill_name(skillnum), target, TO_VICT);
    }
  }

  switch (skillnum) {
    case SKILL_BASH: do_bash(mob, arg, 0, 0); return 1;
    case SKILL_KICK: do_kick(mob, arg, 0, 0); return 1;
#ifdef SKILL_DISARM
    case SKILL_DISARM: do_disarm(mob, arg, 0, 0); return 1;
#endif
#ifdef SKILL_DIRT_KICK
    case SKILL_DIRT_KICK: do_dirtkick(mob, arg, 0, 0); return 1;
#endif
#ifdef SKILL_TRIP
    case SKILL_TRIP: do_trip(mob, arg, 0, 0); return 1;
#endif
    default:
      return 0;
  }
}

static int mob_behavior_skill_has_specific_message(int skillnum)
{
  int i;

  for (i = 0; i < MAX_MESSAGES && fight_messages[i].a_type; i++) {
    if (fight_messages[i].a_type == skillnum && fight_messages[i].msg)
      return 1;
  }

  return 0;
}

static void mob_behavior_debug_notify(struct char_data *mob, const char *fmt, ...)
{
  struct descriptor_data *d;
  va_list args;
  char body[MAX_STRING_LENGTH];
  char out[MAX_STRING_LENGTH];

  if (!mob || !fmt)
    return;

  va_start(args, fmt);
  vsnprintf(body, sizeof(body), fmt, args);
  va_end(args);

  snprintf(out, sizeof(out), "[MBEH %d %s] %.4096s\r\n",
           GET_MOB_VNUM(mob), GET_NAME(mob), body);

  for (d = descriptor_list; d; d = d->next) {
    if (STATE(d) != CON_PLAYING || !d->character)
      continue;
    if (GET_LEVEL(d->character) < LVL_IMMORT)
      continue;
    if (!PRF_FLAGGED(d->character, PRF_LOG1) && !PRF_FLAGGED(d->character, PRF_LOG2))
      continue;
    send_to_char(d->character, "%s", out);
  }
}
