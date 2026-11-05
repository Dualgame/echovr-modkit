#!/bin/sh
set -eu
HERE=$(cd "$(dirname "$0")" && pwd)
REPO="${MAIN_MENU_A_ROOT:?run build_main_menu_a.py}"
BASE=$1; OUT=$2; shift 2
CREDITS=0; SAFE=1
for a in "$@"; do case $a in --credits) CREDITS=1;; --no-safezone) SAFE=0;; *) echo "unknown $a"; exit 2;; esac; done
[ -d "$BASE" ] || { echo "no BASE $BASE"; exit 2; }
if [ -e "$OUT" ] && [ -n "$(ls -A "$OUT")" ]; then echo "OUT $OUT not empty"; exit 2; fi
CV=primary/59a9bd6e4525ecc4/v6500253412
EQ=0xa81caf26df3e7e4c; STORE=0xf769a426b03d60b3; SEASON=0x1d47c73d4d217518; CRED=0x2eb1c2f8417f7141
EQC=d5b560c614d21d60; STOREC=05770c47bd6a32f5; SEASONC=1150841b958f1403; CREDC=bedd912d40161179
set -- --canvas-rebind $EQ:0x$EQC --canvas-rebind $STORE:0x$STOREC --canvas-rebind $SEASON:0x$SEASONC
FILES="$EQC $STOREC $SEASONC"
if [ $CREDITS = 1 ]; then set -- "$@" --canvas-rebind $CRED:0x$CREDC; FILES="$FILES $CREDC"; fi
python3 "$REPO/lib/menu_ui/menu_layout.py" mnu_master --base "$BASE" --out "$OUT" "$@" \
  | python3 -c "import sys,json; j=json.load(sys.stdin); print('menu_layout:', [o for o in j['ops']], 'copied', len(j['copied_from_base']))"
mkdir -p "$OUT/$CV"
for f in $FILES; do cp "$HERE/out/$CV/$f" "$OUT/$CV/$f"; echo "canvas $f"; done
if [ $SAFE = 1 ]; then
  PAGE=$(ls "$OUT"/primary/4a3fe67c1ff9ccc4/*/ac360e41e4ede056)
  python3 "$HERE/card_sprite.py" "$PAGE" "$PAGE.new" 0x6bead65a6955c109 58 tile_lobby && mv "$PAGE.new" "$PAGE"
fi
echo "done: $OUT"
