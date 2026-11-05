#!/usr/bin/env python3
"""Add a card element to one page of a level's `CR15UIPage2CR` and emit it as a loose-tree override (gap 3, the page

A page's `+32` array holds 472-byte elements: word0 = element owner (actor), word1 = 0, word2 = state `default`,
then eight 56-byte descriptors at +24..+416. Card elements populate two: `+80`, 16-byte (key, value) rows
(`sprite` -> `tile_*`), and `+416`, 72-byte rows = key (`text` / `subtext` / `tip`) + a 64-byte NUL-padded string
(PC 6323983201049540, page 24 PLAY / TUTORIALS; `stream-confirmed` framing via page_heap.decode). Arrays carry no
pointers (descriptor ptr = 0), so the file is consumed sequentially: page p's element array, then its elements'
nested payloads in element order, then p's `+88/+144/+200/+272` payloads. An insert therefore adds two blocks
(the element; its nested payloads) and bumps the page's `+32` descriptor size/count/iused. Nothing else in the
file moves except by those insertions.

    python3 lib/menu_ui/page_patch.py mnu_master 24 --card 0xff5375e5a4dee29d tile_create \
        "CREATE GAME" "" "Create a private match" --out '<tree dir>' [--dry-run]

`--card` repeats (several cards, in order, appended after the page's last element unless --index). Safety model
copied from binding_patch.py: the owner must be absent from the whole input file and present in the level's
`CR15UIPage2ElementCR` (the actor has a page-element component, which carries the click uid); the sprite must be
a symbol the card canvas lists; the output must re-decode byte-exactly, and removing the inserted blocks and
restoring the old descriptor must give back the input exactly. Input is the game install via `game_store`;
`--out` (loose-tree grammar `<out>/primary/<TypeSymbol16>/v<AssetType>/<FileSymbol16>`) is refused inside the
"""
from __future__ import annotations

import argparse
import json
import struct
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent))
sys.path.insert(0, str(HERE))
from page_heap import decode
from canvas_edit import FORBIDDEN_OUT
from binding_patch import _winpath

CLS = "CR15UIPage2CR"
FORBIDDEN = FORBIDDEN_OUT
ELEM, PAGE_REC, TOP = 472, 328, 56
SYM = {"default": 0x2fd38a995f615b9e, "sprite": 0x41d2d42d082a131b,
       "text": 0xc8c33e482e7606a4, "subtext": 0x2fc49a9d4a714f9e, "tip": 0xc8e8d0b1a884ebfa}
TILES = {"tile_play": 0xd90e94db4c552ced, "tile_tutorial": 0xeb30a0361bb3327e,
         "tile_create": 0x3e325fc287e68085, "tile_join": 0xd90e94db565624fa}
CARD_CANVAS = "ui_mnu_card"


def _desc(count: int, stride: int) -> bytes:
    """A 56-byte array descriptor exactly as the file writes it (ptr 0, flags 1, 0x20, count, iused)."""
    return struct.pack("<QQQIIQQQ", 0, count * stride, 0, 0, 1, 0x20, count, count)


def card_bytes(owner: int, sprite: int, text: str, subtext: str, tip: str, donor: bytes) -> tuple[bytes, bytes]:
    """(472-byte element, nested payload) for a card, modelled on `donor` (an existing card element)."""
    el = bytearray(donor)
    struct.pack_into("<QQQ", el, 0, owner, 0, SYM["default"])
    for slot in range(24, ELEM, 56):
        el[slot:slot + 56] = _desc(0, 0)
    el[80:80 + 56] = _desc(1, 16)
    el[416:416 + 56] = _desc(3, 72)
    nested = struct.pack("<QQ", SYM["sprite"], sprite)
    for key, s in (("text", text), ("subtext", subtext), ("tip", tip)):
        raw = s.encode("ascii")
        if len(raw) > 63:
            raise ValueError(f"{key} {s!r} longer than 63 bytes")
        nested += struct.pack("<Q", SYM[key]) + raw.ljust(64, b"\0")
    return bytes(el), nested


