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
assert 'best_score >= current_score + mob->ai_prof->target_switch_threshold' in source
assert 'responders < mob->ai_prof->max_responders' in source
print('ai actor combat regression checks passed')
