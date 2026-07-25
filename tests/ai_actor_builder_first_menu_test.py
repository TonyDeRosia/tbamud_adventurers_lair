"""The primary AI menu is additive; historical editors remain compatibility code."""
from pathlib import Path

m = Path('src/medit.c').read_text()
menu_start = m.rindex('static void medit_disp_ai_menu(struct descriptor_data *d)\n{')
root = m[menu_start:m.index('static void medit_disp_legacy_menu', menu_start)]
parse_start = m.index('case MEDIT_AI_MENU:', m.index('void medit_parse('))
parser = m[parse_start:m.index('case MEDIT_LEGACY_MENU:', parse_start)]

for label in ('1) Personality', '2) Identity / Role', '3) Advanced Perception'):
    assert label in root, label
for command, target in (("case '1':", 'medit_disp_ai_personality'),
                        ("case '2':", 'medit_disp_ai_role'),
                        ("case '3':", 'medit_disp_ai_perception')):
    assert command in parser and target in parser

# Status notices are allowed, but no primary command may enter a duplicate editor.
for forbidden_target in ('medit_disp_ai_communication', 'medit_disp_ai_schedule',
                         'medit_disp_ai_patrol', 'medit_disp_ai_movement',
                         'medit_disp_ai_combat', 'medit_disp_ai_social'):
    assert forbidden_target not in parser, forbidden_target
for notice in ('Unavailable - owned by special', 'Unavailable - owned by DG Scripts',
               'SCAVENGER flag authoritative', 'HELPER flag authoritative',
               'WIMPY flag authoritative'):
    assert notice in root, notice

# Undocumented input only redraws the same menu; it cannot fall through to a
# historical mode.  Those modes remain explicit parser cases for old data/state.
assert 'default: medit_disp_ai_menu(d); return;' in parser
print('AI Actor additive builder reachability checks passed')
