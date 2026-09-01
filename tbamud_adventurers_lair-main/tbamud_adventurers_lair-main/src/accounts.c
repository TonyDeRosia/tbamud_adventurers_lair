#include "conf.h"
#include "sysdep.h"

#include "accounts.h"
#include "utils.h"
#include "db.h"
#include "dg_scripts.h"

#include "comm.h"
#include <dirent.h>

#define ACCT_INDEX_FILE (LIB_ACCTFILES "index.txt")

static int ensure_account_dirs(void);
static int ensure_account_index(void);
static int rebuild_account_index(void);
static int account_write_replace(const char *path, int (*writer)(FILE *fp, const struct account_data *acct), const struct account_data *acct);
static void account_debug_log(const char *format, ...);
static void account_resolve_path(char *out, size_t len, const char *relative);
static int account_verify_password(const char *password, const char *stored_hash, const char *acct_name);
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
    snprintf(out, len, "%s", relative);
    return;
  }

  if (getcwd(cwd, sizeof(cwd))) {
    size_t n = strlcpy(out, cwd, len);

    if (n > 0 && n < len && out[n - 1] != '/') {
      if (n + 1 < len) {
        out[n++] = '/';
        out[n] = '\0';
      }
    }

    if (len > n)
      strlcpy(out + n, relative, len - n);
  } else {
    snprintf(out, len, "%s", relative);
  }
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

static int ensure_account_index(void)
{
  char path[PATH_MAX];
  struct stat st;

  if (!ensure_account_dirs())
    return 0;

  account_resolve_path(path, sizeof(path), ACCT_INDEX_FILE);
  if (stat(path, &st) == 0)
    return 1;

  if (errno != ENOENT) {
    mudlog(CMP, LVL_IMPL, TRUE, "SYSERR: Unable to stat account index %s: %s", path, strerror(errno));
    return 0;
  }

  return rebuild_account_index();
}

static int account_write_replace(const char *path, int (*writer)(FILE *fp, const struct account_data *acct), const struct account_data *acct)
{
  char tmp_path[PATH_MAX];
  FILE *fp;
  int write_ok = 1;

  if (strlen(path) + 4 >= sizeof(tmp_path)) {
    mudlog(CMP, LVL_IMPL, TRUE, "SYSERR: Account path too long for temporary write: %s", path);
    return 0;
  }
  snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", path);
  fp = fopen(tmp_path, "w");
  if (!fp) {
    mudlog(CMP, LVL_IMPL, TRUE, "SYSERR: Unable to write temporary account file %s: %s", tmp_path, strerror(errno));
    return 0;
  }

  if (!writer(fp, acct))
    write_ok = 0;
  if (write_ok && fflush(fp) != 0)
    write_ok = 0;
  if (write_ok && fsync(fileno(fp)) != 0)
    write_ok = 0;
  if (fclose(fp) != 0)
    write_ok = 0;

  if (!write_ok) {
    mudlog(CMP, LVL_IMPL, TRUE, "SYSERR: Unable to durably write temporary account file %s: %s", tmp_path, strerror(errno));
    unlink(tmp_path);
    return 0;
  }

  if (rename(tmp_path, path) != 0) {
    if (errno == EEXIST || errno == EACCES) {
      unlink(path);
      if (rename(tmp_path, path) == 0)
        return 1;
    }
    mudlog(CMP, LVL_IMPL, TRUE, "SYSERR: Unable to replace account file %s: %s", path, strerror(errno));
    unlink(tmp_path);
    return 0;
  }

  return 1;
}

static void get_account_filename(char *out, size_t len, long account_id)
{
  char relative[256];

  snprintf(relative, sizeof(relative), "%s%ld.%s", LIB_ACCTFILES, account_id, SUF_ACCT);
  account_resolve_path(out, len, relative);
}

static int account_file_id_from_name(const char *filename, long *out_id)
{
  char *endp;
  long id;
  const char *dot;

  if (out_id)
    *out_id = 0;
  if (!filename || !*filename)
    return 0;

  dot = strrchr(filename, '.');
  if (!dot || strcasecmp(dot + 1, SUF_ACCT))
    return 0;

  errno = 0;
  id = strtol(filename, &endp, 10);
  if (errno || endp != dot || id <= 0)
    return 0;

  if (out_id)
    *out_id = id;
  return 1;
}

