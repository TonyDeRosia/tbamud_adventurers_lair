#!/usr/bin/env python3
"""Source-level regression guards for AI Actor live-test defects."""
from pathlib import Path
import re

medit = Path("src/medit.c").read_text()
utils = Path("src/utils.c").read_text()
mobact = Path("src/mobact.c").read_text()

# The enable confirmation is a string response, so Y/N reaches its parser case.
assert "!medit_is_ai_mode(OLC_MODE(d))" in medit
confirm_case = medit.split("case MEDIT_AI_ENABLE_CONFIRM:", 1)[1].split(
    "case MEDIT_AI_MODE:", 1
)[0]
assert '"yes"' in confirm_case
assert '"no"' in confirm_case
assert "MOB_AI_ACTOR" in confirm_case
assert "medit_disp_ai_menu(d)" in confirm_case
assert "medit_disp_menu(d)" in confirm_case
assert "Please answer Y or N:" in confirm_case

# Textual navigation and values are handled in AI mode handlers, never by the
# legacy numeric pre-parser.  This is the regression behind trapped Q/Y input.
assert "medit_parse_ai_integer" in medit
assert "medit_parse_ai_boolean" in medit
perception = medit.rsplit("case MEDIT_AI_PERCEPTION:", 1)[1].split(
    "case MEDIT_AI_MEMORY:", 1
)[0]
# Accept formatting changes while preserving Q/q and H/h navigation behavior.
assert re.search(r"LOWER\(\*arg\)\s*==\s*'q'", perception)
assert re.search(r"LOWER\(\*arg\)\s*==\s*'h'", perception)
assert "Y) Enable  N) Disable  T) Toggle" in perception
assert "Enter a whole number from 0 to 100" in perception

# SKILL_SHADOW_RESERVOIR (253) remains PC-only when maximum mana is calculated.
mana_function = utils.split("int effective_max_mana", 1)[1].split(
    "void clamp_mana_to_effective_max", 1
)[0]
assert "!IS_NPC(ch)" in mana_function
assert "GET_SKILL((struct char_data *)ch, SKILL_SHADOW_RESERVOIR)" in mana_function

# The heartbeat only invokes the AI loop for explicitly flagged AI Actor mobiles.
assert "MOB_FLAGGED(ch, MOB_AI_ACTOR) && CONFIG_AI_ACTOR_ENABLED" in mobact

print("AI Actor defect regression checks passed")
