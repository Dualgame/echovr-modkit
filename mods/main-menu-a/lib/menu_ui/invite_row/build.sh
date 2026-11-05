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
FIELD_CANVAS=1a76fc13584f4e4a
SETTINGS=0x623b5f0e1e11a695
FIELD=0x94d77fe1ca3e487f; LINK=0x37181a73f772c20d; TIP=0x64a8671996f12efd
Q=0,0.9989180564880371,0.04650497809052467,0

python3 menu_ui/canvas_edit.py emit ui_mnu_deeplinking_invite_code --edits "$HERE/ops/invite_field.json" \
    --out "$WORK/canv" --as ffa_invite_code_field >/dev/null
test -f "$WORK/canv/$D/$FIELD_CANVAS"

python3 menu_ui/ui_actor_mint.py mnu_master --template $SETTINGS --body-from mnu_master:0xa9bd0cb4e6f2004c \
    --name ffa_my_invite_code --label "*****" --text-element codetext --click-event evt_ffa_my_code_eye_clicked \
    --trans 1.05 -1.95 6.0 --page 24 --base "$BASE" --out "$WORK/m1" > "$WORK/mint_field.log"

python3 menu_ui/ui_actor_mint.py mnu_master --template $SETTINGS --body-from mnu_master:0x98be8277516da28c \
    --name ffa_invite_link --trans -0.575 -1.95 6.0 --page 24 --label "INVITE LINK" \
    --click-event evt_link_button_clicked --base "$WORK/m1" --out "$WORK/m2" > "$WORK/mint_link.log"

python3 menu_ui/menu_layout.py mnu_master --base "$WORK/m2" --out "$OUT" \
    --canvas-rebind $FIELD:ffa_invite_code_field \
    --pose $FIELD:1.05,-1.95,6.0:$Q \
    --pose $LINK:-0.575,-1.95,6.0:$Q \
    --tooltip "$LINK=Share a link to your party" \
    --tooltip "$FIELD=Show or hide your invite code" \
    --pose $TIP:0.042,-2.0,4.7:0,0.9848078489303589,0.17364820837974548,0 > "$WORK/menu_layout.log"

mkdir -p "$OUT/$D"; cp "$WORK/canv/$D/$FIELD_CANVAS" "$OUT/$D/"
tail -qn1 "$WORK/mint_field.log" "$WORK/mint_link.log"
rm -rf "$WORK"
echo "built $OUT"
