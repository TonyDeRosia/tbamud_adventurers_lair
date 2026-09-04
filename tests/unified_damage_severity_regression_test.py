from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
FIGHT = (ROOT / "src/fight.c").read_text(encoding="utf-8")
LIMITS = (ROOT / "src/limits.c").read_text(encoding="utf-8")


def section(text, start, end):
    a = text.index(start)
    b = text.index(end, a)
    return text[a:b]


def test_single_authoritative_damage_tiers():
    tier = section(
        FIGHT,
        "static int damage_severity_tier",
        "static const char *severity_color",
    )
    assert "pct <= 2" in tier
    assert "pct <= 6" in tier
    assert "pct <= 14" in tier
    assert "pct <= 24" in tier
    assert "pct <= 39" in tier
    assert "pct <= 54" in tier
    assert "pct <= 69" in tier
    assert "pct <= 84" in tier


def test_weapons_use_same_tier_function():
    start = FIGHT.index("/* message for doing damage with a weapon */")
    end = FIGHT.index("static const char *damage_ability_name", start)
    dam_message = FIGHT[start:end]
    assert "msgnum = damage_severity_tier(dam, victim);" in dam_message
    assert "pct <= 2" not in dam_message
    assert "pct <= 6" not in dam_message
    assert "pct <= 14" not in dam_message


def test_all_successful_nonweapon_damage_uses_severity_renderer():
    damage_block = section(
        FIGHT,
        "/* skill_message sends a message",
        "if (ch && victim && ch != victim && GET_POS(victim) > POS_DEAD",
    )
    assert "if (dam > 0)" in damage_block
    assert "nonweapon_damage_message(dam, ch, victim, attacktype);" in damage_block
    assert "int shown = skill_message(dam, ch, victim, attacktype);" not in damage_block


def test_custom_hit_messages_no_longer_gate_damage_severity():
    damage_block = section(
        FIGHT,
        "/* skill_message sends a message",
        "if (ch && victim && ch != victim && GET_POS(victim) > POS_DEAD",
    )
    severity_call = damage_block.index(
        "nonweapon_damage_message(dam, ch, victim, attacktype);"
    )
    death_flavor = damage_block.index(
        "if (GET_POS(victim) == POS_DEAD && ch && ch != victim)"
    )
    assert severity_call < death_flavor
    assert "skill_message(dam, ch, victim, attacktype);" in damage_block


def test_misses_still_use_spell_skill_message_library():
    damage_block = section(
        FIGHT,
        "/* skill_message sends a message",
        "if (ch && victim && ch != victim && GET_POS(victim) > POS_DEAD",
    )
    assert "skill_message(0, ch, victim, attacktype)" in damage_block
    assert "nonweapon_miss_message(ch, victim, attacktype);" in damage_block


def test_named_spell_and_skill_identity_is_preserved():
    helper = section(
        FIGHT,
        "static const char *damage_ability_name",
        "/*  message for doing damage with a spell or skill.",
    )
    assert "skill_name(attacktype)" in helper
    assert "format_severity_verb(verb_base" in helper
    assert "format_severity_verb(verb_third" in helper
    assert '"You %s $N with your %s%s"' in helper
    assert "physical_skill" in helper


def test_periodic_damage_uses_same_verb_table():
    helper = section(
        FIGHT,
        "static void nonweapon_damage_message",
        "static void nonweapon_miss_message",
    )
    assert "ch && ch != victim" in helper
    assert '"The %s %s you%s\\r\\n"' in helper
    assert "format_severity_verb(verb_third" in helper


def test_corruption_dot_keeps_spell_identity():
    start = LIMITS.index("corruption = find_affect(i, SPELL_CORRUPTION);")
    end = LIMITS.index("if (GET_POS(i) <= POS_STUNNED)", start)
    block = LIMITS[start:end]
    expected = (
        "damage(i, i, (corruption->modifier < 1 ? 1 : corruption->modifier), "
        "SPELL_CORRUPTION)"
    )
    assert expected in block


if __name__ == "__main__":
    tests = [v for k, v in globals().items() if k.startswith("test_")]
    for test in tests:
        test()
    print(f"{len(tests)} unified damage-severity regression tests passed")