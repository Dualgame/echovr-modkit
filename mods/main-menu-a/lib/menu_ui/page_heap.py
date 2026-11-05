#!/usr/bin/env python3
"""Final PC 6323983201049540 menu page heap framing, without action semantics."""
import argparse
import hashlib
import json
import struct
from pathlib import Path

from inventory import BUILD, ROOT, CAP, sha, tsv
from resource_io import _container as c

PAGE_SLOTS = (32, 88, 144, 200, 272)
ELEMENT_SLOTS = tuple(range(24, 472, 56))
HELP_OWNER = 0x3dc5205e08a99ca4


def header(data, offset, stride):
    if offset < 0 or offset + 56 > len(data):
        raise ValueError(f'truncated descriptor at {offset:#x}')
    h = c.read_header(data, offset)
    if (h['ptr'], h['z10'], h['z18'], h['flags']) != (0, 0, 0, 1):
        raise ValueError(f'invalid descriptor at {offset:#x}')
    if h['count'] != h['iused'] or h['size'] != h['count'] * stride:
        raise ValueError(f'unexpected count/stride at {offset:#x}')
    return h


def decode(data):
    """Return reconstructed bytes and offset records; reject any unconsumed tail.

    Unknown fixed fields remain uninterpreted. Every array is independently bounded
    by its descriptor; no opaque remainder is accepted as a parsed heap.
    """
    if len(data) > CAP:
        raise ValueError('resource exceeds 64 MiB cap')
    top = header(data, 0, 328)
    cur = 56 + top['size']
    if cur > len(data):
        raise ValueError('truncated top table')
    rebuilt = bytearray(data[:cur])
    rebuilt[:56] = c.pack_header(top)
    arrays, elements = [], []

    def array(descriptor, stride, page, element, slot):
        nonlocal cur
        h = header(data, descriptor, stride)
        rebuilt[descriptor:descriptor+56] = c.pack_header(h)
        start, end = cur, cur + h['size']
        if end > len(data):
            raise ValueError(f'array at {start:#x} exceeds file')
        rebuilt.extend(data[start:end])
        cur = end
        arrays.append(dict(page=page, element=element, slot=slot,
            descriptor_file_offset=descriptor, payload_file_offset=start,
            count=h['count'], stride=stride, size=h['size']))
        return start, h['count']

    for page in range(top['count']):
        page_base = 56 + page * 328
        for slot in PAGE_SLOTS:
            start, count = array(page_base+slot, 472 if slot == 32 else 16,
                                 page, -1, slot)
            if slot != 32:
                continue
            for element in range(count):
                base = start + element * 472
                words = struct.unpack_from('<QQQ', data, base)
                elements.append(dict(page=page, element=element,
                    page_file_offset=page_base, element_file_offset=base,
                    word0=f'0x{words[0]:016x}', word1=f'0x{words[1]:016x}',
                    word2=f'0x{words[2]:016x}', help_owner_match=words[0] == HELP_OWNER))
                for child_slot in ELEMENT_SLOTS:
                    child_stride = {80: 16, 416: 72}.get(child_slot, 0)
                    h = header(data, base+child_slot, child_stride)
                    if child_stride == 0 and h['count']:
                        raise ValueError('unobserved populated element slot')
                    array(base+child_slot, child_stride, page, element, child_slot)
    if cur != len(data):
        raise ValueError(f'unconsumed bytes: {len(data)-cur}')
    if rebuilt != data:
        raise ValueError('descriptor/array reconstruction mismatch')
    return bytes(rebuilt), arrays, elements


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--output', type=Path, required=True)
    args = ap.parse_args()
    out = args.output.resolve()
    if not out.is_relative_to((ROOT/'generic_rebuilds/menu-ui').resolve()):
        ap.error('output must be under canonical-root generic_rebuilds/menu-ui')
    arrays, elements, files = [], [], []
    for name in ('mnu_master', 'mnu_master_mp_ingame'):
        path = ROOT/'Primary/CR15UIPage2CR'/name
        if path.stat().st_size > CAP:
            raise ValueError('resource exceeds 64 MiB cap')
        data = path.read_bytes()
        rebuilt, aa, ee = decode(data)
        provenance = dict(build=BUILD, platform='win10_dx12', layer='Primary',
            type='CR15UIPage2CR', root=name, source=str(path),
            evidence='stream-confirmed')
        arrays.extend(dict(**provenance, **row) for row in aa)
        elements.extend(dict(**provenance, **row) for row in ee)
        files.append(dict(**provenance, size=len(data),
            sha256=hashlib.sha256(data).hexdigest(), roundtrip=rebuilt == data,
            page_records=header(data, 0, 328)['count'], element_records=len(ee),
            nested_payload_bytes=sum(r['size'] for r in aa if r['element'] >= 0),
            consumed=len(data), help_matches=[r for r in ee if r['help_owner_match']],
            parse_status='complete-array-framing; fixed-field/action semantics unresolved',
            owning_screen_hypothesis='Help candidate only; runtime unverified'))
    out.mkdir(parents=True, exist_ok=True)
    tsv(out/'page_arrays.tsv', arrays, list(arrays[0]))
    tsv(out/'page_elements.tsv', elements, list(elements[0]))
    result = dict(parser_version='final-pc-menu-page-heap-v1',
        script_sha256=sha(Path(__file__)), parser_sha256=sha(Path(c.__file__)), files=files)
    (out/'page_heap.json').write_text(json.dumps(result, indent=2)+'\n')
    print(json.dumps(result, indent=2))


if __name__ == '__main__':
    main()
