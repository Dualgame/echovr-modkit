#!/usr/bin/env bash
set -euo pipefail
REPO="${MAIN_MENU_A_ROOT:?run build_main_menu_a.py}"
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
QBASE=$1; OUT=$2
[ -d "$QBASE" ] || { echo "refused: QBASE $QBASE missing" >&2; exit 2; }
[ ! -e "$OUT" ] || { echo "refused: $OUT exists" >&2; exit 2; }
case "$OUT" in */_data/*) echo "refused: $OUT" >&2; exit 2;; esac
cd "$REPO/lib"
python3 menu_ui/w23c/gen_ac_report.py --check >/dev/null || { echo "refused: w23c/ac_report.json is stale" >&2; exit 1; }
GP_CANVAS=0x51d88ca2bae7aff8; GP=0x8a5dcee389df5b7c; SM=0x6c1f6ff04e070923
ZONES=$(sed -n 's/^ZONES=(\(.*\)/\1/p; s/^       \(--add .*\)/\1/p' "$HERE/build.sh" | tr -d ')')
cp -a "$QBASE" "$OUT"
python3 menu_ui/canvas_edit.py --build q emit $GP_CANVAS --edits "$HERE/ac_report.json" --out "$OUT" --as ffa_ac_report > /dev/null
for L in r14_glb_global_mp r14_glb_global_mp_lowspec; do
  python3 menu_ui/ui_actor_mint.py components $L --owner $GP --donor-owner $SM $ZONES --canvas ffa_ac_report \
      --build q --base "$QBASE" --out "$OUT"
done
echo "Quest twin: $(find "$OUT" -type f | wc -l) files in $OUT"
