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
SET=0x623b5f0e1e11a695; HELP=0x71f7c477f0619237; ONL=0x7593d0177b013e69; QUIT=0x720d06f0ec096825
STORE=0xf769a426b03d60b3; SEASONP=0x1d47c73d4d217518; SAFE=0x6bead65a6955c109
LOGO=0x6d94aa64ae5c56fe; FOOT=0x7da8ba854cd99a2b; RADV=0xc11d245b2fac7825
RADV_H=0x5ad7052968bc8cbc; RADV_S=0xad6477c0ea361317
FRHDR=0xe2b8df2df8ceab62; FRINV=0x1b71f837dc9e5669
KEY_CLICK=0x63becd132df98137; EV_HELP_SEASON=0x8adf5a10c6b3d9b6; EV_HELP_SAFE=evt_ffa_help_safezone_clicked
RQ=0,0.9925,0,0.1219
RAILQ=0,0.9272,0,-0.3746; FRQ=0,0.9272,0,0.3746
clone() { echo --clone-module $P58:27:$1 --clone-bind $1:eula_page=p24 --clone-bind $1:credits_page=p58 \
  --clone-bind $1:invite_friends_button=$SPARE --clone-bind $1:group_select_page=$2 --clone-bind $1:change_group_button=$3 \
  --clone-bind $1:tutorial_card=$SPARE --clone-bind $1:play_card=$SPARE --clone-bind $1:join_card=$SPARE --clone-bind $1:create_card=$SPARE; }

python3 -c 'import json,sys; print(json.dumps(json.load(open(sys.argv[1])) + json.load(open(sys.argv[2]))))' \
    menu_ui/news_dots/dot.json "$HERE/dot_box.json" > "$WORK/dot.json"
python3 menu_ui/canvas_edit.py emit ui_mnu_settings_invite_regenerate --edits "$WORK/dot.json" --out "$WORK/canv" --as ffa_news_dot >/dev/null
test -f "$WORK/canv/$D/a28dfe61aaa3398a"

python3 menu_ui/ui_actor_mint.py mnu_master --template $FOOT --name ffa_radv_help --canvas ui_menu_rad_version \
    --script-from $RADV --trans -1.4882 -1.42 5.819 --page 58 --base "$BASE" --out "$WORK/m1" > "$WORK/mint_radv_help.log"
python3 menu_ui/ui_actor_mint.py mnu_master --template $FOOT --name ffa_radv_settings --canvas ui_menu_rad_version \
    --script-from $RADV --trans -1.4882 -1.42 5.819 --page 30 --base "$WORK/m1" --out "$WORK/m2" > "$WORK/mint_radv_settings.log"

SBTN=$(python3 -c 'import sys; sys.path[:0] = ["menu_ui", "."]
import menu_layout as ml
from game_store import Basis
from pathlib import Path
b = Basis.named("pcf"); d = (Path(sys.argv[1]) / b.tree_rel(b.row("primary", ml.SCRIPT, "mnu_master"))).read_bytes()
r = [i for i in ml._records(d, int(sys.argv[2], 0)) if ml._q(d, ml.TOP + i * ml.SREC + 32) == ml.BUTTON_MODULE]
assert len(r) == 1, r; print(r[0])' "$WORK/m2" $SEASONP)
python3 menu_ui/menu_layout.py mnu_master --base "$WORK/m2" --out "$OUT" \
    --bind $HOMEO:eula_page=p32 $(clone help_store p6 $STORE) --bind $HOMEO:eula_page=p24 \
    --clone-module $SAFE:$SBTN:ffa_safezone_btn \
    --sym $SEASONP:$KEY_CLICK=$EV_HELP_SEASON --sym $SAFE:$KEY_CLICK=$EV_HELP_SAFE \
    --tooltip "$SAFE=Lobby rules" \
    --show $LOGO:$P58 --show $LOGO:$P30 \
    --pose $RADV_H:-1.4882,-1.42,5.819:$RQ --pose $RADV_S:-1.4882,-1.42,5.819:$RQ \
    --pose $LOGO:0,2,9:0,1,0,0:0.68 \
    --pose $ONL:3.78,0.391,4.66:$RAILQ --pose $SET:3.78,0.0127,4.66:$RAILQ \
    --pose $HELP:3.78,-0.3663,4.66:$RAILQ --pose $QUIT:3.78,-0.749,4.66:$RAILQ \
    --pose $FRHDR:-3.9819,0.625,4.4837:$FRQ --pose 0xecce8089fb0c0501:-3.9819,0.4005,4.4837:$FRQ \
    --pose 0xecce8089fb0c0502:-3.9819,0.1455,4.4837:$FRQ --pose 0xecce8089fb0c0503:-3.9819,-0.1095,4.4837:$FRQ \
    --pose 0xecce8089fb0c0504:-3.9819,-0.3645,4.4837:$FRQ --pose 0xecce8089fb0c0505:-3.9819,-0.6195,4.4837:$FRQ \
    --pose $FRINV:-3.9819,-0.8495,4.4837:$FRQ > "$WORK/menu_layout.log"

mkdir -p "$OUT/$D"; cp "$WORK/canv/$D/a28dfe61aaa3398a" "$OUT/$D/"
tail -qn1 "$WORK/mint_radv_help.log" "$WORK/mint_radv_settings.log"
rm -rf "$WORK"
echo "built $OUT"
