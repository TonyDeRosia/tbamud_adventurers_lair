"""Schedule compatibility is retained without a primary builder menu path."""
from pathlib import Path


def test_schedule_data_and_runtime_helpers_are_retained():
    header = Path("src/ai_actor.h").read_text()
    actor = Path("src/ai_actor.c").read_text()
    for token in ("struct ai_schedule_entry", "schedules[AI_SCHEDULE_MAX]",
                  "struct ai_patrol_route", "patrols[AI_PATROL_MAX]"):
        assert token in header
    for helper in ("ai_schedule_add", "ai_schedule_select", "ai_patrol_advance"):
        assert helper in actor


def test_schedule_editor_has_no_primary_transition():
    medit = Path("src/medit.c").read_text()
    start = medit.index("case MEDIT_AI_MENU:", medit.index("void medit_parse("))
    root_parser = medit[start:medit.index("case MEDIT_LEGACY_MENU:", start)]
    assert "medit_disp_ai_schedule" not in root_parser
    assert "default: medit_disp_ai_menu(d); return;" in root_parser


if __name__ == "__main__":
    test_schedule_data_and_runtime_helpers_are_retained()
    test_schedule_editor_has_no_primary_transition()
    print("AI Actor schedule compatibility checks passed")
