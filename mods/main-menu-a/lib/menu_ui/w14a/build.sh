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
HOMEO=0x05ce113632359ce3; P30=0x05eb6c644ad6ef27; P58=0xe73618208c520c8d; SPARE=0xcb17bbfd14e80d86
ONL=0x7593d0177b013e69; HB=0xa487e477aaa52290
FRQ=0,0.9272,0,0.3746
POS=-4.4133,1.35,4.0667
clone() { echo --clone-module $1:27:$2 --clone-bind $2:eula_page=p24 --clone-bind $2:credits_page=p58 \
  --clone-bind $2:invite_friends_button=$SPARE --clone-bind $2:group_select_page=p24 --clone-bind $2:change_group_button=$HB \
  --clone-bind $2:tutorial_card=$SPARE --clone-bind $2:play_card=$SPARE --clone-bind $2:join_card=$SPARE --clone-bind $2:create_card=$SPARE; }

python3 menu_ui/canvas_edit.py emit ui_mni_quit_info --edits "$HERE/home_glyph.json" --out "$WORK/canv" --as ffa_home_glyph > "$WORK/canvas.log"
test -f "$WORK/canv/$D/fa07da0eddb8df3a"

python3 menu_ui/ui_actor_mint.py mnu_master --template $ONL --name ffa_home_btn --canvas ui_mnu_card_third_button \
    --trans ${POS//,/ } --page 24 --label "" --text-element buttontext --highlight-element buttonhighlight \
    --base "$BASE" --out "$WORK/m1" > "$WORK/mint.log"
grep -q "0x$(printf %016x $HB)" "$WORK/mint.log" || { echo "mint: owner id mismatch" >&2; exit 1; }

python3 menu_ui/menu_layout.py mnu_master --base "$WORK/m1" --out "$OUT" \
    --canvas-rebind $HB:ffa_home_glyph --pointer-size $HB:0.393,0.393 --pose $HB:$POS:$FRQ \
    --member $HB:30 --member $HB:58 --tooltip "$HB=Home" \
    --bind $HOMEO:eula_page=p32 $(clone $P58 ffa_home_route_help) $(clone $P30 ffa_home_route_settings) --bind $HOMEO:eula_page=p24 \
    > "$WORK/menu_layout.log"

mkdir -p "$OUT/$D"; cp "$WORK/canv/$D/fa07da0eddb8df3a" "$OUT/$D/"
tail -n1 "$WORK/mint.log"
rm -rf "$WORK"
echo "built $OUT"
