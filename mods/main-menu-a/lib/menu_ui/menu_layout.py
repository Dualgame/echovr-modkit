#!/usr/bin/env python3
"""Apply several menu-layout DATA edits to an existing loose tree in one pass (rail/help layout; ).
Every op is gated on its own (byte-exact re-decode, undo-diff back to its input, existing owners unchanged) and runs
in command-line order on the output of the previous op. Output = the touched files + a copy of the rest of --base.

    python3 lib/menu_ui/menu_layout.py mnu_master --base TREE --out TREE [--build pcf] [--dry-run]
        [--member ACTOR:PAGE] [--unmember ACTOR:PAGE] [--show ACTOR:PAGE] [--unshow ACTOR:PAGE]
        [--text ACTOR=TEXT] [--tooltip ACTOR=TEXT]
        [--trans ACTOR:x,y,z] [--scale ACTOR:s] [--bind OWNER[/COMPONENT]:KEY=VALUE] [--pointer-size ACTOR:W,H]
        [--sym ACTOR:KEY=SYMBOL] [--clone-module OWNER:RECORD[:COMPONENT] [--clone-bind COMPONENT:KEY=VALUE ...]]

Conventions: ACTOR / OWNER = 0x CSymbol64 actor id. PAGE = page INDEX (the CR15UIPage2CR record position, = runtime
page number) or a page OWNER id. VALUE = 0x id or `pN` (= page N's owner, read from the current page file: slot-552
`*_page` rows hold page OWNER actor ids, e.g. home rec 27 credits_page = p31 owner 0x429507a504de0707). KEY = 0x key
or a variable name of the record's module (`VAR_KEYS`). COMPONENT = name (csymbol64) or 0x symbol.

Ops (PC 6323983201049540 `stream-confirmed` framing, page_heap / script_heap / ui_actor_mint layouts):
* --member: the actor's existing PLAIN element (all eight descriptors empty; rail buttons on p24-27) is appended to
  PAGE's +32 array (ui_actor_mint.page_add_plain, member mode). Refused when already a member or no plain donor.
* --unmember: the actor's one element on PAGE is removed together with its nested payloads (any shape page_heap
  decodes: plain, or card = +80 x1 / +416 x3, e.g. TUTORIALS 0xd763e348e835f249 on p24). Elements after it move
  down one index; nothing else in the file names an element index (arrays carry no pointers).
* --show / --unshow: the 16-byte entry (ACTOR, 0) is appended to / removed from PAGE's +88 array (the page's
  shown non-element actors, e.g. the logo 0x6d94aa64ae5c56fe on p24-27; stride 16, second qword 0 on every stock
  entry) and the +88 descriptor's count / size follow. Refused when ACTOR is already listed (--show) or absent
  (--unshow), or a non-zero second qword sits in the array. Same splice-and-gate shape as --member.
* --text / --tooltip: the +496 row keyed initial_text / description (button module e2868929d1ed191a) of every
  CScriptCR record of the actor, rewritten in place (64-byte NUL-padded field; EVERY hit row, e.g. rail rec 91
  carries two description rows, +496[0] empty and [2]). Refused if no row hits.
* --trans / --scale: the actor's one CTransformCR row (trans @+0x30, scale 3 f32 @+0x3c). --trans also mirrors into
  the ADR world QS (dyn2 +16) and the SGActorData row (+32) when those equal the old row translation (unparented
  actor, as ui_actor_mint mints); otherwise only CTransformCR moves and the report says so.
* --pose-rows: --pose for an actor with several byte-equal CTransformCR rows (settings arrows, 9 rows): all rows move,
  plus ADR + SGActorData; a parented actor only when its ADR/SGActorData pose equals the old row (world-valued).
  Its CR15VRMenuWidgetCR row (the world-space POINTER HIT box, centre +32 / quat +44) is carried rigidly too.
  --pose carries it the same way when the 1-row actor has one; the report's `widget` names the row or says
  "no CR15VRMenuWidgetCR row" (cards and tiles: their hit is the CR15PointerMeshCR quad on the ADR pose).
* --sym: the +328 symbol row(s) keyed KEY (0x key or click_event / text_element / highlight_element) of every
  CScriptCR record of ACTOR get SYMBOL (0x id or a name, csymbol64), in place (e.g. a tile's click_event).
* --bind: the slot-552 row with KEY in the owner's records (one record, or the one named COMPONENT) gets VALUE.
* --clone-module: page_add.add_module(allow_extra) - a clone of stock record RECORD as an extra component of OWNER,
  named COMPONENT (default `pageactionscript_<n>`, n = the owner's record count, bumped until unique); its
  --clone-bind rows (by COMPONENT) are applied inside that gate.

Runtime semantics stay `inferred` until a device run: the rail buttons are routed by COMPILED uid constants in the
itself give them an action there.
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
sys.path.insert(0, str(HERE))
import page_add
import ui_actor_mint as uam
from page_heap import decode as page_decode, ELEMENT_SLOTS
from script_heap import decode as script_decode
from page_patch import _desc, ELEM
from binding_patch import _winpath

PAGE, SCRIPT, TRANS, ADR, SCENE = ("CR15UIPage2CR", "CScriptCR", "CTransformCR", "CActorDataResource",
                                   "CGSceneResource")
CANVAS, PMESH, BBOARD = "CCanvasUICR", "CR15PointerMeshCR", "CBlackboardCR"
WIDGET, WREC = "CR15VRMenuWidgetCR", 96
TOP, PREC, SREC, TREC = 56, 328, 720, 176
BUTTON_MODULE, HOME_MODULE = 0xe2868929d1ed191a, 0x991bbec05cde117f
VAR_KEYS = {
    HOME_MODULE: dict(frame=0x61ecc3142afd8634, blackboard=0x67ba964628a7d236, quit_page=0x61ee96422bac8034,
                      eula_page=0x6ee9c54f7bfdd237, credits_page=0x63e8c2452dfdd137,
                      settings_page=0x34bacc132ffddc36, group_select_page=0x63b4c5417ea7873e,
                      servers_down_page=0x35b4924171a8d634, group_legal_notif_page=0x65b4c0157ca6d763,
                      begin_multiplayer_page=0x37bbc34f2bfdd33f, invite_friends_button=0x67b4924e2ffa8662,
                      tutorial_card=0x6fbfc1417aa9803f, play_card=0x35eccd412aa98530,
                      join_card=0x32be954f7ba98532, create_card=0x33e8c04471a8d736,
                      create_page=0x61ebc34779a7853e, join_page=0x65bacc462ffbd73e,
                      view_link_page=0x61bacc147dfadc31, change_group_button=0x6fe8c1162da7863f,
                      quit_info_page=0x66b9c6422bfd8562, lock_out_timer_widget=0x66bd95132aac8634),
    BUTTON_MODULE: dict(initial_text=uam.KEY_INITIAL_TEXT, description=uam.KEY_DESCRIPTION),
}


def _q(data: bytes, off: int) -> int:
    return struct.unpack_from("<Q", data, off)[0]


def _n(data: bytes) -> int:
    return _q(data, 0x28)


def page_owners(data: bytes) -> list[int]:
    return [_q(data, TOP + i * PREC + 8) for i in range(_n(data))]


def script_owners(data: bytes) -> list[int]:
    return [_q(data, TOP + i * SREC + 8) for i in range(_n(data))]


def page_index(data: bytes, page: int) -> int:
    """PAGE index, or a page owner id -> its index."""
    po = page_owners(data)
    if 0 <= page < len(po):
        return page
    if page in po:
        return po.index(page)
    raise ValueError(f"{page:#x} is neither a page index (0..{len(po) - 1}) nor a page owner")


def _changed(a: bytes, b: bytes) -> list[int]:
    return [i for i in range(len(a)) if a[i] != b[i]]


def member(data: bytes, actor: int, page: int) -> tuple[bytes, dict]:
    p = page_index(data, page)
    out, rep = uam.page_add_plain(data, p, actor, actor, member=True)
    if page_owners(out) != page_owners(data):
        raise ValueError("page owners changed")
    return out, dict(op="member", actor=f"0x{actor:016x}", **rep, size=f"{len(data)}->{len(out)}")


def unmember(data: bytes, actor: int, page: int) -> tuple[bytes, dict]:
    """Remove `actor`'s element (and its nested payloads) from page `page`'s +32 array."""
    rebuilt, arrays, elements = page_decode(data)
    if rebuilt != data:
        raise ValueError("CR15UIPage2CR does not round-trip")
    p = page_index(data, page)
    hit = [e for e in elements if e["page"] == p and int(e["word0"], 16) == actor]
    if len(hit) != 1:
        raise ValueError(f"0x{actor:016x} has {len(hit)} elements on page {p}; need exactly 1")
    e = hit[0]
    els = next(a for a in arrays if a["page"] == p and a["element"] < 0 and a["slot"] == 32)
    n, d_off = els["count"], els["descriptor_file_offset"]
    old_desc = data[d_off:d_off + 56]
    if old_desc != _desc(n, ELEM):
        raise ValueError(f"page {p} +32 descriptor not the expected shape")
    nested = [a for a in arrays if a["page"] == p and a["element"] == e["element"]]
    if [a["slot"] for a in nested] != list(ELEMENT_SLOTS):
        raise ValueError("unexpected nested array set")
    nest_at = nested[0]["payload_file_offset"]
    for a, b in zip(nested, nested[1:]):
        if a["payload_file_offset"] + a["size"] != b["payload_file_offset"]:
            raise ValueError("nested payloads not contiguous")
    nest_len = sum(a["size"] for a in nested)
    el_at = e["element_file_offset"]
    if not el_at + ELEM <= nest_at:
        raise ValueError("unexpected layout: nested payload before the element")
    shape = {a["slot"]: a["count"] for a in nested if a["count"]}
    el_blob, nest_blob = data[el_at:el_at + ELEM], data[nest_at:nest_at + nest_len]
    out = bytearray(data[:nest_at] + data[nest_at + nest_len:])
    del out[el_at:el_at + ELEM]
    out[d_off:d_off + 56] = _desc(n - 1, ELEM)
    out = bytes(out)
    rb, _, el2 = page_decode(out)
    if rb != out:
        raise ValueError("page re-decode failed")
    back = bytearray(out[:el_at] + el_blob + out[el_at:])
    back[nest_at:nest_at] = nest_blob
    back[d_off:d_off + 56] = old_desc
    if bytes(back) != data:
        raise ValueError("page undo-diff failed")
    old = [(x["page"], x["word0"]) for x in elements]
    old.pop(elements.index(e))
    if [(x["page"], x["word0"]) for x in el2] != old or page_owners(out) != page_owners(data):
        raise ValueError("element list / page owners differ beyond the removed element")
    return out, dict(op="unmember", actor=f"0x{actor:016x}", page=p, index=e["element"], count=f"{n}->{n - 1}",
                     shape="plain" if not shape else {f"+{k}": v for k, v in shape.items()},
                     removed=f"0x{el_at:x}+{ELEM}, 0x{nest_at:x}+{nest_len}", size=f"{len(data)}->{len(out)}")


SHOW_SLOT, SHOW_STRIDE = 88, 16


def _show_array(data: bytes, page: int):
    rebuilt, arrays, elements = page_decode(data)
    if rebuilt != data:
        raise ValueError("CR15UIPage2CR does not round-trip")
    p = page_index(data, page)
    a = next(x for x in arrays if x["page"] == p and x["element"] < 0 and x["slot"] == SHOW_SLOT)
    n, d_off, at = a["count"], a["descriptor_file_offset"], a["payload_file_offset"]
    old_desc = data[d_off:d_off + 56]
    if old_desc != _desc(n, SHOW_STRIDE) and not (n == 0 and old_desc == _desc(0, 0)):
        raise ValueError(f"page {p} +{SHOW_SLOT} descriptor not the expected shape")
    ents = [struct.unpack_from("<QQ", data, at + i * SHOW_STRIDE) for i in range(n)]
    if any(w1 for _, w1 in ents):
        raise ValueError(f"page {p} +{SHOW_SLOT} has a non-zero second qword: unknown entry shape")
    return p, n, d_off, at, old_desc, [w0 for w0, _ in ents], elements, arrays


def _show_gate(data: bytes, out: bytes, p: int, elements, arrays, back: bytes) -> None:
    rb, ar2, el2 = page_decode(out)
    if rb != out:
        raise ValueError("page re-decode failed")
    if back != data:
        raise ValueError("page undo-diff failed")
    if [(e["page"], e["word0"]) for e in el2] != [(e["page"], e["word0"]) for e in elements] \
            or page_owners(out) != page_owners(data):
        raise ValueError("element list / page owners changed")
    k = lambda xs: [(x["page"], x["element"], x["slot"], x["count"]) for x in xs
                    if not (x["page"] == p and x["element"] < 0 and x["slot"] == SHOW_SLOT)]
    if k(ar2) != k(arrays):
        raise ValueError("an array other than the page's +88 changed")


def show(data: bytes, actor: int, page: int) -> tuple[bytes, dict]:
    """Append (actor, 0) to page `page`'s +88 array (: the logo on HELP / SETTINGS)."""
    p, n, d_off, at, old_desc, listed, elements, arrays = _show_array(data, page)
    if actor in listed:
        raise ValueError(f"0x{actor:016x} is already in page {p}'s +{SHOW_SLOT} array")
    ins = at + n * SHOW_STRIDE
    out = bytearray(data[:ins] + struct.pack("<QQ", actor, 0) + data[ins:])
    out[d_off:d_off + 56] = _desc(n + 1, SHOW_STRIDE)
    out = bytes(out)
    back = bytearray(out[:ins] + out[ins + SHOW_STRIDE:])
    back[d_off:d_off + 56] = old_desc
    _show_gate(data, out, p, elements, arrays, bytes(back))
    return out, dict(op="show", actor=f"0x{actor:016x}", page=p, index=n, count=f"{n}->{n + 1}",
                     size=f"{len(data)}->{len(out)}")


