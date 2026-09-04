from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MAGIC = (ROOT / "src/magic.c").read_text(encoding="utf-8")
SPELLS = (ROOT / "src/spells.c").read_text(encoding="utf-8")
LIMITS = (ROOT / "src/limits.c").read_text(encoding="utf-8")
MESSAGES = (ROOT / "lib/misc/messages").read_text(encoding="utf-8")


def test_chill_touch_damage():
    start = MAGIC.index("case SPELL_CHILL_TOUCH:")
    end = MAGIC.index("case SPELL_BURNING_HANDS:", start)
    block = MAGIC[start:end]
    assert "DAM_COLD" in block
    assert "dice(2, 8)" in block
    assert "dice(2, 6)" in block
    assert "level / 2" in block


def test_corruption_initial_damage():
    start = SPELLS.index("ASPELL(spell_corruption)")
    end = SPELLS.index("ASPELL(spell_plague_bolt)", start)
    block = SPELLS[start:end]
    assert "dice(1, 6) + MAX(1, caster_level)" in block
    assert "set_next_damage_type(DAM_NECROTIC);" in block
    assert "damage(ch, victim, initial_damage, SPELL_CORRUPTION);" in block


def test_corruption_dot():
    assert "int damage = 2 + (level / 12);" in SPELLS
    assert "Corrupting energy tears through you from within!" in LIMITS
    assert "set_next_damage_type(DAM_NECROTIC);" in LIMITS
    assert "TYPE_SUFFERING" in LIMITS


def test_messages():
    assert "* chill touch 8" in MESSAGES
    assert "Your freezing touch bites deep into $N!" in MESSAGES
    assert "* corruption 55" in MESSAGES
    assert "Your corrupting energy tears into $N!" in MESSAGES


if __name__ == "__main__":
    tests = [v for k, v in globals().items() if k.startswith("test_")]
    for test in tests:
        test()
    print(f"{len(tests)} Chill Touch / Corruption regression tests passed")