/* Possession uses switch's NPC transfer, plus explicit PC observation.
 * The target PC always keeps its own descriptor. No account state moves.
 * Sessions own their DG expiry event; end_control_session owns all teardown. */
#include "conf.h"
#include "sysdep.h"
#include "structs.h"
#include "utils.h"
#include "comm.h"
#include "interpreter.h"
#include "handler.h"
#include "db.h"
#include "spells.h"
#include "fight.h"
#include "act.h"
#include "house.h"
#include "constants.h"
#include "dg_event.h"
#include "control.h"

static struct control_session *sessions;

static void clear_control_input(struct descriptor_data *d)
{
  struct txt_block *entry;
  if (!d) return;
  while ((entry = d->input.head) != NULL) {
    d->input.head = entry->next;
    free(entry->text);
    free(entry);
  }
  d->input.tail = NULL;
  /* Do not replay a partial command against the restored body. */
  *d->inbuf = '\0';
}

static int body_available(struct char_data *ch)
{
  struct descriptor_data *d;
  if (!ch || ch->control || DEAD(ch) || IN_ROOM(ch) == NOWHERE ||
      GET_POS(ch) <= POS_DEAD || AFF_FLAGGED(ch, AFF_CHARM)) return FALSE;
  for (d = descriptor_list; d; d = d->next)
    if (d->original == ch) return FALSE;
  return TRUE;
}

static EVENTFUNC(control_expired)
{
  struct control_session *s = event_obj;
  s->expiry = NULL; /* event_process owns the firing event itself. */
  end_control_session(s);
  return 0;
}

int start_control_session(struct char_data *ch, struct char_data *victim,
    enum control_mode mode, int seconds)
{
  struct control_session *s;
  if (mode < CONTROL_IMMORTAL_PUPPET || mode > CONTROL_MIND_CONTROL ||
      !body_available(ch) || !body_available(victim) || ch == victim ||
      IS_NPC(ch) || !ch->desc || ch->desc->original ||
      STATE(ch->desc) != CON_PLAYING || ch->desc->str || ch->desc->showstr_count ||
      ch->desc->character != ch || ch->master || victim->master ||
      victim->followers) return FALSE;
  if (IS_NPC(victim)) {
    if (victim->desc) return FALSE;
  } else if (!victim->desc || STATE(victim->desc) != CON_PLAYING ||
      victim->desc->character != victim || victim->desc->original ||
      victim->desc->str || victim->desc->showstr_count) return FALSE;
  if (mode == CONTROL_IMMORTAL_PUPPET &&
      (GET_LEVEL(ch) < LVL_IMMORT || !IS_NPC(victim))) return FALSE;
  if (mode != CONTROL_IMMORTAL_PUPPET && (seconds < 1 ||
      seconds > (!IS_NPC(victim) ? 30 : mode == CONTROL_MIND_CONTROL ? 90 : 60)))
    return FALSE;

  CREATE(s, struct control_session, 1);
  s->mode = mode;
  s->controller = ch;
  s->target = victim;
  s->driver = ch->desc;
  s->observer = victim->desc;
  s->ready_at = pulse + MAX(0, GET_WAIT_STATE(victim));
  s->next = sessions;
  sessions = s;
  ch->control = victim->control = s;
  clear_control_input(s->driver);
  clear_control_input(s->observer);
  if (mode != CONTROL_MIND_CONTROL) {
    if (IS_NPC(victim)) switch_to_char(ch, victim);
    else {
      /* Two descriptors view the target; its owner remains target->desc. */
      s->driver->original = ch;
      s->driver->character = victim;
      ch->desc = NULL;
    }
    if (FIGHTING(ch)) stop_fighting(ch);
    write_to_output(s->driver, "Your consciousness enters another body.\r\n");
    look_at_room(victim, 0);
  } else
    send_to_char(ch, "You bind %s's will.\r\n", GET_NAME(victim));
  send_to_char(victim, "Your will is bound by another.\r\n");
  if (mode != CONTROL_IMMORTAL_PUPPET)
    s->expiry = event_create(control_expired, s, seconds * PASSES_PER_SEC);
  return TRUE;
}

