# MEDIT as an NPC Behavior Editor

## Philosophy

MEDIT describes **what an NPC can do**, not which runtime subsystem happens to do it. A builder starts from movement, communication, awareness, memory, combat, objects, sounds, and personality. Technical provenance is deliberately confined to **Diagnostics & Sources**. This is an editor-only presentation and synchronization change: it does not migrate specials, replace the AI runtime, or alter pulse ordering.

The compatibility NPC Flags page remains on the main MEDIT screen. It and the behavior pages are two views of the same prototype bits; there is no copied setting to drift out of sync.

## Before and after

### Before

```text
Legacy Behavior:                 (read-only implementation report)
  Special / origin / domains
...
A) NPC Flags
R) Loadout / Loot
I) AI Actor Configuration: Disabled / Enabled
S) DG Scripts
```

```text
AI Actor
1) Personality
2) Communication
3) Daily Routine
4) Combat Behavior
5) Preview NPC
6) Diagnostics
7) Behavior Ownership
A) Advanced AI Brain
```

### After

```text
... creature identity and statistics ...
A) NPC Flags                      (advanced compatibility view)
R) Loadout / Loot
I) NPC Behavior
S) DG Scripts
```

```text
NPC Behavior
1) General Behavior
2) Movement & Routine
3) Communication
4) Awareness & Senses
5) Memory & Relationships
6) Combat Behavior
7) Object Interaction
8) Creature Sounds
9) Advanced Behavior
D) Diagnostics & Sources
P) Preview Everything This NPC Can Do
Q) Return
```

## Why each page exists

- **General Behavior** groups personality, intelligence, role, and the plain-language preview. These answer what kind of creature this is before fine tuning individual actions.
- **Movement & Routine** puts wandering, `SENTINEL`, `STAY_ZONE`, movement style, schedules, destinations, and patrols together. Options 1–3 directly toggle the existing flag bits, so the NPC Flags compatibility page immediately shows the same result.
- **Communication** keeps spoken dialogue, greetings, ambient speech, replies, frequency, presence gates, cooldowns, and conversation previews in one workflow.
- **Awareness & Senses** exposes observed events, hearing, recognition, suspicion, and identity confidence as sensing behavior.
- **Memory & Relationships** displays the compatibility `MEMORY` setting beside relationship memory, attacker memory, trust, fear, hostility, familiarity, and remembered event types. `X` changes the same `MEMORY` bit used by NPC Flags.
- **Combat Behavior** combines aggression alignment flags, `HELPER`, `WIMPY`, combat reactions, style, target selection, assistance, fleeing, and threat escalation. Flag changes are immediately synchronized with NPC Flags.
- **Object Interaction** combines `SCAVENGER` with equipment, inventory, and loot. Unsupported general pickup/drop policy is visible and locked rather than silently absent.
- **Creature Sounds** is a first-class destination while still also being reachable through Communication, because non-speaking creatures should not require a dialogue mental model.
- **Advanced Behavior** preserves detailed configuration for experienced builders without making it the normal entry point.
- **Diagnostics & Sources** is the only behavior page that foregrounds implementation. It reports sources per domain, attached DG Scripts, known or unknown specials, conflicts, warning count, and detailed compatibility provenance.

## Reorganization report

### Menus moved

| Previous location | New location |
|---|---|
| AI personality | General Behavior |
| AI daily routine, schedule, destinations, patrols | Movement & Routine |
| AI perception | Awareness & Senses |
| AI memory details | Memory & Relationships |
| AI combat and threat response | Combat Behavior |
| Creature vocalizations below capabilities/dialogue | Creature Sounds and Communication |
| Loadout plus `SCAVENGER` flag in separate main-menu areas | Object Interaction (loadout remains directly available for compatibility) |
| Behavior ownership and compatibility detail | Advanced Behavior / Diagnostics & Sources |
| Read-only Legacy Behavior block | Removed from the main screen; source detail moved to Diagnostics & Sources |

### Menus renamed

- **AI Actor Configuration** → **NPC Behavior**.
- **AI Actor Role** → **NPC Role**.
- **AI Actor Movement** → **Movement Style**.
- **AI Actor Perception** → **Awareness & Senses**.
- **AI Actor Memory** → **Memory & Relationships**.
- **AI Actor Combat Reactions** → **Combat Reactions**.
- **AI Actor Schedule Entries** → **Scheduled Destinations**.
- **AI Actor Patrol Routes** → **Patrol Routes**.
- **Advanced AI Brain** → **Advanced Behavior**.
- Builder help headings now say **NPC Behavior** rather than naming the implementation.

### Legacy features surfaced

- `SENTINEL` and ordinary wandering.
- `STAY_ZONE` travel boundary.
- `MEMORY` attacker retaliation compatibility.
- `AGGR`, `AGGR_EVIL`, `AGGR_GOOD`, and `AGGR_NEUTRAL` initiation.
- `HELPER` assistance.
- `WIMPY` fleeing.
- `SCAVENGER` object pickup.
- Equipment, inventory, and loot tables.
- Known special routine/movement ownership and attached DG Scripts in diagnostics.
- The complete NPC Flags editor remains available as the advanced compatibility view.

### Configured behavior features surfaced

- Personality traits and presets; intelligence, role, and capabilities.
- Random movement settings and delay.
- Random destinations, scheduled destinations, travel settings, routines, and patrol routes.
- Dialogue categories, ambient speech, replies, presence requirements, frequency, cooldown, per-room limits, emotes, whispers, and creature sounds.
- Perception event gates, hearing/observation, suspicion, recognition, and identity confidence.
- Relationship capacity and duration; trust, fear, hostility, familiarity, forgiveness, and remembered events.
- Combat style, initiation, ally protection, help calls, flee rules, target switching and weights, and threat escalation.
- Effective-behavior preview, validation, ownership, and diagnostics.

### Synchronization added

Behavior pages now write the prototype flag bits directly for `SENTINEL`, `STAY_ZONE`, `MEMORY`, all four aggression flags, `HELPER`, `WIMPY`, and `SCAVENGER`. NPC Flags reads and writes those exact bits. There is no translation table, mirror field, save hook, or second value. Schedule room validation continues to read `STAY_ZONE`, so changes from either view affect destination validation immediately. The existing AI configuration, ownership persistence, special metadata, and loadout storage are reused unchanged.

### Remaining locked behavior

- A known hard-coded special's movement/routine is displayed with source, **Locked** status, reason, and migration prerequisite. Mayor routes therefore remain owned by the Mayor special.
- Unknown custom specials are reported in Diagnostics & Sources; unsafe ownership changes remain locked.
- General pickup rules and deliberate drop rules are visible as locked because no editable runtime behavior exists yet.
- Several stored advanced reactions and target preferences remain unsupported as already identified by their detailed editors; the redesign does not invent runtime support.
- DG Scripts remain an external behavior source. They are reported, not rewritten or arbitrated.

## Recommendations before Mayor migration

1. Specify route behavior in builder vocabulary: waypoints, wait times, door actions, announcements, posture, interruption policy, and return-home behavior.
2. Capture golden tests for the current Mayor's time-of-day route, door operations, speech, return values, and shared static route-state edge cases.
3. Decide how multiple Mayor instances should own independent progress; do not preserve shared static state accidentally.
4. Define an explicit import/preview step that maps the hard-coded route to the existing routine and patrol editors without silently changing a prototype.
5. Keep the special authoritative until parity tests pass, then remove its movement/routine lock as one atomic migration. Do not mix migration with this editor redesign.
6. Add a diagnostics comparison showing current-special actions versus the candidate authored routine before allowing ownership to change.
