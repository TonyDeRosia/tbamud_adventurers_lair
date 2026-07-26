# AI Actor editor navigation audit

## Scope and method

This audit covers all 80 `MEDIT_AI_*` constants declared in `src/oasis.h`.  The
root of the reachability graph is the **visible** `AI Actor Extensions` menu,
not a display function which can only be called from another disconnected
display function.  A parser `case` alone is therefore not evidence that a
builder can reach a mode.

The audit followed each displayed command through `medit_parse()`, checked the
mode set by the destination display/prompt, and then followed its explicit
return or cancel path.  Conditional edges were also checked against the text
shown to the builder.  In particular, a legacy special owning ambient speech,
or any attached DG Script, keeps Communication unavailable.

## Reachability graph

```text
MEDIT main menu
  -> AI Actor enable confirmation (only when enabling the feature)
  -> AI Actor Extensions (MEDIT_AI_MENU)
       1 -> Personality -> Trait | Preset -> Personality
       2 -> Identity / Role -> AI Actor Extensions
       3 -> Advanced Perception -> Perception Value -> Advanced Perception
       D -> Diagnostics -> Compatibility detail | preview
       P -> preview (read-only help/return screen)
       Q -> MEDIT main menu
```

The audit found no verified missing edge in the visible graph.  In particular,
the root's “Timed Speech: Available” line is a capability/status report, not a
numbered command.  The legacy-first menu contract intentionally exposes only
Personality, Identity/Role, Advanced Perception, diagnostics, and preview.
Communication and its children are retained internal editors; their parser
branches are not permission to expose them.

## Classification of all modes

“Internal-only” includes the older advanced editor graph retained for data
maintenance and regression compatibility.  It is deliberately disconnected
from the builder-visible extension menu where legacy flags, specials, or
mobile activity remain authoritative.  “Dead/obsolete” means that no live
transition assigns the mode; retaining its numeric constant avoids renumbering
the Oasis mode ABI.

