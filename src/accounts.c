#include "conf.h"
#include "sysdep.h"

#include "accounts.h"
#include "utils.h"
#include "db.h"

static void get_account_filename(char *out, size_t len, long account_id)
{
  snprintf(out, len, "%s%ld.%s", LIB_ACCTFILES, account_id, SUF_ACCT);
}

void account_init_for_char(struct char_data *ch)
{
  if (GET_ACCOUNT_ID(ch) <= 0)
    GET_ACCOUNT_ID(ch) = GET_IDNUM(ch);
}

void account_load(long account_id, struct account_data *acct)
{
  char fname[256];
  FILE *fp;

  memset(acct, 0, sizeof(*acct));
  acct->account_id = account_id;

  get_account_filename(fname, sizeof(fname), account_id);
  if (!(fp = fopen(fname, "r")))
    return;

  fscanf(fp, "%d\n", &acct->num_chars);
  for (int i = 0; i < acct->num_chars && i < MAX_CHARS_PER_ACCOUNT; i++)
    fscanf(fp, "%ld %s\n", &acct->chars[i].char_id, acct->chars[i].name);

  fclose(fp);
}

void account_save(const struct account_data *acct)
{
  char fname[256];
  FILE *fp;

  get_account_filename(fname, sizeof(fname), acct->account_id);
  if (!(fp = fopen(fname, "w")))
    return;

  fprintf(fp, "%d\n", acct->num_chars);
  for (int i = 0; i < acct->num_chars; i++)
    fprintf(fp, "%ld %s\n", acct->chars[i].char_id, acct->chars[i].name);

  fclose(fp);
}

void account_attach_char(struct char_data *ch)
{
  struct account_data acct;

  account_init_for_char(ch);
  account_load(GET_ACCOUNT_ID(ch), &acct);

  for (int i = 0; i < acct.num_chars; i++)
    if (acct.chars[i].char_id == GET_IDNUM(ch))
      return;

  if (acct.num_chars >= MAX_CHARS_PER_ACCOUNT)
    return;

  acct.chars[acct.num_chars].char_id = GET_IDNUM(ch);
  strlcpy(acct.chars[acct.num_chars].name, GET_NAME(ch),
          sizeof(acct.chars[acct.num_chars].name));
  acct.num_chars++;

  account_save(&acct);
}
