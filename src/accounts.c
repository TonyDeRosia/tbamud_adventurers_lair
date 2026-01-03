#include "conf.h"
#include "sysdep.h"

#include "accounts.h"
#include "utils.h"
#include "db.h"

#include "comm.h"
#include <dirent.h>

#define ACCT_INDEX_FILE (LIB_ACCTFILES "index.txt")

static int ensure_account_dirs(void);
static void account_debug_log(const char *format, ...);
static void account_resolve_path(char *out, size_t len, const char *relative);
static void report_storage_diagnostics(void);

static int account_debugging_enabled(void)
{
#ifdef ACCT_DEBUG_LOG
  return 1;
#else
  return CONFIG_DEBUG_MODE;
#endif
}

static void account_debug_log(const char *format, ...)
{
  va_list ap;
  char buf[MAX_STRING_LENGTH];

  if (!account_debugging_enabled())
    return;

  va_start(ap, format);
  vsnprintf(buf, sizeof(buf), format, ap);
  va_end(ap);

  mudlog(CMP, LVL_IMPL, TRUE, "%s", buf);
}

static void account_resolve_path(char *out, size_t len, const char *relative)
{
  char cwd[PATH_MAX];

  if (!out || !relative || len == 0) {
    if (out && len > 0)
      *out = '\0';
    return;
  }

  if (relative[0] == '/') {
    strlcpy(out, relative, len);
    return;
  }

  if (getcwd(cwd, sizeof(cwd))) {
    strlcpy(out, cwd, len);
    /* build path without strlcat (not available on all platforms) */
    {
      size_t n = strlen(out);
      if (n > 0 && out[n-1] != '/' && n + 1 < len) { out[n++] = '/'; out[n] = '\0'; }
      if ("/" && *"/") {
        snprintf(out + n, (n < len) ? (len - n) : 0, "%s", "/");
      }
    }

    /* build path without strlcat (not available on all platforms) */
    {
      size_t n = strlen(out);
      if (n > 0 && out[n-1] != '/' && n + 1 < len) { out[n++] = '/'; out[n] = '\0'; }
      if (relative && *relative) {
        snprintf(out + n, (n < len) ? (len - n) : 0, "%s", relative);
      }
    }

  } else
    strlcpy(out, relative, len);
}

static int ensure_dir_exists(const char *path)
{
  if (mkdir(path, 0755) == -1 && errno != EEXIST) {
    mudlog(CMP, LVL_IMPL, TRUE, "SYSERR: Unable to create %s: %s", path, strerror(errno));
    return 0;
  }

  return 1;
}

static int ensure_account_dirs(void)
{
  char plr_dir[PATH_MAX], acct_dir[PATH_MAX];

  account_resolve_path(plr_dir, sizeof(plr_dir), LIB_PLRFILES);
  account_resolve_path(acct_dir, sizeof(acct_dir), LIB_ACCTFILES);

  return ensure_dir_exists(plr_dir) && ensure_dir_exists(acct_dir);
}

static void get_account_filename(char *out, size_t len, long account_id)
{
  char relative[256];

  snprintf(relative, sizeof(relative), "%s%ld.%s", LIB_ACCTFILES, account_id, SUF_ACCT);
  account_resolve_path(out, len, relative);
}

