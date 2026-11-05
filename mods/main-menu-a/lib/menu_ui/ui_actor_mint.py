#!/usr/bin/env python3
"""Mint a NEW UI actor in a menu level by cloning a template actor's records under a new owner CSymbol64
never written.

Layouts (PC 6323983201049540, `stream-confirmed` framing unless marked; decoded 2026-10-30 on mnu_master /
mnu_master_mp_ingame):

* Flat component tables - 56-byte descriptor + N x stride rows, row = SncaComponentData 32-byte prefix
  (name sym, actornodeid = OWNER, actorid 0xffff.., flags) + per-class body; rows are NOT owner-sorted:
  CTransformCR 176 (quat @+0x20, trans @+0x30, scale @+0x3c), CCanvasUICR 88 (canvas sym @+0x20),
  CR15PointerInterfaceCR 32, CR15PointerMeshCR 80, CBoundingSphereCR 48, CTextureOverrideCR 32.
  Append = one row at EOF + header size/count/iused. Bodies hold syms and floats only (no level-relative
  index), so a row BODY may come from another level's template (`--body-from`).
* CR15UIPage2ElementCR - same 96-byte rows (click uid @+88) followed by a constant 16-byte tail; the
  row is inserted before the tail.
* CScriptCR - 720-byte top records (word0 class, word1 owner, word4 module), 12 descriptors at
  +48..+664, payloads in record order (script_heap.decode). Clone = record appended to the top table,
  its payloads appended at EOF. Optional exact string swap in the +496 text rows (`--retext OLD=NEW`).
  Face of a button-module (e2868929d1ed191a) clone, by serialized key: `--label` = initial_text (+496),
  `--text-element` / `--highlight-element` = text_element_name / highlight_element_name (+328). With
  `--canvas`, both names must be elements of that canvas (the third_button canvas names them buttontext /
  buttonhighlight; the button_small template names text / highlight, which left the H-M1 tile at the
  canvas literal "MUTE ALL" with its highlight overlay never hidden).
* CR15UIPage2CR - a plain (all-descriptors-empty) 472-byte element appended to a page's +32 array
  (page_heap.decode framing; button/small-tile elements are this shape, card elements are not).
* CActorDataResource (codec resource_io/cactordataresource.py round-trips; semantics `inferred`):
  the codec's leader `pairs` are SHIFTED by 8 bytes. True leader = [u64 prefix == N] + K x {u64 key,
  u64 value = actor index}, signed-sorted by key (200/200 keys verified on mnu_master); the codec's
  `head` is the last value. Per-actor arrays in actor order: prefablookup = the actor's node KEY,
  dyn0 = alias key (-1 normally), dyn1 = group key (self, other, or -1), dyn2 = world CTransfQS (48 B),
  dyn3 u16, dyn4 u16 = parent ACTOR index (0xffff none), dyn5 1-bit array (= has an SGActorData
  row), 4 CBitMaps of N bits; `lookup` groups = inverse of dyn1 (group key -> actor indices),
  signed-sorted by key. prefix == N in 32/32 ADRs of the build.
  Add = new actor index N APPENDED to every per-actor array (no existing actor index moves), leader
  record inserted sorted (binary-searched map, positions unreferenced), prefix N -> N+1, groups updated.
* CGSceneResource (codec resource_io/cgsceneresource.py round-trips): cooker invariant in 13/13
  node-graph scenes: lookup signed-sorted, lookup value == vertex index == rank, vertex id == index,
  idxremap identity. SGActorData (56 B) rows follow ADR ACTOR order over the dyn5 bit (199/199
  mnu_master, 129/129 mp_ingame; quat+trans == ADR dyn2), NOT vertex order.
  Add = vertex INSERTED at its sorted rank; every vertex reference (vertex ids, sedges/tedges vhandle,
  roots, src/tgt leafs, lookup values, types/names positions) renumbered; SGActorData row of the
  donor ACTOR appended. An APPEND variant (v != rank) crashed the PC client on mnu_master load
  2026-10-30 (orchestrator run ffa_hm1/ffa_hm2). Donors with graph edges are refused.
  Gates: every pre-existing actor / node resolves to the same index-free tuple before and after.

The cross-level answer: ADR / scene hold level-relative indices (actor index, vertex index, SGActorData
order), so the ACTOR is always minted from an in-level template; the flat CR bodies are index-free and may
come from any level.

    python3 lib/menu_ui/ui_actor_mint.py mnu_master --template 0x1b71f837dc9e5669 \
        --body-from mnu_master_mp_ingame:0xca760b3376a22fc9 --name ffa_help_tile_test \
        --canvas ui_mnu_card_third_button --trans 4.1 -1.95 4.4 --page 24 \
        --label "EARLY QUIT" --text-element buttontext --highlight-element buttonhighlight \
        --out <bed-local tree> [--dry-run]
"""
from __future__ import annotations

import argparse
import hashlib
import json
import struct
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent))
sys.path.insert(0, str(HERE.parent / "resource_io"))
sys.path.insert(0, str(HERE))
import cactordataresource as adr
import cgsceneresource as gsc
from page_heap import decode as page_decode
from page_patch import _desc as page_desc, FORBIDDEN, ELEM
from script_heap import decode as script_decode
from binding_patch import _winpath

FLAT = {"CTransformCR": 176, "CCanvasUICR": 88, "CR15PointerInterfaceCR": 32, "CR15PointerMeshCR": 80,
        "CBoundingSphereCR": 48, "CTextureOverrideCR": 32}
BODY_CLASSES = ("CCanvasUICR", "CR15PointerInterfaceCR", "CR15PointerMeshCR", "CBoundingSphereCR",
                "CTextureOverrideCR")
ALL = ("CActorDataResource", "CGSceneResource", "CTransformCR", "CCanvasUICR", "CR15UIPage2ElementCR",
       "CR15UIPage2CR", "CScriptCR", "CR15PointerInterfaceCR", "CR15PointerMeshCR", "CBoundingSphereCR",
       "CTextureOverrideCR")
NONE64 = 0xFFFFFFFFFFFFFFFF
M = 0x1_00000000


def q8(x: int) -> bytes:
    return struct.pack("<Q", x)


def s64(x: int) -> int:
    return struct.unpack("<q", q8(x))[0]


def _hdr(data: bytes) -> tuple[int, int]:
    ws = struct.unpack_from("<7Q", data, 0)
    if ws[0] or ws[2] or ws[3] != M or ws[5] != ws[6]:
        raise ValueError(f"unexpected table descriptor {[hex(w) for w in ws]}")
    return ws[1], ws[5]


def _set_hdr(buf: bytearray, size: int, count: int) -> None:
    struct.pack_into("<Q", buf, 8, size)
    struct.pack_into("<QQ", buf, 0x28, count, count)


def _rows(data: bytes, stride: int, owner: int, base: int = 56) -> list[int]:
    _, n = _hdr(data)
    return [i for i in range(n) if struct.unpack_from("<Q", data, base + i * stride + 8)[0] == owner]