def unshow(data: bytes, actor: int, page: int) -> tuple[bytes, dict]:
    """Remove actor's (actor, 0) entry from page `page`'s +88 array (mirror of show)."""
    p, n, d_off, at, old_desc, listed, elements, arrays = _show_array(data, page)
    if listed.count(actor) != 1:
        raise ValueError(f"0x{actor:016x} is listed {listed.count(actor)} times in page {p}'s +{SHOW_SLOT}; need 1")
    i = listed.index(actor)
    rm = at + i * SHOW_STRIDE
    blob = data[rm:rm + SHOW_STRIDE]
    out = bytearray(data[:rm] + data[rm + SHOW_STRIDE:])
    out[d_off:d_off + 56] = _desc(n - 1, SHOW_STRIDE)
    out = bytes(out)
    back = bytearray(out[:rm] + blob + out[rm:])
    back[d_off:d_off + 56] = old_desc
    _show_gate(data, out, p, elements, arrays, bytes(back))
    return out, dict(op="unshow", actor=f"0x{actor:016x}", page=p, index=i, count=f"{n}->{n - 1}",
                     size=f"{len(data)}->{len(out)}")


def _records(data: bytes, owner: int, component: int | None = None) -> list[int]:
    recs = [i for i, o in enumerate(script_owners(data)) if o == owner
            and (component is None or _q(data, TOP + i * SREC) == component)]
    if not recs:
        raise ValueError(f"0x{owner:016x} has no CScriptCR record" +
                         (f" named 0x{component:016x}" if component is not None else ""))
    return recs


