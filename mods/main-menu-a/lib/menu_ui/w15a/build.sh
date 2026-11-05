#!/usr/bin/env bash
set -euo pipefail
REPO="${MAIN_MENU_A_ROOT:?run build_main_menu_a.py}"
BASE=$1; OUT=$2
case "$OUT" in */_data/*) echo "refused: OUT in the basis" >&2; exit 2;; esac
[ -d "$BASE" ] || { echo "refused: BASE $BASE missing" >&2; exit 2; }
[ "$(realpath -m "$BASE")" != "$(realpath -m "$OUT")" ] || { echo "refused: OUT == BASE" >&2; exit 2; }
WORK="$OUT.work"
cd "$REPO/lib"
rm -rf "$OUT" "$WORK"; mkdir -p "$WORK"
L=mnu_master_mp_ingame
SETP1=0x1cd33f3afd635d22; MUTEALL=0xca760b3376a22fc9; RT=0xf34bc06e9c2ef30f
POS=-0.73,-1.545,5.816; QC=0,0.9981348,0,0.06104854

python3 menu_ui/ui_actor_mint.py $L --template $SETP1 --body-from $L:$MUTEALL --name ffa_report_tile \
    --canvas ui_mnu_card_third_button --trans ${POS//,/ } --page 1 --label "REPORT PLAYER" \
    --text-element buttontext --highlight-element buttonhighlight --base "$BASE" --out "$WORK/m1" > "$WORK/mint.log"
grep -q "\"owner\": \"$RT\"" "$WORK/mint.log" || { echo "mint: owner id mismatch" >&2; exit 1; }

python3 menu_ui/menu_layout.py $L --base "$WORK/m1" --out "$OUT" \
    --pose $RT:$POS:$QC:1 --tooltip "$RT=Report a player in this match" > "$WORK/menu_layout.log"

tail -n1 "$WORK/mint.log"
rm -rf "$WORK"
echo "built $OUT"
