#ifndef _AI_ACTOR_BRAIN_H_
#define _AI_ACTOR_BRAIN_H_

#include <time.h>

struct char_data;
struct obj_data;
typedef struct ai_brain_profile ai_brain_profile;

typedef enum { AI_FEAR_NONE, AI_FEAR_WARY, AI_FEAR_INTIMIDATED, AI_FEAR_AFRAID, AI_FEAR_TERRIFIED } ai_fear_state_t;
typedef enum { AI_REV_NONE, AI_REV_RESPECTFUL, AI_REV_ADMIRING, AI_REV_REVERENT, AI_REV_AWED } ai_reverence_state_t;

enum ai_relationship_bucket {
  REL_HATEFUL = 0,
  REL_DISGUSTED,
  REL_HOSTILE,
  REL_DISLIKE,
  REL_NEUTRAL,
  REL_ADMIRE_LIGHT,
  REL_ADMIRE,
  REL_ADORE,
  REL_IN_LOVE
};

const ai_brain_profile *ai_brain_get(struct char_data *mob);
int ai_brain_can_speak(const struct char_data *mob);

void ai_actor_brain_init(struct char_data *mob);
void ai_actor_brain_free(struct char_data *mob);
int ai_actor_brain_think(struct char_data *mob, time_t now);
void ai_actor_brain_on_enter(struct char_data *mob, struct char_data *actor);
void ai_actor_brain_on_leave(struct char_data *mob, struct char_data *actor);
void ai_actor_brain_on_say(struct char_data *mob, struct char_data *actor, const char *msg);
void ai_actor_brain_on_emote(struct char_data *mob, struct char_data *actor, const char *msg);
void ai_actor_brain_on_combat_start(struct char_data *mob, struct char_data *attacker, struct char_data *victim);
void ai_actor_brain_on_attacked(struct char_data *mob, struct char_data *attacker, int dam);
void ai_actor_brain_on_corpse(struct char_data *mob, struct char_data *dead);
void ai_actor_brain_on_drop(struct char_data *mob, struct char_data *actor, struct obj_data *obj);
void ai_actor_brain_on_give(struct char_data *mob, struct char_data *actor, struct obj_data *obj, struct char_data *to);
int ai_actor_brain_enabled(void);
void ai_actor_brain_set_enabled(int enabled);
void ai_actor_brain_show_state(struct char_data *viewer, struct char_data *mob);

#endif
