# Control system validation report

Authoritative checkout: `C:\Users\antho\Desktop\TBAMUD\TBAMUD UPDATED`.
Branch: `main`. HEAD remains `9262124e2a3b82d36f639104c45a5f19b317c984`.
No commit or push. Existing social additions and world work were preserved.
Detailed audit and policies: [control_system.md](control_system.md).

## A. Existing Switch Architecture

Audited SWITCH/RETURN, input dispatch, aliases, account reconnect, NPC activity,
combat, charm/order/followers, descriptor output, death/extraction/free,
disconnect, shutdown, and copyover. Factored `switch_to_char` from the existing
NPC transfer. PC possession uses an explicit observer relationship instead.
Fixed the stale `och` reference after returning a switched body during copyover.
The existing legacy SWITCH DG privilege path is excluded from PUPPET.

## B. Immortal Puppet

`puppet <npc>`: level 101+, visible world target, existing godroom/house rules.
Guaranteed for a structurally eligible unoccupied NPC; no mana, save, or expiry.
The original body retains its room; viewpoint and actions use the NPC.
`unpuppet` / exact `return`, extraction, death, disconnect, and transitions end it.
No nested relationships or inherited immortal level/administrative permissions.

## C. Player Puppet

Spell 226, ordinary knowledge, mana, concentration, wait, and environment rules.
NPC duration `10 + floor(50*P/100)` seconds; PC `3 + floor(27*P/100)` seconds.
An existing paralyzation save plus a second 5..85% bounded mental/level score
follows the normal casting proficiency roll. Exact formula and class levels
are in the implementation document. Any positive post-mitigation damage to the
abandoned caster body breaks Puppet; environmental self-damage still applies.

## D. Player-vs-Player Puppet

Victim descriptor/account ownership stays intact. Caster views the victim;
body output is mirrored only while both participants are playing with valid
ownership links. Victim action input is suppressed, including chat. Account/menu
output is not mirrored. Compelled verbs are limited to movement, inspection,
speech/socials, and basic combat. No casting, inventory transfer, banking, mail,
account edits, save/rent/quit, aliases, OLC, or administration.

## E. Mind Control

Spell 227. Caster stays in their own body. `order <target> <command>` compels a
local bound target through the restricted interpreter; follower lists are not
modified. NPC duration `15 + floor(75*P/100)` seconds; PC duration is at most 30.
Uses the same mental resistance and mutual exclusion checks. Damage alone does
not end this binding; expiry, release, death, extraction and disconnect do.

## F. Control Immunity

MOB_NOCHARM, MOB_NOKILL, MOB_SPEC, assigned special functions, sanctuary,
immortal targets, targets more than five levels above the caster, existing
control/charm, existing masters, and target follower relationships protect
against the player spells. No VNUM blacklist. The high-level saving-table
fallthrough found during testing is fixed by retaining the last common defined
save tier (40). This shared lookup fix also affects other high-level spells;
high-level targets now retain that tier's strong resistance instead of the
previous erroneous fallthrough.

## G. PvP / Peaceful Rules

Player targets require CONFIG_PK_ALLOWED and PRF_SUMMONABLE. Peaceful rooms
block both player spells. Compelled hit/damage and physical-skill eligibility
also enforce PvP restrictions so NPC possession cannot bypass them. Existing
ordinary combat and spell environment rules remain responsible for other
restrictions. Environmental self-damage is exempt from the hostile-target guard.

## H. Extraction / Death / Disconnect

One teardown owner: `end_control_session`. It handles DG event cancellation,
both character links, descriptor restoration and stale input, with deferred
freeing during reentrant command execution. Entry points cover extraction,
free_char, die/raw_kill, close_socket, reconnect, copyover and shutdown.
Runtime tests include actual raw_kill, extract_char, free_char, and both PC
socket-disconnect paths. Actual copyover exec/recovery and account-menu journeys
were source-audited; they were not exercised end-to-end with authenticated users.

## I. Socket Command

