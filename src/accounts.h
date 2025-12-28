#ifndef _ACCOUNTS_H_
#define _ACCOUNTS_H_

#include "structs.h"

#define MAX_CHARS_PER_ACCOUNT 10

struct account_char_entry {
  long char_id;
  char name[MAX_NAME_LENGTH + 1];
};

struct account_data {
  long account_id;
  int  num_chars;
  struct account_char_entry chars[MAX_CHARS_PER_ACCOUNT];
};

void account_init_for_char(struct char_data *ch);
void account_load(long account_id, struct account_data *acct);
void account_save(const struct account_data *acct);
void account_attach_char(struct char_data *ch);

#endif
