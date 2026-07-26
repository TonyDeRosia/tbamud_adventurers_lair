#!/usr/bin/env python3
"""Regression guards for read-only stat prototype lookup."""

from pathlib import Path
import re
import unittest


SOURCE = (Path(__file__).parents[1] / "src" / "act.wizard.c").read_text()


def function_body(name: str) -> str:
    match = re.search(rf"static (?:void|bool) {name}\([^)]*\)\n\{{", SOURCE)
    if not match:
        raise AssertionError(f"missing {name}")
    start = match.end()
    depth = 1
    for index in range(start, len(SOURCE)):
        if SOURCE[index] == "{":
            depth += 1
        elif SOURCE[index] == "}":
            depth -= 1
            if depth == 0:
                return SOURCE[start:index]
    raise AssertionError(f"unterminated {name}")


class StatPrototypeLookupTest(unittest.TestCase):
    def test_prototype_renderers_are_direct_and_read_only(self):
        for name, array in (("do_stat_mobile_prototype", "mob_proto"),
                            ("do_stat_object_prototype", "obj_proto")):
            body = function_body(name)
            self.assertIn(array, body)
            self.assertNotIn("read_mobile", body)
            self.assertNotIn("read_object", body)
            self.assertNotIn("char_to_room", body)
            self.assertNotIn("extract_", body)

    def test_typed_numeric_lookup_precedes_live_lookup(self):
        body = SOURCE[SOURCE.index("ACMD(do_stat)"):SOURCE.index("ACMD(do_shutdown)")]
        self.assertLess(body.index("do_stat_mobile_prototype"), body.index("get_char_vis"))
        self.assertLess(body.index("do_stat_object_prototype"), body.index("get_obj_vis"))
        self.assertIn('!strcmp(buf1, "mobile")', body)
        self.assertIn('is_abbrev(buf1, "object")', body)

    def test_mixed_tokens_are_not_numeric_vnums(self):
        parser = function_body("stat_vnum_token")
        self.assertIn("isdigit", parser)
        self.assertIn("return FALSE", parser)
        self.assertNotRegex("16400.mouse", r"^[0-9]+$")

    def test_clear_headings_and_missing_messages(self):
        for expected in ("Mobile prototype [%d]", "Object prototype [%d]",
                         "No mobile prototype exists with VNUM %d.",
                         "No object prototype exists with VNUM %d.",
                         "Live instance: none; displaying prototype data"):
            self.assertIn(expected, SOURCE)


if __name__ == "__main__":
    unittest.main()
