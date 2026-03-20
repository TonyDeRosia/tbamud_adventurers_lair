# FOOD value layout update (2026-03-20)

FOOD objects now use all four value slots in this order:

- `value[0]` = hunger restore
- `value[1]` = thirst restore
- `value[2]` = sated duration (ticks of delayed hunger decay)
- `value[3]` = poison flag (`0` = no, `1` = yes)

## Builder compatibility note

Older FOOD prototypes commonly used only:

- `value[0]` = stomach fill amount
- `value[3]` = poison flag

with `value[1]` and `value[2]` usually `0`.

Those objects still work, but they will now be interpreted as:

- hunger restore = previous fill amount
- thirst restore = previous `value[1]` (typically `0`)
- sated duration = previous `value[2]` (typically `0`)
- poison = previous `value[3]`

Builders should update legacy FOOD entries if they want thirst restore and/or sated duration behavior.