`socket [player|descriptor-id] [full]`, level 104 in one's own unbound PC body.
Shows real descriptor ID, owner, state label, room, account name, connection
age, idle mud ticks, and control/observer relationships. Default host is fully
redacted; explicit FULL reveals the stored host only at Implementor privilege.
No passwords, hashes, tokens, fabricated attributes or pointer addresses.
Login/missing-character descriptors are covered. Existing USERS is unchanged.

## J. Automated Tests

`python3 tests/control_runtime_test.py`: **1026 assertions passed**.
Links the actual engine and recompiles control/communication with AddressSanitizer
and UBSan. No mocked control, command, event, output, protocol, movement, or spell
implementation. Address/UB checks pass. Leak reporting is disabled for the
intentionally retained synthetic-world fixture.

Coverage includes registration; NPC viewpoint/movement/speech/emote; PC mirrored
output and observer suppression; sensitive-menu non-mirroring; both duration
caps and scaling; resistance success/failure; immunity; mana and wait;
forbidden commands and abbreviations; PvP; environmental damage; frozen caster
escape; event expiry; NPC death; extraction/free; both PC disconnects; queue
clearing; and default/full/missing-character socket inspection.

## K. Existing Regression Suites

32 existing scripts run; **31 pass, 1 preexisting data-count failure**.
The separate compiler-warning script's exact clean-build gate was executed
directly, as recorded below. No existing tests were changed or weakened.

`xp_reward_audit_test.py:33` expects 3,736 mobs. The current world contains
3,769. Its source and parser are unchanged, and the world edits predate this
feature. Its preceding calculation/source contracts pass.

| Script | Result |
|---|---|
| ability_table_formatter_test.py | PASS |
| account_character_persistence_test.py | PASS |
| admin_world_systems_regression_test.py | PASS |
| ambidextrous_weapon_test.py | PASS |
| buff_command_regression_test.py | PASS |
| builder_academy_dg_compatibility_test.py | PASS |
| builder_references_test.py | PASS |
| combat_round_prompt_regression_test.py | PASS |
| deldir_command_test.py | PASS |
| dg_attachment_workflow_test.py | PASS |
| dg_menu_contract_test.py | PASS |
| dg_trigedit_workflow_test.py | PASS |
| dg_trigger_capability_matrix_test.py | PASS |
| dg_trigger_roundtrip_test.py | PASS |
| dual_wield_class_effectiveness_test.py | PASS |
| equipment_sanctuary_boot_regression_test.py | PASS |
| immortal_push_command_test.py | PASS |
| last_room_persistence_regression_test.py | PASS |
| medit_dg_legacy_contract_test.py | PASS |
| oedit_weapon_hand_flags_test.py | PASS |
| phase5_shared_combat_helpers_test.py | PASS |
| prompt_spacing_regression_test.py | PASS |
| qol_admin_phase1_test.py | PASS |
| resource_pool_regression_test.py | PASS |
| showvnums_color_boundary_test.py | PASS |
| stat_prototype_lookup_test.py | PASS |
| two_handed_weapon_test.py | PASS |
| unarmed_proficiency_test.py | PASS |
| weapon_command_unification_test.py | PASS |
| weapon_swap_numbered_target_test.py | PASS |
| world_reset_integrity_test.py | PASS |
| xp_reward_audit_test.py | FAIL: existing world-count expectation |

## L. Build

Executed in the authoritative root:

```sh
make -C src clean
make -C src MYFLAGS="-std=gnu17 -Wall -Wno-char-subscripts -Wno-unused-but-set-variable" -j2
```

Final exit 0. **0 compiler errors, 0 warnings, 0 linker errors.**

## M. Boot

Isolated copied library, loopback-only listener, quick boot. Zones, rooms, mobs,
objects, triggers, player index, spells, help and command initialization completed.
857 commands rebuilt. Entered game loop and returned a socket login greeting.
**0 initialization SYSERRs.** The harness then sent SIGTERM; the engine's expected
signal-shutdown message is labelled SYSERR in the log and is not a boot failure.
The subsequent final rebuild adds only the environmental-damage eligibility fix;
it changes no boot data or initialization. The final binary and engine tests
pass. The production server was not restarted.

