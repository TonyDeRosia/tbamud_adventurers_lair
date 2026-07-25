"""Executable audit contract: every named event bit crosses builder and runtime layers."""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
hdr = (ROOT/'src/dg_scripts.h').read_text()
const = (ROOT/'src/constants.c').read_text()
olc = (ROOT/'src/dg_olc.c').read_text()
trig = (ROOT/'src/dg_triggers.c').read_text()
all_src = '\n'.join(p.read_text(errors='ignore') for p in (ROOT/'src').glob('*.c'))
matrix = (ROOT/'docs/DG_TRIGGER_CAPABILITY_MATRIX.md').read_text()

families = {
 'MTRIG': {'GLOBAL':None,'RANDOM':'random_mtrigger','COMMAND':'command_mtrigger','SPEECH':'speech_mtrigger','ACT':'act_mtrigger','DEATH':'death_mtrigger','GREET':'greet_mtrigger','GREET_ALL':'greet_mtrigger','ENTRY':'entry_mtrigger','RECEIVE':'receive_mtrigger','FIGHT':'fight_mtrigger','HITPRCNT':'hitprcnt_mtrigger','BRIBE':'bribe_mtrigger','LOAD':'load_mtrigger','MEMORY':'entry_memory_mtrigger','CAST':'cast_mtrigger','LEAVE':'leave_mtrigger','DOOR':'door_mtrigger','TIME':'time_mtrigger','DAMAGE':'damage_mtrigger'},
 'OTRIG': {'GLOBAL':None,'RANDOM':'random_otrigger','COMMAND':'command_otrigger','TIMER':'timer_otrigger','GET':'get_otrigger','DROP':'drop_otrigger','GIVE':'give_otrigger','WEAR':'wear_otrigger','REMOVE':'remove_otrigger','LOAD':'load_otrigger','CAST':'cast_otrigger','LEAVE':'leave_otrigger','CONSUME':'consume_otrigger','TIME':'time_otrigger'},
 'WTRIG': {'GLOBAL':None,'RANDOM':'random_wtrigger','COMMAND':'command_wtrigger','SPEECH':'speech_wtrigger','RESET':'reset_wtrigger','ENTER':'enter_wtrigger','DROP':'drop_wtrigger','CAST':'cast_wtrigger','LEAVE':'leave_wtrigger','DOOR':'door_wtrigger','LOGIN':'login_wtrigger','TIME':'time_wtrigger'},
}
for prefix, expected in families.items():
    defined = set(re.findall(rf'^#define {prefix}_([A-Z_]+)\s+\(1 << \d+\)', hdr, re.M))
    assert defined == set(expected), (prefix, defined ^ set(expected))
    for event, dispatch in expected.items():
        constant = f'{prefix}_{event}'
        assert f'`{constant}`' in matrix
        if dispatch:
            assert re.search(rf'\b{dispatch}\s*\([^;]*\)\s*\{{', trig, re.S)
            body = trig[trig.index(dispatch):trig.index(dispatch)+5000]
            assert 'script_driver' in body
            # A definition alone is insufficient: require another translation unit call.
            assert all_src.count(dispatch + '(') >= 2, dispatch

# Generic prototype persistence includes every field and parent persistence stores VNUM refs.
for token in ('attach_type', 'GET_TRIG_TYPE(trig)', 'GET_TRIG_NARG(trig)', 'GET_TRIG_ARG(trig)', 'cmdlist'):
    assert token in olc
assert 'fprintf(fp,"T %d\\n", t->vnum)' in olc
assert 'proto->attach_type != OLC_ITEM_TYPE(d)' in olc
print('DG trigger capability matrix checks passed')
