# MEDIT legacy-first correction work log

## Audit (2026-07-25)

- `MEDIT_MAIN_MENU` still exposes the canonical NPC/AFF flags, positions, attack,
  stats, loadout, and DG Script editors, but the new `I` entry opens a broad
  “NPC Behavior” hierarchy that edits several of those same legacy flag domains.
- `MEDIT_BEHAVIOR_MOVEMENT`, `MEDIT_BEHAVIOR_COMBAT`, and
  `MEDIT_BEHAVIOR_OBJECTS` duplicate canonical NPC flag controls and also lead to
  AI schedules, movement, combat, memory, assistance, and scavenging controls.
- `legacy_behavior.c` already maps real special function pointers and reports
  shops, quests, flags, and DG Scripts.  Its metadata lacks builder descriptions
  and its output is primarily technical rather than a normal MEDIT workflow.
- The Mayor is assigned by VNUM 3105 in `assign_mobiles()`; names and descriptions
  are not assignment sources and must never be used to identify its special.
- DG Script `q` returns zero from `dg_script_edit_parse()`.  MEDIT then falls out
  of the nested-mode case and reaches its generic invalid-input redisplay rather
  than explicitly restoring `MEDIT_MAIN_MENU`.  OEDIT and REDIT use the same
  fragile return pattern.
- AI ownership metadata exists, but compatibility-mode labels and broad AI
  editor links make AI look parallel to legacy behavior.  The main correction
  can reuse the metadata and diagnostics without changing runtime dispatch or
  world-file persistence.

## Implementation plan

1. Add a dedicated legacy behavior menu, special detail view, effective preview,
   and source diagnostics based only on function pointers, canonical flags,
   attached scripts, and existing legacy service data.
2. Keep flag-backed edits in that menu wired directly to the existing NPC flag
   bits; make the classic MEDIT main screen expose `L` and label `I` as optional
   AI Actor extensions.
3. Enrich the central special catalog with purpose, capabilities, editability,
   and safe-combination/domain ownership information, including a read-only
   Mayor explanation.
4. Reduce the reachable AI menu to additive controls and clearly lock domains
   owned by flags, specials, or DG Scripts.  Preserve old configuration and
   implementation code for file compatibility.
5. Explicitly restore each parent editor after DG Script `q`/`Q`, without setting
   the modified flag, and add source-level regression coverage.
6. Build, run focused and existing regression tests, inspect representative world
   prototypes/assignments, and document remaining metadata and ownership limits.