def resolve_key(data: bytes, recs: list[int], key: str) -> int:
    try:
        return int(key, 0)
    except ValueError:
        pass
    mods = {_q(data, TOP + r * SREC + 32) for r in recs}
    got = {VAR_KEYS[m][key] for m in mods if key in VAR_KEYS.get(m, {})}
    if len(got) != 1:
        raise ValueError(f"variable {key!r} names {len(got)} keys over modules "
                         f"{sorted(f'0x{m:016x}' for m in mods)}; give the 0x key")
    return got.pop()


def _gate_inplace(data: bytes, out: bytes, spans: list[tuple[int, int]], what: str) -> None:
    rb, _, _ = script_decode(out) if what == SCRIPT else (out, 0, 0)
    if rb != out or len(out) != len(data):
        raise ValueError(f"{what} re-decode failed")
    back = bytearray(out)
    for o, n in spans:
        back[o:o + n] = data[o:o + n]
    if bytes(back) != data or any(not any(o <= i < o + n for o, n in spans) for i in _changed(data, out)):
        raise ValueError(f"{what}: bytes changed outside the edited fields")


def set_text(data: bytes, actor: int, key: int, text: str, label: str = "text") -> tuple[bytes, dict]:
    """Rewrite the +496 row(s) keyed `key` on every CScriptCR record of `actor` (64-byte NUL-padded field)."""
    raw = text.encode("ascii")
    if len(raw) > 63:
        raise ValueError(f"{text!r} > 63 bytes")
    _, arrays, _ = script_decode(data)
    recs = set(_records(data, actor))
    out, spans, rows = bytearray(data), [], []
    for a in (x for x in arrays if x["record"] in recs and x["slot"] == 496):
        for i in range(a["count"]):
            o = a["payload_file_offset"] + i * 80
            if _q(data, o) == key:
                cur = data[o + 16:o + 80].split(b"\0")[0].decode("ascii", "replace")
                out[o + 16:o + 80] = raw.ljust(64, b"\0")
                spans.append((o + 16, 64))
                rows.append(f"rec {a['record']} +496[{i}] {cur!r}->{text!r}")
    if not rows:
        raise ValueError(f"no +496 row keyed 0x{key:016x} on 0x{actor:016x}'s records {sorted(recs)}")
    out = bytes(out)
    _gate_inplace(data, out, spans, SCRIPT)
    if script_owners(out) != script_owners(data):
        raise ValueError("script owners changed")
    return out, dict(op=label, actor=f"0x{actor:016x}", rows=rows)


SYM_KEYS = dict(click_event=uam.KEY_CLICK_EVENT, text_element=uam.KEY_TEXT_ELEMENT,
                highlight_element=uam.KEY_HIGHLIGHT_ELEMENT)


def set_sym(data: bytes, actor: int, key: int, value: int) -> tuple[bytes, dict]:
    """: rewrite the value word of the +328 row(s) keyed `key` on every CScriptCR record of `actor` (24-byte rows
    key, ?, symbol; e.g. the button module's click_event), in place. Refused if no row hits."""
    _, arrays, _ = script_decode(data)
    recs = set(_records(data, actor))
    out, spans, rows = bytearray(data), [], []
    for a in (x for x in arrays if x["record"] in recs and x["slot"] == 328):
        for i in range(a["count"]):
            o = a["payload_file_offset"] + i * 24
            if _q(data, o) == key:
                rows.append(f"rec {a['record']} +328[{i}] 0x{_q(data, o + 16):016x}->0x{value:016x}")
                struct.pack_into("<Q", out, o + 16, value)
                spans.append((o + 16, 8))
    if not rows:
        raise ValueError(f"no +328 row keyed 0x{key:016x} on 0x{actor:016x}'s records {sorted(recs)}")
    out = bytes(out)
    _gate_inplace(data, out, spans, SCRIPT)
    if script_owners(out) != script_owners(data):
        raise ValueError("script owners changed")
    return out, dict(op="sym", actor=f"0x{actor:016x}", rows=rows)


def bind(data: bytes, owner: int, key: str, value: int, component: int | None = None) -> tuple[bytes, dict]:
    """Set the value word of the one slot-552 row keyed `key` over `owner`'s records (or its `component`)."""
    recs = _records(data, owner, component)
    k = resolve_key(data, recs, key)
    _, arrays, _ = script_decode(data)
    hits = [(a["record"], i, a["payload_file_offset"] + i * 32) for a in arrays
            if a["record"] in recs and a["slot"] == 552 for i in range(a["count"])
            if _q(data, a["payload_file_offset"] + i * 32) == k]
    if len(hits) != 1:
        raise ValueError(f"key 0x{k:016x} sits in {len(hits)} slot-552 rows of records {recs}; need 1 "
                         "(name the component: OWNER/COMPONENT)")
    r, i, o = hits[0]
    old = _q(data, o + 16)
    out = bytearray(data)
    struct.pack_into("<Q", out, o + 16, value)
    out = bytes(out)
    _gate_inplace(data, out, [(o + 16, 8)], SCRIPT)
    if script_owners(out) != script_owners(data):
        raise ValueError("script owners changed")
    return out, dict(op="bind", owner=f"0x{owner:016x}", record=r, row=i, key=f"0x{k:016x}" +
                     (f" ({key})" if not key.startswith("0x") else ""),
                     value=f"0x{old:016x}->0x{value:016x}", noop=old == value)


def component_sym(text: str) -> int:
    return int(text, 0) if text.lower().startswith("0x") else page_add.mint_owner(text)


def default_component(data: bytes, owner: int) -> tuple[str, int]:
    recs = [i for i, o in enumerate(script_owners(data)) if o == owner]
    taken = {_q(data, TOP + i * SREC) for i in recs}
    k = max(len(recs), 1)
    while page_add.mint_owner(f"pageactionscript_{k}") in taken:
        k += 1
    return f"pageactionscript_{k}", page_add.mint_owner(f"pageactionscript_{k}")


