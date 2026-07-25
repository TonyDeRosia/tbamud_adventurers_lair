"""Source-level regression guards for the legacy-first MEDIT correction."""
from pathlib import Path
root = Path(__file__).resolve().parents[1]
medit = (root / "src/medit.c").read_text()
legacy = (root / "src/legacy_behavior.c").read_text()
oasis = (root / "src/oasis.h").read_text()
dg = (root / "src/dg_olc.c").read_text()

assert 'L%s) Legacy Behavior' not in medit
assert 'I%s) AI Actor Extensions (optional)' in medit
assert 'NPC Flags' in medit and 'AFF Flags' in medit and 'DG Scripts' in medit
assert 'C%s) Special Procedure: %s' in medit
assert 'V%s) Effective Behavior Preview' in medit
for flag in ('MOB_SENTINEL', 'MOB_STAY_ZONE', 'MOB_MEMORY', 'MOB_HELPER', 'MOB_WIMPY', 'MOB_SCAVENGER'):
    assert flag in medit
assert 'TOGGLE_BIT_AR(MOB_FLAGS(OLC_MOB(d)),MOB_MEMORY)' in medit
assert 'Assigned Special: %s' in medit
assert 'legacy_special_metadata(GET_MOB_SPEC' in medit
assert 'str_str(GET_SDESC(m),"mayor")' in medit  # notice only, never assignment
assert 'This mob is named Mayor, but no Mayor special procedure is assigned' in medit
for capability in ('Timed wake and sleep', 'Fixed daily route', 'Opens and closes city gates',
                   'Timed public speeches', 'Changes posture', 'Waits between route steps'):
    assert capability in legacy
assert 'Dynamic Dialogue' in medit and 'Unavailable - owned by legacy source' in medit
assert 'SCAVENGER flag authoritative' in medit
assert '#define MEDIT_LEGACY_MENU' in oasis  # retained only for saved/source compatibility
assert 'switch(tolower(*arg))' in dg
for command in ("case 'q':", "case 'n':", "case 'd':", "case 'x':"):
    assert command in dg
assert 'SCRIPT_INSPECT_TRIGGER' in dg
assert 'Unknown / arbitrary DG Script' in dg
assert 'editscript->vnum == 3011' in dg
for editor, display in ((medit, 'medit_disp_menu(d);'),
                        ((root/'src/oedit.c').read_text(), 'oedit_disp_menu(d);'),
                        ((root/'src/redit.c').read_text(), 'redit_disp_menu(d);')):
    nested = editor[editor.index('case OLC_SCRIPT_EDIT:'):]
    assert 'if (dg_script_edit_parse(d, arg)) return;' in nested
    assert display in nested[:300]
print('MEDIT legacy-first regression checks passed')
# Shared DG routing reaches the parser before MEDIT's numeric-only validation.
assert 'OLC_MODE(d) != OLC_SCRIPT_EDIT' in medit
assert "case 'n':" in dg and 'SCRIPT_NEW_TRIGGER' in dg
assert "case 'd':" in dg and 'SCRIPT_DEL_TRIGGER' in dg
assert "case 'x':" in dg and "return 0;" in dg
assert 'if (isdigit((unsigned char)*arg))' in dg
assert '/* Inspection is read-only: never touch OLC_VAL here. */' in dg
assert dg.count('OLC_VAL(d)++;') >= 2  # successful attach/detach dirty the parent
for editor_name in ('src/medit.c', 'src/oedit.c', 'src/redit.c'):
    editor = (root / editor_name).read_text()
    nested = editor[editor.index('case OLC_SCRIPT_EDIT:'):]
    assert 'if (dg_script_edit_parse(d, arg)) return;' in nested[:250]
