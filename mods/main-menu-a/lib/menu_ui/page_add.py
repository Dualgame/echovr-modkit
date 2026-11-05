#!/usr/bin/env python3
"""Append ONE new page to a level's menu (H-M2): a `CR15UIPage2CR` page record + a `CScriptCR` page-owner record,
emitted as a loose-tree override. Offline writer; DRAFT until a device run (see "Not done by this tool").

    python3 lib/menu_ui/page_add.py mnu_master --name ffa_help_page --out /path/to/loose [--dry-run] \
        [--donor-page 32] [--parent 11] [--keep-content] [--spec spec.json]
        [--module-from 27 --action BUTTON:PAGE ... | --bind KEY=VALUE ...] [--base TREE [--module-only]]

script record REC (home = 27) as a SECOND script component of the page owner, beside the BACK-handling page script;
`--action` rebinds one (button key, page key) row pair of that module (`MODULE_ACTIONS`) to a new button actor and
page owner; `--bind` sets any row by key. `--base` reads the level's files from an earlier loose tree (copying its
other files to --out); `--module-only` adds just the component to a page that --base already holds.

The owner id is minted from NAME (`csymbol64`) and must occur in none of the level's resources (search every resource in the tree for the id before writing: the tool prints the check).

What a page is (`stream-confirmed` @ PC 6323983201049540, `mnu_master`; framing via page_heap / script_heap):
- `CR15UIPage2CR`: 56-byte top descriptor + N x 328-byte records = {u64 component type `0x85d5432be2bdb1d6`, u64 owner,
  u32 0xffffffff + pad, five 56-byte array descriptors at +32/+88/+144/+200/+272 (472-B elements, then 16-B rows),
  16-byte tail ff..ff 0}; payloads follow in page order (page p's element array, its elements' nested payloads, then
  +88 ... +272). The page index is the record position; runtime page numbers equal it (the CS component index).
  +88 = content actors (every page lists the shared `0x64a8671996f12efd`), +200 = child pages (exclusive siblings: enabling
  a page disables the siblings in its PARENT's +200; p11's list holds the 46 leaf pages, p4's holds 5-10), +272 = pages
  that stay enabled with it (`[P4, P1]` for p29/p31/p32/p57: the observed `31 + 4 + 1` set).
- `CScriptCR`: 56-byte top + N x 720-byte records {word0 script class `0x6aea99eeb2bab6c4`, word1 owner, word2
  0xffffffff, word4 module DLL fsym, 12 descriptors}; payloads in record order; slot 552 = 32-byte (key, ~0, target, 0)
  rows binding a module key to an actor. Modules are one DLL per page script, but 15 modules are shared by several
  records in `mnu_master` (one by 43), so a clone shares its donor's DLL.
- The page owner is a real actor. Besides those two files it occurs in `CActorDataResource` (2 raw hits: the sorted
  nodeid -> actorid map and the actor-order owner list) and `CGSceneResource` (1: the sorted node table); some pages also
  have a `CTransformCR` row (p29, p42, p24; not p10/p31/p32). No other `mnu_master` resource (52 checked) holds it.
  Writing the page without those actor rows makes the level load abort (ActorID() miss): see "Not done".

BACK: the BACK element (`0x7131d456929f96db`, one element shared by pages 30/31/32/47) sends `evt_btn_clicked` to the page
OWNER; the owner's script handles it. The donor page 32's script has only two rows (`0x65a7556c5ccf07f6`, and

Safety: owner absent from both input files and the other level resources; output must re-decode byte-exactly; removing the
inserted blocks and restoring the touched descriptors must give back the input exactly; `--out` is refused inside the

Not done by this tool (list for the actor minter, `lib/menu_ui/ui_actor_mint.py`): the actor rows in
`CActorDataResource` and `CGSceneResource` (interface wanted: `clone_actor(resources: dict[str, bytes], donor_owner: int,
new_owner: int) -> dict[str, bytes]` for those two classes of the level, donor = the page-32 owner `0x2d50833b4b45e363`;
afterwards the new owner must show 2 raw hits in CActorDataResource and 1 in CGSceneResource).
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
from page_heap import decode as page_decode
from script_heap import decode as script_decode
from page_patch import FORBIDDEN, _desc
from binding_patch import _winpath

PAGE_CLS, SCRIPT_CLS = "CR15UIPage2CR", "CScriptCR"
PAGE_REC, SCRIPT_REC, TOP = 328, 720, 56
PAGE_CS = 0x85d5432be2bdb1d6
COMMON88 = (0x64a8671996f12efd,)
DEFAULT_DONOR, DEFAULT_PARENT = 32, 11


def mint_owner(name: str) -> int:
    from csymbol64 import hash_name
    return hash_name(name)


def _splice_build(data: bytes, splices: list[tuple[int, bytes]], patches: dict[int, bytes]) -> bytes:
    """Apply same-length `patches` (original coordinates, all below the lowest splice) then insert `splices`."""
    low = min(o for o, _ in splices)
    buf = bytearray(data)
    for off, blob in patches.items():
        if off + len(blob) > low:
            raise ValueError(f"patch at 0x{off:x} reaches the first splice at 0x{low:x}")
        buf[off:off + len(blob)] = blob
    for off, blob in sorted(splices, key=lambda s: -s[0]):
        buf[off:off] = blob
    return bytes(buf)


def _undo(out: bytes, splices: list[tuple[int, bytes]], old: dict[int, bytes]) -> bytes:
    buf = bytearray(out)
    shift, spans = 0, []
    for off, blob in sorted(splices, key=lambda s: s[0]):
        spans.append((off + shift, len(blob)))
        shift += len(blob)
    for pos, n in reversed(spans):
        del buf[pos:pos + n]
    for off, blob in old.items():
        buf[off:off + len(blob)] = blob
    return bytes(buf)


def _bump_top(data: bytes, stride: int) -> tuple[bytes, bytes]:
    """(old 56-byte top, new top) with count/iused +1 and size += stride; the top keeps its own stride word."""
    w = list(struct.unpack_from("<7Q", data, 0))
    if w[1] != w[5] * stride or w[5] != w[6]:
        raise ValueError("unexpected top descriptor")
    new = list(w)
    new[1] += stride
    new[5] += 1
    new[6] += 1
    return data[:TOP], struct.pack("<7Q", *new)


def _owners(data: bytes, rec: int, n: int, off_word: int = 8) -> list[int]:
    return [struct.unpack_from("<Q", data, TOP + i * rec + off_word)[0] for i in range(n)]


def add_page(data: bytes, owner: int, donor: int = DEFAULT_DONOR, parent: int | None = DEFAULT_PARENT,
             keep_content: bool = False, keep88: tuple[int, ...] = COMMON88) -> tuple[bytes, dict]:
    """Append page record `len(pages)` to a `CR15UIPage2CR`, cloned from page `donor`. The new record's +88 keeps only
    `keep88` unless `keep_content`; `parent` (None = no) gets the owner appended to its +200 list."""
    _, arrays, elements = page_decode(data)
    n, = struct.unpack_from("<Q", data, 0x28)
    if not 0 <= donor < n or (parent is not None and not 0 <= parent < n):
        raise ValueError(f"donor/parent out of range 0..{n - 1}")
    if struct.pack("<Q", owner) in data:
        raise ValueError(f"owner 0x{owner:016x} already occurs in the file; refusing a duplicate")
    rec = bytearray(data[TOP + donor * PAGE_REC:TOP + (donor + 1) * PAGE_REC])
    if struct.unpack_from("<Q", rec, 0)[0] != PAGE_CS:
        raise ValueError("donor record is not a CR15UIPage2CS record")
    mine = [a for a in arrays if a["page"] == donor]
    if any(a["slot"] in (144, 200) and a["element"] < 0 and a["count"] for a in mine):
        raise ValueError(f"donor page {donor} has child/+144 pages; pick a leaf donor")
    struct.pack_into("<Q", rec, 8, owner)
    blob = bytearray()
    kept88 = None
    for a in mine:
        raw = data[a["payload_file_offset"]:a["payload_file_offset"] + a["size"]]
        if a["slot"] == 88 and a["element"] < 0 and not keep_content:
            rows = [raw[i:i + 16] for i in range(0, len(raw), 16)]
            rows = [r for r in rows if struct.unpack_from("<Q", r, 0)[0] in keep88]
            kept88 = len(rows)
            rec[88:144] = _desc(len(rows), 16)
            raw = b"".join(rows)
        blob += raw
    table_end = TOP + n * PAGE_REC
    splices = [(len(data), bytes(blob)), (table_end, bytes(rec))]
    old_top, new_top = _bump_top(data, PAGE_REC)
    patches, old = {0: new_top}, {0: old_top}
    plist = None
    if parent is not None:
        pa = next(a for a in arrays if a["page"] == parent and a["element"] < 0 and a["slot"] == 200)
        d_off = pa["descriptor_file_offset"]
        if data[d_off:d_off + 56] != _desc(pa["count"], 16):
            raise ValueError(f"page {parent} +200 descriptor is not the expected shape")
        patches[d_off] = _desc(pa["count"] + 1, 16)
        old[d_off] = data[d_off:d_off + 56]
        splices.append((pa["payload_file_offset"] + pa["size"], struct.pack("<QQ", owner, 0)))
        plist = pa["count"] + 1
    out = _splice_build(data, splices, patches)
    rebuilt, arrays2, elements2 = page_decode(out)
    if rebuilt != out:
        raise ValueError("patched file does not round-trip")
    if _undo(out, splices, old) != data:
        raise ValueError("changes outside the inserted blocks / descriptors")
    if struct.unpack_from("<Q", out, 0x28)[0] != n + 1 or _owners(out, PAGE_REC, n + 1)[n] != owner:
        raise ValueError("new record is not page n at the table end")
    if _owners(out, PAGE_REC, n) != _owners(data, PAGE_REC, n):
        raise ValueError("an existing page owner changed")
    old_el = [e["word0"] for e in elements]
    new_el = [e["word0"] for e in elements2]
    donor_el = [e["word0"] for e in elements if e["page"] == donor]
    if new_el != old_el + donor_el:
        raise ValueError("element lists differ beyond the new page's donor elements")
    if parent is not None:
        pa2 = next(a for a in arrays2 if a["page"] == parent and a["element"] < 0 and a["slot"] == 200)
        tail = struct.unpack_from("<QQ", out, pa2["payload_file_offset"] + (pa2["count"] - 1) * 16)
        if tail != (owner, 0) or pa2["count"] != plist:
            raise ValueError("parent +200 list does not end with the new owner")
    rep = dict(page=n, owner=f"0x{owner:016x}", donor=donor, parent=parent, elements=len(donor_el),
               content88_kept=kept88, size=f"{len(data)}->{len(out)}",
               splices=[f"0x{o:x}+{len(b)}" for o, b in sorted(splices)])
    return out, rep


def add_script(data: bytes, owner: int, donor_owner: int) -> tuple[bytes, dict]:
    """Append a `CScriptCR` record for `owner`, a clone of the record owned by `donor_owner` (same module, same rows)."""
    _, arrays, _ = script_decode(data)
    n, = struct.unpack_from("<Q", data, 0x28)
    if struct.pack("<Q", owner) in data:
        raise ValueError(f"owner 0x{owner:016x} already occurs in the file; refusing a duplicate")
    recs = [i for i, o in enumerate(_owners(data, SCRIPT_REC, n)) if o == donor_owner]
    if len(recs) != 1:
        raise ValueError(f"donor owner 0x{donor_owner:016x} has {len(recs)} script records (need exactly 1)")
    r = recs[0]
    rec = bytearray(data[TOP + r * SCRIPT_REC:TOP + (r + 1) * SCRIPT_REC])
    struct.pack_into("<Q", rec, 8, owner)
    blob = b"".join(data[a["payload_file_offset"]:a["payload_file_offset"] + a["size"]]
                    for a in arrays if a["record"] == r)
    if struct.pack("<Q", donor_owner) in blob:
        raise ValueError("donor script rows reference the donor owner; pick a donor without self rows")
    splices = [(len(data), blob), (TOP + n * SCRIPT_REC, bytes(rec))]
    old_top, new_top = _bump_top(data, SCRIPT_REC)
    out = _splice_build(data, splices, {0: new_top})
    rebuilt, _, _ = script_decode(out)
    if rebuilt != out:
        raise ValueError("patched script file does not round-trip")
    if _undo(out, splices, {0: old_top}) != data:
        raise ValueError("changes outside the inserted blocks / descriptor")
    if _owners(out, SCRIPT_REC, n + 1)[n] != owner or _owners(out, SCRIPT_REC, n) != _owners(data, SCRIPT_REC, n):
        raise ValueError("new record is not the last record, or an existing owner changed")
    module = struct.unpack_from("<Q", rec, 32)[0]
    return out, dict(record=n, owner=f"0x{owner:016x}", donor_record=r, module=f"0x{module:016x}",
                     rows=len(blob) // 32, size=f"{len(data)}->{len(out)}")


MODULE_ACTIONS = {
    0x991bbec05cde117f: ((0x6fe8c1162da7863f, 0x63b4c5417ea7873e),),
}


COMPONENT_DEFAULT = 0x58b88ce8265d615c


def record_module(data: bytes, record: int) -> int:
    return struct.unpack_from("<Q", data, TOP + record * SCRIPT_REC + 32)[0]


def action_binds(module: int, actions: list[tuple[int, int]]) -> dict[int, int]:
    """[(button actor, page owner)] -> {row key: new value}, one known (button, page) row pair per action."""
    pairs = MODULE_ACTIONS.get(module, ())
    if len(actions) > len(pairs):
        raise ValueError(f"module 0x{module:016x} has {len(pairs)} rebindable button->page pair(s); "
                         f"{len(actions)} action(s) asked")
    binds = {}
    for (bkey, pkey), (button, page) in zip(pairs, actions):
        binds[bkey], binds[pkey] = button, page
    return binds


def add_module(data: bytes, owner: int, donor_record: int, binds: dict[int, int] | None = None,
               component: int | None = None, as_default: bool = False,
               allow_extra: bool = False) -> tuple[bytes, dict]:
    """Append a clone of stock `CScriptCR` record `donor_record` as an EXTRA script component of `owner` (which must
    already own exactly one record, its page script: stock actors carry 2-4 script records, e.g. FRIENDS 103/104), then
    set each slot-552 row whose key is in `binds` to its new value (each key must sit in exactly one row).

    Word0 of a record is the COMPONENT NAME (CSymbol64: `ncaScript` = 0x6aea99eeb2bab6c4 is the default) and no stock
    owner repeats one, so the extra record is named `component` (default `pageactionscript`). `as_default` swaps the
    names: the clone takes the page record's name and the page record takes `component`.

    `allow_extra` (menu_layout --clone-module): the owner may already own several records (its first is the page
    record); `component` must then differ from every one of their names; and the donor may carry arrays besides
    slot 552 (copied verbatim: the SAFE ZONE card's button-module component)."""
    binds = dict(binds or {})
    component = COMPONENT_DEFAULT if component is None else component
    _, arrays, _ = script_decode(data)
    n, = struct.unpack_from("<Q", data, 0x28)
    if not 0 <= donor_record < n:
        raise ValueError(f"donor record {donor_record} out of range 0..{n - 1}")
    owners = _owners(data, SCRIPT_REC, n)
    if owners.count(owner) != 1 and not (allow_extra and owners.count(owner) > 1):
        raise ValueError(f"owner 0x{owner:016x} owns {owners.count(owner)} script records; need exactly 1 (its page)")
    taken = [struct.unpack_from("<Q", data, TOP + i * SCRIPT_REC)[0] for i, o in enumerate(owners) if o == owner]
    if component in taken:
        raise ValueError(f"component name 0x{component:016x} already names a record of 0x{owner:016x}")
    donor_owner = owners[donor_record]
    rec = bytearray(data[TOP + donor_record * SCRIPT_REC:TOP + (donor_record + 1) * SCRIPT_REC])
    struct.pack_into("<Q", rec, 8, owner)
    page_rec = owners.index(owner)
    page_off = TOP + page_rec * SCRIPT_REC
    page_name, = struct.unpack_from("<Q", data, page_off)
    if component == page_name:
        raise ValueError(f"component name 0x{component:016x} is the page record's own name")
    patches, old = {}, {}
    if as_default:
        struct.pack_into("<Q", rec, 0, page_name)
        patches[page_off], old[page_off] = struct.pack("<Q", component), data[page_off:page_off + 8]
    else:
        struct.pack_into("<Q", rec, 0, component)
    mine = [a for a in arrays if a["record"] == donor_record and a["count"]]
    if any(a["slot"] != 552 for a in mine) and not allow_extra:
        raise ValueError(f"donor record {donor_record} has arrays besides slot 552; not handled")
    if any(struct.pack("<Q", donor_owner) in data[a["payload_file_offset"]:a["payload_file_offset"] + a["size"]]
           for a in mine):
        raise ValueError("donor script rows reference the donor owner; pick a donor without self rows")
    a552 = [a for a in mine if a["slot"] == 552]
    blob = b"".join(data[a["payload_file_offset"]:a["payload_file_offset"] + a["size"]] for a in a552)
    rows = [list(struct.unpack_from("<4Q", blob, i)) for i in range(0, len(blob), 32)]
    for key, val in binds.items():
        hit = [r for r in rows if r[0] == key]
        if len(hit) != 1:
            raise ValueError(f"key 0x{key:016x} sits in {len(hit)} rows of record {donor_record}; need 1")
        hit[0][2] = val
    new_552 = b"".join(struct.pack("<4Q", *r) for r in rows)
    new_blob = b"".join(new_552 if a["slot"] == 552 else
                        data[a["payload_file_offset"]:a["payload_file_offset"] + a["size"]] for a in mine)
    splices = [(len(data), new_blob), (TOP + n * SCRIPT_REC, bytes(rec))]
    old_top, new_top = _bump_top(data, SCRIPT_REC)
    out = _splice_build(data, splices, {0: new_top, **patches})
    rebuilt, arrays2, _ = script_decode(out)
    if rebuilt != out:
        raise ValueError("patched script file does not round-trip")
    if _undo(out, splices, {0: old_top, **old}) != data:
        raise ValueError("changes outside the inserted blocks / descriptor")
    if _owners(out, SCRIPT_REC, n + 1)[n] != owner or _owners(out, SCRIPT_REC, n) != owners:
        raise ValueError("new record is not the last record, or an existing owner changed")
    got = next((a for a in arrays2 if a["record"] == n and a["slot"] == 552), dict(payload_file_offset=0, count=0))
    rows2 = [struct.unpack_from("<4Q", out, got["payload_file_offset"] + 32 * i) for i in range(got["count"])]
    for a in mine:
        b2 = next(x for x in arrays2 if x["record"] == n and x["slot"] == a["slot"])
        src = new_552 if a["slot"] == 552 else data[a["payload_file_offset"]:a["payload_file_offset"] + a["size"]]
        if b2["count"] != a["count"] or out[b2["payload_file_offset"]:b2["payload_file_offset"] + b2["size"]] != src:
            raise ValueError(f"new record slot {a['slot']} differs from the donor's")
    want = [(r[0], r[1], binds.get(r[0], r[2]), r[3]) for r in
            (struct.unpack_from("<4Q", blob, i) for i in range(0, len(blob), 32))]
    if rows2 != [tuple(r) for r in rows] or [tuple(r) for r in rows] != want:
        raise ValueError("new record rows differ from donor rows + binds")
    names = {r: f"0x{struct.unpack_from('<Q', out, TOP + r * SCRIPT_REC)[0]:016x}" for r in (page_rec, n)}
    return out, dict(record=n, owner=f"0x{owner:016x}", donor_record=donor_record, component_names=names,
                     module=f"0x{record_module(data, donor_record):016x}", rows=len(rows),
                     binds={f"0x{k:016x}": f"0x{v:016x}" for k, v in binds.items()},
                     size=f"{len(data)}->{len(out)}")


def refuse_out(out: Path) -> None:
    if any(str(out).startswith(p) for p in FORBIDDEN):
        raise SystemExit(f"refusing --out {out}: read-only root (read-only source tree)")


def level_hits(b, level: str, owner: int) -> dict:
    """Raw 8-byte hits of `owner` in every primary resource of `level` (cheap local gate, not the global scan)."""
    from game_store import fsym_of
    f = fsym_of(level)
    pat, hits = struct.pack("<Q", owner), {}
    for r in b.find(level):
        if r.fsym == f and r.layer.lower() == "primary":
            n = b.read("primary", r.cls, f).count(pat)
            if n:
                hits[r.cls] = n
    return hits


def _pair(text: str, sep: str) -> tuple[int, int]:
    a, b = text.split(sep, 1)
    return int(a, 0), int(b, 0)


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawTextHelpFormatter)
    ap.add_argument("level", help="level name, e.g. mnu_master")
    ap.add_argument("--name", required=True, help="page owner name; the id is csymbol64(NAME)")
    ap.add_argument("--donor-page", type=int, default=DEFAULT_DONOR)
    ap.add_argument("--parent", type=int, default=DEFAULT_PARENT, help="page whose +200 gets the owner (-1: none)")
    ap.add_argument("--keep-content", action="store_true", help="keep the donor's content actors in +88")
    ap.add_argument("--module-from", type=int, metavar="REC",
                    help="also clone stock CScriptCR record REC as a 2nd script component of the page owner")
    ap.add_argument("--bind", action="append", default=[], metavar="KEY=VALUE",
                    help="with --module-from: set the slot-552 row with this key to VALUE")
    ap.add_argument("--action", action="append", default=[], metavar="BUTTON:PAGE",
                    help="with --module-from: bind button actor -> page owner via the module's MODULE_ACTIONS pair")
    ap.add_argument("--module-component", default="pageactionscript", help="component NAME of the extra record")
    ap.add_argument("--module-default", action="store_true",
                    help="the clone takes the page record's component name (ncaScript); the page record takes NAME")
    ap.add_argument("--base", help="read the level's files from this loose tree where present (and copy the rest)")
    ap.add_argument("--module-only", action="store_true",
                    help="the page already exists in --base: only add the --module-from component")
    ap.add_argument("--out", required=True, help="bed-local loose tree root")
    ap.add_argument("--spec", help="write the actor/script requirements as JSON to this path")
    ap.add_argument("--build", default="pcf")
    ap.add_argument("--dry-run", action="store_true")
    a = ap.parse_args()
    out = Path(_winpath(a.out)).resolve()
    refuse_out(out)
    if (a.bind or a.action or a.module_only) and a.module_from is None:
        raise SystemExit("--bind / --action / --module-only need --module-from")
    if a.module_only and not a.base:
        raise SystemExit("--module-only needs --base (a tree that already holds the page)")
    from game_store import Basis
    b = Basis.named(a.build)
    base = Path(_winpath(a.base)).resolve() if a.base else None
    if base is not None and not base.is_dir():
        raise SystemExit(f"--base {base} is not a directory")
    if base is not None and (base == out or out.is_relative_to(base) or base.is_relative_to(out)):
        raise SystemExit("--out must not overlap --base")
    owner = mint_owner(a.name)
    hits = level_hits(b, a.level, owner)
    if hits:
        raise SystemExit(f"refused: 0x{owner:016x} ({a.name}) already occurs in {hits}")
    rp, rs = b.row("primary", PAGE_CLS, a.level), b.row("primary", SCRIPT_CLS, a.level)
    if rp is None or rs is None:
        raise SystemExit(f"{a.level}: no {PAGE_CLS}/{SCRIPT_CLS} in {b.plat} {b.build}")

    def src(r, cls):
        if base is not None and (base / b.tree_rel(r)).is_file():
            return (base / b.tree_rel(r)).read_bytes(), f"base {base}"
        return b.read("primary", cls, a.level), "basis"
    (pdata, pfrom), (sdata, sfrom) = src(rp, PAGE_CLS), src(rs, SCRIPT_CLS)
    spec = dict(build=f"{b.plat} {b.build}", level=a.level, name=a.name, owner=f"0x{owner:016x}",
                inputs={PAGE_CLS: pfrom, SCRIPT_CLS: sfrom})
    files = {}
    try:
        if a.module_only:
            npg = struct.unpack_from("<Q", pdata, 0x28)[0]
            if owner not in _owners(pdata, PAGE_REC, npg):
                raise ValueError(f"0x{owner:016x} is not a page owner in the --base page file")
            spec["page"] = dict(page=_owners(pdata, PAGE_REC, npg).index(owner), from_base=True)
            snew = sdata
        else:
            if struct.pack("<Q", owner) in pdata or struct.pack("<Q", owner) in sdata:
                raise ValueError(f"0x{owner:016x} already occurs in the input page/script file")
            donor_owner = _owners(pdata, PAGE_REC, struct.unpack_from("<Q", pdata, 0x28)[0])[a.donor_page]
            pnew, spec["page"] = add_page(pdata, owner, a.donor_page, None if a.parent < 0 else a.parent,
                                          a.keep_content)
            snew, spec["script"] = add_script(sdata, owner, donor_owner)
            spec.update(donor_owner=f"0x{donor_owner:016x}", needs_actor_rows_in=["CActorDataResource",
                        "CGSceneResource"], global_scan=f"search every resource in the tree for 0x{owner:016x} before writing")
            files[rp] = pnew
        if a.module_from is not None:
            binds = dict(_pair(x, "=") for x in a.bind)
            act = action_binds(record_module(snew, a.module_from), [_pair(x, ":") for x in a.action])
            if set(binds) & set(act):
                raise ValueError("a --bind key is also set by --action")
            snew, spec["module"] = add_module(snew, owner, a.module_from, {**binds, **act},
                                              mint_owner(a.module_component), a.module_default)
        files[rs] = snew
    except ValueError as e:
        raise SystemExit(f"refused: {e}; nothing written")
    print(json.dumps(spec, indent=1))
    written = set()
    for r, new in files.items():
        dest = out / b.tree_rel(r)
        written.add(b.tree_rel(r))
        print(f"{'would write' if a.dry_run else 'wrote'} {dest} sha256 {hashlib.sha256(new).hexdigest()}")
        if not a.dry_run:
            dest.parent.mkdir(parents=True, exist_ok=True)
            dest.write_bytes(new)
    if base is not None:
        for f in sorted(p for p in base.rglob("*") if p.is_file()):
            rel = f.relative_to(base).as_posix()
            if rel not in written:
                print(f"{'would copy' if a.dry_run else 'copied'} {rel} from base")
                if not a.dry_run:
                    (out / rel).parent.mkdir(parents=True, exist_ok=True)
                    (out / rel).write_bytes(f.read_bytes())
    if a.spec:
        Path(_winpath(a.spec)).write_text(json.dumps(spec, indent=1) + "\n")
    return 0


if __name__ == "__main__":
    sys.exit(main())