void end_control_session(struct control_session *s)
{
  struct control_session **link;
  if (!s || s->ended) return;
  s->ended = TRUE;
  if (s->expiry) {
    /* event_cancel normally frees event_obj; this subsystem owns it. */
    s->expiry->event_obj = NULL;
    event_cancel(s->expiry);
    s->expiry = NULL;
  }
  for (link = &sessions; *link && *link != s; link = &(*link)->next) { }
  if (*link) *link = s->next;
  s->controller->control = NULL;
  s->target->control = NULL;
  if (s->mode != CONTROL_MIND_CONTROL) {
    s->driver->character = s->controller;
    s->driver->original = NULL;
    s->controller->desc = s->driver;
    s->target->desc = s->observer;
  }
  clear_control_input(s->driver);
  clear_control_input(s->observer);
  write_to_output(s->driver, s->mode == CONTROL_MIND_CONTROL ?
      "Your hold over the other mind fades.\r\n" :
      "Your consciousness snaps back into your own body.\r\n");
  send_to_char(s->target, "Your will is your own again.\r\n");
  /* Room display is done only on voluntary return, never while extracting. */
  if (!s->executing) free(s);
}

void end_character_control(struct char_data *ch)
{
  if (ch) end_control_session(ch->control);
}

void end_descriptor_control(struct descriptor_data *d)
{
  if (!d) return;
  end_character_control(d->character);
  end_character_control(d->original);
}

void end_all_control(void)
{
  while (sessions) end_control_session(sessions);
}

int control_body_abandoned(struct char_data *ch)
{
  return ch && ch->control && ch->control->controller == ch &&
      ch->control->mode != CONTROL_MIND_CONTROL;
}

struct descriptor_data *control_output_peer(struct descriptor_data *d)
{
  struct control_session *s;
  if (!d || !d->character || !(s = d->character->control)) return NULL;
  if (s->mode == CONTROL_SPELL_PUPPET && s->observer == d &&
      d->character == s->target && s->target->desc == d && STATE(d) == CON_PLAYING &&
      s->driver != d && s->driver->character == s->target &&
      s->driver->original == s->controller && STATE(s->driver) == CON_PLAYING)
    return s->driver;
  return NULL;
}

/* Resolve abbreviations exactly as the normal interpreter, before any DG
 * trigger or special can turn a forbidden verb into a destructive action. */
int control_command_allowed(struct char_data *ch, const char *verb)
{
  static const char *safe[] = {
    "north", "south", "east", "west", "up", "down", "northeast", "northwest",
    "southeast", "southwest", "ne", "nw", "se", "sw", "look", "scan", "exits", "say", "'", "emote", ":",
    "hit", "kill", "kick", "bash", "flee", "stand", "sit", "rest", "score", NULL
  };
  struct control_session *s = ch->control;
  int cmd, i, pass;
  size_t len = strlen(verb);
  if (!s) return TRUE;
  if (s->controller == ch) return s->mode == CONTROL_MIND_CONTROL;
  if (!s->executing || !len) return FALSE;
  for (pass = 0; pass < 2; pass++) {
    for (cmd = 0; *complete_cmd_info[cmd].command != '\n'; cmd++) {
      const struct command_info *info = &complete_cmd_info[cmd];
      if ((info->command_pointer == do_action) != pass ||
          strncmp(info->command, verb, len) || GET_LEVEL(ch) < info->minimum_level)
        continue;
      if (info->minimum_level < 0 || info->minimum_level >= LVL_IMMORT) return FALSE;
      if (s->mode == CONTROL_IMMORTAL_PUPPET) {
        return info->command_pointer != do_switch && info->command_pointer != do_return &&
            info->command_pointer != do_puppet && info->command_pointer != do_socket;
      }
      if (info->command_pointer == do_action) return TRUE;
      for (i = 0; safe[i]; i++)
        if (!strcmp(info->command, safe[i])) return TRUE;
      return FALSE;
    }
  }
  return FALSE;
}

static void controlled_command(struct control_session *s, char *input)
{
  s->executing++;
  command_interpreter(s->target, input);
  if (!s->ended && s->mode == CONTROL_MIND_CONTROL)
    s->ready_at = pulse + MAX(PULSE_VIOLENCE, GET_WAIT_STATE(s->target));
  if (--s->executing == 0 && s->ended) free(s);
}

int control_input(struct descriptor_data *d, char *input)
{
  struct control_session *s;
  char verb[MAX_INPUT_LENGTH];
  if (!d->character || !(s = d->character->control)) return FALSE;
  if (STATE(d) != CON_PLAYING) { end_control_session(s); return TRUE; }
  if (d == s->observer) {
    write_to_output(d, "Your will is bound by another.\r\n");
    return TRUE;
  }
  if (d != s->driver || s->mode == CONTROL_MIND_CONTROL) return FALSE;
  one_argument(input, verb);
  if (!strcmp(verb, "unpuppet") || !strcmp(verb, "return")) {
    end_control_session(s);
    if (IN_ROOM(d->character) != NOWHERE) look_at_room(d->character, 0);
    return TRUE;
  }
  if (STATE(d) != CON_PLAYING || DEAD(s->target) || DEAD(s->controller)) {
    end_control_session(s);
    return TRUE;
  }
  if (PLR_FLAGGED(s->controller, PLR_FROZEN) && GET_LEVEL(s->controller) < LVL_IMPL) {
    write_to_output(d, "The mind-numbing cold prevents you from acting.\r\n");
    return TRUE;
  }
  controlled_command(s, input);
  return TRUE;
}