static int index_find(const char *acct_name, long *out_id)
{
  FILE *fp;
  long id = 0;
  char name[128];
  char path[PATH_MAX];
  static int warned = 0;

  if (!out_id) return 0;
  *out_id = 0;

  account_resolve_path(path, sizeof(path), ACCT_INDEX_FILE);

  fp = fopen(path, "r");
  if (!fp) {
    if (!warned) {
      mudlog(CMP, LVL_IMPL, TRUE, "SYSERR: Unable to open account index %s: %s", path, strerror(errno));
      warned = 1;
    }
    ensure_account_dirs();
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

static long index_next_id(void)
{
  FILE *fp;
  long id = 0, max_id = 0;
  char name[128];
  char path[PATH_MAX];

  account_resolve_path(path, sizeof(path), ACCT_INDEX_FILE);

  fp = fopen(path, "r");
  if (!fp) return 1;

  while (fscanf(fp, "%ld %127s", &id, name) == 2)
    if (id > max_id) max_id = id;

  fclose(fp);
  return max_id + 1;
}

static int index_add(long id, const char *acct_name)
{
  FILE *fp;
  char path[PATH_MAX];

  if (!acct_name || !*acct_name) return 0;

  if (!ensure_account_dirs())
    return 0;

  account_resolve_path(path, sizeof(path), ACCT_INDEX_FILE);
  fp = fopen(path, "a");
  if (!fp) {
    mudlog(CMP, LVL_IMPL, TRUE, "SYSERR: Unable to append account index %s: %s", path, strerror(errno));
    return 0;
  }

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
  char path[PATH_MAX];

  if (!acct) return 0;

  memset(acct, 0, sizeof(*acct));
  acct->account_id = acct_id;

  get_account_filename(fname, sizeof(fname), acct_id);
  account_resolve_path(path, sizeof(path), fname);

  fp = fopen(path, "r");
  if (!fp) {
    mudlog(CMP, LVL_IMPL, TRUE, "SYSERR: Unable to open account file %s: %s", path, strerror(errno));
    ensure_account_dirs();
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
  char path[PATH_MAX];

  if (!acct) return;

  if (!ensure_account_dirs())
    return;

  get_account_filename(fname, sizeof(fname), acct->account_id);
  account_resolve_path(path, sizeof(path), fname);
  fp = fopen(path, "w");
  if (!fp) {
    mudlog(CMP, LVL_IMPL, TRUE, "SYSERR: Unable to write account file %s: %s", path, strerror(errno));
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

int account_authenticate(const char *acct_name, const char *passwd, long *out_id)
{
  long id = 0;
  struct account_data acct;
  char hash[128];
  char clean_pass[MAX_PWD_LENGTH + 1];

  if (out_id) *out_id = 0;
  if (!acct_name || !*acct_name) return 0;
  if (!passwd) return 0;

  strlcpy(clean_pass, passwd, sizeof(clean_pass));
  clean_pass[strcspn(clean_pass, "\r\n")] = '\0';

  if (!*clean_pass || strlen(clean_pass) > MAX_PWD_LENGTH)
    return 0;

  ensure_account_dirs();

  if (!index_find(acct_name, &id))
    return 0;

  if (!account_load_any(id, &acct))
    return 0;

  if (!acct.passwd_hash[0])
    return 0;

  acct_hash_password(hash, sizeof(hash), clean_pass);
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
  char clean_pass[MAX_PWD_LENGTH + 1];

  if (out_id) *out_id = 0;
  if (!acct_name || !*acct_name) return 0;
  if (!passwd || !*passwd) return 0;

  strlcpy(clean_pass, passwd, sizeof(clean_pass));
  clean_pass[strcspn(clean_pass, "\r\n")] = '\0';

  if (!*clean_pass || strlen(clean_pass) > MAX_PWD_LENGTH)
    return 0;

  if (!ensure_account_dirs())
    return 0;

  if (index_find(acct_name, &id))
    return 0;

  id = index_next_id();

  memset(&acct, 0, sizeof(acct));
  acct.account_id = id;
  strlcpy(acct.acct_name, acct_name, sizeof(acct.acct_name));
  acct_hash_password(hash, sizeof(hash), clean_pass);
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

static size_t count_account_files(const char *dirpath)
{
  DIR *dirp;
  struct dirent *dp;
  size_t count = 0;

  dirp = opendir(dirpath);
  if (!dirp)
    return 0;

  while ((dp = readdir(dirp)) != NULL) {
    const char *dot = strrchr(dp->d_name, '.');
    if (dot && !strcasecmp(dot + 1, SUF_ACCT))
      count++;
  }

  closedir(dirp);
  return count;
}

static void report_storage_diagnostics(void)
{
  char acct_dir[PATH_MAX], index_path[PATH_MAX];
  struct stat st;
  int exists = 0, readable = 0, writable = 0;
  size_t acct_files = 0;

  account_resolve_path(acct_dir, sizeof(acct_dir), LIB_ACCTFILES);
  account_resolve_path(index_path, sizeof(index_path), ACCT_INDEX_FILE);

  /* Trim trailing slash for stat/access calls if present. */
  if (strlen(acct_dir) > 1 && acct_dir[strlen(acct_dir) - 1] == '/')
    acct_dir[strlen(acct_dir) - 1] = '\0';

  if (stat(acct_dir, &st) == 0 && S_ISDIR(st.st_mode)) {
    exists = 1;
    readable = (access(acct_dir, R_OK) == 0);
    writable = (access(acct_dir, W_OK) == 0);
    acct_files = count_account_files(acct_dir);
  }

  account_debug_log("Account dir: %s (exists=%s, readable=%s, writable=%s)",
                    acct_dir,
                    exists ? "yes" : "no",
                    readable ? "yes" : "no",
                    writable ? "yes" : "no");
  account_debug_log("Account index: %s", index_path);
  account_debug_log("Account file count: %zu", acct_files);
}

void account_storage_report(void)
{
  static int reported = 0;

  if (reported)
    return;

  reported = 1;

  if (!account_debugging_enabled())
    return;

  /* Attempt to bring directories online before reporting status. */
  ensure_account_dirs();
  report_storage_diagnostics();
}



