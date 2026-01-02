#include "conf.h"
#include "sysdep.h"

#include <openssl/evp.h>
#include <openssl/sha.h>
#include <time.h>

#include "password.h"
#include "utils.h"

#define SHA512_PREFIX "$6$"
#define SHA256_PREFIX "sha256$"
#define SALT_CHARS "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789./"
#define SALT_LEN 16

static unsigned int password_rand(void)
{
  static int seeded = 0;

  if (!seeded) {
    srand((unsigned int) time(NULL));
    seeded = 1;
  }

  return (unsigned int) rand();
}

static int sha512_crypt_supported(void)
{
  static int checked = 0;
  static int supported = 0;

  if (!checked) {
    const char *result = CRYPT("circle-password-check", "$6$1234567890abcdef$");
    supported = (result && !strncmp(result, SHA512_PREFIX, 3));
    checked = 1;
  }

  return supported;
}

static void build_salt(char *out, size_t len)
{
  size_t i;
  size_t chars = strlen(SALT_CHARS);

  if (!out || len == 0)
    return;

  for (i = 0; i + 1 < len; i++)
    out[i] = SALT_CHARS[password_rand() % chars];

  out[len - 1] = '\0';
}
static int hash_sha256_fallback(const char *password, const char *salt, char *out, size_t outlen)
{
  unsigned char digest[SHA256_DIGEST_LENGTH];
  char hex[(SHA256_DIGEST_LENGTH * 2) + 1];
  EVP_MD_CTX *ctx = NULL;
  unsigned int digest_len = 0;
  size_t i;

  if (!password || !salt || !out || outlen == 0)
    return 0;

  ctx = EVP_MD_CTX_new();
  if (!ctx)
    return 0;

  if (EVP_DigestInit_ex(ctx, EVP_sha256(), NULL) != 1 ||
      EVP_DigestUpdate(ctx, salt, strlen(salt)) != 1 ||
      EVP_DigestUpdate(ctx, password, strlen(password)) != 1 ||
      EVP_DigestFinal_ex(ctx, digest, &digest_len) != 1 ||
      digest_len != SHA256_DIGEST_LENGTH) {
    EVP_MD_CTX_free(ctx);
    return 0;
  }

  EVP_MD_CTX_free(ctx);

  for (i = 0; i < SHA256_DIGEST_LENGTH; i++)
    snprintf(hex + (i * 2), 3, "%02x", digest[i]);

  hex[sizeof(hex) - 1] = '\0';
  snprintf(out, outlen, "%s%s$%s", SHA256_PREFIX, salt, hex);

  return 1;
}
int password_hash(const char *password, char *out, size_t outlen)
{
  char salt[SALT_LEN + 1];
  char salt_string[64];
  const char *result;

  if (!password || !out || outlen == 0)
    return 0;

  out[0] = '\0';
  build_salt(salt, sizeof(salt));

  if (sha512_crypt_supported()) {
    snprintf(salt_string, sizeof(salt_string), "%s%s$", SHA512_PREFIX, salt);
    result = CRYPT(password, salt_string);
    if (result && !strncmp(result, SHA512_PREFIX, 3)) {
      snprintf(out, outlen, "%s", result);
      return 1;
    }
  }

  return hash_sha256_fallback(password, salt, out, outlen);
}

int password_is_legacy(const char *stored_hash)
{
  if (!stored_hash || !*stored_hash)
    return 1;

  /* New scheme starts with "sha256$". Anything else is legacy crypt(). */
  return strncmp(stored_hash, SHA256_PREFIX, strlen(SHA256_PREFIX)) != 0;
}
static int __attribute__((unused)) verify_sha256_hash(const char *password, const char *stored_hash)
{
  const char *salt_start = stored_hash + strlen(SHA256_PREFIX);
  const char *second_dollar;
  char salt[SALT_LEN + 1];
  char candidate[MAX_PWD_HASH_LENGTH + 1];
  size_t salt_len;

  second_dollar = strchr(salt_start, '$');
  if (!second_dollar)
    return 0;

  salt_len = (size_t)(second_dollar - salt_start);
  if (salt_len == 0 || salt_len >= sizeof(salt))
    return 0;

  memcpy(salt, salt_start, salt_len);
  salt[salt_len] = '\0';

  if (!hash_sha256_fallback(password, salt, candidate, sizeof(candidate)))
    return 0;

  return strcmp(candidate, stored_hash) == 0;
}

int password_verify(const char *password, const char *stored_hash,
                    char *upgrade_out, size_t upgrade_len, int *upgraded)
{
  if (upgraded)
    *upgraded = 0;

  if (!password || !stored_hash || !*stored_hash)
    return 0;

  /* New format: sha256$<salt>$<hex> */
  if (!strncmp(stored_hash, SHA256_PREFIX, strlen(SHA256_PREFIX))) {
    const char *salt_start = stored_hash + strlen(SHA256_PREFIX);
    const char *salt_end = strchr(salt_start, '$');
    char salt[64];
    char computed[256];

    if (!salt_end)
      return 0;

    size_t slen = (size_t)(salt_end - salt_start);
    if (slen == 0 || slen >= sizeof(salt))
      return 0;

    memcpy(salt, salt_start, slen);
    salt[slen] = '\0';

    if (!hash_sha256_fallback(password, salt, computed, sizeof(computed)))
      return 0;

    return strcmp(computed, stored_hash) == 0;
  }

  /* Legacy format: crypt(3). Classic DES crypt uses only first 8 chars. */
  const char *c = crypt(password, stored_hash);
  if (!c)
    return 0;

  if (strcmp(c, stored_hash) != 0)
    return 0;

  /* Successful legacy login: upgrade to sha256$ if caller supplied buffer. */
  if (upgrade_out && upgrade_len) {
    if (!password_hash(password, upgrade_out, upgrade_len))
      return 0;
    if (upgraded)
      *upgraded = 1;
  }

  return 1;
}