def add_cards(data: bytes, page: int, cards: list[tuple], index: int | None = None) -> tuple[bytes, dict]:
    """Insert `cards` [(owner, sprite, text, subtext, tip)] into page `page` at element `index` (default: append).
    Returns (new bytes, report). Raises ValueError on any precondition or verification failure."""
    _, arrays, elements = decode(data)
    n_pages, = struct.unpack_from("<Q", data, 0x28)
    if not 0 <= page < n_pages:
        raise ValueError(f"page {page} out of range (0..{n_pages - 1})")
    on_page = {int(e["word0"], 16) for e in elements if e["page"] == page}
    for c in cards:
        if c[0] in on_page:
            raise ValueError(f"owner 0x{c[0]:016x} is already an element of page {page}; refusing a duplicate")
    if len({c[0] for c in cards}) != len(cards):
        raise ValueError("duplicate owner among --card")
    top = {a["slot"]: a for a in arrays if a["page"] == page and a["element"] < 0}
    els = top[32]
    n = els["count"]
    index = n if index is None else index
    if not 0 <= index <= n:
        raise ValueError(f"index {index} out of range (0..{n})")
    shape = {}
    for a in arrays:
        if a["element"] >= 0:
            shape.setdefault((a["page"], a["element"]), {})[a["slot"]] = a["count"]
    donor_key = next((k for k, v in shape.items() if v[80] == 1 and v[416] == 3
                      and all(v[s] == 0 for s in v if s not in (80, 416))), None)
    if donor_key is None:
        raise ValueError("no card-shaped donor element in this file")
    donor_off = next(e["element_file_offset"] for e in elements if (e["page"], e["element"]) == donor_key)
    built = [card_bytes(*c, donor=data[donor_off:donor_off + ELEM]) for c in cards]
    el_block = b"".join(b[0] for b in built)
    nest_block = b"".join(b[1] for b in built)
    el_at = els["payload_file_offset"] + index * ELEM
    if index < n:
        nest_at = next(a["payload_file_offset"] for a in arrays
                       if a["page"] == page and a["element"] == index and a["slot"] == 24)
    else:
        nest_at = top[88]["payload_file_offset"]
    if not el_at <= nest_at:
        raise ValueError("unexpected layout: nested region before element array end")
    d_off = els["descriptor_file_offset"]
    old_desc = data[d_off:d_off + 56]
    if old_desc != _desc(n, ELEM):
        raise ValueError(f"page {page} +32 descriptor at 0x{d_off:x} is not the expected shape")
    out = bytearray(data[:nest_at] + nest_block + data[nest_at:])
    out[el_at:el_at] = el_block
    out[d_off:d_off + 56] = _desc(n + len(cards), ELEM)
    out = bytes(out)
    rebuilt, arrays2, elements2 = decode(out)
    if rebuilt != out:
        raise ValueError("patched file does not round-trip")
    shift_el = len(el_block)
    back = bytearray(out)
    del back[nest_at + shift_el:nest_at + shift_el + len(nest_block)]
    del back[el_at:el_at + shift_el]
    back[d_off:d_off + 56] = old_desc
    if bytes(back) != data:
        raise ValueError("changes outside the inserted blocks / descriptor")
    old = [(e["page"], e["word0"]) for e in elements]
    new = [(e["page"], e["word0"]) for e in elements2]
    first = next(i for i, e in enumerate(elements) if e["page"] == page) if n else None
    pos = (first if first is not None else len(old)) + index
    if new[:pos] + new[pos + len(cards):] != old or [w for _, w in new[pos:pos + len(cards)]] != \
            [f"0x{c[0]:016x}" for c in cards]:
        raise ValueError("element list differs beyond the inserted cards")
    rep = dict(page=page, index=index, descriptor=f"0x{d_off:x}", count=f"{n}->{n + len(cards)}",
               element_insert=f"0x{el_at:x}+{len(el_block)}", nested_insert_orig=f"0x{nest_at:x}+{len(nest_block)}",
               nested_insert_new=f"0x{nest_at + shift_el:x}", donor=f"page {donor_key[0]} element {donor_key[1]}",
               size=f"{len(data)}->{len(out)}")
    return out, rep


