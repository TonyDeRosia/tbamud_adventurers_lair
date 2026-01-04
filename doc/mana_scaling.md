# Mana scaling design

The new mana scaling keeps each class's base mana progression intact and layers a smooth INT+WIS bonus on top of the saved `max_mana` value. Base mana still comes from level/class gains, trainers, and other effects that write to `GET_MAX_MANA`.

## Formula

```
over = max(0, s - 30)           # s = INT + WIS
bonus_pct = (over * 2.25) / (1 + (over / 20.0))
max_mana = round(base_mana * (1.0 + bonus_pct / 100.0))
```

- Baseline: `s = 30` → no bonus.
- At `INT = 20`, `WIS = 20` (`s = 40`), bonus is exactly **+15%**.
- Growth continues with diminishing returns; the denominator slows gains as `s` rises.
- Optional caps (off by default):
  - `MANA_BONUS_STAT_CAP` limits the `INT+WIS` sum fed into the curve.
  - `MANA_BONUS_PERCENT_CAP` clamps the resulting percent bonus.

## Example outputs

Assuming `base_mana = 100`:

| INT+WIS | Bonus % | Max mana |
|---------|---------|----------|
| 30      | 0%      | 100      |
| 32      | 4.1%    | 104      |
| 40      | 15.0%   | 115      |
| 50      | 22.5%   | 123      |

## Integration notes

- `GET_MAX_MANA` remains the authoritative base value saved to disk.
- `effective_max_mana(ch)` returns the live, scaled cap using the formula above.
- `clamp_mana_to_effective_max(ch)` keeps current mana within the scaled cap, ensuring existing characters benefit immediately when they log in or when their stats change.
