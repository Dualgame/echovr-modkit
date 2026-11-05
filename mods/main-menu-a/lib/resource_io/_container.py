"""resource_io._container — the shared v3 CTable / SCombined container walker.

This is task #10 of the Resource Codex (handoff): one segmenting walker
that lifts every type whose Primary is a serialized `CTable` (and the nested /
multi-table compositions of them).

THE 56-BYTE CTABLE HEADER (`stream-confirmed` on the whole CR corpus, cross-checked

    +0x00 u64 ptr   = 0          (runtime data-pointer placeholder, zeroed on disk)
    +0x08 u64 size  = count*stride   (record-array bytes — EXPLICIT, the gate field)
    +0x10 u64 0
    +0x18 u32 0
    +0x1c u32 flags (=1)
    +0x20 u64 mark  (0x20 on some types, 0 on others — carried verbatim, not assumed)
    +0x28 u64 count
    +0x30 u64 iused
    +0x38 records[count], each `size/count` bytes

THREE COMPOSITIONS observed on disk:

  * flat        — the whole file is ONE CTable that consumes every byte
                  (CMaterialTypeCR/CStaticRaycastCR/CFrustumCullCR/COcclusionCullCR/
                   CLevelAABBCR/CBoundingSphereCR/CDecalCR). file == 0x38 + size.
  * self-flat   — a leading u64 self-hash, then ONE CTable
                  (CComponentSpaceResource). file == 8 + 0x38 + size.
  * nested      — a top CTable whose every record embeds a sub-CTable header at a
                  fixed record offset `sub_at`; the sub-tables' record arrays are
                  spilled into a HEAP that follows the top array, in record order
                  (CComponentLODCR/CStaticInstanceModelCR/CModelCR/CEventCR/...).
                  The heap can be one level (CComponentLODCR) — deeper nesting is
                  walked recursively via the same sub_at chain when present.

This is a SEGMENTING decoder: it parses only the EXPLICIT header integers
(size/count/iused/mark/flags) and splits the record array into records, then
recomposes byte-exact. It deliberately does NOT interpret record field semantics —
SIZE fields that gate the round-trip are explicit at every level. A byte-exact
round-trip is the proof the boundaries were located correctly (identity copies are
forbidden: the header is re-emitted from parsed ints, the array from sliced records,
the heap from sliced sub-arrays).

Evidence label: `stream-confirmed` (corpus-wide round-trip on every shipped file).
"""
import struct

HDR = 0x38


def read_header(buf, off=0):
    ptr, size, z10 = struct.unpack_from("<QQQ", buf, off)
    z18, flags = struct.unpack_from("<II", buf, off + 0x18)
    mark, count, iused = struct.unpack_from("<QQQ", buf, off + 0x20)
    return dict(ptr=ptr, size=size, z10=z10, z18=z18, flags=flags,
                mark=mark, count=count, iused=iused)


def pack_header(h):
    out = bytearray(HDR)
    struct.pack_into("<QQQ", out, 0x00, h["ptr"], h["size"], h["z10"])
    struct.pack_into("<II", out, 0x18, h["z18"], h["flags"])
    struct.pack_into("<QQQ", out, 0x20, h["mark"], h["count"], h["iused"])
    return bytes(out)


def looks_like_header(buf, off=0):
    """Structural test for a serialized CTable header at `off`."""
    if off + HDR > len(buf):
        return False
    h = read_header(buf, off)
    if h["ptr"] != 0 or h["z10"] != 0 or h["z18"] != 0:
        return False
    if h["count"] == 0:
        return h["size"] == 0
    return h["size"] % h["count"] == 0


