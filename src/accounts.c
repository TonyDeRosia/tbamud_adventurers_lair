#include "conf.h"
#include "sysdep.h"

#include "accounts.h"
#include "utils.h"
#include "db.h"

#include "comm.h"
#define ACCT_INDEX_FILE (LIB_ACCTFILES "index.txt")

static void get_account_filename(char *out, size_t len, long account_id)
{
  snprintf(out, len, "%s%ld.%s", LIB_ACCTFILES, account_id, SUF_ACCT);
}

static int index_find(const char *acct_name, long *out_id)
{
  FILE *fp;
  long id = 0;
  char name[128];

  if (!out_id) return 0;
  *out_id = 0;

  fp = fopen(ACCT_INDEX_FILE, "r");
  if (!fp) return 0;

  while (fscanf(fp, "%ld %127s", &id, name) == 2) {
    if (!strcasecmp(name, acct_name)) {
      fclose(fp);
      *out_id = id;
      return 1;
    }
  }

  fclose(fp);
  return 0;
}

static long index_next_id(void)
{
  FILE *fp;
  long id = 0, max_id = 0;
  char name[128];

  fp = fopen(ACCT_INDEX_FILE, "r");
  if (!fp) return 1;

  while (fscanf(fp, "%ld %127s", &id, name) == 2)
    if (id > max_id) max_id = id;

  fclose(fp);
  return max_id + 1;
}

static int index_add(long id, const char *acct_name)
{
  FILE *fp;

  if (!acct_name || !*acct_name) return 0;

  mkdir(LIB_ACCTFILES, 0755);
  fp = fopen(ACCT_INDEX_FILE, "a");
  if (!fp) return 0;

  fprintf(fp, "%ld %s\n", id, acct_name);
  fclose(fp);
  return 1;
}

static void acct_hash_password(char *out, size_t outlen, const char *passwd)
{
  const char *salt = "AC";
  snprintf(out, outlen, "%s", CRYPT(passwd, salt));
}

int account_load_any(long acct_id, struct account_data *acct)
{
  char fname[256], line[256];
  FILE *fp;

  if (!acct) return 0;

  memset(acct, 0, sizeof(*acct));
  acct->account_id = acct_id;

  get_account_filename(fname, sizeof(fname), acct_id);
  fp = fopen(fname, "r");
  if (!fp) return 0;

  if (!fgets(line, sizeof(line), fp)) {
    fclose(fp);
    return 0;
  }

  if (!strncmp(line, "V2", 2)) {
    while (fgets(line, sizeof(line), fp)) {
      if (!strncmp(line, "Name:", 5)) {
        char *p = line + 5;
        while (*p == ' ') p++;
        p[strcspn(p, "\r\n")] = '\0';
        strlcpy(acct->acct_name, p, sizeof(acct->acct_name));
      } else if (!strncmp(line, "Pass:", 5)) {
        char *p = line + 5;
        while (*p == ' ') p++;
        p[strcspn(p, "\r\n")] = '\0';
        strlcpy(acct->passwd_hash, p, sizeof(acct->passwd_hash));
      } else if (!strncmp(line, "Chars:", 6)) {
        acct->num_chars = atoi(line + 6);
        break;
      }
    }

    for (int i = 0; i < acct->num_chars && i < MAX_CHARS_PER_ACCOUNT; i++) {
      long cid = 0;
      char cname[64] = "";
      if (fscanf(fp, "%ld %63s", &cid, cname) != 2) break;
      acct->chars[i].char_id = cid;
      strlcpy(acct->chars[i].name, cname, sizeof(acct->chars[i].name));
    }

    fclose(fp);
    return 1;
  }

  acct->num_chars = atoi(line);
  for (int i = 0; i < acct->num_chars && i < MAX_CHARS_PER_ACCOUNT; i++) {
    if (fscanf(fp, "%ld %63s", &acct->chars[i].char_id, acct->chars[i].name) != 2)
      break;
  }

  fclose(fp);
  return 1;
}

void account_save_any(const struct account_data *acct)
{
  char fname[256];
  FILE *fp;

  if (!acct) return;

  mkdir(LIB_ACCTFILES, 0755);

  get_account_filename(fname, sizeof(fname), acct->account_id);
  fp = fopen(fname, "w");
  if (!fp) return;

  fprintf(fp, "V2\n");
  fprintf(fp, "Name: %s\n", acct->acct_name[0] ? acct->acct_name : "");
  fprintf(fp, "Pass: %s\n", acct->passwd_hash[0] ? acct->passwd_hash : "");
  fprintf(fp, "Chars: %d\n", acct->num_chars);

  for (int i = 0; i < acct->num_chars && i < MAX_CHARS_PER_ACCOUNT; i++)
    fprintf(fp, "%ld %s\n", acct->chars[i].char_id, acct->chars[i].name);

  fclose(fp);
}

