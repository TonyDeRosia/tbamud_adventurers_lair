"""Regression checks for dialogue editor validation and guided commands."""
from pathlib import Path
s = Path('src/medit.c').read_text()
for token in ('Dialogue line added as entry', 'Dialogue line cannot be empty.',
              'Dialogue line is too long. Maximum:', 'This category already contains',
              'Line number to %s', 'Please enter a line number from 1 to %d',
              'There are no lines to %s.', 'Dialogue line %d moved'):
    assert token in s, token
assert 'mob_ai_dialogue_set(OLC_MOB(d)->ai_config,category,count,arg)' in s
assert 'AI_DIALOGUE_LINE_MAX-1' in s
print('AI Actor dialogue editor regression checks passed')
