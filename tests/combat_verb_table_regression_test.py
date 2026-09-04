from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
FIGHT = (ROOT / "src/fight.c").read_text(encoding="utf-8")


def section(text, start, end):
    a = text.index(start)
    b = text.index(end, a)
    return text[a:b]


def test_universal_base_verb_table():
    block = section(
        FIGHT,
        "static const char *severity_verb_base",
        "static const char *severity_verb_third",
    )
    expected = [
        'case 1: return "graze";',
        'case 2: return "wound";',
        'case 3: return "hit";',
        'case 4: return "injure";',
        'case 5: return "ravage";',
        'case 6: return "overwhelm";',
        'case 7: return "devastate";',
        'case 8: return "obliterate";',
        'case 9: return "CENSORED";',
    ]
    for line in expected:
        assert line in block


def test_universal_third_person_verb_table():
    block = section(
        FIGHT,
        "static const char *severity_verb_third",
        "static const char *severity_impact_wrap_open",
    )
    expected = [
        'case 1: return "grazes";',
        'case 2: return "wounds";',
        'case 3: return "hits";',
        'case 4: return "injures";',
        'case 5: return "ravages";',
        'case 6: return "overwhelms";',
        'case 7: return "devastates";',
        'case 8: return "obliterates";',
        'case 9: return "CENSORED";',
    ]
    for line in expected:
        assert line in block


def test_tier_nine_exact_censored_pattern():
    block = section(
        FIGHT,
        "static void format_severity_verb",
        "static int victim_condition_band",
    )
    assert '\\tD#\\tw#\\tDC\\twE\\tDN\\twS\\twO\\tDR\\twE\\tDD\\tw#\\tD#\\tn' in block
    assert "if (tier == 9)" in block


def test_weapon_damage_uses_shared_formatter():
    block = section(
        FIGHT,
        "/* message for doing damage with a weapon */",
        "static const char *damage_ability_name",
    )
    assert "msgnum = damage_severity_tier(dam, victim);" in block
    assert "format_severity_verb(verb_base" in block
    assert "format_severity_verb(verb_third" in block
    assert "dam_weapons[]" not in block
    assert "int pct" not in block


def test_nonweapon_damage_uses_shared_formatter():
    block = section(
        FIGHT,
        "static void nonweapon_damage_message",
        "static void nonweapon_miss_message",
    )
    assert "format_severity_verb(verb_base" in block
    assert "format_severity_verb(verb_third" in block
    assert "severity_verb_base(tier)" not in block
    assert "severity_verb_third(tier)" not in block


def test_old_nonuniversal_upper_verbs_removed_from_tables():
    base = section(
        FIGHT,
        "static const char *severity_verb_base",
        "static const char *severity_verb_third",
    )
    third = section(
        FIGHT,
        "static const char *severity_verb_third",
        "static const char *severity_impact_wrap_open",
    )
    for old in ("slam", "crush", "blast", "shred", "pulverize"):
        assert f'"{old}"' not in base
    for old in ("slams", "crushes", "blasts", "shreds", "pulverizes"):
        assert f'"{old}"' not in third


def test_existing_damage_thresholds_unchanged():
    block = section(
        FIGHT,
        "static int damage_severity_tier",
        "static const char *severity_color",
    )
    for threshold in (
        "pct <= 2",
        "pct <= 6",
        "pct <= 14",
        "pct <= 24",
        "pct <= 39",
        "pct <= 54",
        "pct <= 69",
        "pct <= 84",
    ):
        assert threshold in block


if __name__ == "__main__":
    tests = [v for k, v in globals().items() if k.startswith("test_")]
    for test in tests:
        test()
    print(f"{len(tests)} combat verb-table regression tests passed")