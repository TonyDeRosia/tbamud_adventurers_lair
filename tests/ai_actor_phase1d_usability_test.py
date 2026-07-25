#!/usr/bin/env python3
"""Contract coverage for the surviving additive AI Actor MEDIT controls."""
from pathlib import Path


def ai_root_sections():
    medit = Path("src/medit.c").read_text()
    menu_start = medit.rindex("static void medit_disp_ai_menu(struct descriptor_data *d)\n{")
    menu = medit[menu_start:medit.index("static void medit_disp_legacy_menu", menu_start)]
    parser_start = medit.index("case MEDIT_AI_MENU:", medit.index("void medit_parse("))
    parser = medit[parser_start:medit.index("case MEDIT_LEGACY_MENU:", parser_start)]
    return menu, parser


def test_primary_menu_is_additive_only():
    menu, parser = ai_root_sections()
    for label in ("1) Personality", "2) Identity / Role", "3) Advanced Perception"):
        assert label in menu
    for obsolete in ("Communication", "Daily Routine", "Combat Behavior", "Advanced AI Brain"):
        assert f") {obsolete}" not in menu
    for target in ("medit_disp_ai_communication", "medit_disp_ai_schedule",
                   "medit_disp_ai_combat", "medit_disp_ai_social"):
        assert target not in parser


def test_capability_data_remains_compatible_but_hidden():
    """Historical fields/modes remain loadable; the root has no transition to them."""
    header = Path("src/ai_actor.h").read_text()
    oasis = Path("src/oasis.h").read_text()
    for field in ("communication", "memory_style", "assistance_style"):
        assert field in header
    for mode in ("MEDIT_AI_CAPABILITIES", "MEDIT_AI_SOCIAL", "MEDIT_AI_DIALOGUE"):
        assert mode in oasis


if __name__ == "__main__":
    test_primary_menu_is_additive_only()
    test_capability_data_remains_compatible_but_hidden()
    print("AI Actor Phase 1D legacy-first contract checks passed")