int control_order(struct char_data *ch, char *argument)
{
  struct control_session *s = ch->control;
  struct char_data *victim;
  char name[MAX_INPUT_LENGTH], command[MAX_INPUT_LENGTH];
  if (!s || s->controller != ch || s->mode != CONTROL_MIND_CONTROL) return FALSE;
  half_chop(argument, name, command);
  if (!*name || !*command || !(victim = get_char_vis(ch, name, NULL, FIND_CHAR_ROOM)) ||
      victim != s->target) {
    send_to_char(ch, "Order your bound target here to do what?\r\n");
    return TRUE;
  }
  if (pulse < s->ready_at || (!IS_NPC(victim) && GET_WAIT_STATE(victim) > 0) || GET_WAIT_STATE(ch) > 1) {
    send_to_char(ch, "You must wait before compelling another action.\r\n");
    return TRUE;
  }
  WAIT_STATE(ch, PULSE_VIOLENCE);
  controlled_command(s, command);
  return TRUE;
}

int control_attack_allowed(struct char_data *ch, struct char_data *victim)
{
  struct control_session *s = ch ? ch->control : NULL;
  if (!s || s->mode == CONTROL_IMMORTAL_PUPPET || s->target != ch) return TRUE;
  /* Poison and other environmental effects use self-damage. */
  if (victim == ch) return TRUE;
  if (!victim || victim == s->controller ||
      (!IS_NPC(victim) && (!CONFIG_PK_ALLOWED || GET_LEVEL(victim) >= LVL_IMMORT)) ||
      MOB_FLAGGED(victim, MOB_NOKILL) || IN_ROOM(ch) == NOWHERE ||
      ROOM_FLAGGED(IN_ROOM(ch), ROOM_PEACEFUL)) return FALSE;
  return TRUE;
}

void cast_control(struct char_data *ch, struct char_data *victim, int spellnum)
{
  int skill, chance, seconds;
  enum control_mode mode = spellnum == SPELL_PUPPET ? CONTROL_SPELL_PUPPET : CONTROL_MIND_CONTROL;
  if (!ch || !victim || IS_NPC(ch) || !body_available(ch) || !body_available(victim) ||
      ch == victim || IN_ROOM(ch) != IN_ROOM(victim) ||
      ROOM_FLAGGED(IN_ROOM(ch), ROOM_PEACEFUL) ||
      AFF_FLAGGED(victim, AFF_SANCTUARY) ||
      GET_LEVEL(victim) >= LVL_IMMORT || GET_LEVEL(victim) > GET_LEVEL(ch) + 5 ||
      MOB_FLAGGED(victim, MOB_NOCHARM) || MOB_FLAGGED(victim, MOB_NOKILL) ||
      MOB_FLAGGED(victim, MOB_SPEC) || GET_MOB_SPEC(victim) ||
      (!IS_NPC(victim) && (!CONFIG_PK_ALLOWED || !PRF_FLAGGED(victim, PRF_SUMMONABLE))) ||
      ch->master || victim->master || victim->followers) {
    if (ch) send_to_char(ch, "That mind cannot be bound.\r\n");
    return;
  }
  skill = GET_LEVEL(ch) >= LVL_IMMORT ? 100 : MIN(100, MAX(0, GET_SKILL(ch, spellnum)));
  if (!skill) { send_to_char(ch, "You do not know that spell.\r\n"); return; }
  chance = MIN(85, MAX(5, skill / 2 + 20 + 2 * (GET_LEVEL(ch) - GET_LEVEL(victim)) +
      GET_INT(ch) - GET_WIS(victim) - (!IS_NPC(victim) ? 15 : 0)));
  if (mag_savingthrow(victim, SAVING_PARA, 0) || rand_number(1, 100) > chance) {
    send_to_char(ch, "Your target resists your grasp.\r\n");
    send_to_char(victim, "You resist an attempt to bind your will.\r\n");
    if (!FIGHTING(victim)) set_fighting(victim, ch);
    if (!FIGHTING(ch)) set_fighting(ch, victim);
    return;
  }
  seconds = !IS_NPC(victim) ? 3 + 27 * skill / 100 :
      mode == CONTROL_MIND_CONTROL ? 15 + 75 * skill / 100 : 10 + 50 * skill / 100;
  if (!start_control_session(ch, victim, mode, seconds))
    send_to_char(ch, "That mind cannot be bound just now.\r\n");
  else if (!FIGHTING(victim))
    set_fighting(victim, ch);
}

