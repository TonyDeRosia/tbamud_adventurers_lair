"""Regression checks for readable schedule lists and guided entry commands."""
from pathlib import Path
s = Path('src/medit.c').read_text()
for token in ('AI Actor Schedule Entries', '#  State     Time       Days', 'All Day',
              'Entry number to %s', 'Please enter an entry number from 1 to %d',
              'Entry %d does not exist. Valid entries', 'already first',
              'Show the day\'s effective activity timeline'):
    assert token in s, token
print('AI Actor schedule editor regression checks passed')
