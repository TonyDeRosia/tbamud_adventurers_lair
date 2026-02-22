#ifndef _AI_REACTIONS_H_
#define _AI_REACTIONS_H_

#include "ai_actor.h"
#include "ai_actor_brain.h"

struct room_data;
struct char_data;

enum ai_rx_intent {
  RX_INTENT_NONE = 0,
  RX_INTENT_GREETING,
  RX_INTENT_SMALLTALK,
  RX_INTENT_QUESTION,
  RX_INTENT_REQUEST_HELP,
  RX_INTENT_REQUEST_HEAL,
  RX_INTENT_REQUEST_TRADE,
  RX_INTENT_REQUEST_TRAIN,
  RX_INTENT_THREATEN,
  RX_INTENT_INSULT,
  RX_INTENT_APOLOGY,
  RX_INTENT_PRAISE,
  RX_INTENT_FRIENDLY_SOCIAL,
  RX_INTENT_HOSTILE_SOCIAL,
  RX_INTENT_FLIRT_LEWD,
  RX_INTENT_COMMAND,
  RX_INTENT_CONFESSION_PRAYER,
  RX_INTENT_LOOT_THEFT,
  RX_INTENT_COMBAT_START,
  RX_INTENT_COMBAT_ONGOING,
  RX_INTENT_DAMAGE_EVENT,
  RX_INTENT_PLAYER_DOWN
};

enum ai_reaction_trigger_reason {
  AI_RX_TRIG_NONE = 0,
  AI_RX_TRIG_ARB_SLOT_DENIED_EARLY,
  AI_RX_TRIG_NON_SPEAK_ACTION_SELECTED,
  AI_RX_TRIG_OPTIONAL_OTHER
};

struct ai_reaction_ctx {
  int event_type;
  int trigger_reason;
  int mob_vnum;
  int mob_role;
  int mob_style;
  int mob_personality;
  int mob_alignment;
  int mob_archetype;
  int action_selected;
  long actor_idnum;
  room_rnum room_rnum;
  unsigned long normalized_hash;
  int intent_id;
  int domain_id;
  float suspicion;
  int threat;
  int urgency;
  float attention;
  int is_fighting;
  int is_sleeping;
  int is_charmed;
  int is_stunned;
  int can_act;
  const char *raw_text;
  const char *normalized_text;
};

struct ai_rx_event {
  struct char_data *mob;
  struct char_data *speaker;
  struct room_data *room;
  const char *raw_text;
  const char *normalized_text;
  int event_type;
  unsigned int flags;
};

struct ai_rx_result {
  int handled;
  int force_emote;
  int suppress;
  int intent;
  int threat;
  int urgency;
  char line[MAX_STRING_LENGTH];
};

int ai_rx_process_event(const struct ai_rx_event *ev, struct ai_rx_result *out);
void ai_rx_clean_sentence(char *s);
int ai_rx_infer_targeted_to_mob(struct char_data *mob, const char *norm_text);
int ai_rx_is_service_style_request(const char *norm_text);
int ai_rx_is_explicit_sexual_request(const char *norm_text);

void ai_reactions_room_event_reset(room_rnum room, int event_type);
int ai_reaction_try(struct char_data *mob, const struct ai_reaction_ctx *ctx);
void ai_react_emote(struct char_data *mob, struct char_data *player, int mood, int reason);
void ai_react_nonverbal(struct char_data *mob, struct char_data *player, int reason);
void ai_react_fear(struct char_data *mob, struct char_data *player, ai_fear_state_t fear);
void ai_react_reverence(struct char_data *mob, struct char_data *player, ai_reverence_state_t rev);

#endif
