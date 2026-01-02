/* ************************************************************************
*  file:  asciipasswd.c (derived from mudpasswd.c)         Part of tbaMUD *
*  Usage: generating hashed passwords for an ascii playerfile.            *
*  Copyright (C) 1990, 1991 - see 'license.doc' for complete information. *
*  All Rights Reserved                                                    *
************************************************************************* */


#include "conf.h"
#include "sysdep.h"

#include "structs.h"
#include "utils.h"
#include "password.h"


char *CAP(char *txt) {
  *txt = UPPER(*txt);
  return (txt);
}

int main(int argc, char **argv) {
  char hash[MAX_PWD_HASH_LENGTH + 1];

  if (argc != 3) {
    fprintf(stderr, "Usage: %s name password\n", argv[0]);
    return (1);
  }

  if (strlen(argv[2]) < MIN_PWD_LENGTH || strlen(argv[2]) > MAX_PWD_LENGTH) {
    fprintf(stderr, "Password must be between %d and %d characters.\n", MIN_PWD_LENGTH, MAX_PWD_LENGTH);
    return (1);
  }

  if (!password_hash(argv[2], hash, sizeof(hash))) {
    fprintf(stderr, "Failed to hash password.\n");
    return (1);
  }

  printf("Name: %s\nPass: %s\n", CAP(argv[1]), hash);
  return (0);
}

