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
print('ai actor combat regression checks passed')