static int rebuild_account_index(void)
{
  char acct_dir[PATH_MAX], index_path[PATH_MAX];
  DIR *dirp;
  struct dirent *dp;
  FILE *fp;
  char tmp_path[PATH_MAX];
  int count = 0, write_ok = 1;

  if (!ensure_account_dirs())
    return 0;

  account_resolve_path(acct_dir, sizeof(acct_dir), LIB_ACCTFILES);
  account_resolve_path(index_path, sizeof(index_path), ACCT_INDEX_FILE);
  if (strlen(acct_dir) > 1 && acct_dir[strlen(acct_dir) - 1] == '/')
    acct_dir[strlen(acct_dir) - 1] = '\0';

  dirp = opendir(acct_dir);
  if (!dirp) {
    mudlog(CMP, LVL_IMPL, TRUE, "SYSERR: Unable to scan account directory %s: %s", acct_dir, strerror(errno));
    return 0;
  }

  if (strlen(index_path) + 4 >= sizeof(tmp_path)) {
    closedir(dirp);
    mudlog(CMP, LVL_IMPL, TRUE, "SYSERR: Account index path too long for temporary rebuild: %s", index_path);
    return 0;
  }
  snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", index_path);
  fp = fopen(tmp_path, "w");
  if (!fp) {
    closedir(dirp);
    mudlog(CMP, LVL_IMPL, TRUE, "SYSERR: Unable to rebuild account index %s: %s", index_path, strerror(errno));
    return 0;
  }

  while ((dp = readdir(dirp)) != NULL) {
    long id = 0;
    struct account_data acct;

    if (!account_file_id_from_name(dp->d_name, &id))
      continue;
    if (!account_load_any(id, &acct) || !acct.acct_name[0])
      continue;
    if (fprintf(fp, "%ld %s\n", id, acct.acct_name) < 0) {
      write_ok = 0;
      break;
    }
    count++;
  }

  closedir(dirp);
  if (write_ok && fflush(fp) != 0)
    write_ok = 0;
  if (write_ok && fsync(fileno(fp)) != 0)
    write_ok = 0;
  if (fclose(fp) != 0)
    write_ok = 0;

  if (!write_ok) {
    mudlog(CMP, LVL_IMPL, TRUE, "SYSERR: Unable to durably rebuild account index %s: %s", index_path, strerror(errno));
    unlink(tmp_path);
    return 0;
  }

  if (rename(tmp_path, index_path) != 0) {
    if (errno == EEXIST || errno == EACCES) {
      unlink(index_path);
      if (rename(tmp_path, index_path) == 0) {
        mudlog(CMP, LVL_IMPL, TRUE, "Account index rebuilt from %d account file%s.", count, count == 1 ? "" : "s");
        return 1;
      }
    }
    mudlog(CMP, LVL_IMPL, TRUE, "SYSERR: Unable to replace rebuilt account index %s: %s", index_path, strerror(errno));
    unlink(tmp_path);
    return 0;
  }

  if (count == 0)
    mudlog(CMP, LVL_IMPL, TRUE, "Account index initialized at %s.", index_path);
  else
    mudlog(CMP, LVL_IMPL, TRUE, "Account index rebuilt from %d account file%s.", count, count == 1 ? "" : "s");
  return 1;
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

  if (!ensure_account_index())
    return 0;

  account_resolve_path(path, sizeof(path), ACCT_INDEX_FILE);

  fp = fopen(path, "r");
  if (!fp) {
    if (!warned) {
      mudlog(CMP, LVL_IMPL, TRUE, "SYSERR: Unable to open account index %s: %s", path, strerror(errno));
      warned = 1;
    }
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

  if (!ensure_account_index())
    return 1;

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

  if (!ensure_account_index())
    return 0;

  account_resolve_path(path, sizeof(path), ACCT_INDEX_FILE);
  fp = fopen(path, "a");
  if (!fp) {
    mudlog(CMP, LVL_IMPL, TRUE, "SYSERR: Unable to append account index %s: %s", path, strerror(errno));
    return 0;
  }

  if (fprintf(fp, "%ld %s\n", id, acct_name) < 0) {
    mudlog(CMP, LVL_IMPL, TRUE, "SYSERR: Unable to append account index %s: %s", path, strerror(errno));
    fclose(fp);
    return 0;
  }
  if (fflush(fp) != 0 || fsync(fileno(fp)) != 0 || fclose(fp) != 0) {
    mudlog(CMP, LVL_IMPL, TRUE, "SYSERR: Unable to durably append account index %s: %s", path, strerror(errno));
    return 0;
  }
  mudlog(CMP, LVL_IMPL, TRUE, "Account index updated for %s.", acct_name);
  return 1;
}

int account_foreach_index(int (*cb)(long id, const char *name, void *arg), void *arg)
{
  FILE *fp;
  long id = 0;
  char name[128];
  char path[PATH_MAX];
  int count = 0;

  if (!cb)
    return 0;

  if (!ensure_account_index())
    return 0;

  account_resolve_path(path, sizeof(path), ACCT_INDEX_FILE);
  fp = fopen(path, "r");
  if (!fp)
    return 0;

  while (fscanf(fp, "%ld %127s", &id, name) == 2) {
    count++;
    if (!cb(id, name, arg))
      break;
  }

  fclose(fp);
  return count;
}

static void acct_hash_password(char *out, size_t outlen, const char *passwd)
{
  const char *salt = "AC";
  snprintf(out, outlen, "%s", CRYPT(passwd, salt));
}

static int account_password_is_valid(const char *passwd)
{
  if (!passwd)
    return 0;

  if (!*passwd || strlen(passwd) > MAX_PWD_LENGTH)
    return 0;

  return 1;
}

static int account_v3_is_valid(const struct account_data *acct, int actual_chars, const char *path)
{
  int i, j;

  if (!acct || acct->account_id <= 0) {
    mudlog(CMP, LVL_IMPL, TRUE, "SYSERR: Malformed account record %s: invalid account id.", path ? path : "<unknown>");
    return 0;
  }
  if (!acct->acct_name[0]) {
    mudlog(CMP, LVL_IMPL, TRUE, "SYSERR: Malformed account record %s: missing account name.", path ? path : "<unknown>");
    return 0;
  }
  if (acct->num_chars < 0 || acct->num_chars > MAX_CHARS_PER_ACCOUNT) {
    mudlog(CMP, LVL_IMPL, TRUE, "SYSERR: Malformed account record %s: invalid character count %d.",
           path ? path : "<unknown>", acct->num_chars);
    return 0;
  }
  if (actual_chars != acct->num_chars) {
    mudlog(CMP, LVL_IMPL, TRUE, "SYSERR: Malformed account record %s: declared %d characters but read %d.",
           path ? path : "<unknown>", acct->num_chars, actual_chars);
    return 0;
  }

  for (i = 0; i < acct->num_chars; i++) {
    if (acct->chars[i].char_id <= 0 || !acct->chars[i].name[0]) {
      mudlog(CMP, LVL_IMPL, TRUE, "SYSERR: Malformed account record %s: invalid character roster entry.",
             path ? path : "<unknown>");
      return 0;
    }
    for (j = i + 1; j < acct->num_chars; j++) {
      if (acct->chars[i].char_id == acct->chars[j].char_id ||
          !str_cmp(acct->chars[i].name, acct->chars[j].name)) {
        mudlog(CMP, LVL_IMPL, TRUE, "SYSERR: Malformed account record %s: duplicate character roster entry.",
               path ? path : "<unknown>");
        return 0;
      }
    }
  }

  return 1;
}

static int account_verify_password(const char *password, const char *stored_hash, const char *acct_name)
{
  const char *crypted;

  if (!password || !stored_hash || !*stored_hash)
    return 0;

  /* Legacy DES hashes are 13 chars and do not start with '$'. */
  if (stored_hash[0] != '$' && strlen(stored_hash) == 13)
    crypted = CRYPT(password, stored_hash);
  else
    crypted = CRYPT(password, stored_hash);

  if (!crypted) {
    account_debug_log("Account auth failed for %s: crypt() returned NULL", acct_name ? acct_name : "<unknown>");
    return 0;
  }

  if (strcmp(crypted, stored_hash) != 0) {
    account_debug_log("Account auth failed for %s: hash mismatch", acct_name ? acct_name : "<unknown>");
    return 0;
  }

  return 1;
}

int account_load_any(long acct_id, struct account_data *acct)
{
  char fname[256], line[256];
  FILE *fp;
  char path[PATH_MAX];

  if (!acct) return 0;
  if (acct_id <= 0) {
    mudlog(CMP, LVL_IMPL, TRUE, "SYSERR: Refusing to load invalid account id %ld.", acct_id);
    return 0;
  }

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

  if (!strncmp(line, "V", 1)) {
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
      } else if (!strncmp(line, "ForcePW:", 8)) {
        acct->force_pw_change = atoi(line + 8) ? 1 : 0;
      } else if (!strncmp(line, "TempPW:", 7)) {
        char *p = line + 7;
        while (*p == ' ') p++;
        p[strcspn(p, "\r\n")] = '\0';
        strlcpy(acct->temp_passwd_hash, p, sizeof(acct->temp_passwd_hash));
      } else if (!strncmp(line, "Chars:", 6)) {
        acct->num_chars = atoi(line + 6);
        break;
      }
    }

    int actual_chars = 0;

    if (acct->num_chars < 0 || acct->num_chars > MAX_CHARS_PER_ACCOUNT) {
      fclose(fp);
      return account_v3_is_valid(acct, 0, path);
    }

    for (int i = 0; i < acct->num_chars; i++) {
      long cid = 0;
      char cname[64] = "";
      if (fscanf(fp, "%ld %63s", &cid, cname) != 2)
        break;
      acct->chars[i].char_id = cid;
      strlcpy(acct->chars[i].name, cname, sizeof(acct->chars[i].name));
      actual_chars++;
    }

    fclose(fp);
    if (!account_v3_is_valid(acct, actual_chars, path))
      return 0;

    mudlog(CMP, LVL_IMPL, TRUE, "Account loaded: id=%ld name=%s chars=%d.",
           acct->account_id, acct->acct_name, acct->num_chars);
    return 1;
  }

  /* Legacy V1 format: first line is the number of characters. */
  acct->num_chars = atoi(line);
  for (int i = 0; i < acct->num_chars && i < MAX_CHARS_PER_ACCOUNT; i++) {
    if (fscanf(fp, "%ld %63s", &acct->chars[i].char_id, acct->chars[i].name) != 2)
      break;
  }

  fclose(fp);
  return 1;
}

