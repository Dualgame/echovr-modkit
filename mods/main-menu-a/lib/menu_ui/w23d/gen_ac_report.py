"""W23d: the w23c REPORT PLAYER layer (ffa_ac_report) after the TR polish.

Reads ../w23c/ac_report.json (the w23c canvas_edit op list), changes it, renumbers the appended primitives and writes
TWO op lists, one per platform (same geometry, different colours; never layer a PC tree over a Quest one or the reverse):

    python3 lib/menu_ui/w23d/gen_ac_report.py            # writes ac_report.json (PC) + ac_report_quest.json (Quest)
    python3 lib/menu_ui/w23d/gen_ac_report.py --check    # exit 1 when either committed file is stale
    python3 lib/menu_ui/w23d/gen_ac_report.py --calib    # print the PC colour fit (below) and exit

Changes against w23c (every sprite is still the generic prompt atlas's solid region; no texture / T2 change, no new hash):
  * (1) no empty band under CANCEL / SUBMIT. The face (0..~756 px; PC shot s022_w23c_ac: rule1 108 -> screen 398, rule2
    452 -> 489, the frame's inner edge at screen 569.5 = px ~756) is laid out on Coffey's proportions: black well
    112..528 (sentence, tiles as w23c, the Mute & Ghost slot), rule2 528..532, then a FOOTER band 532..748 in the face
    colour (acr_cover, was black 572..748) with CANCEL 132..492 / SUBMIT 532..892 x 556..640 in it. The buttons stop at
    640 so their zones (560..636) stay clear of the stock footer zone (px 645..731) and menubutton1-4 (672..740), the
    w23c review rule; the footer band below them is filled, not black.
  * (3) the Mute & Ghost row (report_mute_ghost=1 only) sits ABOVE the footer, in the well: acr_mg 330..790 x 452..508
    (children are parent-relative, unchanged); zone ffa_acr_mg moves to (354, 480) (build.sh). With the row hidden (the
    default) its slot is black well.
  * (2) PC colours re-fitted to the in-game tone response; Quest keeps w23c's artboard colours (see PALETTE).
Prim order = draw order (children follow their parent); `parent` refs are remapped by name.
"""
from __future__ import annotations

import importlib.util
import json
import math
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
SRC = HERE.parent / "w23c" / "ac_report.json"
OUTS = {"pc": HERE / "ac_report.json", "quest": HERE / "ac_report_quest.json"}
_spec = importlib.util.spec_from_file_location("w23b_gen", HERE.parent / "w23b" / "gen_ac_report.py")
W23B = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(W23B)

PC_MEASURED = {
    "r": [(0.184, 80), (0.227, 111), (0.353, 163), (0.420, 185), (1.0, 248)],
    "g": [(0.235, 108), (0.396, 167), (0.431, 176), (0.639, 213), (1.0, 240)],
    "b": [(0.263, 131), (0.404, 181), (0.486, 200), (0.678, 230), (1.0, 246)],
}
TOE = math.log(111 / 80) / math.log(0.227 / 0.184)
TARGETS = {
    "slate": (87, 104, 117),
    "cancel": (115, 111, 112),
    "submit": (80, 80, 84),
    "face": (69, 62, 72),
}


def pc_authored(rendered):
    """Invert the PC fit: the authored tint that renders `rendered` (r, g, b), 3 decimals."""
    out = []
    for ch, y in zip("rgb", rendered):
        pts = PC_MEASURED[ch]
        ly = math.log(y)
        if ly <= math.log(pts[0][1]):
            a = pts[0][0] * math.exp((ly - math.log(pts[0][1])) / TOE)
        else:
            for (a0, y0), (a1, y1) in zip(pts, pts[1:]):
                if ly <= math.log(y1):
                    t = (ly - math.log(y0)) / (math.log(y1) - math.log(y0))
                    a = math.exp(math.log(a0) + t * (math.log(a1) - math.log(a0)))
                    break
            else:
                a = 1.0
        out.append(round(a, 3))
    return out + [1.0]


