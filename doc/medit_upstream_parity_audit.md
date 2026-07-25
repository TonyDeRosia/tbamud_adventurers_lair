# MEDIT upstream parity audit

## Scope and baseline

This audit compares Adventurer's Lair with the official TBA MUD `master` design in
`src/medit.c`, `src/dg_olc.c`, `src/dg_olc.h`, `src/oasis.h`, `src/oedit.c`,
`src/redit.c`, `src/spec_assign.c`, `src/spec_procs.c`, and
`src/spec_procs.h`.  It is a behavioral audit, not a proposal to replace custom
files.  The classic editor remains authoritative; custom fields, persistence,
world formats, DG Scripts, loadouts, and AI extensions remain intact.

The upstream main screen presents: 1 sex, 2 keywords, 3 short description, 4
long description, 5 detailed description, 6 position, 7 default position, 8
attack, 9 stats, A NPC flags, B affect flags, S scripts, W copy, X delete, and Q
quit.  Its parser assigns a field mode, enters the string editor for the detailed
description, delegates scripts to the shared DG parser, and returns to the main
screen after a completed scalar edit.  Q either cleans up an unchanged edit or
asks Y/N to save a dirty edit.  Alphabetic main-menu and confirmation choices
accept both cases; nested flag/numeric screens retain their classic numeric
grammar.

## Findings

| Feature | Official upstream behavior | Current custom behavior | Intentional custom difference | Bug or acceptable difference | Action required |
|---|---|---|---|---|---|
| Main layout | Numbered classic fields plus A/B/S/W/X/Q | Same sequence, with P/R/C/I/V additions | Pet Price, Loadout/Loot, Special, AI extensions, and preview are additive | Acceptable | Preserve layout |
| MEDIT modes | Named constants in `oasis.h`; main, strings, flags, stats, numeric fields, confirm | Retains those and adds loadout, AI, and read-only legacy modes | Historical AI modes remain compiled | Acceptable; reachability must be bounded | Contract tests classify and guard roots |
| String transitions | Keyword/short/long use field modes; detailed description enters `string_write` and dirties | Same | Custom maximums/data remain | Acceptable | None |
| Numeric transitions | Modes parse numeric values, clamp/validate, then redraw parent | Same classic flow, with stricter complete-number pre-parser and custom stats | Additional custom numeric fields | Acceptable | Keep DG mode outside generic numeric parsing |
| NPC flags | Numeric toggles, zero returns; redisplays flag menu | Same canonical editor, plus authority help | Legacy flags explicitly identified as owners | Acceptable | None |
| AFF flags | Numeric toggles and removal of invalid NPC affects | Same | None material | Acceptable | None |
| Positions | Numeric selection returns to main | Same | None | Acceptable | None |
| Attack type | Numeric selection returns to main | Same | Custom attack table retained | Acceptable | None |
| Stats | Submenu owns level, combat values, attributes, saves, gold/exp | Recognizable submenu with custom values/autofill | Project-specific statistics | Acceptable | None |
| DG entry | S enters shared `dg_script_menu`; shared parser owns nested state | Same for MEDIT/OEDIT/REDIT, plus numeric inspection | Read-only inspection is custom | One malformed numeric-selection bug found | Require complete numeric input; test shared routing |
| DG N/X/Q | Case-insensitive attach, detach, return | Uses `tolower`, correct parent redraw on parser return 0 | None | Acceptable | Contract-test both displayed cases |
| DG dirty state | Successful list mutation increments `OLC_VAL`; cancel/view does not | Same | Inspection is explicitly read-only | Acceptable | Guard no dirty write in inspection |
| DG inspection | No upstream read-only numbered inspection | Number opens structural/custom metadata view | Conservative quality-of-life feature | Acceptable after command-backed metadata check | Show all structural fields; accept only Q in nested view |
| Parent restoration | Shared parser returns 0; each editor redraws its own parent | MEDIT/OEDIT/REDIT each do so | None | Acceptable | Test all three routes |
| Copy/delete/quit | W copy, X delete confirmation, Q exit | Same | None material | Acceptable | None |
| Save confirmation | Dirty Q asks Y/N; clean Q exits; scripts transferred safely on no-save cleanup | Same | Custom fields participate in normal copy/save | Acceptable | Preserve full AI config copying |
| Dirty handling | Scalar/string mutation and successful script mutation dirty; display/cancel does not | Same intended contract | Preview/special/AI notices are read-only | Acceptable | Contract tests |
| Case handling | Main alphabetic choices and Y/N accept upper/lower; relevant DG letters are case-insensitive | Same; C/I/V are also paired cases | New commands follow classic convention | Acceptable | Menu contract tests |
| Special assignment | `ASSIGNMOB(3105, mayor)` installs the actual `mayor` function pointer | Metadata lookup compares `GET_MOB_SPEC()` with registered function pointers | Read-only metadata view | Correct; mob names are notice-only | Retain false-Mayor warning |

## Special and world-data verification

The local assignment remains `ASSIGNMOB(3105, mayor)` and the implementation is
`SPECIAL(mayor)`.  Special metadata is selected by pointer equality.  Mob 24201's
name contains Mayor but it has no assignment; its name is used only to produce a
warning.  Mob 3105 receives the pointer and therefore exposes the Mayor schedule,
route, speeches, gate/door actions, posture changes, wake/sleep cycle, and waits.

Mob 3068 attaches trigger 3011.  The record is named `Stock Janitor - 3061,
3068`, is a mobile random trigger (`b`) with numeric argument 100 and an empty
argument phrase.  Its commands iterate room contents and execute `take` only for
non-fountain objects costing at most 15 gold.  “Object interaction” is therefore
supported by commands rather than inferred from the name.  Other triggers remain
`Unknown / arbitrary DG Script` unless explicit metadata is authored.

## Historical AI mode reachability

| Mode family | Classification | Reason |
|---|---|---|
| Personality, identity/role, perception | Reachable from current primary MEDIT | The only numbered AI extension entries |
| Preview and diagnostics | Reachable, read-only | Documented P/D commands |
| Communication/dialogue/social/memory | Persistence compatibility only | Parser/display code remains, but no primary transition enters it |
| Schedule/routine/patrol/movement | Persistence and runtime compatibility only | Data loaders and runtime arbitration consume saved data; primary menu has no transition |
| Combat/threat/target/flee/assist/scavenge | Runtime/persistence compatibility only | Legacy flags and specials remain authoritative; no duplicate primary entry |
| Advanced/capabilities/ownership and old behavior pages | Dead/unreachable from the current root | Retained source modes have no root command and malformed/default input redraws the root |
| Legacy Behavior menu | Dead/unreachable compatibility code | L is absent from MEDIT; C and V are direct read-only entries |

No accidentally reachable historical family was found.  This conclusion is
guarded at the primary parser transition boundary rather than by deleting data or
runtime code.

## Remaining intentional differences

Adventurer's Lair has additional statistics, Pet Price, Loadout/Loot, persistent
AI data, direct special inspection, effective preview, and numbered read-only DG
inspection.  It also retains historical parser modes so existing serialized AI
records remain safe.  These are intentional extensions; none changes the shared
DG parser ownership or replaces legacy flags, specials, services, and scripts.