def flat_append(data: bytes, stride: int, row: bytes, new_owner: int) -> bytes:
    size, n = _hdr(data)
    if size != n * stride or 56 + size != len(data):
        raise ValueError("not a flat table of this stride")
    if q8(new_owner) in data:
        raise ValueError(f"owner 0x{new_owner:016x} already in file")
    row = bytearray(row)
    struct.pack_into("<Q", row, 8, new_owner)
    out = bytearray(data + bytes(row))
    _set_hdr(out, size + stride, n + 1)
    out = bytes(out)
    size2, n2 = _hdr(out)
    if size2 != n2 * stride or 56 + size2 != len(out) or _rows(out, stride, new_owner) != [n]:
        raise ValueError("flat re-decode failed")
    back = bytearray(out[:-stride])
    _set_hdr(back, size, n)
    if bytes(back) != data:
        raise ValueError("flat undo-diff failed")
    return out


def element_append(data: bytes, row: bytes, new_owner: int, uid: int) -> bytes:
    """CR15UIPage2ElementCR: 96-byte rows + constant 16-byte tail."""
    size, n = _hdr(data)
    if size != n * 96 or len(data) != 56 + size + 16:
        raise ValueError("unexpected CR15UIPage2ElementCR framing")
    if q8(new_owner) in data or q8(uid) in data:
        raise ValueError("owner or uid already in file")
    row = bytearray(row)
    struct.pack_into("<Q", row, 8, new_owner)
    struct.pack_into("<Q", row, 88, uid)
    at = 56 + size
    out = bytearray(data[:at] + bytes(row) + data[at:])
    _set_hdr(out, size + 96, n + 1)
    out = bytes(out)
    if out[-16:] != data[-16:] or _rows(out, 96, new_owner) != [n]:
        raise ValueError("element re-decode failed")
    back = bytearray(out[:at] + out[at + 96:])
    _set_hdr(back, size, n)
    if bytes(back) != data:
        raise ValueError("element undo-diff failed")
    return out


KEY_CLICK_EVENT = 0x63becd132df98137
KEY_TEXT_ELEMENT = 0x6ee997457faf8030
KEY_HIGHLIGHT_ELEMENT = 0x63ecc1157cad8562
KEY_INITIAL_TEXT = 0x6fbec2142ffc8732
KEY_DESCRIPTION = 0x63ee96162dae8535


def script_clone(data: bytes, donor: int, new_owner: int, retext: dict[str, str] | None = None,
                 sym_vars: dict[int, int] | None = None,
                 str_vars: dict[int, str] | None = None) -> tuple[bytes, list]:
    """Clone `donor`'s CScriptCR records under `new_owner`. `sym_vars` {key: symbol} rewrites the value word
    of +328 rows by serialized key; `str_vars` {key: text} rewrites +496 rows by key. Every requested key must
    hit at least one row of the donor's records (else refused): a silent miss is how the H-M1 tile kept the
    canvas literal."""
    rebuilt, arrays, _ = script_decode(data)
    if rebuilt != data:
        raise ValueError("CScriptCR does not round-trip")
    if q8(new_owner) in data:
        raise ValueError("owner already in CScriptCR")
    size, n = _hdr(data)
    recs = _rows(data, 720, donor)
    if not recs:
        raise ValueError("donor has no CScriptCR record")
    sym_vars, str_vars = dict(sym_vars or {}), dict(str_vars or {})
    hit = set()
    new_recs, new_pay, report = b"", b"", []
    for r in recs:
        rec = bytearray(data[56 + r * 720:56 + (r + 1) * 720])
        struct.pack_into("<Q", rec, 8, new_owner)
        for a in (x for x in arrays if x["record"] == r and x["count"]):
            pay = bytearray(data[a["payload_file_offset"]:a["payload_file_offset"] + a["size"]])
            if a["slot"] == 328 and sym_vars:
                for i in range(a["count"]):
                    k, _, val = struct.unpack_from("<3Q", pay, i * 24)
                    if k in sym_vars:
                        struct.pack_into("<Q", pay, i * 24 + 16, sym_vars[k])
                        hit.add(("sym", k))
                        report.append(f"rec {r} +328[{i}] key 0x{k:016x} 0x{val:016x}->0x{sym_vars[k]:016x}")
            if a["slot"] == 496 and str_vars:
                for i in range(a["count"]):
                    k = struct.unpack_from("<Q", pay, i * 80)[0]
                    if k in str_vars:
                        raw = str_vars[k].encode("ascii")
                        if len(raw) > 63:
                            raise ValueError(f"text {str_vars[k]!r} > 63 bytes")
                        o = i * 80 + 16
                        cur = bytes(pay[o:o + 64]).split(b"\0")[0].decode("ascii", "replace")
                        pay[o:o + 64] = raw.ljust(64, b"\0")
                        hit.add(("str", k))
                        report.append(f"rec {r} +496[{i}] key 0x{k:016x} {cur!r}->{str_vars[k]!r}")
            if a["slot"] == 496 and retext:
                for i in range(a["count"]):
                    o = i * 80 + 16
                    cur = bytes(pay[o:o + 64]).split(b"\0")[0].decode("ascii", "replace")
                    if cur in retext:
                        raw = retext[cur].encode("ascii")
                        if len(raw) > 63:
                            raise ValueError(f"retext {retext[cur]!r} > 63 bytes")
                        pay[o:o + 64] = raw.ljust(64, b"\0")
                        report.append(f"rec {r} +496[{i}] {cur!r}->{retext[cur]!r}")
            if q8(donor) in pay:
                raise ValueError(f"record {r} slot {a['slot']} payload names the donor; refusing a blind copy")
            new_pay += bytes(pay)
        new_recs += bytes(rec)
        report.append(f"rec {r} cls 0x{struct.unpack_from('<Q', rec, 0)[0]:016x} "
                      f"module 0x{struct.unpack_from('<Q', rec, 32)[0]:016x}")
    missed = [f"+328 0x{k:016x}" for k in sym_vars if ("sym", k) not in hit] + \
             [f"+496 0x{k:016x}" for k in str_vars if ("str", k) not in hit]
    if missed:
        raise ValueError(f"donor script records carry no row for {missed}")
    at = 56 + size
    out = bytearray(data[:at] + new_recs + data[at:] + new_pay)
    _set_hdr(out, size + len(new_recs), n + len(recs))
    out = bytes(out)
    rb, _, _ = script_decode(out)
    if rb != out or _rows(out, 720, new_owner) != list(range(n, n + len(recs))):
        raise ValueError("CScriptCR re-decode failed")
    back = bytearray(out[:at] + out[at + len(new_recs):len(out) - len(new_pay)])
    _set_hdr(back, size, n)
    if bytes(back) != data:
        raise ValueError("CScriptCR undo-diff failed")
    return out, report