def element_uid(elcr: bytes, owner: int) -> int | None:
    """uid (+88) of `owner`'s CR15UIPage2ElementCR record (0x38 header, 96-byte records, owner at +8)."""
    n, = struct.unpack_from("<Q", elcr, 0x28)
    for i in range(n):
        o, = struct.unpack_from("<Q", elcr, 0x38 + i * 96 + 8)
        if o == owner:
            return struct.unpack_from("<Q", elcr, 0x38 + i * 96 + 88)[0]
    return None


def _card(args: list[str]) -> tuple:
    owner, sprite, text, subtext, tip = args
    s = TILES.get(sprite)
    return int(owner, 0), (s if s is not None else int(sprite, 0)), text, subtext, tip


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawTextHelpFormatter)
    ap.add_argument("level", help="CR15UIPage2CR name or fsym, e.g. mnu_master")
    ap.add_argument("page", type=int)
    ap.add_argument("--card", nargs=5, action="append", required=True,
                    metavar=("OWNER", "SPRITE", "TEXT", "SUBTEXT", "TIP"))
    ap.add_argument("--index", type=int, help="insert before element INDEX (default: append)")
    ap.add_argument("--out", required=True, help="bed-local loose tree root")
    ap.add_argument("--build", default="pcf", help="game_store alias or build id (default pcf)")
    ap.add_argument("--base", help="read the level's files from this loose tree where present (and copy the rest)")
    ap.add_argument("--dry-run", action="store_true")
    a = ap.parse_args()
    out = Path(_winpath(a.out)).resolve()
    base = Path(_winpath(a.base)).resolve() if a.base else None
    if base is not None and not base.is_dir():
        raise SystemExit(f"--base {base} is not a directory")
    if base is not None and (base == out or out.is_relative_to(base) or base.is_relative_to(out)):
        raise SystemExit("--out must not overlap --base")
    if any(str(out).startswith(p) for p in FORBIDDEN):
        raise SystemExit(f"refusing --out {out}: read-only root (read-only source tree)")
    from game_store import Basis
    b = Basis.named(a.build)
    r = b.row("primary", CLS, a.level)
    if r is None:
        raise SystemExit(f"{a.level}: no {CLS} in {b.plat} {b.build}")

    def read(cls):
        rr = b.row("primary", cls, a.level)
        if base is not None and rr is not None and (base / b.tree_rel(rr)).is_file():
            return (base / b.tree_rel(rr)).read_bytes()
        return b.read("primary", cls, a.level)
    data = read(CLS)
    cards = [_card(c) for c in a.card]
    elcr = read("CR15UIPage2ElementCR")
    canvas = b.read("primary", "CUICanvasResource", CARD_CANVAS)
    for c in cards:
        uid = element_uid(elcr, c[0])
        if uid is None:
            raise SystemExit(f"refused: owner 0x{c[0]:016x} has no CR15UIPage2ElementCR record; nothing written")
        if struct.pack("<Q", c[1]) not in canvas:
            raise SystemExit(f"refused: sprite 0x{c[1]:016x} not in CUICanvasResource/{CARD_CANVAS}; nothing written")
        print(f"card 0x{c[0]:016x} uid 0x{uid:016x} sprite 0x{c[1]:016x} {c[2]!r}")
    try:
        new, rep = add_cards(data, a.page, cards, a.index)
    except ValueError as e:
        raise SystemExit(f"refused: {e}; nothing written")
    dest = out / b.tree_rel(r)
    print(f"{b.plat} {b.build} {CLS} {b.name_of(r.fsym) or a.level} 0x{r.fsym:016x}: {json.dumps(rep)}; "
          "round trip ok, undo-diff ok")
    if a.dry_run:
        print(f"dry run: would write {dest}")
        return 0
    dest.parent.mkdir(parents=True, exist_ok=True)
    dest.write_bytes(new)
    print(f"wrote {dest}")
    if base is not None:
        for f in sorted(p for p in base.rglob("*") if p.is_file()):
            rel = f.relative_to(base).as_posix()
            if rel != b.tree_rel(r):
                (out / rel).parent.mkdir(parents=True, exist_ok=True)
                (out / rel).write_bytes(f.read_bytes())
                print(f"copied {rel} from base")
    return 0


if __name__ == "__main__":
    sys.exit(main())
