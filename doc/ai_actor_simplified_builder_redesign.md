# AI Actor simplified builder redesign

## Status and decision boundary

This document is a proposal for review. It intentionally makes **no source,
data-format, or world-file changes**. Implementation should begin only after the
builder vocabulary, legacy precedence policy, and migration rules below are
approved.

The redesign treats AI Actor as a small intent layer. Builders author a handful
of durable concepts; a resolver expands those concepts into the existing
runtime profile. Detailed coefficients remain engine-owned and may continue to
evolve without changing MEDIT or BuilderBot.

## Design principles

1. **One obvious path.** `MEDIT -> I) AI Actor` is the only builder entry point,
   and every displayed command has exactly one documented parent and return
   path.
2. **Eight concepts, not engine controls.** Enabled, Role, Personality, Home,
   Behavior, Patrol, Dialogue, and Combat Style are the complete public model.
3. **Presets are declarative intent.** Builder selections are stored as stable
   identifiers. Runtime details are derived in one resolver, not copied through
   menu handlers.
4. **Legacy behavior remains valid.** DG Scripts, mob programs, special
   procedures, shops, trainers, quest bindings, triggers, and established mob
   flags are neither removed nor silently replaced.
5. **Old data is readable and lossless.** Existing detailed AI records can stay
   serialized. The simple editor never mutates unrelated legacy fields merely
   by opening or saving a mobile.
6. **Determinism over cleverness.** Numeric choices, explicit prompts, bounded
   lists, and stable return states make the same workflow usable by people and
   BuilderBot.

## 1. Proposed simplified menu layout

### Main page

```text
AI Actor
--------
1) Enabled       : Yes
2) Role          : Guard
3) Personality   : Suspicious
4) Home          : 3001 - West Gate
5) Behavior      : Guard Post
6) Patrol        : Not used by Guard Post
7) Dialogue      : Greeting, Warning, Idle (3/5 set)
8) Combat Style  : Defensive

P) Preview effective behavior
V) Validate
Q) Return to Mobile Editor
Choice:
```

The page always shows all eight concepts so its numbering never changes.
Option 6 is selectable only when Behavior is Patrol; otherwise it prints a
single explanation and returns to this page. Disabling AI does not delete its
configuration. The preview is read-only and describes both derived behavior and
legacy authorities. Validation reports invalid rooms, an empty patrol, and
known ownership collisions, but never exposes coefficients.

### Selection pages

* **Enabled:** `1) Disabled`, `2) Enabled`, `Q) Cancel`. Enabling creates a
  default simple configuration if none exists; disabling clears only the
  `MOB_AI_ACTOR` flag.
* **Role:** `1 Civilian`, `2 Guard`, `3 Merchant`, `4 Innkeeper`, `5 Quest
  Giver`, `6 Bandit`, `7 Soldier`, `8 Monster`, `9 Animal`, `10 Boss`, `11
  Companion`. Each choice includes one plain-language sentence. Role never
  assigns a special, shop, quest, or trigger.
* **Personality:** `1 Friendly`, `2 Neutral`, `3 Suspicious`, `4 Aggressive`,
  `5 Cowardly`, `6 Brave`, `7 Loyal`, `8 Curious`. Version 1 permits exactly
  one preset. Future engine work may support secondary traits without changing
  the stored primary value.
* **Home:** three room prompts: Home Room (required for location-dependent
  behavior), Workplace (optional), and Sleep Room (optional). `0`/`none` clears
  an optional room. Effective Workplace and Sleep Room fall back to Home.
  Existing Guard Room and Fallback Room remain internal compatibility data.
* **Behavior:** `1 Stationary`, `2 Wander`, `3 Patrol`, `4 Guard Post`, `5
  Shopkeeper`, `6 Trainer`, `7 Quest NPC`. Service behaviors describe how AI
  idles and reacts; the actual shop/trainer/quest service remains owned by the
  existing subsystem.
* **Combat Style:** `1 Balanced`, `2 Aggressive`, `3 Defensive`, `4 Caster`,
  `5 Archer`, `6 Coward`, `7 Boss`. Caster and Archer initially affect target,
  distance, and morale preferences only where the current combat engine can
  honor them; validation/preview must say “limited runtime support” rather than
  promise spell or ranged-action selection.

### Patrol page (conditional)

Version 1 presents exactly one route owned by the simple Behavior=Patrol
selection:

```text
Patrol
------
Loop Mode: Loop
1) 3001 - West Gate
2) 3002 - Guard Walk
3) 3003 - North Gate

A <room-vnum>) Add waypoint
D <number>)    Remove waypoint
U <number>)    Move waypoint up
N <number>)    Move waypoint down
L)             Change loop mode (Loop / Ping-pong / Once)
Q)             Return
Command:
```

