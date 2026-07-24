"""Focused guardrails for the read-only Phase 1 legacy behavior catalog."""
from pathlib import Path

root = Path(__file__).resolve().parents[1]
source = (root / "src" / "legacy_behavior.c").read_text()
header = (root / "src" / "legacy_behavior.h").read_text()

for name in ("Mayor", "Snake", "Thief", "Magic User", "Puff", "Fido", "Janitor",
             "Cityguard", "Postmaster", "Receptionist", "Cryogenicist", "Guildmaster",
             "Guild Guard", "Questmaster", "Shopkeeper"):
    assert f'{{"{name}"' in source, name

assert 'GET_MOB_VNUM(m)==3105&&f==mayor' in source
assert '"Unknown custom function"' in source
assert 'SHOP_KEEPER(i)==GET_MOB_RNUM(m)' in source
assert 'QST_MASTER(i)==GET_MOB_VNUM(m)' in source
assert 'MOB_FLAGGED(m,MOB_SPEC)&&!GET_MOB_SPEC(m)' in source
assert 'GET_MOB_SPEC(m)&&!MOB_FLAGGED(m,MOB_SPEC)' in source
assert 'MOB_SCAVENGER' in source and 'MOB_MEMORY' in source and 'MOB_HELPER' in source
assert 'legacy_ai_domains' in header and 'legacy_behavior_summary' in header

# Metadata must remain a discovery layer: no assignment or mobile pulse ownership.
assert 'ASSIGNMOB' not in source
assert 'mobile_activity' not in source
print("legacy behavior metadata checks passed")
