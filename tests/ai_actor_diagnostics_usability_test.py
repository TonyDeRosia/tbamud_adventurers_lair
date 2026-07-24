#!/usr/bin/env python3
"""Source-level UX regression checks for builder-facing diagnostics."""
from pathlib import Path
medit = Path('src/medit.c').read_text()
actor = Path('src/ai_actor.c').read_text()
for text in ('Diagnostics lists only issues a builder can act on', 'Communication is Silent', 'SENTINEL prevents the active schedule', 'Advanced Technical Diagnostics'):
    assert text in medit, text
assert 'NPC Behavior Diagnostics' in actor
assert 'MEDIT_AI_DIAGNOSTICS' in medit
assert 'MEDIT_AI_COMPATIBILITY' in medit
print('AI Actor Builder Diagnostics usability checks passed')
