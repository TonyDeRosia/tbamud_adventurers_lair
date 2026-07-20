#!/usr/bin/env python3
"""Source-level regression guards for AI Actor live-test defects."""
from pathlib import Path

medit = Path("src/medit.c").read_text()
utils = Path("src/utils.c").read_text()
mobact = Path("src/mobact.c").read_text()

# The enable confirmation is a string response, so Y/N reaches its parser case.
assert "OLC_MODE(d) != MEDIT_AI_ENABLE_CONFIRM" in medit
confirm_case = medit.split("case MEDIT_AI_ENABLE_CONFIRM:", 1)[1].split(
    "case MEDIT_AI_MODE:", 1
)[0]
assert "LOWER(*arg)=='y'" in confirm_case
assert "LOWER(*arg)=='n'" in confirm_case
assert "MOB_AI_ACTOR" in confirm_case
assert "medit_disp_ai_menu(d)" in confirm_case
assert "medit_disp_menu(d)" in confirm_case
assert "Please answer Y or N:" in confirm_case

# SKILL_SHADOW_RESERVOIR (253) remains PC-only when maximum mana is calculated.
mana_function = utils.split("int effective_max_mana", 1)[1].split(
    "void clamp_mana_to_effective_max", 1
)[0]
assert "!IS_NPC(ch)" in mana_function
assert "GET_SKILL((struct char_data *)ch, SKILL_SHADOW_RESERVOIR)" in mana_function

# The heartbeat only invokes the AI loop for explicitly flagged AI Actor mobiles.
assert "MOB_FLAGGED(ch, MOB_AI_ACTOR) && CONFIG_AI_ACTOR_ENABLED" in mobact

print("AI Actor defect regression checks passed")
