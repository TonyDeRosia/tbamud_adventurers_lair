"""Focused contracts for the bidirectional DELDIR/DIRDEL builder command."""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
COPY = (ROOT / "src/oasis_copy.c").read_text(encoding="utf-8")
INTERP = (ROOT / "src/interpreter.c").read_text(encoding="utf-8")
OASIS = (ROOT / "src/oasis.h").read_text(encoding="utf-8")
HELP = (ROOT / "lib/text/help/help.hlp").read_text(encoding="utf-8")


def section(start: str, end: str) -> str:
    pos = COPY.index(start)
    return COPY[pos:COPY.index(end, pos)]


def test_aliases_share_one_builder_command():
    assert '{ "deldir"   , "deldir"  , POS_DEAD    , do_deldir   , LVL_BUILDER, 0 }' in INTERP
    assert '{ "dirdel"   , "dirdel"  , POS_DEAD    , do_deldir   , LVL_BUILDER, 0 }' in INTERP
    assert "ACMD(do_deldir);" in OASIS


def test_missing_and_invalid_directions_are_helpful_and_non_mutating():
    body = section("ACMD(do_deldir)", "/* BuildWalk")
    usage = body.index("Usage: deldir <direction>")
    parse = body.index("dir = search_block(sdir, dirs, FALSE);")
    mutation = body.index("free_room_exit(source_room, dir);")
    assert usage < parse < mutation
    assert "Direction abbreviations" in body
    assert "is not a valid direction" in body


def test_permissions_are_checked_before_either_side_is_changed():
    body = section("ACMD(do_deldir)", "/* BuildWalk")
    source_permission = body.index("!can_edit_zone(ch, source_zone)")
    target_permission = body.index("!can_edit_zone(ch, target_zone)")
    mutation = body.index("free_room_exit(source_room, dir);")
    assert source_permission < target_permission < mutation
    assert "Nothing was changed" in body


def test_reverse_exit_is_removed_only_when_it_points_back():
    body = section("ACMD(do_deldir)", "/* BuildWalk")
    guard = "reverse_exit && reverse_exit->to_room == source_room"
    assert body.count(guard) >= 2
    assert "No matching return exit pointed back" in body
    assert "free_room_exit(target_room, rev_dir[dir]);" in body


def test_both_changed_zones_are_saved_and_action_is_logged():
    body = section("ACMD(do_deldir)", "/* BuildWalk")
    assert "add_to_save_list(zone_table[source_zone].number, SL_WLD);" in body
    assert "add_to_save_list(zone_table[target_zone].number, SL_WLD);" in body
    assert "used DELDIR" in body


def test_existing_dig_delete_reuses_safe_exit_cleanup():
    dig = section("ACMD(do_dig)", "ACMD(do_deldir)")
    assert "free_room_exit(IN_ROOM(ch), dir);" in dig
    assert "dig <direction> -1" in dig


def test_static_help_documents_bidirectional_and_permission_behavior():
    start = HELP.index("DELDIR DIRDEL")
    entry = HELP[start:HELP.index("#0", start)]
    assert "matching opposite exit" in entry
    assert "deldir d" in entry
    assert "permission to edit that zone" in entry


if __name__ == "__main__":
    tests = [value for name, value in globals().items() if name.startswith("test_")]
    for test in tests:
        test()
    print(f"{len(tests)} DELDIR command regression tests passed")
