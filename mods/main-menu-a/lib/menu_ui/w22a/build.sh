#!/usr/bin/env bash
set -euo pipefail
REPO="${MAIN_MENU_A_ROOT:?run build_main_menu_a.py}"
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BASE=$1; OUT=$2; QOUT=${3:-}
for d in "$OUT" "$QOUT"; do case "$d" in */_data/*) echo "refused: $d" >&2; exit 2;; esac; done
case "${OUT%/}" in ""|/) echo "refused: empty or root OUT" >&2; exit 2;; esac; case "$(basename "$OUT")" in ffa_*) ;; *) echo "refused: OUT basename must start with ffa_ ($OUT)" >&2; exit 2;; esac
[ -d "$BASE" ] || { echo "refused: BASE $BASE missing" >&2; exit 2; }
[ "$(realpath -m "$BASE")" != "$(realpath -m "$OUT")" ] || { echo "refused: OUT == BASE" >&2; exit 2; }
WORK="$OUT.work"
cd "$REPO/lib"
rm -rf "$OUT" "$WORK"; mkdir -p "$WORK"
L=mnu_master_mp_ingame
SETP1=0x1cd33f3afd635d22; TB=ui_mnu_card_third_button; PL=22; ZB=5.15
LOG="$WORK/build.log"; : > "$LOG"
id() { python3 -c "import sys; sys.path.insert(0,'.'); from quest_clone_map import csymbol64; print('0x%016x' % csymbol64('$1'))"; }
python3 - "$BASE" <<'PY' || { echo "refused: BASE has no ffa_rp_row6 (not a w19a / w19c tree)" >&2; exit 2; }
import sys, os
sys.path.insert(0, '.')
from quest_clone_map import csymbol64
k = csymbol64('ffa_rp_row6').to_bytes(8, 'little')
hit = any(k in open(os.path.join(d, f), 'rb').read() for d, _, fs in os.walk(sys.argv[1]) for f in fs)
sys.exit(0 if hit else 1)
PY

python3 menu_ui/canvas_edit.py emit $TB --edits "$HERE/row_peek.json" --out "$WORK/canv" --as ffa_rp_row_peek >> "$WORK/canvas.log"

python3 menu_ui/ui_actor_mint.py $L --template $SETP1 --name ffa_rp_list_submit --trans -0.646 -1.693 $ZB --page $PL \
    --label "SUBMIT" --base "$BASE" --out "$WORK/s1" > "$WORK/mint_ffa_rp_list_submit.log"
grep -q "\"owner\": \"$(id ffa_rp_list_submit)\"" "$WORK/mint_ffa_rp_list_submit.log" || { echo "mint owner mismatch" >&2; exit 1; }

Q=0,1,0,0; ARGS=()
RY=(0.840 0.387 -0.066 -0.520 -0.973 -1.336)
for i in 1 2 3 4 5 6; do a=$(id ffa_rp_row$i); ARGS+=(--pose "$a:0,${RY[$((i-1))]},$ZB:$Q:1"); done
a=$(id ffa_rp_row6); ARGS+=(--canvas-rebind "$a:ffa_rp_row_peek" --pointer-size "$a:3.6329,0.2174")
a=$(id ffa_rp_list_submit); ARGS+=(--pose "$a:-0.646,-1.693,$ZB:$Q:1" --tooltip "$a=" --canvas-rebind "$a:ffa_rp_button_dim")
python3 menu_ui/menu_layout.py $L --base "$WORK/s1" --out "$WORK/s2" --unmember "$(id ffa_rp_more):$PL" "${ARGS[@]}" > "$WORK/menu_layout.log"

(cd "$WORK/canv" && find . -type f) | while read -r f; do
  [ ! -e "$WORK/s2/$f" ] || { echo "refused: tree already has $f" >&2; exit 1; }
done
mv "$WORK/s2" "$OUT"
cp -a "$WORK/canv/." "$OUT/"
tail -n 3 "$WORK/menu_layout.log"
rm -rf "$WORK"
echo "built $OUT ($(find "$OUT" -type f | wc -l) files)"
if [ -n "$QOUT" ]; then
  case "$(basename "$QOUT")" in ffa_*) ;; *) echo "refused: QOUT basename must start with ffa_ ($QOUT)" >&2; exit 2;; esac
  rm -rf "$QOUT"
  python3 menu_ui/quest_stage.py transform "$OUT" "$QOUT" | tail -n1
  find "$QOUT" -type f \( -name 0dc32069c96e74a1 -o -name 3f9915d3001dc28e \) -delete
  echo "Quest: $(find "$QOUT" -type f | wc -l) files in $QOUT (stage with ffa_w17a_qglb merged)"
fi
