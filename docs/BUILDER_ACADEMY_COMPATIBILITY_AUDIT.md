# Builder Academy DG Scripts Compatibility Audit

## Scope and authority

This is a legacy-conformance audit, not a redesign. The records in
`lib/world/wld/0.wld` are the primary specification, followed by the current
help database, trigger prototypes, mobiles, objects, rooms, and zone reset.
No Academy prose, VNUM, prototype, attachment, layout, or reset was changed.

The executable contract is `tests/builder_academy_dg_compatibility_test.py`.
It reads the shipped world files rather than maintaining a rewritten copy of
the lessons, and crosses their promises into production command, persistence,
and runtime source paths.

## Builder Academy promises verified

| Lesson | Promise | Verified implementation/data |
|---|---|---|
| Room 13 | `trigedit` instruction and route into the tutorial | Academy room exists; `trigedit` is routed to Oasis OLC; help/menu topics exist. |
| Room 18 | `STAT QUESTMASTER`, `TSTAT 1`, basic quest | mob 14 owns triggers 3 then 1, mob 16 owns death trigger 2, and both reset into rooms 18/19. Trigger 1 greets from the south; 2 loads wings; 3 rewards wings, purges them, and rejects a wrong object with `return 0`. |
| Room 20 | `STAT GATEGUARD`, triggers 4/5/7/8, `GIVE 10 COINS GUARD` | mob 24 has exactly 4, 5, 7, 8 in stored/runtime order and resets in room 20 with key 46. The scripts greet, refund underpayment/change overpayment, unlock/open, then close/lock. |
| Rooms 21–22 | persistent player quest variables and triggers 190–192 | mob 25 owns 192, 191, 190; room 22 owns enter trigger 189; protector 26 owns leave trigger 188; object 47 owns command trigger 6. The completion script uses `remote` on the actor id and guards repeat rewards with `varexists`. |
| Room 23 | trigger help navigation and examples | all required help aliases are present and builder command routes exist. |
| Rooms 98–99 | end-of-course route and Cafe population | both rooms and their reciprocal route remain; Aristotle, sign, cafe staff, and Death resets remain intact. |

## Prototype and entity audit

The contract verifies trigger VNUMs 1–8, 20, and 188–192, including attachment
type, encoded event flags/numeric argument, argument/body landmarks, and exact
ordered ownership for the questmaster, ogre, gateguard, variable questmaster,
protector, Eight Ball, and room 22. It also verifies the zone reset records
that make these interactions reachable.

Additional tutorial coverage is supplied by the existing DG capability matrix:
all mobile, object, and room event bits have an editor constant and production
dispatch path. This includes enter, leave, speech, drop, command, time, reset,
login, cast, door, load, timer, get/give/wear/remove/consume, greet, death,
bribe, fight, memory, and damage events.

## DG language conformance

The Academy bodies provide real examples of variables/entity fields, `if`,
`elseif`, `else`, `switch`, `while`, `eval`, `wait`, `set`, `remote`, and
`return`. The executable audit requires the corresponding production driver
branches, expression evaluator, and script driver. Existing capability and
round-trip tests additionally cover `unset`, `global`, context-bearing
variables, compilation of command lists, and all trigger-family dispatchers.

Important legacy semantics remain unchanged:

* `return 0` records the event result; execution continues until completion,
  `wait`, or `halt`, as the help teaches. This permits the protector lesson to
  stop movement and then resume its delayed script, while command trigger 6 can
  pass an unmatched command back to the normal interpreter.
* Stored prototype order is authoritative. The disk reader appends `T` records,
  deep-copy preserves list traversal, runtime assignment appends triggers, and
  STAT's script display traverses that runtime list. No reversal was introduced.
* Parent OLC still saves legacy `T <vnum>` records. MEDIT's `S`, `N`, `D`, and
  `X` workflow, `position,vnum` insertion, duplicate-compatible ordered storage,
  save/reload, and deep-copy ownership remain covered by the dedicated MEDIT DG
  lifecycle tests.

## Confirmed difference and repair

