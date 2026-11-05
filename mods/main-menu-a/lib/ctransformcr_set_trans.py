#!/usr/bin/env python3
"""ctransformcr_set_trans.py — overwrite the world QTS of existing CTransformCR
records, keyed by instance ENTITY CSymbol64.

The CTransformCR record (176B stride from 0x10) is the world-placement source for
BOTH CModelCR Type-B instances AND CStaticInstanceModelCR static instances — the
join is record key @+0x30 == the instance entity (live-proven for CModelCR;
537/537 static entities were decoded and resolve here). Overwriting an EXISTING record's
QTS is length-preserving and needs no prologue/TOC rebuild (placement-finding §4).

Per-record layout (relative to record start):
    +0x30  key  (CSymbol64, the join field == instance entity)
    +0x48  quat (4x f32, xyzw)
    +0x58  trans(3x f32, world meters)
    +0x64  scale(3x f32)

Usage:
    # dry-run (default): print old->new for each --set
    python3 ctransformcr_set_trans.py <CTransformCR file> \
        --set 0xENTITY:tx,ty,tz[:qx,qy,qz,qw][:sx,sy,sz] ...
    # apply (writes .bak.orig once, then patches in place)
    python3 ctransformcr_set_trans.py <file> --set ... --apply

AGENTS: edit only map-named files (never 0x-hash-named). Test workspace only.
"""
from __future__ import annotations
import argparse, struct, pathlib, sys

STRIDE = 176
REC0 = 0x10
OFF_KEY = 0x30
OFF_QUAT = 0x48
OFF_TRANS = 0x58
OFF_SCALE = 0x64


def parse_set(spec: str):
    """0xENTITY:tx,ty,tz[:qx,qy,qz,qw][:sx,sy,sz]"""
    parts = spec.split(":")
    key = int(parts[0], 16)
    trans = tuple(float(x) for x in parts[1].split(","))
    assert len(trans) == 3, f"trans needs 3 comps: {parts[1]}"
    quat = None
    scale = None
    if len(parts) >= 3 and parts[2]:
        quat = tuple(float(x) for x in parts[2].split(","))
        assert len(quat) == 4, f"quat needs 4 comps: {parts[2]}"
    if len(parts) >= 4 and parts[3]:
        scale = tuple(float(x) for x in parts[3].split(","))
        assert len(scale) == 3, f"scale needs 3 comps: {parts[3]}"
    return key, trans, quat, scale


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("file", type=pathlib.Path)
    ap.add_argument("--set", action="append", required=True, dest="sets")
    ap.add_argument("--apply", action="store_true")
    ap.add_argument("--bak-suffix", default=".bak.orig")
    args = ap.parse_args()

    b = bytearray(args.file.read_bytes())
    recbytes = struct.unpack_from("<Q", b, 0x08)[0]
    nrec = recbytes // STRIDE
    key2off = {}
    for i in range(nrec):
        o = REC0 + i * STRIDE
        key2off[struct.unpack_from("<Q", b, o + OFF_KEY)[0]] = o

    changed = []
    for spec in args.sets:
        key, trans, quat, scale = parse_set(spec)
        if key not in key2off:
            print(f"  MISS 0x{key:016x}: not a CTransformCR key — SKIP", file=sys.stderr)
            sys.exit(2)
        o = key2off[key]
        old_t = struct.unpack_from("<3f", b, o + OFF_TRANS)
        old_q = struct.unpack_from("<4f", b, o + OFF_QUAT)
        old_s = struct.unpack_from("<3f", b, o + OFF_SCALE)
        struct.pack_into("<3f", b, o + OFF_TRANS, *trans)
        if quat is not None:
            struct.pack_into("<4f", b, o + OFF_QUAT, *quat)
        if scale is not None:
            struct.pack_into("<3f", b, o + OFF_SCALE, *scale)
        new_t = struct.unpack_from("<3f", b, o + OFF_TRANS)
        new_q = struct.unpack_from("<4f", b, o + OFF_QUAT)
        new_s = struct.unpack_from("<3f", b, o + OFF_SCALE)
        changed.append((key, old_t, new_t, old_q, new_q, old_s, new_s))
        print(f"0x{key:016x} @rec_off=0x{o:x}")
        print(f"   trans {tuple(round(v,3) for v in old_t)} -> {tuple(round(v,3) for v in new_t)}")
        if quat is not None:
            print(f"   quat  {tuple(round(v,3) for v in old_q)} -> {tuple(round(v,3) for v in new_q)}")
        if scale is not None:
            print(f"   scale {tuple(round(v,3) for v in old_s)} -> {tuple(round(v,3) for v in new_s)}")

    assert len(b) == len(args.file.read_bytes()), "length changed (must be overwrite-only)"

    if args.apply:
        bak = args.file.with_name(args.file.name + args.bak_suffix)
        if not bak.exists():
            bak.write_bytes(args.file.read_bytes())
            print(f"backup -> {bak.name}")
        else:
            print(f"backup exists ({bak.name}) — not overwriting")
        args.file.write_bytes(b)
        print(f"APPLIED {len(changed)} record(s) to {args.file.name}")
    else:
        print("DRY-RUN (pass --apply to write)")


if __name__ == "__main__":
    main()
