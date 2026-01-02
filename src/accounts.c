#include "conf.h"
#include "sysdep.h"

#include "accounts.h"
#include "password.h"
#include "utils.h"
#include "db.h"

#include <ctype.h>

#include "comm.h"
#define ACCT_INDEX_FILE (LIB_ACCTFILES "index.txt")

static int ensure_account_directories(void)
{
  char path[PATH_MAX];
  size_t len;

  if (!*LIB_ACCTFILES)
    return 0;

  len = strlen(LIB_ACCTFILES);
  if (len + 1 > sizeof(path)) {
    log("SYSERR: Account path too long: %s", LIB_ACCTFILES);
    return 0;
  }

  strlcpy(path, LIB_ACCTFILES, sizeof(path));

  /* Remove trailing slash for mkdir logic. */
  if (len > 0 && path[len - 1] == '/')
    path[len - 1] = '\0';

  for (char *p = path + 1; *p; p++) {
    if (*p == '/') {
      *p = '\0';
      if (mkdir(path, 0755) == -1 && errno != EEXIST) {
        log("SYSERR: Unable to create account directory '%s': %s", path, strerror(errno));
        *p = '/';
        return 0;
      }
      *p = '/';
    }
  }

  if (mkdir(path, 0755) == -1 && errno != EEXIST) {
    log("SYSERR: Unable to create account directory '%s': %s", path, strerror(errno));
    return 0;
  }

  return 1;
}

static int ensure_account_index_file(void)
{
  FILE *fp;

  if (!ensure_account_directories())
    return 0;

  fp = fopen(ACCT_INDEX_FILE, "a");
  if (!fp) {
    log("SYSERR: Unable to open or create account index '%s': %s", ACCT_INDEX_FILE, strerror(errno));
    return 0;
  }

  fclose(fp);
  return 1;
}

void account_boot(void)
{
  if (!ensure_account_index_file())
    log("SYSERR: Account storage initialization failed; account login may be unavailable.");
}

static void get_account_filename(char *out, size_t len, long account_id)
{
  snprintf(out, len, "%s%ld.%s", LIB_ACCTFILES, account_id, SUF_ACCT);
}

static void trim_whitespace(char *str)
{
  char *start = str;
  char *end;

  if (!str)
    return;

  while (*start && isspace((unsigned char)*start))
    start++;

  end = start + strlen(start);
  while (end > start && isspace((unsigned char)*(end - 1)))
    end--;

  memmove(str, start, (size_t)(end - start));
  str[end - start] = '\0';
}

