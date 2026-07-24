#!/usr/bin/env python3
"""Perform the supported GNU17 build and fail on compiler warnings."""
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[1]
flags = '-std=gnu17 -Wall -Wno-char-subscripts -Wno-unused-but-set-variable'
subprocess.run(['make', '-C', 'src', 'clean'], cwd=root, check=True)
result = subprocess.run(['make', '-C', 'src', f'MYFLAGS={flags}', '-j2'], cwd=root,
                        text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
(root / 'build-warnings.log').write_text(result.stdout)
assert result.returncode == 0, result.stdout
assert 'warning:' not in result.stdout, result.stdout
print('GNU17 build completed without compiler warnings')