| Mode | Classification | Verified entry / disposition |
|---|---|---|
| `MEDIT_AI_MENU` | directly reachable from a visible menu | MEDIT's visible AI Actor choice; returns to MEDIT. |
| `MEDIT_AI_MODE` | internal-only and intentionally not user-navigable | Dormant Advanced/Profile picker; return is safe but Advanced is not rooted. |
| `MEDIT_AI_ROLE` | directly reachable from a visible menu | Root choice 2; `Q` returns to the root. |
| `MEDIT_AI_MOVEMENT` | internal-only and intentionally not user-navigable | Dormant Advanced/Movement Internals; legacy movement remains authoritative. |
| `MEDIT_AI_PERSONALITY` | directly reachable from a visible menu | Root choice 1; `Q` returns to the root. |
| `MEDIT_AI_TRAIT` | reachable through a verified submenu | Personality's numbered/A-C value prompt. |
| `MEDIT_AI_PRESET` | reachable through a verified submenu | Personality `P`; completion/cancel returns to Personality. |
| `MEDIT_AI_SOCIAL` | internal-only and intentionally not user-navigable | Dormant Advanced Assistance/Social editor. |
| `MEDIT_AI_SOCIAL_VALUE` | internal-only and intentionally not user-navigable | Child value prompt of dormant Social. |
| `MEDIT_AI_DIALOGUE` | internal-only and intentionally not user-navigable | Child category editor of the retained, unrooted Communication graph. |
| `MEDIT_AI_DIALOGUE_ADD` | internal-only and intentionally not user-navigable | Child prompt of retained Dialogue. |
| `MEDIT_AI_DIALOGUE_EDIT` | internal-only and intentionally not user-navigable | Child prompt of retained Dialogue. |
| `MEDIT_AI_DIALOGUE_DELETE` | dead or obsolete | Superseded by the operation encoded in `MEDIT_AI_DIALOGUE_INDEX`. |
| `MEDIT_AI_DIALOGUE_REORDER` | dead or obsolete | Superseded by the operation encoded in `MEDIT_AI_DIALOGUE_INDEX`. |
| `MEDIT_AI_ENABLE_CONFIRM` | internal-only and intentionally not user-navigable | Transitional confirmation owned by the outer MEDIT menu, not an AI submenu choice. |
| `MEDIT_AI_PERCEPTION` | directly reachable from a visible menu | Root choice 3; `Q` returns to the root. |
| `MEDIT_AI_PERCEPTION_VALUE` | reachable through a verified submenu | Perception field prompt; completion returns to Perception. |
| `MEDIT_AI_MEMORY` | internal-only and intentionally not user-navigable | Dormant Advanced detail; the legacy MEMORY flag is authoritative. |
| `MEDIT_AI_MEMORY_VALUE` | internal-only and intentionally not user-navigable | Child value prompt of dormant Memory. |
| `MEDIT_AI_THREAT` | internal-only and intentionally not user-navigable | Dormant Advanced detail. |
| `MEDIT_AI_THREAT_VALUE` | internal-only and intentionally not user-navigable | Child value prompt of dormant Threat. |
| `MEDIT_AI_THREAT_SEQUENCE` | internal-only and intentionally not user-navigable | Child editor of dormant Threat. |
| `MEDIT_AI_THREAT_STEP` | dead or obsolete | No live assignment; sequence edits are parsed inline. |
| `MEDIT_AI_COMBAT` | internal-only and intentionally not user-navigable | Dormant Advanced detail; legacy combat flags remain authoritative. |
| `MEDIT_AI_COMBAT_VALUE` | internal-only and intentionally not user-navigable | Child value prompt of dormant Combat. |
| `MEDIT_AI_TARGETS` | internal-only and intentionally not user-navigable | Child target-weight editor of dormant Combat. |
| `MEDIT_AI_TARGET_VALUE` | internal-only and intentionally not user-navigable | Child value prompt of dormant Targets. |
| `MEDIT_AI_SCHEDULE` | internal-only and intentionally not user-navigable | Complete retained schedule graph, withheld because legacy movement/routine is authoritative. |
| `MEDIT_AI_SCHEDULE_EDIT` | dead or obsolete | No live assignment; replaced by Entries/Entry modes. |
| `MEDIT_AI_SCHEDULE_ROOM` | dead or obsolete | Old travel-room prompt has a parser remnant but no live assignment. |
| `MEDIT_AI_SCHEDULE_FAILURE` | internal-only and intentionally not user-navigable | Failure prompt below dormant Schedule Entry. |
| `MEDIT_AI_SCHEDULE_ENTRIES` | internal-only and intentionally not user-navigable | Child list below dormant Schedule. |
| `MEDIT_AI_SCHEDULE_ENTRY` | internal-only and intentionally not user-navigable | Child editor below dormant Schedule Entries. |
| `MEDIT_AI_SCHEDULE_ENTRY_VALUE` | internal-only and intentionally not user-navigable | Child numeric/room prompt below dormant Schedule Entry. |
| `MEDIT_AI_SCHEDULE_DAYS` | internal-only and intentionally not user-navigable | Child day-mask prompt below dormant Schedule Entry. |
| `MEDIT_AI_SCHEDULE_ACTIVITY` | internal-only and intentionally not user-navigable | Child activity prompt below dormant Schedule Entry. |
| `MEDIT_AI_SCHEDULE_DESTINATION` | internal-only and intentionally not user-navigable | Child destination prompt below dormant Schedule Entry. |
| `MEDIT_AI_SCHEDULE_ROUTE` | internal-only and intentionally not user-navigable | Child route prompt below dormant Schedule Entry. |
| `MEDIT_AI_SCHEDULE_ACTION` | internal-only and intentionally not user-navigable | Child action prompt below dormant Schedule Entry. |
| `MEDIT_AI_SCHEDULE_INTERRUPT` | internal-only and intentionally not user-navigable | Child interruption prompt below dormant Schedule Entry. |
| `MEDIT_AI_SCHEDULE_DIALOGUE` | dead or obsolete | No live assignment; schedule dialogue now uses ordinary dialogue categories. |
| `MEDIT_AI_SCHEDULE_DELETE` | internal-only and intentionally not user-navigable | Confirmation below dormant Schedule Entries. |
| `MEDIT_AI_PATROL_ROUTES` | internal-only and intentionally not user-navigable | Advanced Patrol child of dormant Schedule. |
| `MEDIT_AI_PATROL_ROUTE` | internal-only and intentionally not user-navigable | Route editor below dormant Patrol Routes. |
| `MEDIT_AI_PATROL_LABEL` | internal-only and intentionally not user-navigable | Child prompt below dormant Patrol Route. |
| `MEDIT_AI_PATROL_MODE` | internal-only and intentionally not user-navigable | Child prompt below dormant Patrol Route. |
| `MEDIT_AI_PATROL_FAILURE` | internal-only and intentionally not user-navigable | Child prompt below dormant Patrol Route. |
| `MEDIT_AI_PATROL_DELETE` | internal-only and intentionally not user-navigable | Confirmation below dormant Patrol Routes. |
| `MEDIT_AI_PATROL_WAYPOINTS` | internal-only and intentionally not user-navigable | Child list below dormant Patrol Route. |
| `MEDIT_AI_PATROL_WAYPOINT` | internal-only and intentionally not user-navigable | Child editor below dormant Waypoints. |
| `MEDIT_AI_DIALOGUE_INDEX` | internal-only and intentionally not user-navigable | Child operation prompt of retained Dialogue. |
| `MEDIT_AI_SCHEDULE_INDEX` | internal-only and intentionally not user-navigable | Index prompt below dormant Schedule Entries. |
| `MEDIT_AI_HELP` | diagnostics/preview-only | Shared read-only help/preview return screen; stored caller controls return. |
| `MEDIT_AI_COMPATIBILITY` | diagnostics/preview-only | Diagnostics `D` (and dormant technical screens); read-only report. |
| `MEDIT_AI_CAPABILITIES` | internal-only and intentionally not user-navigable | Dormant Advanced capability override editor. |
| `MEDIT_AI_CAPABILITY_VALUE` | internal-only and intentionally not user-navigable | Child picker/value prompt of dormant Capabilities. |
| `MEDIT_AI_VOCALIZATIONS` | internal-only and intentionally not user-navigable | Child editor of retained Communication. |
| `MEDIT_AI_VOCALIZATION_ADD` | internal-only and intentionally not user-navigable | Child prompt of retained Creature Sounds. |
| `MEDIT_AI_VOCALIZATION_EDIT` | internal-only and intentionally not user-navigable | Child prompt of retained Creature Sounds. |
| `MEDIT_AI_VOCALIZATION_INDEX` | internal-only and intentionally not user-navigable | Child entry editor of retained Creature Sounds. |
| `MEDIT_AI_PATROL_WAYPOINT_VALUE` | internal-only and intentionally not user-navigable | Child value prompt below dormant Waypoint editor. |
| `MEDIT_AI_PATROL_WAYPOINT_ACTION` | dead or obsolete | No live assignment; action is handled by Waypoint Value. |
| `MEDIT_AI_PATROL_WAYPOINT_DELETE` | internal-only and intentionally not user-navigable | Confirmation below dormant Waypoints. |
| `MEDIT_AI_ADVANCED` | internal-only and intentionally not user-navigable | Retained technical hub with no visible root transition by design. |
| `MEDIT_AI_COMMUNICATION` | internal-only and intentionally not user-navigable | Retained communication graph; deliberately absent from the legacy-first root commands. |
| `MEDIT_AI_INTELLIGENCE` | internal-only and intentionally not user-navigable | Retained preset editor; no visible AI root command. |
| `MEDIT_AI_DIAGNOSTICS` | directly reachable from a visible menu | Root `D`; `Q` returns to root. |
| `MEDIT_AI_VOCAL_PRESENCE` | internal-only and intentionally not user-navigable | Child prompt of retained Communication. |
| `MEDIT_AI_VOCAL_FREQUENCY` | internal-only and intentionally not user-navigable | Child prompt of retained Communication. |
| `MEDIT_AI_VOCAL_COOLDOWN` | internal-only and intentionally not user-navigable | Child prompt of retained Communication. |
| `MEDIT_AI_VOCAL_LIMIT` | internal-only and intentionally not user-navigable | Child prompt of retained Communication. |
| `MEDIT_AI_OWNERSHIP` | internal-only and intentionally not user-navigable | Retained Phase 2B technical arbitration editor; no visible root edge. |
| `MEDIT_AI_OWNERSHIP_VALUE` | internal-only and intentionally not user-navigable | Child picker below dormant Ownership. |
| `MEDIT_AI_OWNERSHIP_RESET` | internal-only and intentionally not user-navigable | Confirmation below dormant Ownership. |
| `MEDIT_AI_ROUTINE_RANDOM` | internal-only and intentionally not user-navigable | Child editor below dormant Schedule. |
| `MEDIT_AI_ROUTINE_RANDOM_ENTRY` | internal-only and intentionally not user-navigable | Entry editor below dormant Random Routine. |
| `MEDIT_AI_ROUTINE_RANDOM_VNUM` | internal-only and intentionally not user-navigable | Child VNUM prompt below dormant Random Routine. |
| `MEDIT_AI_ROUTINE_WAIT` | internal-only and intentionally not user-navigable | Child wait prompt below dormant Random Routine. |
| `MEDIT_AI_ROUTINE_MODE` | internal-only and intentionally not user-navigable | Child mode prompt below dormant Schedule. |
| `MEDIT_AI_ROUTINE_TIMING` | dead or obsolete | No live assignment; schedule timing is edited in Schedule Entry. |

## Result

The visible graph has no verified missing transition or unsafe return path.
No editor transition was changed: parser-only and legacy-first internal states
were not exposed, and no AI runtime behavior was changed.
