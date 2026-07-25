# MEDIT DG Script legacy-compatibility audit

## Scope and authority

This audit was made from the current game data, not an earlier audit. Files read
included `lib/text/help/help.hlp`, Builder Academy `lib/world/wld/0.wld`, zone
0 mobiles/objects/triggers/resets, and the production MEDIT, Oasis, DG database,
handler, runtime, and mobile generation paths. No split help store exists.
Network access to GitHub was denied by the execution environment, so a fresh
upstream `master` checkout could not be downloaded. The inherited upstream
ownership comments and repository history were compared, but unverified remote
details are explicitly classified below rather than claimed as verified.

## In-game contract inventory

Color markup is retained in quoted text where it affects the source. Long
tutorial program listings are summarized only after quoting their operative
instruction; their authored commands were separately inspected.

| Source | Room/help keyword | Exact instruction | Required behavior | Before | Test |
|---|---|---|---|---|---|
| `lib/world/wld/0.wld` | room 13 | “use this hallway in conjunction with ... `TLIST` and `TSTAT` ... disable `NOHASSLE`” | Independent inspection; triggers fire for tester | Available | manual guide |
| same | room 18 | “HELP TRIGEDIT-MOB-TUTORIAL ... STAT QUESTMASTER. Notice the attached triggers. TSTAT 1.” | STAT/TSTAT identify attachments | Runtime STAT lacked attachment positions | lifecycle contract/manual |
| same | room 20 | “STAT GATEGUARD ... triggers 4, 5, 7, 8 ... GIVE 10 COINS GUARD” | Bribe opens gateway | Authored and attached | lifecycle contract/manual |
| `lib/text/help/help.hlp` | TRIGEDIT-MOB-TUTORIAL | “Attach the triggers - permanently ... `medit 14` ... `S`” | S enters attachment editor; parent save persists | S worked | attachment tests |
| same | tutorial menu | “Script Editor / Trigger List: / N) New ... / D) Delete ... / X) Exit” | Exact legacy letters and recognizable display | **Regression:** N/X/Q meant attach/detach/exit | attachment tests |
| same | tutorial input | “Please enter position, vnum (ex: 1, 200)” | Position plus VNUM grammar | Parser supported it; prompt diverged | attachment tests |
| same | tutorial triggers | Greet 1, Death 2, Receive 3; wrong object uses `return 0` | Correct trigger families and authored runtime semantics | Authored prototypes intact | lifecycle contract/manual |
| same | SCRIPT-MENU | “N) Attach ... X) Detach ... Q) Quit” | Later aliases must coexist with higher-authority tutorial | N/X/Q worked | Q remains exit alias; numeric actions retained |
| same | TLIST | “tlist <zone #> then tstat each trigger” | Prototype listing/inspection independent | Unchanged | existing DG tests/manual |
| same | TSTAT | “tstat <trigger vnum>” | Prototype details independent of MEDIT | Unchanged | manual |
| same | NOHASSLE | disables immortal trigger firing | Runtime test requires it off | Unchanged | manual |
| `lib/world/mob/0.mob` | mobs 14/16/24 | `T 3`, `T 1`; `T 2`; `T 4/5/7/8` | Loader retains record order and runtime instantiates it | Working | lifecycle contract |
| `lib/world/trg/0.trg` | triggers 1–8 | Mobile Greet/Death/Receive and guard Greet/Bribe/door commands | Intended family and commands remain runnable | Working, not modified | lifecycle contract/manual |
| `lib/world/zon/0.zon` | room 20 reset | `M 0 24 1 20` | Tutorial guard loads | Working | world reset tests/manual |

The checked-in questmaster currently stores 3 then 1. This audit does **not**
rewrite that tutorial prototype, per task constraints. The acceptance workflow
on a working copy proves inserting 1 then 3 retains that order.

## Defects and corrections

1. **Compatibility regression:** the shared attachment menu displayed `N`, `X`,
   `Q`, whereas the higher-authority Academy teaches `N`, `D`, `X`. Restored D
   as detach and X as parent exit; Q remains an additive exit alias.