### Final-prototype attachment omitted by CHECKLOAD

`trg_checkload()` searched permanent mobile, object, and room prototype tables
with `< top_of_*`. Those three variables are highest valid real indices, so a
trigger attached to the final prototype in any table was omitted from this
Builder-facing inspection result. (Trigger-table `top_of_trigt` is separately
a count and was not changed.) The loops now use `<=`, making inspection match
actual attachment ownership without changing storage or runtime behavior.

A regression assertion covers all three corrected bounds. This repair is
additive and data-format neutral.

## Commands, persistence, and inspection

The audit checks command-table routing for TRIGEDIT, TLIST, TSTAT, STAT, MEDIT,
OEDIT, and REDIT. Existing focused suites cover Trigedit field/menu operations,
trigger save and reload grammar, parent attachment insertion/deletion and
persistence, attach-type rejection, zone permissions, deep-copy ownership, and
runtime instantiation.

TSTAT reads the actual prototype and its compiled ordered command list. STAT
uses the runtime script list. The Script Editor, disk `T` records, prototype
copy, runtime attachment, and STAT display all traverse head-to-tail; therefore
editor, saved, runtime, and displayed order agree.

## Manual-equivalent walkthrough

The repository has no deterministic in-process telnet fixture capable of
advancing DG wait events as a logged-in builder. Rather than claim an
unrepeatable interactive session, this pass executes the shipped Academy data
as a cross-layer contract:

1. resolve each required lesson room;
2. resolve every named prototype and exact metadata/body behavior;
3. resolve exact parent attachment order;
4. resolve the zone resets that spawn each participant and prop;
5. resolve each taught command to its production handler;
6. resolve persistence/reload/runtime traversal paths; and
7. resolve every taught language/event feature to production driver dispatch.

For live smoke verification, boot a disposable world, disable NOHASSLE, and
follow rooms 13, 18, 20–23, 98, and 99 literally. In room 18 enter from the
south, kill ogre 16, return object 1, then give a different object and confirm
it remains with the player. In room 20 test 9, 10, and 11 coins and both sides
of the gateway. In rooms 21–22 accept the variable quest, shake object 47,
return it once, repeat, then attempt to leave without an Eight Ball. Finally
save/reload an ordered attachment edit and compare Script Editor, STAT, and the
parent world file.

## Regression tests

* `builder_academy_dg_compatibility_test.py`: Academy rooms/help/prototypes,
  quest, guard, variables, Eight Ball, objects/rooms, resets, language,
  commands, ordering, persistence/runtime, and CHECKLOAD boundary repair.
* `medit_dg_legacy_contract_test.py` and `dg_attachment_workflow_test.py`:
  legacy MEDIT interaction, validation, insertion/deletion, ownership, save,
  reload, and assignment.
* `dg_trigger_capability_matrix_test.py`: every event family from editor bit to
  production dispatcher.
* `dg_trigger_roundtrip_test.py` and `dg_trigedit_workflow_test.py`: trigger
  prototype serialization and editor workflow.

## Upstream comparison

The retained design matches the tbaMUD lineage already present in this tree:
ASCII trigger flags, tilde-terminated prototype fields, `T <vnum>` parent
attachments, linked-list ordering, Oasis editors, and `script_driver` event
dispatch. The fix deliberately follows this tree's `top_of_world`,
`top_of_mobt`, and `top_of_objt` highest-index convention; it does not import a
new storage or workflow model. No upstream remote is configured in this
checkout, so no unverifiable claim is made about a later external revision.

## Remaining limitations

* Automated coverage is cross-layer and deterministic but does not emulate a
  network client or wall-clock DG event queue. The live smoke procedure above
  remains the final operational check for timing and player-visible text.
* Trigger 20 is a historical speech-spy example and intentionally retains its
  authored comment noting that one alternative command does not work. It is
  audited, not rewritten, because the Academy/prototype authority order forbids
  silently changing the example.
* The audit guarantees the current Academy records and named help topics. It
  does not certify arbitrary third-party zone scripts beyond the complete
  trigger-family capability matrix.
