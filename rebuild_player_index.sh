#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"

OUT="lib/plrfiles/index"
tmp="$(mktemp)"
trap 'rm -f "$tmp"' EXIT

shopt -s nullglob

# Build lines as: id name level flags last
for f in lib/plrfiles/*/*.plr; do
  [[ -f "$f" ]] || continue

  name="$(awk -F': *' '$1=="Name"{print $2; exit}' "$f" | tr -d '\r' | tr 'A-Z' 'a-z')"
  id="$(awk -F': *' '$1=="Id  "{print $2; exit}' "$f" | tr -d '\r')"
  lvl="$(awk -F': *' '$1=="Levl"{print $2; exit}' "$f" | tr -d '\r')"
  last="$(awk -F': *' '$1=="Last"{print $2; exit}' "$f" | tr -d '\r')"

  [[ -n "${name:-}" && -n "${id:-}" ]] || continue
  [[ "${id}" =~ ^[0-9]+$ ]] || continue

  # Defaults if missing
  [[ "${lvl:-}" =~ ^[0-9]+$ ]] || lvl=0
  [[ "${last:-}" =~ ^[0-9]+$ ]] || last=0

  # flags field: safe default "0"
  printf "%s %s %s %s %s\n" "$id" "$name" "$lvl" "0" "$last" >> "$tmp"
done

# Sort by name so duplicates are visible, then keep last occurrence per name
sort -k2,2 "$tmp" | awk '
  {
    key=$2
    line[key]=$0
  }
  END {
    for (k in line) print line[k]
  }
' | sort -n -k1,1 > "$OUT"

echo "~" >> "$OUT"

echo "Wrote $OUT"
echo "Last 10 lines:"
tail -n 10 "$OUT"
