#ifndef PASSWORD_H
#define PASSWORD_H

#include <stddef.h>
#include "structs.h"

int password_hash(const char *password, char *out, size_t outlen);
int password_verify(const char *password, const char *stored_hash, char *upgrade_out, size_t upgrade_len, int *upgraded);
int password_is_legacy(const char *stored_hash);

#endif /* PASSWORD_H */
