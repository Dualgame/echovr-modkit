#!/usr/bin/env python3
"""Bounded final-PC menu script arrays; field and event semantics unresolved."""
import argparse
import json
import struct
from pathlib import Path

from inventory import BUILD, ROOT, CAP, sha, tsv
from page_heap import header
from resource_io import _container as c

SLOTS = tuple(range(48, 720, 56))
STRIDES = {272: 24, 328: 24, 384: 24, 440: 24, 496: 80, 552: 32}


def decode(data):
    if len(data) > CAP:
        raise ValueError('resource exceeds cap')
    top = header(data, 0, 720)
    cur = 56 + top['size']
    if cur > len(data):
        raise ValueError('truncated fixed table')
    rebuilt = bytearray(data[:cur])
    rebuilt[:56] = c.pack_header(top)
    arrays, entries = [], []
    for record in range(top['count']):
        base = 56 + record * 720
        owner, = struct.unpack_from('<Q', data, base + 8)
        for slot in SLOTS:
            stride = STRIDES.get(slot, 0)
            h = header(data, base + slot, stride)
            if not stride and h['count']:
                raise ValueError('unobserved populated slot')
            rebuilt[base+slot:base+slot+56] = c.pack_header(h)
            end = cur + h['size']
            if end > len(data):
                raise ValueError('truncated array payload')
            arrays.append(dict(record=record, owner=f'0x{owner:016x}', slot=slot,
                descriptor_file_offset=base+slot, payload_file_offset=cur,
                count=h['count'], stride=stride, size=h['size']))
            for index in range(h['count']):
                off = cur + index * stride
                raw = data[off:off+stride]
                words = struct.unpack('<' + 'Q' * (stride // 8), raw)
                entries.append(dict(record=record, owner=f'0x{owner:016x}',
                    slot=slot, index=index, file_offset=off,
                    words=' '.join(f'0x{w:016x}' for w in words),
                    text_candidate=raw[16:].split(b'\0')[0].decode('utf-8', errors='backslashreplace')
                    if slot == 496 else ''))
            rebuilt.extend(data[cur:end])
            cur = end
    if cur != len(data):
        raise ValueError('unconsumed trailing bytes')
    if rebuilt != data:
        raise ValueError('reconstruction mismatch')
    return bytes(rebuilt), arrays, entries


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--output', required=True, type=Path)
    out = ap.parse_args().output.resolve()
    if not out.is_relative_to((ROOT/'generic_rebuilds/menu-ui').resolve()):
        ap.error('output must be under generic_rebuilds/menu-ui')
    arrays, entries, files = [], [], []
    for name in ('mnu_master', 'mnu_master_mp_ingame'):
        path = ROOT/'Primary/CScriptCR'/name
        if path.stat().st_size > CAP:
            raise ValueError('resource exceeds cap')
        data = path.read_bytes()
        rebuilt, aa, ee = decode(data)
        provenance = dict(build=BUILD, platform='win10_dx12', layer='Primary',
            type='CScriptCR', root=name, source=str(path), evidence='stream-confirmed')
        arrays.extend(dict(**provenance, **a) for a in aa)
        entries.extend(dict(**provenance, **e) for e in ee)
        files.append(dict(**provenance, size=len(data), sha256=sha(path),
            records=header(data, 0, 720)['count'], entries=len(ee),
            heap_bytes=sum(a['size'] for a in aa), roundtrip=rebuilt == data,
            consumed=len(data),
            scope='array framing only; record-order mapping corroborated by string alignment; event semantics unresolved'))
    out.mkdir(parents=True, exist_ok=True)
    tsv(out/'script_arrays.tsv', arrays, list(arrays[0]))
    tsv(out/'script_entries.tsv', entries, list(entries[0]))
    report = dict(parser_version='final-pc-menu-script-heap-v1',
        parser_hashes={str(p): sha(p) for p in (Path(__file__),
            Path(__file__).with_name('page_heap.py'), Path(c.__file__))}, files=files)
    (out/'script_heap.json').write_text(json.dumps(report, indent=2)+'\n')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