ACMD(do_puppet)
{
  char arg[MAX_INPUT_LENGTH];
  struct char_data *victim;
  if (IS_NPC(ch) || GET_LEVEL(ch) < LVL_IMMORT || !ch->desc) return;
  one_argument(argument, arg);
  if (!*arg) { send_to_char(ch, "Puppet which creature?\r\n"); return; }
  victim = get_char_vis(ch, arg, NULL, FIND_CHAR_WORLD);
  if (!victim || !IS_NPC(victim) || IN_ROOM(victim) == NOWHERE ||
      (GET_LEVEL(ch) < LVL_GRGOD && (ROOM_FLAGGED(IN_ROOM(victim), ROOM_GODROOM) ||
      (ROOM_FLAGGED(IN_ROOM(victim), ROOM_HOUSE) && !House_can_enter(ch, GET_ROOM_VNUM(IN_ROOM(victim)))))) ||
      !start_control_session(ch, victim, CONTROL_IMMORTAL_PUPPET, 0))
    send_to_char(ch, "You cannot puppet that creature. End any current control first.\r\n");
}

ACMD(do_unpuppet)
{
  struct control_session *s = ch->control;
  struct char_data *body;
  if (!s || (s->controller != ch && !s->executing)) {
    send_to_char(ch, "You are not puppeting anyone.\r\n"); return;
  }
  body = s->controller;
  end_control_session(s);
  if (IN_ROOM(body) != NOWHERE) look_at_room(body, 0);
}

ACMD(do_socket)
{
  struct descriptor_data *d;
  char target[MAX_INPUT_LENGTH], option[MAX_INPUT_LENGTH];
  int full, count = 0;
  char state[64];
  if (IS_NPC(ch) || GET_LEVEL(ch) < LVL_IMPL || ch->control || !ch->desc || ch->desc->original) {
    send_to_char(ch, "Only an Implementor in their own body may inspect connections.\r\n"); return;
  }
  two_arguments(argument, target, option);
  full = !str_cmp(target, "full") || !str_cmp(option, "full");
  if (!str_cmp(target, "full")) *target = '\0';
  if (*option && str_cmp(option, "full")) {
    send_to_char(ch, "Usage: socket [player|descriptor-id] [full]\r\n"); return;
  }
  for (d = descriptor_list; d; d = d->next) {
    struct char_data *body = d->character, *owner = d->original ? d->original : body;
    struct control_session *s = body ? body->control : NULL;
    if (*target && !(is_number(target) && atoi(target) == d->desc_num) &&
        !(owner && isname(target, GET_NAME(owner)))) continue;
    sprinttype(STATE(d), connected_types, state, sizeof(state));
    send_to_char(ch, "ID %d  Character: %s  State: %s  Host: %s  Room: %d\r\n",
        d->desc_num, owner ? GET_NAME(owner) : "(login)", state,
        full && *d->host ? d->host : "[redacted]",
        body && IN_ROOM(body) != NOWHERE ? GET_ROOM_VNUM(IN_ROOM(body)) : NOWHERE);
    send_to_char(ch, "  Account: %s  Connected: %ld seconds  Idle: %d mud ticks\r\n",
        *d->acct_name ? d->acct_name : "(none)", (long)MAX(0, time(NULL) - d->login_time),
        body ? body->char_specials.timer : 0);
    if (s)
      send_to_char(ch, "  Controller: %s  Controlled body: %s  Original body: %s\r\n"
          "  Control mode: %s  Role: %s\r\n", GET_NAME(s->controller), GET_NAME(s->target),
          GET_NAME(s->controller), s->mode == CONTROL_MIND_CONTROL ? "Mind Control" :
          s->mode == CONTROL_IMMORTAL_PUPPET ? "Immortal Puppet" : "Puppet",
          d == s->observer ? "target observer" : "controller");
    else if (d->original)
      send_to_char(ch, "  Control mode: Switch  Controlled body: %s\r\n", body ? GET_NAME(body) : "(none)");
    count++;
  }
  send_to_char(ch, "%d connections.\r\n", count);
}
