"""Source-level regression coverage for Phase 3 AI Actor builder guidance."""
from pathlib import Path
m = Path('src/medit.c').read_text()
r = Path('src/ai_actor.c').read_text()
for title in ('AI Actor Perception', 'AI Actor Memory', 'AI Actor Threat Response',
              'AI Actor Combat Reactions', 'Combat Target Weights', 'Schedule',
              'AI Actor Patrol Route'):
    assert title in m, title
for token in ('Notice Entry', 'Recognition', 'Memory Enabled', 'Remember Attacks',
              'Cooldown: %d seconds', 'Flee Health Threshold', 'Current attacker',
              'Ping-pong', 'H) Help'):
    assert token in m, token
# Refactored audited editors use strict conversion helpers, not atoi.
for mode in ('MEDIT_AI_MEMORY_VALUE', 'MEDIT_AI_COMBAT_VALUE', 'MEDIT_AI_PERCEPTION_VALUE'):
    parser = m[m.index('void medit_parse('):]
    section = parser[parser.index('case ' + mode):]
    assert 'medit_parse_ai_integer' in section[:2500]
assert 'strtol(arg, &end, 10)' in m
assert 'ai_actor_schedule_validate' in r
assert 'ai_actor_combat_validate' in r
print('AI Actor Phase 3 usability regression checks passed')
