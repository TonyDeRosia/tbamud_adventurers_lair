"""Separate historical schedule data, runtime, and builder reachability guards."""
from pathlib import Path
h = Path('src/ai_actor.h').read_text()
s = Path('src/ai_actor.c').read_text()
db = Path('src/db.c').read_text()
gen = Path('src/genmob.c').read_text()
assert 'AI_SCHEDULE_MAX 16' in h and 'AI_PATROL_MAX 8' in h
for name in ('ai_schedule_time_matches', 'ai_schedule_day_matches', 'ai_schedule_select',
             'ai_schedule_entries_overlap', 'ai_patrol_advance'):
    assert name in s
assert 'start<end ? hour>=start&&hour<end : hour>=start||hour<end' in s
assert 'perform_move(m,dir,1)' in s
assert 'ROOM_NOMOB' in s and 'ROOM_DEATH' in s and 'MOB_STAY_ZONE' in s
for record in ('AIConfigSchedule:', 'AISchedule:', 'AIPatrol:', 'AIPatrolWaypoint:'):
    assert record in gen
for record in ('AIConfigSchedule', 'AISchedule', 'AIPatrol', 'AIPatrolWaypoint'):
    assert record in db
assert 'MOB_AI_ACTOR' in s and 'FIGHTING(m)' in s
print('AI actor schedule data compatibility checks passed')
medit = Path('src/medit.c').read_text()
oasis = Path('src/oasis.h').read_text()
for name in ('medit_disp_ai_schedule_entries', 'medit_disp_ai_schedule_entry',
             'medit_disp_ai_patrol_routes', 'medit_disp_ai_patrol_route',
             'medit_disp_ai_patrol_waypoints', 'medit_disp_ai_patrol_waypoint'):
    assert name in medit
for token in ('MEDIT_AI_SCHEDULE_ROOM',
              'MEDIT_AI_SCHEDULE_ENTRIES', 'MEDIT_AI_PATROL_ROUTES',
              'ai_patrol_delete', 'ai_patrol_move', 'ai_patrol_duplicate',
              'ai_patrol_waypoint_add', 'ai_patrol_waypoint_delete',
              'ai_patrol_waypoint_duplicate', 'ai_patrol_waypoint_move',
              'Referenced route cannot be deleted', 'Invalid room VNUM'):
    assert token in medit or token in oasis
print('AI actor hidden schedule compatibility code checks passed')

# Historical fields and modes remain loadable, but primary menu input cannot
# transition to the old schedule, patrol, movement, or broad behavior pages.
menu_start = medit.rindex('static void medit_disp_ai_menu(struct descriptor_data *d)')
root_menu = medit[menu_start:medit.index('static void medit_disp_legacy_menu', menu_start)]
parse_start = medit.index('case MEDIT_AI_MENU:', medit.index('void medit_parse('))
root_parse = medit[parse_start:medit.index('case MEDIT_LEGACY_MENU:', parse_start)]
assert 'medit_disp_ai_schedule' not in root_parse
assert 'medit_disp_ai_patrol' not in root_parse
assert 'medit_disp_ai_movement' not in root_parse
assert 'default: medit_disp_ai_menu(d); return;' in root_parse
for obsolete in ('Daily Routine', 'Patrol Routes', 'Home Room', 'Work Room'):
    assert obsolete not in root_menu

# Copy/save paths preserve the entire AI configuration; merely displaying MEDIT
# has no schedule mutator and cannot erase dormant records.
genmob = Path('src/genmob.c').read_text()
assert 'copy_ai_config' in gen or 'mob_ai_config_copy' in gen
assert 'AIConfigSchedule:' in genmob and 'AISchedule:' in genmob
assert 'schedule_count' not in root_menu
print('AI actor builder reachability and preservation checks passed')

# Runtime policy engine guards: stable state, interruption, retry, and wandering helpers.
for token in (
    'AI_SCHED_RUNTIME_DISABLED', 'AI_SCHEDULE_INTERRUPT_COMBAT',
    'AI_SCHEDULE_INTERRUPT_UNKNOWN_DISPLACEMENT', 'AI_SCHEDULE_RESUME_VALID',
    'AI_SCHEDULE_ALLOW_WANDER', 'ai_schedule_set_state',
    'ai_schedule_begin_entry', 'ai_schedule_complete_entry',
    'ai_schedule_reset_transition_flags', 'ai_schedule_apply_interruption_policy',
    'ai_schedule_apply_failure_policy', 'ai_schedule_resume_check',
    'ai_schedule_interruption_is_minor', 'ai_schedule_entry_activation_signature',
    'ai_schedule_entry_is_suppressed_for_window', 'ai_schedule_retry_ready',
    'ai_schedule_travel_timed_out', 'ai_schedule_should_block_wandering'):
    assert token in h or token in s, token
assert 'perform_move(m,dir,1)' in s
assert 's->schedule_attempts++;ai_schedule_failure' in s
assert 's->schedule_failure_emitted' in s and 's->schedule_failure_applied' in s
assert 's->resume_schedule_id' in s and 's->resume_waypoint' in s
assert 'ai_schedule_mark_skip(s,e)' in s
assert 'AI_PATROL_LOOP' in s and 'AI_PATROL_PINGPONG' in s and 'AI_PATROL_ONCE' in h
print('AI actor schedule runtime policy regression checks passed')
# Final schedule reporting/diagnostic integration guards.
assert 'Compiled prototype preview;' in s and 'live runtime suppression' in s
assert 'Schedule Errors' in s and 'Patrol Errors' in s and 'Cross-System Warnings' in s
assert 'Winner explanation: eligible; canonical selection' in s and 'compares higher priority' in s
assert 'Traversal: %s' in s and 'loop closure' in s
assert 'void ai_actor_schedule_show_state' in s
assert 'Schedule Diagnostics (read-only)' in s
assert 'ai_actor_schedule_show_state(ch, mob)' in open('src/act.wizard.c').read()
assert 'memset(it->ai_state, 0, sizeof(*it->ai_state))' in s
assert 'AIConfigSchedule' in open('src/db.c').read()
print('AI actor schedule reporting, diagnostics, persistence, and refresh guards passed')
