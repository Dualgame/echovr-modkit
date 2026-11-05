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
SETP1=0x1cd33f3afd635d22; BUBBLE=0xca760b3376a22fc9
DONOR_PAGE=15; DONOR_OWNER=0x82a038c3a66135a9; DONOR_EL=0xbac67d9aa539d03e
TB=ui_mnu_card_third_button
LOG="$WORK/build.log"; : > "$LOG"

emit() { python3 menu_ui/canvas_edit.py emit "$1" --edits "$HERE/$2.json" --out "$WORK/canv" --as "$3" >> "$WORK/canvas.log"; }
emit $TB frame ffa_rp_frame
emit $TB frame_done ffa_rp_frame_done
emit $TB row ffa_rp_row
emit $TB reason ffa_rp_reason
emit $TB reason_harass ffa_rp_reason_harass
emit $TB reason_inappr ffa_rp_reason_inappr
emit $TB reason_unsport ffa_rp_reason_unsport
emit $TB reason_imperson ffa_rp_reason_imperson
emit $TB check ffa_rp_check
emit ui_mnu_button_small button_dim ffa_rp_button_dim

CUR="$BASE"; N=0
step() { N=$((N+1)); NX="$WORK/s$N"; }
for P in ffa_rp_list_page ffa_rp_reasons_page ffa_rp_done_page; do
  step; python3 menu_ui/page_add.py $L --name $P --donor-page $DONOR_PAGE --parent 0 --base "$CUR" --out "$NX" >> "$LOG"; CUR="$NX"
  step; python3 menu_ui/ui_actor_mint.py $L --template $DONOR_OWNER --name $P --actor-only --base "$CUR" --out "$NX" >> "$LOG"; CUR="$NX"
done
grep -o '"page": [0-9]*' "$LOG" | tr '\n' ' ' >> "$WORK/pages.txt"
PL=22; PR=23; PD=24
[ "$(grep -c '"page": 2[234]' "$LOG")" -ge 3 ] || { echo "page_add: pages are not 22..24" >&2; cat "$WORK/pages.txt" >&2; exit 1; }

tile() { step
  python3 menu_ui/ui_actor_mint.py $L --template $SETP1 --body-from $L:$BUBBLE --name "$1" --canvas $TB \
    --text-element buttontext --highlight-element "$7" --trans "$3" "$4" "$5" --page "$2" --label "$6" \
    --base "$CUR" --out "$NX" > "$WORK/mint_$1.log"; CUR="$NX"; }
btn() { step
  python3 menu_ui/ui_actor_mint.py $L --template $SETP1 --name "$1" --trans "$3" "$4" "$5" --page "$2" --label "$6" \
    --base "$CUR" --out "$NX" > "$WORK/mint_$1.log"; CUR="$NX"; }
ZF=5.20; ZB=5.15
tile ffa_rp_list_frame $PL 0 -0.175 $ZF "REPORT PLAYER" buttonicon
RY=(0.952 0.530 0.108 -0.315 -0.737 -1.159)
for i in 1 2 3 4 5 6; do tile ffa_rp_row$i $PL 0 "${RY[$((i-1))]}" $ZB "PLAYER $i" buttonhighlight; done
btn ffa_rp_more $PL -0.646 -1.693 $ZB "MORE"
btn ffa_rp_list_cancel $PL 0.646 -1.693 $ZB "CANCEL"
tile ffa_rp_reasons_frame $PR 0 -0.175 $ZF "REPORT PLAYER" buttonicon
tile ffa_rp_reason1 $PR 1.155 0.288 $ZB "Hate speech" buttonhighlight
tile ffa_rp_reason2 $PR 0 0.288 $ZB "Cheating" buttonhighlight
tile ffa_rp_reason3 $PR -1.155 0.288 $ZB "Harassment" buttonhighlight
tile ffa_rp_reason4 $PR 1.155 -0.222 $ZB "Inappropriate" buttonhighlight
tile ffa_rp_reason5 $PR 0 -0.222 $ZB "Unsportsmanlike" buttonhighlight
tile ffa_rp_reason6 $PR -1.155 -0.222 $ZB "Impersonation" buttonhighlight
tile ffa_rp_mg $PR -0.075 -0.659 $ZB "Mute & Ghost Reported Player" buttonhighlight
btn ffa_rp_cancel $PR 0.646 -1.693 $ZB "CANCEL"
btn ffa_rp_submit $PR -0.646 -1.693 $ZB "SUBMIT"
tile ffa_rp_done_frame $PD 0 -0.175 $ZF "REPORT PLAYER" buttonicon
btn ffa_rp_ok $PD 0 -1.693 $ZB "OK"

