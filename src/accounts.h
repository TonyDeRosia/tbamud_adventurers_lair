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
  char passwd_hash[128];
  char temp_passwd_hash[128];
  int force_pw_change;
  int num_chars;
  struct account_char_entry chars[MAX_CHARS_PER_ACCOUNT];
};

int account_authenticate(const char *acct_name, const char *passwd, long *out_id,
                         struct account_data *out_acct, int *used_temp_pw);
int account_create(const char *acct_name, const char *passwd, long *out_id);
int account_load_any(long acct_id, struct account_data *acct);
int account_id_by_name(const char *acct_name, long *out_id);
int account_set_force_pw(long acct_id, int force);
int account_set_password(long acct_id, const char *passwd, int force_pw_change);
int account_foreach_index(int (*cb)(long id, const char *name, void *arg), void *arg);
void account_save_any(const struct account_data *acct);

void account_init_for_char(struct char_data *ch);
void account_attach_char(struct char_data *ch);
void account_storage_report(void);

void acct_show_character_menu(struct descriptor_data *d);
void account_remove_character(struct account_data *acct, const char *name);

#endif