static int account_file_writer(FILE *fp, const struct account_data *acct)
{
  int i;

  if (!fp || !acct)
    return 0;

  if (fprintf(fp, "V3\n") < 0) return 0;
  if (fprintf(fp, "Name: %s\n", acct->acct_name[0] ? acct->acct_name : "") < 0) return 0;
  if (fprintf(fp, "Pass: %s\n", acct->passwd_hash[0] ? acct->passwd_hash : "") < 0) return 0;
  if (fprintf(fp, "ForcePW: %d\n", acct->force_pw_change ? 1 : 0) < 0) return 0;
  if (acct->temp_passwd_hash[0] && fprintf(fp, "TempPW: %s\n", acct->temp_passwd_hash) < 0) return 0;
  if (fprintf(fp, "Chars: %d\n", acct->num_chars) < 0) return 0;

  for (i = 0; i < acct->num_chars && i < MAX_CHARS_PER_ACCOUNT; i++)
    if (fprintf(fp, "%ld %s\n", acct->chars[i].char_id, acct->chars[i].name) < 0)
      return 0;

  return 1;
}

int account_save_any(const struct account_data *acct)
{
  char fname[256];
  char path[PATH_MAX];

  if (!acct) return 0;
  if (acct->account_id <= 0) {
    mudlog(CMP, LVL_IMPL, TRUE, "SYSERR: Refusing to save account with invalid id %ld.", acct->account_id);
    return 0;
  }
  if (acct->num_chars < 0 || acct->num_chars > MAX_CHARS_PER_ACCOUNT) {
    mudlog(CMP, LVL_IMPL, TRUE, "SYSERR: Refusing to save malformed account %ld: invalid character count %d.",
           acct->account_id, acct->num_chars);
    return 0;
  }

  if (!ensure_account_dirs())
    return 0;

  get_account_filename(fname, sizeof(fname), acct->account_id);
  account_resolve_path(path, sizeof(path), fname);
  if (!account_write_replace(path, account_file_writer, acct)) {
    mudlog(CMP, LVL_IMPL, TRUE, "SYSERR: Account save failed for account id %ld.", acct->account_id);
    return 0;
  }

  mudlog(CMP, LVL_IMPL, TRUE, "Account saved: id=%ld name=%s chars=%d.", acct->account_id,
         acct->acct_name[0] ? acct->acct_name : "<legacy>", acct->num_chars);
  return 1;
}

