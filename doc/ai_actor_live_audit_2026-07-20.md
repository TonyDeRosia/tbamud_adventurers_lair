# AI Actor live integration and Builder audit

## Scope and environment

This is a reproducible **manual live audit**, not a claim that a source guard exercised a running MUD.  The repository provides an intentionally empty reserved test prototype zone, `19500–19599`; it contains no rooms or reset commands.  Create the compact room fixture in a builder-owned development zone rather than changing production content.  Use real game time (`time`); do not change host time.  The command table confirms `medit`, `load mob <vnum> [count]`, `zreset [zone]`, `aistate <mob>`, `goto`, `trans`, `force`, `purge`, and `at` are available to the required staff levels.

## Fixture

Create nine adjacent rooms in one development zone: **Home, Work, Guard Post, Sleep, Patrol 1, Patrol 2, Patrol 3, Fallback, Threat Room**.  Link Work--Guard--Patrol 1--Patrol 2--Patrol 3 and provide a Patrol 3-to-Patrol 1 loop exit.  Add a closed alternative exit, a no-mob/death destination where safe, and one absent VNUM only for validation.  Do not use cross-zone links.

Reserve these prototypes in `19500–19507` (the zone is reserved for test prototypes): Civilian, Guard, Worker, Patrol Guard, Coward, Social NPC, Aggressor, Helper.  Spawn with `load mob <vnum> [count]`; use `zreset 195` only after adding reset records in the development copy of the zone.  Record actual room and VNUM choices in the zone change review.

## Builder procedure and save/reload proof

1. `medit <vnum>`; choose **I) AI Actor Configuration**, confirm enablement, then return with `Q`.  The pages are Personality, Social behavior, Dialogue, Perception, Memory, Threat response, Combat reactions, and **S) Schedules and patrol-safe routines**.
2. On every page: try an out-of-range value and `Q`; verify a useful error/parent return and that cancel leaves the displayed old value.  Make one valid value change and verify the normal OLC save prompt marks the copy changed.  Run Preview/Validate twice and verify neither causes a save prompt by itself.
3. Configure the Civilian with home/work/sleep, perception/memory, social lines, and flee or call-help; Guard with guard schedule, warning/challenge, retaliation/call-help; Worker with work/rest and no initiation; Patrol Guard with a three-waypoint route; Coward with flee; Social NPC with arrival/departure lines; Aggressor with initiation; Helper with assistance.  Use distinct dialogue text per category.
4. On a fully configured Guard, use Schedule Preview and Validate, Combat Preview and Validate, save normally, quit MEDIT, restart or reload through the normal server boot path, re-open MEDIT, and compare all displayed values, stable entry/route IDs, waypoint order, and policies.  Also inspect the saved mobile file for the emitted records as a secondary artifact—not as the proof.
5. `load mob <guard-vnum> 2`; alter one instance by greeting, attacking, fleeing, and waiting.  Compare `aistate` for independent memory/combat/schedule/waypoint/retry/expected-room state.  Purge one and respawn it; its runtime state must be fresh.  Edit/reload the prototype only between controlled observations.

## Time, selection, movement, and patrol script

At each observed game hour use `time`, `aistate <mob>`, and look.  Configure morning Work, afternoon Guard/Social, evening Home, night Sleep, an overnight entry, and overlapping entries that separately demonstrate priority, day specificity, window width, and stored-order resolution.  Compare the preview winner with `aistate` active entry.

For each movement transition verify one pulse at a time: wake, stand, one departure action, one adjacent `perform_move`, one arrival action, then active posture/activity.  Confirm Work/Guard do not wander and Sleep stays sleeping until the next travel entry.  `force <mob> sleep`, `force <mob> rest`, and ordinary movement observations supply posture checks where the actor accepts the command.

Configure three otherwise identical Patrol routes and run each separately:

* Loop: 1 → 2 → 3 → 1.
* Ping-pong: 1 → 2 → 3 → 2 → 1.
* Once: 1 → 2 → 3 → complete.

