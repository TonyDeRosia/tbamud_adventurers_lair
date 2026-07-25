"""Phase 3 data compatibility and current perception reachability coverage."""
from pathlib import Path


def test_only_perception_is_reachable_from_phase3_builder_controls():
    medit = Path("src/medit.c").read_text()
    start = medit.index("case MEDIT_AI_MENU:", medit.index("void medit_parse("))
    parser = medit[start:medit.index("case MEDIT_LEGACY_MENU:", start)]
    assert "case '3': medit_disp_ai_perception(d); return;" in parser
    for hidden in ("medit_disp_ai_memory", "medit_disp_ai_threat",
                   "medit_disp_ai_combat", "medit_disp_ai_schedule",
                   "medit_disp_ai_patrol"):
        assert hidden not in parser


def test_historical_phase3_runtime_and_persistence_contract_remains():
    header = Path("src/ai_actor.h").read_text()
    actor = Path("src/ai_actor.c").read_text()
    for structure in ("struct ai_schedule_entry", "struct ai_patrol_route",
                      "memory_enabled", "combat_enabled"):
        assert structure in header
    for validator in ("ai_actor_schedule_validate", "ai_actor_combat_validate"):
        assert validator in actor


if __name__ == "__main__":
    test_only_perception_is_reachable_from_phase3_builder_controls()
    test_historical_phase3_runtime_and_persistence_contract_remains()
    print("AI Actor Phase 3 legacy-first contract checks passed")
