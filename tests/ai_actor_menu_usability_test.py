"""Source-level coverage for the builder-first AI Actor OLC screens."""
from pathlib import Path
medit = Path('src/medit.c').read_text()
main = medit.rsplit('static void medit_disp_ai_menu', 1)[1].split('static int medit_is_ai_mode', 1)[0]
for text in ('Communication', 'Intelligence', 'Schedule', 'Dialogue', 'Preview', 'Diagnostics', 'Advanced AI Brain', 'H) Help', 'Q) Return'):
    assert text in main, text
for text in ('Profile Mode', 'Role:', 'Movement:', 'Capabilities', 'Perception', 'Threat Response'):
    assert text not in main, text
for text in ('medit_disp_ai_role', 'medit_disp_ai_movement', 'medit_disp_ai_mode', 'AI Actor Personality (0-100)', 'Random Move Delay'):
    assert text in medit, text
print('AI Actor menu usability regression checks passed')