Room validation occurs before mutation. Add appends; delete closes the list;
up/down perform one bounded swap. There are no route IDs, labels, weights,
arrival actions, waits, failure policies, duplication, schedules, or previews
on this page. Engine defaults supply those values. An empty or one-room patrol
is retained but validation blocks an apparently complete result.

### Dialogue page

Use five optional slots: Greeting, Warning, Idle, Combat, and Death. Each is a
single text value with `1..5`, `C <number>` to clear, and `Q` to return. Input is
bounded to the existing dialogue line limit and uses the normal MUD text
sanitization rules.

The slots are AI fallback utterances, not replacements for DG Scripts or mob
programs. If an existing trigger owns an event, it remains authoritative and
the preview identifies the AI line as suppressed/fallback. Death delivery must
be wired only through an existing safe death/event hook; until then it is stored
and previewed as unsupported rather than emitted from an unsafe lifecycle
point.

### State-machine contract

The public editor needs only these normalized states: main, choose enabled,
choose role, choose personality, edit home, choose behavior, patrol, choose loop
mode, dialogue, edit dialogue value, choose combat style, preview, and validate.
Every child accepts `Q` and returns to its fixed parent. Invalid input redraws
the current state without mutation. Save/quit remains the standard MEDIT
transaction—there is no nested save command.

Historical `MEDIT_AI_*` constants and parser cases may remain temporarily for
source compatibility, but no public menu, undocumented command, help return, or
stored descriptor state may transition into them. After one release and a scan
for out-of-tree users, delete dead display/parser functions in a separate
cleanup change rather than renumbering shared Oasis modes.

## 2. Required source modifications

### Public configuration and vocabulary

In `src/ai_actor.h`:

* Add stable enums for simple role, personality, behavior, and combat style.
  Assign explicit numeric values and append future values; never reorder them.
* Add a schema/version marker and builder-facing fields to `mob_ai_config`:
  `simple_version`, `simple_role`, `simple_personality`, `simple_behavior`,
  `simple_combat_style`, and an “authored simple model” flag. Reuse existing
  home/work/sleep room storage, dialogue arrays, and patrol arrays where doing
  so is unambiguous.
* Do not reuse the existing `override_mask` to mean simple ownership. It records
  detailed historical overrides and is part of compatibility preservation.
* Introduce a private resolved-policy structure (or extend
  `ai_actor_profile`) containing engine values. The public simple enums must not
  depend on the numeric layout of existing internal enums.

In `src/ai_actor.c`:

* Add one table-driven resolver,
  `ai_actor_resolve_simple_config(config, mob_metadata, profile)`, invoked by
  profile build/refresh. It applies Role defaults, then Personality modifiers,
  then Behavior rules, then Combat Style rules, followed by hard safety and
  legacy-authority gates.
* Keep derivation pure and deterministic: no RNG, room mutation, live actor
  state, or builder-session state. Runtime randomness may act only after a
  resolved policy exists.
* Centralize names, summaries, validation, and preset tables here (or a small
  dedicated module) so MEDIT, runtime diagnostics, persistence migration, and
  BuilderBot share one vocabulary.
* Preserve existing schedule, memory, threat, combat, and event machinery as
  implementation detail. Remove none of the hooks used by DG Scripts or normal
  MUD behavior.

### MEDIT

In `src/medit.c` and `src/oasis.h`:

* Replace the current AI root and its reachable transitions with the state
  machine above. Do not reconnect the historical hierarchy.
* Implement small picker helpers and generic “set/clear room” and “set/clear
  dialogue slot” handlers. All mutations set the ordinary OLC dirty state.
* Make Patrol a direct waypoint editor over one designated simple route. Stable
  list indices are user-facing; internal route IDs never are.
* Preview must show: enabled state; eight authored values; effective fallback
  rooms; a concise behavior summary; legacy/DG/service ownership; ignored or
  unsupported selections; and validation warnings. It must not print weights,
  thresholds, masks, memory scores, parser diagnostics, or state-machine data.
* Retain staff-only runtime diagnostics such as `aistate` outside MEDIT if they
  are operationally necessary. “Not in builder UI” does not require destroying
  observability.

### Persistence and copy lifecycle

In `src/genmob.c`, `src/db.c`, and all mobile copy/free paths:

* Add a compact, versioned record, for example
  `AISimple: 1 <role> <personality> <behavior> <combat-style>`.
