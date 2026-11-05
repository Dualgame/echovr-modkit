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
L=mnu_master_mp_ingame
SETP1=0x1cd33f3afd635d22; FRAME=0xedcea26f534fa586; RESUME=0x96514fc60600d922
BUBBLE=0xca760b3376a22fc9; MUTE=0xc9a885a88c32ba75; GHOST=0x58e7e36afb3a0af3; RT=0xf34bc06e9c2ef30f
REV=${W18_REV:-c}; CH=0xb8ccc5b0561c8885
case $REV in
  a) CREATE=0xcad1d512fe67a466; JOIN=0xe2e143bfb28cb4b1; CHROME=pause_grid_chrome.json;;
  b) CREATE=0xe2e143bfb28cb4b1; JOIN=0xcad1d512fe67a466; CHROME=pause_grid_chrome_b.json;;
  c) CREATE=0xe2e143bfb28cb4b1; JOIN=0xcad1d512fe67a466; CHROME=;;
  *) echo "W18_REV must be a, b or c" >&2; exit 2;;
esac
LINFO=0x1d9fdece169bf28e; QLOBBY=0x0c023e8ea24ab73a; QMENU=0x26a1c4923b34ee4e; QGAME=0x3ed0bc316093f490; FRL=0x2d9dedefa051bd20
QL7=0,0.9981348,0,-0.06104854; QR7=0,0.9981348,0,0.06104854
QL20=0,0.9848077,0,-0.17364818; QR20=0,0.9848077,0,0.17364818
TW=1.3097,0.43
ST=0xb8ccd5ac45079093; RV=0x83e11e392245aa8e
QST=0,0.9925462,0,-0.1218693; QRV=0,0.9925462,0,0.1218693

if [ "$REV" = c ]; then
  python3 menu_ui/canvas_edit.py emit ui_mnu_service_status_text_only --edits "$HERE/pause_footer_status.json" --out "$WORK/canv" --as ffa_pause_status_text > "$WORK/canvas.log"
  RADV_EDITS=pause_footer_radv_rt.json; [ "${W18_RADV_RT:-1}" = 0 ] && RADV_EDITS=pause_footer_radv.json
  python3 menu_ui/canvas_edit.py emit ui_menu_rad_version --edits "$HERE/$RADV_EDITS" --out "$WORK/canv" --as ffa_pause_radv_text >> "$WORK/canvas.log"
else
  python3 menu_ui/canvas_edit.py emit ui_mnu_button_small --edits "$HERE/$CHROME" --out "$WORK/canv" --as ffa_pause_grid > "$WORK/canvas.log"
fi
python3 menu_ui/canvas_edit.py emit ui_mnu_card_third_button --edits "$HERE/pause_toggle.json" --out "$WORK/canv" --as ffa_pause_toggle >> "$WORK/canvas.log"
python3 menu_ui/canvas_edit.py emit ui_mnu_card_third_button --edits "$HERE/pause_report_tile.json" --out "$WORK/canv" --as ffa_pause_report_tile >> "$WORK/canvas.log"

if [ "$REV" = c ]; then
  python3 menu_ui/ui_actor_mint.py $L --template $SETP1 --name ffa_pause_status --canvas ui_mnu_button_small \
      --trans 1.4064 -1.5 5.8214 --page 1 --label "Online" --base "$BASE" --out "$WORK/m0" > "$WORK/mint_status.log"
  grep -q "\"owner\": \"$ST\"" "$WORK/mint_status.log" || { echo "mint: ffa_pause_status owner id mismatch" >&2; exit 1; }
  python3 menu_ui/ui_actor_mint.py $L --template $SETP1 --name ffa_pause_radv --canvas ui_mnu_button_small \
      --trans -1.4882 -1.42 5.819 --page 1 --label "RAD VERSION" --base "$WORK/m0" --out "$WORK/m1" > "$WORK/mint.log"
  grep -q "\"owner\": \"$RV\"" "$WORK/mint.log" || { echo "mint: ffa_pause_radv owner id mismatch" >&2; exit 1; }
  CHROME_OPS=(--canvas-rebind $ST:ffa_pause_status_text --pointer-size $ST:3.1,0.28 --pose $ST:1.4064,-1.5,5.8214:$QST:1 --tooltip "$ST="
              --canvas-rebind $RV:ffa_pause_radv_text --pointer-size $RV:3.0,0.14 --pose $RV:-1.4882,-1.42,5.819:$QRV:1 --tooltip "$RV=")
