"""Source-level checks for the compact builder-first AI Actor menus."""
from pathlib import Path
s = Path('src/medit.c').read_text()
for token in ('AI Actor', 'Communication', 'Intelligence', 'Schedule', 'Dialogue',
              'Advanced AI Brain', 'AI Actor Social Behavior', 'Idle Behavior', 'Cooldowns'):
    assert token in s, token
assert 'Selects inferred, custom' not in s
print('AI Actor visual hierarchy regression checks passed')
