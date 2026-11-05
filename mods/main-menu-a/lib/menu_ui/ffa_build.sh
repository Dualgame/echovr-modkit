#!/usr/bin/env bash
set -euo pipefail
REPO="${MAIN_MENU_A_ROOT:?run build_main_menu_a.py}"
ROOT="${MAIN_MENU_A_OUT_ROOT:?run build_main_menu_a.py}"
NAME=ffa_w0; QNAME=""; QREF=""; FORCE=0; REF=""; KEEP=0; STAGE=w0; W18_REV=b; W23_REV=e
while [ $# -gt 0 ]; do
  case $1 in
    --force) FORCE=1;; --keep-work) KEEP=1;;
    --ref) REF=$2; shift;;
    --quest) QNAME=$2; shift;; --qref) QREF=$2; shift;;
    --stage) STAGE=$2; shift;;
    -h|--help) echo "run build_main_menu_a.py --help"; exit 0;;
    -*) echo "unknown $1" >&2; exit 2;;
    *) NAME=$1;;
  esac; shift
done
[ "$STAGE" = latest ] && STAGE=w24b
case $STAGE in w0) RANK=0;; w9) RANK=1;; w8) RANK=2;; w7) RANK=3;; w10) RANK=4;; w11) RANK=5;; w12a) RANK=6;; w12b) RANK=7;; w13a) RANK=8;; w14a) RANK=9;; w15a) RANK=10;; w17a) RANK=11;; w18a) RANK=12; W18_REV=a;; w18b) RANK=12; W18_REV=b;; w18c) RANK=12; W18_REV=c;; w19a) RANK=13; W18_REV=b;; w19c) RANK=13; W18_REV=c;;
  w22a) RANK=14; W18_REV=c;; w23[a-f]) RANK=15; W18_REV=c; W23_REV=${STAGE#w23};; w24a) RANK=16; W18_REV=c; W23_REV=e;; w24b) RANK=16; W18_REV=c; W23_REV=f;;
  *) echo "unknown --stage $STAGE (w0, w9, w8, w7, w10, w11, w12a, w12b, w13a, w14a, w15a, w17a, w18a, w18b, w18c, w19a, w19c, w22a, w23a .. w23f, w24a, w24b, latest)" >&2; exit 2;; esac
