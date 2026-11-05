"""Edit a shipped `CUICanvasResource` and emit it as a loose-tree override (web-viewer tier DW0).

The core (`apply`, `check`, `diff`) imports only `resource_io` and the stdlib, so the browser viewer can run
this same file under Pyodide (feasibility § 5: every emit goes through one codec). Ops, as JSON objects:

  {"op": "tint", "prim": N, "rgba": [r, g, b, a]}      T0 rec+0x78, 4 f32, authored sRGB (gui_vs decodes it)
  {"op": "rect", "prim": N, "rect": [x0, y0, x1, y1]}  parent-local canvas units; see the rect rule below
  {"op": "text", "prim": N, "text": "...", "cap": C?}  string of text primitive N (type 1)
  {"op": "uv", "prim": N, "uv": [u0, v0, u1, v1]}      sprite N (type 3) atlas UV rect, rec+0x90 (no aspect fit)
  {"op": "textstyle", "prim": N, "align": [ax, ay]?, "size": S?}
                                                       text N: align rec+0x9c/+0xa0 (2 f32), u16 fontsize rec+0xa4
                                                       (gui-canvas-render-path.md § 5; origin = x0 + ax·(w − w_text))
  {"op": "design", "size": [W, H], "rt": [U, V]?}     prologue +0x14/+0x18 (root P) and render target
                                                       +0x0c/+0x10; re-derives every stored quad +0x14
  {"op": "name", "prim": N, "name": "buttontext"}    element name rec+0x00 = CSymbol64(name) (or "0x.." symbol);
                                                       must stay unique in T0. Element names occur nowhere else in
                                                       the file (check), so a script's text/highlight element
                                                       binding can be matched by renaming
  {"op": "texture", "prim": N, "texture": "ui_x"}      sprite N (type 3) texture rec+0x88 = CSymbol64 (or "0x..");
                                                       T2 rows naming the old texture follow when no other primitive
                                                       still uses it (T2 keeps its size)
  {"op": "disabled", "prim": N, "on": 0|1}             T0 rec+0x10 u32, the disabled flag RenderPrimitives skips a
                                                       subtree on (gui-canvas-render-path.md); ShowUIElement writes the
                                                       same field at run time. 211 PC-final / 200 Quest-final stock prims
                                                       ship with 1 (review M1: author a layer hidden at load)
  {"op": "clone", "prim": N, "names": [...]?, "parent": P?}
                                                       append primitive N + its subtree at the END of T0 (new
                                                       indices len(T0)..), framing words re-derived (`_clone`);
                                                       a text copy gets its own T4 slot. Then address the copies
                                                       by index with the ops above (five-tile card, 2026-10-31)

1,309/1,309): T4 is one zero-padded slot per T3 entry, the slot of string i is exactly the capacity
(rec+0x94) of the one text primitive that uses i, and T3[i] = sum of the earlier slots. So a label that fits
(len + 1 <= cap) rewrites slot bytes only. A longer one sets cap = len + 1 (or `cap`), shifts the later T3
offsets, grows T4, and raises the prologue vertex/index counts. Growing T4 past its header `count`
raises `count` to `iused`; whether the loader reads `count` is open (R17).

7,904/7,904 PC and 6,988/6,988 Quest final primitives): the engine draws r = P.xy0 + (P.xy1 - P.xy0) * anchor
(+0x24) + offset (+0x34), P = the parent's r or (0, 0, W, H). It never reads the stored quad +0x14, so `rect`
solves the offset for the wanted quad and then re-derives +0x14 for every primitive, keeping the two in step
(children with a non-zero anchor follow their parent, as in game).

all six tables, and a byte round trip of the emitted file. `diff` lists the changed regions so a caller can
assert an edit touched only what it meant to.

    python3 lib/menu_ui/canvas_edit.py list ui_mnu_button_small
    python3 lib/menu_ui/canvas_edit.py emit ui_mnu_button_small --edits ops.json --out <bed-local tree>
    python3 lib/menu_ui/canvas_edit.py emit ui_mni_quit_info --edits ops.json --out T --as ffa_help_pic_eq
    python3 lib/menu_ui/canvas_edit.py [--build q] selftest     # golden edit set over every canvas

`emit` writes `<out>/primary/<TypeSymbol16>/v<AssetType>/<FileSymbol16>` (the loose-tree grammar) and
prints the shared-UI fan-out (every CCanvasUICR record and canvas that names this one). It refuses an `--out`
inside the game install or the NAS corpus. Overrides belong in a separate tree, never in place.
"""
from __future__ import annotations

