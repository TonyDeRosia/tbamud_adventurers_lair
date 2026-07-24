# AI Actor legacy NPC integration: Phase 2A

Phase 2A adds a conservative mobile-pulse arbitration boundary for `mobile_activity()` and the AI Actor tick path used from that pulse.  It is intentionally compatibility-preserving: existing NPC behavior remains unchanged unless arbitration is enabled and a safe explicit domain owner is selected.

## Scope

Covered domains are routine/time activity, movement, posture, ambient communication, reactive communication, combat initiation, helper/coordination, memory retaliation, scavenging/object interaction, and fleeing.  Service commands, arbitrary DG Script authority, combat tactics, spellcasting specials, command special dispatch, shops, quests, guilds, mail, rent, banking, boards, pet shops, dumps, zone resets, loadouts, loot, world-file formats, and AIConfig persistence are outside Phase 2A.

## Current ordering and compatibility mode

The current pulse order remains:

1. legacy periodic mobile special;
2. AI Actor tick;
3. legacy `mobile_activity` tail.

Compatibility Mode is `Legacy Preserving`.  When all domains are compatibility-owned, special return values, AI truthy returns, and legacy-tail short-circuiting are authoritative exactly as before.  The global rollback flag `AI_LEGACY_ARBITRATION_ENABLED` defaults to `FALSE`; when false, `mobile_activity()` delegates directly to the original legacy-preserving path.

## Runtime types and owners

The shared seam is declared in `legacy_behavior.h`: `MobBehaviorDomainMask`, `MobBehaviorPulseContext`, `MobBehaviorActionResult`, `MobBehaviorOwner`, and `MobBehaviorCompatibilityMode`.

Owners are:

- `compatibility`: preserve current execution semantics;
- `legacy`: AI Actor must not act in the domain during this pulse;
- `ai`: corresponding legacy-tail behavior must not act in the domain;
- `disabled`: neither legacy tail nor AI Actor may initiate the domain.

Absent ownership settings mean compatibility.  No persistence contract is added in Phase 2A; persistence is a Phase 2B seam.

## Effective owner and special locking rules

Known periodic specials use Phase 1 metadata.  Unknown custom specials are conservatively identified and, when explicit arbitration exists in a future persistence layer, should legacy-lock all overlapping Phase 2A domains and emit a diagnostic.  The adapter records special return value, metadata domains, unknown function status, false-after-acting risk, and old pulse-consumption semantics without changing special signatures.

Mayor remains unmigrated.  Its known domains are routine/time activity, movement, posture, ambient communication, and door interaction.  AI ownership of those domains is unavailable until Mayor receives a migrated adapter; builders must see: `AI ownership unavailable: legacy Mayor special must be migrated first.`

## AI Actor and legacy-tail gates

`ai_actor_tick_with_context(ch, now, context)` checks domain availability before practical domain actions such as routine movement, scheduled posture changes, ambient communication, random movement, and combat initiation.  Idle AI does not claim movement, speech, posture, or combat domains.  The existing `ai_actor_tick()` remains as the compatibility wrapper.

The Phase 2A legacy-tail seam identifies hunting precedence as still legacy-tail and currently compatibility-preserved; explicit gating is documented for scavenging, ordinary random movement, aggression, memory retaliation, charm rebellion, and helper assistance.  SENTINEL and STAY_ZONE remain movement restrictions.

## Diagnostics and rollback

Legacy Integration diagnostics now distinguish configured owner, effective owner, and observed runtime pulse data as separate concepts.  Recent pulse storage is bounded and transient; it is not persisted.  Metadata/diagnostic queries must not mutate runtime state.

Rollback is code-level: keep `AI_LEGACY_ARBITRATION_ENABLED` false to execute the original mobile_activity path with minimal overhead and no world-data edits.

## Manual acceptance procedures

1. Mayor 3105, arbitration disabled: boot with `ai_legacy_arbitration_enabled = 0`; observe route and messages unchanged.
2. Mayor 3105, arbitration enabled, compatibility mode: set the flag to `1`; observe route and AI coexistence unchanged.
3. Attempt AI movement ownership on Mayor: confirm the editor/diagnostic refuses with the migration-required message.
4. Green blob 3068: configure AI ambient communication; confirm AI communication works and no legacy ambient source is present.
5. SCAVENGER: compatibility preserves pickup behavior; future explicit legacy permits scavenging; future AI/disabled prevents legacy scavenging.
6. Aggressive mob: compare legacy-owned and AI-owned combat initiation after Phase 2B persistence lands.
7. MEMORY mob: compare memory retaliation ownership after Phase 2B persistence lands.
8. HELPER mob: compare helper ownership after Phase 2B persistence lands.
9. Shopkeeper: confirm buy/list/value service commands work in every Phase 2A mode.
10. DG-scripted mob: confirm triggers execute unchanged and diagnostics mark DG outside Phase 2A.
11. Reboot: old records load unchanged; no ownership persistence exists yet.

## Tests and next phases

Focused tests cover the feature flag, compatibility defaults, owner names, domain gates, Mayor AI-ownership rejection, unknown-special conservative locking, diagnostics strings, and absence of persistence mutations.  Phase 2B should add a backward-compatible ownership persistence/editor contract.  Phase 3 can migrate selected known specials, starting with safe non-service specials rather than Mayor.
