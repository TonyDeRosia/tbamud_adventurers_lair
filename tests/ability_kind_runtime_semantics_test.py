from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]

PARSER = (ROOT / "src" / "spell_parser.c").read_text(encoding="utf-8")
DG = (ROOT / "src" / "dg_misc.c").read_text(encoding="utf-8")
OTHER = (ROOT / "src" / "act.other.c").read_text(encoding="utf-8")
FIGHT = (ROOT / "src" / "fight.c").read_text(encoding="utf-8")
MAGIC = (ROOT / "src" / "magic.c").read_text(encoding="utf-8")
INFO = (ROOT / "src" / "act.informative.c").read_text(encoding="utf-8")
DB = (ROOT / "src" / "db.c").read_text(encoding="utf-8")
OEDIT = (ROOT / "src" / "oedit.c").read_text(encoding="utf-8")
SPELLS_H = (ROOT / "src" / "spells.h").read_text(encoding="utf-8")
STRUCTS_H = (ROOT / "src" / "structs.h").read_text(encoding="utf-8")

def define_value(text: str, name: str) -> int:
    m = re.search(rf"^#define\s+{re.escape(name)}\s+(\d+)\b", text, re.M)
    assert m, name
    return int(m.group(1))

def test_numeric_layout_contract_is_unchanged():
    assert define_value(SPELLS_H, "MAX_SPELLS") == 230
    assert define_value(SPELLS_H, "NUM_SPELLS") == 230
    assert define_value(STRUCTS_H, "MAX_SKILLS") == 272
    assert define_value(SPELLS_H, "SPELL_DG_AFFECT") == 298
    assert define_value(SPELLS_H, "TOP_SPELL_DEFINE") == 299

def test_player_casting_uses_explicit_spell_kind():
    assert PARSER.count("if (!ability_is_spell(spellnum))") >= 2
    assert (
        "return (ability_is_spell(spellnum) && spell_info[spellnum].name"
        in PARSER
    )
    assert (
        PARSER.count(
            "for (spellnum = 1; spellnum <= TOP_SPELL_DEFINE; spellnum++)"
        )
        >= 2
    )
    assert (
        "for (spellnum = 1; spellnum <= MAX_SPELLS; spellnum++)"
        not in PARSER
    )
    assert "if ((spellnum < 1) || (spellnum > MAX_SPELLS)) {" not in PARSER

def test_dg_casting_uses_explicit_spell_kind():
    assert "if (!ability_is_spell(spellnum)) {" in DG
    assert "spellnum > MAX_SPELLS" not in DG

def test_study_stored_spell_slots_use_kind():
    assert "if (!ability_is_spell(sid))" in OTHER
    assert "sid > MAX_SPELLS" not in OTHER

def test_damage_message_skill_identity_uses_kind():
    assert FIGHT.count("physical_skill = ability_is_skill(attacktype);") == 2
    assert "physical_skill = (attacktype > MAX_SPELLS" not in FIGHT

def test_combat_spell_hooks_use_kind():
    assert FIGHT.count("ability_is_spell(attacktype)") >= 5
    assert "IS_SPELL(attacktype)" not in FIGHT
    assert "attacktype > 0 && attacktype <= MAX_SPELLS" not in FIGHT

def test_magic_affect_spell_semantics_use_kind():
    assert MAGIC.count("ability_is_spell(af->spell)") >= 2
    assert "af->spell <= MAX_SPELLS" not in MAGIC

def test_live_help_lists_filter_by_kind():
    assert "for (i = 1; i <= TOP_SPELL_DEFINE; i++)" in INFO
    assert (
        "show_skills ? !ability_is_skill(i) : !ability_is_spell(i)"
        in INFO
    )
    assert "int start = show_skills ? MAX_SPELLS + 1 : 1;" not in INFO
    assert "int end = show_skills ? TOP_SPELL_DEFINE : MAX_SPELLS;" not in INFO

def test_object_boot_and_olc_work_is_still_deferred():
    # Phase 2 intentionally leaves object spell authoring/boot validation
    # for the next isolated phase.
    assert "MAX_SPELLS" in DB
    assert "NUM_SPELLS" in OEDIT

def run():
    tests = [
        test_numeric_layout_contract_is_unchanged,
        test_player_casting_uses_explicit_spell_kind,
        test_dg_casting_uses_explicit_spell_kind,
        test_study_stored_spell_slots_use_kind,
        test_damage_message_skill_identity_uses_kind,
        test_combat_spell_hooks_use_kind,
        test_magic_affect_spell_semantics_use_kind,
        test_live_help_lists_filter_by_kind,
        test_object_boot_and_olc_work_is_still_deferred,
    ]

    for test in tests:
        test()

    print(f"{len(tests)} tests passed: runtime ability-kind semantics V1")

if __name__ == "__main__":
    run()