* Continue reading and writing existing `AIConfig*`, `AIRoutine*`, `AISchedule*`,
  `AIPatrol*`, `AIDialogue`, and ownership records during the compatibility
  window. This guarantees a load/save cycle does not erase advanced historical
  configuration.
* Keep enabled state in `MOB_AI_ACTOR` for compatibility. `AISimple` describes
  intent even while disabled.
* Reuse `AIConfig` home/work room values and `AIConfigSchedule` sleep room until
  a future format consolidation. Document the fallback semantics rather than
  duplicating rooms in two records.
* Map five dialogue slots onto existing categories: Greeting ->
  `AI_DIALOGUE_GREETING`, Warning -> `AI_DIALOGUE_WARNING`, Idle ->
  `AI_DIALOGUE_AMBIENT_SPEECH`, Combat -> `AI_DIALOGUE_CHALLENGE` (or a newly
  appended explicit category after runtime review), Death -> a newly appended
  category. Never renumber the existing 20 categories. Preserve additional old
  lines/categories invisibly.
* Tag or reserve one patrol route as the simple route without changing existing
  route IDs. Prefer a persisted `simple_patrol_id`; do not assume array element
  zero forever.

### Tests and documentation

* Replace tests that assert old labels or orphan reachability with a generated
  transition contract covering every public state, command, invalid input, and
  `Q` return.
* Add unit tests for all preset rows and precedence order, round-trip fixtures
  for old-only/new-only/mixed records, and copy/free tests for dialogue and
  patrol ownership.
* Add integration tests proving DG triggers, specials, shops, quests, trainers,
  and legacy flags behave identically when AI is disabled and retain declared
  precedence when it is enabled.
* Update builder documentation and the stale live-audit procedure only when the
  implementation lands; the present audit remains evidence of the old system.

## 3. Data migration strategy

Migration is lazy, versioned, and reversible for at least one release.

### Load classification

Classify each prototype without rewriting its file:

1. **No AI data:** no config is allocated until the builder enables or edits AI.
2. **Simple-native:** `AISimple` is present; resolve it and retain all legacy
   records as opaque compatibility detail.
3. **Legacy-exact:** detailed AI records exist but `AISimple` does not. Infer a
   display projection and mark it `legacy-derived`, not builder-authored.
4. **Mixed:** both exist. The simple model drives engine-owned domains; legacy
   values survive serialization and remain effective only in legacy-owned
   domains.

### Conservative projection of existing data

Projection is for display/migration, never a claim of exact equivalence:

* Map existing roles directly where names match. Civilian, Guard, Merchant,
  Bandit, Beast/Animal, and Boss are high-confidence. Infer Innkeeper,
  Quest Giver, Trainer, and Companion only from authoritative service/follower
  metadata; otherwise use Civilian or Monster with a migration warning.
* Choose Personality by nearest preset using the 12 stored traits, with a
  deterministic distance calculation and fixed tie order. Mark the result
  approximate. Do not overwrite traits until the builder explicitly saves a
  personality choice.
* Map movement Stationary/Random/Patrol/Guard Room to the matching simple
  behavior. Scheduled, Return Home, multiple routes, and complex routines have
  no lossless public equivalent; show the closest behavior plus “legacy routine
  retained.”
* Map existing combat styles directly for Balanced, Aggressive, Defensive,
  Cowardly, and Boss. Protector/Passive/Opportunist/Fanatical/Controller choose
  the closest documented preset with a warning. Never infer Caster or Archer
  without positive metadata.
* Select an existing patrol as the simple route only when exactly one enabled
  route is unambiguously active. Otherwise retain all routes and require an
  explicit builder choice before simple patrol editing.
* Display the first existing line in each mapped dialogue category. Preserve
  every extra line and unmapped category. Editing a slot changes only its mapped
  first line.

### First explicit edit and save

Opening the editor performs no migration. On the first explicit simple-field
edit, materialize `AISimple` version 1 from the current projection, set the
authored flag, and leave detailed fields intact. On save, emit both formats.
The resolver uses the simple model only after that materialization; until then,
legacy-exact mobiles use their existing profile path byte-for-byte.

Provide an offline audit command before rollout that reports counts and VNUMs
for: exact mappings, approximate mappings, complex schedules, multiple patrols,
unsupported combat styles, invalid rooms, and ownership collisions. It should
be report-only by default. A later explicit migration tool may write world data
after backups and review; boot must never bulk-rewrite areas.

### Eventual cleanup

After at least one production release with dual-read/dual-write and rollback
proof, a separate proposal may stop emitting unused legacy values for
simple-native mobs. Readers should continue accepting them indefinitely. No
cleanup is required to ship the simple editor.

