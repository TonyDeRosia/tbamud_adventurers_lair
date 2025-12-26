#ifndef PROMPT_H
#define PROMPT_H

#include "structs.h"

/* Build the player's prompt into a static buffer and return it. */
char *make_prompt(struct descriptor_data *d);

/* Set a character's prompt template to the provided string. */
void set_prompt_template(struct char_data *ch, const char *template);

/* Queue the current prompt for a descriptor. */
void queue_prompt(struct descriptor_data *d);

#endif /* PROMPT_H */
