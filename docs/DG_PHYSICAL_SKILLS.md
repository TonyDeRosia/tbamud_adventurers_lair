# DG physical skills and per-mob cooldowns

Mob triggers may invoke only `backstab`, `bash`, and `kick` through
`dg_skill <skill> <target>`. The command sets `dg_skill_result` to
`ATTEMPTED` when a legal action (including a miss) occurred, otherwise to
`NOT_ATTEMPTED`. NPC proficiency is `min(90, 30 + level)`. These skills use
the same weapon, target, roll, damage, knockdown, stamina, peaceful-room, and
wait-state mechanics as their player commands. Magic remains under
`dg_cast 'spell name' <target>`.

Cooldowns belong to one runtime mob instance and are keyed by a stable player
ID plus a 1-32 character letters/digits/underscore key. Durations are 1-604800
seconds, with at most 256 entries per mob. They are runtime-only and freed
with the mob.

## Greet: chance, backstab, then cooldown

```
if %random.100% <= 25
  dg_cooldown_check %actor% ambush
  if %dg_cooldown_result% == READY
    dg_skill backstab %actor%
    if %dg_skill_result% == ATTEMPTED
      dg_cooldown_set %actor% ambush 300
    end
  end
end
```

## Fight: occasional kick

```
if %random.100% <= 20
  dg_skill kick %actor%
end
```

## Physical versus magic

```
dg_skill bash %actor%
dg_cast 'magic missile' %actor%
```
