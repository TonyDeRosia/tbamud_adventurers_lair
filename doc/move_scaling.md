# Movement scaling design

The movement bonus preserves each class's base `max_move` progression while layering a smooth DEX+CHA curve on top of the saved value. Base move still comes from class/level gains, trainers, and other effects that write to `GET_MAX_MOVE`.

## Formula

```
over = max(0, s - 30)           # s = DEX + CHA
bonus_pct = (over * 2.25) / (1 + (over / 20.0))
max_moves = round(base_moves * (1.0 + bonus_pct / 100.0))
```

- Baseline: `s = 30` → no bonus.
- At `DEX = 20`, `CHA = 20` (`s = 40`), bonus is exactly **+15%**.
- Growth continues with diminishing returns because the denominator gradually dampens later gains.
- Optional caps (off by default):
  - `MOVE_BONUS_STAT_CAP` limits the `DEX+CHA` sum fed into the curve.
  - `MOVE_BONUS_PERCENT_CAP` clamps the resulting percent bonus.

## Example outputs

Assuming `base_moves = 100`:

| DEX+CHA | Bonus % | Max moves |
|---------|---------|-----------|
| 30      | 0%      | 100       |
| 32      | 4.1%    | 104       |
| 40      | 15.0%   | 115       |
| 50      | 22.5%   | 123       |

## Integration notes

- `effective_max_move(ch)` returns the live, scaled cap using the formula above without altering the stored base value.
- `clamp_move_to_effective_max(ch)` keeps current move points within the scaled cap so existing characters benefit as soon as they log in or their stats change.
- Tuning is isolated to the constants in `utils.h`, keeping class identity intact while allowing easy adjustments.
