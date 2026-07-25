"""Contract checks for current MEDIT additions and the small AI extension menu."""
from pathlib import Path

medit = Path('src/medit.c').read_text()
menu_start = medit.rindex('static void medit_disp_menu(struct descriptor_data *d)')
menu = medit[menu_start:medit.index('static const char *medit_ai_state', menu_start)]
parse_start = medit.index('case MEDIT_MAIN_MENU:', medit.index('void medit_parse('))
main_parser = medit[parse_start:medit.index('case MEDIT_STATS_MENU:', parse_start)]
for shown, command in (('%sC%s) Special Procedure', 'c'),
                       ('%sI%s) AI Actor Extensions', 'i'),
                       ('%sV%s) Effective Behavior Preview', 'v'),
                       ('%sS%s) DG Scripts', 's'), ('%sQ%s) Quit', 'q')):
    assert shown in menu, shown
    assert f"case '{command}':" in main_parser and f"case '{command.upper()}':" in main_parser

ai_start = medit.rindex('static void medit_disp_ai_menu(struct descriptor_data *d)')
ai_menu = medit[ai_start:medit.index('static void medit_disp_legacy_menu', ai_start)]
for text in ('Personality', 'Identity / Role', 'Advanced Perception',
             'Optional additions only', 'Q) Return'):
    assert text in ai_menu, text
for obsolete_control in ('2) Communication', '3) Daily Routine', '4) Combat Behavior',
                         'A) Advanced AI Brain'):
    assert obsolete_control not in ai_menu, obsolete_control
print('MEDIT and AI extension menu contract checks passed')