id() { python3 -c "import sys; sys.path.insert(0,'.'); from quest_clone_map import csymbol64; print('0x%016x' % csymbol64('$1'))"; }
Q=0,1,0,0; ARGS=()
lay() { local a; a=$(id "$1"); ARGS+=(--pose "$a:$2,$3,$4:$Q:1" --tooltip "$a=")
  [ -z "${5:-}" ] || ARGS+=(--canvas-rebind "$a:$5" --pointer-size "$a:$6"); }
FR=4.6948,3.5211; ROWQ=3.6329,0.3974; RQ=1.0433,0.3974; MGQ=1.9872,0.2546
lay ffa_rp_list_frame 0 -0.175 $ZF ffa_rp_frame $FR
for i in 1 2 3 4 5 6; do lay ffa_rp_row$i 0 "${RY[$((i-1))]}" $ZB ffa_rp_row $ROWQ; done
lay ffa_rp_more -0.646 -1.693 $ZB
lay ffa_rp_list_cancel 0.646 -1.693 $ZB
lay ffa_rp_reasons_frame 0 -0.175 $ZF ffa_rp_frame $FR
lay ffa_rp_reason1 1.155 0.288 $ZB ffa_rp_reason $RQ
lay ffa_rp_reason2 0 0.288 $ZB ffa_rp_reason $RQ
lay ffa_rp_reason3 -1.155 0.288 $ZB ffa_rp_reason_harass $RQ
lay ffa_rp_reason4 1.155 -0.222 $ZB ffa_rp_reason_inappr $RQ
lay ffa_rp_reason5 0 -0.222 $ZB ffa_rp_reason_unsport $RQ
lay ffa_rp_reason6 -1.155 -0.222 $ZB ffa_rp_reason_imperson $RQ
lay ffa_rp_mg -0.075 -0.659 $ZB ffa_rp_check $MGQ
lay ffa_rp_cancel 0.646 -1.693 $ZB
lay ffa_rp_submit -0.646 -1.693 $ZB
ARGS+=(--canvas-rebind "$(id ffa_rp_submit):ffa_rp_button_dim")
lay ffa_rp_done_frame 0 -0.175 $ZF ffa_rp_frame_done $FR
lay ffa_rp_ok 0 -1.693 $ZB
step
python3 menu_ui/menu_layout.py $L --base "$CUR" --out "$NX" \
    --unmember $DONOR_EL:$PL --unmember $DONOR_EL:$PR --unmember $DONOR_EL:$PD "${ARGS[@]}" > "$WORK/menu_layout.log"
CUR="$NX"

(cd "$WORK/canv" && find . -type f) | while read -r f; do
  [ ! -e "$CUR/$f" ] || { echo "refused: tree already has $f" >&2; exit 1; }
done
mv "$CUR" "$OUT"
cp -a "$WORK/canv/." "$OUT/"
rm -rf "$WORK"
echo "built $OUT ($(find "$OUT" -type f | wc -l) files)"
if [ -n "$QOUT" ]; then
  rm -rf "$QOUT"
  python3 menu_ui/quest_stage.py transform "$OUT" "$QOUT" | tail -n1
  find "$QOUT" -type f \( -name 0dc32069c96e74a1 -o -name 3f9915d3001dc28e \) -delete
  echo "Quest: $(find "$QOUT" -type f | wc -l) files in $QOUT (stage with ffa_w17a_qglb merged)"
fi
