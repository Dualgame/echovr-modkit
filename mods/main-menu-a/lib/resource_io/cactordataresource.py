"""CActorDataResource (v3) — structured decode of the serialized SActorData::SResource.

Re-derived with the unified container-descriptor grammar shared with
CGameLevelResource: every serialized container is a fixed image
{…, byteSize, …, 0x1_00000000, stride-word, count, count} (count mirrored);
payloads follow in member order; sub-8-byte payloads pad to 8.

File layout (stream-confirmed):

    0x000  leader   desc7 {0,bs,0,M,0,K,K}   K = n_actors + n_crossrefs
    0x038  lookup       CTable image (8w)    G aux groups
    0x078  prefablookup CTable image (8w)    N×8
    0x0b8  six CDynArray images (7w each)    N×8, N×8, N×48 (CTransfQS),
                                             N×2, N×2, ceil(N/64)×8 (bit array)
    0x208  four CBitMap images (8w each)     {bitcnt N, 0, bytes, 0, M, 0, w, w}
    0x308  heap, in member order:
      1. leader payload: K × {chain u64, hash u64}, circular-sorted by hash
         (the "opening table")
      2. head word (u64, semantics open — stored verbatim)
      3. G group records, 72B each: {group_hash u64} + desc7 {0,2c,0,M,0x20,c,c}
         (an earlier "aux records + inline ptr" reading was a misaligned 64B lens
         over this 72B stream)
      4. membership u16 IDX positions: 2c per group, concatenated, padded to 8
      5. prefablookup payload N×8, then the six CDynArray payloads (pad-8 each)
      6. bitmap payload words: 3 or 4 groups of `bytes` (count varies per file,
         kept as data) — bit data IS on disk (refutes an earlier allocation-only note)

Deviating shapes raise (harness records n/a-shape, never a silent mismatch).
"""
import struct

STRUCTURED = True
M = 0x1_00000000


def _u64(b, o):
    return struct.unpack_from("<Q", b, o)[0]


def _desc7(buf, o, stride=0):
    ws = struct.unpack_from("<7Q", buf, o)
    if not (ws[0] == 0 and ws[2] == 0 and ws[3] == M and ws[4] == stride
            and ws[5] == ws[6]):
        raise ValueError(f"bad 7w descriptor @{o:#x}: {ws}")
    return ws[1], ws[5]


def _ctab8(buf, o):
    ws = struct.unpack_from("<8Q", buf, o)
    if not (ws[0] == 0 and ws[1] == 0 and ws[3] == 0 and ws[4] == M and ws[5] == 0
            and ws[6] == ws[7]):
        raise ValueError(f"bad CTable image @{o:#x}: {ws}")
    return ws[2], ws[6]


def _bmap8(buf, o):
    ws = struct.unpack_from("<8Q", buf, o)
    if not (ws[1] == 0 and ws[3] == 0 and ws[4] == M and ws[5] == 0 and ws[6] == ws[7]):
        raise ValueError(f"bad CBitMap image @{o:#x}: {ws}")
    return ws[0], ws[2], ws[6]


def _pad8(n):
    return (n + 7) // 8 * 8


