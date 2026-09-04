#ifndef _QUEST_REWARDS_H_
#define _QUEST_REWARDS_H_

struct char_data;

void quest_rewards_list(struct char_data *ch);
void quest_rewards_buy(struct char_data *ch, char *argument);

#endif