#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")"

say() { printf "\n%s\n" "$*"; }
hr()  { printf "%s\n" "============================================================"; }

hr
say "TBAMUD LOGIN DIAGNOSTICS: index + pfiles + runtime paths"
say "Run time: $(date)"
say "Repo: $(pwd)"
hr

say "1) Git state"
git rev-parse --abbrev-ref HEAD || true
git rev-parse --short HEAD || true
git status -sb || true

say "2) Running server process and working dir"
pid="$(pgrep -f 'bin/circle' | head -n1 || true)"
if [[ -z "${pid:-}" ]]; then
  echo "No running bin/circle process found."
else
  echo "PID=$pid"
  echo "cwd: $(readlink -f "/proc/$pid/cwd" || true)"
  echo "exe: $(readlink -f "/proc/$pid/exe" || true)"
  echo "cmdline:"
  tr '\0' ' ' < "/proc/$pid/cmdline" || true
  echo
  echo "env (filtered):"
  tr '\0' '\n' < "/proc/$pid/environ" | egrep '^(PWD|CIRCLE|LIB|TBADIR|HOME)=' || true
fi

say "3) Confirm lib tree exists, plrfiles exists, and is writable"
ls -ld lib lib/plrfiles 2>/dev/null || true
for d in A-E F-J K-O P-T U-Z; do
  [[ -d "lib/plrfiles/$d" ]] && ls -ld "lib/plrfiles/$d" || echo "missing dir: lib/plrfiles/$d"
done
if [[ -d lib/plrfiles ]]; then
  if touch lib/plrfiles/.write_test 2>/dev/null; then
    echo "write test: OK"
    rm -f lib/plrfiles/.write_test
  else
    echo "write test: FAILED (permission or filesystem issue)"
  fi
fi

say "4) Player index presence and format check"
idx="lib/plrfiles/index"
if [[ -f "$idx" ]]; then
  echo "index exists: $idx"
  echo "size: $(wc -c < "$idx") bytes"
  echo "ends with ~ line:"
  tail -n 3 "$idx" || true

  echo
  echo "first 10 lines:"
  head -n 10 "$idx" || true

  echo
  echo "check for malformed lines (should be either '~' or: id name level flags last)"
  bad=0
  lineno=0
  while IFS= read -r line; do
    lineno=$((lineno+1))
    [[ "$line" == "~" ]] && continue
    [[ -z "$line" ]] && continue

    # expect: number + space + lowercase-name + space + number + space + flags + space + number
    if ! echo "$line" | egrep -q '^[0-9]+[[:space:]]+[a-z0-9_]+[[:space:]]+[0-9]+[[:space:]]+[^[:space:]]+[[:space:]]+[0-9]+$'; then
      printf "BAD LINE %d: %s\n" "$lineno" "$line"
      bad=$((bad+1))
      [[ $bad -ge 25 ]] && break
    fi
  done < "$idx"
  echo "bad lines found: $bad"
else
  echo "index missing: $idx"
  echo "this causes: No player index file! First new char will be IMP!"
fi

say "5) Detect duplicate LIB_PLRFILES defines and what the compiler uses"
echo "grep of db.h for LIB_PLRFILES:"
grep -n '^[[:space:]]*#define[[:space:]]\+LIB_PLRFILES' -n src/db.h || true

echo
echo "preprocessor output (LIB_PLRFILES actually used when compiling players.c):"
if command -v gcc >/dev/null 2>&1; then
  gcc -E -dM -I./src src/players.c 2>/dev/null | grep 'LIB_PLRFILES' || true
else
  echo "gcc not found"
fi

say "6) Sample pfile parse (Name, Id, Levl, Last) from a few files"
count=0
shopt -s nullglob
for f in lib/plrfiles/*/*.plr; do
  [[ -f "$f" ]] || continue
  echo
  echo "FILE: $f"
  egrep -n '^(Name|Id  |Levl|Last|Host|Pass):' "$f" | head -n 20 || true
  count=$((count+1))
  [[ $count -ge 8 ]] && break
done
[[ $count -eq 0 ]] && echo "No .plr files found under lib/plrfiles/*/*.plr"

say "7) Cross check: do pfile names exist in index"
if [[ -f "$idx" ]]; then
  miss=0
  check=0
  for f in lib/plrfiles/*/*.plr; do
    [[ -f "$f" ]] || continue
    name="$(awk -F': *' '$1=="Name"{print $2; exit}' "$f" | tr -d '\r' | tr 'A-Z' 'a-z')"
    [[ -n "${name:-}" ]] || continue
    check=$((check+1))
    if ! egrep -qi "^[0-9]+[[:space:]]+${name}[[:space:]]" "$idx"; then
      echo "MISSING IN INDEX: $name  (from $f)"
      miss=$((miss+1))
      [[ $miss -ge 25 ]] && break
    fi
  done
  echo
  echo "checked pfiles: $check"
  echo "missing from index: $miss"
fi

say "8) Look for recent log lines related to player index / login"
# Try common log locations used in tba/circle setups
for logf in syslog lib/log/syslog syslog.CRASH lib/syslog; do
  if [[ -f "$logf" ]]; then
    echo
    echo "LOG FILE: $logf (last 80 lines mentioning player/index/name/new char)"
    tail -n 500 "$logf" | egrep -in 'player index|No player index|new char|By what name|load_char|get_ptable|pfile|plrfiles|index file|IMP' | tail -n 80 || true
  fi
done

say "9) Sanity: where does admin.plr live and does get_filename path pattern match?"
adminf="$(ls -1 lib/plrfiles/*/admin.plr 2>/dev/null | head -n1 || true)"
if [[ -n "${adminf:-}" ]]; then
  echo "admin.plr found at: $adminf"
else
  echo "admin.plr not found under lib/plrfiles/*/"
fi

say "10) Summary hints"
echo "If index is missing or malformed, the game thinks no players exist and prompts for new char."
echo "If LIB_PLRFILES macro points somewhere else, the game reads/writes a different index than you edit."
echo "If index exists but missing a name, login will act like new char for that name."

hr
say "DONE"
