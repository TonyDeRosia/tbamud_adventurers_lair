"""Focused displayed-command contract for the one shared DG OLC parser."""
from pathlib import Path
import re

root = Path(__file__).resolve().parents[1]
dg = (root / 'src/dg_olc.c').read_text()
menu = dg[dg.index('void dg_script_menu'):dg.index('int dg_script_edit_parse')]
parser = dg[dg.index('int dg_script_edit_parse'):dg.index('void trigedit_string_cleanup')]

for shown, accepted in (('%sN%s)', "case 'n':"), ('%sX%s)', "case 'x':"),
                        ('%sQ%s)', "case 'q':")):
    assert shown in menu and accepted in parser
assert 'switch(tolower(*arg))' in parser  # lower and upper case share one route
assert 'strtol(arg, &end, 10)' in parser and "*end == '\\0'" in parser
assert 'SCRIPT_INSPECT_TRIGGER' in parser
inspect = parser[parser.index('case SCRIPT_INSPECT_TRIGGER:'):
                 parser.index('case SCRIPT_NEW_TRIGGER:')]
assert "tolower((unsigned char)*arg) == 'q'" in inspect
assert 'OLC_VAL(d)++' not in inspect and 'OLC_VAL(d) =' not in inspect

for editor, parent in (('medit.c', 'medit_disp_menu(d);'),
                       ('oedit.c', 'oedit_disp_menu(d);'),
                       ('redit.c', 'redit_disp_menu(d);')):
    source = (root / 'src' / editor).read_text()
    route = source[source.index('case OLC_SCRIPT_EDIT:'):][:300]
    assert 'if (dg_script_edit_parse(d, arg)) return;' in route
    assert parent in route

# The main parser documents exactly these letters; selection numbers are the
# only other main-menu grammar and are range-checked against the attached list.
main = parser[parser.index('case SCRIPT_MAIN_MENU:'):parser.index('case SCRIPT_INSPECT_TRIGGER:')]
assert set(re.findall(r"case '([a-z])':", main)) == {'n', 'x', 'q'}
assert 'while (currtrig && --pos > 0)' in main and 'currtrig && pos == 0' in main
assert parser.count('OLC_VAL(d)++;') >= 2
attach = parser[parser.index('case SCRIPT_NEW_TRIGGER:'):parser.index('case SCRIPT_DEL_TRIGGER:')]
detach = parser[parser.index('case SCRIPT_DEL_TRIGGER:'):parser.index('\n  dg_script_menu(d);', parser.index('case SCRIPT_DEL_TRIGGER:'))]
assert 'OLC_VAL(d)++;' in attach and 'OLC_VAL(d)++;' in detach
print('shared DG menu contract checks passed')
