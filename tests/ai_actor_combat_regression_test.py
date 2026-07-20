#!/usr/bin/env python3
"""Deterministic source-level guard for the bounded AI actor combat policy."""
from pathlib import Path
source = Path('src/ai_actor.c').read_text()
header = Path('src/ai_actor.h').read_text()
db = Path('src/db.c').read_text()
writer = Path('src/genmob.c').read_text()
assert 'enum ai_combat_style' in header
assert 'AIConfigCombat' in db and 'AIConfigTargetWeight' in db
assert 'AIConfigCombat:' in writer and 'AIConfigTargetWeight:' in writer
assert 'ai_actor_target_score' in source and 'ROOM_PEACEFUL' in source
assert 'ai_actor_is_local_ally' in source and 'same prototype' in source
assert 'ai_actor_should_flee' in source and 'MOB_SENTINEL' in source
assert 'do_flee(mob,"",0,0)' in source
assert 'best_score >= current_score + ai_style_switch_threshold(mob->ai_prof)' in source
assert 'struct ai_help_event' in header and 'ai_help_event_admit' in source
assert 'AI_HELP_EVENT_MAX' in header and 'ai_actor_dispatch_help' in source
assert 'group member' in source and 'GROUP(mob) == GROUP(other)' in source
assert 'AI_COMBAT_CONTROLLER' in source and 'AI_COMBAT_BOSS' in source
assert 'ai_style_switch_threshold' in source
assert 'last_flee_attempt' in source
# Lifecycle transitions are event driven and use bounded ID bookkeeping.
for token in ('combat_event_id', 'combat_active', 'last_help_heard_event_id',
              'last_help_answered_event_id', 'ai_actor_event_combat_end',
              'ai_actor_event_defeat', 'ai_actor_event_fled'):
    assert token in header or token in source
assert 'IN_ROOM(mob)!=before || !FIGHTING(mob)' in source
assert 'if(FIGHTING(mob)==target' in source
assert 'mob==caller' in source
assert 'ai_actor_lifecycle_memory' in source
# Preview and validation are non-mutating report helpers exposed by combat MEDIT.
for token in ('ai_actor_combat_preview', 'ai_actor_combat_validate',
              'Combat Profile', 'Target Weights:', 'Lifecycle tracking:',
              'Rescue: Unavailable', 'Surrender: Unavailable',
              'Pursuit: ', 'ERROR:', 'WARNING:'):
    assert token in source
medit = Path('src/medit.c').read_text()
assert 'K) Preview L) Validate' in medit
assert "LOWER(*arg)=='k'" in medit and "LOWER(*arg)=='l'" in medit
print('ai actor combat regression checks passed')