## 4. Runtime behavior mapping

The resolver starts from a safe neutral baseline. The following tables describe
semantic effects, not public numeric coefficients.

### Role defaults

| Role | Default behavior | Social/memory intent | Combat intent |
|---|---|---|---|
| Civilian | Stationary or modest Wander | ordinary speech, remembers direct harm | avoids initiating; flees/calls help |
| Guard | Guard Post | notices crime/threat, warns, assists allied guards | retaliates, protects, calls help |
| Merchant | Shopkeeper | greets and answers service interaction | defensive; protects self, avoids pursuit |
| Innkeeper | Shopkeeper | welcoming, talkative service presence | defensive and help-seeking |
| Quest Giver | Quest NPC | greets and responds to quest interaction | defensive; quest subsystem remains authoritative |
| Bandit | Wander | suspicious/extorting, remembers profitable or hostile actors | opportunistic initiation |
| Soldier | Guard Post | disciplined, assists faction/group | coordinated and brave |
| Monster | Wander | limited speech unless dialogue exists | aggressive/territorial |
| Animal | Wander | vocalization/no speech by default | instinctive; flees or defends by personality/style |
| Boss | Stationary | imposing warnings, strong hostile memory | boss targeting, low flee tendency |
| Companion | Stationary/follow owner | loyal, assists master/group | protects owner; follower system owns movement |

Changing Role supplies defaults only to the resolver; it does not overwrite the
other four builder choices. Thus a Friendly Bandit or Cowardly Boss is legal and
predictable.

### Personality modifiers

| Personality | Derived influence |
|---|---|
| Friendly | more greetings/help, slower suspicion/escalation |
| Neutral | no modifier |
| Suspicious | notices strangers sooner, warns before trusting |
| Aggressive | escalates and initiates more readily where Role permits |
| Cowardly | earlier flee/help behavior, less solo initiation |
| Brave | later flee, more persistence and assistance |
| Loyal | prioritizes master/group/faction protection and memory |
| Curious | observes speech/events and investigates locally without unsafe global pathing |

Personality cannot grant a capability forbidden by metadata or legacy
ownership. For example, Curious does not make a mindless creature speak and
Aggressive does not replace a special procedure's combat domain.

### Behavior rules

| Behavior | Engine rule |
|---|---|
| Stationary | suppress autonomous movement; may still react locally |
| Wander | use bounded adjacent random movement, obeying SENTINEL, STAY_ZONE, room restrictions, and ownership |
| Patrol | run the designated simple waypoint route; block unrelated wandering while active |
| Guard Post | return/remain at effective Home (or existing guard room for legacy-derived data) and use guard reactions |
| Shopkeeper | remain at effective Workplace/Home; existing shop commands own commerce |
| Trainer | remain at effective Workplace/Home; existing trainer/guild code owns training |
| Quest NPC | remain at effective Workplace/Home; existing quest/DG code owns quest state |

Missing Home never causes teleportation. Stationary remains usable; other
location-dependent behaviors validate as incomplete and fall back to current or
spawn room according to the existing safe runtime rules.

### Combat Style rules

| Style | Derived policy |
|---|---|
| Balanced | ordinary retaliation, target stability, moderate flee |
| Aggressive | permits Role-appropriate initiation, faster escalation, less flee |
| Defensive | retaliate/protect, prefer current attacker, avoid initiation |
| Caster | prefer distance/casting-capable tactics only when supported; otherwise Balanced with warning |
| Archer | prefer ranged-capable tactics only when supported; otherwise Balanced with warning |
| Coward | avoid initiation, call help, flee earlier |
| Boss | stable targets, protects encounter integrity, rarely flees; no scripted mechanic replacement |

### Precedence

Resolve in this exact order:

1. Hard safety and eligibility rules (dead/incapacitated/charmed, peaceful room,
   invalid exits, global feature switch).
2. DG Script/mob program action for the pulse, then registered special
   procedure and service authority, using the existing behavior arbitration
   contract.
3. Simple Role defaults.
4. Personality modifiers.
5. Behavior movement/routine rules.
6. Combat Style combat rules.
7. Existing internal AI machinery executes the resolved policy.
8. Legacy mobile flags act only in domains not claimed/suppressed by the audited
   runtime contract.

The implementation must encode this precedence once and test it. Menu code must
not attempt to predict or mutate ownership.

## 5. BuilderBot impact

BuilderBot should use the same numbered MEDIT path as a human; no private engine
back door is necessary. Its canonical transaction is:

