#!/usr/bin/env python3
"""Retarget one slot-552 page binding in a level's `CScriptCR` and emit it as a loose-tree override (gap 1 of

A slot-552 row is 32 bytes, four u64: key (e.g. `group_select_page`), ?, value (a page owner), 0. The patch
swaps exactly one u64 that sits in such a row of record N, after asserting it is there exactly once, then
re-decodes the result with `script_heap.decode` (framing round trip) and diffs it: only those 8 bytes may move.

    python3 lib/menu_ui/binding_patch.py mnu_master 27 0xf943a6fd6fb713fc 0x6a9e38e434a28911 \
        --out '<tree dir>' [--row 0x270d8] [--dry-run]

Input is always the game install via `game_store` (default PC final `6323983201049540`). `--out` gets
`<out>/primary/<TypeSymbol16>/v<AssetType>/<FileSymbol16>` (loose-tree grammar, same as `canvas_edit emit`)
"""
from __future__ import annotations

import argparse
import re
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from script_heap import decode
from canvas_edit import FORBIDDEN_OUT

CLS, SLOT, STRIDE = "CScriptCR", 552, 32
FORBIDDEN = FORBIDDEN_OUT


def _int(text):
    return int(text, 0)


def _winpath(text):
    """`F:\\x\\y` -> `x/y`; anything else unchanged."""
    m = re.match(r"^([A-Za-z]):[\\/](.*)$", text)
    return f"/mnt/{m[1].lower()}/{m[2].replace(chr(92), '/')}" if m else text


def patch(data: bytes, record: int, old: int, new: int, row: int | None = None) -> tuple[bytes, int]:
    """(patched bytes, file offset of the swapped u64). Raises ValueError unless `old` sits exactly once in
    one slot-552 row of `record` (the row at file offset `row`, when given)."""
    _, _, entries = decode(data)
    rows = [e for e in entries if e["record"] == record and e["slot"] == SLOT
            and (row is None or e["file_offset"] == row)]
    if not rows:
        raise ValueError(f"no slot-{SLOT} row in record {record}" + (f" at 0x{row:x}" if row is not None else ""))
    hits = [e["file_offset"] + 8 * i for e in rows
            for i, w in enumerate(struct.unpack_from("<4Q", data, e["file_offset"])) if w == old]
    if len(hits) != 1:
        raise ValueError(f"0x{old:016x} occurs {len(hits)}x in the slot-{SLOT} row(s) of record {record}; need 1")
    off = hits[0]
    out = bytearray(data)
    struct.pack_into("<Q", out, off, new)
    out = bytes(out)
    rebuilt, _, _ = decode(out)
    if rebuilt != out:
        raise ValueError("patched file does not round-trip")
    moved = [i for i in range(len(data)) if data[i] != out[i]]
    if len(out) != len(data) or not moved or moved[0] < off or moved[-1] >= off + 8:
        raise ValueError(f"unexpected byte changes outside 0x{off:x}..+8")
    return out, off


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawTextHelpFormatter)
    ap.add_argument("level", help="CScriptCR name or fsym, e.g. mnu_master")
    ap.add_argument("record", type=int)
    ap.add_argument("old", type=_int)
    ap.add_argument("new", type=_int)
    ap.add_argument("--row", type=_int, help="file offset of the slot-552 row (optional extra pin)")
    ap.add_argument("--out", required=True, help="bed-local loose tree root")
    ap.add_argument("--build", default="pcf", help="game_store alias or build id (default pcf)")
    ap.add_argument("--dry-run", action="store_true")
    a = ap.parse_args()
    out = Path(_winpath(a.out)).resolve()
    if any(str(out).startswith(p) for p in FORBIDDEN):
        raise SystemExit(f"refusing --out {out}: read-only root (read-only source tree)")
    from game_store import Basis
    b = Basis.named(a.build)
    r = b.row("primary", CLS, a.level)
    if r is None:
        raise SystemExit(f"{a.level}: no {CLS} in {b.plat} {b.build}")
    data = b.read("primary", CLS, a.level)
    try:
        new, off = patch(data, a.record, a.old, a.new, a.row)
    except ValueError as e:
        raise SystemExit(f"refused: {e}; nothing written")
    dest = out / b.tree_rel(r)
    print(f"{b.plat} {b.build} {CLS} {b.name_of(r.fsym) or a.level} 0x{r.fsym:016x} ({len(data):,} B) "
          f"record {a.record} @0x{off:x}: 0x{a.old:016x} -> 0x{a.new:016x}; round trip ok, 8 bytes changed")
    if a.dry_run:
        print(f"dry run: would write {dest}")
        return 0
    dest.parent.mkdir(parents=True, exist_ok=True)
    dest.write_bytes(new)
    print(f"wrote {dest}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