int account_authenticate(const char *acct_name, const char *passwd, long *out_id,
                         struct account_data *out_acct, int *used_temp_pw)
{
  long id = 0;
  struct account_data acct;
  char clean_pass[MAX_PWD_LENGTH + 1];
  int temp_used = 0;

  if (out_id) *out_id = 0;
  if (used_temp_pw) *used_temp_pw = 0;
  if (out_acct) memset(out_acct, 0, sizeof(*out_acct));

  if (!acct_name || !*acct_name) return 0;
  if (!passwd) return 0;

  strlcpy(clean_pass, passwd, sizeof(clean_pass));
  clean_pass[strcspn(clean_pass, "\r\n")] = '\0';

  if (!account_password_is_valid(clean_pass))
    return 0;

  ensure_account_dirs();

  if (!index_find(acct_name, &id))
    return 0;

  if (!account_load_any(id, &acct))
    return 0;

  if (!acct.passwd_hash[0])
    return 0;

  if (!account_verify_password(clean_pass, acct.passwd_hash, acct.acct_name)) {
    if (acct.temp_passwd_hash[0] && account_verify_password(clean_pass, acct.temp_passwd_hash, acct.acct_name)) {
      temp_used = 1;
      acct.temp_passwd_hash[0] = '\0';
      if (!account_save_any(&acct))
        return 0;
    } else {
      return 0;
    }
  }

