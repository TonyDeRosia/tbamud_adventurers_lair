"""Regression contracts for account-bound player persistence.

The test uses an isolated directory for the filesystem behavior and inspects
the C integration points so repository fixture data is never touched.
"""
from pathlib import Path
import tempfile

ROOT = Path(__file__).resolve().parents[1]
PLAYERS = (ROOT / "src/players.c").read_text()
NANNY = (ROOT / "src/interpreter.c").read_text()
DB = (ROOT / "src/db.c").read_text()
ACCOUNTS = (ROOT / "src/accounts.c").read_text()


def test_hashed_player_storage_is_created_in_runtime_root():
    with tempfile.TemporaryDirectory() as root:
        runtime = Path(root) / "plrfiles"
        for bucket in ("A-E", "F-J", "K-O", "P-T", "U-Z", "ZZZ"):
            (runtime / bucket).mkdir(parents=True, exist_ok=True)
        save_path = runtime / "K-O" / "kokudar.plr"
        load_path = runtime / "K-O" / "kokudar.plr"
        save_path.write_text("Name: Kokudar\nLevl: 104\n")
        assert save_path == load_path and load_path.read_text().endswith("Levl: 104\n")
    assert 'LIB_PLRFILES "K-O"' in PLAYERS


def test_core_player_and_index_writes_are_checked_and_durable():
    assert "int save_char(" in PLAYERS
    assert "int save_player_index(" in PLAYERS
    assert PLAYERS.count("fsync(fileno(") >= 2
    assert "if (!save_char(d->character) || !save_player_index())" in NANNY
    assert "if (!account_attach_char(d->character))" in NANNY


def test_roster_selection_is_existing_only_and_never_falls_into_creation():
    assert NANNY.count("d->acct_roster_load_only = 1") == 2
    assert "if (d->acct_roster_load_only)" in NANNY
    assert "The account entry has been preserved" in NANNY
    failure = NANNY.index("Account roster load failed")
    creation = NANNY.index("/* player unknown -- make new character */")
    assert failure < creation
    assert "return;" in NANNY[failure:creation]


def test_roster_validation_excludes_the_loading_descriptor_and_diagnoses_conflicts():
    assert "get_ptable_by_name(tmp_name)" in NANNY
    assert "GET_ACCOUNT_ID(d->character) != d->acct_id" in NANNY
    assert "account_character_in_use_elsewhere(tmp_name, d)" in NANNY
    for field in (
        "load-result=", "expected-account=", "loaded-account=",
        "in-use-elsewhere=", "conflicting-descriptor=",
        "conflicting-state=", "self-descriptor-excluded=yes",
    ):
        assert field in NANNY


def test_in_use_elsewhere_contract_covers_self_duplicates_menu_and_switching():
    helper = ACCOUNTS[
        ACCOUNTS.index("account_character_in_use_elsewhere("):
        ACCOUNTS.index("int account_character_is_in_use(")
    ]
    assert "d == current_desc" in helper  # freshly loaded character is self
    assert "STATE(d) == CON_CLOSE || STATE(d) == CON_DISCONNECT" in helper
    assert "d->character && GET_NAME(d->character)" in helper
    assert "d->original && GET_NAME(d->original)" in helper
    assert helper.count("!str_cmp(") == 2  # case-insensitive comparison
    assert "return d;" in helper  # another descriptor is reported for diagnostics

    # The no-current-descriptor wrapper preserves deletion protection, while
    # roster selection uses the excluding form after load_char populated D.
    assert "account_character_in_use_elsewhere(name, NULL) != NULL" in ACCOUNTS
    load = NANNY.index("load_char(tmp_name, d->character)")
    validate = NANNY.index("account_character_in_use_elsewhere(tmp_name, d)")
    account_match = NANNY.index("GET_ACCOUNT_ID(d->character) != d->acct_id", load)
    assert load < validate < account_match


def test_roster_failure_paths_still_return_to_account_menu_not_creation():
    rejection = NANNY.index("GET_ACCOUNT_ID(d->character) != d->acct_id")
    missing = NANNY.index("Account roster load failed")
    creation = NANNY.index("/* player unknown -- make new character */")
    assert rejection < missing < creation
    assert NANNY[rejection:creation].count("STATE(d) = CON_ACCT_MENU") >= 2
    assert NANNY[rejection:creation].count("return;") >= 2
    first_return = NANNY.index("return;", rejection)
    assert first_return < NANNY.index("save_char(d->character)", rejection)


def test_first_player_semantics_are_not_top_zero_alone():
    assert "top_of_p_table != 0 || pfilepos != 0" in PLAYERS
    assert '"." SUF_PLR' in PLAYERS  # orphan files prevent accidental promotion
    assert "first_player_record_is_new(GET_PFILEPOS(ch))" in DB
    assert "top_of_p_table == 0)" not in DB[DB.index("void init_char"):DB.index("void init_char") + 900]


def test_free_player_index_restores_empty_sentinel_for_restart_tests():
    assert "top_of_p_table = -1;" in PLAYERS[PLAYERS.index("void free_player_index"):]


def test_save_and_load_share_get_filename_player_path():
    assert PLAYERS.count("get_filename(filename, sizeof(filename), PLR_FILE") >= 2
    assert 'suffix = SUF_PLR' in (ROOT / "src/utils.c").read_text()
