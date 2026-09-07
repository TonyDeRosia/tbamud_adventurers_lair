from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
MOVE = (ROOT / "src" / "act.movement.c").read_text(encoding="utf-8")
EVENT_C = (ROOT / "src" / "mud_event.c").read_text(encoding="utf-8")
EVENT_H = (ROOT / "src" / "mud_event.h").read_text(encoding="utf-8")

def block_after(text: str, signature: str) -> str:
    start = text.find(signature)
    assert start >= 0, f"missing signature: {signature}"

    open_brace = text.find("{", start)
    assert open_brace >= 0, f"missing opening brace: {signature}"

    semi = text.find(";", start, open_brace)
    assert semi < 0, f"prototype found instead of definition: {signature}"

    depth = 0
    for i in range(open_brace, len(text)):
        if text[i] == "{":
            depth += 1
        elif text[i] == "}":
            depth -= 1
            if depth == 0:
                return text[start:i + 1]

    raise AssertionError(f"unterminated function: {signature}")

def test_area_destination_contract():
    b = block_after(MOVE, "static room_rnum runto_zone_start_room")
    assert "zone_table[zone].bot" in b
    assert "zone_table[zone].top" in b
    assert "real_room(vnum)" in b
    assert "room_is_runto_safe(room)" in b

def test_area_lookup_contract():
    b = block_after(MOVE, "static zone_rnum find_runto_zone")
    assert "is_number(query)" in b
    assert "zone_table[zone].number" in b
    assert "runto_zone_name_matches(query, zone_table[zone].name)" in b

def test_runto_is_event_driven():
    b = block_after(MOVE, "ACMD(do_runto)")
    assert "NEW_EVENT(eRUNTO, ch, event_vars, runto_step_delay())" in b
    assert "perform_move(" not in b
    assert "execute_runto_path_to_room" not in b
    assert "while (" not in b

def test_runto_stop_command():
    b = block_after(MOVE, "ACMD(do_runto)")
    assert 'str_cmp(argument, "stop")' in b
    assert "char_has_mud_event(ch, eRUNTO)" in b
    assert "event_cancel(existing->pEvent)" in b
    assert "runto stop" in b

def test_event_moves_exactly_one_step_per_fire():
    b = block_after(MOVE, "EVENTFUNC(event_runto)")
    assert b.count("perform_move(ch, dir, 0)") == 1
    assert "while (" not in b
    assert "graph_find_first_step(IN_ROOM(ch), target_room)" in b
    assert "return runto_step_delay();" in b

def test_event_uses_real_normal_movement():
    b = block_after(MOVE, "EVENTFUNC(event_runto)")

    # Safety comments intentionally mention forbidden APIs by name. Strip C
    # comments before checking executable code so documentation cannot create
    # a false positive.
    code = re.sub(r"/\*.*?\*/", "", b, flags=re.S)
    code = re.sub(r"//[^\n]*", "", code)

    banned_calls = ("char_to_room", "char_from_room", "do_goto", "perform_goto")
    for token in banned_calls:
        assert re.search(rf"\b{token}\s*\(", code) is None, (
            f"teleport-style function call remains in event: {token}"
        )

    assert code.count("perform_move(ch, dir, 0)") == 1

def test_event_interrupts_for_combat_and_position():
    b = block_after(MOVE, "EVENTFUNC(event_runto)")
    assert "FIGHTING(ch)" in b
    assert "GET_POS(ch) < POS_STANDING" in b

def test_event_detects_trigger_relocation():
    b = block_after(MOVE, "EVENTFUNC(event_runto)")
    assert "IN_ROOM(ch) != next_room" in b
    assert "Your route is disrupted" in b

def test_event_is_paced_across_future_pulses():
    b = block_after(MOVE, "static long runto_step_delay")
    assert "PASSES_PER_SEC / 2" in b
    runto = block_after(MOVE, "ACMD(do_runto)")
    assert "The first room transition happens on a later event" in runto

def test_event_registered_as_character_event():
    assert "eRUNTO" in EVENT_H
    assert "EVENTFUNC(event_runto);" in EVENT_H
    assert '{ "RunTo"        , event_runto    , EVENT_CHAR  },  /* eRUNTO */' in EVENT_C

def test_old_synchronous_executor_removed():
    assert "execute_runto_path_to_room" not in MOVE
    assert "while (IN_ROOM(ch) != target_room)" not in MOVE

def test_no_old_mobile_target_behavior():
    assert "find_closest_mob_in_area_by_name" not in MOVE
    b = block_after(MOVE, "ACMD(do_runto)")
    assert "GET_KQUEST_ACTIVE(ch)" not in b
    assert "GET_NAME(target)" not in b

def test_normal_perform_move_contract_still_present():
    b = block_after(
        MOVE,
        "int perform_move(struct char_data *ch, int dir, int need_specials_check)",
    )
    assert "do_simple_move(ch, dir, need_specials_check)" in b
    assert "FIGHTING(ch)" in b
    assert "EX_CLOSED" in b

def test_normal_room_display_still_present():
    b = block_after(
        MOVE,
        "int do_simple_move(struct char_data *ch, int dir, int need_specials_check)",
    )
    assert "look_at_room(ch, 0)" in b

tests = [
    test_area_destination_contract,
    test_area_lookup_contract,
    test_runto_is_event_driven,
    test_runto_stop_command,
    test_event_moves_exactly_one_step_per_fire,
    test_event_uses_real_normal_movement,
    test_event_interrupts_for_combat_and_position,
    test_event_detects_trigger_relocation,
    test_event_is_paced_across_future_pulses,
    test_event_registered_as_character_event,
    test_old_synchronous_executor_removed,
    test_no_old_mobile_target_behavior,
    test_normal_perform_move_contract_still_present,
    test_normal_room_display_still_present,
]

for test in tests:
    test()

print(f"{len(tests)} tests passed: RUNTO paced area traversal V2")