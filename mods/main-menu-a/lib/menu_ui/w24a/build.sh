#!/usr/bin/env bash
set -euo pipefail
REPO="${MAIN_MENU_A_ROOT:?run build_main_menu_a.py}"
BASE=$1; OUT=$2; QBASE=${3:-}; QOUT=${4:-}
for d in "$OUT" "$QOUT"; do case "$d" in */_data/*) echo "refused: $d" >&2; exit 2;; esac; done
case "${OUT%/}" in ""|/) echo "refused: empty or root OUT" >&2; exit 2;; esac
case "$(basename "$OUT")" in ffa_*) ;; *) echo "refused: OUT basename must start with ffa_ ($OUT)" >&2; exit 2;; esac
[ -d "$BASE" ] || { echo "refused: BASE $BASE missing" >&2; exit 2; }
[ ! -e "$OUT" ] || { echo "refused: $OUT exists (never rebuild a tree in place)" >&2; exit 2; }
if [ -n "$QBASE$QOUT" ]; then
  [ -d "$QBASE" ] && [ -n "$QOUT" ] || { echo "refused: QBASE missing or QOUT empty" >&2; exit 2; }
  [ ! -e "$QOUT" ] || { echo "refused: $QOUT exists" >&2; exit 2; }
  case "$(basename "$QOUT")" in ffa_*) ;; *) echo "refused: QOUT basename must start with ffa_" >&2; exit 2;; esac
fi
cd "$REPO/lib"
TOPL=0xd0c2f5fc6b1c66bb; BOTL=0x86dfbf430913e614; RADV_S=0xad6477c0ea361317; RADV_H=0x5ad7052968bc8cbc
pose() { python3 - "$BASE" "$1" <<'PY'
import sys, struct
sys.path[:0] = ["menu_ui", "."]
import menu_layout as ml, ui_actor_mint as uam
from game_store import Basis
from pathlib import Path
b = Basis.named("pcf"); p = Path(sys.argv[1]) / b.tree_rel(b.row("primary", ml.TRANS, "mnu_master"))
d = p.read_bytes() if p.exists() else b.read("primary", ml.TRANS, "mnu_master")
r = uam._rows(d, ml.TREC, int(sys.argv[2], 0)); assert len(r) == 1, r
o = ml.TOP + r[0] * ml.TREC
t = struct.unpack_from("<3f", d, o + 0x30); q = struct.unpack_from("<4f", d, o + 0x20); s = struct.unpack_from("<3f", d, o + 0x3c)
assert s == (1.0, 1.0, 1.0), f"{sys.argv[2]} already scaled {s}"
print(",".join(repr(x) for x in t) + ":" + ",".join(repr(x) for x in q) + ":0.001")
PY
}
RADV_ARGS=(); [ "${W24A_RADV:-keep}" = stock ] && RADV_ARGS=(--pointer-size $RADV_S:3,0.14 --pointer-size $RADV_H:3,0.14)
HDR=0xb4b67254b666dc6a; BANR=0x99af55d93126a17a
hpose() { python3 - "$BASE" "$HDR" "$BANR" <<'PY'
import sys, struct
sys.path[:0] = ["menu_ui", "."]
import menu_layout as ml, ui_actor_mint as uam
from game_store import Basis
from pathlib import Path
b = Basis.named("pcf"); p = Path(sys.argv[1]) / b.tree_rel(b.row("primary", ml.TRANS, "mnu_master"))
d = p.read_bytes() if p.exists() else b.read("primary", ml.TRANS, "mnu_master")
def row(a):
    r = uam._rows(d, ml.TREC, int(a, 0)); assert len(r) == 1, (a, r)
    o = ml.TOP + r[0] * ml.TREC
    return [struct.unpack_from(f, d, o + x) for f, x in (("<3f", 0x30), ("<4f", 0x20), ("<3f", 0x3c))]
(ht, hq, hs), (bt, bq, bs) = row(sys.argv[2]), row(sys.argv[3])
assert hs == bs == (1.0, 1.0, 1.0), f"header/banner already scaled {hs} {bs}"
assert hq == bq and ht[0] == bt[0] == 0.0 and ht[2] < bt[2], (ht, hq, bt, bq)
k = ht[2] / bt[2]
print(f"{bt[0] * k!r},{bt[1] * k!r},{ht[2]!r}:" + ",".join(repr(x) for x in hq) + f":{k!r}")
PY
}
TAB_ARGS=(); [ "${W24A_TAB:-project}" = project ] && TAB_ARGS=(--pose-rows "$HDR:$(hpose)")
python3 menu_ui/menu_layout.py mnu_master --base "$BASE" --out "$OUT" \
    --pose-rows "$TOPL:$(pose $TOPL)" --pose-rows "$BOTL:$(pose $BOTL)" "${RADV_ARGS[@]}" "${TAB_ARGS[@]}" > "$OUT.layout.log"
python3 - "$BASE" "$OUT" "${W24A_RADV:-keep}" "${W24A_TAB:-project}" <<'PY'
import sys, os, filecmp, struct
sys.path[:0] = ["menu_ui", "."]
import menu_layout as ml, ui_actor_mint as uam
from game_store import Basis
from pathlib import Path
b = Basis.named("pcf"); out = Path(sys.argv[2])
d = (out / b.tree_rel(b.row("primary", ml.PMESH, "mnu_master"))).read_bytes()
q = lambda a: d[ml.TOP + uam._rows(d, 80, a)[0] * 80 + 32: ml.TOP + uam._rows(d, 80, a)[0] * 80 + 40]
if sys.argv[3] == "stock":
    assert q(0xad6477c0ea361317) == q(0xc11d245b2fac7825) == q(0x5ad7052968bc8cbc), "quad gate"
if sys.argv[4] == "project":
    rd = lambda c: (out / b.tree_rel(b.row("primary", c, "mnu_master"))).read_bytes()
    T, v = rd(ml.TRANS), uam.adr_view(rd(ml.ADR))
    sga = uam.gsc.read(rd(ml.SCENE))["actors"][1]
    def poses(a):
        r = uam._rows(T, ml.TREC, a); assert len(r) == 1
        o = ml.TOP + r[0] * ml.TREC; i = v["actor_key"].index(a); d2 = v["d2"][i]
        ps = [(struct.unpack_from("<3f", T, o + 0x30), struct.unpack_from("<f", T, o + 0x3c)[0]),
              (struct.unpack_from("<3f", d2, 16), struct.unpack_from("<f", d2, 28)[0])]
        k = uam._sga_rank(v, i)
        if k is not None:
            rec = sga[k * 56:(k + 1) * 56]
            ps.append((struct.unpack_from("<3f", rec, 32), struct.unpack_from("<f", rec, 44)[0]))
        assert all(abs(x - y) < 1e-6 for p in ps for x, y in zip(p[0] + (p[1],), ps[0][0] + (ps[0][1],))), (hex(a), ps)
        return ps[0]
    (ht, hs), (bt, bs) = poses(0xb4b67254b666dc6a), poses(0x99af55d93126a17a)
    k = ht[2] / bt[2]
    assert bs == 1.0 and k < 1 and abs(hs - k) < 1e-6 and all(abs(h - x * k) < 1e-6 for h, x in zip(ht[:2], bt[:2])), (ht, hs, bt)
    print(f"tab gate ok: header t={tuple(round(x, 5) for x in ht)} s={hs:.6f} projects onto banner t={bt}")
base = Path(sys.argv[1]); diff = []
for dp, _, fs in os.walk(out):
    for f in fs:
        p = Path(dp, f); rel = p.relative_to(out); bp = base / rel
        if not bp.exists() or not filecmp.cmp(p, bp, shallow=False):
            diff.append(str(rel))
assert all(r.endswith("ac360e41e4ede056") for r in diff), diff
print(f"gate ok: {len(diff)} mnu_master files changed: {sorted(diff)}")
PY
rm -f "$OUT.layout.log"
echo "built $OUT"
if [ -n "$QOUT" ]; then
  W="$QOUT.work"; rm -rf "$W"; mkdir -p "$W"
  python3 menu_ui/quest_stage.py transform "$BASE" "$W/base" > /dev/null
  python3 menu_ui/quest_stage.py transform "$OUT" "$W/out" > /dev/null
  python3 - "$W/base" "$W/out" "$QBASE" <<'PY'
import sys, os, filecmp
wb, wo, qb = sys.argv[1:]
lv = [os.path.relpath(os.path.join(dp, f), wo) for dp, _, fs in os.walk(wo) for f in fs if f == "f927772b9e2aefb1"]
assert lv, "no mnu_master_lowspec files"
for r in lv:
    assert filecmp.cmp(os.path.join(wb, r), os.path.join(qb, r), shallow=False), f"QBASE {r} != transform(BASE): wrong QBASE"
print("quest gate ok:", len(lv), "level files")
PY
  cp -a "$QBASE" "$QOUT"
  (cd "$W/out" && find . -type f -name f927772b9e2aefb1) | while read -r r; do cp "$W/out/$r" "$QOUT/$r"; done
  rm -rf "$W"
  echo "Quest: $(find "$QOUT" -type f | wc -l) files in $QOUT (merge the qglb twin into a *g tree next)"
fi
