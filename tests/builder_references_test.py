#!/usr/bin/env python3
"""Structural regression contract for the shared Builder References service."""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
service = (ROOT / "src/builder_refs.c").read_text()
header = (ROOT / "src/builder_refs.h").read_text()

assert "struct builder_reference" in header
assert all(field in header for field in ("display_name", "relationship", "location_vnum", "source"))
assert all(rel in service for rel in (
    "Spawned In", "Attached Trigger", "Incoming Exit", "Outgoing Exit",
    "Mob Reset", "Object Reset", "Loaded By Zone", "Contained In Object",
    "Equipped By Mob Reset", "Linked Zone"))
assert "builder_refs_invalidate" in service and "generation++" in service
assert "if (!index_valid) build_index();" in service
assert "No references found." in service
assert "Total References: %lu" in service
assert "return;" in service[service.index("static void add("):service.index("static void add_scripts")]

for editor, kind in (("medit.c", "BREF_MOB"), ("oedit.c", "BREF_OBJECT"),
                     ("redit.c", "BREF_ROOM"), ("dg_olc.c", "BREF_TRIGGER"),
                     ("zedit.c", "BREF_ZONE")):
    text = (ROOT / "src" / editor).read_text()
    assert "builder_refs_display" in text and kind in text
    assert "References (read-only)" in text

print("builder reference contracts: ok")
