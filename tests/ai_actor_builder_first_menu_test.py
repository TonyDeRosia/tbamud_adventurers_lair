"""Builder-facing AI Actor workflow regression coverage."""
from pathlib import Path
m = Path('src/medit.c').read_text()
root = m[m.index('static void medit_disp_ai_menu(struct descriptor_data *d)\n{'):m.index('/* AI modes accept', m.index('static void medit_disp_ai_menu'))]
for text in ('1) Personality', '2) Communication', '3) Daily Routine', '4) Combat Behavior',
             '5) Preview NPC', '6) Diagnostics', 'A) Advanced AI Brain'):
    assert text in root, text
communication = m[m.index('static void medit_disp_ai_communication'):m.index('static void medit_disp_ai_intelligence')]
for text in ('Current Style:', 'Creature Sounds Authored:', 'Edit Creature Sounds',
             'Edit Greetings', 'Edit Ambient Speech', 'Edit Replies'):
    assert text in communication, text
assert 'This NPC...' in m
assert "isdigit((unsigned char)*arg) && !arg[1]" in m
print('AI Actor builder workflow regression checks passed')
