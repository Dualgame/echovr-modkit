#!/usr/bin/env bash
set -euo pipefail
REPO="${MAIN_MENU_A_ROOT:?run build_main_menu_a.py}"
BASE=$1; OUT=$2
case "$OUT" in */_data/*) echo "refused: OUT in the basis" >&2; exit 2;; esac
[ -d "$BASE" ] || { echo "refused: BASE $BASE missing" >&2; exit 2; }
[ "$(realpath -m "$BASE")" != "$(realpath -m "$OUT")" ] || { echo "refused: OUT == BASE" >&2; exit 2; }
cd "$REPO/lib"
rm -rf "$OUT"
SLAB=0x0bb67ae2a9f6b56c
python3 menu_ui/menu_layout.py mnu_master --base "$BASE" --out "$OUT" \
    --pose-rows $SLAB:0,-0.4751,5.959:0,1,0,0:0.001 > "$OUT.layout.log"
rm -f "$OUT.layout.log"
echo "built $OUT"
