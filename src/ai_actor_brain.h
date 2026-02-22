#ifndef _AI_ACTOR_BRAIN_H_
#define _AI_ACTOR_BRAIN_H_

#include <stdint.h>
#include <time.h>

#define AI_BRAIN_MEM_MAX 20

typedef struct ai_brain_profile ai_brain_profile;

struct char_data;
struct obj_data;
typedef enum { AI_FEAR_NONE, AI_FEAR_WARY, AI_FEAR_INTIMIDATED, AI_FEAR_AFRAID, AI_FEAR_TERRIFIED } ai_fear_state_t;
typedef enum { AI_REV_NONE, AI_REV_RESPECTFUL, AI_REV_ADMIRING, AI_REV_REVERENT, AI_REV_AWED } ai_reverence_state_t;

enum ai_actor_brain_state {
  AI_BRAIN_IDLE = 0,
  AI_BRAIN_OBSERVE,
  AI_BRAIN_ENGAGE,
  AI_BRAIN_WARN,
  AI_BRAIN_ASSIST,
  AI_BRAIN_FLEE,
  AI_BRAIN_PURSUE,
  AI_BRAIN_REPORT,
  AI_BRAIN_TRADE
};

enum ai_actor_relationship {
  AI_REL_ALLY = 0,
  AI_REL_NEUTRAL,
  AI_REL_ENEMY
};

struct ai_actor_traits {
  int bravery, greed, curiosity, empathy, aggression, discipline, superstition;
};

struct ai_actor_brain_mem {
  long idnum;
  int trust;
  int fear;
  int hostility;
  int relationship;
  int crime_flags;
  time_t last_seen;
  time_t last_update;
  char last_action[48];
};

struct ai_actor_brain {
  int archetype;
  int social_style;
  int goal_mask;
  int roam_allowed;
  enum ai_actor_brain_state state;
  struct ai_actor_traits traits;
  time_t last_think;
  time_t next_move_at;
  time_t next_global_speak;
  long last_target_id;
  char last_speaker[32];
  char last_loud_event[64];
  char last_fight_outcome[48];
  struct ai_actor_brain_mem mem[AI_BRAIN_MEM_MAX];
  int mem_count;
};

const ai_brain_profile *ai_brain_get(struct char_data *mob);
void ai_brain_ensure(struct char_data *mob);
void ai_brain_infer_role_and_caps(struct char_data *mob, int *out_role, int *out_fit, uint32_t *out_caps);
int ai_brain_can_speak(const struct char_data *mob);
int ai_brain_voice_style(const struct char_data *mob);
int ai_brain_knows_domain(const struct char_data *mob, int domain);
int ai_brain_pick_referral_in_room(struct char_data *mob, struct char_data *player, int domain, struct char_data **out_target);
int ai_brain_intimidation_score(struct char_data *mob, struct char_data *player);
ai_fear_state_t ai_brain_fear_state(struct char_data *mob, struct char_data *player);
int ai_brain_respect_score(struct char_data *mob, struct char_data *player);
ai_reverence_state_t ai_brain_reverence_state(struct char_data *mob, struct char_data *player);
void ai_brain_apply_stance_bias(const ai_brain_profile *p, ai_fear_state_t fear, ai_reverence_state_t rev, int *io_mood, int *io_voice_style, uint32_t *io_caps);

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
