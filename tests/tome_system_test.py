#!/usr/bin/env python3
"""Focused source contracts for builder-configured Tome behavior."""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
src = lambda name: (ROOT / "src" / name).read_text(encoding="utf-8")
structs, constants, tome, oedit, db, genobj, players, spell, practice = map(src,
    ["structs.h", "constants.c", "tome.c", "oedit.c", "db.c", "genobj.c", "players.c", "spell_parser.c", "spec_procs.c"])

assert "#define ITEM_TOME      24" in structs
assert "#define NUM_ITEM_TYPES    25" in structs
assert '"TOME",' in constants
assert "TOME_ABILITY_SLOTS 4" in src("tome.h")
assert "GET_OBJ_VAL(obj, i)" in tome
assert "tome_cooldown_seconds" in structs
assert "GET_OBJ_TIMER" not in tome
assert "tome_validate" in tome and "duplicates" in tome
assert "find_skill_num_with_ambig" in oedit and "OEDIT_TOME_MENU" in oedit
assert "case 'C': /* Tome study cooldown" in db
assert "fprintf(fp, \"C\\n%d\\n\"" in genobj
assert "tome_abilities" in structs and "TmAb:" in players and "TmCd:" in players
assert "has_tome_ability(ch, spellnum)" in spell
assert "has_tome_ability(ch, ability_id)" in practice
assert "extract_obj(obj);" in tome and "save_char(ch);" in tome
assert "GET_TOME_STUDY_EXPIRES_AT(ch) = now + obj->tome_cooldown_seconds" in tome
assert "tome status" in (ROOT / "lib" / "text" / "help" / "help.hlp").read_text(encoding="utf-8")
print("tome system source contracts passed")
