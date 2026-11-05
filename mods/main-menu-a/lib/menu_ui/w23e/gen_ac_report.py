"""W23e: the w23d REPORT PLAYER layer (ffa_ac_report) refined against Coffey's mockup.

Builds w23d's per-platform block lists (w23d/gen_ac_report.py transform over w23c's JSON), changes them and writes two op
lists (same geometry, different colours; never layer a PC tree over a Quest one, nor a w23e tree beside w23c / w23d):

    python3 lib/menu_ui/w23e/gen_ac_report.py            # writes ac_report.json (PC) + ac_report_quest.json (Quest)
    python3 lib/menu_ui/w23e/gen_ac_report.py --check    # exit 1 when either committed file is stale
    python3 lib/menu_ui/w23e/gen_ac_report.py --preview OUT.png [--atlas ATLAS.png] [--mg] [--pick N] [--quest]
        # offline preview: the face (1024 x 768, the frame edge at y 756) drawn from the op list, zones in red (ours) /
        # yellow (stock), and the mockup scaled to the same face beside it. --atlas = the prompt atlas 0x79563b60c7895d4e
        # as a PNG (sprites then sampled; else drawn as outlines). Approximate: PIL font, no tone curve.

-> 0..1024 (s = 1.51 face px per mockup px), rule1 90..93 -> 108..112. Elements keep the mockup size (isotropic); our
visible face runs to y 756, 105 px taller than the mockup's at s, so the vertical GAPS are stretched (about x 1.5, every
gap below the tiles 34 px, mockup 12..16 -> 18..24): no empty band under the buttons. Changes against w23d:
  * Mute & Ghost: the box was the atlas outline region (uv 0.5176..0.6152: a 4 px stroke in a 200 px sprite) drawn at
    48 px = a 0.96 px stroke on the face = ~0.26 px on the PC screen: it vanished (not colour, alpha or order). Now a solid
    teal square (mockup (5, 200, 193)) + a black inset (5 px border), 62 px; the tick inside (mockup proportions); label
    "Mute & Ghost Reported Player" (rad-menu writes it) size 24 at +87; the pair centred on 512; zone over box + label.
  * "<": the frame was the same outline region (68 px, 1.4 px stroke: corner marks only) and the icon region is an
    opaque grey square (the arrow only in its RGB) -> white square 911..971 x 29..89 + a face-colour inset + a white text
    "<" (size 44); acr_back_icon dropped.
  * CANCEL / SUBMIT: mockup size + x (200 x 76 at 169 / 655), y 646..722 in the footer band 612..748 (34 px above, 34 to
    the frame edge); text 28. Rules 6 px (4 px aliased on PC: half of rule2 vanished in s023_w23d_pc_ac). Tile / button
    borders 4 px (mockup 3 mockup px = 4.5).
  * tiles at the mockup grid: 263 x 106 at x 98 / 379 / 660, y 244 / 370 (w23d 270 x 110, gaps 25), two-line labels 34
    px apart (w23d 41; mockup 22 x 1.51 = 33); sentence centred
    in 114..244; chip 57..421 x 28..108, title 44 (mockup).
"""
from __future__ import annotations

import importlib.util
import json
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
REPO = HERE.parents[2]
OUTS = {"pc": HERE / "ac_report.json", "quest": HERE / "ac_report_quest.json"}
MOCKUP = HERE / "mockup.png"


