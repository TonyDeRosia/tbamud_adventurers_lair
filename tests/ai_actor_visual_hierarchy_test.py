"""The visible AI hierarchy is the documented three-item additive menu."""
from pathlib import Path


def test_visual_hierarchy_matches_legacy_first_contract():
    source = Path("src/medit.c").read_text()
    start = source.rindex("static void medit_disp_ai_menu(struct descriptor_data *d)\n{")
    menu = source[start:source.index("static void medit_disp_legacy_menu", start)]
    for token in ("AI Actor Extensions", "Optional additions only",
                  "1) Personality", "2) Identity / Role", "3) Advanced Perception"):
        assert token in menu
    for obsolete in ("Communication", "Schedule", "Patrol", "Combat Behavior",
                     "Advanced AI Brain", "Idle Behavior", "Cooldowns"):
        assert f") {obsolete}" not in menu


if __name__ == "__main__":
    test_visual_hierarchy_matches_legacy_first_contract()
    print("AI Actor visual hierarchy checks passed")