for n in "$NAME" ${QNAME:+"$QNAME"}; do
  if { [[ $n =~ ^ffa_(b1|hm|w[1-9]|p[0-9]|q[0-9]) ]] && [[ ! $n =~ ^ffa_(w9b|w8a|w8b|w10b|w11b|w12a|w12b|w13a|w14a|w15a|w17a|w18a|w18b|w18c|w19a|w19c|w22a|w23[a-f]|w24a|w24b)$ ]]; } || [[ $n == */* ]]; then echo "refusing NAME $n (existing chain tree or path)" >&2; exit 2; fi
done
[ "$NAME" != "$QNAME" ] || { echo "refusing: --quest QNAME == NAME" >&2; exit 2; }
FINAL="$ROOT/$NAME"; WORK="$ROOT/.$NAME.work"; QFINAL=${QNAME:+$ROOT/$QNAME}
for f in "$FINAL" ${QFINAL:+"$QFINAL"}; do
  if [ -e "$f" ]; then
    [ $FORCE = 1 ] || { echo "$f exists (use --force)" >&2; exit 2; }
    rm -rf "$f"
  fi
done
rm -rf "$WORK"; mkdir -p "$WORK"
cd "$REPO/lib"
LOG="$WORK/build.log"; : > "$LOG"
T0=$(date +%s)

HOMEO=0x05ce113632359ce3; P30=0x05eb6c644ad6ef27; P58=0xe73618208c520c8d; SPARE=0xcb17bbfd14e80d86
SET=0x623b5f0e1e11a695; HELP=0x71f7c477f0619237; ONL=0x7593d0177b013e69; QUIT=0x720d06f0ec096825
TUT=0xd763e348e835f249; PLAY=0xa3d3ef315cb999db; CREATE=0xff5375e5a4dee29d; JOIN=0x097b785350a61c5f
NEWS=0x27dfdb284b22a843; COMM=0x12bfda55b0b0af9f; SEASONT=0x9e0bf5fe92aba570; ANC_C=0x5502bbb6f4ae1862; ANC_S=0xc3aff9abe56dfae0
CHLOBBY=0xe28948c2885a18c1; PLAYGR=0x54ab073795636abf; FRLIST=0x1b71f837dc9e5669; FRHDR=0xe2b8df2df8ceab62; FRROW=0xecce8089fb0c0501
EQ=0xa81caf26df3e7e4c; STORE=0xf769a426b03d60b3; SEASONP=0x1d47c73d4d217518; REPORT=0x1d47c63d5c3d6802; LEGAL=0xf769a439a13573ba; CRED=0x2eb1c2f8417f7141
SAFE=0x6bead65a6955c109
Q4=0,0.9848077,0,0.17364818; Q2=0,0.9981348,0,0.06104854

ml() { local b=$1 o=$2; shift 2; python3 menu_ui/menu_layout.py mnu_master --base "$b" --out "$o" "$@" >> "$LOG"; }
mint() { local b=$1 o=$2; shift 2; python3 menu_ui/ui_actor_mint.py mnu_master --base "$b" --out "$o" "$@" >> "$LOG"; }
card() { local b=$1 o=$2; shift 2; python3 menu_ui/page_patch.py mnu_master --base "$b" --out "$o" "$@" >> "$LOG"; }
clone() { echo --clone-module $P58:27:$1 --clone-bind $1:eula_page=p24 --clone-bind $1:credits_page=p58 \
  --clone-bind $1:invite_friends_button=$SPARE --clone-bind $1:group_select_page=$2 --clone-bind $1:change_group_button=$3 \
  --clone-bind $1:tutorial_card=$SPARE --clone-bind $1:play_card=$SPARE --clone-bind $1:join_card=$SPARE --clone-bind $1:create_card=$SPARE; }

python3 menu_ui/page_add.py mnu_master --name ffa_help_page --out "$WORK/hm2p" >> "$LOG"
mint "$WORK/hm2p" "$WORK/hm2" --template 0x2d50833b4b45e363 --name ffa_help_page --actor-only

mint "$WORK/hm2" "$WORK/hm3a" --template $FRLIST --body-from mnu_master_mp_ingame:0xca760b3376a22fc9 --name ffa_help_tile_test \
  --canvas ui_mnu_card_third_button --trans 4.1 -1.95 4.4 --page 58 --label "EARLY QUIT" --text-element buttontext --highlight-element buttonhighlight
python3 menu_ui/page_add.py mnu_master --name ffa_help_page --base "$WORK/hm3a" --module-only --module-from 27 \
  --action $EQ:0x791142542a3a6dc5 --out "$WORK/hm3" >> "$LOG"

ml "$WORK/hm3" "$WORK/w1a" \
  --clone-module $P30:27:railscript --clone-bind railscript:eula_page=p24 --clone-bind railscript:credits_page=p58 --clone-bind railscript:invite_friends_button=$SPARE \
  --bind $HOMEO:eula_page=p24 --bind $HOMEO:credits_page=p58 --bind $HOMEO:invite_friends_button=$SPARE \
  --bind $P58/pageactionscript:eula_page=p24 --bind $P58/pageactionscript:credits_page=p58 --bind $P58/pageactionscript:invite_friends_button=$SPARE \
  --text "$ONL=ONLINE" --text "$HELP=HELP" --tooltip "$ONL=Play online" --tooltip "$HELP=Help, tutorials and info" \
  --trans $CHLOBBY:-2.15,0.62,5.0 --trans $PLAYGR:-2.15,0.84,5.0 --trans $ONL:4.1,0.391,4.4 --trans $QUIT:4.1,-0.749,4.4 \
  --member $SET:30 --member $HELP:30 --member $ONL:30 --member $QUIT:30 --member $SET:58 --member $HELP:58 --member $ONL:58 --member $QUIT:58 \
  --unmember $TUT:24 --unmember 0x7131d456929f96db:30 --unmember 0x7131d456929f96db:58
ml "$WORK/w1a" "$WORK/w1t" --trans $PLAY:2.15,-0.275,5.53 --trans $CREATE:0.73,-0.275,5.817 --trans $JOIN:-0.73,-0.275,5.816
card "$WORK/w1t" "$WORK/w1" 24 --card $CREATE tile_create "CREATE GAME" "" "Create a private match" \
  --card $JOIN tile_join "JOIN VIA CODE" "" "Join a match with an invite code"
CT=primary/92abd3e1432bf5e8/v7790901203/ac360e41e4ede056
cp "$WORK/w1t/$CT" "$WORK/w1/$CT"
python3 ctransformcr_set_trans.py "$WORK/w1/$CT" --apply --bak-suffix .orig \
  --set $PLAY:2.15,-0.275,5.53:0,0.9848077,0,-0.17364818 --set $CREATE:0.73,-0.275,5.817:0,0.9981348,0,-0.06104854 \
  --set $JOIN:-0.73,-0.275,5.816:0,0.9981348,0,0.06104854 >> "$LOG"
rm -f "$WORK/w1/$CT.orig"

mint "$WORK/w1" "$WORK/w2a" --template $SET --name ffa_friends_header --trans -4.1 0.391 4.4 --page 24 --label FRIENDS
mint "$WORK/w2a" "$WORK/w2b" --template $SET --name ffa_friends_row1 --trans -4.1 0.013 4.4 --page 24 --label "NONE ONLINE"
HQ=0,0.914,0,0.407
ml "$WORK/w2b" "$WORK/w2c" \
  --tooltip "$FRHDR= " --tooltip "$FRROW=Friends online appear here" --text "$FRLIST=INVITE FRIENDS" --tooltip "$FRLIST=Invite friends to your lobby" \
  --pose $FRHDR:-4.1,0.391,4.4:$HQ --pose $FRROW:-4.1,0.013,4.4:$HQ --pose $FRLIST:-4.1,-0.366,4.4:$HQ \
  --pose $NEWS:-2.15,0.32,5.53:$Q4:1 --pose $COMM:-2.15,-0.4,5.53:$Q4:1 --pose $SEASONT:-2.15,0.32,5.6:$Q4:0.01 \
  --pose $ANC_S:-2.15,0.32,5.6:$Q4 --pose $ANC_C:-2.15,-0.4,5.53:$Q4 \
  --pose $CHLOBBY:-2.15,0.75,5.53:$Q4 --pose $PLAYGR:-2.15,0.97,5.53:$Q4 \
  --canvas-scale $NEWS:0.56 --canvas-scale $COMM:0.56 \
  --pose $PLAY:2.15,-0.275,5.53:0,0.9848077,0,-0.17364818 --pose $CREATE:0.73,-0.275,5.817:0,0.9981348,0,-0.06104854 \
  --pose $JOIN:-0.73,-0.275,5.816:0,0.9981348,0,0.06104854
echo '[{"op":"text","prim":4,"text":"JOIN THE COMMUNITY!"}]' > "$WORK/discord.json"
cp -r "$WORK/w2c" "$WORK/w2"
python3 menu_ui/canvas_edit.py emit ui_mnu_discord --edits "$WORK/discord.json" --out "$WORK/w2" >> "$LOG"

prev="$WORK/w2"; i=0
for spec in "ffa_help_store:ECHO VR STORE:-2.15 0.3 5.53" "ffa_help_season:SEASON PASS:-0.73 -0.25 5.816" \
            "ffa_help_report:REPORT ISSUE:-2.15 -0.25 5.53" "ffa_help_legal:LEGAL:-0.73 -0.8 5.816" "ffa_help_credits:CREDITS:-2.15 -0.8 5.53"; do
  IFS=: read -r nm lab pos <<<"$spec"; i=$((i+1))
  mint "$prev" "$WORK/w5m$i" --template $SET --body-from mnu_master_mp_ingame:0xca760b3376a22fc9 --name $nm \
    --canvas ui_mnu_card_third_button --trans $pos --page 58 --label "$lab" --text-element buttontext --highlight-element buttonhighlight
  prev="$WORK/w5m$i"
done
card "$prev" "$WORK/w5pp" 58 --card $TUT tile_tutorial TUTORIAL "" "Learn the basics"
ml "$WORK/w5pp" "$WORK/w5e" \
  --bind $HOMEO:eula_page=p32 $(clone help_legal p32 $LEGAL) $(clone help_credits p31 $CRED) $(clone help_report p6 $REPORT) --bind $HOMEO:eula_page=p24 \
  --pose $TUT:1.45,-0.275,5.7:0,0.99144,0,-0.13053 --pose $EQ:-0.73,0.3,5.816:$Q2:1 \
  --pose $STORE:-2.15,0.3,5.53:$Q4:1 --pose $SEASONP:-0.73,-0.25,5.816:$Q2:1 --pose $REPORT:-2.15,-0.25,5.53:$Q4:1 \
  --pose $LEGAL:-0.73,-0.8,5.816:$Q2:1 --pose $CRED:-2.15,-0.8,5.53:$Q4:1

ml "$WORK/w5e" "$WORK/w6a" --text "$FRLIST=INVITE" \
  --pose $ANC_C:-2.15,-1.01,5.53:$Q4 --pose $COMM:-2.15,-1.01,5.53:$Q4 --pose $NEWS:-2.15,0.34,5.53:$Q4 \
  --pose $CHLOBBY:-2.3,0.87,5.53:$Q4 --pose $FRLIST:-4.1,-0.75,4.4:$HQ --pose $TUT:2.15,-0.275,5.53:0,0.9848077,0,-0.17364818 \
  --pose $PLAYGR:-2.3,1.09,5.53:$Q4 --pose $EQ:-0.73,0.35,5.816:$Q2 --pose $FRHDR:-4.1,0.66,4.4:$HQ --pose $FRROW:-4.1,0.4,4.4:$HQ \
  --pose $STORE:-0.73,-0.275,5.816:$Q2 --pose $SEASONP:-0.73,-0.9,5.816:$Q2 --pose $REPORT:-2.15,0.35,5.53:$Q4 \
  --pose $LEGAL:-2.15,-0.275,5.53:$Q4 --pose $CRED:-2.15,-0.9,5.53:$Q4

mint "$WORK/w6a" "$WORK/w6b_a" --template $TUT --name ffa_help_safezone --trans 0.73 -0.275 5.817
card "$WORK/w6b_a" "$WORK/w6b_b" 58 --card $SAFE 0x22e4981216f28162 "SAFE ZONE" "" "Coming soon"
ml "$WORK/w6b_b" "$WORK/w6b" --blackboard-from $TUT:$SAFE --pose $SAFE:0.73,-0.275,5.817:0,0.9981348,0,-0.06104854

FRAME=0xe4b590687a34ec60; FOOT=0x7da8ba854cd99a2b; RADV=0xc11d245b2fac7825
ml "$WORK/w6b" "$WORK/w6c" --member $FRAME:58 --member $FOOT:58 --member $RADV:58 --member $FRAME:30 --member $FOOT:30 --member $RADV:30

mint "$WORK/w6c" "$WORK/w6d_a" --template $FOOT --name ffa_status_help --trans 1.406 -1.5 5.821 --page 58
mint "$WORK/w6d_a" "$WORK/w6d_b" --template $FOOT --name ffa_status_settings --trans 1.406 -1.5 5.821 --page 30
ml "$WORK/w6d_b" "$WORK/w6d" --unmember $FOOT:58 --unmember $RADV:58 --unmember $FOOT:30 --unmember $RADV:30

cat > "$WORK/news.json" <<'EOF'
[{"op":"design","size":[640,672],"rt":[1024,1076]},
 {"op":"rect","prim":0,"rect":[0,0,640,505]}, {"op":"rect","prim":1,"rect":[0,0,640,504]},
 {"op":"uv","prim":1,"uv":[0.18254,0,0.81746,1]},
 {"op":"rect","prim":2,"rect":[0,0,640,672]}, {"op":"uv","prim":2,"uv":[0.46875,0.662109,0.625,0.824219]},
 {"op":"rect","prim":3,"rect":[0,0,640,672]}, {"op":"uv","prim":3,"uv":[0.3125,0.662109,0.46875,0.824219]},
 {"op":"rect","prim":4,"rect":[20,512,620,656]}]
EOF
ml "$WORK/w6d" "$WORK/w6e" --canvas-scale $NEWS:1.1137 --canvas-scale $COMM:1.1137 --pointer-aspect $NEWS:1.05 \
  --pose $NEWS:-2.15,-0.06,5.53:$Q4 --pose $COMM:-2.15,-0.99,5.53:$Q4 --pose $ANC_C:-2.15,-0.99,5.53:$Q4
python3 menu_ui/canvas_edit.py emit ui_mnu_news_info --edits "$WORK/news.json" --out "$WORK/w6e" >> "$LOG"

echo '[{"op":"tint","prim":4,"rgba":[1,1,1,0]}]' > "$WORK/noicon.json"
python3 menu_ui/canvas_edit.py emit ui_mnu_card_third_button --edits "$WORK/noicon.json" --out "$WORK/w6f_a" --as ffa_help_card_third_button_noicon >> "$LOG"
rb=(); for a in $EQ $STORE $SEASONP $REPORT $LEGAL $CRED; do rb+=(--canvas-rebind $a:ffa_help_card_third_button_noicon); done
ml "$WORK/w6e" "$WORK/w6f" "${rb[@]}"
CV=primary/59a9bd6e4525ecc4/v6500253412
cp "$WORK/w6f_a/$CV/170ef2ff50b4695e" "$WORK/w6f/$CV/"

bash menu_ui/friends_panel/build.sh "$WORK/w6f" "$WORK/w6h" >> "$LOG"
python3 menu_ui/help_tiles/build.py menu_ui/help_tiles/out >> "$LOG"
sh menu_ui/help_tiles/apply.sh "$WORK/w6h" "$WORK/final" >> "$LOG"

if [ $RANK -ge 1 ]; then
  mv "$WORK/final" "$WORK/w0"
  bash menu_ui/news_dots/build.sh "$WORK/w0" "$WORK/final" >> "$LOG"
fi
if [ $RANK -ge 2 ]; then
  mv "$WORK/final" "$WORK/w9"
  bash menu_ui/settings_tab/build.sh "$WORK/w9" "$WORK/final" >> "$LOG"
fi
if [ $RANK -ge 3 ]; then
  mv "$WORK/final" "$WORK/w8"
  bash menu_ui/friends_data/build.sh "$WORK/w8" "$WORK/final" >> "$LOG"
fi
if [ $RANK -ge 4 ]; then
  mv "$WORK/final" "$WORK/w7"
  bash menu_ui/invite_row/build.sh "$WORK/w7" "$WORK/final" >> "$LOG"
fi
if [ $RANK -ge 5 ]; then
  mv "$WORK/final" "$WORK/w10"
  bash menu_ui/polish/build.sh "$WORK/w10" "$WORK/final" >> "$LOG"
fi
if [ $RANK -ge 6 ]; then
  mv "$WORK/final" "$WORK/w11"
  bash menu_ui/w12a/build.sh "$WORK/w11" "$WORK/final" >> "$LOG"
fi
if [ $RANK -ge 7 ]; then
  mv "$WORK/final" "$WORK/w12a"
  bash menu_ui/w12b/build.sh "$WORK/w12a" "$WORK/final" >> "$LOG"
fi
if [ $RANK -ge 8 ]; then
  mv "$WORK/final" "$WORK/w12b"
  bash menu_ui/w13a/build.sh "$WORK/w12b" "$WORK/final" >> "$LOG"
fi
if [ $RANK -ge 9 ]; then
  mv "$WORK/final" "$WORK/w13a"
  bash menu_ui/w14a/build.sh "$WORK/w13a" "$WORK/final" >> "$LOG"
fi
if [ $RANK -ge 10 ]; then
  mv "$WORK/final" "$WORK/w14a"
  bash menu_ui/w15a/build.sh "$WORK/w14a" "$WORK/final" >> "$LOG"
fi
if [ $RANK -ge 11 ]; then
  mv "$WORK/final" "$WORK/w15a"
  bash menu_ui/w17a/build.sh "$WORK/w15a" "$WORK/final" >> "$LOG"
fi
if [ $RANK -ge 12 ]; then
  mv "$WORK/final" "$WORK/w17a"
  W18_REV=$W18_REV bash menu_ui/w18a/build.sh "$WORK/w17a" "$WORK/final" >> "$LOG"
fi
if [ $RANK -ge 13 ]; then
  mv "$WORK/final" "$WORK/w18$W18_REV"
  bash menu_ui/w19a/build.sh "$WORK/w18$W18_REV" "$WORK/final" >> "$LOG"
fi
if [ $RANK -ge 14 ]; then
  mv "$WORK/final" "$WORK/w19c"
  bash menu_ui/w22a/build.sh "$WORK/w19c" "$WORK/ffa_w22a" >> "$LOG"
  mv "$WORK/ffa_w22a" "$WORK/final"
fi
if [ $RANK -ge 15 ]; then
  mv "$WORK/final" "$WORK/w22a"
  bash menu_ui/w23$W23_REV/build.sh "$WORK/w22a" "$WORK/ffa_w23$W23_REV" >> "$LOG"
  mv "$WORK/ffa_w23$W23_REV" "$WORK/final"
fi
if [ $RANK -ge 16 ]; then
  mv "$WORK/final" "$WORK/w23$W23_REV"
  W24A_TAB=project W24A_RADV=keep bash menu_ui/w24a/build.sh "$WORK/w23$W23_REV" "$WORK/ffa_w24a" >> "$LOG"
  mv "$WORK/ffa_w24a" "$WORK/final"
fi

mv "$WORK/final" "$FINAL"
cmp_tree() {
  diff -rq "$1" "$2" && echo "diff -rq: identical to $1"
  local nf=0; while IFS= read -r f; do cmp -s "$1/$f" "$2/$f" || { echo "DIFFERS $f"; exit 1; }; nf=$((nf+1)); done < <(cd "$1" && find . -type f)
  echo "cmp: $nf files byte-identical"
}
[ -z "$REF" ] || cmp_tree "$REF" "$FINAL"

if [ -n "$QNAME" ]; then
  QW="$WORK/quest"; rm -rf "$QW"; mkdir -p "$QW"
  python3 menu_ui/quest_stage.py transform "$FINAL" "$QW/tree" >> "$LOG"
  find "$QW/tree" -type f \( -name 0dc32069c96e74a1 -o -name 3f9915d3001dc28e -o -name 90950ed3107711a6 \) -delete
  if [ $RANK -ge 11 ]; then
    TW="$QW/ffa_qglb_card"; bash menu_ui/w17a/quest_twin.sh "$TW" >> "$LOG"
    if [ $RANK -ge 15 ]; then bash menu_ui/w23$W23_REV/quest_twin.sh "$TW" "$QW/ffa_qglb_report" >> "$LOG"; TW="$QW/ffa_qglb_report"; fi
    (cd "$TW" && find . -type f) | while IFS= read -r f; do
      [ ! -e "$QW/tree/$f" ] || { echo "refused: Quest tree already has $f" >&2; exit 1; }; done
    cp -a "$TW/." "$QW/tree/"
  fi
  mv "$QW/tree" "$QFINAL"
  echo "built $QFINAL ($(find "$QFINAL" -type f | wc -l) files)"
  [ -z "$QREF" ] || cmp_tree "$QREF" "$QFINAL"
fi
[ $KEEP = 1 ] || rm -rf "$WORK" 2>/dev/null || true
echo "built $FINAL ($(find "$FINAL" -type f | wc -l) files, $(( $(date +%s) - T0 )) s)"
