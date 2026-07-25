"""Static regression contract for the shared DG attachment workflow."""
from pathlib import Path

root = Path(__file__).resolve().parents[1]
dg = (root / "src/dg_olc.c").read_text()
hdr = (root / "src/dg_olc.h").read_text()

for mode in ("SCRIPT_TRIGGER_ACTION", "SCRIPT_INSPECT_TRIGGER", "SCRIPT_DETACH_CONFIRM"):
    assert mode in hdr and f"case {mode}:" in dg
assert "Edit Trigger Prototype" in dg and "trigedit %d" in dg
assert "Nested Oasis OLC is not safe" in dg
assert "dg_parent_noun" in dg and '"Mob"' in dg and '"Object"' in dg and '"Room"' in dg
assert "Enter insertion slot and trigger VNUM" in dg
assert "Enter both an insertion slot and trigger VNUM" in dg
assert 'strtol(cursor, &end, 10)' in dg and "if (*end" in dg
assert "Valid insertion slots: 1 through %d" in dg
assert "does not exist" in dg and "cannot be attached" in dg
assert "can_edit_zone(d->character, real_zone_by_thing(vnum))" in dg
assert "Duplicate attachments are allowed by the legacy DG policy" in dg
assert "This removes only the attachment; it does not delete the trigger prototype" in dg
confirm = dg[dg.index("case SCRIPT_DETACH_CONFIRM:"):dg.index("void trigedit_string_cleanup")]
assert "OLC_VAL(d)++" in confirm
assert confirm.index("tolower((unsigned char)*arg) == 'y'") < confirm.index("OLC_VAL(d)++")
start = dg.index("static void dg_trigger_inspect(struct descriptor_data *d)\n{")
inspect = dg[start:dg.index("void dg_script_menu", start)]
assert "cmdlist" in inspect and "OLC_VAL" not in inspect
print("DG attachment workflow contract checks passed")
