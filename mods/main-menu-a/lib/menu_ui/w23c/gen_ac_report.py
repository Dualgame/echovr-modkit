"""W23c: the w23b REPORT PLAYER layer (ffa_ac_report) after the review.

Reads ../w23b/ac_report.json (the w23b canvas_edit op list), changes it, renumbers the appended primitives and writes
./ac_report.json. Offline, deterministic; run it again after editing either file:

    python3 lib/menu_ui/w23c/gen_ac_report.py            # writes lib/menu_ui/w23c/ac_report.json
    python3 lib/menu_ui/w23c/gen_ac_report.py --check    # exit 1 when the committed file is stale

Changes against w23b (every sprite is still the generic prompt atlas's solid region; no texture / T2 change, no new hash):
  * M1: acr_root is authored DISABLED (T0 rec+0x10 = 1, canvas_edit op `disabled`; 211 PC-final / 200 Quest-final stock
    prims ship with it), so a stock prompt drawn before rad-menu's first poll, or with no rad-menu at all, stays stock.
    rad-menu shows the root only inside its flow (ShowUIElement writes the same field).
  * m12 (1): acr_dots_cover, a black solid over the row of dots right of the title chip (x 462..894, y 24..100; the
    dots are not one of the canvas's prims, so they are taken to be the background sprite's art: `inferred`).
  * m12 (3): no empty band under the tiles when Mute & Ghost is hidden (the default): the well ends under the tiles
    (112..452, rule2 452..456), the footer moves up to 476..560, and the Mute & Ghost row (shown only with
    report_mute_ghost=1) sits under the footer at 580..636. acr_cover grows to the whole lower face (20..1004 x
    572..748, black) and hides the stock footer bar and its dot row.
  * m12 (2): CANCEL 132..492 and SUBMIT 532..892 (360 px each, centred on 512, 40 px apart), text size 30.
  * n3: the Mute & Ghost label is "Mute & Ghost", the pause string (rad-menu also writes g_rp_mg_label into it).
Prim order = draw order (children follow their parent); `parent` refs are remapped by name.
"""
from __future__ import annotations

import importlib.util
import json
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
SRC, OUT = HERE.parent / "w23b" / "ac_report.json", HERE / "ac_report.json"
_spec = importlib.util.spec_from_file_location("w23b_gen", HERE.parent / "w23b" / "gen_ac_report.py")
W23B = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(W23B)
BLACK = [0, 0, 0, 1]
CANCEL, SUBMIT = [132, 476, 492, 560], [532, 476, 892, 560]
MG = [330, 580, 790, 636]


def transform(bl):
    out = []
    for b in bl:
        n = b["name"]
        out.append(b)
        if n == "acr_root":
            b["ops"].append({"op": "disabled", "on": 1})
        elif n == "acr_well":
            W23B.setop(b, "rect", rect=[20, 112, 1004, 452])
        elif n == "acr_rule2":
            W23B.setop(b, "rect", rect=[20, 452, 1004, 456])
        elif n == "acr_cover":
            W23B.setop(b, "rect", rect=[20, 572, 1004, 748])
            W23B.setop(b, "tint", rgba=BLACK)
            out.append(W23B.solid("acr_dots_cover", "acr_root", [462, 24, 894, 100], BLACK))
        elif n == "acr_mg":
            W23B.setop(b, "rect", rect=MG)
        elif n == "acr_mg_text":
            W23B.setop(b, "text", text="Mute & Ghost", cap=32)
        elif n in ("acr_cancel_bg", "acr_cancel_text", "acr_submit_bg", "acr_submit_text"):
            W23B.setop(b, "rect", rect=CANCEL if n.startswith("acr_cancel") else SUBMIT)
            if n.endswith("_text"):
                W23B.setop(b, "textstyle", align=[0.5, 0.5], size=30)
        elif n in ("acr_cancel_fill", "acr_submit_fill"):
            W23B.setop(b, "rect", rect=W23B.inset(CANCEL if n.startswith("acr_cancel") else SUBMIT))
    names = [b["name"] for b in out]
    assert len(set(names)) == len(names), "duplicate element name"
    assert names.index("acr_dots_cover") < names.index("acr_chip"), "the dots cover draws under the chip"
    return out


def main() -> int:
    text_out = W23B.render(W23B.emit(transform(W23B.blocks(json.loads(SRC.read_text())))))
    if "--check" in sys.argv[1:]:
        ok = OUT.exists() and OUT.read_text() == text_out
        print("ac_report.json", "current" if ok else "STALE")
        return 0 if ok else 1
    OUT.write_text(text_out)
    print(f"wrote {OUT} ({text_out.count(chr(10))} ops)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
