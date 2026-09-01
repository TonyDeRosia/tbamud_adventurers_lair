#ifndef BUILDER_REFS_H
#define BUILDER_REFS_H

#include <stddef.h>

enum builder_ref_type { BREF_MOB, BREF_OBJECT, BREF_ROOM, BREF_TRIGGER,
  BREF_ZONE, BREF_SHOP, BREF_SPECIAL };

struct builder_reference {
  enum builder_ref_type type;
  int vnum;
  const char *display_name;
  const char *relationship;
  int location_vnum;
  const char *source;
};

struct builder_reference_list {
  struct builder_reference *entries;
  size_t count;
};

struct builder_reference_list builder_refs_find(enum builder_ref_type type, int vnum);
void builder_refs_free(struct builder_reference_list *list);
void builder_refs_invalidate(void);
unsigned long builder_refs_generation(void);
void builder_refs_display(struct descriptor_data *d, enum builder_ref_type type,
                          int vnum, const char *title);

#endif