import argparse
import json
import math
import os
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from resource_io import cuicanvasresource as canvas

TYPE_TEXT = 1
OFF_TYPE, OFF_RECT, OFF_TINT, OFF_STR, OFF_CAP = 0x08, 0x14, 0x78, 0x90, 0x94
OFF_ANCHOR, OFF_OFFSET, OFF_PARENT = 0x24, 0x34, 0x5C
OFF_ALIGN, OFF_FONTSIZE = 0x9C, 0xA4
OFF_DISABLED = 0x10
OFF_TEX = 0x88
ROOT = 0xFFFFFFFF
TYPES = {0: "panel", 1: "text", 2: "box", 3: "sprite", 4: "maskgroup", 6: "type6", 7: "child", 8: "type8"}


class EditError(ValueError):
    pass


def _u32(b, o):
    return struct.unpack_from("<I", b, o)[0]


def _sym(v) -> int:
    """'0x..' symbol or a name -> CSymbol64 (the hasher is imported only when a name is given)."""
    if isinstance(v, int):
        return v & 0xFFFFFFFFFFFFFFFF
    if v.startswith("0x"):
        return int(v, 16)
    from csymbol64 import hash_name
    return hash_name(v)


def strings(c: canvas.Canvas) -> list[bytes]:
    """T4 slots in T3 order, each with its padding."""
    heap = b"".join(c.records(4))
    offs = [_u32(r, 0) for r in c.records(3)]
    return [heap[o:e] for o, e in zip(offs, offs[1:] + [len(heap)])]


def expected_counts(c: canvas.Canvas) -> tuple[int, int, int]:
    """(vertices +0x2c, indices +0x30, +0x28) from T0."""
    n = {}
    glyphs = 0
    for r in c.records(0):
        t = _u32(r, OFF_TYPE)
        n[t] = n.get(t, 0) + 1
        if t == TYPE_TEXT:
            glyphs += _u32(r, OFF_CAP) - 1
    v = 4 * (n.get(2, 0) + n.get(3, 0) + n.get(8, 0) + glyphs)
    return v, v * 3 // 2, n.get(3, 0) + n.get(4, 0) + n.get(8, 0)


def computed_rects(c: canvas.Canvas) -> list[tuple[tuple, tuple]]:
    """Per T0 record: (engine rect from anchor + offset, stored quad + parent origin), both canvas space."""
    W, H = struct.unpack_from("<II", c.prologue, 0x14)
    rect, origin, out = {}, {}, []
    for r in c.records(0):
        parent, own = struct.unpack_from("<II", r, OFF_PARENT)
        q = struct.unpack_from("<4f", r, OFF_RECT)
        a = struct.unpack_from("<4f", r, OFF_ANCHOR)
        o = struct.unpack_from("<4f", r, OFF_OFFSET)
        P = (0.0, 0.0, float(W), float(H)) if parent == ROOT else rect[parent]
        rr = (P[0] + (P[2] - P[0]) * a[0] + o[0], P[1] + (P[3] - P[1]) * a[1] + o[1],
              P[0] + (P[2] - P[0]) * a[2] + o[2], P[1] + (P[3] - P[1]) * a[3] + o[3])
        px, py = (0.0, 0.0) if parent == ROOT else origin[parent]
        rect[own], origin[own] = rr, (px + q[0], py + q[1])
        out.append((rr, (px + q[0], py + q[1], px + q[2], py + q[3])))
    return out


def subtree(c: canvas.Canvas, i: int) -> list[int]:
    """T0 record indices of primitive i and every descendant (parents precede children in all shipped files)."""
    ids, out = set(), []
    for n, r in enumerate(c.records(0)):
        parent, own = struct.unpack_from("<II", r, OFF_PARENT)
        if n == i or parent in ids:
            ids.add(own)
            out.append(n)
    return out