  if (out_id) *out_id = id;
  if (out_acct) *out_acct = acct;
  if (used_temp_pw) *used_temp_pw = temp_used;
  return 1;
}

int account_create(const char *acct_name, const char *passwd, long *out_id)
{
  long id = 0;
  struct account_data acct;
  char hash[128];
  char clean_pass[MAX_PWD_LENGTH + 1];
  char fname[256], path[PATH_MAX];

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
  get_account_filename(fname, sizeof(fname), id);
  account_resolve_path(path, sizeof(path), fname);

  if (!account_save_any(&acct))
    return 0;
  if (!index_add(id, acct_name)) {
    unlink(path);
    return 0;
  }

  mudlog(CMP, LVL_IMPL, TRUE, "Account created: id=%ld name=%s.", id, acct_name);
  if (out_id) *out_id = id;
  return 1;
}

int account_id_by_name(const char *acct_name, long *out_id)
{
  return index_find(acct_name, out_id);
}

int account_set_force_pw(long acct_id, int force)
{
  struct account_data acct;

  if (!account_load_any(acct_id, &acct))
    return 0;

  acct.force_pw_change = force ? 1 : 0;
  if (!acct.force_pw_change)
    acct.temp_passwd_hash[0] = '\0';

  return account_save_any(&acct);
}

int account_set_password(long acct_id, const char *passwd, int force_pw_change)
{
  struct account_data acct;
  char hash[128];

  if (!account_password_is_valid(passwd))
    return 0;

  if (!account_load_any(acct_id, &acct))
    return 0;

  acct_hash_password(hash, sizeof(hash), passwd);
  strlcpy(acct.passwd_hash, hash, sizeof(acct.passwd_hash));
  acct.force_pw_change = force_pw_change ? 1 : 0;
  acct.temp_passwd_hash[0] = '\0';

  return account_save_any(&acct);
}

