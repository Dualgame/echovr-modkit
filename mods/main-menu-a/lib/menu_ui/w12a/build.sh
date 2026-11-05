#!/usr/bin/env bash
set -euo pipefail
REPO="${MAIN_MENU_A_ROOT:?run build_main_menu_a.py}"
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BASE=$1; OUT=$2
case "$OUT" in */_data/*) echo "refused: OUT in the basis" >&2; exit 2;; esac
[ -d "$BASE" ] || { echo "refused: BASE $BASE missing" >&2; exit 2; }
[ "$(realpath -m "$BASE")" != "$(realpath -m "$OUT")" ] || { echo "refused: OUT == BASE" >&2; exit 2; }
WORK="$OUT.work"
cd "$REPO/lib"
rm -rf "$OUT" "$WORK"; mkdir -p "$WORK"
D=primary/59a9bd6e4525ecc4/v6500253412
M=menu_ui
FIELD=0x94d77fe1ca3e487f
Q=0,0.9989180564880371,0.04650497809052467,0

python3 $M/canvas_edit.py emit ui_mnu_deeplinking_invite_code --edits $M/invite_row/ops/invite_field_w12a.json \
    --out "$WORK/canv" --as ffa_invite_code_field >/dev/null
python3 $M/canvas_edit.py emit ui_mnu_settings_invite_regenerate --edits $M/friends_data/ops/friend_row_w12a.json \
    --out "$WORK/canv" --as ffa_friend_row >/dev/null
python3 $M/canvas_edit.py emit menu_central_region_canvas --edits $M/settings_tab/diag_w12a.json \
    --out "$WORK/canv" --as ffa_settings_diagram >/dev/null
for h in 1a76fc13584f4e4a 6468b73f55e3d43d d666ceaea20907c3; do test -f "$WORK/canv/$D/$h"; done

python3 $M/menu_layout.py mnu_master --base "$BASE" --out "$WORK/l1" \
    --canvas-scale $FIELD:1.45 --pose $FIELD:1.466,-1.95,6.0:$Q > "$WORK/layout1.log"
python3 $M/menu_layout.py mnu_master --base "$WORK/l1" --out "$OUT" --pointer-aspect $FIELD:0.0931034 > "$WORK/layout2.log"

cp "$WORK/canv/$D/1a76fc13584f4e4a" "$WORK/canv/$D/6468b73f55e3d43d" "$WORK/canv/$D/d666ceaea20907c3" "$OUT/$D/"
rm -rf "$WORK"
echo "built $OUT"
