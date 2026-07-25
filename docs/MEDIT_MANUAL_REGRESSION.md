# MEDIT manual regression

Run these over telnet as a builder on a test instance.  Do not alter production
world data.  `ENTER` means a blank line.

## Test A — mob 3068 DG Scripts

1. Enter `medit 3068`, then `s`, then `n`.
2. Expect `Please enter position, vnum`; generic MEDIT must not print `Try again`.
3. Enter `0` to cancel.  Expect the attached-trigger menu and no dirty change.
4. Return if needed, enter `s`, then `1`.
5. Expect read-only trigger 3011 inspection: exact name `Stock Janitor - 3061,
   3068`, trigger/attach types, numeric argument, argument phrase, and verified
   metadata describing the command-backed inexpensive non-fountain pickup.
6. Enter an invalid letter.  Expect the Q prompt again, not a parent transition.
7. Enter `q`.  Expect the attached-trigger menu; the mob remains unmodified.
8. Separately verify `q` and `Q` return to MEDIT, and `n`/`N` both show attach
   mode.  On a disposable copied mob only, verify `x`/`X` show detach mode and
   cancel with `0`.

Never detach trigger 3011 from the real prototype during routine testing.

## Test B — false Mayor

Enter `medit 24201`, then `c`.  Expect `Assigned Special: None`, an explicit
named-Mayor warning, and no claim that route, speech, gate, posture, sleep, or wait
behavior is active.

## Test C — actual Mayor

Enter `medit 3105`, then `c`.  Expect `Assigned Special: Mayor` and read-only
views for schedule, fixed route, speeches, door actions, posture, wake/sleep, and
waits.  Return with `q`; no edit should be recorded.

## Test D — AI Actor Extensions

Open any mob with `medit <vnum>`, then `i`.  Expect only numbered entries for
Personality, Identity / Role, and Advanced Perception.  Ownership notices may be
shown, but there must be no broad Communication, Daily Routine, Movement, Patrol,
or Combat editor command.  Invalid old command letters must simply redraw this
menu.

## Test E — Effective Preview

For each of 3068, 24201, and 3105 enter `medit <vnum>`, then `v`.  Confirm the
preview reports actual NPC flags, actual assigned function pointer, and actual
attached scripts.  Mob 24201 must not acquire Mayor behavior by name; mob 3105
must report Mayor; mob 3068 must report its script without duplicate AI ownership.

## Test F — OEDIT and REDIT DG menus

Using disposable object and room prototypes, enter their editors and then `s`.
For each parent verify `n`/`N`, `x`/`X` (cancel deletion with `0`), `q`/`Q`, and a
displayed numeric trigger inspection.  Q from inspection returns to the trigger
list; Q from that list returns to the correct object or room menu.  Opening and
inspection must not cause a save prompt; a successful attach/detach must.