void account_init_for_char(struct char_data *ch)
{
  if (GET_ACCOUNT_ID(ch) <= 0)
    GET_ACCOUNT_ID(ch) = GET_IDNUM(ch);
}

int account_attach_char(struct char_data *ch)
{
  struct account_data acct;

  if (!ch) return 0;

  account_init_for_char(ch);

  if (!account_load_any(GET_ACCOUNT_ID(ch), &acct)) {
    memset(&acct, 0, sizeof(acct));
    acct.account_id = GET_ACCOUNT_ID(ch);
  }

  for (int i = 0; i < acct.num_chars; i++)
    if (acct.chars[i].char_id == GET_IDNUM(ch))
      return 1;

  if (acct.num_chars >= MAX_CHARS_PER_ACCOUNT)
    return 0;

  acct.chars[acct.num_chars].char_id = GET_IDNUM(ch);
  strlcpy(acct.chars[acct.num_chars].name, GET_NAME(ch),
          sizeof(acct.chars[acct.num_chars].name));
  acct.num_chars++;

  return account_save_any(&acct);
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
  write_to_output(d, "  PASS  Change your account password\r\n");
  write_to_output(d, "  DEL <slot|name>   Delete a character on this account\r\n");
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

int account_find_character_on_roster(const struct account_data *acct, const char *slot_or_name,
                                     int *out_index, char *out_name, size_t out_name_len)
{
  int i, slot;

  if (out_index)
    *out_index = -1;
  if (out_name && out_name_len > 0)
    *out_name = '\0';

  if (!acct || !slot_or_name || !*slot_or_name)
    return 0;

  while (*slot_or_name == ' ' || *slot_or_name == '\t')
    slot_or_name++;
  if (!*slot_or_name)
    return 0;

  if (isdigit((unsigned char)*slot_or_name)) {
    slot = atoi(slot_or_name);
    if (slot < 1 || slot > acct->num_chars)
      return 0;
    i = slot - 1;
    if (i >= MAX_CHARS_PER_ACCOUNT || !acct->chars[i].name[0])
      return 0;
    if (out_index)
      *out_index = i;
    if (out_name && out_name_len > 0)
      strlcpy(out_name, acct->chars[i].name, out_name_len);
    return 1;
  }

  for (i = 0; i < acct->num_chars && i < MAX_CHARS_PER_ACCOUNT; i++) {
    if (!acct->chars[i].name[0])
      continue;
    if (!str_cmp(slot_or_name, acct->chars[i].name)) {
      if (out_index)
        *out_index = i;
      if (out_name && out_name_len > 0)
        strlcpy(out_name, acct->chars[i].name, out_name_len);
      return 1;
    }
  }

  return 0;
}

struct descriptor_data *account_character_in_use_elsewhere(
    const char *name, const struct descriptor_data *current_desc)
{
  struct descriptor_data *d;

  if (!name || !*name)
    return NULL;

  for (d = descriptor_list; d; d = d->next) {
    if (d == current_desc)
      continue;
    if (STATE(d) == CON_CLOSE || STATE(d) == CON_DISCONNECT)
      continue;
    if (d->character && GET_NAME(d->character) &&
        !str_cmp(GET_NAME(d->character), name))
      return d;
    if (d->original && GET_NAME(d->original) &&
        !str_cmp(GET_NAME(d->original), name))
      return d;
  }

  return NULL;
}

int account_character_is_in_use(const char *name)
{
  return account_character_in_use_elsewhere(name, NULL) != NULL;
}

int account_delete_character_data(const char *name)
{
  int i, pfilepos;
  char filename[PATH_MAX];

  if (!name || !*name)
    return 0;

  for (i = 0; i < MAX_FILES; i++) {
    if (!get_filename(filename, sizeof(filename), i, name))
      continue;
    if (unlink(filename) != 0 && errno != ENOENT) {
      mudlog(CMP, LVL_IMPL, TRUE, "SYSERR: Could not remove %s for %s: %s",
             filename, name, strerror(errno));
      return 0;
    }
  }

  delete_variables(name);

  pfilepos = get_ptable_by_name(name);
  if (pfilepos >= 0)
    remove_player(pfilepos);

  return 1;
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