## N. Live Verification

Puppet: **NOT VERIFIED LIVE with authenticated sessions**.
Mind Control: **NOT VERIFIED LIVE with authenticated sessions**.
Socket: **NOT VERIFIED LIVE with authenticated sessions**.
The actual functions and real descriptor buffers are covered by linked-engine
tests; network boot/greeting is verified separately. No production players were
used or controlled.

## O. World Files

Newly modified by this feature: **0**. Production world hashes are unchanged
through final validation. The world modifications listed below were already
present before this feature. The nested tracked source directory has no diff.
Feature-scoped `git diff --check` passes. Repository-wide check reports only the
preexisting whitespace at `lib/world/obj/12.obj:12`.

## P. Ending Git Status

Complete `git status --short`:

```text
 M lib/misc/socials
 M lib/misc/socials.new
 M lib/text/help/help.hlp
 M lib/world/mob/12.mob
 M lib/world/mob/343.mob
 M lib/world/mob/346.mob
 M lib/world/mob/37.mob
 M lib/world/obj/12.obj
 M lib/world/obj/30.obj
 M lib/world/obj/343.obj
 M lib/world/obj/346.obj
 M lib/world/obj/37.obj
 M lib/world/trg/12.trg
 M lib/world/trg/343.trg
 M lib/world/trg/346.trg
 M lib/world/trg/37.trg
 M lib/world/wld/12.wld
 M lib/world/wld/343.wld
 M lib/world/wld/346.wld
 M lib/world/wld/37.wld
 M lib/world/zon/12.zon
 M lib/world/zon/343.zon
 M lib/world/zon/346.zon
 M lib/world/zon/37.zon
 M src/act.offensive.c
 M src/act.wizard.c
 M src/class.c
 M src/comm.c
 M src/db.c
 M src/fight.c
 M src/handler.c
 M src/interpreter.c
 M src/limits.c
 M src/mobact.c
 M src/spell_parser.c
 M src/spells.c
 M src/spells.h
 M src/structs.h
?? builder_scan_output/
?? builder_source_contract_scan.ps1
?? cleanup_backups/
?? docs/control_system.md
?? docs/control_validation_report.md
?? lib/world/zon/32.zon.pre_z32_driftwood_cleanup_20260902_032718.bak
?? src/control.c
?? src/control.h
?? src/spec_assign.c.pre_board_cleanup
?? tests/control_runtime_test.c
?? tests/control_runtime_test.py
```

Complete untracked-file listing (`git ls-files --others --exclude-standard`):

