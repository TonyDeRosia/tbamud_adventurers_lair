"""Regression contracts for SHOWVNUMS color ownership in room contents."""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
INFO = (ROOT / "src/act.informative.c").read_text(encoding="utf-8")


def section(start: str, end: str) -> str:
    pos = INFO.index(start)
    return INFO[pos:INFO.index(end, pos)]


def test_character_list_does_not_apply_a_blanket_yellow_color():
    body = section(
        "static void list_char_to_char(struct char_data *list, struct char_data *ch)\n{",
        "static void do_auto_exits(",
    )
    assert "CCYEL(ch, C_NRM)" not in body
    assert 'send_to_char(ch, "%s", CCNRM(ch, C_NRM));' in body


def test_mob_and_trigger_vnums_own_and_reset_their_color():
    body = section(
        "static void list_one_char(struct char_data *i, struct char_data *ch)\n{",
        "static void list_char_to_char(struct char_data *list, struct char_data *ch)\n{",
    )
    yellow = 'send_to_char(ch, "%s", CCYEL(ch, C_NRM));'
    reset = 'send_to_char(ch, "%s", CCNRM(ch, C_NRM));'
    mob = 'send_to_char(ch, "[%d] ", GET_MOB_VNUM(i));'
    assert body.index(yellow) < body.index(mob) < body.index(reset)
    assert body.index(reset) < body.index("build_visible_target_tags(ch, i")


def test_object_and_trigger_vnums_reset_before_tags_and_descriptions():
    body = section(
        "static void show_obj_to_char(struct obj_data *obj, struct char_data *ch, int mode)\n{",
        "static void show_obj_modifiers(",
    )
    for marker in ("case SHOW_OBJ_LONG:", "case SHOW_OBJ_SHORT:"):
        start = body.index(marker)
        end = body.find("case ", start + len(marker))
        branch = body[start:end if end != -1 else len(body)]
        assert 'CCYEL(ch, C_NRM), GET_OBJ_VNUM(obj)' in branch
        assert 'send_to_char(ch, "%s", CCNRM(ch, C_NRM));' in branch
        assert branch.index("CCNRM(ch, C_NRM)") < branch.index("build_obj_aura_tags")


def test_room_title_resets_before_independent_room_content():
    body = section("void look_at_room(", "static void look_in_direction(")
    reset = 'send_to_char(ch, "%s\\r\\n", CCNRM(ch, C_NRM));'
    assert reset in body
    assert body.index(reset) < body.index("world[IN_ROOM(ch)].description")
    assert body.index(reset) < body.index("list_obj_to_char(")
    assert body.index(reset) < body.index("list_char_to_char(")


if __name__ == "__main__":
    tests = [value for name, value in globals().items() if name.startswith("test_")]
    for test in tests:
        test()
    print(f"{len(tests)} SHOWVNUMS color-boundary regression tests passed")
