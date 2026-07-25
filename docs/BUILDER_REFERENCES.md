# Builder Cross-References

## Purpose and read-only guarantee

Builder References is the Oasis equivalent of an IDE's **Find References**.
It reports relationships already present in the in-memory world and never writes
prototype, zone/reset, shop, or trigger files. Opening or leaving a reference
screen does not set an editor's changed flag.

## Relationship model

The authoritative service returns `builder_reference` records rather than
formatted strings. Each record identifies the source type and VNUM, display
name, relationship, optional location VNUM, and authoritative source. The thin
Oasis screens only render those records.

Currently indexed relationships are:

* mobile spawns, reset zones, shops, and prototype trigger attachments;
* object zone loads, mob equipment, container resets, room loads, and prototype
  trigger attachments;
* room zone membership, incoming/outgoing exits, mob/object resets, shops, and
  prototype trigger attachments;
* reverse trigger attachments from mobile, object, and room prototypes and zone
  reset attachment commands;
* zone contents (rooms, mobiles, objects, and triggers) and cross-zone exits.

Only concrete prototype tables, exits, reset commands, trigger attachment lists,
and shop tables are inspected. Trigger command text is deliberately not parsed,
so a word that resembles a VNUM is never reported as a dependency.

## Indexing, invalidation, and performance

The first query builds one process-local reverse index. Later editor menu visits
filter that index and do not rescan the world. Duplicate records with the same
target, source, relationship, and location are suppressed while building.

MEDIT, OEDIT, REDIT, TRIGEDIT, and ZEDIT saves invalidate the index. The next
query rebuilds it from the newly authoritative in-memory data. The generation
counter is exposed for regression tests and diagnostics. Merely querying and
freeing results does not change the generation or any Oasis dirty state.

## Extension points and current limits

New editors should call `builder_refs_find()` and render its structured result,
or use the common compact renderer. Add new relationships only in
`builder_refs.c`; do not scan world data in an editor.

Quest relationships are not indexed because this codebase does not expose one
uniform authoritative quest-to-prototype relationship table. References found
only inside free-form DG command text are also unsupported by design. Special
procedure implementation/source metadata remains available through the existing
MEDIT Special Procedure inspector; a stable registry of reverse special-proc
assignments is a future extension.
