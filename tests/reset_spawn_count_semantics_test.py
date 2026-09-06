"""Compile the actual reset_zone() implementation with deterministic wrappers."""
from pathlib import Path
import subprocess, tempfile

ROOT = Path(__file__).resolve().parents[1]

def function(file, signature):
    s = (ROOT / "src" / file).read_text()
    a = s.index(signature + "\n{")
    brace = s.index("{", a)
    depth = 0
    i = brace
    in_string = False
    in_char = False
    escape = False
    line_comment = False
    block_comment = False

    while i < len(s):
        c = s[i]
        n = s[i + 1] if i + 1 < len(s) else ""

        if line_comment:
            if c == "\n":
                line_comment = False
            i += 1
            continue

        if block_comment:
            if c == "*" and n == "/":
                block_comment = False
                i += 2
                continue
            i += 1
            continue

        if in_string:
            if escape:
                escape = False
            elif c == "\\":
                escape = True
            elif c == '"':
                in_string = False
            i += 1
            continue

        if in_char:
            if escape:
                escape = False
            elif c == "\\":
                escape = True
            elif c == "'":
                in_char = False
            i += 1
            continue

        if c == "/" and n == "/":
            line_comment = True
            i += 2
            continue
        if c == "/" and n == "*":
            block_comment = True
            i += 2
            continue
        if c == '"':
            in_string = True
            i += 1
            continue
        if c == "'":
            in_char = True
            i += 1
            continue

        if c == "{":
            depth += 1
        elif c == "}":
            depth -= 1
            if depth == 0:
                return s[a:i + 1]

        i += 1

    raise RuntimeError(f"Unbalanced braces while extracting {signature}")

with tempfile.TemporaryDirectory() as td:
    tmp = Path(td)
    generated = (
        "#define Z zone_table[zone]\n"
        "#define ZCMD zone_table[zone].cmd[cmd_no]\n"
        "#define ZONE_ERROR(message) do { last_cmd = 0; } while (0)\n"
    )
    generated += function("db.c", "static int count_custom_reset_mobs(zone_vnum zone_vnum_value, room_vnum room_vnum_value,\n                                   mob_vnum mob_vnum_value)") + "\n"
    generated += function("db.c", "static int count_custom_reset_objs(zone_vnum zone_vnum_value, room_vnum room_vnum_value,\n                                   obj_vnum obj_vnum_value)") + "\n"

    for name in [
        "read_mobile", "read_object", "char_to_room", "obj_to_room", "obj_to_char",
        "equip_char", "load_mtrigger", "load_otrigger", "wear_otrigger",
        "reset_wtrigger", "rand_number"
    ]:
        generated += f"#define {name} test_{name}\n"

    generated += function("db.c", "void reset_zone(zone_rnum zone)").replace(
        "void reset_zone(", "void test_reset_zone("
    ) + "\n"

    for name in [
        "read_mobile", "read_object", "char_to_room", "obj_to_room", "obj_to_char",
        "equip_char", "load_mtrigger", "load_otrigger", "wear_otrigger",
        "reset_wtrigger", "rand_number"
    ]:
        generated += f"#undef {name}\n"

    (tmp / "engine.inc").write_text(generated)

    flags = [
        "-std=gnu17", "-g", "-O0",
        "-I" + str(ROOT / "src"),
        "-I" + str(tmp)
    ]

    subprocess.run(
        ["gcc", *flags, "-Dmain=mud_main", "-c",
         str(ROOT / "src/comm.c"), "-o", str(tmp / "comm.o")],
        check=True
    )

    objects = [
        str(p) for p in (ROOT / "src").glob("*.o")
        if p.name != "comm.o"
    ]

    subprocess.run(
        ["gcc", *flags,
         str(ROOT / "tests/reset_spawn_count_semantics_test.c"),
         str(tmp / "comm.o"), *objects, "-lcrypt", "-lm",
         "-o", str(tmp / "test")],
        check=True
    )
    subprocess.run([str(tmp / "test")], check=True)