def clone_module(data: bytes, owner: int, record: int, component: str | None = None,
                 binds: list[tuple[str, int]] = ()) -> tuple[bytes, dict]:
    name, sym = default_component(data, owner) if component is None else (component, component_sym(component))
    kb = {resolve_key(data, [record], k): v for k, v in binds}
    if len(kb) != len(binds):
        raise ValueError("a --clone-bind key repeats")
    before = script_owners(data)
    out, rep = page_add.add_module(data, owner, record, kb, sym, allow_extra=True)
    if script_owners(out)[:len(before)] != before:
        raise ValueError("existing script owners changed")
    return out, dict(op="clone-module", component=name, **rep)


def set_transform(data: bytes, actor: int, trans=None, scale=None) -> tuple[bytes, dict]:
    rows = uam._rows(data, TREC, actor)
    if len(rows) != 1:
        raise ValueError(f"0x{actor:016x} has {len(rows)} CTransformCR rows; need 1")
    o = TOP + rows[0] * TREC
    if _q(data, 8) != len(data) - TOP or _n(data) * TREC != len(data) - TOP:
        raise ValueError("not a flat 176-byte CTransformCR table")
    out = bytearray(data)
    old_t, old_s = struct.unpack_from("<3f", data, o + 0x30), struct.unpack_from("<3f", data, o + 0x3c)
    rep = dict(op="transform", actor=f"0x{actor:016x}", row=rows[0])
    if trans is not None:
        struct.pack_into("<3f", out, o + 0x30, *trans)
        rep["trans"] = f"{tuple(round(x, 4) for x in old_t)}->{tuple(trans)}"
    if scale is not None:
        struct.pack_into("<3f", out, o + 0x3c, scale, scale, scale)
        rep["scale"] = f"{tuple(round(x, 4) for x in old_s)}->{scale}"
    out = bytes(out)
    _gate_inplace(data, out, [(o + 0x30, 24)], TRANS)
    if [_q(out, TOP + i * TREC + 8) for i in range(_n(out))] != [_q(data, TOP + i * TREC + 8) for i in range(_n(data))]:
        raise ValueError("transform owners changed")
    return out, rep


def _close(a, b) -> bool:
    return all(abs(x - y) < 1e-4 for x, y in zip(a, b))


def mirror_world(adr_data: bytes, scene_data: bytes, actor: int, old, new) -> tuple[bytes | None, bytes | None, str]:
    """ADR dyn2 +16 and the actor's SGActorData +32 translation -> `new`, only when both equal `old`."""
    v = uam.adr_view(adr_data)
    if actor not in v["actor_key"]:
        return None, None, "actor not in ADR: CTransformCR only"
    i = v["actor_key"].index(actor)
    if not _close(struct.unpack_from("<3f", v["d2"][i], 16), old):
        return None, None, "ADR world != CTransformCR (parented?): CTransformCR only"
    w = dict(v)
    w["d2"] = list(v["d2"])
    d2 = bytearray(v["d2"][i])
    struct.pack_into("<3f", d2, 16, *new)
    w["d2"][i] = bytes(d2)
    a = uam.adr_pack(w)
    ch = _changed(adr_data, a) if len(a) == len(adr_data) else [-1]
    if not ch or ch[0] < 0 or ch[-1] - ch[0] >= 12:
        raise ValueError("ADR mirror changed more than one translation")
    b0, b1 = uam.adr_resolve(adr_data), uam.adr_resolve(a)
    if any(b1[k] != b0[k] for k in b0 if k != actor):
        raise ValueError("ADR mirror: another actor resolves differently")
    s = None
    rank = uam._sga_rank(v, i)
    if rank is not None:
        sc = uam.gsc.read(scene_data)
        rec = bytearray(sc["actors"][1][rank * 56:(rank + 1) * 56])
        if not _close(struct.unpack_from("<3f", rec, 32), old):
            raise ValueError("SGActorData translation != ADR world; refusing a half mirror")
        struct.pack_into("<3f", rec, 32, *new)
        s2 = dict(sc)
        s2["actors"] = (sc["actors"][0], sc["actors"][1][:rank * 56] + bytes(rec) + sc["actors"][1][(rank + 1) * 56:])
        s = uam.gsc.write(s2)
        ch = _changed(scene_data, s) if len(s) == len(scene_data) else [-1]
        if not ch or ch[0] < 0 or ch[-1] - ch[0] >= 12 or uam.gsc.write(uam.gsc.read(s)) != s:
            raise ValueError("scene mirror changed more than one translation")
    return a, s, f"mirrored: ADR actor {i}" + (f", SGActorData {rank}" if rank is not None else ", no SGActorData row")


def set_pose(files: dict, actor: int, trans, quat=None, scale=None) -> tuple[dict, dict]:
    """--pose: CTransformCR trans (+0x30) / quat (+0x20) / scale (+0x3c, 3 f32) AND the actor's world pose in ADR
    dyn2 (quat +0, trans +16, scale +28) and its SGActorData row (quat +16, trans +32, uniform scale +44), written
    drawing, but its pointer hit then answered neither at the old nor the new place (raw right-click, PC
    6323983201049540). Unparented actors only: a parented actor's world pose is not its row."""
    rows = uam._rows(files[TRANS], TREC, actor)
    if len(rows) != 1:
        raise ValueError(f"0x{actor:016x} has {len(rows)} CTransformCR rows; need 1")
    o = TOP + rows[0] * TREC
    old_q, old_t = struct.unpack_from("<4f", files[TRANS], o + 0x20), struct.unpack_from("<3f", files[TRANS], o + 0x30)
    out = bytearray(files[TRANS])
    struct.pack_into("<3f", out, o + 0x30, *trans)
    if quat is not None:
        struct.pack_into("<4f", out, o + 0x20, *quat)
    if scale is not None:
        struct.pack_into("<3f", out, o + 0x3c, scale, scale, scale)
    out = bytes(out)
    _gate_inplace(files[TRANS], out, [(o + 0x20, 40)], TRANS)
    res, rep = {TRANS: out}, dict(op="pose", actor=f"0x{actor:016x}", trans=tuple(trans), quat=quat, scale=scale)
    world, rep["world"] = _world_pose(files, actor, trans, quat, scale)
    res.update(world)
    if WIDGET in files:
        wd, rep["widget"] = _widget_pose(files[WIDGET], actor, old_t, old_q, trans, quat)
        if wd is not None:
            res[WIDGET] = wd
    return res, rep


