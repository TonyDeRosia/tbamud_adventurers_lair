"""Executable Builder Academy DG Scripts compatibility contract.

The Academy data is intentionally the oracle here.  These checks cross the
authored lessons through prototypes, attachments, resets, command routing,
persistence, and runtime dispatch instead of duplicating tutorial prose.
"""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]


def records(path: str) -> dict[int, str]:
    text = (ROOT / path).read_text()
    return {
        int(match.group(1)): match.group(2)
        for match in re.finditer(
            r"^#(\d+)\n(.*?)(?=^#\d+\n|^\$~?$)", text, re.MULTILINE | re.DOTALL
        )
    }


rooms = records("lib/world/wld/0.wld")
mobs = records("lib/world/mob/0.mob")
objects = records("lib/world/obj/0.obj")
triggers = records("lib/world/trg/0.trg") | records("lib/world/trg/1.trg")
zone = (ROOT / "lib/world/zon/0.zon").read_text()
help_text = (ROOT / "lib/text/help/help.hlp").read_text()

# The complete lesson route and its literal command references remain present.
for vnum in (13, 18, 20, 21, 22, 23, 98, 99):
    assert vnum in rooms, f"missing Academy room {vnum}"
for promise in (
    "TRIGEDIT-MOB-TUTORIAL", "STAT QUESTMASTER", "TSTAT 1",
    "STAT GATEGUARD", "GIVE 10 COINS GUARD", "TSTAT each",
    "STAT SELF", "triggers 190 thru 192", "TRIGEDIT-MENU",
    "TRIGEDIT-TYPE", "HELP EXAMPLES", "HELP TRIG-EXAMPLES",
):
    assert promise.lower() in "\n".join(rooms[v] for v in (13, 18, 20, 21, 22, 23)).lower()

# Every help topic explicitly required by the Academy/user remains addressable.
for topic in (
    "TRIGEDIT", "TRIGEDIT-MENU", "TRIGEDIT-TYPE",
    "TRIGEDIT-MOB-TUTORIAL", "TRIGEDIT-ADVANCED-TUTORIAL", "EXAMPLES",
    "TRIG-EXAMPLES", "SCRIPT-MENU", "MEDIT", "TSTAT", "TLIST",
):
    assert re.search(rf"(?m)^[^#\n]*\b{re.escape(topic)}\b[^\n]*\n", help_text), topic

# Canonical tutorial trigger metadata, bodies, and ordered prototype ownership.
expected = {
    1: ("0 g 100", "if %actor.is_pc%"),
    2: ("0 f 100", "%load% obj 1"),
    3: ("0 j 100", "return 0"),
    4: ("0 g 100", "%direction% == south"),
    5: ("0 m 1", "unlock gateway"),
    6: ("1 c 2", "switch %random.20%"),
    7: ("0 e 0", "close gate"),
    8: ("0 e 0", "lock gate"),
    20: ("0 d 100", "%at% rumble say"),
    188: ("0 q 100", "!%actor.has_item(47)%"),
    189: ("2 g 100", "%load% obj 47"),
    190: ("0 j 100", "remote solved_example_quest_zone_0"),
    191: ("0 d 1", "%actor.varexists(solved_example_quest_zone_0)%"),
    192: ("0 g 100", "say Please say yes"),
}
for vnum, (metadata, body) in expected.items():
    assert vnum in triggers, f"missing tutorial trigger {vnum}"
    assert metadata in triggers[vnum] and body in triggers[vnum], vnum


def attachments(record: str) -> list[int]:
    return [int(v) for v in re.findall(r"(?m)^T (\d+)$", record)]


assert attachments(mobs[14]) == [3, 1]       # receive then greet, authored order
assert attachments(mobs[16]) == [2]          # quest-ogre death
assert attachments(mobs[24]) == [4, 5, 7, 8] # guard greet/bribe/two act handlers
assert attachments(mobs[25]) == [192, 191, 190]
assert attachments(mobs[26]) == [188]
assert attachments(objects[47]) == [6]
assert attachments(rooms[22]) == [189]

# Academy actors and props really load where the walkthrough says they do.
for reset in (
    "M 0 24 1 20", "M 0 25 1 21", "M 0 26 1 22",
    "O 0 47 99 22", "M 0 16 1 19", "M 0 14 1 18",
    "M 0 22 2 98", "O 0 98 1 99", "M 0 99 1 99",
):
    assert reset in zone, reset

# The language constructs exercised by the lessons still have driver branches.
driver = (ROOT / "src/dg_scripts.c").read_text()
for construct in (
    '"if "', '"elseif "', '"else"', '"while "', '"switch "', '"set "',
    '"eval "', '"unset "', '"global "', '"remote "', '"wait "', '"return "',
):
    assert construct in driver, construct
assert "eval_expr(" in driver and "script_driver(" in driver

# Builder commands route to production implementations, and parent saves emit
# T records in traversal order. Runtime assignment appends in that same order.
interpreter = (ROOT / "src/interpreter.c").read_text()
for command, handler in (
    ("trigedit", "do_oasis_trigedit"), ("tlist", "do_oasis_list"),
    ("tstat", "do_tstat"), ("stat", "do_stat"), ("medit", "do_oasis_medit"),
    ("oedit", "do_oasis_oedit"), ("redit", "do_oasis_redit"),
):
    assert re.search(rf'\{{ "{command}"[^\n]+{handler}', interpreter), command
olc = (ROOT / "src/dg_olc.c").read_text()
loader = (ROOT / "src/dg_db_scripts.c").read_text()
runtime = (ROOT / "src/dg_scripts.c").read_text()
assert 'fprintf(fp,"T %d\\n", t->vnum)' in olc and "t = t->next" in olc
assert "trg_proto->next = new_trg" in loader
assert "add_trigger(SCRIPT(mob), read_trigger(rnum), -1)" in loader
assert "Attachment %d:" in runtime

# CHECKLOAD is also Builder-facing inspection: the highest valid prototype
# must not disappear from attachment results.
wizard = (ROOT / "src/act.wizard.c").read_text()
for table in ("mobt", "objt", "world"):
    assert re.search(rf"for \([^\n]+<= top_of_{table};", wizard), table

print("Builder Academy DG compatibility checks passed")
