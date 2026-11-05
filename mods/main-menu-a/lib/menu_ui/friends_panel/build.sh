#!/usr/bin/env bash
set -euo pipefail
REPO="${MAIN_MENU_A_ROOT:?run build_main_menu_a.py}"
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BASE="${1:?usage: BASE OUT}"
OUT="${2:-$HERE/ffa_w6h}"
WORK="$OUT.work"; CANV="$OUT.canv"; OPS="$HERE/ops"
cd "$REPO/lib"
rm -rf "$OUT" "$WORK" "$CANV"; mkdir -p "$WORK"

D=primary/59a9bd6e4525ecc4/v6500253412
mkdir -p "$CANV/$D"
for spec in ui_mnu_settings_invite_regenerate:friend_row_b:ffa_friend_row \
            ui_mnu_settings_invite_regenerate:friend_row_empty:ffa_friend_row_empty \
            ui_mnu_earlyquit_lock_out_timer:friends_banner_b:ffa_friends_banner \
            ui_mnu_button_small:invite_button_b:ffa_invite_friends_button; do
  IFS=: read -r src ops name <<<"$spec"
  python3 menu_ui/canvas_edit.py emit "$src" --edits "$OPS/$ops.json" --out "$WORK/emit_$ops" >/dev/null
  srcf=$(python3 -c "from quest_clone_map import csymbol64; print('%016x' % csymbol64('$src'))")
  newf=$(python3 -c "from quest_clone_map import csymbol64; print('%016x' % csymbol64('$name'))")
  cp "$WORK/emit_$ops/$D/$srcf" "$CANV/$D/$newf"
done

Q=0,0.9135454297065735,0,0.40673667192459106
CX=-4.285; CZ=4.195
prev="$BASE"
for n in 2 3 4 5; do
  nxt="$WORK/m$n"
  python3 menu_ui/ui_actor_mint.py mnu_master --template 0x623b5f0e1e11a695 --name ffa_friends_row$n \
      --canvas ui_mnu_settings_invite_regenerate --trans $CX 0 $CZ --page 24 --label "" \
      --base "$prev" --out "$nxt" >/dev/null
  prev="$nxt"
done

HDR=0xe2b8df2df8ceab62; R1=0xecce8089fb0c0501; INV=0x1b71f837dc9e5669
args=(--canvas-rebind $HDR:ffa_friends_banner --pointer-size $HDR:1.6,0.177 --pose $HDR:$CX,0.625,$CZ:$Q
      --canvas-rebind $INV:ffa_invite_friends_button --pointer-size $INV:1.6,0.175 --pose $INV:$CX,-0.8495,$CZ:$Q
      --text "$INV=INVITE FRIENDS" --text "$R1=NO FRIENDS ONLINE")
Y=(0.4005 0.1455 -0.1095 -0.3645 -0.6195)
k=0
for R in $R1 0xecce8089fb0c0502 0xecce8089fb0c0503 0xecce8089fb0c0504 0xecce8089fb0c0505; do
  C=ffa_friend_row_empty; [ "$R" = "$R1" ] && C=ffa_friend_row
  args+=(--canvas-rebind $R:$C --pointer-size $R:1.6,0.245 --pose $R:$CX,${Y[$k]},$CZ:$Q --tooltip "$R=See this friend")
  k=$((k+1))
done
python3 menu_ui/menu_layout.py mnu_master --base "$prev" --out "$OUT" "${args[@]}" > "$WORK/menu_layout.log"

mkdir -p "$OUT/$D"; cp "$CANV/$D"/* "$OUT/$D/"
echo "built $OUT"