def _world_pose(files: dict, actor: int, trans, quat=None, scale=None, allow_parented_if=None) -> tuple[dict, str]:
    """The ADR dyn2 (quat +0, trans +16, scale +28) and SGActorData (quat +16, trans +32, scale +44) half of --pose /
    --pose-rows. An unparented actor is always written. A PARENTED actor is written only when allow_parented_if =
    (old_q, old_t) is given and its ADR dyn2 quat+trans AND its SGActorData quat+trans equal it (world-valued data, as
    on the settings arrows 0xdd45867f6ff95898 / 0x8ecb1ba00a7781a2, `stream-confirmed` ffa_w0); else refused."""
    v = uam.adr_view(files[ADR])
    if actor not in v["actor_key"]:
        return {}, "actor not in ADR: CTransformCR only"
    i = v["actor_key"].index(actor)
    parented = v["d4"][i] != 0xffff
    if parented:
        if allow_parented_if is None:
            raise ValueError(f"0x{actor:016x} is parented (ADR parent {v['d4'][i]}); --pose writes world poses only")
        oq, ot = allow_parented_if
        if not (_close(struct.unpack_from("<4f", v["d2"][i], 0), oq)
                and _close(struct.unpack_from("<3f", v["d2"][i], 16), ot)):
            raise ValueError(f"0x{actor:016x} is parented and its ADR pose != its CTransformCR row; refusing")
    w = dict(v)
    w["d2"] = list(v["d2"])
    d2 = bytearray(v["d2"][i])
    struct.pack_into("<3f", d2, 16, *trans)
    if quat is not None:
        struct.pack_into("<4f", d2, 0, *quat)
    if scale is not None:
        struct.pack_into("<3f", d2, 28, scale, scale, scale)
    w["d2"][i] = bytes(d2)
    a = uam.adr_pack(w)
    b0, b1 = uam.adr_resolve(files[ADR]), uam.adr_resolve(a)
    if len(a) != len(files[ADR]) or any(b1[k] != b0[k] for k in b0 if k != actor):
        raise ValueError("ADR pose: another actor resolves differently")
    res = {ADR: a}
    rank = uam._sga_rank(v, i)
    if rank is not None:
        sc = uam.gsc.read(files[SCENE])
        rec = bytearray(sc["actors"][1][rank * 56:(rank + 1) * 56])
        if parented and not (_close(struct.unpack_from("<4f", rec, 16), allow_parented_if[0])
                             and _close(struct.unpack_from("<3f", rec, 32), allow_parented_if[1])):
            raise ValueError(f"0x{actor:016x}: SGActorData pose != its CTransformCR row; refusing a half move")
        struct.pack_into("<3f", rec, 32, *trans)
        if quat is not None:
            struct.pack_into("<4f", rec, 16, *quat)
        if scale is not None:
            struct.pack_into("<f", rec, 44, scale)
        s2 = dict(sc)
        s2["actors"] = (sc["actors"][0], sc["actors"][1][:rank * 56] + bytes(rec) + sc["actors"][1][(rank + 1) * 56:])
        sb = uam.gsc.write(s2)
        if len(sb) != len(files[SCENE]) or uam.gsc.write(uam.gsc.read(sb)) != sb:
            raise ValueError("scene pose did not round-trip in place")
        res[SCENE] = sb
    return res, (f"ADR actor {i}" + (f" (parented {v['d4'][i]}, world-valued)" if parented else "")
                 + (f", SGActorData {rank}" if rank is not None else ", no SGActorData row"))


def set_pose_rows(files: dict, actor: int, trans, quat=None, scale=None) -> tuple[dict, dict]:
    """--pose-rows: --pose for an actor with SEVERAL CTransformCR rows, or ONE row on a parented actor (the settings
    slab 0x0bb67ae2a9f6b56c, w12b; --pose refuses parented actors) (the settings arrows: row ncaTransform + 8 rows
    named 0x7874b6ce7e1d57b8..bf, all byte-equal in q/t/s; w8-settings-recipe-2026-10-30.md (c)). Every row moves;
    refused unless all rows are equal first (stock invariant). ADR + SGActorData move as in --pose, and a parented
    actor is accepted only when its ADR/SGActorData pose equals the old row (world-valued)."""
    d = files[TRANS]
    rows = uam._rows(d, TREC, actor)
    if len(rows) == 1:
        v = uam.adr_view(files[ADR])
        if actor not in v["actor_key"] or v["d4"][v["actor_key"].index(actor)] == 0xffff:
            raise ValueError(f"0x{actor:016x} has 1 unparented CTransformCR row; use --pose for a 1-row actor")
    elif len(rows) < 2:
        raise ValueError(f"0x{actor:016x} has {len(rows)} CTransformCR rows; use --pose for a 1-row actor")
    blk = lambda r: d[TOP + r * TREC + 0x20: TOP + r * TREC + 0x48]
    if any(blk(r) != blk(rows[0]) for r in rows):
        raise ValueError(f"0x{actor:016x}: CTransformCR rows differ; refusing a partial move")
    o0 = TOP + rows[0] * TREC
    old_q, old_t = struct.unpack_from("<4f", d, o0 + 0x20), struct.unpack_from("<3f", d, o0 + 0x30)
    out = bytearray(d)
    for r in rows:
        o = TOP + r * TREC
        struct.pack_into("<3f", out, o + 0x30, *trans)
        if quat is not None:
            struct.pack_into("<4f", out, o + 0x20, *quat)
        if scale is not None:
            struct.pack_into("<3f", out, o + 0x3c, scale, scale, scale)
    out = bytes(out)
    _gate_inplace(d, out, [(TOP + r * TREC + 0x20, 40) for r in rows], TRANS)
    rep = dict(op="pose-rows", actor=f"0x{actor:016x}", rows=rows, trans=tuple(trans), quat=quat, scale=scale,
               old=dict(q=tuple(round(x, 5) for x in old_q), t=tuple(round(x, 5) for x in old_t)))
    world, rep["world"] = _world_pose(files, actor, trans, quat, scale, allow_parented_if=(old_q, old_t))
    res = {TRANS: out, **world}
    if WIDGET in files:
        wd, rep["widget"] = _widget_pose(files[WIDGET], actor, old_t, old_q, trans, quat)
        if wd is not None:
            res[WIDGET] = wd
    return res, rep


def _qmul(a, b):
    ax, ay, az, aw = a
    bx, by, bz, bw = b
    return (aw * bx + ax * bw + ay * bz - az * by, aw * by - ax * bz + ay * bw + az * bx,
            aw * bz + ax * by - ay * bx + az * bw, aw * bw - ax * bx - ay * by - az * bz)


def _qnorm(q):
    n = sum(x * x for x in q) ** 0.5
    return tuple(x / n for x in q)


def _qrot(q, v):
    x, y, z, _ = _qmul(_qmul(q, (*v, 0.0)), (-q[0], -q[1], -q[2], q[3]))
    return (x, y, z)


