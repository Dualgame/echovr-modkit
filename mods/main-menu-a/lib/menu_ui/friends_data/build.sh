#!/usr/bin/env bash
set -euo pipefail
REPO="${MAIN_MENU_A_ROOT:?run build_main_menu_a.py}"
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BASE=$1; OUT=$2
case "$OUT" in */_data/*) echo "refused: OUT in the basis" >&2; exit 2;; esac
[ "$(realpath -m "$BASE")" != "$(realpath -m "$OUT")" ] || { echo "refused: OUT == BASE" >&2; exit 2; }
WORK="$OUT.work"
cd "$REPO/lib"
rm -rf "$OUT" "$WORK"; mkdir -p "$WORK"
D=primary/59a9bd6e4525ecc4/v6500253412
ROW_CANVAS=6468b73f55e3d43d

python3 menu_ui/canvas_edit.py emit ui_mnu_settings_invite_regenerate --edits "$HERE/ops/friend_row_v2.json" \
    --out "$WORK/canv" --as ffa_friend_row >/dev/null
test -f "$WORK/canv/$D/$ROW_CANVAS"

args=()
for R in 0xecce8089fb0c0502 0xecce8089fb0c0503 0xecce8089fb0c0504 0xecce8089fb0c0505; do
  args+=(--canvas-rebind $R:ffa_friend_row)
done
python3 menu_ui/menu_layout.py mnu_master --base "$BASE" --out "$OUT" "${args[@]}" > "$WORK/menu_layout.log"

python3 menu_ui/friends_data/click_event.py "$OUT" \
    0xecce8089fb0c0501=evt_ffa_friend_row1_clicked 0xecce8089fb0c0502=evt_ffa_friend_row2_clicked \
    0xecce8089fb0c0503=evt_ffa_friend_row3_clicked 0xecce8089fb0c0504=evt_ffa_friend_row4_clicked \
    0xecce8089fb0c0505=evt_ffa_friend_row5_clicked > "$WORK/click_event.log"

cp "$WORK/canv/$D/$ROW_CANVAS" "$OUT/$D/"
rm -rf "$WORK"
echo "built $OUT"
