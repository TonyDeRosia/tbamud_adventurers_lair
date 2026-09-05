#include "conf.h"
#include "sysdep.h"
#include "structs.h"
#include "utils.h"
#include "comm.h"
#include "interpreter.h"
#include "handler.h"
#include "spells.h"
#include "db.h"
#include "tome.h"
#include "class.h"

int tome_valid_ability(int ability)
{
  return ability > 0 && ability <= TOP_SPELL_DEFINE && ability <= MAX_SKILLS &&
      ability != SPELL_DG_AFFECT && spell_info[ability].name && *spell_info[ability].name &&
      str_cmp(spell_info[ability].name, unused_spellname);
}

int has_tome_ability(const struct char_data *ch, int ability)
{
  return ch && !IS_NPC(ch) && ability > 0 && ability <= MAX_SKILLS && HAS_TOME_ABILITY(ch, ability);
}

int character_has_ability_access(const struct char_data *ch, int ability)
{
  if (!ch || !is_valid_class(GET_CLASS(ch)) || ability < 1 || ability > TOP_SPELL_DEFINE)
    return FALSE;
  if (GET_LEVEL(ch) >= LVL_IMMORT || has_tome_ability(ch, ability))
    return TRUE;
  return spell_info[ability].min_level[(int)GET_CLASS(ch)] < LVL_IMMORT;
}

int get_ability_class_affinity(const struct char_data *ch, int ability)
{
  if (!ch || !is_valid_class(GET_CLASS(ch)) || ability < 1 || ability > TOP_SPELL_DEFINE)
    return 0;
  return spell_info[ability].min_level[(int)GET_CLASS(ch)] < LVL_IMMORT ? 100 : TOME_DEFAULT_OFFCLASS_AFFINITY;
}

int tome_validate(const struct obj_data *obj, char *why, size_t whylen)
{
  int i, j, count = 0;
  if (!obj || GET_OBJ_TYPE(obj) != ITEM_TOME) {
    snprintf(why, whylen, "That is not a tome."); return FALSE;
  }
  for (i = 0; i < TOME_ABILITY_SLOTS; i++) {
    int ability = GET_OBJ_VAL(obj, i);
    if (!ability) continue;
    if (!tome_valid_ability(ability)) {
      snprintf(why, whylen, "Slot %d contains invalid ability ID %d.", i + 1, ability); return FALSE;
    }
    for (j = 0; j < i; j++) if (ability == GET_OBJ_VAL(obj, j)) {
      snprintf(why, whylen, "Slot %d duplicates %s.", i + 1, spell_info[ability].name); return FALSE;
    }
    count++;
  }
  if (!count) { snprintf(why, whylen, "This tome contains no abilities."); return FALSE; }
  if (obj->tome_cooldown_seconds < 0) { snprintf(why, whylen, "This tome has an invalid cooldown."); return FALSE; }
  return TRUE;
}

void tome_format_remaining(time_t until, char *buf, size_t buflen)
{
  time_t now = time(NULL), remain = until > now ? until - now : 0;
  long days = remain / 86400, hours = (remain % 86400) / 3600, minutes = (remain % 3600) / 60, seconds = remain % 60;
  if (days) snprintf(buf, buflen, "%ld days, %ld hours, and %ld minutes", days, hours, minutes);
  else if (hours) snprintf(buf, buflen, "%ld hours and %ld minutes", hours, minutes);
  else if (minutes) snprintf(buf, buflen, "%ld minutes and %ld seconds", minutes, seconds);
  else snprintf(buf, buflen, "%ld seconds", seconds);
}

/* Missing authorization is new learning even when a stale percentage survives.
 * Native known abilities and existing Tome grants still provide nothing new. */
static int tome_can_grant_ability(const struct char_data *ch, int ability)
{
  return ability && !has_tome_ability(ch, ability) &&
      (GET_SKILL(ch, ability) <= 0 || !character_has_ability_access(ch, ability));
}

int tome_study_object(struct char_data *ch, struct obj_data *obj)
{
  int i, learned = 0;
  char why[MAX_INPUT_LENGTH], remaining[128];
  time_t now = time(NULL);
  if (IS_NPC(ch)) { send_to_char(ch, "Only players may study tomes.\r\n"); return FALSE; }
  if (!is_valid_class(GET_CLASS(ch))) { send_to_char(ch, "Your class requires administrative correction.\r\n"); return FALSE; }
  if (!tome_validate(obj, why, sizeof(why))) { send_to_char(ch, "%s\r\n", why); return FALSE; }
  if (GET_LEVEL(ch) < LVL_IMMORT && GET_TOME_STUDY_EXPIRES_AT(ch) > now) {
    tome_format_remaining(GET_TOME_STUDY_EXPIRES_AT(ch), remaining, sizeof(remaining));
    send_to_char(ch, "Your mind is still strained from your previous studies.\r\nYou may study another tome in %s.\r\n", remaining); return FALSE;
  }
  for (i = 0; i < TOME_ABILITY_SLOTS; i++) if (tome_can_grant_ability(ch, GET_OBJ_VAL(obj, i))) learned++;
  if (!learned) { send_to_char(ch, "You find nothing within this tome that you have not already mastered.\r\n"); return FALSE; }
  send_to_char(ch, "You study %s and absorb its lore:\r\n", obj->short_description);
  for (i = 0; i < TOME_ABILITY_SLOTS; i++) {
    int ability = GET_OBJ_VAL(obj, i);
    if (!tome_can_grant_ability(ch, ability)) continue;
    SET_TOME_ABILITY(ch, ability);
    if (GET_SKILL(ch, ability) <= 0) SET_SKILL(ch, ability, 1);
    send_to_char(ch, "  %s [%s] at %d%%\r\n", spell_info[ability].name, ability <= MAX_SPELLS ? "Spell" : "Skill", GET_SKILL(ch, ability));
  }
  GET_TOME_STUDY_EXPIRES_AT(ch) = now + obj->tome_cooldown_seconds;
  extract_obj(obj);
  save_char(ch);
  return TRUE;
}

ACMD(do_tome)
{
  char arg[MAX_INPUT_LENGTH], remaining[128]; int i, any = FALSE;
  one_argument(argument, arg);
  if (!*arg || !str_cmp(arg, "status")) {
    if (GET_TOME_STUDY_EXPIRES_AT(ch) <= time(NULL) || GET_LEVEL(ch) >= LVL_IMMORT) send_to_char(ch, "Tome Study: Available\r\n");
    else { tome_format_remaining(GET_TOME_STUDY_EXPIRES_AT(ch), remaining, sizeof(remaining)); send_to_char(ch, "Tome Study: %s remaining\r\n", remaining); }
    return;
  }
  if (!str_cmp(arg, "list")) {
    send_to_char(ch, "Tome Abilities\r\nAbility                         Type   Proficiency Affinity\r\n");
    for (i=1;i<=TOP_SPELL_DEFINE;i++) if (has_tome_ability(ch,i)) { send_to_char(ch, "%-31s %-7s %3d%%        %3d%%\r\n", spell_info[i].name, i<=MAX_SPELLS?"Spell":"Skill", GET_SKILL(ch,i), get_ability_class_affinity(ch,i)); any=TRUE; }
    if (!any)
      send_to_char(ch, "None.\r\n");
    return;
  }
  send_to_char(ch, "Usage: tome status | tome list\r\n");
}
