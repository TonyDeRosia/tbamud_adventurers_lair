from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SRC = (ROOT / "src" / "act.movement.c").read_text(encoding="utf-8")

def block_after(signature: str) -> str:
    start = SRC.find(signature)
    assert start >= 0, f"missing signature: {signature}"
    open_brace = SRC.find("{", start)
    assert open_brace >= 0
    depth = 0
    for i in range(open_brace, len(SRC)):
        c = SRC[i]
        if c == "{":
            depth += 1
        elif c == "}":
            depth -= 1
            if depth == 0:
                return SRC[start:i+1]
    raise AssertionError(f"unterminated function: {signature}")

tests = []

def check(name, fn):
    fn()
    tests.append(name)

check(
    "old mob target lookup removed",
    lambda: (
        (_ for _ in ()).throw(AssertionError("old mob lookup remains"))
        if "find_closest_mob_in_area_by_name" in SRC else None
    ),
)

check(
    "old quest-special RUNTO behavior removed",
    lambda: (
        (_ for _ in ()).throw(AssertionError("quest-target behavior remains in do_runto"))
        if "GET_KQUEST_ACTIVE(ch)" in block_after("ACMD(do_runto)") else None
    ),
)

check(
    "zone starting-room helper exists",
    lambda: (
        (_ for _ in ()).throw(AssertionError("missing zone start helper"))
        if "static room_rnum runto_zone_start_room(zone_rnum zone)" not in SRC else None
    ),
)

def test_zone_start_semantics():
    b = block_after("static room_rnum runto_zone_start_room")
    assert "zone_table[zone].bot" in b
    assert "zone_table[zone].top" in b
    assert "real_room(vnum)" in b
    assert "room_is_runto_safe(room)" in b
    assert "ZONE_FLAGGED(zone, ZONE_CLOSED)" in b
check("zone start uses bottom/first safe real room", test_zone_start_semantics)

def test_zone_lookup():
    b = block_after("static zone_rnum find_runto_zone")
    assert "is_number(query)" in b
    assert "zone_table[zone].number" in b
    assert "runto_zone_name_matches(query, zone_table[zone].name)" in b
    assert "count == 1" in b
check("zone lookup supports number and name", test_zone_lookup)

def test_runto_usage():
    b = block_after("ACMD(do_runto)")
    assert "Usage: runto <area name | zone number>" in b
    assert "Areas:" in b
    assert "find_runto_zone(query, &matches)" in b
    assert "runto_zone_start_room(zone)" in b
check("runto selects areas rather than mobiles", test_runto_usage)

def test_runto_targets_start():
    b = block_after("ACMD(do_runto)")
    assert "target_room = runto_zone_start_room(zone)" in b
    assert "runto_path_distance(IN_ROOM(ch), target_room)" in b
    assert "execute_runto_path_to_room(ch, target_room)" in b
check("runto routes to area starting room", test_runto_targets_start)

def test_real_movement_executor():
    b = block_after("static int execute_runto_path_to_room")
    assert "graph_find_first_step(IN_ROOM(ch), target_room)" in b
    assert "perform_move(ch, dir, 0)" in b
    assert "FIGHTING(ch)" in b
    assert "char_to_room" not in b
    assert "char_from_room" not in b
    assert "do_goto" not in b
check("executor performs real room-by-room movement", test_real_movement_executor)

def test_no_teleport_in_runto():
    b = block_after("ACMD(do_runto)")
    banned = ("char_to_room", "char_from_room", "do_goto", "perform_goto", "teleport")
    for token in banned:
        assert token not in b, f"teleport-style token found in do_runto: {token}"
check("do_runto contains no teleport path", test_no_teleport_in_runto)

def test_normal_movement_contract():
    b = block_after("int perform_move(struct char_data *ch, int dir, int need_specials_check)")
    assert "do_simple_move(ch, dir, need_specials_check)" in b
    assert "FIGHTING(ch)" in b
    assert "EX_CLOSED" in b
check("perform_move remains normal movement contract", test_normal_movement_contract)

def test_normal_move_cost_contract():
    b = block_after("int do_simple_move(struct char_data *ch, int dir, int need_specials_check)")
    assert "need_movement" in b
    assert "GET_MOVE(ch)" in b
check("normal movement cost remains active", test_normal_move_cost_contract)

print(f"{len(tests)} tests passed: RUNTO area navigation V1")