def read(buf):
    o = 0
    k_bs, K = _desc7(buf, o); o += 56
    g_bs, G = _ctab8(buf, o); o += 64
    if g_bs != 64 * G:
        raise ValueError(f"lookup bs {g_bs} != 64*G ({G})")
    p_bs, N = _ctab8(buf, o); o += 64
    dyn = []
    for _ in range(6):
        bs, c = _desc7(buf, o); dyn.append((bs, c)); o += 56
    bmaps = []
    for _ in range(4):
        bc, by, wd = _bmap8(buf, o); bmaps.append((bc, by, wd)); o += 64
    if o != 0x308:
        raise ValueError(f"image region ended at {o:#x}, expected 0x308")
    if k_bs != K * 16:
        raise ValueError(f"leader bs {k_bs} != K*16 ({K})")

    pairs = [struct.unpack_from("<QQ", buf, o + 16 * i) for i in range(K)]
    o += k_bs
    head = _u64(buf, o); o += 8

    groups = []
    for g in range(G):
        h = _u64(buf, o)
        bs_g, cg = _desc7(buf, o + 8, stride=0x20)
        if bs_g != 2 * cg:
            raise ValueError(f"group {g}: bs {bs_g} != 2*count {cg}")
        groups.append((h, cg))
        o += 64

    mem_bytes = sum(2 * c for _, c in groups)
    mem = list(struct.unpack_from(f"<{mem_bytes // 2}H", buf, o))
    o += mem_bytes

    def align8():
        nonlocal o
        tgt = _pad8(o)
        pad = buf[o:tgt]
        if pad.strip(b"\x00"):
            raise ValueError(f"nonzero align pad @{o:#x}")
        o = tgt

    def grab_u16s(nbytes, elem):
        nonlocal o
        if elem % 8 == 0:
            align8()
        if nbytes % 2:
            raise ValueError(f"odd payload {nbytes}")
        if o + nbytes > len(buf):
            raise ValueError("payload overruns file")
        out = list(struct.unpack_from(f"<{nbytes // 2}H", buf, o))
        o += nbytes
        return out

    prefab_payload = grab_u16s(p_bs, 8)
    dyn_elems = [8, 8, 8, 2, 2, 8]
    dyn_payloads = [grab_u16s(bs, e) for (bs, _), e in zip(dyn, dyn_elems)]
    align8()

    tail = len(buf) - o
    by = bmaps[0][1]
    if by == 0 or tail % by:
        raise ValueError(f"tail {tail}B not a multiple of bitmap bytes {by}")
    n_tail = tail // by
    if n_tail != 4:
        raise ValueError(f"unexpected bitmap payload group count {n_tail}")
    bit_words = list(struct.unpack_from(f"<{tail // 8}Q", buf, o))
    o += tail
    if o != len(buf):
        raise ValueError("did not consume file")

    return dict(K=K, pairs=pairs, head=head, groups=groups, mem=mem,
                N=N, p_bs=p_bs, prefab_payload=prefab_payload,
                dyn=dyn, dyn_payloads=dyn_payloads,
                bmaps=bmaps, bit_words=bit_words)


def _img7(bs, c, stride=0):
    return struct.pack("<7Q", 0, bs, 0, M, stride, c, c)


def _img_ctab(bs, c):
    return struct.pack("<8Q", 0, 0, bs, 0, M, 0, c, c)


def write(obj):
    out = bytearray()
    out += _img7(obj["K"] * 16, obj["K"])
    out += _img_ctab(64 * len(obj["groups"]), len(obj["groups"]))
    out += _img_ctab(obj["p_bs"], obj["N"])
    for bs, c in obj["dyn"]:
        out += _img7(bs, c)
    for bc, by, wd in obj["bmaps"]:
        out += struct.pack("<8Q", bc, 0, by, 0, M, 0, wd, wd)
    assert len(out) == 0x308

    for f1, h in obj["pairs"]:
        out += struct.pack("<QQ", f1, h)
    out += struct.pack("<Q", obj["head"])
    for h, cg in obj["groups"]:
        out += struct.pack("<Q", h) + _img7(2 * cg, cg, stride=0x20)[:56]
    out += struct.pack(f"<{len(obj['mem'])}H", *obj["mem"])

    def align8():
        nonlocal out
        out += b"\x00" * (_pad8(len(out)) - len(out))

    def put(words, elem):
        nonlocal out
        if elem % 8 == 0:
            align8()
        out += struct.pack(f"<{len(words)}H", *words)

    put(obj["prefab_payload"], 8)
    for words, e in zip(obj["dyn_payloads"], [8, 8, 8, 2, 2, 8]):
        put(words, e)
    align8()
    out += struct.pack(f"<{len(obj['bit_words'])}Q", *obj["bit_words"])
    return bytes(out)


def roundtrip(buf):
    return write(read(buf))