def page_add_plain(data: bytes, page: int, donor: int, new_owner: int, member: bool = False) -> tuple[bytes, dict]:
    """Append a plain element for `new_owner` (cloned from `donor`'s plain element) to page `page`'s +32 array.
    `member`: `new_owner` is the EXISTING actor `donor` joining another page (menu_layout --member); refused when
    it already has an element on `page`. Otherwise `new_owner` must be absent from the file."""
    rebuilt, arrays, elements = page_decode(data)
    if rebuilt != data:
        raise ValueError("CR15UIPage2CR does not round-trip")
    if member:
        if new_owner != donor:
            raise ValueError("member mode: the donor element must be the actor's own")
        if any(e["page"] == page and int(e["word0"], 16) == new_owner for e in elements):
            raise ValueError(f"0x{new_owner:016x} is already an element of page {page}")
    elif q8(new_owner) in data:
        raise ValueError("owner already in CR15UIPage2CR")
    if not 0 <= page < struct.unpack_from("<Q", data, 0x28)[0]:
        raise ValueError(f"page {page} out of range")
    shape = {}
    for a in arrays:
        if a["element"] >= 0:
            shape.setdefault((a["page"], a["element"]), 0)
            shape[(a["page"], a["element"])] += a["count"]
    d = next((e for e in elements if int(e["word0"], 16) == donor and shape[(e["page"], e["element"])] == 0), None)
    if d is None:
        raise ValueError("donor has no plain (empty-descriptor) page element")
    el = bytearray(data[d["element_file_offset"]:d["element_file_offset"] + ELEM])
    struct.pack_into("<Q", el, 0, new_owner)
    top = {a["slot"]: a for a in arrays if a["page"] == page and a["element"] < 0}
    els = top[32]
    n = els["count"]
    d_off = els["descriptor_file_offset"]
    old_desc = data[d_off:d_off + 56]
    if old_desc != page_desc(n, ELEM):
        raise ValueError("page +32 descriptor not the expected shape")
    at = els["payload_file_offset"] + n * ELEM
    out = bytearray(data[:at] + bytes(el) + data[at:])
    out[d_off:d_off + 56] = page_desc(n + 1, ELEM)
    out = bytes(out)
    rb, _, el2 = page_decode(out)
    if rb != out:
        raise ValueError("page re-decode failed")
    back = bytearray(out[:at] + out[at + ELEM:])
    back[d_off:d_off + 56] = old_desc
    if bytes(back) != data:
        raise ValueError("page undo-diff failed")
    old = [(e["page"], e["word0"]) for e in elements]
    new = [(e["page"], e["word0"]) for e in el2]
    pos = next(i for i, e in enumerate(el2) if e["page"] == page) + n
    if new[:pos] + new[pos + 1:] != old or new[pos] != (page, f"0x{new_owner:016x}"):
        raise ValueError("element list differs beyond the inserted element")
    return out, dict(page=page, index=n, donor=f"page {d['page']} element {d['element']}")


def _u16s_to_q(w: list[int]) -> list[int]:
    return list(struct.unpack(f"<{len(w) // 4}Q", struct.pack(f"<{len(w)}H", *w)))


def _q_to_u16s(q: list[int]) -> list[int]:
    return list(struct.unpack(f"<{len(q) * 4}H", struct.pack(f"<{len(q)}Q", *q)))


def adr_view(data: bytes) -> dict:
    """The ADR in true (unshifted) form."""
    o = adr.read(data)
    if adr.write(o) != data:
        raise ValueError("ADR does not round-trip")
    P = o["pairs"]
    keys = [k for _, k in P]
    vals = [P[j + 1][0] for j in range(len(P) - 1)] + [o["head"]]
    N = o["N"]
    groups, pos = [], 0
    for h, c in o["groups"]:
        groups.append([h, o["mem"][pos:pos + c]])
        pos += c
    words = [_u16s_to_q(w) for w in o["dyn_payloads"][:2]]
    v = dict(o=o, prefix=P[0][0], keys=keys, vals=vals, N=N, groups=groups,
             actor_key=_u16s_to_q(o["prefab_payload"]), d0=words[0], d1=words[1],
             d2=[bytes(struct.pack("<24H", *o["dyn_payloads"][2][i * 24:i * 24 + 24])) for i in range(N)],
             d3=list(o["dyn_payloads"][3]), d4=list(o["dyn_payloads"][4]),
             d5=_u16s_to_q(o["dyn_payloads"][5]), bits=list(o["bit_words"]))
    sk = [s64(k) for k in keys]
    if sk != sorted(sk) or v["prefix"] != N:
        raise ValueError("ADR leader not in the verified (prefix N, signed-sorted) form")
    for i, k in enumerate(v["actor_key"]):
        if vals[keys.index(k)] != i:
            raise ValueError("ADR leader value != actor index")
    return v


def adr_pack(v: dict) -> bytes:
    o = dict(v["o"])
    N = v["N"]
    keys, vals = v["keys"], v["vals"]
    o["K"] = len(keys)
    o["pairs"] = [(v["prefix"] if j == 0 else vals[j - 1], keys[j]) for j in range(len(keys))]
    o["head"] = vals[-1]
    o["groups"] = [(h, len(m)) for h, m in v["groups"]]
    o["mem"] = [x for _, m in v["groups"] for x in m]
    o["N"] = N
    o["p_bs"] = 8 * N
    o["prefab_payload"] = _q_to_u16s(v["actor_key"])
    d2 = list(struct.unpack(f"<{24 * N}H", b"".join(v["d2"])))
    o["dyn_payloads"] = [_q_to_u16s(v["d0"]), _q_to_u16s(v["d1"]), d2, v["d3"], v["d4"], _q_to_u16s(v["d5"])]
    o["dyn"] = [(8 * N, N), (8 * N, N), (48 * N, N), (2 * N, N), (2 * N, N), (8 * len(v["d5"]), len(v["d5"]))]
    by = v["o"]["bmaps"][0][1]
    o["bmaps"] = [(N, by, w) for _, by, w in v["o"]["bmaps"]]
    o["bit_words"] = v["bits"]
    return adr.write(o)


