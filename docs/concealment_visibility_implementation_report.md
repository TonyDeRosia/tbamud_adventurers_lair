# Concealment and Visibility Implementation Report

## Architecture

Character perception is now evaluated in `handler.c` as ordered layers: staff
visibility, environmental sight, magical invisibility and its hard counters,
mundane Hide, then context-specific mobile concealment. `CAN_SEE` remains as a
deterministic compatibility wrapper for ordinary direct identification; it no
longer embeds binary Hide logic.

The public API defines `perception_context`, `perception_result`,
`can_perceive_environment`, `passes_administrative_visibility`,
`get_concealment_score`, `get_detection_score`, `perceive_character`, and
`break_concealment`. Results mean no usable perception, non-identifying presence,
or identified character.

## Rules and scoring

- Blindness/darkness remain environmental. Infravision addresses darkness only.
- Invisibility requires Detect Invis or Truesight. Sense Life and MOB_AWARE do
  not counter it.
- Sense Life and Truesight hard-counter Hide; Detect Invis and Infravision do not.
- Hide stores activation potency in the `SKILL_HIDE` affect modifier. Existing
  flag-only/saved states use a safe level-based fallback. Ordinary Look never
  rolls.
- Base detection is 35, plus observer level, twice Wisdom above 10, Dexterity
  above 10, less half target level. Hide activation potency is 25 plus half
  skill, Dexterity above 10, quarter level, and one activation roll of -5..5.
- MOB_AWARE adds 20 in movement/aggression/theft contexts. Sense Life adds 15
  to movement/aggression detection. Sneak adds 10 plus one-third skill to event
  concealment; Skulk adds 25 plus half skill. Movement rolls -10..10 per observer
  and event. Aggression is deterministic between state changes.
- Sneak and Skulk do not hide a stationary character. Skulk remains stronger
  and retains its doubled movement cost and lag.

## Routed behavior

Room lists, Scan, NPC aggression, ordinary lookup through `CAN_SEE`, explicit
spell targets, implicit current-fight spell targets, combat hit perception,
normal movement, and Track use the shared policy. Hide breaks on normal movement
and combat entry. Normal movement sends identity-bearing departure/arrival text
only to observers who identify the mover. Established combat accepts presence
without granting global identity. The red-eye darkness clue explicitly excludes
hidden and invisible targets.

Track now searches names independently of remote visual sight, while retaining
staff invisibility and `AFF_NOTRACK`. Pickpocket keeps its live-tested independent
success/detection rolls and 5..75 detection clamp, but concealment cleanup uses
the common breaker. Backstab, Circle, Vanish, Peek, object visibility, and
Sneak/Skulk cost mechanics were deliberately preserved.

## Compatibility and remaining direct `CAN_SEE`

Remaining uses in Who/Where/staff tools, shops, quests, object carrier checks,
DG scripts/triggers, socials through generic lookup, combat procs, follower
messages, and `PERS` mean ordinary visual identification and therefore safely
use the deterministic wrapper. Administrative/script operations that already
bypass visibility remain unchanged.

Alternative transports still emit several legacy room-wide animations. Their
text is routed through `act()`/`PERS` where applicable, but a complete semantic
classification of every spell-specific animation would be a separate messaging
project; normal directional movement is fully observer-aware.

## Files modified

`src/utils.h`, `src/handler.c`, `src/act.other.c`, `src/act.movement.c`,
`src/act.informative.c`, `src/mobact.c`, `src/graph.c`, `src/spell_parser.c`,
`src/fight.c`, this report, and
`tests/concealment_visibility_regression_test.py`.

## Validation

Completed on 2026-09-04: 7 concealment/visibility regression checks, 10 phase 5
shared-combat checks, and 9 buff-command checks passed. The full WSL `gnu17`
build compiled and linked `bin/circle`; `git diff --check` passed. The Windows
`py -3` launcher was unavailable, so the same scripts ran under WSL Python 3.

Live validation should compare
low/high-Wisdom observers; Sense Life, Detect Invis, Truesight, Infravision, and
MOB_AWARE observers; Hide plus Invisibility layering; repeated Look; Sneak versus
Skulk departures; dark-room red eyes; aggressive NPC acquisition; implicit combat
spells; Track with Hide/Invisibility/NoTrack; Pickpocket's four outcomes; and the
existing Backstab/Circle/Vanish flows.
