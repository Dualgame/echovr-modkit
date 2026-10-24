#!/usr/bin/env python3
"""Read and extend an Echo VR resource package pair (win10 schema, used by both PC and Quest).

A package pair is `manifests/<name>` (a ZSTD-wrapped table) plus `packages/<name>_<n>` (concatenated zstd frames).
The manifest holds three tables:

  contents  (type, file, frame, offset_in_frame, size, align)   32-byte rows
  metadata  (type, file, a, b, asset_version)                   40-byte rows
  frames    (package_index, offset, compressed_size, length)    16-byte rows, terminated by (0, len, 0, 0)

The engine loads a base package and then its language package; on a duplicate (type, file) the language package
wins. `append_rows` exploits that: it returns a copy of the language pair with extra rows (one zstd frame each),
taking each row's align and metadata from the base manifest. Nothing here writes to disk.

Third-party dependency: `zstandard`.
"""
from __future__ import annotations

import hashlib
import os
import struct

import zstandard

HEADER = 0xc0                 # win10 manifest header
FC, FM, FR = struct.Struct("<QQIIII"), struct.Struct("<QQQQQ"), struct.Struct("<IIII")
PKG_CONST = 0x80000           # header +0x04
CAP = 64 * 1024 * 1024        # refuse any single decompressed frame larger than this


def sha256(b: bytes) -> str:
    return hashlib.sha256(b).hexdigest()


# ---- ZSTD container ---------------------------------------------------------------------------------------------
def unwrap(raw: bytes) -> bytes:
    """ZSTD container (b'ZSTD', u32 16, u64 usize, u64 csize, frame) -> payload."""
    if raw[:4] != b"ZSTD":
        raise ValueError("not a ZSTD container")
    usize, csize = struct.unpack_from("<QQ", raw, 8)
    return zstandard.ZstdDecompressor().decompress(raw[24:24 + csize], max_output_size=max(usize, 1 << 20))


def wrap(payload: bytes, level: int = 19) -> bytes:
    frame = zstandard.ZstdCompressor(level=level).compress(payload)
    return b"ZSTD" + struct.pack("<IQQ", 16, len(payload), len(frame)) + frame


# ---- manifest table ---------------------------------------------------------------------------------------------
def parse(payload: bytes) -> dict:
    """Decompressed win10 manifest -> dict(pkgcount, contents, metadata, frames). Refuses any other schema."""
    pkgcount, = struct.unpack_from("<I", payload, 0)
    size_a, = struct.unpack_from("<Q", payload, 0x10)
    size_b, = struct.unpack_from("<Q", payload, 0x50)
    nf, = struct.unpack_from("<Q", payload, 0x90 + 0x28)
    n = size_a // FC.size
    if size_b // FM.size != n or HEADER + n * FC.size + n * FM.size + nf * FR.size != len(payload):
        raise ValueError("manifest is not the win10 schema this tool supports")
    a0, b0 = HEADER, HEADER + n * FC.size
    f0 = b0 + n * FM.size
    return dict(pkgcount=pkgcount,
                contents=[FC.unpack_from(payload, a0 + i * FC.size) for i in range(n)],
                metadata=[FM.unpack_from(payload, b0 + i * FM.size) for i in range(n)],
                frames=[FR.unpack_from(payload, f0 + i * FR.size) for i in range(nf)])


def _descriptor(rows: int, stride: int) -> bytes:
    d = bytearray(0x40)
    struct.pack_into("<Q", d, 0x00, rows * stride)
    struct.pack_into("<II", d, 0x14, 1, 0x20)
    struct.pack_into("<I", d, 0x20, rows)
    struct.pack_into("<Q", d, 0x28, rows)
    return bytes(d)


def serialize(m: dict) -> bytes:
    """Inverse of `parse` (all counts derived from the row lists)."""
    out = bytearray(struct.pack("<II", m["pkgcount"], PKG_CONST) + bytes(8))
    out += _descriptor(len(m["contents"]), FC.size)
    out += _descriptor(len(m["metadata"]), FM.size)
    out += _descriptor(len(m["frames"]), FR.size)[:HEADER - 0x90]
    for r in m["contents"]:
        out += FC.pack(*r)
    for r in m["metadata"]:
        out += FM.pack(*r)
    for r in m["frames"]:
        out += FR.pack(*r)
    return bytes(out)


def load_manifest(path: str) -> dict:
    with open(path, "rb") as fh:
        return parse(unwrap(fh.read()))


# ---- reading one resource ---------------------------------------------------------------------------------------
def find_row(m: dict, type_sym: int, file_sym: int):
    return next((r for r in m["contents"] if r[0] == type_sym and r[1] == file_sym), None)


def read_resource(m: dict, type_sym: int, file_sym: int, read_frame) -> bytes:
    """Bytes of (type, file). `read_frame(package_index, offset, compressed_size) -> bytes` supplies the frame."""
    r = find_row(m, type_sym, file_sym)
    if r is None:
        raise KeyError(f"no row type=0x{type_sym:016x} file=0x{file_sym:016x}")
    pk, off, csz, ln = m["frames"][r[2]]
    if ln > CAP:
        raise ValueError(f"frame {r[2]} declares {ln} bytes, over the {CAP}-byte cap")
    raw = zstandard.ZstdDecompressor().decompress(read_frame(pk, off, csz), max_output_size=ln)
    return raw[r[3]:r[3] + r[4]]


def file_frame_reader(packages_dir: str, name: str):
    """read_frame over `packages_dir/<name>_<n>` files."""
    def read(pk: int, off: int, csz: int) -> bytes:
        with open(os.path.join(packages_dir, f"{name}_{pk}"), "rb") as fh:
            fh.seek(off)
            b = fh.read(csz)
        if len(b) != csz:
            raise ValueError(f"{name}_{pk}: short read at {off}")
        return b
    return read


# ---- appending rows to a language pair --------------------------------------------------------------------------
def append_rows(base: dict, lang_manifest_raw: bytes, lang_package: bytes,
                rows: list[tuple[int, int, bytes]], level: int = 3) -> tuple[bytes, bytes]:
    """(new manifest file bytes, new package_0 bytes) = the language pair + one row per (type, file, data).
    Each row's align and metadata come from the base manifest's row for (type, file)."""
    lang = parse(unwrap(lang_manifest_raw))
    blob = bytearray(lang_package)
    if lang["pkgcount"] != 1 or lang["frames"][-1][3] != 0 or lang["frames"][-1][1] != len(blob):
        raise ValueError("language package is not a single package with a terminal frame")
    frames, contents, metadata = lang["frames"][:-1], list(lang["contents"]), list(lang["metadata"])
    bmeta = {(x[0], x[1]): x for x in base["metadata"]}
    for type_sym, file_sym, data in rows:
        r = find_row(base, type_sym, file_sym)
        if r is None:
            raise KeyError(f"base manifest has no row type=0x{type_sym:016x} file=0x{file_sym:016x}")
        fr = zstandard.ZstdCompressor(level=level).compress(data)
        frames.append((0, len(blob), len(fr), len(data)))
        blob += fr
        contents.append((r[0], r[1], len(frames) - 1, 0, len(data), r[5]))
        metadata.append(bmeta[(r[0], r[1])])
    frames.append((0, len(blob), 0, 0))
    out = dict(pkgcount=1, contents=contents, metadata=metadata, frames=frames)
    return wrap(serialize(out)), bytes(blob)