```text
builder_scan_output/builder_source_scan_20260903_011325.zip
builder_scan_output/builder_source_scan_20260903_011325/README_FIRST.txt
builder_scan_output/builder_source_scan_20260903_011325/builder_case_blocks.csv
builder_scan_output/builder_source_scan_20260903_011325/builder_choice_tables.csv
builder_scan_output/builder_source_scan_20260903_011325/builder_command_contract.json
builder_scan_output/builder_source_scan_20260903_011325/builder_command_contract.txt
builder_scan_output/builder_source_scan_20260903_011325/builder_command_registrations.csv
builder_scan_output/builder_source_scan_20260903_011325/builder_menu_entries.csv
builder_scan_output/builder_source_scan_20260903_011325/builder_mode_definitions.csv
builder_scan_output/builder_source_scan_20260903_011325/builder_mode_transitions.csv
builder_scan_output/builder_source_scan_20260903_011325/builder_multiline_editors.csv
builder_scan_output/builder_source_scan_20260903_011325/builder_prompts.csv
builder_scan_output/builder_source_scan_20260903_011325/builder_risk_candidates.csv
builder_scan_output/builder_source_scan_20260903_011325/builder_save_paths.csv
builder_scan_output/builder_source_scan_20260903_011325/builder_source_bundle.txt
builder_scan_output/builder_source_scan_20260903_011325/builder_source_manifest.csv
builder_scan_output/builder_source_scan_20260903_011325/builder_state_transitions.dot
builder_source_contract_scan.ps1
cleanup_backups/deleted_zone30_refs_20260901_175553/lib_world_zon_12.zon
cleanup_backups/deleted_zone30_refs_20260901_175553/lib_world_zon_2.zon
cleanup_backups/deleted_zone30_refs_20260901_175553/lib_world_zon_232.zon
cleanup_backups/deleted_zone30_refs_20260901_175553/lib_world_zon_30.zon
cleanup_backups/deleted_zone30_refs_20260901_175553/src_boards.c
cleanup_backups/deleted_zone30_refs_20260901_175553/src_spec_assign.c
cleanup_backups/drink_system_20260901_180423/src_act.item.c
cleanup_backups/drink_system_20260901_180423/src_db.c
cleanup_backups/drink_system_v2_20260901_180553/src_act.item.c
cleanup_backups/drink_system_v2_20260901_180553/src_db.c
cleanup_backups/empty_drink_stat_20260901_182507/src_act.wizard.c
cleanup_backups/runtime_specials_20260901_181110/src_boards.c
cleanup_backups/runtime_specials_20260901_181110/src_boards.h
cleanup_backups/runtime_specials_20260901_181110/src_db.c
cleanup_backups/zone31_southwatch_20260901_193350/31.zon
docs/control_system.md
docs/control_validation_report.md
lib/world/zon/32.zon.pre_z32_driftwood_cleanup_20260902_032718.bak
src/control.c
src/control.h
src/spec_assign.c.pre_board_cleanup
tests/control_runtime_test.c
tests/control_runtime_test.py
```

Complete tracked diff statistics (includes preexisting work):

```text
 lib/misc/socials       |   10 +
 lib/misc/socials.new   |   15 +
 lib/text/help/help.hlp |   46 ++
 lib/world/mob/12.mob   |  302 +++++----
 lib/world/mob/343.mob  |  142 ++---
 lib/world/mob/346.mob  |  662 +++++++++++++++++++-
 lib/world/mob/37.mob   |  300 ++++++---
 lib/world/obj/12.obj   |  214 ++++---
 lib/world/obj/30.obj   |   22 +-
 lib/world/obj/343.obj  |  551 +++++++++-------
 lib/world/obj/346.obj  |  454 ++++++++++++--
 lib/world/obj/37.obj   |  116 ++--
 lib/world/trg/12.trg   |  588 +++++------------
 lib/world/trg/343.trg  |  199 +-----
 lib/world/trg/346.trg  | 1635 ++++++++++++++++++++++++++++++++++++++++++++++++
 lib/world/trg/37.trg   |  137 ++++
 lib/world/wld/12.wld   |  157 +++--
 lib/world/wld/343.wld  |  937 +++++++++++++--------------
 lib/world/wld/346.wld  | 1023 +++++++++++++++++++++---------
 lib/world/wld/37.wld   |  769 +++++++++++++----------
 lib/world/zon/12.zon   |   31 +-
 lib/world/zon/343.zon  |  508 +++++++--------
 lib/world/zon/346.zon  |  247 +++-----
 lib/world/zon/37.zon   |  141 ++---
 src/act.offensive.c    |    8 +-
 src/act.wizard.c       |   29 +-
 src/class.c            |    9 +
 src/comm.c             |   31 +-
 src/db.c               |    2 +
 src/fight.c            |    8 +
 src/handler.c          |    5 +
 src/interpreter.c      |   25 +-
 src/limits.c           |    2 +
 src/mobact.c           |    2 +
 src/spell_parser.c     |    9 +
 src/spells.c           |    6 +
 src/spells.h           |    4 +-
 src/structs.h          |    1 +
 38 files changed, 6325 insertions(+), 3022 deletions(-)
```

Implementation files added: `src/control.c`, `src/control.h`.
Tests added: `tests/control_runtime_test.c`, `tests/control_runtime_test.py`.
Documentation added: this report and `docs/control_system.md`.
Tracked changes integrate the subsystem and add help; details are in the diff.
Validation logs are stored outside the repository in the task's visualization
workspace under `control-validation`.
