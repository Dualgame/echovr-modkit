#!/usr/bin/env bash
set -euo pipefail
REPO="${MAIN_MENU_A_ROOT:?run build_main_menu_a.py}"
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
OUT=$1
[ ! -e "$OUT" ] || { echo "refused: $OUT exists" >&2; exit 2; }
cd "$REPO/lib"
CARD=0xa9bf5a844a019d97; OWNER=0x61c4b2b9153a12b7
BTN=(--move button1@-0.05013,0.02560,0.016 --move button2@0.00034,0.02560,0.016
     --move button3@-0.02910,-0.04429,0.016 --move button4@0.03478,-0.04429,0.016
     --add button4=button5@0.05081,0.02560,0.016 --canvas ffa_social_card5)
mkdir -p "$OUT"
python3 menu_ui/canvas_edit.py --build q emit $CARD --edits "$HERE/five_tile.json" --out "$OUT" --as ffa_social_card5 > /dev/null
for L in r14_glb_global_mp r14_glb_global_mp_lowspec; do
  python3 menu_ui/ui_actor_mint.py components $L --owner $OWNER --build q "${BTN[@]}" --out "$OUT" > /dev/null
done
echo "Quest twin: $(find "$OUT" -type f | wc -l) files in $OUT"