def adr_add_actor(data: bytes, donor: int, new: int, trans=None) -> tuple[bytes, dict]:
    v = adr_view(data)
    if new in v["keys"] or any(h == new for h, _ in v["groups"]) or q8(new) in data:
        raise ValueError("new owner already in ADR")
    if donor not in v["actor_key"]:
        raise ValueError("donor is not an actor of this ADR")
    i = v["actor_key"].index(donor)
    N = v["N"]
    if v["d0"][i] != NONE64:
        raise ValueError("donor has an alias key (dyn0); minting aliases is not supported")
    if (N + 1 + 63) // 64 != len(v["d5"]) or (N + 1 + 7) // 8 > v["o"]["bmaps"][0][1]:
        raise ValueError("bit arrays would need to grow; not supported")
    w = dict(v)
    w["groups"] = [[h, list(m)] for h, m in v["groups"]]
    for k in ("actor_key", "d0", "d1", "d2", "d3", "d4", "d5", "bits"):
        w[k] = list(v[k])
    d2 = bytearray(v["d2"][i])
    if trans is not None:
        struct.pack_into("<3f", d2, 16, *trans)
    w["actor_key"].append(new)
    w["d0"].append(NONE64)
    g = v["d1"][i]
    w["d1"].append(new if g == donor else g)
    w["d2"].append(bytes(d2))
    w["d3"].append(v["d3"][i])
    w["d4"].append(v["d4"][i])
    by, words = v["o"]["bmaps"][0][1], v["o"]["bmaps"][0][2]
    for arr, nw, base in ((w["d5"], len(v["d5"]), None),) + tuple((w["bits"], words, gi * words) for gi in range(4)):
        b0 = 0 if base is None else base
        if (arr[b0 + i // 64] >> (i % 64)) & 1:
            arr[b0 + N // 64] |= 1 << (N % 64)
    w["N"] = N + 1
    w["prefix"] = N + 1
    at = next((j for j, k in enumerate(v["keys"]) if s64(k) > s64(new)), len(v["keys"]))
    w["keys"] = v["keys"][:at] + [new] + v["keys"][at:]
    w["vals"] = v["vals"][:at] + [N] + v["vals"][at:]
    gnote = "none"
    if g == donor:
        gat = next((j for j, (h, _) in enumerate(w["groups"]) if s64(h) > s64(new)), len(w["groups"]))
        w["groups"].insert(gat, [new, [N]])
        gnote = f"new group at {gat}"
    elif g != NONE64:
        gi = next(j for j, (h, _) in enumerate(w["groups"]) if h == g)
        w["groups"][gi][1].append(N)
        gnote = f"joined group 0x{g:016x}"
    out = adr_pack(w)
    v2 = adr_view(out)
    if adr_pack(v2) != out or v2["actor_key"][N] != new or v2["vals"][v2["keys"].index(new)] != N:
        raise ValueError("ADR re-decode failed")
    if adr_pack(_adr_remove_last(v2, at, g == donor)) != data:
        raise ValueError("ADR undo-diff failed")
    return out, dict(actor=N, leader_insert=at, group=gnote, donor_actor=i)


def _adr_remove_last(v: dict, at: int, new_group: bool) -> dict:
    N = v["N"] - 1
    w = dict(v)
    for k in ("actor_key", "d0", "d1", "d2", "d3", "d4"):
        w[k] = v[k][:-1]
    w["d5"], w["bits"] = list(v["d5"]), list(v["bits"])
    words = v["o"]["bmaps"][0][2]
    w["d5"][N // 64] &= ~(1 << (N % 64)) & NONE64
    for gi in range(4):
        w["bits"][gi * words + N // 64] &= ~(1 << (N % 64)) & NONE64
    w["keys"] = v["keys"][:at] + v["keys"][at + 1:]
    w["vals"] = v["vals"][:at] + v["vals"][at + 1:]
    w["groups"] = [[h, [x for x in m if x != N]] for h, m in v["groups"]]
    if new_group:
        w["groups"] = [g for g in w["groups"] if g[1]]
    w["N"] = w["prefix"] = N
    return w


def _tab(t, stride, fmt):
    return [struct.unpack_from(fmt, t[1], i * stride) for i in range(t[0])]


def _pack_tab(rows, fmt):
    return (len(rows), b"".join(struct.pack(fmt, *r) for r in rows))


def _scene_parts(s: dict) -> dict:
    ng = s["nodegraph"]
    return dict(V=_tab(ng["verts"], 16, "<4I"), LK=_tab(ng["lookup"][:2], 16, "<QQ"),
                TY=_tab(ng["types"], 4, "<I"), NM=_tab(ng["names"], 8, "<Q"), IR=_tab(ng["idxremap"], 4, "<I"),
                RT=_tab(ng["roots"], 4, "<I"), SL=_tab(ng["srcleafs"][:2], 8, "<II"),
                TL=_tab(ng["tgtleafs"][:2], 8, "<II"), SE=_tab(ng["sedges"], 8, "<II"),
                TE=_tab(ng["tedges"], 8, "<II"), DL=ng["deadlist"][0])


def _check_rank_form(p: dict) -> None:
    """The cooker invariant (13/13 node-graph scenes, PC 6323983201049540): lookup signed-sorted by key,
    lookup value == vertex index == its rank, vertex id == index, idxremap identity, no dead list."""
    nv = len(p["V"])
    if (len(p["LK"]) != nv or [v for _, v in p["LK"]] != list(range(nv))
            or [s64(k) for k, _ in p["LK"]] != sorted(s64(k) for k, _ in p["LK"])
            or [x[2] for x in p["V"]] != list(range(nv)) or p["IR"] != [(i,) for i in range(nv)] or p["DL"]):
        raise ValueError("scene node graph not in the rank form (lookup value == vertex == sorted rank)")


def scene_resolve(data: bytes) -> dict:
    """key -> (type, name, root, src-leaf, tgt-leaf, source-chain keys, target-chain keys): index-free view."""
    p = _scene_parts(gsc.read(data))
    key = [k for k, _ in p["LK"]]

    def chain(head, tab):
        out, e, n = [], head, 0
        while e != 0xFFFFFFFF and n <= len(tab):
            out.append(key[tab[e][0]])
            e, n = tab[e][1], n + 1
        return tuple(out)
    roots = {r for (r,) in p["RT"]}
    sl = {v for v, _ in p["SL"]}
    tl = {v for v, _ in p["TL"]}
    return {key[j]: (p["TY"][j], p["NM"][j], j in roots, j in sl, j in tl, chain(p["V"][j][1], p["SE"]),
                     chain(p["V"][j][0], p["TE"])) for j in range(len(key))}


def scene_add_vertex(data: bytes, donor: int, new: int, sga_src: int | None, trans_delta=None) -> tuple[bytes, dict]:
    """Insert `new` as an isolated root vertex AT ITS SORTED RANK and renumber every vertex reference, so
    lookup value == vertex index == rank keeps holding (appending broke it: in-game crash 2026-10-30, the
    nodes after the insert point resolved to their neighbours' parents). `sga_src` = the donor actor's
    SGActorData row; rows are in ADR actor order (199/199 mnu_master, 129/129 mp_ingame), so the new
    actor's row is APPENDED (it is the last actor)."""
    s = gsc.read(data)
    if gsc.write(s) != data:
        raise ValueError("CGSceneResource does not round-trip")
    if q8(new) in data:
        raise ValueError("new owner already in scene")
    p = _scene_parts(s)
    _check_rank_form(p)
    LK = p["LK"]
    nv = len(p["V"])
    dv = next((v for k, v in LK if k == donor), None)
    if dv is None:
        raise ValueError("donor has no scene vertex")
    if p["V"][dv][:2] != (0xFFFFFFFF, 0xFFFFFFFF) or (dv,) not in p["RT"]:
        raise ValueError("donor vertex has edges / is not a root; only isolated roots are supported")
    at = next((j for j, (k, _) in enumerate(LK) if s64(k) > s64(new)), nv)

    def rn(j):
        return j if j == 0xFFFFFFFF else j + (j >= at)
    V = [(t, sh, rn(i), f) for t, sh, i, f in p["V"]]
    V.insert(at, (0xFFFFFFFF, 0xFFFFFFFF, at, p["V"][dv][3]))
    ng = dict(s["nodegraph"])
    ng["verts"] = _pack_tab(V, "<4I")
    ng["sedges"] = _pack_tab([(rn(v), e) for v, e in p["SE"]], "<II")
    ng["tedges"] = _pack_tab([(rn(v), e) for v, e in p["TE"]], "<II")
    for key, rows in (("srcleafs", p["SL"]), ("tgtleafs", p["TL"])):
        drow = [r for r in rows if r[0] == dv]
        new_rows = sorted([(rn(v), x) for v, x in rows] + ([(at, drow[0][1])] if drow else []))
        ng[key] = _pack_tab(new_rows, "<II") + (s["nodegraph"][key][2],)
    ng["idxremap"] = _pack_tab([(i,) for i in range(nv + 1)], "<I")
    ng["roots"] = _pack_tab(sorted([(rn(r),) for (r,) in p["RT"]] + [(at,)]), "<I")
    lk = [(k, rn(v)) for k, v in LK]
    lk.insert(at, (new, at))
    ng["lookup"] = _pack_tab(lk, "<QQ") + (s["nodegraph"]["lookup"][2],)
    TY, NM = list(p["TY"]), list(p["NM"])
    TY.insert(at, p["TY"][dv])
    NM.insert(at, p["NM"][dv])
    ng["types"] = _pack_tab(TY, "<I")
    ng["names"] = _pack_tab(NM, "<Q")
    s2 = dict(s)
    s2["nodegraph"] = ng
    sga_note = "none"
    if sga_src is not None:
        rec = bytearray(s["actors"][1][sga_src * 56:(sga_src + 1) * 56])
        if trans_delta is not None:
            q = struct.unpack_from("<3f", rec, 32)
            struct.pack_into("<3f", rec, 32, *(x + d for x, d in zip(q, trans_delta)))
        s2["actors"] = (s["actors"][0] + 1, s["actors"][1] + bytes(rec))
        sga_note = f"SGActorData[{s['actors'][0]}] from [{sga_src}]"
    out = gsc.write(s2)
    if gsc.write(gsc.read(out)) != out:
        raise ValueError("scene re-decode failed")
    _check_rank_form(_scene_parts(gsc.read(out)))
    before, after = scene_resolve(data), scene_resolve(out)
    if set(after) - set(before) != {new} or any(after[k] != before[k] for k in before):
        raise ValueError("a pre-existing scene node resolves differently after the insert")
    if after[new] != before[donor]:
        raise ValueError("new node does not mirror the donor node")
    r = gsc.read(out)
    pr = _scene_parts(r)

    def un(j):
        return j if j == 0xFFFFFFFF else j - (j > at)
    u = dict(r)
    ung = dict(r["nodegraph"])
    ung["verts"] = _pack_tab([(t, sh, un(i), f) for k, (t, sh, i, f) in enumerate(pr["V"]) if k != at], "<4I")
    ung["sedges"] = _pack_tab([(un(v), e) for v, e in pr["SE"]], "<II")
    ung["tedges"] = _pack_tab([(un(v), e) for v, e in pr["TE"]], "<II")
    for key in ("srcleafs", "tgtleafs"):
        ung[key] = _pack_tab([(un(v), x) for v, x in pr[key[:1].upper() + "L"] if v != at], "<II") + \
            (r["nodegraph"][key][2],)
    ung["idxremap"] = _pack_tab([(i,) for i in range(nv)], "<I")
    ung["roots"] = _pack_tab([(un(x),) for (x,) in pr["RT"] if x != at], "<I")
    ung["lookup"] = _pack_tab([(k, un(v)) for k, v in pr["LK"] if k != new], "<QQ") + (r["nodegraph"]["lookup"][2],)
    ung["types"] = _pack_tab([x for k, x in enumerate(pr["TY"]) if k != at], "<I")
    ung["names"] = _pack_tab([x for k, x in enumerate(pr["NM"]) if k != at], "<Q")
    u["nodegraph"] = ung
    if sga_src is not None:
        u["actors"] = (r["actors"][0] - 1, r["actors"][1][:-56])
    if gsc.write(u) != data:
        raise ValueError("scene undo-diff failed")
    return out, dict(vertex=at, donor_vertex=dv, renumbered=nv - at, sga=sga_note)


def adr_resolve(data: bytes) -> dict:
    """key -> index-free tuple (actor index, alias, group key, world QS, d3, parent KEY via d4, bits, members)."""
    v = adr_view(data)
    ak = v["actor_key"]
    words = v["o"]["bmaps"][0][2]
    groups = {h: tuple(ak[x] for x in m) for h, m in v["groups"]}
    out = {}
    for i, k in enumerate(ak):
        bits = tuple((v["bits"][g * words + i // 64] >> (i % 64)) & 1 for g in range(4)) + \
            ((v["d5"][i // 64] >> (i % 64)) & 1,)
        par = ak[v["d4"][i]] if v["d4"][i] != 0xFFFF else None
        out[k] = (i, v["d0"][i], v["d1"][i], v["d2"][i], v["d3"][i], par, bits, groups.get(k))
    for k, val in zip(v["keys"], v["vals"]):
        if k not in out:
            out[k] = ("alias", ak[val])
    return out


def _sga_rank(v: dict, i: int) -> int | None:
    """Row of actor i in SGActorData (actor order over the dyn5 bit), or None when the bit is clear."""
    bit = lambda j: (v["d5"][j // 64] >> (j % 64)) & 1
    return sum(bit(j) for j in range(i)) if bit(i) else None


def clone_actor(resources: dict[str, bytes], donor_owner: int, new_owner: int, trans=None) -> dict[str, bytes]:
    """Clone `donor_owner`'s actor-table presence (CActorDataResource + CGSceneResource) under `new_owner`.
    `resources` maps class -> bytes of ONE level; returns class -> new bytes for the two classes (gated:
    every pre-existing actor and scene node resolves to the same index-free tuple before and after)."""
    a0, s0 = resources["CActorDataResource"], resources["CGSceneResource"]
    v = adr_view(a0)
    i = v["actor_key"].index(donor_owner) if donor_owner in v["actor_key"] else None
    if i is None:
        raise ValueError("donor is not an actor of this ADR")
    sc = gsc.read(s0)
    nd5 = sum(bin(w).count("1") for w in v["d5"])
    if sc["actors"][0] != nd5:
        raise ValueError(f"SGActorData count {sc['actors'][0]} != dyn5 popcount {nd5}")
    a, _ = adr_add_actor(a0, donor_owner, new_owner, trans)
    before, after = adr_resolve(a0), adr_resolve(a)
    if set(after) - set(before) != {new_owner} or any(after[k] != before[k] for k in before
                                                       if k != after[new_owner][2]):
        raise ValueError("a pre-existing actor resolves differently after the insert")
    delta = None
    if trans is not None:
        old = struct.unpack_from("<3f", v["d2"][i], 16)
        delta = tuple(t - o for t, o in zip(trans, old))
    s, _ = scene_add_vertex(s0, donor_owner, new_owner, _sga_rank(v, i), delta)
    return {"CActorDataResource": a, "CGSceneResource": s}


def script_sym_vars(data: bytes, owner: int) -> dict[int, int]:
    """{+328 key: symbol value} over `owner`'s CScriptCR records (last row wins)."""
    _, arrays, _ = script_decode(data)
    recs = set(_rows(data, 720, owner))
    out = {}
    for a in (x for x in arrays if x["record"] in recs and x["slot"] == 328):
        for i in range(a["count"]):
            k, _, v = struct.unpack_from("<3Q", data, a["payload_file_offset"] + i * 24)
            out[k] = v
    return out


def canvas_element_names(data: bytes) -> set[int]:
    """Element name CSymbol64s of a PC CUICanvasResource (SUI2Primitive +0x00; canvas_elements.py layout)."""
    sys.path.insert(0, str(HERE.parent))
    from resource_io import cuicanvasresource as cuic
    c = cuic.read(data, platform="win10_dx12", build_id="6323983201049540")
    if cuic.write(c) != data:
        raise ValueError("canvas does not round-trip")
    return {struct.unpack_from("<Q", r, 0)[0] for r in c.records(0)}


def mint_tile(read, level: str, template: int, new: int, uid: int, trans, canvas_sym: int | None = None,
              body: tuple | None = None, page: int | None = None, retext=None, label: str | None = None,
              text_element: int | None = None, highlight_element: int | None = None,
              canvas_elems: set[int] | None = None, click_event: int | None = None,
              script_from: int | None = None) -> tuple[dict, list]:
    """read(class, level) -> bytes. Returns ({class: new bytes}, report lines).
    Face (button module e2868929d1ed191a): `label` -> initial_text; `text_element` / `highlight_element`
    (CSymbol64 of canvas element names) -> text_element_name / highlight_element_name. With `canvas_elems`
    (the target canvas's element names) the cloned record's text/highlight element names must exist on the
    canvas, else refused: an unresolved name leaves the canvas literal text and an un-hidden highlight.
    `click_event` (CSymbol64 of an event name) -> the click_event row: the event the button sends on press.
    shipped invite_code script on a SETTINGS-templated page element). Face rows still apply to the clone and are
    refused when the donor's records lack them."""
    rep = []
    src = {c: read(c, level) for c in ALL}
    for c, d in src.items():
        if q8(new) in d or q8(uid) in d:
            raise ValueError(f"new owner/uid already present in {c}/{level}")
    out = clone_actor(src, template, new, trans)
    v = adr_view(src["CActorDataResource"])
    old_t = struct.unpack_from("<3f", v["d2"][v["actor_key"].index(template)], 16)
    rep.append(f"ADR + scene: actor cloned; template world trans {tuple(round(x, 4) for x in old_t)}")
    tr = src["CTransformCR"]
    rows = _rows(tr, 176, template)
    if len(rows) != 1:
        raise ValueError(f"template has {len(rows)} CTransformCR rows")
    row = bytearray(tr[56 + rows[0] * 176:56 + (rows[0] + 1) * 176])
    if tuple(round(x, 4) for x in struct.unpack_from("<3f", row, 0x30)) != tuple(round(x, 4) for x in old_t):
        raise ValueError("CTransformCR +0x30 does not equal the ADR world translation; layout assumption broken")
    struct.pack_into("<3f", row, 0x30, *trans)
    out["CTransformCR"] = flat_append(tr, 176, bytes(row), new)
    blvl, bown = body if body else (level, template)
    for c in BODY_CLASSES:
        bd = read(c, blvl)
        st = FLAT[c]
        r = _rows(bd, st, bown)
        if len(r) != 1:
            raise ValueError(f"body donor has {len(r)} {c} rows")
        brow = bytearray(bd[56 + r[0] * st:56 + (r[0] + 1) * st])
        if c == "CCanvasUICR" and canvas_sym is not None:
            if struct.unpack_from("<Q", brow, 0x20)[0] != canvas_sym:
                rep.append(f"CCanvasUICR canvas 0x{struct.unpack_from('<Q', brow, 0x20)[0]:016x} -> 0x{canvas_sym:016x}")
                struct.pack_into("<Q", brow, 0x20, canvas_sym)
        out[c] = flat_append(src[c], st, bytes(brow), new)
    er = _rows(src["CR15UIPage2ElementCR"], 96, template)
    if len(er) != 1:
        raise ValueError("template has no single CR15UIPage2ElementCR row")
    erow = src["CR15UIPage2ElementCR"][56 + er[0] * 96:56 + (er[0] + 1) * 96]
    out["CR15UIPage2ElementCR"] = element_append(src["CR15UIPage2ElementCR"], erow, new, uid)
    sym_vars = {k: v for k, v in ((KEY_TEXT_ELEMENT, text_element), (KEY_HIGHLIGHT_ELEMENT, highlight_element),
                                  (KEY_CLICK_EVENT, click_event)) if v is not None}
    str_vars = {KEY_INITIAL_TEXT: label} if label is not None else None
    sdonor = template if script_from is None else script_from
    if sdonor != template:
        rep.append(f"CScriptCR records from 0x{sdonor:016x} (--script-from), not the template")
    out["CScriptCR"], srep = script_clone(src["CScriptCR"], sdonor, new, retext, sym_vars, str_vars)
    rep += [f"CScriptCR {x}" for x in srep]
    if canvas_elems is not None:
        sv = script_sym_vars(out["CScriptCR"], new)
        bad = [f"{n} 0x{sv[k]:016x}" for n, k in (("text_element_name", KEY_TEXT_ELEMENT),
                                                 ("highlight_element_name", KEY_HIGHLIGHT_ELEMENT))
               if k in sv and sv[k] not in canvas_elems]
        if bad:
            raise ValueError(f"script names canvas elements the canvas lacks: {bad}; "
                             "set --text-element / --highlight-element")
    if page is not None:
        out["CR15UIPage2CR"], prep = page_add_plain(src["CR15UIPage2CR"], page, template, new)
        rep.append(f"CR15UIPage2CR {prep}")
    for c, d in out.items():
        rep.append(f"{c}: {len(src[c])} -> {len(d)} B, new-owner hits {d.count(q8(new))}")
    return out, rep


BUTTON_POS = 0x80
BUTTON_HALF = 0x9c


def _row_of(data: bytes, stride: int, owner: int, name: int) -> int:
    size, n = _hdr(data)
    if size != n * stride or 56 + size != len(data):
        raise ValueError("not a flat table of this stride")
    hit = [i for i in range(n) if struct.unpack_from("<QQ", data, 56 + i * stride) == (name, owner)]
    if len(hit) != 1:
        raise ValueError(f"{len(hit)} rows with name 0x{name:016x} owner 0x{owner:016x}")
    return hit[0]


def component_add(data: bytes, stride: int, owner: int, donor_name: int, new_name: int,
                  f32: dict[int, tuple] | None = None, donor_owner: int | None = None) -> bytes:
    """Append a copy of row (owner, donor_name) named new_name, with f32 tuples patched at row offsets.
    `donor_owner`: take the row of (donor_owner, donor_name) instead and re-own the copy (+8) to `owner` (an owner
    with no row of this class yet: the tablet report zones on the generic prompt actor, donor state_mgr BackButton)."""
    size, n = _hdr(data)
    src = _row_of(data, stride, owner if donor_owner is None else donor_owner, donor_name)
    if any(struct.unpack_from("<QQ", data, 56 + i * stride) == (new_name, owner) for i in range(n)):
        raise ValueError(f"owner already has a component 0x{new_name:016x}")
    row = bytearray(data[56 + src * stride:56 + (src + 1) * stride])
    struct.pack_into("<Q", row, 0, new_name)
    struct.pack_into("<Q", row, 8, owner)
    for off, vals in (f32 or {}).items():
        if off < 32 or off + 4 * len(vals) > stride:
            raise ValueError(f"f32 patch at +0x{off:x} outside the row body")
        struct.pack_into(f"<{len(vals)}f", row, off, *vals)
    out = bytearray(data + bytes(row))
    _set_hdr(out, size + stride, n + 1)
    out = bytes(out)
    if _row_of(out, stride, owner, new_name) != n:
        raise ValueError("component re-decode failed")
    back = bytearray(out[:-stride])
    _set_hdr(back, size, n)
    if bytes(back) != data:
        raise ValueError("component undo-diff failed")
    return out


def component_patch(data: bytes, stride: int, owner: int, name: int, off: int, fmt: str, vals: tuple) -> bytes:
    """Same-length patch of one row field; every other byte must stay."""
    i = _row_of(data, stride, owner, name)
    if off < 32 or off + struct.calcsize(fmt) > stride:
        raise ValueError(f"patch at +0x{off:x} outside the row body")
    out = bytearray(data)
    at = 56 + i * stride + off
    struct.pack_into(fmt, out, at, *vals)
    w = struct.calcsize(fmt)
    if out[:at] != data[:at] or out[at + w:] != data[at + w:]:
        raise ValueError("patch touched other bytes")
    return bytes(out)


def components_main(argv: list[str]) -> int:
    """`ui_actor_mint.py components LEVEL --owner 0x.. [--donor-owner 0x..] [--add DONOR=NEW@x,y,z] [--move NAME@x,y,z]
    [--canvas NAME|0x..] --out TREE [--base TREE] [--build pcf|q] [--dry-run] [--noop]`"""
    from game_store import Basis
    from quest_clone_map import csymbol64
    ap = argparse.ArgumentParser(prog="ui_actor_mint.py components", description=components_main.__doc__)
    ap.add_argument("level")
    ap.add_argument("--owner", required=True)
    ap.add_argument("--add", action="append", default=[], help="DONOR=NEW@x,y,z[/hx,hy,hz] (CR15ButtonInteractCR; /half-extents in m, default = the donor's)")
    ap.add_argument("--donor-owner", help="--add takes DONOR's row from this owner (0x..) and re-owns the copy to --owner")
    ap.add_argument("--move", action="append", default=[], help="NAME@x,y,z (CR15ButtonInteractCR)")
    ap.add_argument("--canvas", help="rebind the owner's CCanvasUICR row (+0x20) to this canvas")
    ap.add_argument("--out")
    ap.add_argument("--base")
    ap.add_argument("--build", default="pcf")
    ap.add_argument("--dry-run", action="store_true")
    ap.add_argument("--noop", action="store_true", help="no-op round trip of the two tables: add+remove, move back")
    a = ap.parse_args(argv)
    b = Basis.named(a.build)
    owner = int(a.owner, 0)
    sym = lambda s: int(s, 16) if s.startswith("0x") else csymbol64(s)
    base = Path(_winpath(a.base)).resolve() if a.base else None

    def read(c):
        r = b.row("primary", c, a.level)
        if r is None:
            raise SystemExit(f"{c}/{a.level} missing in {b.build}")
        if base is not None and (base / b.tree_rel(r)).is_file():
            return (base / b.tree_rel(r)).read_bytes()
        return b.read("primary", c, a.level)

    btn0 = read("CR15ButtonInteractCR")
    can0 = read("CCanvasUICR")
    st = 296
    if a.noop:
        size, n = _hdr(btn0)
        rows = [i for i in range(n) if struct.unpack_from("<Q", btn0, 56 + i * st + 8)[0] == owner]
        ok = bool(rows)
        for i in rows:
            nm = struct.unpack_from("<Q", btn0, 56 + i * st)[0]
            pos = struct.unpack_from("<3f", btn0, 56 + i * st + BUTTON_POS)
            ok &= component_patch(btn0, st, owner, nm, BUTTON_POS, "<3f", pos) == btn0
            component_add(btn0, st, owner, nm, 0x0123456789abcdef)
        crow = _rows(can0, FLAT["CCanvasUICR"], owner)
        ok &= len(crow) == 1
        try:
            from resource_io import cr15buttoninteractcr as bic, ccanvasuicr as ccu
            ok &= bic.roundtrip(btn0) == btn0 and ccu.roundtrip(can0) == can0
            codec = "codec rt ok"
        except Exception as e:
            codec = f"codec rt skipped ({type(e).__name__}: {e})"[:120]
        print(codec)
        print(f"noop {b.plat} {b.build} {a.level}: button rows {rows} canvas row {crow} -> {'PASS' if ok else 'FAIL'}")
        return 0 if ok else 1
    btn, can = btn0, can0
    for spec in a.move:
        nm, xyz = spec.split("@")
        btn = component_patch(btn, st, owner, sym(nm), BUTTON_POS, "<3f", tuple(float(v) for v in xyz.split(",")))
    for spec in a.add:
        pair, xyz = spec.split("@")
        donor, new = pair.split("=")
        xyz, _, half = xyz.partition("/")
        f32 = {BUTTON_POS: tuple(float(v) for v in xyz.split(","))}
        if half:
            f32[BUTTON_HALF] = tuple(float(v) for v in half.split(","))
            if len(f32[BUTTON_HALF]) != 3 or min(f32[BUTTON_HALF]) <= 0:
                raise SystemExit(f"--add {spec}: /hx,hy,hz takes three positive half-extents")
        btn = component_add(btn, st, owner, sym(donor), sym(new), f32,
                            int(a.donor_owner, 0) if a.donor_owner else None)
    if a.canvas:
        crow = _rows(can, FLAT["CCanvasUICR"], owner)
        if len(crow) != 1:
            raise SystemExit(f"owner has {len(crow)} CCanvasUICR rows")
        buf = bytearray(can)
        struct.pack_into("<Q", buf, 56 + crow[0] * 88 + 0x20, sym(a.canvas))
        can = bytes(buf)
    if not a.out:
        raise SystemExit("--out is required unless --noop")
    out = Path(_winpath(a.out)).resolve()
    if any(str(out).startswith(p) for p in FORBIDDEN):
        raise SystemExit(f"refusing --out {out}: read-only root or shared test workspace")
    for c, d, d0 in (("CR15ButtonInteractCR", btn, btn0), ("CCanvasUICR", can, can0)):
        if d == d0:
            continue
        dest = out / b.tree_rel(b.row("primary", c, a.level))
        print(f"{'would write' if a.dry_run else 'wrote'} {dest} {len(d0)} -> {len(d)} B sha256 "
              f"{hashlib.sha256(d).hexdigest()[:12]}")
        if not a.dry_run:
            dest.parent.mkdir(parents=True, exist_ok=True)
            dest.write_bytes(d)
    return 0


def main() -> int:
    if len(sys.argv) > 1 and sys.argv[1] == "components":
        return components_main(sys.argv[2:])
    from game_store import Basis
    from quest_clone_map import csymbol64
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawTextHelpFormatter)
    ap.add_argument("level")
    ap.add_argument("--template", required=True, help="in-level template owner (0x...)")
    ap.add_argument("--body-from", help="LEVEL:OWNER donor for the index-free CR row bodies")
    ap.add_argument("--name", required=True, help="new owner NAME (CSymbol64 minted from it)")
    ap.add_argument("--uid-name", help="click uid NAME (default <name>_uid)")
    ap.add_argument("--canvas", help="CUICanvasResource name for the CCanvasUICR row")
    ap.add_argument("--trans", nargs=3, type=float, help="new world translation (required for a tile; --actor-only keeps the donor's when omitted)")
    ap.add_argument("--page", type=int, help="also append a plain element to this page")
    ap.add_argument("--retext", action="append", default=[], help="OLD=NEW exact script text swap")
    ap.add_argument("--label", help="button label: the template record's initial_text row (<= 63 ASCII)")
    ap.add_argument("--text-element", help="canvas element NAME for the label (third_button: buttontext)")
    ap.add_argument("--highlight-element", help="canvas element NAME for hover (third_button: buttonhighlight)")
    ap.add_argument("--click-event", help="event NAME the button sends on press (click_event row; template default "
                    "evt_btn_clicked)")
    ap.add_argument("--script-from", help="in-level OWNER (0x...) whose CScriptCR records the new actor clones "
                    "instead of the template's (e.g. a shipped widget script)")
    ap.add_argument("--actor-only", action="store_true", help="only CActorDataResource + CGSceneResource")
    ap.add_argument("--out", required=True)
    ap.add_argument("--base", help="read the level's files from this loose tree where present (and copy the rest), "
                    "to chain mints, e.g. a tile onto a page that page_add.py wrote")
    ap.add_argument("--build", default="pcf")
    ap.add_argument("--dry-run", action="store_true")
    a = ap.parse_args()
    out = Path(_winpath(a.out)).resolve()
    if any(str(out).startswith(p) for p in FORBIDDEN):
        raise SystemExit(f"refusing --out {out}: read-only root or shared test workspace")
    base = Path(_winpath(a.base)).resolve() if a.base else None
    if base is not None and not base.is_dir():
        raise SystemExit(f"--base {base} is not a directory")
    if base is not None and (base == out or out.is_relative_to(base) or base.is_relative_to(out)):
        raise SystemExit("--out must not overlap --base")
    b = Basis.named(a.build)
    new = csymbol64(a.name)
    uid = csymbol64(a.uid_name or f"{a.name}_uid")
    tmpl = int(a.template, 0)
    print(f"new owner {a.name} = 0x{new:016x}; uid = 0x{uid:016x}")

    def read(c, lvl):
        if base is not None and lvl == a.level:
            r = b.row("primary", c, lvl)
            if r is not None and (base / b.tree_rel(r)).is_file():
                return (base / b.tree_rel(r)).read_bytes()
        d = b.read("primary", c, lvl)
        if d is None:
            raise SystemExit(f"{c}/{lvl} missing in {b.build}")
        return d
    try:
        if a.actor_only:
            res = clone_actor({c: read(c, a.level) for c in ("CActorDataResource", "CGSceneResource")},
                              tmpl, new, tuple(a.trans) if a.trans else None)
            rep = [f"{c}: new-owner hits {d.count(q8(new))}" for c, d in res.items()]
        else:
            if not a.trans:
                raise SystemExit("--trans is required for a tile")
            body = None
            if a.body_from:
                lv, ow = a.body_from.split(":")
                body = (lv, int(ow, 0))
            celems = None
            if a.canvas:
                cd = b.read("primary", "CUICanvasResource", a.canvas)
                if cd is None:
                    raise SystemExit(f"canvas {a.canvas} missing in {b.build}")
                celems = canvas_element_names(cd)
            res, rep = mint_tile(read, a.level, tmpl, new, uid, tuple(a.trans),
                                 csymbol64(a.canvas) if a.canvas else None, body, a.page,
                                 dict(x.split("=", 1) for x in a.retext), a.label,
                                 csymbol64(a.text_element) if a.text_element else None,
                                 csymbol64(a.highlight_element) if a.highlight_element else None, celems,
                                 csymbol64(a.click_event) if a.click_event else None,
                                 int(a.script_from, 0) if a.script_from else None)
    except ValueError as e:
        raise SystemExit(f"refused: {e}; nothing written")
    for line in rep:
        print(line)
    for c, d in res.items():
        dest = out / b.tree_rel(b.row("primary", c, a.level))
        print(f"{'would write' if a.dry_run else 'wrote'} {dest} sha256 {hashlib.sha256(d).hexdigest()}")
        if not a.dry_run:
            dest.parent.mkdir(parents=True, exist_ok=True)
            dest.write_bytes(d)
    if base is not None:
        written = {b.tree_rel(b.row("primary", c, a.level)) for c in res}
        for f in sorted(p for p in base.rglob("*") if p.is_file()):
            rel = f.relative_to(base).as_posix()
            if rel not in written:
                print(f"{'would copy' if a.dry_run else 'copied'} {rel} from base")
                if not a.dry_run:
                    (out / rel).parent.mkdir(parents=True, exist_ok=True)
                    (out / rel).write_bytes(f.read_bytes())
    print(json.dumps(dict(level=a.level, owner=f"0x{new:016x}", uid=f"0x{uid:016x}", files=len(res))))
    return 0


if __name__ == "__main__":
    sys.exit(main())
