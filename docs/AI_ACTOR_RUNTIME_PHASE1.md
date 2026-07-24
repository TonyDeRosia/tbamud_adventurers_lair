# AI Actor runtime phase 1

AI actors retain the existing pulse ordering: specials run first, then the AI actor,
then the legacy mobile tail only when the actor does not own the pulse.  Within the
actor, combat and scheduled/patrol work precede autonomous movement and idle work.

## Random local movement

`Random` uses one adjacent `perform_move()` attempt, so normal departure/arrival,
DG movement triggers, follower handling, and extraction protections remain canonical.
It rejects sentinel mobs, masters/charm control, incapacitation, combat, closed exits,
`ROOM_NOMOB`, `ROOM_DEATH`, and cross-zone exits for `STAY_ZONE`.  The authored
movement delay is a per-instance cooldown (minimum one second); blocked attempts also
consume the cooldown to prevent pulse spam.  No pathfinding or world scans are used.

## Effective creature policy

Compiled profiles expose an archetype, communication, memory, and assistance style.
Explicit persisted capability values override inference.  Inference is conservative and
runs at profile compilation: service roles speak; beast/undead/construct roles vocalize;
blob/ooze/slime/gelatinous descriptions become mindless and vocalize with basic hostile
memory and no default assistance.  Generic legacy actors retain humanoid/speaking
compatibility.  Telepathy is stored but does not masquerade as speech.

Idle decisions are bounded to observe, emote, speech, vocalization, random movement,
or no action.  Speech is capability-gated; creatures never use ordinary ambient
speech unless configured to `Speak`.

## Persistence and diagnostics

Only authored `AIConfigCapabilities` values persist; compiled values, cooldowns and
last-decision diagnostics do not.  Older mob files omit that record and receive inferred
values.  Builder diagnostics show effective behavior and state that Random movement is
enabled instead of reporting it unimplemented.  `aistate` continues to show schedule
state; runtime state also retains the last bounded decision and blocked reason for future
admin presentation.

Schedules, patrols, threat/combat, DG scripts, specials, AI memory, and legacy MEMORY
and HELPER remain separate.  This phase intentionally does not add language generation,
pathfinding, social taxonomy, or telepathy delivery.
