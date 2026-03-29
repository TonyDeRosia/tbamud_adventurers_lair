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

static long g_mob_behavior_pulse = 0;

static struct char_data *mob_behavior_pick_target(struct char_data *mob, const struct mob_combat_ability *ab, struct char_data *event_actor);
static int mob_behavior_schedule_random_round(struct char_data *mob, int idx, const struct mob_combat_ability *ab);
static int mob_behavior_can_use_combat_ability(struct char_data *mob, int idx, struct char_data **out_target);
static int mob_behavior_use_combat_ability(struct char_data *mob, int idx, struct char_data *target);
static int mob_behavior_execute_reaction(struct char_data *mob, const struct mob_event_reaction *ev, struct char_data *actor);

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
  return (skillnum == SKILL_BASH || skillnum == SKILL_KICK);
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
  return mob->mob_behavior_random_round[idx];
}

static int mob_behavior_target_has_effect(struct char_data *target, int ability_vnum)
{
  if (!target || ability_vnum <= 0)
    return 0;
  return affected_by_spell(target, ability_vnum);
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

static int mob_behavior_can_use_combat_ability(struct char_data *mob, int idx, struct char_data **out_target)
{
  struct mob_combat_ability *ab;
  struct char_data *target;

  if (!mob || !IS_NPC(mob) || !FIGHTING(mob))
    return 0;
  if (idx < 0 || idx >= mob->mob_specials.combat_ability_count)
    return 0;

  ab = &mob->mob_specials.combat_abilities[idx];
  if (!ab->enabled)
    return 0;
  if (ab->ability_vnum <= 0)
    return 0;
  if (ab->ability_type == MOB_ABILITY_SKILL && !mob_behavior_validate_skill(ab->ability_vnum))
    return 0;

  if (ab->once_per_fight && mob->mob_behavior_uses[idx] > 0)
    return 0;
  if (ab->max_uses_per_fight > 0 && mob->mob_behavior_uses[idx] >= ab->max_uses_per_fight)
    return 0;
  if (ab->cooldown_rounds > 0 &&
      mob->mob_behavior_fight_round - mob->mob_behavior_last_used_round[idx] < ab->cooldown_rounds)
    return 0;

  switch (ab->trigger_mode) {
    case MOB_TRIGGER_OPENER:
      if (mob->mob_behavior_opener_attempted[idx])
        return 0;
      if (mob->mob_behavior_fight_round != 1)
        return 0;
      break;
    case MOB_TRIGGER_RANDOM_ROUND_WINDOW:
      if (mob->mob_behavior_random_spent[idx])
        return 0;
      if (mob->mob_behavior_random_round[idx] <= 0)
        mob_behavior_schedule_random_round(mob, idx, ab);
      if (mob->mob_behavior_fight_round != mob->mob_behavior_random_round[idx])
        return 0;
      break;
    case MOB_TRIGGER_SELF_HP_THRESHOLD:
      if (GET_MAX_HIT(mob) <= 0)
        return 0;
      if (((GET_HIT(mob) * 100) / GET_MAX_HIT(mob)) > MAX(0, ab->self_hp_pct_max))
        return 0;
      break;
    case MOB_TRIGGER_TARGET_HP_THRESHOLD:
      if (!FIGHTING(mob) || GET_MAX_HIT(FIGHTING(mob)) <= 0)
        return 0;
      if (((GET_HIT(FIGHTING(mob)) * 100) / GET_MAX_HIT(FIGHTING(mob))) > MAX(0, ab->target_hp_pct_max))
        return 0;
      break;
    case MOB_TRIGGER_COOLDOWN:
    default:
      break;
  }

  target = mob_behavior_pick_target(mob, ab, NULL);
  if (!target)
    return 0;

  if (ab->require_target_not_affected && mob_behavior_target_has_effect(target, ab->ability_vnum))
    return 0;
  if (ab->require_self_not_affected && mob_behavior_target_has_effect(mob, ab->ability_vnum))
    return 0;

  if (out_target)
    *out_target = target;
  return 1;
}

static int mob_behavior_use_combat_ability(struct char_data *mob, int idx, struct char_data *target)
{
  struct mob_combat_ability *ab = &mob->mob_specials.combat_abilities[idx];
  char arg[MAX_INPUT_LENGTH];
  int attempted = 0;

  if (ab->trigger_mode == MOB_TRIGGER_OPENER)
    mob->mob_behavior_opener_attempted[idx] = 1;

  if (ab->ability_type == MOB_ABILITY_SPELL) {
    if (cast_spell(mob, target, NULL, ab->ability_vnum))
      attempted = 1;
  } else if (ab->ability_type == MOB_ABILITY_SKILL) {
    if (!target)
      return 0;
    snprintf(arg, sizeof(arg), "%s", GET_NAME(target));
    if (ab->ability_vnum == SKILL_BASH) {
      do_bash(mob, arg, 0, 0);
      attempted = 1;
    } else if (ab->ability_vnum == SKILL_KICK) {
      do_kick(mob, arg, 0, 0);
      attempted = 1;
    }
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

  for (i = 0; i < mob->mob_specials.combat_ability_count && i < MAX_MOB_COMBAT_ABILITIES; i++) {
    struct char_data *tmp_target = NULL;
    struct mob_combat_ability *ab = &mob->mob_specials.combat_abilities[i];
    if (!mob_behavior_can_use_combat_ability(mob, i, &tmp_target))
      continue;
    if (ab->priority < best_priority) {
      best_priority = ab->priority;
      best = i;
      target = tmp_target;
    }
  }

  if (best >= 0)
    mob_behavior_use_combat_ability(mob, best, target);

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
      return cast_spell(mob, target, NULL, ev->ability_vnum) ? 1 : 0;
    case MOB_EVENT_ACTION_USE_SKILL:
      if (!target)
        return 0;
      if (!mob_behavior_validate_skill(ev->ability_vnum))
        return 0;
      snprintf(arg, sizeof(arg), "%s", GET_NAME(target));
      if (ev->ability_vnum == SKILL_BASH)
        do_bash(mob, arg, 0, 0);
      else if (ev->ability_vnum == SKILL_KICK)
        do_kick(mob, arg, 0, 0);
      else
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
    if (ev->once_per_reset && mob->mob_behavior_event_used_this_reset[i])
      continue;
    if (mob->mob_behavior_event_cooldown_until[i] > (int)g_mob_behavior_pulse)
      continue;
    if (ev->chance_percent < 100 && rand_number(1, 100) > MAX(0, ev->chance_percent))
      continue;

    if (event_type == MOB_EVENT_PLAYER_ENTERS_ROOM) {
      if (!actor || IS_NPC(actor))
        continue;
      if (mob->mob_behavior_last_entry_actor_id == GET_IDNUM(actor))
        continue;
      mob->mob_behavior_last_entry_actor_id = GET_IDNUM(actor);
    }

    if (event_type == MOB_EVENT_LOW_HP) {
      int hp_pct;
      if (GET_MAX_HIT(mob) <= 0)
        continue;
      hp_pct = (GET_HIT(mob) * 100) / GET_MAX_HIT(mob);
      if (hp_pct > MAX(1, ev->hp_pct_threshold))
        continue;
    }

    if (!mob_behavior_execute_reaction(mob, ev, actor))
      continue;

    mob->mob_behavior_event_used_this_reset[i] = 1;
    mob->mob_behavior_event_cooldown_until[i] = (int)g_mob_behavior_pulse + MAX(0, ev->cooldown_pulses);
  }
}
