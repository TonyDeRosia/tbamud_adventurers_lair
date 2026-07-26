# Account Character Persistence Audit

## Root cause and reproduction

The account bootstrap created only `plrfiles/` and `plrfiles/accounts/`. Player
files are hashed by `get_filename()` into `plrfiles/A-E`, `F-J`, `K-O`, `P-T`,
`U-Z`, or `ZZZ`; a clean runtime tree had none of those directories. Thus
`save_char(Kokudar)` attempted `plrfiles/K-O/kokudar.plr`, `fopen()` failed, and
the old `void` API merely logged and returned. Creation continued, wrote the
separate player index and account file, and later attached Kokudar to the
account. After restart the roster therefore survived independently, while
`load_char()` either found no index record or could not open the absent pfile.
The generic name path interpreted that failure as an unused name and entered
`CON_NAME_CNFRM`, permitting destructive recreation.

This was reproduced from a temporary clean runtime layout: creating only the
account storage makes `plrfiles/K-O/kokudar.plr` fail with `ENOENT`, while the
account file and top-level player index remain writable.

## Lifecycle and paths

The creation path is `create_entry()` (in-memory table insertion),
`init_char()`, final stat allocation, `save_char()`, `save_player_index()`, then
`account_attach_char()` / `account_save_any()`. The production executable
changes directory to the `-d` data directory (normally `lib`), so all player,
alias, variable, object, and account constants resolve relative to that single
runtime root. Player core data uses `plrfiles/<bucket>/<lower-name>.plr`; the
index is `plrfiles/index`; accounts use `plrfiles/accounts/<id>.acct` and
`plrfiles/accounts/index.txt`. Aliases are embedded in the core pfile; script
variables use `plrvars/<bucket>/<name>.mem`; objects use
`plrobjs/<bucket>/<name>.objs`; text uses `plrtext/<bucket>/<name>.text`.
Save and load both call `get_filename()`, hence use identical case folding,
bucket selection, suffix, and current runtime root.

Boot now creates every player bucket before reading the index and reports the
resolved player root/index plus record count. Save/load failures include the
resolved filename and system error; roster failures log index result, expected
and loaded account IDs, and in-use state. No credentials are logged.

## Safety changes

`save_char()`, `save_player_index()`, `account_save_any()`, and
`account_attach_char()` now report success. Core files and indexes are flushed
and `fsync()`ed. Character creation attaches the account only after durable
core player and index writes; an attachment failure leaves the player file for
administrator recovery and does not admit the character to play.

A roster slot/name sets explicit existing-only intent. A missing index, missing
file, unreadable/corrupt load, account-ID mismatch, or in-use character returns
to `CON_ACCT_MENU` with the roster unchanged. It never reaches confirmation,
sex, race, stats, `init_char()`, `create_entry()`, or replacement saving.

## Player-table and first implementor semantics

`top_of_p_table` is the last occupied zero-based index: `-1` means empty, `0`
means exactly one record. Because `init_char()` runs after `create_entry()`, the
genuine first insertion is `(top == 0 && pfilepos == 0)`. The new helper also
scans for orphan `.plr` files, preventing a missing/corrupt index from granting
level 104. A loaded existing character never calls `init_char()`, and its
serialized `Levl: 104` is restored normally.

## Verification

`tests/account_character_persistence_test.py` uses temporary directories and
checks hashed path identity, durable checked writes, safe roster routing,
validation diagnostics, restart empty-sentinel semantics, and robust one-time
implementor detection. A full build validates all changed public signatures.

Manual protocol: start production `bin/circle -d lib`, create account `admin`
and `Kokudar`, finish race/stats, enter and verify level 104; quit; verify the
four core/index/account artifacts; stop and restart with the same command;
select slot 1 and verify direct existing-character login and level 104; repeat
the restart. After a completed durable save, an abrupt termination cannot lose
the fsynced core file. Interactive network execution is environment-dependent;
the automated isolated persistence regression supplies the repeatable check.

## Remaining limitations

The writes are durable but not a multi-file filesystem transaction: power loss
between the player/index save and account attachment can leave a recoverable
orphan player. The code deliberately retains and logs that data rather than
deleting or overwriting it. Existing pre-fix broken rosters are preserved for
administrator repair.