def _sync_quads(c: canvas.Canvas, i: int) -> None:
    """Re-derive the stored quad +0x14 of primitive i and its descendants from the engine rect
    (quad = r - parent r.xy0); other records keep their bytes."""
    recs, rr = c.records(0), [x[0] for x in computed_rects(c)]
    index = {struct.unpack_from("<I", r, OFF_PARENT + 4)[0]: n for n, r in enumerate(recs)}
    for n in subtree(c, i):
        parent = struct.unpack_from("<I", recs[n], OFF_PARENT)[0]
        px, py = (0.0, 0.0) if parent == ROOT else rr[index[parent]][:2]
        r = rr[n]
        _set_t0(c, n, OFF_RECT, "<4f", r[0] - px, r[1] - py, r[2] - px, r[3] - py)


def _set_t0(c: canvas.Canvas, i: int, off: int, fmt: str, *vals) -> None:
    s = c.strides[0]
    body = bytearray(c.bodies[0])
    if not 0 <= i < len(body) // s:
        raise EditError(f"primitive {i} out of range (T0 holds {len(body) // s})")
    struct.pack_into(fmt, body, i * s + off, *vals)
    c.bodies[0] = bytes(body)


def _set_strings(c: canvas.Canvas, slots: list[bytes]) -> None:
    offs, cur = [], 0
    for s in slots:
        offs.append(cur)
        cur += len(s)
    c.bodies[3] = b"".join(struct.pack("<I", o) for o in offs)
    c.bodies[4] = b"".join(slots)
    h = c.headers[4]
    h["iused"] = len(c.bodies[4])
    if h["iused"] > h["count"]:
        h["count"] = h["iused"]
        h["size"] = h["count"] * c.strides[4]


def _recount(c: canvas.Canvas) -> None:
    v, i, k28 = expected_counts(c)
    p = bytearray(c.prologue)
    struct.pack_into("<I", p, 0x28, k28)
    struct.pack_into("<II", p, 0x2C, v, i)
    c.prologue = bytes(p)


OFF_NCHILD, OFF_ORDINAL = 0x64, 0x68


def _clone(c: canvas.Canvas, i: int, op: dict) -> None:
    """`clone`: append a copy of primitive i and its subtree at the END of T0 (subtree order kept), so the
    new indices are len(T0) .. len(T0)+k-1 and a recipe can address them. Record framing (`stream-confirmed`,
    PC 6323983201049540 / Quest 6528386917200980 card 0xa9bf5a844a019d97, 22/22 records): own id +0x60 ==
    index, parent +0x5c = parent's own id (ROOT for a root), +0x64 = child count, +0x68 = ordinal among
    the parent's children (roots: among the roots), prologue +0x08 = root count. A cloned text primitive
    gets its own new T4 slot (copy of the donor's, same capacity) at T3 index len(T3). `names`: one entry
    per cloned record in subtree order (null keeps the donor's name; a non-null name must be unique in T0).
    `parent`: re-parent the cloned root under this primitive index instead of the donor's parent."""
    s = c.strides[0]
    recs = c.records(0)
    n0 = len(recs)
    if any(_u32(r, OFF_PARENT + 4) != k for k, r in enumerate(recs)):
        raise EditError("clone needs own id == index in T0 (true for every record checked so far)")
    group = subtree(c, i)
    names = op.get("names") or [None] * len(group)
    if len(names) != len(group):
        raise EditError(f"clone of {i}: names has {len(names)} entries, subtree has {len(group)}")
    remap = {g: n0 + k for k, g in enumerate(group)}
    if "parent" in op:
        root_parent = int(op["parent"])
        if not 0 <= root_parent < n0:
            raise EditError(f"clone parent {root_parent} out of range")
    else:
        root_parent = _u32(recs[i], OFF_PARENT)
    slots = strings(c)
    body = bytearray(c.bodies[0])
    used = {struct.unpack_from("<Q", r, 0)[0] for r in recs}
    for k, g in enumerate(group):
        r = bytearray(recs[g])
        parent = root_parent if g == i else remap[_u32(recs[g], OFF_PARENT)]
        struct.pack_into("<II", r, OFF_PARENT, parent, n0 + k)
        if g == i:
            if parent == ROOT:
                ordinal = sum(_u32(x, OFF_PARENT) == ROOT for x in recs)
            else:
                ordinal = _u32(body[parent * s:(parent + 1) * s], OFF_NCHILD)
                struct.pack_into("<I", body, parent * s + OFF_NCHILD, ordinal + 1)
            struct.pack_into("<I", r, OFF_ORDINAL, ordinal)
        if names[k] is not None:
            nm = _sym(names[k])
            if nm in used:
                raise EditError(f"clone name {names[k]} (0x{nm:016x}) already used in T0")
            used.add(nm)
            struct.pack_into("<Q", r, 0, nm)
        if _u32(r, OFF_TYPE) == TYPE_TEXT:
            si = _u32(recs[g], OFF_STR)
            struct.pack_into("<I", r, OFF_STR, len(slots))
            slots.append(slots[si])
        body += r
    c.bodies[0] = bytes(body)
    h = c.headers[0]
    h["iused"] = len(body) // s
    if h["iused"] > h["count"]:
        h["count"] = h["iused"]
        h["size"] = h["count"] * s
    h3 = c.headers[3]
    _set_strings(c, slots)
    h3["iused"] = len(slots)
    if h3["iused"] > h3["count"]:
        h3["count"] = h3["iused"]
        h3["size"] = h3["count"] * c.strides[3]
    p = bytearray(c.prologue)
    struct.pack_into("<I", p, 0x08, sum(_u32(r, OFF_PARENT) == ROOT for r in c.records(0)))
    c.prologue = bytes(p)


