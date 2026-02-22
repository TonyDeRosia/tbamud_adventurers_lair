#ifndef _AI_REACTIONS_H_
#define _AI_REACTIONS_H_

#include "ai_actor.h"

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

#endif