def _widget_pose(data: bytes, actor: int, old_t, old_q, trans, quat=None,
                 reach: float = 1.0) -> tuple[bytes | None, str]:
    """The pointer hit of a VR menu widget (the settings arrows): its CR15VRMenuWidgetCR row holds a WORLD-space box
    (centre +32, quat +44, half-extents +60), not tied to CTransformCR / ADR / SGActorData. ffa_w8a (`device-observed`
    @ PC 6323983201049540, 2026-10-30) moved the arrows with --pose-rows but not this row: they drew at the new place
    and a click there did nothing, while a click at the old, empty spot still paged. Stock next arrow: box centre
    (-2.9883, -0.5199, 5.0451) vs row (-3.0378, -0.5231, 5.1127). The box is carried rigidly with the actor:
    centre' = t' + R(q' q^-1)(centre - t), quat' = (q' q^-1) quat. No row -> nothing; > 1 row, or a centre farther than
    `reach` m from the old row (not world-valued), -> refused."""
    rows = uam._rows(data, WREC, actor)
    if not rows:
        return None, "no CR15VRMenuWidgetCR row"
    if len(rows) != 1:
        raise ValueError(f"0x{actor:016x} has {len(rows)} {WIDGET} rows; need 0 or 1")
    if _q(data, 8) != len(data) - TOP or _n(data) * WREC != len(data) - TOP:
        raise ValueError(f"not a flat {WREC}-byte {WIDGET} table")
    o = TOP + rows[0] * WREC
    c, bq = struct.unpack_from("<3f", data, o + 32), struct.unpack_from("<4f", data, o + 44)
    if sum((a - b) ** 2 for a, b in zip(c, old_t)) ** 0.5 > reach:
        raise ValueError(f"0x{actor:016x}: {WIDGET} box centre {c} is not near its CTransformCR row {tuple(old_t)}; "
                         "refusing (not world-valued?)")
    dq = _qmul(_qnorm(quat), (-old_q[0], -old_q[1], -old_q[2], old_q[3])) if quat is not None else (0.0, 0.0, 0.0, 1.0)
    dq = _qnorm(dq)
    off = _qrot(dq, tuple(a - b for a, b in zip(c, old_t)))
    nc = tuple(t + d for t, d in zip(trans, off))
    nq = _qnorm(_qmul(dq, bq)) if quat is not None else bq
    out = bytearray(data)
    struct.pack_into("<3f", out, o + 32, *nc)
    struct.pack_into("<4f", out, o + 44, *nq)
    out = bytes(out)
    _gate_inplace(data, out, [(o + 32, 28)], WIDGET)
    if [_q(out, TOP + i * WREC + 8) for i in range(_n(out))] != [_q(data, TOP + i * WREC + 8) for i in range(_n(data))]:
        raise ValueError(f"{WIDGET} owners changed")
    return out, (f"row {rows[0]}: centre {tuple(round(x, 4) for x in c)}->{tuple(round(x, 4) for x in nc)}, "
                 f"quat {tuple(round(x, 4) for x in bq)}->{tuple(round(x, 4) for x in nq)}")


def set_canvas_scale(files: dict, actor: int, k: float) -> tuple[dict, dict]:
    """--canvas-scale: shrink/grow an actor's drawn canvas WITHOUT a transform scale. CCanvasUICR row body
    +8/+12 (two f32 after the canvas symbol, 1.0 in every stock row; `inferred` canvas x/y scale) and the
    6323983201049540): a CTransformCR scale != 1 draws the page-5 tiles smaller but kills their pointer hit (hover
    n=0, also with the mesh resized); this pair draws them at 0.56 and hover + click still resolve."""
    res, rep = {}, dict(op="canvas-scale", actor=f"0x{actor:016x}", k=k)
    for cls, stride, off in ((CANVAS, 88, 40), (PMESH, 80, 32)):
        d = files[cls]
        rows = uam._rows(d, stride, actor)
        if len(rows) != 1:
            raise ValueError(f"0x{actor:016x} has {len(rows)} {cls} rows; need 1")
        o = TOP + rows[0] * stride + off
        out = bytearray(d)
        a, b_ = struct.unpack_from("<2f", d, o)
        struct.pack_into("<2f", out, o, a * k, b_ * k)
        out = bytes(out)
        _gate_inplace(d, out, [(o, 8)], cls)
        res[cls] = out
        rep[cls] = f"({a:.4g}, {b_:.4g}) -> ({a * k:.4g}, {b_ * k:.4g})"
    return res, rep


def _bb_walk(d: bytes) -> tuple[int, list[tuple[int, list[tuple[int, int]]]]]:
    """CBlackboardCR: 56-B header (+8 top size, +40 count, +48 iused), 592-B rows = 32-B prefix (owner +8) + ten
    56-B descriptors (+8 payload size), payloads record-major from the end of the top table, no tail
    (`stream-confirmed` PC 6323983201049540, final-pc-page-blackboard.md). -> (top end, [(row off, [(pay off, size)])])."""
    top, n = _q(d, 8), _q(d, 40)
    if top != n * 592 or _q(d, 48) != n:
        raise ValueError("CBlackboardCR header is not the 592-byte row table")
    cur, rows = TOP + top, []
    for r in range(n):
        o = TOP + r * 592
        pays = []
        for i in range(10):
            sz = _q(d, o + 32 + 56 * i + 8)
            pays.append((cur, sz))
            cur += sz
        rows.append((o, pays))
    if cur != len(d):
        raise ValueError("CBlackboardCR payloads do not end at EOF")
    return TOP + top, rows


def blackboard_clone(data: bytes, donor: int, new: int) -> tuple[bytes, dict]:
    """--blackboard-from DONOR:NEW: give a minted card owner the donor's CBlackboardCR row (same keys, flag 1).
    A card / any element with descriptor data writes into its OWNER's blackboard; without one the mnu_master
    load asserts 'has target blackboard data, but no blackboard'. Row appended to the top table, its
    payload copies appended at EOF; header size/count/iused bumped. Gates: re-walk, undo to the input."""
    end, rows = _bb_walk(data)
    owners = [_q(data, o + 8) for o, _ in rows]
    if new in owners:
        raise ValueError(f"0x{new:016x} already has a CBlackboardCR row")
    hits = [i for i, w in enumerate(owners) if w == donor]
    if len(hits) != 1:
        raise ValueError(f"donor 0x{donor:016x} has {len(hits)} CBlackboardCR rows; need 1")
    o, pays = rows[hits[0]]
    row = bytearray(data[o:o + 592])
    struct.pack_into("<Q", row, 8, new)
    pay = b"".join(data[a:a + n] for a, n in pays)
    out = bytearray(data[:end] + bytes(row) + data[end:] + pay)
    n = len(rows)
    struct.pack_into("<Q", out, 8, (n + 1) * 592)
    struct.pack_into("<Q", out, 40, n + 1)
    struct.pack_into("<Q", out, 48, n + 1)
    out = bytes(out)
    _, rows2 = _bb_walk(out)
    if [_q(out, r + 8) for r, _ in rows2] != owners + [new]:
        raise ValueError("CBlackboardCR re-walk: owners changed")
    back = bytearray(out[:end] + out[end + 592:len(out) - len(pay)])
    struct.pack_into("<Q", back, 8, n * 592)
    struct.pack_into("<Q", back, 40, n)
    struct.pack_into("<Q", back, 48, n)
    if bytes(back) != data:
        raise ValueError("CBlackboardCR undo-diff failed")
    return out, dict(op="blackboard", donor=f"0x{donor:016x}", new=f"0x{new:016x}", payload=len(pay))


