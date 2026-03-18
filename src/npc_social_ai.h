#ifndef _NPC_SOCIAL_AI_H_
#define _NPC_SOCIAL_AI_H_

#include <time.h>

struct char_data;

enum npc_role {
  NPC_ROLE_GUARD = 0,
  NPC_ROLE_MERCHANT,
  NPC_ROLE_TRAINER,
  NPC_ROLE_HEALER,
  NPC_ROLE_QUESTGIVER,
  NPC_ROLE_INNKEEPER,
  NPC_ROLE_CLERK,
  NPC_ROLE_PRIEST,
  NPC_ROLE_OFFICIAL,
  NPC_ROLE_CIVILIAN,
  NPC_ROLE_BANDIT,
  NPC_ROLE_WANDERER,
  NPC_ROLE_GENERIC_SERVICE,
  NPC_ROLE_GENERIC_TOWNSFOLK
};

enum npc_temperament {
  NPC_TEMP_AGGRESSIVE = 0,
  NPC_TEMP_STEADY,
  NPC_TEMP_TIMID
};

enum npc_social_style {
  NPC_SOCIAL_INTROVERT = 0,
  NPC_SOCIAL_NEUTRAL,
  NPC_SOCIAL_EXTROVERT
};

enum npc_priority {
  NPC_PRIO_IDLE = 0,
  NPC_PRIO_WORK,
  NPC_PRIO_OBSERVE,
  NPC_PRIO_WARN,
  NPC_PRIO_ENGAGE,
  NPC_PRIO_ASSIST,
  NPC_PRIO_FLEE,
  NPC_PRIO_RECOVER,
  NPC_PRIO_SOCIALIZE
};

struct npc_social_profile {
  enum npc_role role;
  enum npc_temperament temperament;
  enum npc_social_style social_style;
};

int npc_ai_is_humanoid_social_candidate(struct char_data *ch);
void npc_ai_build_profile(struct char_data *ch, struct npc_social_profile *out);
enum npc_priority npc_ai_choose_priority(struct char_data *ch, const struct npc_social_profile *profile, time_t now);
void npc_ai_handle_player_enter(struct char_data *ch, struct char_data *player, time_t now);
void npc_ai_handle_player_leave(struct char_data *ch, struct char_data *player, time_t now);
void npc_ai_handle_speech_event(struct char_data *ch, struct char_data *player, const char *text, time_t now);
void npc_ai_handle_room_danger(struct char_data *ch, struct char_data *actor, time_t now);
void npc_ai_maybe_do_ambient_action(struct char_data *ch, const struct npc_social_profile *profile, time_t now);
const char *npc_ai_get_dialogue_line(const struct npc_social_profile *profile, enum npc_priority prio, int repeat);
void npc_ai_do_emote(struct char_data *ch, const struct npc_social_profile *profile, time_t now);
void npc_ai_update_memory(struct char_data *ch, struct char_data *player, int trust_delta, int annoyance_delta, int fear_delta, time_t now);

#endif
