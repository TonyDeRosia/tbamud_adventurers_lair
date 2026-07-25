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

## Focused correction audit (2026-07-25, before implementation)

Manual testing of `medit 3068` found that the displayed DG command `N) Attach
trigger` rejected lowercase `n` with `Try again :`.  The shared DG parser itself
normalizes `N`, `X`, and `Q` with `tolower()`, but MEDIT's generic numeric
pre-parser runs first for `OLC_SCRIPT_EDIT`; consequently **both displayed N/n
and X/x commands disagree with actual MEDIT routing** and never reach the shared
parser.  Q/q happened to work only because an earlier isolated exception/path
masked the larger routing problem.  OEDIT and REDIT do reach the shared parser,
but their displayed numeric rows have no inspection route: numeric input merely
redisplays the menu.  Across all three editors the displayed numeric trigger
choices therefore disagree with parser behavior.

The primary menu also exposes `L) Legacy Behavior`, whose numbered movement,
combat, memory, helper, fleeing, and scavenging controls duplicate the
canonical NPC Flags editor.  The reachable AI implementation retains broad
historical editors, but its current top-level menu only links additive entries;
this correction will remove the legacy abstraction entry and route direct,
read-only special and effective-preview views from classic MEDIT instead.

Implementation will (1) exempt the complete shared DG mode from MEDIT's numeric
pre-parser, (2) make every displayed DG command and numeric row work through the
shared parser without dirtying on inspection, (3) restore each editor parent on
Q/q, (4) add conservative trigger metadata, (5) add contextual canonical-flag
help without changing bit storage, and (6) expose actual-pointer special
inspection and a read-only effective preview directly from classic MEDIT.

## DG attachment workflow audit (2026-07-25)

- The authoritative DG prototype command is `trigedit`, registered at
  `LVL_BUILDER`/`POS_DEAD` and dispatched to `do_oasis_trigedit`. `tedit` is a
  separate god-level text-file editor and is not the DG trigger editor.
- The command handler requires a player with a descriptor in `CON_PLAYING`, a
  numeric in-range VNUM, no concurrent `CON_TRIGEDIT` session for that VNUM, a
  resolvable zone, and `can_edit_zone` ownership. The interpreter provides the
  builder-level command gate.
- Entry allocates the descriptor's sole `oasis_olc_data`, calls
  `trigedit_setup_existing` (or `trigedit_setup_new` for a new VNUM), and enters
  `CON_TRIGEDIT`. Saves use `trigedit_save`, then the existing zone `.trg`
  writer/index path.
- Nested OLC is not safe: `do_oasis_trigedit` finds an existing `d->olc`, logs
  an error, and frees it before allocating the trigger editor state. That would
  destroy an active MEDIT/OEDIT/REDIT working copy. The selected attachment
  screen therefore uses a safe, explicit handoff that shows
  `trigedit <vnum>` and requires the builder to save/discard and leave the
  parent first. Running that command normally preserves all permission, zone,
  state, and concurrency checks; merely viewing the handoff does not dirty the
  parent.
- Attachment duplicates remain allowed, matching the pre-existing linked-list
  policy. The new prompt states this explicitly. Validation now completes
  before linked-list mutation, and detachment mutates only after confirmation.
