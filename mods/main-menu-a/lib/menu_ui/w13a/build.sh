#!/usr/bin/env bash
set -euo pipefail
REPO="${MAIN_MENU_A_ROOT:?run build_main_menu_a.py}"
BASE=$1; OUT=$2
case "$OUT" in */_data/*) echo "refused: OUT in the basis" >&2; exit 2;; esac
[ -d "$BASE" ] || { echo "refused: BASE $BASE missing" >&2; exit 2; }
[ "$(realpath -m "$BASE")" != "$(realpath -m "$OUT")" ] || { echo "refused: OUT == BASE" >&2; exit 2; }
cd "$REPO/lib"
rm -rf "$OUT"
PLAY=0xa3d3ef315cb999db; TUT=0xd763e348e835f249; CREATE=0xff5375e5a4dee29d; SAFE=0x6bead65a6955c109
JOIN=0x097b785350a61c5f; EQ=0xa81caf26df3e7e4c; STORE=0xf769a426b03d60b3; SEASONP=0x1d47c73d4d217518
NEWS=0x27dfdb284b22a843; COMM=0x12bfda55b0b0af9f; ANC_C=0x5502bbb6f4ae1862; ANC_S=0xc3aff9abe56dfae0; SEASONT=0x9e0bf5fe92aba570
REPORT=0x1d47c63d5c3d6802; LEGAL=0xf769a439a13573ba; CRED=0x2eb1c2f8417f7141
DOT0=0x1cfbcecb4b2d74a4; DOT1=0x1cfbcecb4b2d74a5; DOT2=0x1cfbcecb4b2d74a6
Q1=0,0.9848077,0,-0.17364818; Q2=0,0.9981348,0,-0.06104854; Q3=0,0.9981348,0,0.06104854; Q4=0,0.9848077,0,0.17364818
X1=2.10302; Z1=5.54710
X2=0.71015; Z2=5.81944
X3=-0.71015; Z3=5.81844
X4=-2.10302; Z4=5.54710
python3 menu_ui/menu_layout.py mnu_master --base "$BASE" --out "$OUT" \
    --pose $PLAY:$X1,-0.275,$Z1:$Q1 --pose $TUT:$X1,-0.275,$Z1:$Q1 \
    --pose $CREATE:$X2,-0.275,$Z2:$Q2 --pose $SAFE:$X2,-0.275,$Z2:$Q2 \
    --pose $JOIN:$X3,-0.275,$Z3:$Q3 --pose $STORE:$X3,-0.275,$Z3:$Q3 --pose $EQ:$X3,0.35,$Z3:$Q3 --pose $SEASONP:$X3,-0.9,$Z3:$Q3 \
    --pose $NEWS:$X4,-0.06,$Z4:$Q4 --pose $COMM:$X4,-0.99,$Z4:$Q4 --pose $ANC_C:$X4,-0.99,$Z4:$Q4 \
    --pose $SEASONT:$X4,0.32,5.61710:$Q4 --pose $ANC_S:$X4,0.32,5.61710:$Q4 \
    --pose $REPORT:$X4,0.35,$Z4:$Q4 --pose $LEGAL:$X4,-0.275,$Z4:$Q4 --pose $CRED:$X4,-0.9,$Z4:$Q4 \
    --pose $DOT0:-1.87412,-0.6657,5.61980:$Q4 --pose $DOT1:-2.09962,-0.6657,5.53770:$Q4 --pose $DOT2:-2.32512,-0.6657,5.45560:$Q4 \
    > "$OUT.layout.log"
python3 - "$OUT.layout.log" <<'EOF'
import json, sys
ops = json.load(open(sys.argv[1]))["ops"]
bad = [o["actor"] for o in ops if o.get("widget") != "no CR15VRMenuWidgetCR row"]
assert len(ops) == 19 and not bad, f"widget rows on {bad} (or {len(ops)} ops)"
EOF
rm -f "$OUT.layout.log"
echo "built $OUT"
