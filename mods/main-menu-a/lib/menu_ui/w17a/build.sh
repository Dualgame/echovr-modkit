#!/usr/bin/env bash
set -euo pipefail
REPO="${MAIN_MENU_A_ROOT:?run build_main_menu_a.py}"
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BASE=$1; OUT=$2; QOUT=${3:-}
for d in "$OUT" "$QOUT"; do case "$d" in */_data/*) echo "refused: $d" >&2; exit 2;; esac; done
[ -d "$BASE" ] || { echo "refused: BASE $BASE missing" >&2; exit 2; }
[ "$(realpath -m "$BASE")" != "$(realpath -m "$OUT")" ] || { echo "refused: OUT == BASE" >&2; exit 2; }
WORK="$OUT.work"
cd "$REPO/lib"
rm -rf "$OUT" "$WORK"; mkdir -p "$WORK"
CARD=0xa9bf5a844a019d97; OWNER=0x61c4b2b9153a12b7
BTN=(--move button1@-0.05013,0.02560,0.016 --move button2@0.00034,0.02560,0.016
     --move button3@-0.02910,-0.04429,0.016 --move button4@0.03478,-0.04429,0.016
     --add button4=button5@0.05081,0.02560,0.016 --canvas ffa_social_card5)

python3 menu_ui/canvas_edit.py emit $CARD --edits "$HERE/five_tile.json" --out "$WORK/new" --as ffa_social_card5 > "$WORK/canvas.log"
python3 menu_ui/ui_actor_mint.py components r14_glb_global_mp --owner $OWNER "${BTN[@]}" --out "$WORK/new" > "$WORK/glb.log"
(cd "$WORK/new" && find . -type f) | while read -r f; do
  [ ! -e "$BASE/$f" ] || { echo "refused: BASE already has $f" >&2; exit 1; }
done
cp -a "$BASE" "$OUT"
cp -a "$WORK/new/." "$OUT/"
echo "PC: $(cd "$WORK/new" && find . -type f | wc -l) files added to $OUT"

if [ -n "$QOUT" ]; then
  rm -rf "$QOUT"; mkdir -p "$QOUT"
  python3 menu_ui/canvas_edit.py --build q emit $CARD --edits "$HERE/five_tile.json" --out "$QOUT" --as ffa_social_card5 > "$WORK/qcanvas.log"
  for L in r14_glb_global_mp r14_glb_global_mp_lowspec; do
    python3 menu_ui/ui_actor_mint.py components $L --owner $OWNER --build q "${BTN[@]}" --out "$QOUT" >> "$WORK/qglb.log"
  done
  echo "Quest: $(find "$QOUT" -type f | wc -l) files in $QOUT (stage with quest_stage.py build --tree, beside the menu tree)"
fi
rm -rf "$WORK"
