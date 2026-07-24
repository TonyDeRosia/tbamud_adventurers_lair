# AI Actor / Legacy Integration Phase 2B: MEDIT Behavior Ownership

Phase 2B is an Adventurer's Lair tbaMUD MEDIT feature. It extends the existing Oasis MEDIT, mobile prototype, ESpec, AI Actor, and `mobile_activity()` architecture; it does not introduce Smart MUD architecture.

## Ownership fields and meanings

`mob_ai_config` now stores one behavior owner per audited domain. Values are `Compatibility`, `Legacy`, `AI`, and `Disabled`.

- `Compatibility`: legacy-preserving pre-Phase-2B behavior.
- `Legacy`: AI Actor must not initiate the domain; legacy tail remains allowed.
- `AI`: AI Actor may initiate the domain; the matching legacy tail block is skipped.
- `Disabled`: neither AI Actor nor the matching legacy tail block may initiate it.

All new configs default every owner to `Compatibility`; old mobiles with no records load safely.

## Supported editable domains

MEDIT exposes routine/time activity, movement, posture, ambient communication, combat initiation, memory retaliation, helper/coordination, scavenging/object handling, and fleeing. Reactive communication remains diagnostic-only. Service commands, DG Scripts, and combat-pulse specials remain outside Phase 2B arbitration.

## Persistence

ESpec records are optional and human-readable:

```text
AIBehaviorOwner: Movement AI
AIBehaviorOwner: Routine Compatibility
AIBehaviorOwner: Scavenging Disabled
```

Only non-Compatibility owners are normally written. Unknown domains are logged and ignored. Unknown owners are logged and treated as `Compatibility`. Duplicate domain records are deterministic: the last valid record wins. Existing `AIConfig` records remain valid.

## MEDIT controls

AI Actor Configuration keeps its existing root menu. `Behavior Ownership` opens an editor with the nine domains, `R` reset-all, `D` diagnostics, and `Q` return. Domain prompts accept strictly parsed choices 1-4 or q/Q to cancel. Junk such as `2abc` is rejected. Reset-all asks for confirmation and q/Q or N cancels.

The screen states that changes apply to newly created instances after save and existing live instances are not automatically refreshed.

## Locks and external authorities

MEDIT refuses ownership claims that cannot be enforced. Mayor-controlled routine, movement, posture, and ambient communication reject `AI` and `Disabled` until the Mayor special is migrated. Unknown custom specials reject unsafe `AI` and `Disabled` choices. DG Scripts may still move, speak, fight, open doors, or issue commands independently. Shop/service commands remain unchanged.

## Runtime mapping

When `AI_LEGACY_ARBITRATION_ENABLED` is `NO`, `mobile_activity()` uses the original legacy-preserving path and ignores saved ownership. When enabled, the pulse context is built from saved owner policy and gates AI Actor actions plus legacy tail blocks. Compatibility owners preserve old behavior inside the enabled path.

Legacy-tail gates cover random movement, scavenging, aggression, memory retaliation, and helper assistance. Multi-domain legacy actions require every required domain to be available: memory retaliation requires Memory Retaliation and Combat Initiation; helper assistance requires Helper/Coordination and Combat Initiation.

## Prototype/live policy

MEDIT edits prototypes. Saving updates the prototype. New live mobiles copy the saved ownership configuration. Existing live instances, AI state, memories, threats, cooldowns, schedules, and route state are not silently refreshed.

## Rollback

Leave `AI_LEGACY_ARBITRATION_ENABLED` set to `NO` to preserve pre-Phase-2B behavior regardless of saved ownership records. Reset all owners to Compatibility and save to remove explicit records.

## Test coverage

Phase 2B adds structural tests for persistence records, shared mappings, MEDIT parser modes, q/Q behavior, locks, and runtime gates. Prior Phase 1 and Phase 2A tests should still pass.

## Manual acceptance steps

Run the acceptance scenarios from the Phase 2B task: no-record mobs, green blob 3068 ambient communication, wandering mobs with Movement AI/Disabled, SCAVENGER, aggressive, MEMORY, HELPER, Mayor 3105, unknown-special, shopkeeper, DG-scripted, prototype/live, and reset-all.

## Recommended next phase

Phase 2C should migrate or adapt selected known specials (starting with Mayor) only after each domain can be truthfully suppressed or implemented on both AI and legacy sides.