class CTable:
    """A parsed flat CTable: header dict + list of `count` record byte-strings."""

    def __init__(self, header, records):
        self.header = header
        self.records = records

    @property
    def stride(self):
        c = self.header["count"]
        return (self.header["size"] // c) if c else 0

    def pack(self):
        out = bytearray(pack_header(self.header))
        for r in self.records:
            out += r
        return bytes(out)


def read_table(buf, off=0):
    """Parse one flat CTable at `off`. Returns (CTable, end_offset)."""
    h = read_header(buf, off)
    assert h["ptr"] == 0 and h["z10"] == 0 and h["z18"] == 0, \
        f"not a CTable header @0x{off:x} (ptr/z10/z18 nonzero)"
    c = h["count"]
    stride = (h["size"] // c) if c else 0
    if c:
        assert h["size"] == c * stride, f"size {h['size']} not divisible by count {c}"
    base = off + HDR
    end = base + h["size"]
    assert end <= len(buf), f"table @0x{off:x} runs past end ({end} > {len(buf)})"
    recs = [buf[base + i * stride: base + (i + 1) * stride] for i in range(c)]
    return CTable(h, recs), end


def roundtrip_flat(buf):
    """The whole file is a single CTable (the flat CR family)."""
    t, end = read_table(buf, 0)
    assert end == len(buf), f"flat table consumed {end} of {len(buf)} bytes"
    return t.pack()


def roundtrip_self_flat(buf):
    """u64 self-hash + a single CTable (CComponentSpaceResource shape)."""
    assert len(buf) >= 8 + HDR, "too short for self-flat"
    self_hash = buf[0:8]
    t, end = read_table(buf, 8)
    assert end == len(buf), f"self-flat table consumed {end} of {len(buf)} bytes"
    return self_hash + t.pack()


class NestedTable:
    """A top CTable whose records each carry a sub-CTable header at record offset
    `sub_at`; the sub-tables' record arrays follow the top array as an ordered heap.
    `subheaps` is a list parallel to top.records, each the raw sub-record bytes."""

    def __init__(self, top, sub_at, subheaps):
        self.top = top
        self.sub_at = sub_at
        self.subheaps = subheaps

    def pack(self):
        out = bytearray(self.top.pack())
        for h in self.subheaps:
            out += h
        return bytes(out)


def read_nested(buf, off, sub_at):
    """Parse a nested table: top CTable at `off`, each record embedding a sub-CTable
    header at record-relative `sub_at`, sub-record arrays spilled to a heap after the
    top array in record order. Returns (NestedTable, end_offset)."""
    top, heap = read_table(buf, off)
    subheaps = []
    cur = heap
    for i, rec in enumerate(top.records):
        assert sub_at + HDR <= len(rec), f"record {i} too short for sub-header @0x{sub_at:x}"
        sh = read_header(rec, sub_at)
        assert sh["ptr"] == 0 and sh["z10"] == 0 and sh["z18"] == 0, \
            f"record {i} sub-header @0x{sub_at:x} not a CTable header"
        seg = buf[cur:cur + sh["size"]]
        assert len(seg) == sh["size"], f"heap underrun for record {i}"
        subheaps.append(seg)
        cur += sh["size"]
    return NestedTable(top, sub_at, subheaps), cur


def roundtrip_nested(buf, sub_at):
    """The whole file is a nested table (top + ordered sub-record heap)."""
    nt, end = read_nested(buf, 0, sub_at)
    assert end == len(buf), f"nested table consumed {end} of {len(buf)} bytes"
    return nt.pack()


class RecursiveNested:
    """A multi-level nested table. The top CTable's record array is written inline;
    every deeper level's sub-record arrays follow in the heap, BREADTH-BY-LEVEL:

        [top array] [all level-1 sub-arrays] [all level-2 sub-arrays] ...

    Confirmed (CComponentLODCR depth-2): combat's only depth-2 content comes from
    top-record #21 of 298, yet its sub-arrays sit at the very END of the file (after
    all 298 level-1 arrays) — which rules out strict depth-first interleaving and
    fixes the order as breadth-by-level. `segments` is the ordered list of heap
    sub-arrays (each a sub-table's record bytes); the sub-CTable HEADERS live inside
    the parent records (already in `top`), so pack = top.pack() + concat(segments).
    `sub_ats[d]` is the record-relative sub-CTable header offset at nesting depth d."""

    def __init__(self, top, sub_ats, segments):
        self.top = top
        self.sub_ats = sub_ats
        self.segments = segments

    def pack(self):
        return self.top.pack() + b"".join(self.segments)


def read_nested_recursive(buf, off, sub_ats):
    """Parse a multi-level nested table at `off`. `sub_ats` is the per-depth list of
    record-relative sub-CTable header offsets (e.g. [0x28, 0x40] for CComponentLODCR).
    Walks level by level: at each depth, every current-level record that carries a
    valid sub-CTable header at `sub_ats[depth]` spills its record array from the
    monotonic heap cursor (in record order); those records become the next level.
    Returns (RecursiveNested, end_offset)."""
    top, cur = read_table(buf, off)
    segments = []
    level_records = top.records
    for depth, sub_at in enumerate(sub_ats):
        nxt = []
        for rec in level_records:
            if sub_at + HDR > len(rec):
                continue
            sh = read_header(rec, sub_at)
            if not (sh["ptr"] == 0 and sh["z10"] == 0 and sh["z18"] == 0):
                continue
            size, count = sh["size"], sh["count"]
            if count and size % count:
                continue
            seg = buf[cur:cur + size]
            assert len(seg) == size, f"heap underrun at depth {depth} (cur=0x{cur:x})"
            segments.append(seg)
            cur += size
            if count:
                stride = size // count
                if stride:
                    nxt += [seg[k * stride:(k + 1) * stride] for k in range(count)]
        level_records = nxt
        if not level_records:
            break
    return RecursiveNested(top, sub_ats, segments), cur


def roundtrip_nested_recursive(buf, sub_ats):
    """The whole file is a multi-level nested table (breadth-by-level heap)."""
    rn, end = read_nested_recursive(buf, 0, sub_ats)
    assert end == len(buf), f"recursive nested consumed {end} of {len(buf)} bytes"
    return rn.pack()



def _df_consume(buf, records, sub_ats, depth, state, segments):
    """Depth-first heap consumption. `sub_ats[depth]` is the record-relative sub-CTable
    header offset at this depth; for each record (in order) spill its sub-array from the
    monotonic cursor, then recurse into that sub-array's elements before the next record.
    `sub_ats` may be a list (explicit) or a dict accumulated by the auto-detector."""
    if depth >= len(sub_ats):
        return
    sa = sub_ats[depth]
    for rec in records:
        if sa + HDR > len(rec):
            continue
        sh = read_header(rec, sa)
        if not (sh["ptr"] == 0 and sh["z10"] == 0 and sh["z18"] == 0):
            continue
        size, count = sh["size"], sh["count"]
        if count and size % count:
            continue
        seg = buf[state[0]:state[0] + size]
        if len(seg) != size:
            raise ValueError(f"df heap underrun at depth {depth} (cur=0x{state[0]:x})")
        segments.append(seg)
        state[0] += size
        if count:
            stride = size // count
            if stride:
                subrecs = [seg[k * stride:(k + 1) * stride] for k in range(count)]
                _df_consume(buf, subrecs, sub_ats, depth + 1, state, segments)


def roundtrip_nested_recursive_df(buf, sub_ats):
    """The whole file is a DEPTH-FIRST (record-major) recursive nested forest with the
    per-depth sub_at chain `sub_ats`. Re-emits top.pack() + the depth-first segment
    stream; byte-exact match proves the chain + order."""
    top, cur0 = read_table(buf, 0)
    segments, state = [], [cur0]
    _df_consume(buf, top.records, sub_ats, 0, state, segments)
    if state[0] != len(buf):
        raise ValueError(f"df recursive consumed {state[0]} of {len(buf)} bytes")
    return top.pack() + b"".join(segments)


def detect_nested_recursive_df(buf, off=0, max_depth=8):
    """Auto-detect the per-depth sub_at chain for a depth-first recursive forest by a
    single consuming pass that picks, at each newly-seen depth, the first record-relative
    offset where EVERY record carries a structural header and >=1 is well-formed. Returns
    the sub_ats list or None. The caller MUST still validate with a byte-exact
    `roundtrip_nested_recursive_df` (this only locates the chain)."""
    top, cur0 = read_table(buf, off)
    if not top.records:
        return None
    sub_ats = {}
    state = [cur0]

    def pick(records, depth):
        if not records:
            return
        stride = len(records[0])
        if stride < HDR:
            return
        if depth not in sub_ats:
            cand = None
            o = 0
            while o + HDR <= stride:
                if all(looks_like_header(r, o) for r in records) and \
                   any(_well_formed_subtable(read_header(r, o)) for r in records):
                    cand = o
                    break
                o += 8
            if cand is None:
                return
            sub_ats[depth] = cand
        sa = sub_ats[depth]
        for rec in records:
            sh = read_header(rec, sa)
            if not (sh["ptr"] == 0 and sh["z10"] == 0 and sh["z18"] == 0):
                continue
            size, count = sh["size"], sh["count"]
            if count and size % count:
                continue
            if state[0] + size > len(buf):
                return
            seg = buf[state[0]:state[0] + size]
            state[0] += size
            if count:
                st = size // count
                if st:
                    pick([seg[k * st:(k + 1) * st] for k in range(count)], depth + 1)

    pick(top.records, 0)
    if not sub_ats:
        return None
    return [sub_ats[d] for d in sorted(sub_ats)]


def detect_sub_at(buf, off=0, stride=None):
    """Best-effort: find the record offset where the embedded sub-CTable header sits,
    by scanning for a structural header whose declared size accounts for the file's
    trailing heap. Returns the offset or None. Used to bootstrap a new nested type;
    once known, pass sub_at explicitly."""
    top, heap = read_table(buf, off)
    if not top.records:
        return None
    rec0 = top.records[0]
    stride = stride or top.stride
    for cand in range(0, stride - HDR + 1, 8):
        if not looks_like_header(rec0, cand):
            continue
        total = 0
        ok = True
        for rec in top.records:
            if cand + HDR > len(rec) or not looks_like_header(rec, cand):
                ok = False
                break
            total += read_header(rec, cand)["size"]
        if ok and heap + total == len(buf):
            return cand
    return None


def _well_formed_subtable(h):
    """A header describes a populated sub-table iff count>0, size is an integral
    multiple of count, AND the implied stride is positive. The stride>0 floor is the
    critical OOM guard: an all-zero or garbage record window passes `looks_like_header`
    with size=0/count=huge (0 % count == 0), and building `count` zero-width slices
    blows up memory (s-qrt07: 18 GB on real OBB bytes). Bounding count by a positive
    stride caps records-built at `size` (<= len(buf))."""
    c = h["count"]
    return c > 0 and h["size"] % c == 0 and h["size"] // c > 0


_MAX_SUB_RECORDS = 1 << 20


def _level_sub_at_candidates(records):
    """Record-relative offsets where EVERY record of this level carries a structural
    CTable header AND at least one record declares a WELL-FORMED non-empty sub-table.

    Restricting to "present in all records + >=1 well-formed non-empty" keeps the
    recursive search tiny and gated: an all-zero record window satisfies
    `looks_like_header` at every 8-offset (ptr/size/count all 0), so without the
    well-formed floor the candidate set would explode and a degenerate sub_at could
    spuriously "consume" a zero-length heap (or trigger the count=huge blowup). A
    genuine nested level always has at least one populated sub-table."""
    if not records:
        return []
    stride = len(records[0])
    if stride < HDR:
        return []
    out = []
    for cand in range(0, stride - HDR + 1, 8):
        if all(looks_like_header(r, cand) for r in records) and \
           any(_well_formed_subtable(read_header(r, cand)) for r in records):
            out.append(cand)
    return out


def detect_nested_sub_ats(buf, off=0, max_depth=4):
    """Auto-detect the per-depth `sub_ats` list of a (possibly multi-level) nested
    CTable so that the breadth-by-level heap (see RecursiveNested) is consumed
    EXACTLY. DFS over candidate sub_ats at each level; the gate is exact full-file
    consumption. Returns a sub_ats list (len>=1) or None.

    This generalises `detect_sub_at` (single level) to the recursive shape used by
    CComponentLODCR ([0x28,0x40]) without hard-coding the depth or offsets. The
    list it returns must still be validated by a byte-exact
    `roundtrip_nested_recursive` (the caller's real gate); this only LOCATES it.

    Memory-bounded (s-qrt07): a record's sub-table is only descended when
    well-formed (positive stride), so `count` slices are bounded by `size`
    (<= len(buf)); a global `_MAX_SUB_RECORDS` backstop aborts a pathological branch.
    Mirrors `read_nested_recursive`'s per-record skip semantics so a detected list
    re-walks identically."""
    top, heap0 = read_table(buf, off)
    if not top.records:
        return None

    def solve(level_records, cur, depth):
        if cur == len(buf):
            return []
        if depth >= max_depth:
            return None
        for sa in _level_sub_at_candidates(level_records):
            nxt, c, ok = [], cur, True
            for rec in level_records:
                sh = read_header(rec, sa)
                size, count = sh["size"], sh["count"]
                if count and not _well_formed_subtable(sh):
                    continue
                if c + size > len(buf):
                    ok = False
                    break
                seg = buf[c:c + size]
                c += size
                if count:
                    stride = size // count
                    nxt += [seg[k * stride:(k + 1) * stride] for k in range(count)]
                    if len(nxt) > _MAX_SUB_RECORDS:
                        ok = False
                        break
            if not ok:
                continue
            sub = solve(nxt, c, depth + 1)
            if sub is not None:
                return [sa] + sub
        return None

    return solve(top.records, heap0, 0)



def _multi_sub_at_candidates(records):
    """Record-relative offsets where EVERY record carries a structural CTable header
    AND at least one record declares a WELL-FORMED non-empty sub-table (count>0,
    positive stride). The well-formed floor excludes (a) all-zero windows that pass
    `looks_like_header` trivially and (b) spurious count>0/size=0 matches inside the
    universal entry header — both consume 0 heap bytes, so dropping them is lossless
    for the record-major reconstruction (see module note)."""
    if not records:
        return []
    stride = len(records[0])
    if stride < HDR:
        return []
    out = []
    off = 0
    while off + HDR <= stride:
        if all(looks_like_header(r, off) for r in records) and \
           any(_well_formed_subtable(read_header(r, off)) for r in records):
            out.append(off)
        off += 8
    return out


def read_nested_multi(buf, off, sub_ats, order="record"):
    """Top CTable at `off`; each record embeds K sub-CTable headers at the offsets in
    `sub_ats`; the K*N sub-record arrays spill into the heap after the top array.
    `order` = 'record' (record-major, slot-ascending) or 'slot' (slot-major). Returns
    (top, segments, end). The sub-CTable HEADERS live inside the records (already in
    `top`), so pack = top.pack() + concat(segments-in-read-order). Gated by the
    caller's exact-consumption round-trip — never an identity copy (segments are
    sliced from the heap by each sub-header's declared size)."""
    top, cur = read_table(buf, off)
    segments = []
    if order == "record":
        pairs = [(rec, sa) for rec in top.records for sa in sub_ats]
    elif order == "slot":
        pairs = [(rec, sa) for sa in sub_ats for rec in top.records]
    else:
        raise ValueError(f"unknown order {order!r}")
    for rec, sa in pairs:
        sh = read_header(rec, sa)
        size = sh["size"]
        seg = buf[cur:cur + size]
        assert len(seg) == size, f"heap underrun @sub_at 0x{sa:x} (cur=0x{cur:x}, need {size})"
        segments.append((sa, seg))
        cur += size
    return top, segments, cur


def roundtrip_nested_multi(buf, sub_ats, order="record"):
    """The whole file is a K-slot multi-nested table (top + shared heap). Re-emits
    top.pack() then the heap segments IN THE SAME ORDER they were sliced (record- or
    slot-major), so a byte-exact match proves both the slot set and the spill order."""
    top, segments, end = read_nested_multi(buf, 0, sub_ats, order)
    assert end == len(buf), f"multi-nested consumed {end} of {len(buf)} bytes"
    return top.pack() + b"".join(seg for _sa, seg in segments)


def detect_nested_multi(buf, off=0):
    """Locate the K-slot multi-nested shape: detect the populated sub_at offsets, then
    try record-major then slot-major heap consumption. Returns (sub_ats, order) iff one
    order round-trips byte-exact (verified here), else None. Requires K>=2 populated
    slots in the file (K==1 is the existing single `nested` shape — leave it to that
    branch so labels stay stable)."""
    top, _heap = read_table(buf, off)
    if not top.records:
        return None
    sub_ats = _multi_sub_at_candidates(top.records)
    if len(sub_ats) < 2:
        return None
    for order in ("record", "slot"):
        try:
            if roundtrip_nested_multi(buf, sub_ats, order) == buf:
                return (sub_ats, order)
        except Exception:
            pass
    return None



def roundtrip_desc_forest(buf, min_descs=1):
    """Parse [top thin-entries CTable][preamble][K well-formed 56-B descriptors][K
    arrays in order][trailing] and re-emit byte-exact. Raises if the shape does not
    hold or the file is not fully consumed."""
    top, end = read_table(buf, 0)
    n = len(buf)
    pre = None
    o = end
    while o + HDR <= n:
        if looks_like_header(buf, o) and _well_formed_subtable(read_header(buf, o)):
            pre = o
            break
        o += 8
    if pre is None:
        raise ValueError("no descriptor block after entries")
    descs = []
    o = pre
    while o + HDR <= n and looks_like_header(buf, o) and _well_formed_subtable(read_header(buf, o)):
        descs.append(read_header(buf, o))
        o += HDR
    if len(descs) < min_descs:
        raise ValueError(f"only {len(descs)} descriptors (< {min_descs})")
    cur = o
    arrays = []
    for i, h in enumerate(descs):
        seg = buf[cur:cur + h["size"]]
        if len(seg) != h["size"]:
            raise ValueError(f"array {i} underrun (cur=0x{cur:x}, need {h['size']})")
        arrays.append(seg)
        cur += h["size"]
    trailing = buf[cur:]
    out = bytearray(top.pack())
    out += buf[end:pre]
    for h in descs:
        out += pack_header(h)
    for a in arrays:
        out += a
    out += trailing
    if bytes(out) != buf:
        raise ValueError("desc-forest reconstruction != buf")
    return bytes(out)


_MULTISHAPE_ORDER = ("flat", "self_flat", "nested", "nested_rec", "multi_nested")


def classify_multishape(buf):
    """Return the label of the FIRST shape in {flat, self_flat, nested(auto sub_at),
    nested_rec(auto multi-level sub_ats)} that round-trips `buf` byte-exact, else None.

    A per-file shape dispatch: a multi-shape type is one whose empty/stub instances
    serialize as one shape (e.g. flat) and whose populated instances serialize as
    another (e.g. nested(sa=..)). Every branch reconstructs from parsed headers +
    sliced heap and compares `== buf`, so a label is never returned on a coincidental
    structural parse — only on a proven byte-exact recomposition."""
    try:
        if roundtrip_flat(buf) == buf:
            return "flat"
    except Exception:
        pass
    try:
        if roundtrip_self_flat(buf) == buf:
            return "self_flat"
    except Exception:
        pass
    try:
        sa = detect_sub_at(buf)
        if sa is not None and roundtrip_nested(buf, sa) == buf:
            return f"nested(sa=0x{sa:x})"
    except Exception:
        pass
    try:
        sas = detect_nested_sub_ats(buf)
        if sas and roundtrip_nested_recursive(buf, sas) == buf:
            return "nested_rec(sa=" + ",".join(f"0x{s:x}" for s in sas) + ")"
    except Exception:
        pass
    try:
        det = detect_nested_multi(buf)
        if det is not None:
            sub_ats, order = det
            return ("multi_nested(" + order + ",sa=" +
                    ",".join(f"0x{s:x}" for s in sub_ats) + ")")
    except Exception:
        pass
    try:
        sas = detect_nested_recursive_df(buf)
        if sas and roundtrip_nested_recursive_df(buf, sas) == buf:
            return "nested_rec_df(sa=" + ",".join(f"0x{s:x}" for s in sas) + ")"
    except Exception:
        pass
    return None


def roundtrip_multishape(buf):
    """Per-file multi-shape walker: reconstruct `buf` via the first of
    {flat, self_flat, nested(auto), nested_rec(auto)} that round-trips byte-exact.
    Returns the reconstructed bytes (== buf) or raises if NONE of the four shapes
    fits — gated, never a false pass (each branch is itself byte-exact-gated)."""
    if classify_multishape(buf) is not None:
        return buf
    raise ValueError(
        "no shape in {flat, self_flat, nested, nested_rec} round-trips this blob")
