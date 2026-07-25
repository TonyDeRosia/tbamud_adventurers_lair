"""TRIGEDIT parsing/filtering/help regression contract."""
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
src=(ROOT/'src/dg_olc.c').read_text()
hdr=(ROOT/'src/dg_olc.h').read_text()
menu=src[src.index('static void trigedit_disp_menu'):src.index('static void trigedit_disp_types')]
parser=src[src.index('void trigedit_parse'):src.index('void trigedit_save')]
assert '%sH%s) Trigger field help' in src and "case 'h':" in parser
assert 'TRIGEDIT_HELP' in hdr and 'case TRIGEDIT_HELP:' in parser
assert 'switch (tolower(*arg))' in parser
assert 'strtol(arg, &end, 10)' in parser
assert 'intended < MOB_TRIGGER || intended > WLD_TRIGGER' in parser
assert 'Trigger flags cleared' in parser and 'GET_TRIG_TYPE(OLC_TRIG(d)) = 0' in parser
assert 'Numeric argument must be a whole number' in parser and 'value > INT_MAX' in parser
assert 'LIMIT(atoi(arg), 0, 100)' not in parser
assert 'That bit is reserved and cannot be selected' in parser
assert 'if (!strncmp(types[i], "UNUSED", 6))' in src
for choice in "123456qhyw":
    assert f"case '{choice}':" in parser
print('DG TRIGEDIT workflow checks passed')