static int index_find(const char *acct_name, long *out_id)
{
  FILE *fp;
  long id = 0;
  char name[128];

  if (!out_id) return 0;
  *out_id = 0;

  if (!ensure_account_index_file())
    return 0;

  fp = fopen(ACCT_INDEX_FILE, "r");
  if (!fp) {
    log("SYSERR: Unable to read account index '%s': %s", ACCT_INDEX_FILE, strerror(errno));
    return 0;
  }

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

int account_find_id(const char *acct_name, long *out_id)
{
  return index_find(acct_name, out_id);
}

static long index_next_id(void)
{
  FILE *fp;
  long id = 0, max_id = 0;
  char name[128];

  if (!ensure_account_index_file())
    return 1;

  fp = fopen(ACCT_INDEX_FILE, "r");
  if (!fp) {
    log("SYSERR: Unable to read account index '%s': %s", ACCT_INDEX_FILE, strerror(errno));
    return 1;
  }

  while (fscanf(fp, "%ld %127s", &id, name) == 2)
    if (id > max_id) max_id = id;

  fclose(fp);
  return max_id + 1;
}

static int index_add(long id, const char *acct_name)
{
  FILE *fp;

  if (!acct_name || !*acct_name) return 0;

  if (!ensure_account_index_file())
    return 0;

  fp = fopen(ACCT_INDEX_FILE, "a");
  if (!fp) {
    log("SYSERR: Unable to write account index '%s': %s", ACCT_INDEX_FILE, strerror(errno));
    return 0;
  }

  fprintf(fp, "%ld %s\n", id, acct_name);
  fclose(fp);
  return 1;
}

int account_load_any(long acct_id, struct account_data *acct)
{
  char fname[256], line[256];
  FILE *fp;

  if (!acct) return 0;

  memset(acct, 0, sizeof(*acct));
  acct->account_id = acct_id;

  if (!ensure_account_directories())
    return 0;

  get_account_filename(fname, sizeof(fname), acct_id);
  fp = fopen(fname, "r");
  if (!fp) {
    log("SYSERR: Unable to load account file '%s': %s", fname, strerror(errno));
    return 0;
  }

  if (!fgets(line, sizeof(line), fp)) {
    fclose(fp);
    return 0;
  }

  if (!strncmp(line, "V2", 2)) {
    while (fgets(line, sizeof(line), fp)) {
      if (!strncmp(line, "Name:", 5)) {
        char *p = line + 5;
        trim_whitespace(p);
        strlcpy(acct->acct_name, p, sizeof(acct->acct_name));
      } else if (!strncmp(line, "Pass:", 5)) {
        char *p = line + 5;
        trim_whitespace(p);
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

  if (!ensure_account_directories())
    return;

  get_account_filename(fname, sizeof(fname), acct->account_id);
  fp = fopen(fname, "w");
  if (!fp) {
    log("SYSERR: Unable to save account file '%s': %s", fname, strerror(errno));
    return;
  }

  fprintf(fp, "V2\n");
  fprintf(fp, "Name: %s\n", acct->acct_name[0] ? acct->acct_name : "");
  fprintf(fp, "Pass: %s\n", acct->passwd_hash[0] ? acct->passwd_hash : "");
  fprintf(fp, "Chars: %d\n", acct->num_chars);

  for (int i = 0; i < acct->num_chars && i < MAX_CHARS_PER_ACCOUNT; i++)
    fprintf(fp, "%ld %s\n", acct->chars[i].char_id, acct->chars[i].name);

  fclose(fp);
}

int account_add_character(struct account_data *acct, long char_id, const char *char_name)
{
  int i;

  if (!acct || !char_name || !*char_name)
    return 0;

  for (i = 0; i < acct->num_chars && i < MAX_CHARS_PER_ACCOUNT; i++) {
    if (acct->chars[i].char_id == char_id || !str_cmp(acct->chars[i].name, char_name)) {
      acct->chars[i].char_id = char_id;
      strlcpy(acct->chars[i].name, char_name, sizeof(acct->chars[i].name));
      return 1;
    }
  }

  if (acct->num_chars >= MAX_CHARS_PER_ACCOUNT)
    return 0;

  acct->chars[acct->num_chars].char_id = char_id;
  strlcpy(acct->chars[acct->num_chars].name, char_name,
          sizeof(acct->chars[acct->num_chars].name));
  acct->num_chars++;
  return 1;
}

int account_authenticate(const char *acct_name, const char *passwd, long *out_id)
{
  long id = 0;
  struct account_data acct;
  char hash[MAX_PWD_HASH_LENGTH + 1];
  int upgraded = 0;

  if (out_id) *out_id = 0;
  if (!acct_name || !*acct_name) return 0;
  if (!passwd) return 0;

  if (!index_find(acct_name, &id))
    return 0;

  if (!account_load_any(id, &acct))
    return 0;

  if (!acct.passwd_hash[0])
    return 0;

  if (!password_verify(passwd, acct.passwd_hash, hash, sizeof(hash), &upgraded))
    return 0;

  if (upgraded) {
    strlcpy(acct.passwd_hash, hash, sizeof(acct.passwd_hash));
    account_save_any(&acct);
    mudlog(NRM, LVL_IMMORT, TRUE, "Account %s password upgraded to SHA512 hash.", acct.acct_name);
  }

  if (out_id) *out_id = id;
  return 1;
}

int account_create(const char *acct_name, const char *passwd, long *out_id)
{
  long id = 0;
  struct account_data acct;
  char hash[MAX_PWD_HASH_LENGTH + 1];

  if (out_id) *out_id = 0;
  if (!acct_name || !*acct_name) return 0;
  if (!passwd || !*passwd) return 0;

  if (index_find(acct_name, &id))
    return 0;

  if (strlen(passwd) < MIN_PWD_LENGTH || strlen(passwd) > MAX_PWD_LENGTH)
    return 0;

  id = index_next_id();

  memset(&acct, 0, sizeof(acct));
  acct.account_id = id;
  strlcpy(acct.acct_name, acct_name, sizeof(acct.acct_name));
  if (!password_hash(passwd, hash, sizeof(hash)))
    return 0;
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