def _load(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


W23D = _load("w23d_gen", HERE.parent / "w23d" / "gen_ac_report.py")
W23B = W23D.W23B
SOLID = W23B.SOLID

FACE_BOTTOM = 756
RULE1, RULE2 = [20, 108, 1004, 114], [20, 606, 1004, 612]
WELL, FOOTER = [20, 114, 1004, 606], [20, 612, 1004, 748]
CHIP, TITLE_SIZE = [57, 28, 421, 108], 44
BACK, BACK_GLYPH_SIZE = [911, 29, 971, 89], 44
SENT, SENT_L1, SENT_L2 = [28, 144, 996, 216], [28, 146, 996, 182], [28, 182, 996, 218]
COLS, ROWS = [(98, 361), (379, 642), (660, 923)], [(244, 350), (370, 476)]
BORDER = 4
MG = [288, 510, 790, 572]
MG_BOX, MG_BOX_W, MG_CHECK, MG_TEXT = [0, 0, 62, 62], 5, [11, 15, 51, 45], [87, 0, 502, 62]
CANCEL, SUBMIT, BTN_TEXT = [169, 646, 369, 722], [655, 646, 855, 722], 28
TEAL = {"pc": W23D.pc_authored((5, 200, 193)), "quest": [0.02, 0.784, 0.757, 1.0]}


def tile(i):
    """1-based reason tile i -> its rect (Coffey order, 3 x 2)."""
    (x0, x1), (y0, y1) = COLS[(i - 1) % 3], ROWS[(i - 1) // 3]
    return [x0, y0, x1, y1]


LINE_GAP = 34


def _shift_to(r, old_tile, new_tile, part):
    """a label rect on the new tile's x range: a one-line label (t1 = the whole tile) takes the tile; the lines of a
    two-line label sit LINE_GAP / 2 above / below the tile centre, 40 px tall"""
    if part == "t1" and r[3] - r[1] >= 100:
        return list(new_tile)
    c = (new_tile[1] + new_tile[3]) / 2 + (LINE_GAP / 2 if part == "t2" else -LINE_GAP / 2)
    return [new_tile[0], round(c - 20), new_tile[2], round(c + 20)]


def transform(bl, platform, pal):
    by = {b["name"]: b for b in bl}
    old_tiles = {i: W23B.rect_of(by[f"acr_r{i}_bg"]) for i in range(1, 7)}
    out = []
    back_icon = None
    for b in bl:
        n = b["name"]
        if n == "acr_back_icon":
            back_icon = b
            continue
        out.append(b)
        if n == "acr_rule1":
            W23B.setop(b, "rect", rect=RULE1)
        elif n == "acr_rule2":
            W23B.setop(b, "rect", rect=RULE2)
        elif n == "acr_well":
            W23B.setop(b, "rect", rect=WELL)
        elif n == "acr_cover":
            W23B.setop(b, "rect", rect=FOOTER)
        elif n == "acr_chip":
            W23B.setop(b, "rect", rect=CHIP)
        elif n == "acr_title":
            W23B.setop(b, "rect", rect=CHIP)
            W23B.setop(b, "textstyle", align=[0.5, 0.5], size=TITLE_SIZE)
        elif n == "acr_back_frame":
            W23B.setop(b, "rect", rect=BACK)
            W23B.setop(b, "uv", uv=SOLID)
            W23B.setop(b, "tint", rgba=[1, 1, 1, 1])
            out.append(W23B.solid("acr_back_fill", b["parent"], W23B.inset(BACK, BORDER), pal["face"]))
            assert back_icon is not None, "acr_back_icon precedes acr_back_frame in w23d"
            g = W23B.text("acr_back_glyph", b["parent"], W23B.inset(BACK, BORDER), 4, BACK_GLYPH_SIZE)
            W23B.setop(g, "text", text="<", cap=4)
            out.append(g)
        elif n == "acr_sentence":
            W23B.setop(b, "rect", rect=SENT)
        elif n in ("acr_sent_l1", "acr_sent_l2"):
            W23B.setop(b, "rect", rect=SENT_L1 if n.endswith("1") else SENT_L2)
        elif n.startswith("acr_r") and n[5:6].isdigit():
            i, part = int(n[5]), n[7:]
            t = tile(i)
            if part == "bg":
                W23B.setop(b, "rect", rect=t)
            elif part in ("fill", "sel"):
                W23B.setop(b, "rect", rect=W23B.inset(t, BORDER))
            else:
                W23B.setop(b, "rect", rect=_shift_to(W23B.rect_of(b), old_tiles[i], t, part))
        elif n == "acr_mg":
            W23B.setop(b, "rect", rect=MG)
        elif n == "acr_mg_box":
            W23B.setop(b, "rect", rect=MG_BOX)
            W23B.setop(b, "uv", uv=SOLID)
            W23B.setop(b, "tint", rgba=TEAL[platform])
            out.append(W23B.solid("acr_mg_box_in", "acr_mg", W23B.inset(MG_BOX, MG_BOX_W), [0, 0, 0, 1]))
        elif n == "acr_mg_check":
            W23B.setop(b, "rect", rect=MG_CHECK)
        elif n == "acr_mg_text":
            W23B.setop(b, "rect", rect=MG_TEXT)
        elif n in ("acr_cancel_bg", "acr_cancel_text", "acr_submit_bg", "acr_submit_text"):
            W23B.setop(b, "rect", rect=CANCEL if n.startswith("acr_cancel") else SUBMIT)
            if n.endswith("_text"):
                W23B.setop(b, "textstyle", align=[0.5, 0.5], size=BTN_TEXT)
        elif n in ("acr_cancel_fill", "acr_submit_fill"):
            W23B.setop(b, "rect", rect=W23B.inset(CANCEL if n.startswith("acr_cancel") else SUBMIT, BORDER))
    names = [b["name"] for b in out]
    assert len(set(names)) == len(names) == len(bl) + 2, "+acr_back_fill +acr_back_glyph +acr_mg_box_in -acr_back_icon"
    pos = {nm: k for k, nm in enumerate(names)}
    for b in out:
        assert not b.get("parent") or pos[b["parent"]] < pos[b["name"]], f"{b['name']} precedes its parent"
    assert pos["acr_back_frame"] < pos["acr_back_fill"] < pos["acr_back_glyph"], "back: frame, fill, glyph"
    assert pos["acr_mg_box"] < pos["acr_mg_box_in"] < pos["acr_mg_check"] < pos["acr_mg_text"], "mg draw order"
    kids = [k for k, b in enumerate(out) if b.get("parent") == "acr_mg"]
    assert kids == list(range(pos["acr_mg"] + 1, pos["acr_mg"] + 1 + len(kids))), "acr_mg children contiguous"
    rows_bottom = ROWS[-1][1]
    assert RULE1[3] == WELL[1] and WELL[3] == RULE2[1] and RULE2[3] == FOOTER[1], "bands abut"
    assert SENT[3] <= ROWS[0][0] and rows_bottom < MG[1] and MG[1] + MG_BOX[3] < RULE2[1], "well order"
    assert FOOTER[1] < CANCEL[1] and CANCEL[3] < FOOTER[3] <= FACE_BOTTOM, "buttons inside the footer band"
    return out


def build(platform):
    pal = W23D.PALETTE[platform]
    bl = W23D.transform(W23B.blocks(json.loads(W23D.SRC.read_text())), pal)
    return W23B.render(W23B.emit(transform(bl, platform, pal)))


def preview(path, ops_path, atlas=None, mg=False, pick=0):
    from PIL import Image, ImageDraw, ImageFont
    sys.path.insert(0, str(HERE))
    import check_layout as CL
    ops = json.loads(Path(ops_path).read_text())
    prims, parent = {}, {}
    for op in ops:
        if op["op"] == "clone":
            k = 8 + len(prims)
            prims[k] = {"name": op["names"][0]}
            parent[k] = op.get("parent")
    for op in ops:
        if op["op"] != "clone":
            prims[op["prim"]][op["op"]] = op
    hidden = {"acr_mg"} if not mg else {"acr_mg_check"} if mg == "off" else set()
    hidden |= {f"acr_r{i}_sel" for i in range(1, 7) if i != pick}
    hidden |= {"acr_sent_l1", "acr_sent_l2"}
    W, H = 1024, 768
    face = Image.new("RGBA", (W, H), (69, 62, 72, 255))
    at = Image.open(atlas).convert("RGBA") if atlas else None
    font_path = "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"
    texts = {"acr_mg_text": "Mute & Ghost Reported Player",
             "acr_sentence": "AgentSpecialK will be reported for violating the Echo VR Code of Conduct:"}

    def absrect(k):
        r = list(prims[k]["rect"]["rect"])
        p = parent.get(k)
        if p is not None and prims[p]["name"] != "acr_root":
            pr = absrect(p)
            r = [pr[0] + r[0], pr[1] + r[1], pr[0] + r[2], pr[1] + r[3]]
        return r

    def is_hidden(k):
        while k is not None:
            if prims[k]["name"] in hidden:
                return True
            k = parent.get(k)
        return False

    d = ImageDraw.Draw(face)
    for k in sorted(prims):
        p = prims[k]
        if p["name"] == "acr_root" or is_hidden(k) or "rect" not in p:
            continue
        r = [round(v) for v in absrect(k)]
        rgba = p.get("tint", {}).get("rgba", [1, 1, 1, 1])
        col = tuple(int(255 * max(0, min(1, c))) for c in rgba)
        if "text" in p:
            s = texts.get(p["name"], p["text"].get("text", ""))
            if not s:
                continue
            size = p.get("textstyle", {}).get("size", 22)
            f = ImageFont.truetype(font_path, int(size * 0.92))
            al = p.get("textstyle", {}).get("align", [0.5, 0.5])
            tw = d.textlength(s, font=f)
            x = r[0] + (r[2] - r[0] - tw) * al[0]
            d.text((x, (r[1] + r[3]) / 2), s, font=f, fill=col, anchor="lm")
            continue
        if col[3] == 0 or r[2] <= r[0] or r[3] <= r[1]:
            continue
        uv = p.get("uv", {}).get("uv", SOLID)
        if uv == SOLID:
            d.rectangle([r[0], r[1], r[2] - 1, r[3] - 1], fill=col)
        elif at is not None:
            aw, ah = at.size
            spr = at.crop((round(uv[0] * aw), round(uv[1] * ah), round(uv[2] * aw), round(uv[3] * ah)))
            spr = spr.resize((r[2] - r[0], r[3] - r[1]), Image.BILINEAR)
            tint = Image.new("RGBA", spr.size, col)
            spr = Image.composite(tint, Image.new("RGBA", spr.size, (0, 0, 0, 0)), spr.getchannel("A"))
            face.alpha_composite(spr, (r[0], r[1]))
        else:
            d.rectangle([r[0], r[1], r[2] - 1, r[3] - 1], outline=col, width=1)
    for name, z in CL.zones().items():
        d.rectangle([round(v) for v in z], outline=(255, 40, 40, 255), width=2)
    for name, z in CL.STOCK.items():
        d.rectangle(list(z), outline=(255, 220, 0, 255), width=2)
    d.line([0, FACE_BOTTOM, W, FACE_BOTTOM], fill=(255, 0, 255, 255), width=1)
    mock = Image.open(MOCKUP).convert("RGBA")
    s = 1024 / (715 - 37)
    mock = mock.resize((round(mock.width * s), round(mock.height * s)), Image.LANCZOS)
    off = (-round(37 * s), 112 - round(93 * s))
    canvas = Image.new("RGBA", (2 * W + 16, H), (30, 30, 30, 255))
    canvas.alpha_composite(face, (0, 0))
    m2 = Image.new("RGBA", (W, H), (30, 30, 30, 255))
    m2.alpha_composite(mock, (max(0, off[0]), max(0, off[1])), (max(0, -off[0]), max(0, -off[1])))
    ImageDraw.Draw(m2).line([0, FACE_BOTTOM, W, FACE_BOTTOM], fill=(255, 0, 255, 255), width=1)
    canvas.alpha_composite(m2, (W + 16, 0))
    canvas.convert("RGB").save(path)
    print(f"preview {path}")


def main() -> int:
    a = sys.argv[1:]
    if "--preview" in a:
        out = a[a.index("--preview") + 1]
        atlas = a[a.index("--atlas") + 1] if "--atlas" in a else None
        pick = int(a[a.index("--pick") + 1]) if "--pick" in a else 0
        preview(out, OUTS["quest" if "--quest" in a else "pc"], atlas, mg=True if "--mg" in a else False, pick=pick)
        return 0
    stale = 0
    for platform, path in OUTS.items():
        text_out = build(platform)
        if "--check" in a:
            ok = path.exists() and path.read_text() == text_out
            print(path.name, "current" if ok else "STALE")
            stale |= not ok
        else:
            path.write_text(text_out)
            print(f"wrote {path} ({text_out.count(chr(10))} ops)")
    return 1 if stale else 0


if __name__ == "__main__":
    sys.exit(main())
