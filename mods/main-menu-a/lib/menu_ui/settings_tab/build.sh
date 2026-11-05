#!/usr/bin/env bash
set -euo pipefail
REPO="${MAIN_MENU_A_ROOT:?run build_main_menu_a.py}"
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BASE=$1; OUT=$2
WORK="$OUT.work"
cd "$REPO/lib"
rm -rf "$OUT" "$WORK"; mkdir -p "$WORK"
D=primary/59a9bd6e4525ecc4/v6500253412

python3 menu_ui/canvas_edit.py emit menu_central_region --edits "$HERE/hdr.json" --out "$WORK/canv" --as ffa_settings_header >/dev/null
python3 menu_ui/canvas_edit.py emit menu_central_region_canvas --edits "$HERE/diag.json" --out "$WORK/canv" --as ffa_settings_diagram >/dev/null
test -f "$WORK/canv/$D/ab36ff7906d521e2"; test -f "$WORK/canv/$D/d666ceaea20907c3"

python3 menu_ui/menu_layout.py mnu_master --base "$BASE" --out "$OUT" \
  --canvas-rebind 0x7875c93ce7f5b4e2:ffa_settings_diagram --canvas-rebind 0xb4b67254b666dc6a:ffa_settings_header \
  --pose-rows 0xdd45867f6ff95898:-2.2196,-0.5231,5.3585:0,0.98079,0,0.19509 \
  --pose-rows 0x8ecb1ba00a7781a2:2.2196,-0.5231,5.3585:0,0.98079,0,-0.19509 > "$WORK/menu_layout.log"

cp "$WORK/canv/$D/ab36ff7906d521e2" "$WORK/canv/$D/d666ceaea20907c3" "$OUT/$D/"
rm -rf "$WORK"
echo "built $OUT"