def apply(c: canvas.Canvas, ops: list[dict]) -> canvas.Canvas:
    """Apply ops in order, then recompute the alignment pads and the prologue counts."""
    s = c.strides[0]
    for op in ops:
        if op.get("op") == "design":
            p = bytearray(c.prologue)
            W, H = (int(v) for v in op["size"])
            if W <= 0 or H <= 0:
                raise EditError("design size must be positive")
            struct.pack_into("<II", p, 0x14, W, H)
            if op.get("rt"):
                struct.pack_into("<II", p, 0x0C, *(int(v) for v in op["rt"]))
            c.prologue = bytes(p)
            for n, r in enumerate(c.records(0)):
                if struct.unpack_from("<I", r, OFF_PARENT)[0] == ROOT:
                    _sync_quads(c, n)
            continue
        kind, i = op.get("op"), int(op["prim"])
        rec = c.bodies[0][i * s:(i + 1) * s]
        if len(rec) != s:
            raise EditError(f"primitive {i} out of range")
        typ = _u32(rec, OFF_TYPE)
        if kind == "tint":
            old = struct.unpack_from("<4f", rec, OFF_TINT)
            if not all(math.isfinite(v) for v in old):
                raise EditError(f"primitive {i} ({TYPES.get(typ, typ)}) keeps no tint (rec+0x78 not finite)")
            _set_t0(c, i, OFF_TINT, "<4f", *map(float, op["rgba"]))
        elif kind == "rect":
            parent = struct.unpack_from("<I", rec, OFF_PARENT)[0]
            W, H = struct.unpack_from("<II", c.prologue, 0x14)
            prects = {struct.unpack_from("<I", r, OFF_PARENT + 4)[0]: cr[0]
                      for r, cr in zip(c.records(0), computed_rects(c))}
            P = (0.0, 0.0, float(W), float(H)) if parent == ROOT else prects[parent]
            a = struct.unpack_from("<4f", rec, OFF_ANCHOR)
            want = [float(v) for v in op["rect"]]
            want = (P[0] + want[0], P[1] + want[1], P[0] + want[2], P[1] + want[3])
            _set_t0(c, i, OFF_OFFSET, "<4f", *(want[k] - (P[k & 1] + (P[2 + (k & 1)] - P[k & 1]) * a[k]) for k in range(4)))
            _sync_quads(c, i)
        elif kind == "disabled":
            on = int(op["on"])
            if on not in (0, 1):
                raise EditError("disabled takes on = 0 or 1")
            _set_t0(c, i, OFF_DISABLED, "<I", on)
        elif kind == "uv":
            if typ != 3:
                raise EditError(f"primitive {i} is {TYPES.get(typ, typ)}, not sprite")
            _set_t0(c, i, 0x90, "<4f", *map(float, op["uv"]))
        elif kind == "name":
            new = _sym(op["name"])
            names = [struct.unpack_from("<Q", r, 0)[0] for n, r in enumerate(c.records(0)) if n != i]
            if new in names:
                raise EditError(f"element name 0x{new:016x} already used in T0")
            _set_t0(c, i, 0x00, "<Q", new)
        elif kind == "texture":
            if typ != 3:
                raise EditError(f"primitive {i} is {TYPES.get(typ, typ)}, not sprite")
            old, new = struct.unpack_from("<Q", rec, OFF_TEX)[0], _sym(op["texture"])
            _set_t0(c, i, OFF_TEX, "<Q", new)
            still = any(_u32(r, OFF_TYPE) == 3 and struct.unpack_from("<Q", r, OFF_TEX)[0] == old
                        for r in c.records(0))
            if old != new and not still:
                st = c.strides[2]
                t2 = bytearray(c.bodies[2])
                for o in range(0, len(t2) - st + 1, st):
                    if struct.unpack_from("<Q", t2, o)[0] == old:
                        struct.pack_into("<Q", t2, o, new)
                c.bodies[2] = bytes(t2)
        elif kind == "textstyle":
            if typ != TYPE_TEXT:
                raise EditError(f"primitive {i} is {TYPES.get(typ, typ)}, not text")
            if "align" in op:
                ax, ay = (float(v) for v in op["align"])
                if not (0.0 <= ax <= 1.0 and 0.0 <= ay <= 1.0):
                    raise EditError("align components must lie in [0, 1]")
                _set_t0(c, i, OFF_ALIGN, "<2f", ax, ay)
            if "size" in op:
                fs = int(op["size"])
                if not 1 <= fs <= 0xFFFF:
                    raise EditError("fontsize must fit a u16 >= 1")
                _set_t0(c, i, OFF_FONTSIZE, "<H", fs)
        elif kind == "text":
            if typ != TYPE_TEXT:
                raise EditError(f"primitive {i} is {TYPES.get(typ, typ)}, not text")
            raw = op["text"].encode("utf-8")
            if b"\0" in raw:
                raise EditError("text may not contain NUL")
            si, cap = _u32(rec, OFF_STR), _u32(rec, OFF_CAP)
            slots = strings(c)
            if len(raw) + 1 > cap or op.get("cap"):
                cap = max(int(op.get("cap") or 0), len(raw) + 1)
                _set_t0(c, i, OFF_CAP, "<I", cap)
            if len(raw) + 1 > cap:
                raise EditError(f"cap {cap} is shorter than the {len(raw)}-byte text + NUL")
            slots[si] = raw + bytes(cap - len(raw))
            _set_strings(c, slots)
        elif kind == "clone":
            _clone(c, i, op)
        else:
            raise EditError(f"unknown op {kind!r}")
    canvas.repack(c)
    _recount(c)
    return c


