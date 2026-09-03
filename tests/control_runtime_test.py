"""Compile and run control regression against the actual linked engine objects."""
from pathlib import Path
import subprocess, tempfile, shutil, os
ROOT = Path(__file__).resolve().parents[1]
def test_linked_control_runtime():
    with tempfile.TemporaryDirectory(prefix="control-runtime-") as directory:
        tmp = Path(directory)
        flags = ["-std=gnu17", "-g", "-O1", "-fsanitize=address,undefined", "-fno-omit-frame-pointer", "-I" + str(ROOT / "src")]
        for name in ("comm", "control"):
            subprocess.run(["gcc", *flags, "-Dmain=mud_main", "-c", str(ROOT / "src" / (name + ".c")), "-o", str(tmp / (name + ".o"))], check=True)
        objects = [str(p) for p in (ROOT / "src").glob("*.o") if p.name not in ("comm.o", "control.o")]
        subprocess.run(["gcc", *flags, "-Wall", "-Werror", "-I" + str(ROOT / "src"), str(ROOT / "tests/control_runtime_test.c"), str(tmp / "comm.o"), str(tmp / "control.o"), *objects, "-lcrypt", "-lm", "-o", str(tmp / "test")], check=True)
        (tmp / "missing-test-config").write_text("")
        result = subprocess.run([str(tmp / "test")], cwd=tmp, env={**os.environ, "ASAN_OPTIONS": "detect_leaks=0", "UBSAN_OPTIONS": "halt_on_error=1"})
        if result.returncode and shutil.which("gdb"):
            subprocess.run(["gdb", "-batch", "-ex", "run", "-ex", "bt", str(tmp / "test")], cwd=tmp)
        assert result.returncode == 0
if __name__ == "__main__":
    test_linked_control_runtime()
