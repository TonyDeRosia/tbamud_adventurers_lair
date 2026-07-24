#!/usr/bin/env python3
"""Validate world reset references against the prototypes in this checkout."""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
WORLD = ROOT / 'lib' / 'world'
VNUM = re.compile(r'^#(\d+)$', re.M)
COMMAND = re.compile(r'^([MOGEPDR])\s+\d+\s+(-?\d+)\s+(-?\d+)\s+(-?\d+)', re.M)

def prototypes(kind):
    values = set()
    for path in (WORLD / kind).glob(f'*.{kind}'):
        values.update(map(int, VNUM.findall(path.read_text(errors='replace'))))
    return values

def main():
    mobs, objs, rooms = prototypes('mob'), prototypes('obj'), prototypes('wld')
    errors = []
    position_17 = 0
    for path in (WORLD / 'zon').glob('*.zon'):
        for line, match in enumerate(COMMAND.finditer(path.read_text(errors='replace')), 1):
            command, a, _b, c = match.group(1), int(match.group(2)), int(match.group(3)), int(match.group(4))
            if command == 'M' and (a not in mobs or c not in rooms):
                errors.append(f'{path}:{line}: M requires existing mob {a} and room {c}')
            elif command in 'GE' and a not in objs:
                errors.append(f'{path}:{line}: {command} requires existing object {a}')
            elif command == 'E':
                if not 0 <= c < 18:
                    errors.append(f'{path}:{line}: E position {c} is outside 0..17')
                position_17 += c == 17
            elif command == 'O' and (a not in objs or (c != -1 and c not in rooms)):
                errors.append(f'{path}:{line}: O has invalid object/room reference')
    assert position_17 == 511, position_17
    assert not errors, '\n'.join(errors)
    print(f'validated {position_17} legacy hold-slot resets and all reset references')

if __name__ == '__main__':
    main()
