# AI Actor / legacy integration cleanup (through Phase 2B)

## Scope and compatibility contract

This pass hardens only Adventurer's Lair tbaMUD Oasis MEDIT and the existing AI Actor/legacy NPC integration. It introduces no special migration and no unrelated architecture. Existing NPC behavior remains unchanged unless global arbitration is enabled, a builder saves an explicit enforceable owner, and a newly spawned mobile copies that prototype configuration. Compatibility is legacy-preserving; with arbitration disabled `mobile_activity()` takes its original path and ignores ownership.

## Historical-artifact reconciliation

Base `af96c4083b2dde4c3078738d30ddd7b896bc4eed` was inspected on local branches `work` and `cleanup-medit-ai-legacy-integration` (the only reachable refs). Commands were `git branch -a`, `git for-each-ref --format='%(refname:short) %(objectname)' refs/heads refs/remotes`, and, for each requested path, `git log --all --oneline -- <path>`. None of `docs/AI_ACTOR_LEGACY_NPC_BEHAVIOR_AUDIT.md`, `docs/MEDIT_LEGACY_TO_AI_ACTOR_GAP_AUDIT.md`, or `tests/medit_audit_structure_test.py` exists in reachable history; none was fabricated or restored. The new routing guardrail is a current implementation test, not a replacement audit.

## Authoritative model

`MOB_BEHAVIOR_DOMAIN_COUNT` is 13 and sizes every inline owner/context array. A compile-time assertion ties the 13-entry definition table to that count; a second ties the nine editable entries to `MOB_BEHAVIOR_EDITABLE_DOMAIN_COUNT`. The table owns display names and canonical persistence tokens. Editable domains, in stable persistence/display order, are Routine, Movement, Posture, Ambient Communication, Combat Initiation, Memory Retaliation, Helper/Coordination, Scavenging, and Fleeing. Diagnostic-only domains are Service Commands, Combat Tactics, Door Interaction, and DG Scripts. Owners are Compatibility (default), Legacy, AI, and Disabled.

Canonical records are `AIBehaviorOwner: <DomainToken> <OwnerToken>`. Canonical domain tokens contain no spaces (for example `AmbientCommunication`, `CombatInitiation`, `MemoryRetaliation`, and `HelperCoordination`). The writer emits only non-Compatibility editable owners in table order. Loading is case-insensitive, rejects unknown/truncated domains and extra fields, leaves an unknown domain unchanged, and resets only a valid domain with an unknown owner to Compatibility. Consequently duplicate valid records are last-record-wins. Old files without records retain defaults.

The owner array is inline: allocation initializes the entire config, structure copying copies all slots without shared owner storage, and no separate free is required. Saved prototype values reach subsequently spawned copies; existing instances are intentionally not refreshed. Transient actor state is separate from prototype policy.

## Routing, validation, capabilities, and gates

`medit_is_ai_mode()` explicitly switches over every declared AI mode; it has no numeric upper-bound trap. Ownership choices use strict whole-string decimal parsing: only 1–4 are accepted, while signs, whitespace-only input, suffixes, decimals, and overflow are rejected. Q/q cancels without owner mutation; reset accepts only Y/y, N/n, or Q/q.

Current AI Actor actions cover routine/schedule movement, posture, autonomous movement, ambient communication, and combat initiation. AI ownership remains unavailable for scavenging and fleeing. Legacy-tail gates exist for ordinary movement, scavenging, aggression, memory retaliation, and helper assistance. Memory and helper gates additionally require Combat Initiation. There are no claimed routine, posture, or fleeing legacy-tail gates. Mayor's overlapping domains remain locked pending migration. Unknown custom specials are conservative; DG Scripts, service commands, and combat-pulse specials remain visible external authorities and are not suppressed by this model.

Ownership screens distinguish the saved configured owner from enforceability explanations, display global arbitration state, identify external authority exclusions, and state the new-instance prototype policy. Diagnostics count only rendered WARNING/HIGH RISK findings; INFO is excluded. Recent-pulse storage is a bounded 32-entry diagnostic ring and does not mutate mobile policy. Where action instrumentation cannot establish an event, treat it as **Not observed / unavailable**, never inferred.

`AI_LEGACY_ARBITRATION_ENABLED` / `ai_legacy_arbitration_enabled` defaults to NO, persists through the existing configuration file and CEDIT path, and does not modify prototype ownership. Builders should leave it disabled until acceptance succeeds.

## Manual builder acceptance checklist (not performed by this automated pass)

1. Open an old recordless mob; verify all nine owners are Compatibility; save unchanged and verify no owner records.
2. Edit one domain, save, inspect its canonical record, reboot, and verify reload; reset all and verify records disappear.
3. Exercise q and Q at the ownership list, owner prompt, and reset prompt; enter `2abc`, blank/whitespace, `1.5`, `+2`, `-1`, and an oversized integer.
4. Inspect Mayor 3105 locks, a DG-scripted mob's external-authority notice, a shopkeeper's service exclusion, and an unknown-special mob.
5. Inspect green blob 3068 ambient communication.
6. Exercise a wanderer under Compatibility, Legacy, AI, and Disabled; repeat for SCAVENGER and aggressive behavior.
7. Exercise MEMORY + Combat Initiation and HELPER + Combat Initiation, verifying unrelated domains continue.
8. Verify an existing live mob does not refresh after prototype save; respawn and verify the new instance receives policy.
9. Disable global arbitration and verify original behavior; re-enable and verify saved ownership resumes only for newly spawned mobs.

## Remaining limitations and migration readiness

No legacy special is suppressible until individually migrated. AI has no scavenging or fleeing implementation; external DG scripts, services, and combat-pulse specials remain outside ownership. Manual in-game acceptance remains required.

| Special | Domains | Per-instance state required | Current MEDIT locks | Safe next? | Order | Reason |
|---|---|---:|---|---|---:|---|
| Janitor | Scavenging | No known | AI/Disabled overlap | Yes | 1 | Small periodic object loop; valuable first end-to-end gate, though AI scavenging must precede AI ownership. |
| Fido | Scavenging | No known | AI/Disabled overlap | Yes | 2 | Narrow corpse/object behavior, slightly more semantic risk. |
| Puff | Ambient communication | No known | overlapping special authority | Yes | 3 | Narrow periodic speech with existing AI communication support. |
| Guild Guard | Service, Movement | Position/assignment | overlapping domains | Later | 4 | Command/service exclusion complicates movement migration. |
| Cityguard | Combat initiation/tactics | Target/combat state | overlapping domains | Later | 5 | Combat selection and pulse behavior are higher risk. |
| Snake | Combat tactics | Combat state | combat special external | Later | 6 | Combat-pulse special is outside current gates. |
| Thief | Combat tactics | Victim/cooldown state | combat special external | Later | 7 | Theft semantics are not an existing AI action. |
| Magic User | Combat tactics | Combat/spell state | combat special external | Later | 8 | Spell selection is complex and ungated. |
| Mayor | Routine, movement, posture, speech, doors | **Shared static route state** | AI/Disabled overlap locked | No | 9 | Multi-domain route and door sequencing needs state isolation first. |
| Other registered mobile specials | Metadata-specific | Audit individually | overlapping metadata domains | Not blanket-safe | After audit | Preserve command intercepts, services, and combat-pulse ordering. |

Migration readiness requires isolated per-instance state, an implemented AI capability, a truthful suppressible legacy gate, centralized validation, persistence round-trip coverage, runtime tests, and in-game acceptance. Janitor is recommended first after implementing the missing AI scavenging action; this cleanup does not migrate it.
