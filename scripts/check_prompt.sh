#!/usr/bin/env bash
# Diagnostic helper to ensure the legacy prompt handler is gone and the
# modern template-based prompt is what ships. Run from the repository
# root.

set -euo pipefail

REPO_ROOT=$(pwd)
BIN_PATH="$REPO_ROOT/bin/circle"
LEGACY_USAGE="Usage: prompt { { H | M | V } | all | auto | none }"
MODERN_USAGE="Usage: prompt <template>"
DEFAULT_PROMPT="[%h / %H] [%m / %M] [%v / %V] [%tnl]"

step() {
  printf "\n=== %s ===\n" "$1"
}

step "1) Prove what binary is running"
if pgrep -a circle >/dev/null 2>&1; then
  pgrep -a circle
  latest_pid=$(pgrep -n circle)
  if [[ -n "$latest_pid" && -e "/proc/$latest_pid/exe" ]]; then
    echo "Resolved binary for PID $latest_pid:"
    readlink -f "/proc/$latest_pid/exe"
  else
    echo "Unable to resolve running binary path."
  fi
else
  echo "No running circle process detected."
fi

step "2) Prove what text is inside the compiled binary"
if [[ -x "$BIN_PATH" ]]; then
  if strings "$BIN_PATH" | grep -F "$LEGACY_USAGE" >/dev/null; then
    echo "Legacy usage string still present in binary." \
      "(Binary: $BIN_PATH)"
  else
    echo "OK: legacy usage string not in this binary."
  fi

  if strings "$BIN_PATH" | grep -F "$MODERN_USAGE" >/dev/null; then
    echo "OK: modern prompt usage text detected."
  else
    echo "Modern prompt usage text not found in binary." \
      "(Expected: '$MODERN_USAGE')"
  fi

  if strings "$BIN_PATH" | grep -F "$DEFAULT_PROMPT" >/dev/null; then
    echo "OK: default prompt template detected."
  else
    echo "Default prompt template not found in binary." \
      "(Expected: '$DEFAULT_PROMPT')"
  fi
else
  echo "Binary $BIN_PATH not found or not executable; build before running this check."
fi

step "3) Find where that legacy usage string still lives in source"
if rg -nF "$LEGACY_USAGE" src; then
  :
else
  echo "Legacy usage string not found in src tree."
fi

step "4) Confirm the command routing is correct in source"
rg -n '^\s*\{\s*"prompt"' src/interpreter.c || echo "Prompt command mapping not found."
rg -n '^\s*\{\s*"display"' src/interpreter.c || echo "Display command mapping not found."
rg -n "do_prompt|do_display" src/interpreter.c || echo "Command handler references not found."

step "Next steps"
echo "If the legacy usage string is present in the binary, clean and rebuild, then restart the correct process."
