"""Regression contracts for reliable renewable dynamic quest targets."""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
QUEST = (ROOT / "src/quest.c").read_text(encoding="utf-8")


def section(text, start, end):
    a = text.index(start)
    b = text.index(end, a)
    return text[a:b]


def test_shared_reliable_reset_validator_exists():
    helper = section(
        QUEST,
        "static bool mob_has_reliable_local_quest_reset",
        "struct campaign_candidate_data",
    )
    assert "cmd->command != 'M'" in helper
    assert "cmd->if_flag" in helper
    assert "cmd->arg1 == mr" in helper
    assert "cmd->arg3 == room" in helper
    assert "cmd->arg2 > 0" in helper


def test_cross_zone_imports_are_rejected():
    helper = section(
        QUEST,
        "static bool mob_has_reliable_local_quest_reset",
        "struct campaign_candidate_data",
    )
    assert "vnum < zone_table[zone].bot" in helper
    assert "vnum > zone_table[zone].top" in helper


def test_campaign_and_regular_quest_share_reliability_filter():
    campaign = section(
        QUEST,
        "static int select_campaign_targets",
        "static struct char_data *select_kill_quest_target",
    )
    regular = section(
        QUEST,
        "static struct char_data *select_kill_quest_target",
        "static void quest_request_kill",
    )
    assert "mob_has_reliable_local_quest_reset(mob)" in campaign
    assert "mob_has_reliable_local_quest_reset(mob)" in regular


def test_qvalidate_reports_unreliable_targets():
    qvalidate = section(
        QUEST,
        "static const char *qvalidate_base_reason",
        "static int qvalidate_kill_level_ok",
    )
    assert 'return "no reliable local reset";' in qvalidate


def test_regular_dynamic_quests_remain_vnum_based():
    request = section(
        QUEST,
        "static void quest_request_kill",
        "static const char *qvalidate_base_reason",
    )
    assert "GET_KQUEST_TARGET_ID(ch) = 0;" in request
    assert "Dynamic kill quests are VNUM-based, not instance-based." in request


if __name__ == "__main__":
    tests = [v for k, v in globals().items() if k.startswith("test_")]
    for test in tests:
        test()
    print(f"{len(tests)} quest target reliability regression tests passed")