from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]

SPELLS_H = (ROOT / "src" / "spells.h").read_text(encoding="utf-8")
PARSER = (ROOT / "src" / "spell_parser.c").read_text(encoding="utf-8")
UTILS = (ROOT / "src" / "utils.c").read_text(encoding="utf-8")
TOME = (ROOT / "src" / "tome.c").read_text(encoding="utf-8")
TRACK = (ROOT / "src" / "classtrack.c").read_text(encoding="utf-8")
INFO = (ROOT / "src" / "act.informative.c").read_text(encoding="utf-8")

def define_value(text: str, name: str) -> int:
    m = re.search(rf"^#define\s+{re.escape(name)}\s+(\d+)\b", text, re.M)
    assert m, name
    return int(m.group(1))

def test_numeric_boundaries_are_unchanged():
    assert define_value(SPELLS_H, "MAX_SPELLS") == 230
    assert define_value(SPELLS_H, "SPELL_DG_AFFECT") == 298
    assert define_value(SPELLS_H, "TOP_SPELL_DEFINE") == 299
    assert define_value(SPELLS_H, "SKILL_DOUBLE_ATTACK") == 270
    assert define_value(SPELLS_H, "SKILL_TRIPLE_ATTACK") == 271
    assert define_value(SPELLS_H, "SKILL_FOURTH_ATTACK") == 272

def test_explicit_ability_kind_metadata_exists():
    for marker in (
        "ABILITY_KIND_NONE = 0",
        "ABILITY_KIND_SPELL",
        "ABILITY_KIND_SKILL",
        "ABILITY_KIND_SYSTEM",
        "byte ability_kind;",
        "int ability_is_spell(int ability);",
        "int ability_is_skill(int ability);",
        "int ability_is_system(int ability);",
        "const char *ability_kind_name(int ability);",
    ):
        assert marker in SPELLS_H, marker

def test_registration_distinguishes_spell_skill_system():
    assert "abilityo(spl, ABILITY_KIND_SPELL" in PARSER
    assert (
        "#define skillo(skill, name) abilityo(skill, ABILITY_KIND_SKILL"
        in PARSER
    )
    assert (
        "#define skillo_cost(skill, name, cost) abilityo(skill, ABILITY_KIND_SKILL"
        in PARSER
    )
    assert (
        'abilityo(SPELL_DG_AFFECT, ABILITY_KIND_SYSTEM, "Script-inflicted"'
        in PARSER
    )
    assert "spell_info[spl].ability_kind = ABILITY_KIND_NONE;" in PARSER

def test_public_kind_helpers_read_metadata():
    assert (
        "spell_info[ability].ability_kind == ABILITY_KIND_SPELL"
        in PARSER
    )
    assert (
        "spell_info[ability].ability_kind == ABILITY_KIND_SKILL"
        in PARSER
    )
    assert (
        "spell_info[ability].ability_kind == ABILITY_KIND_SYSTEM"
        in PARSER
    )
    assert 'return "Spell";' in PARSER
    assert 'return "Skill";' in PARSER
    assert 'return "System";' in PARSER

def test_proficiency_semantics_use_explicit_kind():
    start = UTILS.index("void improve_ability_from_use")
    block = UTILS[start:]
    assert (
        "classtrack_record_ability_use(ch, ability, ability_is_spell(ability));"
        in block
    )
    assert "if (ability_is_spell(ability))" in block
    assert "ability <= MAX_SPELLS" not in block.split("\n}", 1)[0]

def test_tome_labels_use_explicit_kind():
    assert "ability_kind_name(ability)" in TOME
    assert "ability_kind_name(i)" in TOME
    assert 'ability <= MAX_SPELLS ? "Spell" : "Skill"' not in TOME
    assert 'i<=MAX_SPELLS?"Spell":"Skill"' not in TOME

def test_classtrack_filter_uses_explicit_kind():
    assert "if (!ability_is_spell(ability_id))" in TRACK
    assert "if (!ability_is_skill(ability_id))" in TRACK

def test_live_help_type_uses_explicit_kind():
    assert "type = ability_kind_name(ability);" in INFO
    assert 'type = (ability <= MAX_SPELLS) ? "Spell" : "Skill";' not in INFO

def test_runtime_casting_now_uses_explicit_kind():
    # Runtime Phase 2 deliberately widened spell discovery to the registered
    # table while filtering by explicit spell kind.
    assert "if (!ability_is_spell(spellnum))" in PARSER
    assert "ability_is_spell(spellnum) && spell_info[spellnum].name" in PARSER
    assert "for (spellnum = 1; spellnum <= TOP_SPELL_DEFINE; spellnum++)" in PARSER

def run():
    tests = [
        test_numeric_boundaries_are_unchanged,
        test_explicit_ability_kind_metadata_exists,
        test_registration_distinguishes_spell_skill_system,
        test_public_kind_helpers_read_metadata,
        test_proficiency_semantics_use_explicit_kind,
        test_tome_labels_use_explicit_kind,
        test_classtrack_filter_uses_explicit_kind,
        test_live_help_type_uses_explicit_kind,
        test_runtime_casting_now_uses_explicit_kind,
    ]

    for test in tests:
        test()

    print(f"{len(tests)} tests passed: ability kind foundation V1")

if __name__ == "__main__":
    run()