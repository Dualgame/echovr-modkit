""": the w23a REPORT PLAYER layer (ffa_ac_report) polished toward Coffey's AC artboard.

Reads ../w23a/ac_report.json (the w23a canvas_edit op list), changes it, renumbers the appended primitives and writes
./ac_report.json. Offline, deterministic; run it again after editing either file:

    python3 lib/menu_ui/w23b/gen_ac_report.py            # writes lib/menu_ui/w23b/ac_report.json
    python3 lib/menu_ui/w23b/gen_ac_report.py --check    # exit 1 when the committed file is stale

Changes against w23a (every sprite is still the generic prompt atlas's solid region, no texture / T2 change):
  * tiles acr_r<i>: bg = a solid WHITE rect (the border) + NEW acr_r<i>_fill, a 3 px inset solid SLATE #5a6e7c, drawn
    under acr_r<i>_sel (teal, hidden until a pick) and the labels. w23a drew the prompt's button sprite tinted white =
    light grey.
  * CANCEL: white border + NEW acr_cancel_fill #6b6567 (Coffey's enabled grey), white text.
  * SUBMIT: border + text authored GREY (0.5, rad-menu sets white once a reason is picked, as before) over NEW
    acr_submit_fill #3a3c43 (the footer dark): reads disabled until a pick.
  * title chip 50..454 x 18..104, "REPORT PLAYER" size 48 (w23a: 54..430 x 22..100, 40).
  * sentence: acr_sentence = one centred line, size 22, rect 28..996 x 116..192 (rad-menu writes it when the sentence
    fits RPT_LINE_CHARS); NEW acr_sent_l1 / acr_sent_l2 (cap 72, size 22) = the two-line form rad-menu writes for a
    longer name (name + " will be reported for violating" / "the Echo VR Code of Conduct:"). No engine wordwrap.
Prim order = draw order (children follow their parent); `parent` refs are remapped by name.
"""
from __future__ import annotations

import json
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
SRC, OUT = HERE.parent / "w23a" / "ac_report.json", HERE / "ac_report.json"
SOLID = [0.7087, 0.0942, 0.7283, 0.1058]
WHITE, GREY = [1, 1, 1, 1], [0.5, 0.5, 0.5, 1.0]
SLATE = [0.353, 0.431, 0.486, 1.0]
CANCEL_FILL = [0.420, 0.396, 0.404, 1.0]
SUBMIT_FILL = [0.227, 0.235, 0.263, 1.0]
FIRST = 8


def blocks(ops):
    """[(name, clone_op, [ops on that prim])] in file order, with the w23a index of each."""
    out, idx = [], {}
    for op in ops:
        if op["op"] == "clone":
            out.append({"name": op["names"][0], "clone": dict(op), "ops": [], "old": FIRST + len(out)})
            idx[out[-1]["old"]] = out[-1]
        else:
            idx[op["prim"]]["ops"].append(dict(op))
    for b in out:
        if "parent" in b["clone"]:
            b["parent"] = idx[b["clone"]["parent"]]["name"]
    return out


def solid(name, parent, rect, rgba):
    return {"name": name, "parent": parent, "clone": {"op": "clone", "prim": 0, "names": [name]},
            "ops": [{"op": "rect", "rect": rect}, {"op": "tint", "rgba": rgba}, {"op": "uv", "uv": SOLID}]}


def text(name, parent, rect, cap, size):
    return {"name": name, "parent": parent, "clone": {"op": "clone", "prim": 6, "names": [name]},
            "ops": [{"op": "rect", "rect": rect}, {"op": "text", "text": "", "cap": cap},
                    {"op": "textstyle", "align": [0.5, 0.5], "size": size}, {"op": "tint", "rgba": WHITE}]}


def setop(b, kind, **kv):
    for op in b["ops"]:
        if op["op"] == kind:
            op.update(kv)
            return
    b["ops"].append({"op": kind, **kv})


def inset(r, d=3):
    return [r[0] + d, r[1] + d, r[2] - d, r[3] - d]


def rect_of(b):
    return next(op["rect"] for op in b["ops"] if op["op"] == "rect")


def transform(bl):
    by = {b["name"]: b for b in bl}
    out = []
    for b in bl:
        n = b["name"]
        out.append(b)
        if n in ("acr_chip", "acr_title"):
            setop(b, "rect", rect=[50, 18, 454, 104])
            if n == "acr_title":
                setop(b, "textstyle", align=[0.5, 0.5], size=48)
        elif n == "acr_sentence":
            setop(b, "rect", rect=[28, 116, 996, 192])
            setop(b, "textstyle", align=[0.5, 0.5], size=22)
            out.append(text("acr_sent_l1", "acr_root", [28, 118, 996, 154], 72, 22))
            out.append(text("acr_sent_l2", "acr_root", [28, 154, 996, 190], 72, 22))
        elif n.endswith("_bg") and n.startswith(("acr_r", "acr_cancel", "acr_submit")):
            sub = n.startswith("acr_submit")
            setop(b, "uv", uv=SOLID)
            setop(b, "tint", rgba=GREY if sub else WHITE)
            fill = SLATE if n.startswith("acr_r") else SUBMIT_FILL if sub else CANCEL_FILL
            out.append(solid(n[:-3] + "_fill", b["parent"], inset(rect_of(b)), fill))
        elif n in ("acr_submit_text",):
            setop(b, "tint", rgba=GREY)
    assert len({b["name"] for b in out}) == len(out), "duplicate element name"
    assert all(k in by for k in ("acr_root", "acr_sentence", "acr_r6_sel", "acr_submit_text"))
    return out


def emit(bl):
    pos, ops = {}, []
    for i, b in enumerate(bl):
        pos[b["name"]] = FIRST + i
        c = {k: v for k, v in b["clone"].items() if k != "parent"}
        if b.get("parent"):
            c["parent"] = pos[b["parent"]]
        ops.append(c)
        ops += [{**op, "prim": FIRST + i} for op in b["ops"]]
    return ops


def render(ops):
    return "[" + ",\n ".join(json.dumps(o) for o in ops) + "]\n"


def main() -> int:
    text_out = render(emit(transform(blocks(json.loads(SRC.read_text())))))
    if "--check" in sys.argv[1:]:
        ok = OUT.exists() and OUT.read_text() == text_out
        print("ac_report.json", "current" if ok else "STALE")
        return 0 if ok else 1
    OUT.write_text(text_out)
    print(f"wrote {OUT} ({text_out.count(chr(10))} ops)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