int account_authenticate(const char *acct_name, const char *passwd, long *out_id)
{
  long id = 0;
  struct account_data acct;
  char hash[128];

  if (out_id) *out_id = 0;
  if (!acct_name || !*acct_name) return 0;
  if (!passwd) return 0;

  if (!index_find(acct_name, &id))
    return 0;

  if (!account_load_any(id, &acct))
    return 0;

  if (!acct.passwd_hash[0])
    return 0;

  acct_hash_password(hash, sizeof(hash), passwd);
  if (strcmp(hash, acct.passwd_hash) != 0)
    return 0;

  if (out_id) *out_id = id;
  return 1;
}

int account_create(const char *acct_name, const char *passwd, long *out_id)
{
  long id = 0;
  struct account_data acct;
  char hash[128];

  if (out_id) *out_id = 0;
  if (!acct_name || !*acct_name) return 0;
  if (!passwd || !*passwd) return 0;

  if (index_find(acct_name, &id))
    return 0;

  id = index_next_id();

  memset(&acct, 0, sizeof(acct));
  acct.account_id = id;
  strlcpy(acct.acct_name, acct_name, sizeof(acct.acct_name));
  acct_hash_password(hash, sizeof(hash), passwd);
  strlcpy(acct.passwd_hash, hash, sizeof(acct.passwd_hash));

  account_save_any(&acct);
  index_add(id, acct_name);

  if (out_id) *out_id = id;
  return 1;
}

void account_init_for_char(struct char_data *ch)
{
  if (GET_ACCOUNT_ID(ch) <= 0)
    GET_ACCOUNT_ID(ch) = GET_IDNUM(ch);
}

void account_attach_char(struct char_data *ch)
{
  struct account_data acct;

  if (!ch) return;

  account_init_for_char(ch);

  if (!account_load_any(GET_ACCOUNT_ID(ch), &acct)) {
    memset(&acct, 0, sizeof(acct));
    acct.account_id = GET_ACCOUNT_ID(ch);
  }

  for (int i = 0; i < acct.num_chars; i++)
    if (acct.chars[i].char_id == GET_IDNUM(ch))
      return;

  if (acct.num_chars >= MAX_CHARS_PER_ACCOUNT)
    return;

  acct.chars[acct.num_chars].char_id = GET_IDNUM(ch);
  strlcpy(acct.chars[acct.num_chars].name, GET_NAME(ch),
          sizeof(acct.chars[acct.num_chars].name));
  acct.num_chars++;

  account_save_any(&acct);
}


/* Account character menu helper.
 * This is intentionally conservative: it compiles first, then you can expand listing logic.
 */
void acct_show_character_menu(struct descriptor_data *d)
{
  struct account_data acct;
  int i;

  if (!d) return;

  memset(&acct, 0, sizeof(acct));
  if (d->acct_id > 0 && !account_load_any(d->acct_id, &acct)) {
    /* If load fails, show empty but do not crash. */
    memset(&acct, 0, sizeof(acct));
    acct.account_id = d->acct_id;
  }

  write_to_output(d, "\r\nCharacters on this account:\r\n");
  if (acct.num_chars <= 0) {
    write_to_output(d, "  (none yet)\r\n");
  } else {
    for (i = 0; i < acct.num_chars && i < MAX_CHARS_PER_ACCOUNT; i++) {
      if (!acct.chars[i].name[0]) continue;
      write_to_output(d, "  %2d) %s\r\n", i + 1, acct.chars[i].name);
    }
  }

  write_to_output(d, "\r\nOptions:\r\n");
  write_to_output(d, "  NEW   Create a new character\r\n");
  write_to_output(d, "  0     Disconnect\r\n");
  write_to_output(d, "\r\nSelect: ");
}




void account_remove_character(struct account_data *acct, const char *name)
{
  int i, j;

  if (!acct || !name)
    return;

  for (i = 0; i < acct->num_chars; i++) {
    if (!acct->chars[i].name[0])
      continue;

    if (!str_cmp(acct->chars[i].name, name)) {
      for (j = i; j < acct->num_chars - 1; j++)
        acct->chars[j] = acct->chars[j + 1];

      memset(&acct->chars[acct->num_chars - 1], 0, sizeof(acct->chars[acct->num_chars - 1]));
      acct->num_chars--;
      break;
    }
  }
}



