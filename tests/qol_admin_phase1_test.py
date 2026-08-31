"""Focused regression contracts for the first player/admin QOL pass."""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
OTHER = (ROOT / "src/act.other.c").read_text(encoding="utf-8")
INFO = (ROOT / "src/act.informative.c").read_text(encoding="utf-8")
MOVE = (ROOT / "src/act.movement.c").read_text(encoding="utf-8")
INTERP = (ROOT / "src/interpreter.c").read_text(encoding="utf-8")
HANDLER = (ROOT / "src/handler.c").read_text(encoding="utf-8")


def section(text: str, start: str, end: str) -> str:
    pos = text.index(start)
    return text[pos:text.index(end, pos)]


def test_deliberate_quit_and_logout_close_the_descriptor():
    quit_body = section(OTHER, "ACMD(do_quit)", "ACMD(do_save)")
    assert "STATE(desc) = CON_DISCONNECT" in quit_body
    assert "STATE(desc) = CON_CLOSE" not in quit_body
    assert '"logout"' in INTERP
    assert "do_quit" in INTERP[INTERP.index('"logout"'):INTERP.index('"logout"') + 100]


def test_worth_always_displays_all_four_currencies():
    worth = section(INFO, "static void show_currency_only", "ACMD(do_worth)")
    assert "You have no currency" not in worth
    for label in ("Gold:", "Diamonds:", "Glory:", "Bank:"):
        assert label in worth


def test_run_matches_full_direction_words_before_abbreviations():
    parser = section(MOVE, "static int run_dir_from_token", "static int room_is_runto_safe")
    assert parser.index('"north"') < parser.index("token[0]) == 'n'")
    assert parser.index('"east"') < parser.index("token[0]) == 'e'")
    run = section(MOVE, "ACMD(do_run)", "ACMD(do_runto)")
    assert "perform_move(ch, dir, 0)" in run
    assert "reps > 1000" in run


def test_world_character_lookup_already_prioritizes_the_room():
    lookup = section(HANDLER, "struct char_data *get_char_world_vis", "struct char_data *get_char_vis")
    assert lookup.index("get_char_room_vis") < lookup.index("get_player_vis")
    assert lookup.index("get_player_vis") < lookup.index("for (i = character_list")


def test_ungrouped_report_is_visible_and_room_output_has_prompt_spacing():
    report = section(OTHER, "ACMD(do_report)", "ACMD(do_split)")
    assert "But you are not a member" not in report
    assert "TO_ROOM" in report
    look = section(INFO, "void look_at_room", "static void look_in_direction")
    assert 'list_char_to_char(world[IN_ROOM(ch)].people, ch);\n  send_to_char(ch, "\\r\\n");' in look


if __name__ == "__main__":
    tests = [value for name, value in globals().items() if name.startswith("test_")]
    for test in tests:
        test()
    print(f"{len(tests)} Phase 1 QOL regression tests passed")
