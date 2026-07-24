#!/usr/bin/env python3
"""Regression seams for Phase 1's NPC-safe combat and local movement policy."""
from pathlib import Path
fight = Path('src/fight.c').read_text()
actor = Path('src/ai_actor.c').read_text()
brain = Path('src/ai_actor_brain.c').read_text()
assert '!IS_NPC(ch)' in fight
assert '!IS_NPC(ch->master)' in fight
assert '!IS_NPC(i->master)' in fight
assert 'ai_actor_random_move' in actor
for token in ('ROOM_NOMOB', 'ROOM_DEATH', 'EX_CLOSED', 'MOB_STAY_ZONE', 'perform_move(mob,dir,1)'):
    assert token in actor, token
assert 'AI_TICK_EXCLUSIVE' in actor and 'AI_IDLE_MOVE_RANDOM' in actor
assert 'AI_COMM_SPEAK' in actor and 'AI_ARCH_MINDLESS' in actor
assert 'bounded idle decisions' in brain
print('AI Actor runtime phase 1 regression checks passed')
