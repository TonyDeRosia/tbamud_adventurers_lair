"""Regression checks for the builder-first AI Actor MEDIT flow."""
from pathlib import Path

medit = Path('src/medit.c').read_text()
root = medit[medit.index('static void medit_disp_ai_menu(struct descriptor_data *d)\n{'):medit.index('/* AI modes accept', medit.index('static void medit_disp_ai_menu'))]
for text in ('1) Communication', '2) Intelligence', '3) Schedule', '4) Dialogue',
             '5) Preview', '6) Diagnostics', 'A) Advanced AI Brain', 'H) Help', 'Q) Return'):
    assert text in root, text
for forbidden in ('Profile Mode', 'Role:', 'Movement:', 'Capabilities', 'Perception', 'Memory', 'Threat Response', 'Combat Reactions'):
    assert forbidden not in root, forbidden
for text in ('Silent', 'Creature Sounds', 'Speaks'):
    assert text in medit[medit.index('static void medit_disp_ai_communication'):medit.index('static void medit_disp_ai_intelligence')]
for text in ('Mindless', 'Animal', 'Simple', 'Average', 'Clever', 'Brilliant'):
    assert text in medit[medit.index('static void medit_disp_ai_intelligence'):medit.index('static void medit_disp_ai_advanced')]
advanced = medit[medit.index('static void medit_disp_ai_advanced'):medit.index('static void medit_disp_ai_diagnostics')]
for text in ('Perception and Awareness', 'Memory Details', 'Threat Response', 'Combat Reactions', 'Capability Overrides', 'Movement Internals', 'Profile and Inference'):
    assert text in advanced
for text in ('Edit Daily Schedule', 'Edit Patrol Routes', "Preview Today's Routine", 'Validate Destinations'):
    assert text in medit
assert 'Random Move Delay' not in root
assert 'Effective NPC Behavior' in medit
assert 'Diagnostics lists only issues a builder can act on' in medit
print('AI Actor builder-first menu regression checks passed')
