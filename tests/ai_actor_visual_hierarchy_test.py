"""Source-level checks for compact, color-aware AI Actor menus."""
from pathlib import Path
s = Path('src/medit.c').read_text()
for token in ('AI Actor Configuration', 'Profile', 'Interaction', 'World Behavior',
              'CCCYN(d->character', 'CCGRN(d->character', 'CCYEL(d->character',
              'AI Actor Social Behavior', 'Idle Behavior', 'Cooldowns'):
    assert token in s, token
assert 'Selects inferred, custom' not in s
print('AI Actor visual hierarchy regression checks passed')