def check(c: canvas.Canvas) -> dict:
    """The emit gates. Every value must be True."""
    v, i, k28 = expected_counts(c)
    offs = [_u32(r, 0) for r in c.records(3)]
    caps = {}
    for r in c.records(0):
        if _u32(r, OFF_TYPE) == TYPE_TEXT:
            caps[_u32(r, OFF_STR)] = _u32(r, OFF_CAP)
    slots = strings(c)
    data = canvas.write(c)
    again = canvas.read(data, platform=c.platform, build_id=_build_hint(c))
    return {
        "counts": c.vertex_index_counts == (v, i) and _u32(c.prologue, 0x28) == k28,
        "roots_0x08": _u32(c.prologue, 0x08) == sum(_u32(r, OFF_PARENT) == ROOT for r in c.records(0)),
        "rect_rule": all(max(abs(x - y) for x, y in zip(rr, qa)) <= 0.01 for rr, qa in computed_rects(c)),
        "t3_sorted_from_0": not offs or (offs[0] == 0 and offs == sorted(offs)),
        "slot_is_capacity": all(len(slots[k]) == cap and slots[k][cap - 1] == 0 for k, cap in caps.items()),
        "iused_le_count": all(h["iused"] <= h["count"] for h in c.headers),
        "roundtrip": canvas.write(again) == data,
    }


def _build_hint(c: canvas.Canvas):
    if c.platform == "win10_dx12":
        return None
    return next(b for b, st in canvas.QUEST_STRIDES_BY_BUILD.items() if st == c.strides)


