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
