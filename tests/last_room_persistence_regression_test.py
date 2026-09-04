"""Regression contracts for player room persistence and login precedence."""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
STRUCTS = (ROOT / "src/structs.h").read_text()
UTILS = (ROOT / "src/utils.h").read_text()
PLAYERS = (ROOT / "src/players.c").read_text()
OTHER = (ROOT / "src/act.other.c").read_text()
INTERP = (ROOT / "src/interpreter.c").read_text()
PFDEFAULTS = (ROOT / "src/pfdefaults.h").read_text()
CONFIG = (ROOT / "src/config.c").read_text()
COMM = (ROOT / "src/comm.c").read_text()
WIZARD = (ROOT / "src/act.wizard.c").read_text()


def section(text, start, end):
    i = text.index(start)
    return text[i:text.index(end, i)]


def test_optional_ascii_last_room_round_trip_and_old_file_default():
    assert "room_vnum last_room" in STRUCTS
    assert "GET_LAST_ROOM(ch)" in UTILS
    assert "GET_LAST_ROOM(ch) = NOWHERE;" in PLAYERS
    assert '"LRoom"' in PLAYERS
    assert '"LRoom: %d\\n"' in PLAYERS


def test_all_live_save_paths_capture_last_room_without_repurposing_loadroom():
    quit_body = section(OTHER, "ACMD(do_quit)", "ACMD(do_save)")
    save_body = section(PLAYERS, "int save_char", "/* Separate an id tag")
    assert "GET_LAST_ROOM(ch) = GET_ROOM_VNUM(IN_ROOM(ch));" in quit_body
    assert "VALID_ROOM_RNUM(IN_ROOM(ch))" in save_body
    assert "GET_LAST_ROOM(ch) = GET_ROOM_VNUM(IN_ROOM(ch));" in save_body
    assert "GET_ROOM_VNUM(IN_ROOM(ch)) != 0" in save_body
    assert "GET_LOADROOM(ch) = GET_ROOM_VNUM(IN_ROOM(ch));" not in quit_body
    assert "GET_LOADROOM(ch) = GET_ROOM_VNUM(IN_ROOM(ch));" not in save_body


def test_login_priority_is_forced_then_last_room_then_level_fallback_then_frozen():
    login = section(INTERP, "int enter_player_game", "EVENTFUNC(get_protocols)")
    forced = login.index("PLR_FLAGGED(d->character, PLR_LOADROOM)")
    last = login.index("GET_LAST_ROOM(d->character)", forced)
    fallback = login.index("if (load_room == NOWHERE)", last)
    frozen = login.index("PLR_FLAGGED(d->character, PLR_FROZEN)")
    assert forced < last < fallback < frozen


def test_missing_invalid_and_accidental_void_locations_fall_back_safely():
    login = section(INTERP, "static int enter_player_game_internal", "EVENTFUNC(get_protocols)")
    assert "GET_LAST_ROOM(d->character) != 0" in login
    assert "load_room = r_mortal_start_room;" in login
    assert "#define PFDEF_LOADROOM\t\tNOWHERE" in PFDEFAULTS


def test_new_character_start_is_separate_from_mortal_fallback():
    login = section(INTERP, "static int enter_player_game_internal", "EVENTFUNC(get_protocols)")
    assert "GET_LEVEL(d->character) == 0" in login
    assert "real_room(newbie_start_room)" in login
    assert "room_vnum newbie_start_room = 18600;" in CONFIG
    assert "room_vnum mortal_start_room = 3001;" in CONFIG


def test_copyover_preserves_staff_loadroom_and_resumes_live_room():
    copyover = section(WIZARD, "ACMD(do_copyover)", "ACMD(do_peace)")
    assert "GET_LAST_ROOM(och) = GET_ROOM_VNUM(IN_ROOM(och));" in copyover
    assert "GET_LOADROOM(och) = GET_ROOM_VNUM(IN_ROOM(och));" not in copyover
    assert "enter_player_game_copyover(d);" in COMM
    login = section(INTERP, "static int enter_player_game_internal", "EVENTFUNC(get_protocols)")
    resume = login.index("if (copyover && load_room == NOWHERE")
    forced = login.index("PLR_FLAGGED(d->character, PLR_LOADROOM)")
    assert resume < forced


if __name__ == "__main__":
    tests = [v for k, v in globals().items() if k.startswith("test_")]
    for test in tests:
        test()
    print(f"{len(tests)} last-room persistence regression tests passed")
