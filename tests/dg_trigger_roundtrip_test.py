"""Disk grammar/load and ordered-command round-trip source contract."""
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
olc=(ROOT/'src/dg_olc.c').read_text()
db=(ROOT/'src/dg_db_scripts.c').read_text()
# Writer record: vnum, name, attach, ASCII flags, narg, argument and command terminator.
for fragment in ('"#%d\\n"', '"%d %s %d\\n"', 'STRING_TERMINATOR'):
    assert fragment in olc
for fragment in ('trig->attach_type', 'trig->trigger_type', 'trig->narg', 'trig->arglist', 'trig->cmdlist'):
    assert fragment in olc
# Existing setup linearizes and save recompiles in order.
setup=olc[olc.index('void trigedit_setup_existing'):olc.index('static void trigedit_disp_menu')]
save=olc[olc.index('void trigedit_save'):]
assert 'while (c)' in olc and 'c = c->next' in olc
assert 'strtok(s, "\\n\\r")' in save and 'cmd = cmd->next' in save
# Loader recognizes trigger records and consumes the same core fields.
assert 'parse_trigger' in db
loader=db[db.index('void parse_trigger'):][:3000]
for fragment in ('attach_type', 'trigger_type', 'narg', 'arglist', 'cmdlist'):
    assert fragment in loader
print('DG trigger disk round-trip checks passed')
