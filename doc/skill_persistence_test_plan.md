# Skill and spell proficiency persistence regression test plan

## Manual test steps

1. Log in on a low-level character with a trainer available.
2. Practice a spell (for example, corruption) until it rises above 1% (aim for 75%).
3. Practice a skill (for example, kick) until it rises above 1% (aim for 36%).
4. Run `save`, then `quit`.
5. Stop the server cleanly, restart it, and log back in.
6. Run `practice` to list abilities.
7. Confirm the practiced values remain at the saved percentages (corruption still 75%, kick still 36%).
