from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
UTILS = (ROOT / "src" / "utils.h").read_text(encoding="utf-8")
HANDLER = (ROOT / "src" / "handler.c").read_text(encoding="utf-8")
OTHER = (ROOT / "src" / "act.other.c").read_text(encoding="utf-8")
MOVE = (ROOT / "src" / "act.movement.c").read_text(encoding="utf-8")
INFO = (ROOT / "src" / "act.informative.c").read_text(encoding="utf-8")
FIGHT = (ROOT / "src" / "fight.c").read_text(encoding="utf-8")
MOB = (ROOT / "src" / "mobact.c").read_text(encoding="utf-8")
SPELLS = (ROOT / "src" / "spell_parser.c").read_text(encoding="utf-8")
TRACK = (ROOT / "src" / "graph.c").read_text(encoding="utf-8")


def test_shared_layer_and_compatibility_wrapper():
    for context in ("PERCEIVE_ROOM_LIST", "PERCEIVE_DIRECT_TARGET", "PERCEIVE_SCAN",
                    "PERCEIVE_MOVEMENT", "PERCEIVE_AGGRESSION", "PERCEIVE_COMBAT"):
        assert context in UTILS
    assert "perceive_character((struct char_data *)(sub)" in UTILS
    assert "PERCEPTION_IDENTIFIED" in UTILS


def test_hard_counters_are_layered():
    invis = HANDLER.index("AFF_FLAGGED(target, AFF_INVISIBLE)")
    hide = HANDLER.index("AFF_FLAGGED(target, AFF_HIDE)", invis)
    invis_block = HANDLER[invis:hide]
    assert "AFF_DETECT_INVIS" in invis_block and "AFF_TRUESIGHT" in invis_block
    assert "AFF_SENSE_LIFE" not in invis_block and "MOB_AWARE" not in invis_block
    hide_block = HANDLER[hide:HANDLER.index("return PERCEPTION_IDENTIFIED", hide)]
    assert "AFF_SENSE_LIFE" in hide_block and "AFF_TRUESIGHT" in hide_block
    assert "AFF_DETECT_INVIS" not in hide_block and "AFF_INFRAVISION" not in hide_block


def test_hide_is_stable_and_breaks_on_movement_and_combat():
    assert "af.spell = SKILL_HIDE" in OTHER
    assert "af.modifier =" in OTHER
    assert "break_concealment(ch, CONCEAL_HIDE, \"movement\")" in MOVE
    assert "break_concealment(ch, CONCEAL_HIDE, \"combat\")" in FIGHT
    perception = HANDLER[HANDLER.index("enum perception_result perceive_character"):
                         HANDLER.index("void break_concealment")]
    assert "if (context == PERCEIVE_MOVEMENT)" in perception
    assert "context == PERCEIVE_MOVEMENT ? rand_number(-10, 10) : 0" in perception


def test_mobile_stealth_is_contextual_and_skulk_is_stronger():
    assert "PERCEPTION_SKULK_BONUS   25" in HANDLER
    assert "PERCEPTION_SNEAK_BONUS   10" in HANDLER
    assert "PERCEIVE_MOVEMENT" in MOVE
    assert "AFF_SNEAK" not in INFO[INFO.index("static void list_char_to_char"):INFO.index("static void do_auto_exits")]


def test_major_routes_use_explicit_contexts():
    assert "PERCEIVE_ROOM_LIST" in INFO and "PERCEIVE_SCAN" in INFO
    assert "PERCEIVE_AGGRESSION" in MOB
    assert "PERCEIVE_SPELL_TARGET" in SPELLS and "PERCEIVE_COMBAT" in SPELLS
    assert "get_char_vis(ch, arg, NULL, FIND_CHAR_WORLD)" not in TRACK


def test_red_eye_clue_cannot_leak_concealment():
    start = INFO.index("static void list_char_to_char(struct char_data *list")
    room = INFO[start:INFO.index("static void do_auto_exits", start)]
    assert "!AFF_FLAGGED(i, AFF_HIDE)" in room
    assert "!AFF_FLAGGED(i, AFF_INVISIBLE)" in room


def test_pickpocket_probabilities_and_combat_contracts_remain():
    assert "return MAX(5, MIN(75, chance));" in OTHER
    assert "Theft success and detection remain completely independent rolls" in OTHER
    assert "CONCEAL_HIDE | CONCEAL_INVIS" in OTHER
    assert "Entering real combat breaks mobile concealment" in FIGHT


def test_npc_stealth_never_reads_player_skill_storage():
    helper_start = HANDLER.index("static int concealment_skill_proficiency")
    score_start = HANDLER.index("int get_concealment_score", helper_start)
    score_end = HANDLER.index("int get_detection_score", score_start)
    helper = HANDLER[helper_start:score_start]
    scoring = HANDLER[score_start:score_end]

    assert "IS_NPC(target)" in helper
    assert "GET_LEVEL(target)" in helper
    assert "GET_SKILL(target, skill)" in helper
    assert "GET_SKILL(target, SKILL_SNEAK)" not in scoring
    assert "GET_SKILL(target, SKILL_SKULK)" not in scoring
    assert "concealment_skill_proficiency(target, SKILL_SNEAK)" in scoring
    assert "concealment_skill_proficiency(target, SKILL_SKULK)" in scoring

if __name__ == "__main__":
    tests = [value for name, value in sorted(globals().items())
             if name.startswith("test_") and callable(value)]
    for test in tests:
        test()
    print(f"{len(tests)} concealment/visibility regression tests passed")