def diff(a: canvas.Canvas, b: canvas.Canvas) -> list[str]:
    """Changed regions, as 'prologue+0x2c', 'T0[3]+0x78', 'T3', 'T4', 'hdr4'."""
    out = []
    for o in range(0, canvas.PROLOGUE, 4):
        if a.prologue[o:o + 4] != b.prologue[o:o + 4]:
            out.append(f"prologue+0x{o:02x}")
    ra, rb = a.records(0), b.records(0)
    for n, (x, y) in enumerate(zip(ra, rb)):
        for o in range(0, len(x), 4):
            if x[o:o + 4] != y[o:o + 4]:
                out.append(f"T0[{n}]+0x{o:02x}")
    if len(ra) != len(rb):
        out.append("T0 length")
    for t in range(1, 6):
        if a.bodies[t] != b.bodies[t]:
            out.append(f"T{t}")
    for t in range(6):
        if a.headers[t] != b.headers[t]:
            out.append(f"hdr{t}")
    if a.subheap != b.subheap:
        out.append("T1 subheap")
    return out


def _basis(build):
    from game_store import Basis
    return Basis.named(build)


def _load(b, ident):
    data = b.read("primary", "CUICanvasResource", ident)
    if data is None:
        raise SystemExit(f"{ident}: no CUICanvasResource in {b.plat} {b.build}")
    plat = "win10_dx12" if b.plat == "PC" else "android_vulkan"
    return data, canvas.read(data, platform=plat, build_id=b.build)


def cmd_list(a) -> int:
    b = _basis(a.build)
    _, c = _load(b, a.canvas)
    slots = strings(c)
    print(f"{b.name_of(canvas_fsym(b, a.canvas)) or a.canvas}: design {c.design_size} rt {c.atlas_size} "
          f"verts/idx {c.vertex_index_counts}")
    for n, r in enumerate(c.records(0)):
        typ, (parent, own) = _u32(r, OFF_TYPE), struct.unpack_from("<II", r, 0x5C)
        rect = [round(v, 1) for v in struct.unpack_from("<4f", r, OFF_RECT)]
        tint = struct.unpack_from("<4f", r, OFF_TINT)
        line = (f"{n:4d} {TYPES.get(typ, typ):9s} parent {parent if parent != 0xFFFFFFFF else '-':>3} "
                f"name 0x{struct.unpack_from('<Q', r, 0)[0]:016x} rect {rect}")
        if all(math.isfinite(v) for v in tint):
            line += " tint " + ",".join(f"{v:.3f}" for v in tint)
        if typ == TYPE_TEXT:
            si = _u32(r, OFF_STR)
            line += f" text {slots[si].split(b'\0')[0].decode('utf-8', 'replace')!r} cap {_u32(r, OFF_CAP)}"
        print(line)
    return 0


def canvas_fsym(b, ident) -> int:
    from game_store import fsym_of
    r = b.row("primary", "CUICanvasResource", ident)
    return r.fsym if r else fsym_of(ident)


def fan_out(b, fsym: int) -> list[str]:
    """Every CCanvasUICR / CUICanvasResource file of the build whose bytes name this canvas."""
    key = struct.pack("<Q", fsym)
    hits = []
    for cls in ("CCanvasUICR", "CUICanvasResource"):
        for f, row in b.cell("primary", cls).items():
            if f == fsym:
                continue
            n = b.read("primary", cls, f).count(key)
            if n:
                hits.append(f"{cls} {b.name_of(f) or f'0x{f:016x}'} x{n}")
    return sorted(hits)


FORBIDDEN_OUT = ()


def cmd_emit(a) -> int:
    out = Path(a.out).resolve()
    if any(str(out).startswith(p) for p in FORBIDDEN_OUT):
        raise SystemExit(f"refusing --out {out}: read-only root (game install / NAS corpus)")
    b = _basis(a.build)
    data, c = _load(b, a.canvas)
    before = canvas.read(data, platform=c.platform, build_id=b.build)
    ops = json.loads(Path(a.edits).read_text(encoding="utf-8"))
    apply(c, ops)
    gates = check(c)
    changed = diff(before, c)
    new = canvas.write(c)
    row = b.row("primary", "CUICanvasResource", a.canvas)
    dest = out / b.tree_rel(row)
    if a.as_name:
        dest = dest.parent / f"{_sym(a.as_name):016x}"
    print(json.dumps({"canvas": b.name_of(row.fsym), "gates": gates, "changed": changed,
                      "bytes": [len(data), len(new)]}, indent=1))
    if not all(gates.values()):
        raise SystemExit("gate failed; nothing written")
    if not a.dry_run:
        dest.parent.mkdir(parents=True, exist_ok=True)
        dest.write_bytes(new)
        print(f"wrote {dest}")
    if a.as_name:
        print(f"copy 0x{_sym(a.as_name):016x} ({a.as_name}): no shipped row names it; bind it with "
              "menu_layout.py --canvas-rebind")
        return 0
    print("fan-out (every one of these shows the change):")
    for h in fan_out(b, row.fsym):
        print("  " + h)
    return 0


