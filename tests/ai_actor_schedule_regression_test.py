"""Deterministic source-level regression guard for bounded AI schedules."""
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
print('AI actor schedule regression checks passed')
medit = Path('src/medit.c').read_text()
oasis = Path('src/oasis.h').read_text()
for name in ('medit_disp_ai_schedule_entries', 'medit_disp_ai_schedule_entry',
             'medit_disp_ai_patrol_routes', 'medit_disp_ai_patrol_route',
             'medit_disp_ai_patrol_waypoints', 'medit_disp_ai_patrol_waypoint'):
    assert name in medit
for token in ('Home Room', 'Work Room', 'Sleep Room', 'Guard Room', 'Fallback Room',
              'Schedule Entries', 'Patrol Routes', 'MEDIT_AI_SCHEDULE_ROOM',
              'MEDIT_AI_SCHEDULE_ENTRIES', 'MEDIT_AI_PATROL_ROUTES',
              'ai_patrol_delete', 'ai_patrol_move', 'ai_patrol_duplicate',
              'ai_patrol_waypoint_add', 'ai_patrol_waypoint_delete',
              'ai_patrol_waypoint_duplicate', 'ai_patrol_waypoint_move',
              'Referenced route cannot be deleted', 'Invalid room VNUM'):
    assert token in medit or token in oasis
print('AI actor schedule Oasis editor regression checks passed')
