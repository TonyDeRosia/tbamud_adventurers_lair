"""End-to-end static contract over editor, persistence, reload, and runtime paths.

The repository has no linkable Oasis unit-test fixture; this guard follows the
actual production functions rather than maintaining a second parser model.
"""
from pathlib import Path

root = Path(__file__).resolve().parents[1]
dg_olc = (root / "src/dg_olc.c").read_text()
medit = (root / "src/medit.c").read_text()
db_scripts = (root / "src/dg_db_scripts.c").read_text()
handler = (root / "src/dg_handler.c").read_text()
runtime = (root / "src/dg_scripts.c").read_text()
mob = (root / "lib/world/mob/0.mob").read_text()
trg = (root / "lib/world/trg/0.trg").read_text()

# MEDIT owns a copied working list and delegates S to the shared parser.
assert "dg_olc_script_copy(d);" in medit
assert "case 's':" in medit and "dg_script_menu(d);" in medit
assert "OLC_MOB(d)->proto_script = OLC_SCRIPT(d);" in medit
assert "mob_proto[new_rnum].proto_script = OLC_SCRIPT(d);" in medit
assert "copy_proto_script(&mob_proto[new_rnum], mob, MOB_TRIGGER);" in medit
assert "assign_triggers(mob, MOB_TRIGGER);" in medit

# Grammar is strict, supports comma or whitespace, and mutates only after all checks.
new = dg_olc[dg_olc.index("case SCRIPT_NEW_TRIGGER:"):dg_olc.index("case SCRIPT_DEL_TRIGGER:")]
for validation in ("real_trigger(vnum) == NOTHING", "attach_type != OLC_ITEM_TYPE(d)",
                   "pos < 1 || pos > count + 1", "can_edit_zone"):
    assert validation in new
assert new.index("CREATE(trig") > new.index("can_edit_zone")
assert "if (*cursor == ',')" in new and "isspace" in new and "if (*end" in new
assert "while (currtrig->next && --pos > 1)" in new

# Disk writer, boot reader, copy, and runtime instantiation preserve list traversal order.
assert 'fprintf(fp,"T %d\\n", t->vnum);' in dg_olc
assert "while (trg_proto->next)" in db_scripts and "trg_proto->next = new_trg" in db_scripts
assert "while (tp_src)" in handler and "tp_dst->vnum = tp_src->vnum" in handler
assert "while (trg_proto)" in db_scripts and "add_trigger(SCRIPT(mob), read_trigger(rnum), -1)" in db_scripts

# Existing Academy prototypes and authored runtime branches remain untouched and runnable.
questmaster = mob[mob.index("#14\n"):mob.index("#15\n")]
ogre = mob[mob.index("#16\n"):mob.index("#17\n")]
guard = mob[mob.index("#24\n"):mob.index("#25\n")]
assert {"T 1", "T 3"}.issubset(set(questmaster.splitlines()))
assert "T 2" in ogre
for vnum in (4, 5, 7, 8):
    assert f"T {vnum}" in guard
assert "return 0" in trg and "say Thank you." in trg
assert "unlock gateway" in trg and "open gateway" in trg
assert "Attachment %d:" in runtime

print("MEDIT DG legacy lifecycle contract checks passed")
