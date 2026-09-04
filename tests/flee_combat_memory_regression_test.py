from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
STRUCTS = (ROOT / "src/structs.h").read_text(encoding="utf-8")
HANDLER = (ROOT / "src/handler.h").read_text(encoding="utf-8")
MOBACT = (ROOT / "src/mobact.c").read_text(encoding="utf-8")
OFFENSIVE = (ROOT / "src/act.offensive.c").read_text(encoding="utf-8")


def test_memory_record_extended():
    assert "int combat_zone;" in STRUCTS
    assert "time_t away_since;" in STRUCTS
    assert "temporary_flee_memory" in STRUCTS


def test_helper_declared():
    assert "void remember_fleeing_opponent" in HANDLER


def test_flee_records_all_npc_attackers():
    start = OFFENSIVE.index("ACMD(do_flee)")
    end = OFFENSIVE.index("enum combat_skill_result perform_bash", start)
    block = OFFENSIVE[start:end]
    assert "remember_fleeing_opponent(was_fighting, ch);" in block
    assert "for (opponent = combat_list;" in block
    assert "FIGHTING(opponent) == ch" in block


def test_reengage_uses_existing_perception_path():
    assert "PERCEIVE_AGGRESSION" in MOBACT
    assert "mob_has_temporary_flee_memory(ch)" in MOBACT


def test_area_away_expiry():
    assert "#define FLEE_MEMORY_AWAY_SECONDS (5 * 60)" in MOBACT
    assert "curr->away_since = 0;" in MOBACT
    assert "zone_table[world[IN_ROOM(vict)].zone].number == curr->combat_zone" in MOBACT


def test_permanent_memory_not_activated_on_every_mob():
    assert "!MOB_FLAGGED(ch, MOB_MEMORY) && !names->temporary_flee_memory" in MOBACT


if __name__ == "__main__":
    tests = [v for k, v in globals().items() if k.startswith("test_")]
    for test in tests:
        test()
    print(f"{len(tests)} flee combat-memory regression tests passed")