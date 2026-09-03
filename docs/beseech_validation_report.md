# BESEECH implementation and validation

Implemented `beseech <message>` in `src/act.comm.c` as `do_beseech`, declared in `src/act.h` and registered in `src/interpreter.c` at level 1 / POS_RESTING. Resting, sitting, fighting, and standing are allowed. NPC senders are rejected. No DG primitive was added; possession's existing compelled-command restrictions remain in effect.

## Delivery and security

The audience is descriptor_list entries in CON_PLAYING with a character and a non-NPC connection owner at GET_LEVEL >= LVL_IMMORT (101). The owner is original when switched, otherwise character. The sending descriptor is excluded from the audience to avoid a duplicate echo. Immortals can send normally from their own bodies. No mortal bystander receives the appeal, regardless of room, group, or channel preferences. Switched immortals receive directly at their own connection, never through another player's body descriptor.

Sender: `You beseech the immortals, 'message'`. With PRF_NOREPEAT, CONFIG_OK instead. Empty input: `Beseech what?`. Staff: `[Beseech] Name beseeches: 'message'`. Text uses printf %s arguments, with no act-token expansion or speech triggers. Output is plain text with the existing protocol handling.

Added `write_to_connection` in comm.c/comm.h: it shares the existing output buffering and protocol implementation but omits possession mirroring. Normal write_to_output/vwrite_to_output still mirror body output as before. Authorized staff snooping remains in the existing input/output processing. A regression simulates an immortal target controlled by a mortal, including promotion during possession, and proves the private appeal never reaches the mortal driver.

No BESEECH messages enter any history, including sender history and HIST_ALL. Tests inspect all nine history slots and invoke HISTORY ALL on another mortal after a unique secret marker. There is no new history index or replay command.

## Restrictions and architecture audit

PLR_NOSHOUT is the actual staff channel mute used by the existing general channels and staff punishment command; it blocks BESEECH. AFF_SILENCED also blocks it. Frozen characters below LVL_IMPL are blocked consistently with the interpreter's existing exception. Sender soundproof rooms block levels below LVL_GOD, matching do_gen_comm. Recipients receive regardless of soundproof rooms, position, writing status, or personal channel preferences.

PRF_NOGOSS, PRF_NOSHOUT, PRF_NOWIZ, and other personal channel settings do not gate BESEECH. No ignore-list routing is used; ignores cannot suppress an appeal in either direction. No new toggle was added.

Audited do_gen_comm (public audiences and per-channel history), do_qcomm (quest participants), tells (direct recipient/preferences), and do_wiznet (immortal sending, NOWIZ, and history). None supplies the required mortal-to-staff privacy contract unchanged. No separate newbie/OOC/immtalk command was found in the current registration. Existing handlers remain unchanged.

HELP BESEECH describes usage, privacy, restrictions, lack of history, NOREPEAT, a neutral assistance example, and avoiding spam. Staff replies continue through TELL.

## Validation

- Exact requested clean GNU17 build: PASS, zero compiler warnings, errors, or link errors.
- New BESEECH linked-engine regression: 94 assertions PASS, with ASan/UBSan on comm.c, act.comm.c, and the fixture.
- Existing control regression: 1,026 assertions PASS, confirming body output mirroring still works.
- Other regression scripts: 31 PASS, 1 pre-existing failure. xp_reward_audit_test.py:33 expects 3,736 records while the existing world contains 3,769. Neither that test nor world data was changed.
- Focused checks cover registration, level/position metadata, empty input, two staff recipients, mortal and immortal senders, same-room/group mortal exclusion, NPC send/receive exclusion, disconnected/nonplaying/missing-character staff, staff preferences, sender restrictions, NOREPEAT, literal formatting, all history slots, switched identities, possession privacy, public gossip, and staff TELL reply.
- Isolated boot of a temporary full library copy: PASS; entered game loop, accepted a loopback connection, and emitted no startup SYSERR. The log's final SIGTERM shutdown message is the deliberate harness termination.
- NOT VERIFIED LIVE: no authenticated mortal/immortal gameplay sessions were used. The production server was not restarted.
- SHA-256 comparison of every production lib/world file before and after: unchanged. Feature-scoped git diff --check: PASS.
- No commit or push.

## Files changed for this request

src/act.comm.c, src/act.h, src/comm.c, src/comm.h, src/interpreter.c,
lib/text/help/help.hlp, tests/beseech_runtime_test.c,
tests/beseech_runtime_test.py, docs/beseech_validation_report.md.

Earlier hello, control-system, and world edits are preserved. Validation logs are in the session's beseech-validation artifact directory.

## Final Git status (including pre-existing changes)

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
 M src/act.comm.c
 M src/act.h
 M src/act.offensive.c
 M src/act.wizard.c
 M src/class.c
 M src/comm.c
 M src/comm.h
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
?? docs/beseech_validation_report.md
?? docs/control_system.md
?? docs/control_validation_report.md
?? lib/world/zon/32.zon.pre_z32_driftwood_cleanup_20260902_032718.bak
?? src/control.c
?? src/control.h
?? src/spec_assign.c.pre_board_cleanup
?? tests/beseech_runtime_test.c
?? tests/beseech_runtime_test.py
?? tests/control_runtime_test.c
?? tests/control_runtime_test.py
```
