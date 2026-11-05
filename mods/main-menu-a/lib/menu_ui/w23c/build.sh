#!/usr/bin/env bash
set -euo pipefail
REPO="${MAIN_MENU_A_ROOT:?run build_main_menu_a.py}"
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BASE=$1; OUT=$2
case "$OUT" in */_data/*) echo "refused: $OUT" >&2; exit 2;; esac
case "${OUT%/}" in ""|/) echo "refused: empty or root OUT" >&2; exit 2;; esac; case "$(basename "$OUT")" in ffa_*) ;; *) echo "refused: OUT basename must start with ffa_ ($OUT)" >&2; exit 2;; esac
[ -d "$BASE" ] || { echo "refused: BASE $BASE missing" >&2; exit 2; }
[ "$(realpath -m "$BASE")" != "$(realpath -m "$OUT")" ] || { echo "refused: OUT == BASE" >&2; exit 2; }
[ ! -e "$OUT" ] || { echo "refused: $OUT exists (pick a new tree name)" >&2; exit 2; }
WORK="$OUT.work"
cd "$REPO/lib"
python3 menu_ui/w23c/gen_ac_report.py --check >/dev/null || { echo "refused: w23c/ac_report.json is stale (run gen_ac_report.py)" >&2; exit 1; }
rm -rf "$WORK"; mkdir -p "$WORK/new"
GP_CANVAS=0x51d88ca2bae7aff8; GP=0x8a5dcee389df5b7c; SM=0x6c1f6ff04e070923
ZONES=(--add BackButton=ffa_acr_reason1@0.06357,-0.07354,0.015/0.03838,0.01553,0.0075
       --add BackButton=ffa_acr_reason2@0.15000,-0.07354,0.015/0.03838,0.01553,0.0075
       --add BackButton=ffa_acr_reason3@0.22236,-0.07354,0.015/0.02432,0.01553,0.0075
       --add BackButton=ffa_acr_reason4@0.06357,-0.11045,0.015/0.03838,0.01553,0.0075
       --add BackButton=ffa_acr_reason5@0.15000,-0.11045,0.015/0.03838,0.01553,0.0075
       --add BackButton=ffa_acr_reason6@0.23643,-0.11045,0.015/0.03838,0.01553,0.0075
       --add BackButton=ffa_acr_mg@0.10371,-0.17813,0.015
       --add BackButton=ffa_acr_cancel@0.09141,-0.15176,0.015/0.05156,0.01113,0.0075
       --add BackButton=ffa_acr_submit@0.20859,-0.15176,0.015/0.05156,0.01113,0.0075
       --add BackButton=ffa_acr_back@0.27422,-0.01641,0.015)

python3 menu_ui/canvas_edit.py emit $GP_CANVAS --edits "$HERE/ac_report.json" --out "$WORK/new" --as ffa_ac_report > "$WORK/canvas.log"
python3 menu_ui/ui_actor_mint.py components r14_glb_global_mp --owner $GP --donor-owner $SM "${ZONES[@]}" \
    --canvas ffa_ac_report --base "$BASE" --out "$WORK/new" > "$WORK/glb.log"
grep -c '^wrote' "$WORK/glb.log" | grep -qx 2 || { cat "$WORK/glb.log" >&2; echo "expected 2 level files" >&2; exit 1; }
(cd "$WORK/new" && find . -type f) > "$WORK/files"
cp -a "$BASE" "$OUT"
while read -r f; do
  case "$f" in *CUICanvasResource*|*/v*/90950ed3107711a6) [ ! -e "$OUT/$f" ] || { echo "refused: BASE already has canvas $f" >&2; exit 1; };; esac
done < "$WORK/files"
cp -a "$WORK/new/." "$OUT/"
echo "PC: $(wc -l < "$WORK/files") files written over $BASE -> $OUT ($(find "$OUT" -type f | wc -l) files)"
cat "$WORK/glb.log"
rm -rf "$WORK"