def set_canvas_rebind(data: bytes, actor: int, sym: int) -> tuple[bytes, dict]:
    """--canvas-rebind ACTOR:CANVAS: point the actor's CCanvasUICR row (+0x20) at another canvas, e.g. a map-unique
    copy emitted by canvas_edit.py (the icon-less HELP tile canvas). The new canvas must be served by the
    same tree; search the tree for a new canvas symbol first."""
    rows = uam._rows(data, 88, actor)
    if len(rows) != 1:
        raise ValueError(f"0x{actor:016x} has {len(rows)} CCanvasUICR rows; need 1")
    o = TOP + rows[0] * 88 + 0x20
    out = bytearray(data)
    old = _q(data, o)
    struct.pack_into("<Q", out, o, sym)
    out = bytes(out)
    _gate_inplace(data, out, [(o, 8)], CANVAS)
    return out, dict(op="canvas-rebind", actor=f"0x{actor:016x}", canvas=f"0x{old:016x}->0x{sym:016x}")


def set_pointer_aspect(data: bytes, actor: int, ratio: float) -> tuple[bytes, dict]:
    """--pointer-aspect ACTOR:RATIO: pointer quad height = width × ratio (CR15PointerMeshCR +32/+36), after a canvas
    reshape (news tile 640×672 → 1.05) or 0 for a label that must not take hover. Run after --canvas-scale."""
    rows = uam._rows(data, 80, actor)
    if len(rows) != 1:
        raise ValueError(f"0x{actor:016x} has {len(rows)} CR15PointerMeshCR rows; need 1")
    o = TOP + rows[0] * 80 + 32
    w, h = struct.unpack_from("<2f", data, o)
    out = bytearray(data)
    struct.pack_into("<f", out, o + 4, w * ratio)
    out = bytes(out)
    _gate_inplace(data, out, [(o + 4, 4)], PMESH)
    return out, dict(op="pointer-aspect", actor=f"0x{actor:016x}", quad=f"({w:.4g}, {h:.4g})->({w:.4g}, {w * ratio:.4g})")


def set_pointer_size(data: bytes, actor: int, w: float, h: float) -> tuple[bytes, dict]:
    """--pointer-size ACTOR:W,H: set the pointer quad (CR15PointerMeshCR +32/+36) outright, for a canvas rebind whose
    community ratio, so quad and RT agree whichever one sets the drawn size). W > 0, H >= 0 (0 = no hover)."""
    if not (w > 0 and h >= 0):
        raise ValueError(f"--pointer-size needs W > 0 and H >= 0, got ({w}, {h})")
    rows = uam._rows(data, 80, actor)
    if len(rows) != 1:
        raise ValueError(f"0x{actor:016x} has {len(rows)} CR15PointerMeshCR rows; need 1")
    o = TOP + rows[0] * 80 + 32
    w0, h0 = struct.unpack_from("<2f", data, o)
    out = bytearray(data)
    struct.pack_into("<2f", out, o, w, h)
    out = bytes(out)
    _gate_inplace(data, out, [(o, 8)], PMESH)
    return out, dict(op="pointer-size", actor=f"0x{actor:016x}", quad=f"({w0:.4g}, {h0:.4g})->({w:.4g}, {h:.4g})")


def _pair(text: str, sep: str) -> tuple[str, str]:
    if sep not in text:
        raise argparse.ArgumentTypeError(f"{text!r}: expected A{sep}B")
    return tuple(text.split(sep, 1))


def _value(text: str, files: dict) -> int:
    if text[:1] in "pP" and text[1:].isdigit():
        po = page_owners(files[PAGE])
        return po[int(text[1:])]
    return int(text, 0)


