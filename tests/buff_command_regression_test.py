"""Phase 6 contracts for targeted persistent buff sequencing."""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SPELL = (ROOT / "src/spell_parser.c").read_text(encoding="utf-8")
INTERP = (ROOT / "src/interpreter.c").read_text(encoding="utf-8")
HEADER = (ROOT / "src/spells.h").read_text(encoding="utf-8")
HELP = (ROOT / "lib/text/help/help.hlp").read_text(encoding="utf-8")


def section(text, start, end):
    pos = text.index(start)
    return text[pos:text.index(end, pos)]


def test_command_registration_and_help():
    assert '{ "buff"     , "buff"    , POS_SITTING , do_buff     , 1, 0 },' in INTERP
    assert "ACMD(do_buff);" in HEADER
    assert "BUFF\n\nUsage:\n  buff <target>" in HELP


def test_missing_local_numbered_and_self_targeting():
    body = section(SPELL, "ACMD(do_buff)", "/* do_cast is the entry point")
    assert '"Buff whom?\\r\\n"' in body
    assert "number = get_number(&argp);" in body
    assert "get_char_vis(ch, argp, &number, FIND_CHAR_ROOM)" in body
    assert "FIND_CHAR_WORLD" not in body
    assert '"Use SPELLUP to buff yourself.\\r\\n"' in body


def test_metadata_driven_other_target_policy():
    body = section(SPELL, "static bool is_buff_target_eligible_spell", "static void normalize_ability_input")
    assert "is_persistent_beneficial_spell(spellnum)" in body
    assert "TAR_SELF_ONLY" in body
    assert "TAR_CHAR_ROOM | TAR_CHAR_WORLD" in body
    assert "TAR_NOT_SELF" not in body


def test_persistent_gate_excludes_violence_and_one_shot_routines():
    body = section(SPELL, "static bool is_persistent_beneficial_spell", "static bool is_spellup_eligible_spell")
    assert "SINFO.violent" in body
    assert "MAG_AFFECTS" in body and "MAG_MANUAL" in body
    assert "MAG_DAMAGE | MAG_POINTS | MAG_UNAFFECTS" in body
    for utility in ("SPELL_IDENTIFY", "SPELL_CURE_LIGHT", "SPELL_HEAL", "SPELL_CLEANSE",
                    "SPELL_WORD_OF_RECALL", "SPELL_TELEPORT", "SPELL_GREATER_TELEPORTATION"):
        assert utility in body


def test_shared_sequence_preserves_real_cast_rules():
    body = section(SPELL, "static void perform_automatic_buff_sequence", "ACMD(do_spellup)")
    for contract in ("can_character_cast_known_spell", "is_spellup_buff_active(tch",
                     "spell_on_cooldown", "mag_manacost", "AFF_SPELLLOCK",
                     "GET_SKILL(ch, spellnum)", "cast_spell(ch, tch, NULL, spellnum)",
                     "improve_ability_from_use", "WAIT_STATE", "GET_MANA(ch)"):
        assert contract in body


def test_environment_combat_and_extraction_guards():
    body = section(SPELL, "static void perform_automatic_buff_sequence", "ACMD(do_spellup)")
    for contract in ("ROOM_NOMAGIC", "ROOM_EFFECT_NULL_FIELD", "ROOM_EFFECT_SILENCE_FIELD",
                     "AFF_SILENCED", "FIGHTING(ch) && other_target", "DEAD(ch)", "DEAD(tch)",
                     "IN_ROOM(tch) != start_room"):
        assert contract in body


def test_target_receives_cast_and_target_aware_active_check():
    body = section(SPELL, "static void perform_automatic_buff_sequence", "ACMD(do_spellup)")
    assert "is_spellup_buff_active(tch, spellnum)" in body
    assert "cast_spell(ch, tch, NULL, spellnum)" in body
    assert "cast_spell(ch, ch" not in body


def test_spellup_remains_on_shared_self_eligible_path():
    body = section(SPELL, "ACMD(do_spellup)", "ACMD(do_buff)")
    assert "struct char_data *tch = ch;" in body
    assert "perform_automatic_buff_sequence(ch, tch, tch != ch);" in body
    predicate = section(SPELL, "static bool is_spellup_eligible_spell", "static bool is_buff_target_eligible_spell")
    assert "TAR_SELF_ONLY | TAR_FIGHT_SELF" in predicate


def test_summary_is_concise_and_targeted():
    body = section(SPELL, "static void perform_automatic_buff_sequence", "ACMD(do_spellup)")
    assert "Buff complete on %s: %d cast, %d already active, %d low mana, %d unavailable." in body


if __name__ == "__main__":
    tests = [value for name, value in globals().items() if name.startswith("test_")]
    for test in tests:
        test()
    print(f"{len(tests)} buff command regression tests passed")
