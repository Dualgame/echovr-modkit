"""W23e offline layout check: the press zones in build.sh's ZONES against the canvas (ac_report.json) and the stock zones.

    python3 lib/menu_ui/w23e/check_layout.py      # exit 1 on any failure; build.sh / quest_twin.sh run it

Checks (px on the 1024 x 768 face; actor metres -> px: x = m * 1024 / 0.3, y = -m * 1024 / 0.3; a zone
without half-extents is the BackButton default 0.01 m = 34.1 px):
  * each ffa_acr_* zone's centre lies inside its element (reason i -> acr_r<i>_bg, mg -> box + label, cancel / submit ->
    acr_{cancel,submit}_bg) and no zone sticks out of its element; CANCEL / SUBMIT / mg zones cover >= 80 % of it, the
    reason zones >= 70 % (tile 3 is cut under the stock zone 0x6567539d25efbdc0);
  * no two ffa_acr_* zones overlap; every zone lies above the frame edge (y 756);
  * no ffa_acr_* zone overlaps a BLOCKING stock zone: the footer 0x5071f3ac46630320 (398..651 x 645..731;
    its stock CANCEL route stays live) and 0x6567539d25efbdc0 ((899, 205) +- 54);
  * menubutton1-4 (x 374/579/784/170 + 68, y 672..740) may lie under CANCEL / SUBMIT only (w23e): rad-menu consumes their
    g_rpt_mb_name and fails when they are missing (an older rad-menu would let the stock button run too).
Both platforms share the geometry (gen_ac_report.py), so the PC file is the one read.
"""
from __future__ import annotations

import json
import re
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
RADMENU = HERE.parents[3] / "rad-menu/src/report_flow.h"
PX = 1024 / 0.3
DEFAULT_HALF = 0.01
FACE_BOTTOM = 756
BLOCKING = {
    "footer 0x5071f3ac46630320": (398, 645, 651, 731),
    "0x6567539d25efbdc0": (845, 151, 953, 259),
}
CONSUMED = {
    "menubutton1": (374, 672, 442, 740), "menubutton2": (579, 672, 647, 740),
    "menubutton3": (784, 672, 852, 740), "menubutton4": (170, 672, 238, 740),
}
STOCK = {**BLOCKING, **CONSUMED}


def zones():
    out = {}
    for m in re.finditer(r"--add BackButton=(ffa_acr_\w+)@([-\d.]+),([-\d.]+),[-\d.]+(?:/([-\d.]+),([-\d.]+),[-\d.]+)?",
                         (HERE / "build.sh").read_text()):
        name, x, y = m.group(1), float(m.group(2)) * PX, -float(m.group(3)) * PX
        hx = float(m.group(4) or DEFAULT_HALF) * PX
        hy = float(m.group(5) or DEFAULT_HALF) * PX
        out[name] = (x - hx, y - hy, x + hx, y + hy)
    return out


def rects():
    ops = json.loads((HERE / "ac_report.json").read_text())
    names, out = {}, {}
    for op in ops:
        if op["op"] == "clone":
            names[8 + len(names)] = (op["names"][0], op.get("parent"))
    for op in ops:
        if op["op"] == "rect":
            out[names[op["prim"]][0]] = list(op["rect"])
    mg = out["acr_mg"]
    for k in ("acr_mg_box", "acr_mg_text"):
        r = out[k]
        out[k + "_abs"] = [mg[0] + r[0], mg[1] + r[1], mg[0] + r[2], mg[1] + r[3]]
    b, t = out["acr_mg_box_abs"], out["acr_mg_text_abs"]
    lab_w = 28 * 0.567 * 24
    out["acr_mg_pair"] = [b[0], min(b[1], t[1]), round(t[0] + lab_w), max(b[3], t[3])]
    return out


def overlap(a, b):
    return a[0] < b[2] and b[0] < a[2] and a[1] < b[3] and b[1] < a[3]


def inside(z, r, tol=1.0):
    return z[0] >= r[0] - tol and z[1] >= r[1] - tol and z[2] <= r[2] + tol and z[3] <= r[3] + tol


def area(r):
    return max(0, r[2] - r[0]) * max(0, r[3] - r[1])


def consumed_in_radmenu():
    try:
        src = RADMENU.read_text()
    except OSError:
        return False
    m = re.search(r"g_rpt_mb_name\[RPT_NMB\]\s*=\s*\{([^}]*)\}", src)
    return bool(m) and all(f'"menubutton{i}"' in m.group(1) for i in range(1, 5))


def main() -> int:
    z, r, bad = zones(), rects(), []
    owner = {f"ffa_acr_reason{i}": f"acr_r{i}_bg" for i in range(1, 7)}
    owner.update({"ffa_acr_cancel": "acr_cancel_bg", "ffa_acr_submit": "acr_submit_bg", "ffa_acr_mg": "acr_mg_pair"})
    if len(z) != 10:
        bad.append(f"expected 10 zones, read {len(z)}")
    for name, el in owner.items():
        zb, rb = z[name], r[el]
        cx, cy = (zb[0] + zb[2]) / 2, (zb[1] + zb[3]) / 2
        if not (rb[0] <= cx <= rb[2] and rb[1] <= cy <= rb[3]):
            bad.append(f"{name} centre ({cx:.0f}, {cy:.0f}) outside {el} {rb}")
        if not inside(zb, rb, tol=6.0 if name == "ffa_acr_mg" else 1.0):
            bad.append(f"{name} {[round(v) for v in zb]} sticks out of {el} {rb}")
        cover = area(zb) / area(rb)
        need = 0.70 if name.startswith("ffa_acr_reason") else 0.80
        if cover < need:
            bad.append(f"{name} covers {cover:.0%} of {el} (< {need:.0%})")
    names = sorted(z)
    consumed_ok = consumed_in_radmenu()
    info = []
    for i, a in enumerate(names):
        if z[a][3] > FACE_BOTTOM:
            bad.append(f"{a} runs under the frame edge ({z[a][3]:.0f} > {FACE_BOTTOM})")
        for b in names[i + 1:]:
            if overlap(z[a], z[b]):
                bad.append(f"{a} overlaps {b}")
        for s, sb in STOCK.items():
            if not overlap(z[a], sb):
                continue
            if s in CONSUMED and a in ("ffa_acr_cancel", "ffa_acr_submit") and consumed_ok:
                info.append(f"{a} over stock {s} {sb}: consumed by rad-menu while the panel is up")
            else:
                bad.append(f"{a} {[round(v) for v in z[a]]} overlaps stock {s} {sb}"
                           + (" (rad-menu report_flow.h lacks the menubutton consumption)" if s in CONSUMED else ""))
    for name in ("ffa_acr_cancel", "ffa_acr_submit", "ffa_acr_mg"):
        print(f"{name:16s} px {[round(v) for v in z[name]]}")
    for line in info:
        print("INFO", line)
    for line in bad:
        print("FAIL", line)
    print("layout", "ok" if not bad else f"{len(bad)} failure(s)")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
