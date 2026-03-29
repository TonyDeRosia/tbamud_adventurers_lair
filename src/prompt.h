#ifndef PROMPT_H
#define PROMPT_H

#include <stddef.h>
#include "structs.h"

/* Build the player's prompt into a static buffer and return it. */
char *make_prompt(struct descriptor_data *d);

/* Build the player's prompt into a caller-provided buffer. */
void build_prompt(struct char_data *ch, char *buffer, size_t buffer_size);

/* Queue the current prompt for a descriptor. */
void queue_prompt(struct descriptor_data *d);

ACMD(do_prompt);

#endif /* PROMPT_H */
