# BuilderBot graceful logout contract

Adventurer's Lair keeps a logged-in connection open after the in-game
`quit` command. This is intentional: quitting extracts and saves the active
character, then returns the descriptor to the character menu. A client must
therefore complete logout as a two-step exchange rather than treating the
first goodbye message as a closed connection.

## Expected exchange

1. While playing (`CON_PLAYING`), send the complete command `quit` once.
2. Wait until the configured character menu prompt is received. The server
   has now saved and extracted the character and entered `CON_MENU`.
3. Send the menu choice `0` once.
4. Wait for `Goodbye.` from the menu handler.
5. Wait for the server to close the connection (`CON_CLOSE`).

The first message, `Goodbye, friend.. Come back soon!`, is an in-game quit
acknowledgement, not the end of the network session. Likewise, seeing the
character menu during this exchange must not be interpreted as returning to
the ready-to-play state.

## Client safety requirements

Automation should obtain `quit`, `0`, and the prompt matchers from a trusted,
validated connection profile. If the exchange times out, the client may close
its socket locally. A client should also close locally—without sending logout
commands—when login has failed, an editor is active, or another command must
be interrupted. Repeated local close requests should remain harmless.

Diagnostics for this exchange must redact passwords and account or character
identifiers. Desktop clients should perform the wait off their UI thread and
use a bounded wait followed by local socket closure during application
shutdown.

## Server implementation points

The server-side sequence is split across two handlers:

* `do_quit` in `src/act.other.c` saves the character and calls
  `extract_char`.
* `extract_char` in `src/handler.c` changes an attached descriptor to
  `CON_MENU` and writes `CONFIG_MENU`.
* The `CON_MENU` branch in `src/interpreter.c` handles `0`, writes
  `Goodbye.`, and changes the descriptor to `CON_CLOSE`.

`tests/logout_contract_test.py` protects these integration points against
accidental protocol drift. It is a source-level regression contract; a real
MUD connection is not required.