PALETTE = {
    "pc": {"slate": pc_authored(TARGETS["slate"]), "cancel": pc_authored(TARGETS["cancel"]),
           "submit": pc_authored(TARGETS["submit"]), "face": pc_authored(TARGETS["face"]),
           "dots": pc_authored(TARGETS["face"])},
    "quest": {"slate": W23B.SLATE, "cancel": W23B.CANCEL_FILL, "submit": W23B.SUBMIT_FILL,
              "face": [0.173, 0.188, 0.224, 1.0],
              "dots": [0, 0, 0, 1]},
}

WELL, RULE2 = [20, 112, 1004, 528], [20, 528, 1004, 532]
FOOTER = [20, 532, 1004, 748]
CANCEL, SUBMIT = [132, 556, 492, 640], [532, 556, 892, 640]
MG = [330, 452, 790, 508]
STOCK_ZONES_TOP = 645


def transform(bl, pal):
    out = []
    for b in bl:
        n = b["name"]
        out.append(b)
        if n == "acr_well":
            W23B.setop(b, "rect", rect=WELL)
        elif n == "acr_rule2":
            W23B.setop(b, "rect", rect=RULE2)
        elif n == "acr_cover":
            W23B.setop(b, "rect", rect=FOOTER)
            W23B.setop(b, "tint", rgba=pal["face"])
        elif n == "acr_dots_cover":
            W23B.setop(b, "tint", rgba=pal["dots"])
        elif n == "acr_mg":
            W23B.setop(b, "rect", rect=MG)
        elif n.endswith("_fill") and n.startswith("acr_r"):
            W23B.setop(b, "tint", rgba=pal["slate"])
        elif n in ("acr_cancel_bg", "acr_cancel_text", "acr_submit_bg", "acr_submit_text"):
            W23B.setop(b, "rect", rect=CANCEL if n.startswith("acr_cancel") else SUBMIT)
        elif n in ("acr_cancel_fill", "acr_submit_fill"):
            cancel = n.startswith("acr_cancel")
            W23B.setop(b, "rect", rect=W23B.inset(CANCEL if cancel else SUBMIT))
            W23B.setop(b, "tint", rgba=pal["cancel" if cancel else "submit"])
    names = [b["name"] for b in out]
    assert len(set(names)) == len(names), "duplicate element name"
    by = {b["name"]: W23B.rect_of(b) for b in out}
    tiles_bottom = max(by[f"acr_r{i}_bg"][3] for i in range(1, 7))
    assert tiles_bottom < MG[1] and MG[3] < RULE2[1], "Mute & Ghost sits between the tiles and the footer rule"
    assert RULE2[3] == FOOTER[1] and FOOTER[1] < CANCEL[1] and CANCEL[3] <= STOCK_ZONES_TOP - 5, "footer layout"
    assert WELL[3] == RULE2[1], "the well ends at rule2"
    return out


def build(platform):
    return W23B.render(W23B.emit(transform(W23B.blocks(json.loads(SRC.read_text())), PALETTE[platform])))


def main() -> int:
    if "--calib" in sys.argv[1:]:
        print(f"toe slope {TOE:.3f}")
        for k, v in TARGETS.items():
            print(f"{k:7s} rendered {v} -> PC authored {pc_authored(v)[:3]}")
        return 0
    stale = 0
    for platform, path in OUTS.items():
        text_out = build(platform)
        if "--check" in sys.argv[1:]:
            ok = path.exists() and path.read_text() == text_out
            print(path.name, "current" if ok else "STALE")
            stale |= not ok
        else:
            path.write_text(text_out)
            print(f"wrote {path} ({text_out.count(chr(10))} ops)")
    return 1 if stale else 0


if __name__ == "__main__":
    sys.exit(main())
