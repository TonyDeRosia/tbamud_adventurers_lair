"""Source-level regression guards for the legacy-first MEDIT correction."""
from pathlib import Path
root = Path(__file__).resolve().parents[1]
medit = (root / "src/medit.c").read_text()
legacy = (root / "src/legacy_behavior.c").read_text()
oasis = (root / "src/oasis.h").read_text()
dg = (root / "src/dg_olc.c").read_text()

assert 'L%s) Legacy Behavior' in medit
assert 'I%s) AI Actor Extensions (optional)' in medit
assert 'NPC Flags' in medit and 'AFF Flags' in medit and 'DG Scripts' in medit
assert 'Legacy Behavior\\r\\n---------------' in medit
for flag in ('MOB_SENTINEL', 'MOB_STAY_ZONE', 'MOB_MEMORY', 'MOB_HELPER', 'MOB_WIMPY', 'MOB_SCAVENGER'):
    assert flag in medit
assert 'TOGGLE_BIT_AR(MOB_FLAGS(OLC_MOB(d)),MOB_MEMORY)' in medit
assert 'Special Procedure : %s' in medit
assert 'legacy_special_metadata(GET_MOB_SPEC' in medit
assert 'str_str(GET_SDESC(m),"mayor")' in medit  # notice only, never assignment
assert 'This mob is named Mayor, but no Mayor special procedure is assigned' in medit
for capability in ('Timed wake and sleep', 'Fixed daily route', 'Opens and closes city gates',
                   'Timed public speeches', 'Changes posture', 'Waits between route steps'):
    assert capability in legacy
assert 'Dynamic Dialogue' in medit and 'Unavailable - owned by legacy source' in medit
assert 'SCAVENGER flag authoritative' in medit
assert '#define MEDIT_LEGACY_MENU' in oasis
assert "case 'q':" in dg and 'return 0;' in dg
for editor, display in ((medit, 'medit_disp_menu(d);'),
                        ((root/'src/oedit.c').read_text(), 'oedit_disp_menu(d);'),
                        ((root/'src/redit.c').read_text(), 'redit_disp_menu(d);')):
    nested = editor[editor.index('case OLC_SCRIPT_EDIT:'):]
    assert 'if (dg_script_edit_parse(d, arg)) return;' in nested
    assert display in nested[:300]
print('MEDIT legacy-first regression checks passed')