Give each waypoint a wait duration and distinct arrival line.  Check `aistate` after every arrival for waypoint/direction/expected room.  Close an exit, use the invalid VNUM, set Sentinel/Stay Zone where supported, and exercise every failure policy: Wait/Retry, Skip, Restart, Fallback, Abort, Disable.  Confirm retry time/attempts and one failure line per failure transition; fallback must travel rather than teleport.

## Social, perception, threat, combat, and interruption script

Use a visible player to enter/leave, say/whisper/emote, greet, insult/threaten, attack the actor, and attack an ally.  Repeat while the actor sleeps and fights; repeat hidden/invisible only if the build offers that state.  Check category-specific dialogue and cooldown behavior.  Repeat with Silent style and confirm optional speech is suppressed.

For configured actors provoke Observe, Warn, Challenge, Call Help, Attack, Flee, and Ignore.  Use Guard + Helper + Coward to verify eligible local assistance, no recursion/duplicate joins, deterministic visible target selection, and that Coward declines.  End combat by victory, loss, flee on each side, external stop, and safe extraction.  After each case inspect `aistate` and later re-enter the room to verify one lifecycle memory/relationship update and no stale opponent state.  Generic opponent-flee detection remains limited to canonical observed flee/end paths.

For each interruption policy (Ignore Minor, Pause/Resume, Restart, Skip, Abort, Fallback), separately cause minor social activity, major threat, combat/flee, `trans <mob> <target>` or `at <room> force <mob> <direction>`, and invalid destination.  Verify only the documented policy result and diagnostic reason.  These external movement tests also cover admin displacement; use a safe existing DG trigger and special-procedure mobile, if the local test world has one, to cover script/special movement, speech, combat, and extraction.  Do not reinterpret arbitrary scripts.

## Diagnostics, reset, limits, and result recording

Capture `aistate` at inactive, selected, preparing, traveling, arrived, work, guard, sleeping, waypoint wait, interrupted/resuming, failed/aborted/disabled, combat, and post-combat.  It must agree on active entry, route/waypoint, expected room, reason, attempts/retry, wandering suppression, and resume snapshot.  Repeated `zreset 195` must obey normal reset limits and create fresh state.

Finally create 16 entries, 8 routes, and 16 waypoints per route and repeat a short tick observation.  Validate malformed/truncated mobile records only in a disposable copy.  Record PASS, PASS WITH LIMITATION, FIXED, FAIL, or NOT TESTABLE for every requested area; a missing live trigger/special fixture is **NOT TESTABLE**, not PASS.

## Static-audit results in this checkout

| Area | Status | Evidence/limitation |
|---|---|---|
| Builder navigation, preview, validation, persistence records | PASS WITH LIMITATION | Implemented menus and parser/writer source guards pass; no interactive descriptor was available. |
| Prototype copy and reset ownership | PASS WITH LIMITATION | Copy/reset initialization is source-guarded; two-live-instance session remains manual. |
| Schedule selection, movement, patrol modes, posture, wandering | PASS WITH LIMITATION | Bounded helpers and `perform_move` guard pass; requires live fixture. |
| Social/perception/memory/relationships/threat/combat/coordination/lifecycle | PASS WITH LIMITATION | Four regression guards pass; event interactions require live fixture. |
| All interruption/failure policies and displacement | PASS WITH LIMITATION | Runtime-policy guard passes; every external cause needs a running test world. |
| DG Scripts and special procedures | NOT TESTABLE | This checkout has no safe attached test trigger/special fixture. |
| Diagnostics/preview/live agreement | PASS WITH LIMITATION | Read-only diagnostic/preview code is guarded; live comparison remains manual. |
| Boundedness and corruption handling | PASS WITH LIMITATION | Maximum constants and malformed-record parser paths are source inspected; load stress remains manual. |
| Compiler warning cleanup in touched code | FIXED | Reformatted schedule/combat preview and validation paths remove their misleading-indentation warnings. |
