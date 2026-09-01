"""Regression contracts for clean-logout room persistence."""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
STRUCTS = (ROOT / "src/structs.h").read_text()
UTILS = (ROOT / "src/utils.h").read_text()
PLAYERS = (ROOT / "src/players.c").read_text()
OTHER = (ROOT / "src/act.other.c").read_text()
INTERP = (ROOT / "src/interpreter.c").read_text()


def section(text, start, end):
    i = text.index(start)
    return text[i:text.index(end, i)]


def test_optional_ascii_last_room_round_trip_and_old_file_default():
    assert "room_vnum last_room" in STRUCTS
    assert "GET_LAST_ROOM(ch)" in UTILS
    assert "GET_LAST_ROOM(ch) = NOWHERE;" in PLAYERS
    assert '"LRoom"' in PLAYERS
    assert '"LRoom: %d\\n"' in PLAYERS


def test_quit_captures_last_room_but_manual_save_does_not_change_it():
    quit_body = section(OTHER, "ACMD(do_quit)", "ACMD(do_save)")
    save_body = section(OTHER, "ACMD(do_save)", "ACMD(do_recall)")
    assert "GET_LAST_ROOM(ch) = GET_ROOM_VNUM(IN_ROOM(ch));" in quit_body
    assert "GET_LAST_ROOM(ch)" not in save_body
    assert "GET_LOADROOM(ch) = GET_ROOM_VNUM(IN_ROOM(ch));" not in quit_body
    assert "GET_LOADROOM(ch) = GET_ROOM_VNUM(IN_ROOM(ch));" not in save_body


def test_login_priority_is_forced_then_last_room_then_level_fallback_then_frozen():
    login = section(INTERP, "int enter_player_game", "EVENTFUNC(get_protocols)")
    forced = login.index("PLR_FLAGGED(d->character, PLR_LOADROOM)")
    last = login.index("GET_LAST_ROOM(d->character)")
    fallback = login.index("if (load_room == NOWHERE)")
    frozen = login.index("PLR_FLAGGED(d->character, PLR_FROZEN)")
    assert forced < last < fallback < frozen


if __name__ == "__main__":
    tests = [v for k, v in globals().items() if k.startswith("test_")]
    for test in tests:
        test()
    print(f"{len(tests)} last-room persistence regression tests passed")
