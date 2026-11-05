#!/usr/bin/env python3
"""Set the click_event (+328 symbol row, key 0x63becd132df98137) of existing mnu_master button actors, in place,
in a loose tree this run built. Same effect as `ui_actor_mint.py --click-event` at mint time, for actors
that already exist (the FRIENDS rows 1-5, minted without one in). Size-preserving: only value words change.

Usage: click_event.py TREE ACTOR=EVENT_NAME [...]   (TREE = a build tree; never the basis)
PC final 6323983201049540 (--build pcf); the tree must already hold mnu_master's CScriptCR (a menu_layout output)."""
from __future__ import annotations

import argparse
import struct
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path[:0] = [str(HERE.parent), str(HERE.parent.parent)]
import ui_actor_mint as uam
from quest_clone_map import csymbol64


def set_click_events(data: bytes, pairs: list[tuple[int, int]]) -> tuple[bytes, list[str]]:
    rebuilt, arrays, _ = uam.script_decode(data)
    if rebuilt != data:
        raise ValueError("CScriptCR does not round-trip")
    out, rep = bytearray(data), []
    for actor, ev in pairs:
        recs = set(uam._rows(data, 720, actor))
        if not recs:
            raise ValueError(f"0x{actor:016x}: no CScriptCR record")
        hits = 0
        for a in (x for x in arrays if x["record"] in recs and x["slot"] == 328):
            for i in range(a["count"]):
                o = a["payload_file_offset"] + i * 24
                k, _, v = struct.unpack_from("<3Q", data, o)
                if k == uam.KEY_CLICK_EVENT:
                    struct.pack_into("<Q", out, o + 16, ev)
                    hits += 1
                    rep.append(f"0x{actor:016x} rec {a['record']} click_event 0x{v:016x} -> 0x{ev:016x}")
        if hits != 1:
            raise ValueError(f"0x{actor:016x}: {hits} click_event rows (need 1)")
    if len(out) != len(data) or uam.script_decode(bytes(out))[0] != bytes(out):
        raise ValueError("re-decode failed")
    return bytes(out), rep


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("tree")
    ap.add_argument("pairs", nargs="+", metavar="ACTOR=EVENT")
    ap.add_argument("--build", default="pcf")
    a = ap.parse_args()
    tree = Path(a.tree).resolve()
    if "_data" in tree.parts:
        raise SystemExit("refused: never write the basis")
    from game_store import Basis
    b = Basis.named(a.build)
    p = tree / b.tree_rel(b.row("primary", "CScriptCR", "mnu_master"))
    if not p.is_file():
        raise SystemExit(f"{p}: no mnu_master CScriptCR in the tree")
    pairs = []
    for s in a.pairs:
        act, ev = s.split("=", 1)
        pairs.append((int(act, 0), int(ev, 0) if ev.startswith("0x") else csymbol64(ev)))
    try:
        new, rep = set_click_events(p.read_bytes(), pairs)
    except ValueError as e:
        raise SystemExit(f"refused: {e}; nothing written")
    if p.is_symlink():
        raise SystemExit(f"refused: {p} is a symlink")
    p.write_bytes(new)
    print("\n".join(rep))
    return 0


if __name__ == "__main__":
    sys.exit(main())
