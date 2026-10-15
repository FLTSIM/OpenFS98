#!/usr/bin/env bash
# Pull the FS98 code modules out of the install CD into orig/.
# Only code, and ENGLISH.FLL for its string table, is extracted; scenery,
# sound and textures stay on the ISO.
set -euo pipefail
ISO="${1:-$HOME/Downloads/MSFlightSimulator98.iso}"
OUT="$(cd "$(dirname "$0")/.." && pwd)/orig"

[ -f "$ISO" ] || { echo "no such ISO: $ISO" >&2; exit 1; }
mkdir -p "$OUT"
tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT

7z x -y -o"$tmp" "$ISO" \
   'fs98/*.EXE' 'fs98/*.exe' 'fs98/*.DLL' 'fs98/*.FLL' \
   'fs98/Modules/*' 'fs98/English/*' 'fs98/Gauges/*' >/dev/null

cp -r "$tmp"/fs98/. "$OUT"/
find "$OUT" -type f -exec chmod 644 {} +
echo "extracted to $OUT:"
find "$OUT" -type f | wc -l | xargs echo "  files:"
du -sh "$OUT" | cut -f1 | xargs echo "  size: "
