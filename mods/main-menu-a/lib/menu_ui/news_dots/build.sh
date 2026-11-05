#!/usr/bin/env bash
set -euo pipefail
REPO="${MAIN_MENU_A_ROOT:?run build_main_menu_a.py}"
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BASE=$1; OUT=$2
WORK="$OUT.work"
cd "$REPO/lib"
rm -rf "$OUT" "$WORK"; mkdir -p "$WORK"
D=primary/59a9bd6e4525ecc4/v6500253412

python3 menu_ui/canvas_edit.py emit ui_mnu_settings_invite_regenerate --edits "$HERE/dot.json" --out "$WORK/canv" --as ffa_news_dot >/dev/null
test -f "$WORK/canv/$D/a28dfe61aaa3398a"

Q=0,0.9848077,0,0.17364818
Y=-0.6657
X=(-1.9211 -2.1466 -2.3721); Z=(5.6027 5.5206 5.4385)
prev="$BASE"
for i in 0 1 2; do
  python3 menu_ui/ui_actor_mint.py mnu_master --template 0x623b5f0e1e11a695 --name ffa_news_dot$i \
      --canvas ui_mnu_settings_invite_regenerate --trans ${X[$i]} $Y ${Z[$i]} --page 24 --label "" \
      --click-event evt_ffa_news_dot${i}_clicked --base "$prev" --out "$WORK/m$i" >/dev/null
  prev="$WORK/m$i"
done

args=()
i=0
for A in 0x1cfbcecb4b2d74a4 0x1cfbcecb4b2d74a5 0x1cfbcecb4b2d74a6; do
  args+=(--canvas-rebind $A:ffa_news_dot --canvas-scale $A:3 --pointer-size $A:0.22,0.22 --pose $A:${X[$i]},$Y,${Z[$i]}:$Q --tooltip "$A=")
  i=$((i+1))
done
python3 menu_ui/menu_layout.py mnu_master --base "$prev" --out "$OUT" "${args[@]}" > "$WORK/menu_layout.log"

cp "$WORK/canv/$D/a28dfe61aaa3398a" "$OUT/$D/"
python3 menu_ui/canvas_edit.py emit ui_mnu_news_info --edits "$HERE/news_full.json" --out "$OUT" >/dev/null
rm -rf "$WORK"
echo "built $OUT"