def _strip(rec: bytes, mask) -> bytes:
    return b"".join(rec[o:o + 4] for o in range(0, len(rec), 4) if o not in mask)


def cmd_selftest(a) -> int:
    """Golden edit set over every canvas: tint, same-capacity text, longer text, rect. Each must pass every gate
    and change only its declared regions."""
    b = _basis(a.build)
    plat = "win10_dx12" if b.plat == "PC" else "android_vulkan"
    tally = {}

    def tick(k, ok):
        t = tally.setdefault(k, [0, 0])
        t[0] += bool(ok)
        t[1] += 1

    for f in sorted(b.cell("primary", "CUICanvasResource")):
        data = b.read("primary", "CUICanvasResource", f)
        base = canvas.read(data, platform=plat, build_id=b.build)
        tick("unmodified", canvas.write(apply(canvas.read(data, platform=plat, build_id=b.build), [])) == data)
        recs = base.records(0)
        tinted = next((n for n, r in enumerate(recs) if all(math.isfinite(v) for v in struct.unpack_from("<4f", r, OFF_TINT))), None)
        text = next((n for n, r in enumerate(recs) if _u32(r, OFF_TYPE) == TYPE_TEXT), None)
        cases = []
        if tinted is not None:
            cases.append(("tint", [{"op": "tint", "prim": tinted, "rgba": [1, 0, 0, 1]}], {f"T0[{tinted}]+0x{o:02x}" for o in (0x78, 0x7c, 0x80, 0x84)}))
            moved = {f"T0[{tinted}]+0x{o:02x}" for o in range(0x34, 0x44, 4)}
            moved |= {f"T0[{n}]+0x{o:02x}" for n in subtree(base, tinted) for o in range(0x14, 0x24, 4)}
            cases.append(("rect", [{"op": "rect", "prim": tinted, "rect": [1, 2, 3, 4]}], moved))
        sprite = next((n for n, r in enumerate(recs) if _u32(r, OFF_TYPE) == 3), None)
        if sprite is not None:
            cases.append(("uv", [{"op": "uv", "prim": sprite, "uv": [0.125, 0.25, 0.5, 0.75]}],
                          {f"T0[{sprite}]+0x{o:02x}" for o in (0x90, 0x94, 0x98, 0x9c)}))
        if sprite is not None:
            cases.append(("texture", [{"op": "texture", "prim": sprite, "texture": "0x1234567890abcdef"}],
                          {f"T0[{sprite}]+0x{o:02x}" for o in (0x88, 0x8c)} | {"T2"}))
        cases.append(("disabled", [{"op": "disabled", "prim": 0, "on": 1 - (_u32(recs[0], OFF_DISABLED) != 0)}],
                      {"T0[0]+0x10"}))
        names = {struct.unpack_from("<Q", r, 0)[0] for r in recs}
        cases.append(("name", [{"op": "name", "prim": 0, "name": "0x0fedcba987654321"}],
                      {"T0[0]+0x00", "T0[0]+0x04"}))
        DW0, H0 = base.design_size
        cases.append(("design", [{"op": "design", "size": [DW0, H0 + 64], "rt": list(base.atlas_size)}],
                      {"prologue+0x18"} | {f"T0[{n}]+0x{o:02x}" for n in range(len(recs)) for o in range(0x14, 0x24, 4)}))
        if text is not None:
            cap = _u32(recs[text], OFF_CAP)
            cases.append(("text_fits", [{"op": "text", "prim": text, "text": "X" * (cap - 1)}], {"T4"}))
            cases.append(("text_grows", [{"op": "text", "prim": text, "text": "Y" * (cap + 6)}],
                          {f"T0[{text}]+0x94", "prologue+0x2c", "prologue+0x30", "T3", "T4", "hdr4"}))
            cases.append(("textstyle", [{"op": "textstyle", "prim": text, "align": [0.0, 0.25], "size": 77}],
                          {f"T0[{text}]+0x{o:02x}" for o in (0x9c, 0xa0, 0xa4)}))
        own_ok = all(_u32(r, OFF_PARENT + 4) == k for k, r in enumerate(recs))
        groot = next((n for n, r in enumerate(recs) if _u32(r, OFF_PARENT) == ROOT and _u32(r, OFF_NCHILD)), None)
        if own_ok and groot is not None:
            grp = subtree(base, groot)
            allowed = {"T0 length", "hdr0", "prologue+0x08", "prologue+0x28", "prologue+0x2c", "prologue+0x30"}
            if any(_u32(recs[g], OFF_TYPE) == TYPE_TEXT for g in grp):
                allowed |= {"T3", "T4", "hdr3", "hdr4"}
            cases.append(("clone", [{"op": "clone", "prim": groot}], allowed))
        for name, ops, allowed in cases:
            c = apply(canvas.read(data, platform=plat, build_id=b.build), ops)
            if name == "clone":
                new = c.records(0)
                mask = (OFF_PARENT, OFF_PARENT + 4, OFF_ORDINAL, OFF_STR)
                g = check(c)
                g["landed"] = (len(new) == len(recs) + len(grp) and new[:len(recs)] == recs and
                               all(_strip(new[len(recs) + k], mask) == _strip(recs[gi], mask) for k, gi in enumerate(grp))
                               and _u32(new[len(recs)], OFF_ORDINAL) == sum(_u32(r, OFF_PARENT) == ROOT for r in recs)
                               and _u32(c.prologue, 0x08) == _u32(base.prologue, 0x08) + 1)
                ch = set(diff(base, c))
                tick(name, all(g.values()) and ch <= allowed and ch)
                if a.verbose and not (all(g.values()) and ch <= allowed):
                    print(f"FAIL {name} {b.name_of(f) or f}: {g} extra {sorted(ch - allowed)}")
                continue
            g = check(c)
            ch = set(diff(base, c))
            if name == "rect":
                q = struct.unpack_from("<4f", c.records(0)[tinted], OFF_RECT)
                g["landed"] = max(abs(x - y) for x, y in zip(q, (1, 2, 3, 4))) <= 1e-3
            if name == "design":
                g["landed"] = c.design_size == (DW0, H0 + 64)
            if name == "texture":
                g["landed"] = struct.unpack_from("<Q", c.records(0)[sprite], OFF_TEX)[0] == 0x1234567890abcdef
            if name == "name":
                g["landed"] = 0x0fedcba987654321 not in names and \
                    struct.unpack_from("<Q", c.records(0)[0], 0)[0] == 0x0fedcba987654321
            if name == "textstyle":
                r = c.records(0)[text]
                g["landed"] = (struct.unpack_from("<2f", r, OFF_ALIGN) == (0.0, 0.25)
                               and struct.unpack_from("<H", r, OFF_FONTSIZE)[0] == 77)
            tick(name, all(g.values()) and ch <= allowed and ch)
            if a.verbose and not (all(g.values()) and ch <= allowed):
                print(f"FAIL {name} {b.name_of(f) or f}: {g} extra {sorted(ch - allowed)}")
    print(f"{b.plat} {b.build}: canvas_edit selftest")
    for k, (ok, n) in tally.items():
        print(f"  {k:12s} {ok}/{n}")
    return 0 if all(ok == n for ok, n in tally.values()) else 1


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawTextHelpFormatter)
    ap.add_argument("--build", default="pcf", help="game_store alias or build id (default pcf)")
    sub = ap.add_subparsers(dest="cmd", required=True)
    p = sub.add_parser("list")
    p.add_argument("canvas")
    p = sub.add_parser("emit")
    p.add_argument("canvas")
    p.add_argument("--edits", required=True, help="JSON list of ops")
    p.add_argument("--out", required=True, help="bed-local loose tree root")
    p.add_argument("--dry-run", action="store_true")
    p.add_argument("--as", dest="as_name", help="write a map-unique copy under this name (or 0x symbol)")
    p = sub.add_parser("selftest")
    p.add_argument("-v", "--verbose", action="store_true")
    a = ap.parse_args()
    return {"list": cmd_list, "emit": cmd_emit, "selftest": cmd_selftest}[a.cmd](a)


if __name__ == "__main__":
    sys.exit(main())
