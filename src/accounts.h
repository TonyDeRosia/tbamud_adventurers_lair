#ifndef _ACCOUNTS_H_
#define _ACCOUNTS_H_

#include "structs.h"

#define MAX_CHARS_PER_ACCOUNT 10

struct account_char_entry {
  long char_id;
  char name[64];
};

struct account_data {
  long account_id;
  char acct_name[64];
  char passwd_hash[MAX_PWD_HASH_LENGTH + 1];
  int num_chars;
  struct account_char_entry chars[MAX_CHARS_PER_ACCOUNT];
};

int account_authenticate(const char *acct_name, const char *passwd, long *out_id);
int account_create(const char *acct_name, const char *passwd, long *out_id);
int account_load_any(long acct_id, struct account_data *acct);
void account_save_any(const struct account_data *acct);
void account_boot(void);

void account_init_for_char(struct char_data *ch);
void account_attach_char(struct char_data *ch);

void acct_show_character_menu(struct descriptor_data *d);
void account_remove_character(struct account_data *acct, const char *name);

#endif
