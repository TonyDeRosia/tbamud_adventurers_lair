# AI Actor Builder Compatibility

This page describes the **current** Adventurer's Lair runtime, not a future architecture. MEDIT is explanatory: it does not remove flags, rewrite routes, or synchronize legacy and AI state.

## Pulse ownership

On an eligible AI Actor pulse, the AI tick normally reports that it handled the pulse even when idle. The normal legacy mobile tail is then skipped: legacy scavenging, wandering, immediate aggression, MEMORY retaliation, rebellion, HELPER assistance, and hunting do not run. A handled special procedure runs first and prevents both AI Actor and that tail for the pulse. Scripts and AI event hooks can still move, speak, fight, or react through event paths.

## Flag compatibility

| Flag or setting | Current builder meaning |
| --- | --- |
| SENTINEL | Blocks legacy wandering and causes schedule movement failure; it does not universally block specials, scripts, or forced movement. |
| STAY_ZONE | Restricts checked steps to the current zone; it is not route analysis. |
| MEMORY | Separate legacy attacker-ID list. AI relationship records are not synchronized and legacy retaliation is normally bypassed. |
| HELPER | Separate legacy tail assistance. It does not enable AI assistance. |
| SCAVENGER | Legacy-only; normally bypassed while AI owns the pulse. No AI scavenging equivalent was found. |
| WIMPY | An inferred combat/flee input; legacy flee remains separate. AI combat flee uses the normal flee command. |
| AGGRESSIVE / AGGR_* | Inferred-default inputs. Legacy immediate aggression is separate and normally bypassed; AI event threat can escalate separately. |
| AWARE, NO_SLEEP, NO_BASH, NO_BLIND, NO_CHARM, NO_SUMMON, GUILD_MASTER | Independent engine restrictions; AI Actor does not reinterpret their semantics. |

## Movement and partial implementation

Random movement is stored/configurable but no AI random-movement tick was found. Schedule and patrol travel takes only directly adjacent destinations; there is no pathfinding. `hunt_enabled` is compiled/stored but no AI tick hunt path was found. `NO_TRACK` has no audited AI movement consumer. Brain think callbacks are partial/stubbed, and `ai_brain_can_speak()` currently always permits speech.

## Social, profile modes, specials, and scripts

Social eligibility currently depends on NPC status, AI_ACTOR, and a valid room; it does not check race, body type, intelligence, animal identity, or speech ability. Inferred derives defaults from prototype data and selected flags. Custom uses stored fields, while Overrides applies supported mask bits over inference. Only role, movement, and social consistently use that mode/mask logic; some runtime paths read compiled data while schedule and dialogue read stored configuration directly.

## Examples

* **Fido:** SENTINEL + STAY_ZONE + WIMPY + AI_ACTOR warns that autonomous paths are constrained; it does not promise every flee path is blocked.
* **Guard:** MEMORY + HELPER + AI_ACTOR has two separate legacy systems, both normally skipped on AI-owned pulses.
* **Scavenger:** SCAVENGER + AI_ACTOR warns legacy pickup is bypassed and no AI scavenging implementation exists.
* **Aggressive mob:** AGGRESSIVE can influence inferred threat defaults, but warning/challenge AI escalation is not legacy immediate aggression.
* **Scheduled shopkeeper:** a special procedure may handle a pulse before AI schedule processing.
* **Talking animal:** dialogue can be eligible because no humanoid/speech check currently prohibits it.
* **Non-adjacent patrol:** a waypoint route needs directly adjacent steps; no automatic route is calculated.
