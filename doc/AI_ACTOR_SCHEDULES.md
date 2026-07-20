# AI Actor schedules

Schedules are persistent prototype configuration (`mob_ai_config`): authored locations,
entries, routes, waypoint values, and category-level dialogue pools are saved in mobile
ESpec records.  Runtime state is per live mobile in `ai_actor_state`; it is deliberately
not serialized and includes selection, retry, interruption, expected-room, and patrol
position values.

The canonical clock is `time_info.month`, `time_info.day`, and `time_info.hours`.  A
compiled preview uses that game calendar, not host time.  Entries select by higher
priority, more-specific day mask, narrower time window, and finally stored order.

Movement is deliberately bounded: schedules only move to a directly adjacent destination
or authored adjacent patrol waypoint, one `perform_move(mob, direction, 1)` per eligible
cycle.  Arbitrary non-adjacent destination travel is unavailable, no BFS pathfinding is
used, and fallback travel is limited by that same adjacent model.  Route modes are Loop,
Ping-pong, and Once.

Builders use MEDIT's AI Schedule page to edit entries and patrol routes, then use Preview
and Validate.  Preview is read-only and shows compiled selection, resolved rooms,
traversal, and wandering effect.  Validation is read-only and reports Schedule, Patrol,
and Cross-System sections.  `aistate <mob>` shows prototype configuration and live runtime
state without selecting, moving, or changing timers.

Combat and major threats interrupt schedule execution; generic wandering is blocked only
while the runtime schedule state says it is active.  DG scripts and special procedures
retain normal movement pathways and can therefore displace an actor.  Per-entry and
per-waypoint dialogue-category fields do not exist: schedule dialogue uses the existing
category-level configuration.  Tests combine callable pure helper coverage already in the
schedule regression harness with source-level integration guards for ESpec, OLC, runtime,
and diagnostics.