else
  python3 menu_ui/ui_actor_mint.py $L --template $SETP1 --name ffa_pause_chrome --canvas ui_mnu_button_small \
      --trans 0 -0.268 6.0 --page 1 --label "PAUSE MENU" --base "$BASE" --out "$WORK/m1" > "$WORK/mint.log"
  grep -q "\"owner\": \"$CH\"" "$WORK/mint.log" || { echo "mint: owner id mismatch" >&2; exit 1; }
  CHROME_OPS=(--unmember $FRAME:1
              --canvas-rebind $CH:ffa_pause_grid --pointer-size $CH:5.87,2.478 --pose $CH:0,-0.268,6.0:0,1,0,0:1 --tooltip "$CH=")
fi

python3 menu_ui/page_patch.py $L 1 --card $CREATE tile_create "CREATE GAME" "" "Create a private match" \
    --card $JOIN tile_join "JOIN VIA CODE" "" "Join a match with an invite code" --base "$WORK/m1" --out "$WORK/m2" > "$WORK/cards.log"

python3 menu_ui/menu_layout.py $L --base "$WORK/m2" --out "$WORK/m3" \
    "${CHROME_OPS[@]}" \
    --pose $RESUME:2.15,-0.275,5.53:$QL20:1 \
    --canvas-rebind $BUBBLE:ffa_pause_toggle --pointer-size $BUBBLE:$TW --pose $BUBBLE:0.73,0.442,5.817:$QL7:1 \
    --canvas-rebind $GHOST:ffa_pause_toggle --pointer-size $GHOST:$TW --pose $GHOST:0.73,-0.030,5.817:$QL7:1 \
    --canvas-rebind $MUTE:ffa_pause_toggle --pointer-size $MUTE:$TW --pose $MUTE:0.73,-0.508,5.817:$QL7:1 \
    --canvas-rebind $RT:ffa_pause_report_tile --pointer-size $RT:$TW --pose $RT:0.73,-0.989,5.817:$QL7:1 --text "$RT=REPORT" \
    --pose $CREATE:-0.73,-0.275,5.816:$QR7:1 \
    --pose $JOIN:-2.15,-0.275,5.53:$QR20:1 \
    --pose $LINFO:4.1,0.333,4.4 --pose $SETP1:4.1,0.0,4.4 --pose $QLOBBY:4.1,-0.337,4.4 \
    --pose $QMENU:4.1,-0.673,4.4 --pose $QGAME:4.1,-1.008,4.4 --pose $FRL:4.1,-1.735,4.4 > "$WORK/menu_layout.log"

(cd "$WORK/canv" && find . -type f) | while read -r f; do
  [ ! -e "$WORK/m3/$f" ] || { echo "refused: tree already has $f" >&2; exit 1; }
done
mv "$WORK/m3" "$OUT"
cp -a "$WORK/canv/." "$OUT/"
[ "$REV" != c ] || tail -n1 "$WORK/mint_status.log"
tail -n1 "$WORK/mint.log"
rm -rf "$WORK"
echo "built $OUT"
if [ -n "$QOUT" ]; then
  rm -rf "$QOUT"
  python3 menu_ui/quest_stage.py transform "$OUT" "$QOUT" | tail -n1
  find "$QOUT" -type f \( -name 0dc32069c96e74a1 -o -name 3f9915d3001dc28e \) -delete
  echo "Quest: $(find "$QOUT" -type f | wc -l) files in $QOUT (stage with ffa_w17a_qglb merged)"
fi
