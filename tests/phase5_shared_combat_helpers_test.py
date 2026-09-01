"""Phase 5 architecture and safety contracts for DG physical skills."""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
OFF = (ROOT / "src/act.offensive.c").read_text(encoding="utf-8")
DG = (ROOT / "src/dg_misc.c").read_text(encoding="utf-8")
ACT = (ROOT / "src/act.h").read_text(encoding="utf-8")
INTERP = (ROOT / "src/interpreter.c").read_text(encoding="utf-8")
DB = (ROOT / "src/db.c").read_text(encoding="utf-8")
STRUCTS = (ROOT / "src/structs.h").read_text(encoding="utf-8")


def section(text, start, end):
    at = text.index(start)
    return text[at:text.index(end, at)]


def test_player_and_dg_share_each_authoritative_helper():
    for skill in ("backstab", "bash", "kick"):
        assert OFF.count(f"perform_{skill}(") == 2
        assert f"perform_{skill}(mob, vict, proficiency, FALSE" in DG
        assert f"perform_{skill}(ch, vict, GET_SKILL(ch, SKILL_{skill.upper()})" in OFF
        assert f"enum combat_skill_result perform_{skill}" in ACT


def test_dg_wrapper_has_no_parallel_combat_formula():
    body = section(DG, "void do_dg_skill", "/* copied from spell_parser.c")
    for forbidden in ("damage(", "hit(", "\n    WAIT_STATE(", "compute_armor(", "GET_POS(vict)"):
        assert forbidden not in body
    assert "dg_execute_skill" not in OFF + DG + ACT


def test_whitelist_and_result_contract_are_explicit():
    body = section(DG, "void do_dg_skill", "/* copied from spell_parser.c")
    assert '(str_cmp(skill, "backstab") && str_cmp(skill, "bash") && str_cmp(skill, "kick"))' in body
    assert '"NOT_ATTEMPTED"' in body and '"ATTEMPTED"' in body
    assert 'command_interpreter' not in body


def test_normal_npc_command_paths_remain_blocked():
    for skill in ("backstab", "bash", "kick"):
        body = section(OFF, f"ACMD(do_{skill})", "\n}\n")
        assert "IS_NPC(ch)" in body
        assert re.search(rf'\{{ "{skill}"\s*,', INTERP)


def test_shared_validation_covers_room_position_self_and_peaceful():
    common = section(OFF, "static int physical_skill_target_ok", "enum combat_skill_result perform_backstab")
    for contract in ("ch != vict", "IN_ROOM(ch) != NOWHERE", "IN_ROOM(ch) == IN_ROOM(vict)",
                     "GET_POS(ch) >= POS_FIGHTING", "ROOM_PEACEFUL"):
        assert contract in common


def test_backstab_preserves_weapon_roll_damage_and_lag():
    body = section(OFF, "enum combat_skill_result perform_backstab", "ACMD(do_backstab)")
    for contract in ("WEAR_WIELD", "TYPE_PIERCE", "FIGHTING(vict)", "MOB_AWARE",
                     "rand_number(1, 101)", "hit(ch, vict, SKILL_BACKSTAB)",
                     "damage(ch, vict, 0, SKILL_BACKSTAB)", "2 * PULSE_VIOLENCE"):
        assert contract in body


def test_bash_preserves_roll_knockdown_damage_and_lag():
    body = section(OFF, "enum combat_skill_result perform_bash", "ACMD(do_bash)")
    for contract in ("WEAR_WIELD", "MOB_NOKILL", "MOB_NOBASH", "damage(ch, vict, 1, SKILL_BASH)",
                     "WAIT_STATE(vict, PULSE_VIOLENCE)", "GET_POS(vict) = POS_SITTING",
                     "WAIT_STATE(ch, PULSE_VIOLENCE * 2)"):
        assert contract in body


def test_kick_preserves_armor_damage_and_lag():
    body = section(OFF, "enum combat_skill_result perform_kick", "ACMD(do_kick)")
    for contract in ("compute_armor(vict)", "rand_number(1, 101)", "GET_LEVEL(ch) / 2",
                     "damage(ch, vict", "WAIT_STATE(ch, PULSE_VIOLENCE * 3)"):
        assert contract in body


def test_npc_proficiency_is_bounded_and_npcs_do_not_improve():
    assert "proficiency = MIN(90, 30 + GET_LEVEL(mob));" in DG
    assert "perform_backstab(mob, vict, proficiency, FALSE)" in DG
    assert "if (improve)" in OFF


def test_cooldowns_remain_instance_owned_bounded_and_freed():
    assert "struct dg_cooldown_entry *dg_cooldowns" in STRUCTS
    assert "#define DG_COOLDOWN_KEY_MAX 32" in STRUCTS
    assert "#define DG_COOLDOWN_MAX_ENTRIES 256" in STRUCTS
    assert "#define DG_COOLDOWN_MAX_SECONDS (7 * 24 * 60 * 60)" in DG
    assert "GET_IDNUM(player)" in DG
    assert "dg_cooldowns_free(ch);" in DB


if __name__ == "__main__":
    tests = [v for k, v in globals().items() if k.startswith("test_")]
    for test in tests:
        test()
    print(f"{len(tests)} Phase 5 shared-combat regression tests passed")
