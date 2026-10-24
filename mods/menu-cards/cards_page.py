#!/usr/bin/env python3
"""Decode a menu page resource (CR15UIPage2CR) and insert card elements into one of its pages. Pure, in memory.

File layout (every array is a 56-byte descriptor followed, in consumption order, by its payload; descriptors carry
no pointers, so the file is read sequentially):

  top table      328-byte page records
  per page p     +32  element array (472-byte elements), then each element's nested payloads in element order,
                 then the page's +88 / +144 / +200 / +272 payloads (16-byte rows)
  per element    word0 = owner actor, word1 = 0, word2 = state `default`, then eight descriptors at +24..+416;
                 a card populates +80 (16-byte rows: `sprite` -> tile symbol) and +416 (72-byte rows: key +
                 64-byte NUL-padded ASCII string for `text` / `subtext` / `tip`)

An insert adds two blocks (the elements; their nested payloads) and bumps the page's +32 descriptor. `add_cards`
verifies that the result re-decodes byte-exactly and that removing the insertions gives back the input exactly.
"""
from __future__ import annotations

import struct

CAP = 64 * 1024 * 1024
ELEM, PAGE_REC, HDR = 472, 328, 56
PAGE_SLOTS = (32, 88, 144, 200, 272)
ELEMENT_SLOTS = tuple(range(24, ELEM, 56))
# CSymbol64 values of the keys and tile sprites (the names occur in the shipped page / card canvas)
SYM = {"default": 0x2fd38a995f615b9e, "sprite": 0x41d2d42d082a131b,
       "text": 0xc8c33e482e7606a4, "subtext": 0x2fc49a9d4a714f9e, "tip": 0xc8e8d0b1a884ebfa}
TILES = {"tile_play": 0xd90e94db4c552ced, "tile_tutorial": 0xeb30a0361bb3327e,
         "tile_create": 0x3e325fc287e68085, "tile_join": 0xd90e94db565624fa}


def _read_desc(data: bytes, off: int) -> dict:
    ptr, size, z10 = struct.unpack_from("<QQQ", data, off)
    z18, flags = struct.unpack_from("<II", data, off + 0x18)
    mark, count, iused = struct.unpack_from("<QQQ", data, off + 0x20)
    return dict(ptr=ptr, size=size, z10=z10, z18=z18, flags=flags, mark=mark, count=count, iused=iused)


def _pack_desc(h: dict) -> bytes:
    return (struct.pack("<QQQ", h["ptr"], h["size"], h["z10"]) + struct.pack("<II", h["z18"], h["flags"])
            + struct.pack("<QQQ", h["mark"], h["count"], h["iused"]))


def _desc(count: int, stride: int) -> bytes:
    """A descriptor exactly as the file writes it (ptr 0, flags 1, mark 0x20, count, iused)."""
    return struct.pack("<QQQIIQQQ", 0, count * stride, 0, 0, 1, 0x20, count, count)


def header(data: bytes, offset: int, stride: int) -> dict:
    if offset < 0 or offset + HDR > len(data):
        raise ValueError(f"truncated descriptor at {offset:#x}")
    h = _read_desc(data, offset)
    if (h["ptr"], h["z10"], h["z18"], h["flags"]) != (0, 0, 0, 1):
        raise ValueError(f"invalid descriptor at {offset:#x}")
    if h["count"] != h["iused"] or h["size"] != h["count"] * stride:
        raise ValueError(f"unexpected count/stride at {offset:#x}")
    return h


def decode(data: bytes):
    """(reconstructed bytes, arrays, elements). Rejects any unconsumed tail or unexpected populated slot."""
    if len(data) > CAP:
        raise ValueError("resource exceeds 64 MiB cap")
    top = header(data, 0, PAGE_REC)
    cur = HDR + top["size"]
    if cur > len(data):
        raise ValueError("truncated top table")
    rebuilt = bytearray(data[:cur])
    rebuilt[:HDR] = _pack_desc(top)
    arrays, elements = [], []

    def array(descriptor, stride, page, element, slot):
        nonlocal cur
        h = header(data, descriptor, stride)
        rebuilt[descriptor:descriptor + HDR] = _pack_desc(h)
        start, end = cur, cur + h["size"]
        if end > len(data):
            raise ValueError(f"array at {start:#x} exceeds file")
        rebuilt.extend(data[start:end])
        cur = end
        arrays.append(dict(page=page, element=element, slot=slot, descriptor_file_offset=descriptor,
                           payload_file_offset=start, count=h["count"], stride=stride, size=h["size"]))
        return start, h["count"]

    for page in range(top["count"]):
        page_base = HDR + page * PAGE_REC
        for slot in PAGE_SLOTS:
            start, count = array(page_base + slot, ELEM if slot == 32 else 16, page, -1, slot)
            if slot != 32:
                continue
            for element in range(count):
                base = start + element * ELEM
                w0, = struct.unpack_from("<Q", data, base)
                elements.append(dict(page=page, element=element, element_file_offset=base, word0=f"0x{w0:016x}"))
                for child_slot in ELEMENT_SLOTS:
                    child_stride = {80: 16, 416: 72}.get(child_slot, 0)
                    h = header(data, base + child_slot, child_stride)
                    if child_stride == 0 and h["count"]:
                        raise ValueError("unobserved populated element slot")
                    array(base + child_slot, child_stride, page, element, child_slot)
    if cur != len(data):
        raise ValueError(f"unconsumed bytes: {len(data) - cur}")
    if rebuilt != data:
        raise ValueError("descriptor/array reconstruction mismatch")
    return bytes(rebuilt), arrays, elements


def card_bytes(owner: int, sprite: int, text: str, subtext: str, tip: str, donor: bytes) -> tuple[bytes, bytes]:
    """(472-byte element, nested payload) for a card, modelled on `donor` (an existing card element)."""
    el = bytearray(donor)
    struct.pack_into("<QQQ", el, 0, owner, 0, SYM["default"])
    for slot in ELEMENT_SLOTS:
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
    for c in cards:
        if struct.pack("<Q", c[0]) in data:
            raise ValueError(f"owner 0x{c[0]:016x} already occurs in the file; refusing a duplicate")
    if len({c[0] for c in cards}) != len(cards):
        raise ValueError("duplicate owner among cards")
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
    rebuilt, _, elements2 = decode(out)
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
    rep = dict(page=page, index=index, count=f"{n}->{n + len(cards)}", size=f"{len(data)}->{len(out)}",
               donor=f"page {donor_key[0]} element {donor_key[1]}")
    return out, rep


def element_uid(elcr: bytes, owner: int) -> int | None:
    """uid (+88) of `owner`'s CR15UIPage2ElementCR record (0x38 header, 96-byte records, owner at +8)."""
    n, = struct.unpack_from("<Q", elcr, 0x28)
    for i in range(n):
        o, = struct.unpack_from("<Q", elcr, 0x38 + i * 96 + 8)
        if o == owner:
            return struct.unpack_from("<Q", elcr, 0x38 + i * 96 + 88)[0]
    return None
