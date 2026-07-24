#!/usr/bin/env python3
"""Regression guards for AI Actor destination VNUM editor behavior."""
from pathlib import Path
import re

medit = Path("src/medit.c").read_text()
oasis = Path("src/oasis.h").read_text()
actor = Path("src/ai_actor.c").read_text()

# Mode 130 must bypass Oasis's legacy numeric pre-parser.
assert "#define MEDIT_AI_ROUTINE_RANDOM_VNUM     130" in oasis
assert "switch (mode)" in medit
assert "case MEDIT_AI_ROUTINE_TIMING:" in medit
assert "mode <= MEDIT_AI_ROUTINE_TIMING" not in medit
assert "if (medit_is_ai_mode(OLC_MODE(d)))" in medit

random = medit[medit.index("void medit_parse("):].split("case MEDIT_AI_ROUTINE_RANDOM:", 1)[1].split("case MEDIT_AI_ROUTINE_WAIT:", 1)[0]
assert "Add Random Destination" in random
assert "The NPC will calculate its own route from its current live room." in random
assert "Room VNUM (Q cancels):" in random
assert "medit_parse_ai_integer(arg,1,c->random_destination_count,&n)" in random
assert "random_destination_count++" in random  # only after the VNUM is validated
assert "Destination creation cancelled." in random
assert "Random destination added:" in random
assert "world[real_room(v)].name" in random
assert "enabled=TRUE" in random
assert "weight=1" in random
for message in ("Enter a numeric room VNUM, or Q to cancel.",
                "That room VNUM does not exist.", "ROOM_NOMOB", "death room",
                "outside this NPC's allowed zone", "already configured"):
    assert message in medit

# The shared validator resolves a VNUM, protects invalid/prohibited rooms, and
# keeps the authored VNUM rather than replacing it with a room RNUM.
validator = medit.split("static const char *medit_validate_routine_room", 1)[1].split("static int medit_parse_ai_integer", 1)[0]
assert "room = real_room(vnum)" in validator
assert "ROOM_FLAGGED(room, ROOM_NOMOB)" in validator
assert "ROOM_FLAGGED(room, ROOM_DEATH)" in validator
assert "world[room].zone != mob_zone" in validator
assert "random_destinations[i].room_vnum == vnum" in validator
assert re.search(r"random_destinations\[n\]\.room_vnum=v", random)
assert "random_destinations[n].room_vnum=real_room" not in random

# Scheduled room authoring has the same strict parser, cancellation, validation,
# and no-write-before-validation behavior.
scheduled = medit[medit.index("void medit_parse("):].split("case MEDIT_AI_SCHEDULE_ENTRY_VALUE:", 1)[1].split("case MEDIT_AI_SCHEDULE_DAYS:", 1)[0]
assert "Scheduled destination edit cancelled." in scheduled
assert "medit_parse_ai_integer(arg,1,INT_MAX,&i)" in scheduled
assert "medit_validate_routine_room(d,i,-1,e)" in scheduled
assert "e->destination_value=i" in scheduled
assert "atoi(arg)" not in scheduled.split("if(OLC_VAL(d)=='r')", 1)[1].split("if(OLC_VAL(d)=='2')", 1)[0]

# Inactive authoring is permitted and called out; summaries are enabled counts.
assert "destinations are inactive until a routine mode including Random Destination Travel is enabled" in medit
assert "%d enabled (%d authored)" in medit

# Warning totals count every WARNING class but not INFO-only notices.
assert "communication == AI_COMM_TELEPATHY) warnings++" in actor
assert "WARNING  Telepathy delivery" in actor
assert "INFO  Vocalize is effective" in actor
assert "STAY_ZONE blocks patrol route" in actor
print("AI Actor destination VNUM regression checks passed")