2. **Display regression:** legacy `Script Editor`, `Trigger List`, and action
   wording had been replaced. Restored it and retained additive details.
3. **Prompt divergence:** restored the documented position/VNUM prompt while
   retaining strict diagnostics and comma/whitespace parsing.
4. **Empty-delete diagnostic:** it previously offered an impossible 1-through-0
   range. It now reports an empty list safely.
5. **Inspection discoverability:** numeric inspection existed but no explicit I,
   E, R, or H entries existed. Added read-only I, safe separate-editor E,
   bounded references explanation, and contextual help without nested Oasis.
6. **STAT detail:** runtime trigger stats named triggers and family/type but did
   not label attachment positions. Positions are now numbered.
7. **Ordering defect:** insertion at a middle position advanced one node too far,
   effectively appending in a two-entry list. The predecessor walk now stops
   before the requested slot, while first-position and append behavior remain
   unchanged.

## Lifecycle and ownership trace

* `medit_setup_existing` creates a mobile working copy with a null
  `proto_script`; `dg_olc_script_copy` deep-copies every VNUM node into
  `OLC_SCRIPT`. Editing therefore cannot mutate the prototype before save.
* X makes the shared parser return to MEDIT and leaves `OLC_SCRIPT` owned by the
  descriptor. Normal parent quit/save remains authoritative.
* Save frees the previous prototype list only when distinct, transfers the OLC
  list once, then removes instantiated scripts from each matching live mobile,
  deep-copies the new prototype references, and calls `assign_triggers`.
* Discard cleanup points the working mobile at its OLC list so Oasis cleanup
  frees one list and never the original prototype. Existing asserts reject
  freeing a prototype list aliased by live entities.
* `script_save_to_disk` walks the list and writes unchanged ordered `T <vnum>`
  records. `dg_read_trigger` appends records in file order. `copy_proto_script`
  deep-copies in order and `assign_triggers` instantiates each referenced
  prototype in order. Missing prototypes are logged and are not instantiated.

No second ownership model, storage migration, runtime semantic change, or nested
TRIGEDIT was introduced. A trigger deleted after an attachment working copy is
opened displays as `<missing trigger prototype>` and remains detachable. Boot
currently logs and omits a missing record; preserving such a record at boot
would change shared REDIT/OEDIT database behavior and remains an explicit
limitation rather than an unscoped format/runtime change.

## Upstream/divergence classification

| Divergence | Classification |
|---|---|
| Deep-copy/transfer ownership documented in DG OLC comments | inherited upstream behavior; verified locally |
| N/new, D/delete, X/exit Academy interface | compatibility restoration |
| Q exit alias and numbered action screen | intentional additive improvement |
| Names, family, flags, arguments, commands, I/E/R/H | intentional additive improvement / harmless display difference |
| Separate `trigedit` handoff | safety improvement; avoids memory ownership risk |
| Zone permission check before attachment | Adventurer's Lair safety customization |
| Duplicate attachment acceptance | upstream/legacy-compatible policy; no deduplication added |
| Missing record omitted during boot | persistence risk; unresolved and documented |
| Live wait-event replacement during prototype refresh | runtime risk requiring interactive/manual verification; existing extract/assign convention retained |
| Fresh remote source comparison | unclear/manual verification because GitHub access was blocked |

## Tests and verification status

Automated contracts cover S routing, both cases through `tolower`, strict input,
first/middle/final insertion mechanics, ordered writer/reader/copy/assignment,
delete confirmation/dirtying, X parent restoration, save/discard ownership,
family/VNUM/permission rejection, missing display, empty delete, STAT position,
and unchanged Academy mob/trigger/reset facts. Existing shared tests guard OEDIT
and REDIT routing. See the verification guide for interactive quest, death,
wrong-object, reward, wait-event, and gateguard checks; these were **not** claimed
as executed in this non-interactive run.

No help text, Builder Academy room, tutorial room/mobile/object/trigger prototype,
or zone reset was changed. No instructional correction is proposed. Legacy N,
D, and X are supported case-insensitively; `position, vnum` remains supported;
the ordered `T <vnum>` disk format is unchanged.