def apply_ops(ops: list[tuple[str, str]], files: dict[str, bytes], clone_binds: dict[str, list] | None = None
              ) -> tuple[dict[str, bytes], list[dict]]:
    """files: class -> bytes (PAGE, SCRIPT, TRANS, ADR, SCENE as needed). Returns (touched class -> bytes, reports)."""
    files, touched, reports = dict(files), {}, []
    clone_binds = {k: list(v) for k, v in (clone_binds or {}).items()}
    for kind, arg in ops:
        if kind in ("member", "unmember", "show", "unshow"):
            a, p = _pair(arg, ":")
            fn = dict(member=member, unmember=unmember, show=show, unshow=unshow)[kind]
            new, rep = fn(files[PAGE], int(a, 0), int(p, 0))
            cls = PAGE
        elif kind in ("text", "tooltip"):
            a, t = _pair(arg, "=")
            key = uam.KEY_INITIAL_TEXT if kind == "text" else uam.KEY_DESCRIPTION
            new, rep = set_text(files[SCRIPT], int(a, 0), key, t, kind)
            cls = SCRIPT
        elif kind == "sym":
            lhs, v = _pair(arg, "=")
            a, k = _pair(lhs, ":")
            key = SYM_KEYS[k] if k in SYM_KEYS else int(k, 0)
            val = int(v, 0) if v.startswith("0x") else __import__("quest_clone_map").csymbol64(v)
            new, rep = set_sym(files[SCRIPT], int(a, 0), key, val)
            cls = SCRIPT
        elif kind == "bind":
            lhs, v = _pair(arg, "=")
            o, k = _pair(lhs, ":")
            o, comp = (o.split("/", 1) + [None])[:2]
            new, rep = bind(files[SCRIPT], int(o, 0), k, _value(v, files),
                            None if comp is None else component_sym(comp))
            cls = SCRIPT
        elif kind == "clone-module":
            parts = arg.split(":")
            if len(parts) not in (2, 3):
                raise ValueError(f"--clone-module {arg!r}: OWNER:RECORD[:COMPONENT]")
            comp = parts[2] if len(parts) == 3 else None
            binds = [(k, _value(v, files)) for k, v in clone_binds.pop(comp, [])] if comp else []
            new, rep = clone_module(files[SCRIPT], int(parts[0], 0), int(parts[1], 0), comp, binds)
            cls = SCRIPT
        elif kind in ("pose", "pose-rows"):
            a, t = _pair(arg, ":")
            parts = t.split(":")
            xyz = tuple(float(x) for x in parts[0].split(","))
            q = tuple(float(x) for x in parts[1].split(",")) if len(parts) > 1 and parts[1] else None
            sc_ = float(parts[2]) if len(parts) > 2 and parts[2] else None
            if len(xyz) != 3 or (q is not None and len(q) != 4) or len(parts) > 3:
                raise ValueError(f"--{kind} {arg!r}: ACTOR:x,y,z[:qx,qy,qz,qw[:scale]]")
            res, rep = (set_pose if kind == "pose" else set_pose_rows)(files, int(a, 0), xyz, q, sc_)
            for c, b in res.items():
                if c != TRANS:
                    files[c] = touched[c] = b
            new, cls = res[TRANS], TRANS
        elif kind == "blackboard-from":
            a, t = _pair(arg, ":")
            new, rep = blackboard_clone(files[BBOARD], int(a, 0), int(t, 0))
            cls = BBOARD
        elif kind == "canvas-rebind":
            a, t = _pair(arg, ":")
            sym = int(t, 0) if t.startswith("0x") else __import__("quest_clone_map").csymbol64(t)
            new, rep = set_canvas_rebind(files[CANVAS], int(a, 0), sym)
            cls = CANVAS
        elif kind == "pointer-aspect":
            a, t = _pair(arg, ":")
            new, rep = set_pointer_aspect(files[PMESH], int(a, 0), float(t))
            cls = PMESH
        elif kind == "pointer-size":
            a, t = _pair(arg, ":")
            wh = [float(x) for x in t.split(",")]
            if len(wh) != 2:
                raise ValueError(f"--pointer-size {arg!r}: ACTOR:W,H")
            new, rep = set_pointer_size(files[PMESH], int(a, 0), *wh)
            cls = PMESH
        elif kind == "canvas-scale":
            a, t = _pair(arg, ":")
            res, rep = set_canvas_scale(files, int(a, 0), float(t))
            files[PMESH] = touched[PMESH] = res[PMESH]
            new, cls = res[CANVAS], CANVAS
        elif kind in ("trans", "scale"):
            a, t = _pair(arg, ":")
            actor = int(a, 0)
            if kind == "trans":
                xyz = tuple(float(x) for x in t.split(","))
                if len(xyz) != 3:
                    raise ValueError(f"--trans {arg!r}: x,y,z")
                old = struct.unpack_from("<3f", files[TRANS], TOP + uam._rows(files[TRANS], TREC, actor)[0] * TREC
                                         + 0x30) if uam._rows(files[TRANS], TREC, actor) else None
                new, rep = set_transform(files[TRANS], actor, trans=xyz)
                a2, s2, note = mirror_world(files[ADR], files[SCENE], actor, old, xyz)
                rep["world"] = note
                for c, b in ((ADR, a2), (SCENE, s2)):
                    if b is not None:
                        files[c] = touched[c] = b
            else:
                new, rep = set_transform(files[TRANS], actor, scale=float(t))
            cls = TRANS
        else:
            raise ValueError(f"unknown op {kind}")
        files[cls] = touched[cls] = new
        rep["file"] = cls
        reports.append(rep)
    if clone_binds:
        raise ValueError(f"--clone-bind for component(s) {sorted(clone_binds)} with no --clone-module of that name")
    for cls, b in touched.items():
        rb = {PAGE: lambda x: page_decode(x)[0], SCRIPT: lambda x: script_decode(x)[0],
              ADR: lambda x: uam.adr_pack(uam.adr_view(x)), SCENE: lambda x: uam.gsc.write(uam.gsc.read(x)),
              TRANS: lambda x: x, CANVAS: lambda x: x, PMESH: lambda x: x, WIDGET: lambda x: x,
              BBOARD: lambda x: x if _bb_walk(x) else x}[cls](b)
        if rb != b:
            raise ValueError(f"{cls} does not re-decode after the pass")
    return touched, reports


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawTextHelpFormatter)
    ap.add_argument("level", help="level name, e.g. mnu_master")
    ap.add_argument("--base", help="read the level's files from this loose tree where present (and copy the rest)")
    ap.add_argument("--out", required=True, help="bed-local loose tree root")
    ap.add_argument("--build", default="pcf")
    ap.add_argument("--dry-run", action="store_true")
    for flag, meta in (("member", "ACTOR:PAGE"), ("unmember", "ACTOR:PAGE"), ("show", "ACTOR:PAGE"),
                       ("unshow", "ACTOR:PAGE"), ("text", "ACTOR=TEXT"),
                       ("tooltip", "ACTOR=TEXT"), ("trans", "ACTOR:x,y,z"), ("scale", "ACTOR:s"),
                       ("pose", "ACTOR:x,y,z[:qx,qy,qz,qw[:s]]"),
                       ("pose-rows", "ACTOR:x,y,z[:qx,qy,qz,qw[:s]]"), ("canvas-scale", "ACTOR:k"),
                       ("blackboard-from", "DONOR:NEW"), ("canvas-rebind", "ACTOR:CANVAS"),
                       ("pointer-aspect", "ACTOR:RATIO"), ("pointer-size", "ACTOR:W,H"),
                       ("bind", "OWNER[/COMPONENT]:KEY=VALUE"), ("sym", "ACTOR:KEY=SYMBOL"), ("clone-module", "OWNER:RECORD[:COMPONENT]")):
        ap.add_argument(f"--{flag}", dest="ops", action="append", default=[], metavar=meta,
                        type=lambda s, f=flag: (f, s))
    ap.add_argument("--clone-bind", action="append", default=[], metavar="COMPONENT:KEY=VALUE",
                    help="slot-552 row of the --clone-module component COMPONENT")
    a = ap.parse_args()
    if not a.ops:
        ap.error("no ops")
    out = Path(_winpath(a.out)).resolve()
    page_add.refuse_out(out)
    base = Path(_winpath(a.base)).resolve() if a.base else None
    if base is not None and not base.is_dir():
        raise SystemExit(f"--base {base} is not a directory")
    if base is not None and (base == out or out.is_relative_to(base) or base.is_relative_to(out)):
        raise SystemExit("--out must not overlap --base")
    from game_store import Basis
    b = Basis.named(a.build)
    need = {PAGE, SCRIPT} | ({TRANS, ADR, SCENE} if any(k in ("trans", "scale", "pose", "pose-rows") for k, _ in a.ops) else set()) \
        | ({WIDGET} if any(k in ("pose", "pose-rows") for k, _ in a.ops) else set()) \
        | ({CANVAS, PMESH} if any(k in ("canvas-scale", "canvas-rebind", "pointer-aspect", "pointer-size")
                                  for k, _ in a.ops) else set()) \
        | ({BBOARD} if any(k == "blackboard-from" for k, _ in a.ops) else set())
    rows, files, inputs = {}, {}, {}
    for cls in sorted(need):
        r = b.row("primary", cls, a.level)
        if r is None:
            raise SystemExit(f"{a.level}: no {cls} in {b.plat} {b.build}")
        rows[cls] = r
        p = base / b.tree_rel(r) if base is not None else None
        files[cls], inputs[cls] = ((p.read_bytes(), "base") if p is not None and p.is_file()
                                   else (b.read("primary", cls, a.level), "basis"))
    cb = {}
    for x in a.clone_bind:
        comp, rest = _pair(x, ":")
        cb.setdefault(comp, []).append(_pair(rest, "="))
    try:
        touched, reports = apply_ops(a.ops, files, cb)
    except ValueError as e:
        raise SystemExit(f"refused: {e}; nothing written")
    summary = dict(build=f"{b.plat} {b.build}", level=a.level, base=str(base) if base else None, inputs=inputs,
                   ops=reports, files={})
    written = set()
    for cls, new in touched.items():
        rel = b.tree_rel(rows[cls])
        written.add(rel)
        summary["files"][cls] = dict(path=rel, size=f"{len(files[cls])}->{len(new)}",
                                     sha256=hashlib.sha256(new).hexdigest())
        if not a.dry_run:
            (out / rel).parent.mkdir(parents=True, exist_ok=True)
            (out / rel).write_bytes(new)
    copied = []
    if base is not None:
        for f in sorted(p for p in base.rglob("*") if p.is_file()):
            rel = f.relative_to(base).as_posix()
            if rel not in written:
                copied.append(rel)
                if not a.dry_run:
                    (out / rel).parent.mkdir(parents=True, exist_ok=True)
                    (out / rel).write_bytes(f.read_bytes())
    summary.update(copied_from_base=copied, out=str(out), dry_run=a.dry_run)
    print(json.dumps(summary, indent=1))
    return 0


if __name__ == "__main__":
    sys.exit(main())