1. Enter `medit <vnum>` and choose `I`.
2. Read the `AI Actor` page and set choices `1` through `8` in ascending order.
3. For room fields, submit a VNUM or `none`; wait for the redisplayed Home page.
4. If Behavior is Patrol, enter `6`, issue only `A/D/U/N/L`, then `Q`.
5. Enter `P`, parse the concise effective summary, enter `V`, and require zero
   errors (warnings are reported to the caller).
6. `Q` to MEDIT and use the ordinary save confirmation.

Deterministic requirements:

* Fixed numeric IDs and labels; new enum choices append rather than reorder.
* One command per prompt and one response state; no free-form compound command
  except the explicitly shaped patrol commands.
* No context-dependent renumbering. Patrol remains option 6 even when inactive.
* Every screen begins with a unique, stable title and ends with `Choice:` or
  `Command:`. Errors redisplay the same title.
* Preview exposes authored versus effective values and a machine-stable
  `Validation: OK|WARNING|ERROR` line. Human prose may follow it.
* BuilderBot never edits derived fields and never needs knowledge of
  `MEDIT_AI_*`, override masks, schedule IDs, threat weights, or coefficients.

Add a transcript/golden-path test for one Stationary Civilian and one Patrol
Guard. Add a transition-table test that starts at every public mode, submits
invalid input and `Q`, and proves the expected state and lack of mutation.

## 6. Compatibility risks and mitigations

| Risk | Impact | Mitigation / release gate |
|---|---|---|
| Simple presets subtly change existing NPC combat or movement | High | Legacy-exact records stay on the old resolver until an explicit simple edit; compare runtime profiles in an offline audit and canary zone. |
| DG Script/special/service double action | High | Preserve current arbitration and add pulse-level integration tests for every owned domain; preview names the authority. |
| Saving through new MEDIT destroys hidden advanced values | High | Dual-write, field-local mutations, old/mixed fixture round trips, byte/semantic diff review. |
| Existing schedules have no simple equivalent | High | Retain and execute them for legacy-exact mobs; never expose or silently flatten them. Explicit conversion requires warning/confirmation in a later design. |
| Multiple patrol routes are edited accidentally | High | Persist a designated simple route ID; ambiguous old data is read-only until explicitly selected/converted. |
| Personality projection is misleading | Medium | Label it approximate and do not materialize it on open/save-without-change. |
| Role names imply services AI cannot provide | Medium | Role and Behavior never create shops, trainers, quests, triggers, or programs; validation says when the corresponding service binding is absent. |
| Caster/Archer presets overpromise tactics | Medium | Resolve to supported behavior only and show a limitation warning until combat hooks exist. |
| Dialogue competes with triggers or duplicates lines | Medium | Treat AI text as fallback, preserve extra legacy lines, test event ownership/cooldowns, and defer Death emission until a safe hook is verified. |
| Enum changes corrupt world data | Medium | Explicit stable values, versioned record, append-only choices, unknown-value fallback plus boot warning. |
| Hidden modes remain reachable by stale descriptor state | Medium | Audit every transition and help return; unknown/retired AI modes return safely to the new root. Remove dead cases after a compatibility release. |
| Disabled mobs lose authored intent | Low | Enabled remains a flag independent from persisted simple configuration. |
| Rollback to an older binary | Medium | Keep legacy records authoritative/complete during dual-write and test a new-save/old-load copy in staging. |

## Implementation sequence and acceptance gates

1. **Freeze vocabulary and precedence.** Approve this document, especially the
   single personality model, five dialogue slots, service-role semantics, and
   legacy-exact opt-in boundary.
2. **Add model and resolver behind no UI.** Unit-test every preset and legacy
   comparison; no world records change.
3. **Add versioned persistence with dual-read/dual-write.** Pass old/new/mixed,
   malformed, copy/free, and rollback fixtures.
4. **Replace the reachable MEDIT surface.** Keep old runtime and serialization;
   pass exhaustive state-machine and BuilderBot transcript tests.
5. **Canary runtime activation.** Compare selected prototype behavior before and
   after explicit conversion in a development zone, including DG/special/shop/
   trainer/quest cases.
6. **Ship with telemetry/audit commands.** Review warnings and rollback proof.
7. **Only later remove dead editor code.** Do not combine deletion with the
   behavioral rollout.

The redesign is accepted when a new actor can be authored from MEDIT in one
linear pass, BuilderBot can replay the same transcript without hidden knowledge,
no advanced tuning page is reachable, old-only mobiles retain their prior
runtime path, mixed data round-trips without loss, and legacy systems retain
their tested authority.
