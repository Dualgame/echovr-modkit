#!/usr/bin/env python3
"""Quest: make CR15NetGame::LoadLocalConfig try `/sdcard/readyatdawn/_local/config.json` before the stock tier-2 file
(a step of the Quest APK builder). Quest 6528386917200980 vc 4987566, sites found by disassembly; text VA == file
offset in this .so. Written against STOCK bytes: it does not reuse the
community trampoline, which overwrote live code at 0xf85264.

One 4-byte site plus a stub in a zero run of .text (0x2a75120-0x2a7611f, 4096 B, R+X LOAD 0):
  0x124d610  mov x2,x0 (e2 03 00 aa)  ->  b CAVE                     tier 2's "%s|%s" base = the local-dir char*
  CAVE = 0x2a75200 (cave+0xe0):  probe the /sdcard file with SPrintF + FileExists into the caller's scratch
  CFixedStringT<512> (caller sp+0x200 = stub sp+0x220); x2 = "/sdcard/readyatdawn/_local" if it exists, else the
  stock x0; re-materialise the x1 / x3 ADRP pages the calls clobbered; b 0x124d614 (the stock ADDs finish them).
Every call target and page is DECODED from the stock instructions around the site and asserted, not trusted.

  quest_libr15_sdcard_config_patch.py --so <stock.so> --out <dir>/libr15.so   write a patched COPY of the stock libr15.so
  quest_libr15_sdcard_config_patch.py --so <stock.so> --dry-run               print the patched-byte listing, write nothing
  quest_libr15_sdcard_config_patch.py --so <stock.so> --verify <patched.so>   re-check a patched file against the stock one
  quest_libr15_sdcard_config_patch.py --so <stock.so> --xref-scan             nothing may reference the cave (exit 1 if so)
The input is the stock libr15.so of the Quest APK (sha256 8dd9a961...). Every byte outside the patched ranges must
equal it. Refuses to write over its input or onto a hardlink."""
import argparse, hashlib, os, struct, sys

BUILD = '6528386917200980'                            # vc 4987566
STOCK_SHA = '8dd9a961'                                # prefix; the full hash is printed on use
SITE = 0x124d610
CAVE_LO, CAVE_HI = 0x2a75120, 0x2a76120               # zero run, half-open
STUB = 0x2a75200                                      # cave + 0xe0
# the one stock reference into the cave: an unreachable `b` (no FDE, no inbound xref, lands in zeros at cave+0xff4)
DEAD_B = 0x2a75114
KNOWN_HIT = '.text B/BL imm26 0x2a75114 -> 0x2a76114'
REJOIN = 0x124d614
# stock tier-2 block 0x124d5fc-0x124d640 (LoadLocalConfig 0x124d594): every assumption the stub makes lives here
STOCK_BLOCK = (0x124d5fc, bytes.fromhex(
    'a82801b0006144f94780f39721f900d0e3ca00f0e20300aa214c209163043391'
    'e0030891d42df297e0030891e1031f2a3911f397e0030891e1031faa825df297'
    '40010035'))
PROBE = b'/sdcard/readyatdawn/_local/config.json\0'
BASE = b'/sdcard/readyatdawn/_local\0'
X0, X1, X2, X3, X29, X30, SP, XZR = 0, 1, 2, 3, 29, 30, 31, 31


# ---- explicit AArch64 encoders (A64 ISA; each checked by the decoder below and, if present, capstone) ----------
def _rel(frm, to, bits, shift=2):
    d = to - frm
    assert d % (1 << shift) == 0, (hex(frm), hex(to))
    v = d >> shift
    assert -(1 << (bits - 1)) <= v < (1 << (bits - 1)), f'out of range {hex(frm)}->{hex(to)}'
    return v & ((1 << bits) - 1)

def b(pc, t):        return 0x14000000 | _rel(pc, t, 26)
def bl(pc, t):       return 0x94000000 | _rel(pc, t, 26)
def cbz_w(pc, rt, t): return 0x34000000 | _rel(pc, t, 19) << 5 | rt
def adr(pc, rd, t):
    v = _rel(pc, t, 21, 0)
    return 0x10000000 | (v & 3) << 29 | (v >> 2) << 5 | rd
def adrp(pc, rd, t):
    assert t & 0xfff == 0
    v = _rel(pc & ~0xfff, t, 21, 12)
    return 0x90000000 | (v & 3) << 29 | (v >> 2) << 5 | rd
def add_imm(rd, rn, imm): assert 0 <= imm < 0x1000; return 0x91000000 | imm << 10 | rn << 5 | rd
def mov_xzr(rd):     return 0xaa1f03e0 | rd                      # orr xd, xzr, xzr
def str_x(rt, rn, off):  assert off % 8 == 0 and 0 <= off < 0x8000; return 0xf9000000 | (off // 8) << 10 | rn << 5 | rt
def ldr_x(rt, rn, off):  assert off % 8 == 0 and 0 <= off < 0x8000; return 0xf9400000 | (off // 8) << 10 | rn << 5 | rt
def stp_pre(t1, t2, rn, off):  assert off % 8 == 0; return 0xa9800000 | ((off // 8) & 0x7f) << 15 | t2 << 10 | rn << 5 | t1
def ldp_post(t1, t2, rn, off): assert off % 8 == 0; return 0xa8c00000 | ((off // 8) & 0x7f) << 15 | t2 << 10 | rn << 5 | t1


# ---- decoders for the stock instructions we derive targets from ------------------------------------------------
def u32(buf, va): return struct.unpack_from('<I', buf, va)[0]
def _sx(v, bits): return v - (1 << bits) if v >> (bits - 1) & 1 else v
def dec_bl(w, pc):
    assert w & 0xfc000000 == 0x94000000, hex(w); return pc + _sx(w & 0x3ffffff, 26) * 4
def dec_adrp(w, pc):
    assert w & 0x9f000000 == 0x90000000, hex(w)
    imm = (w >> 5 & 0x7ffff) << 2 | (w >> 29 & 3)
    return w & 31, (pc & ~0xfff) + _sx(imm, 21) * 4096
def dec_add(w):
    assert w & 0xffc00000 == 0x91000000, hex(w); return w & 31, w >> 5 & 31, w >> 10 & 0xfff


def derive(buf):
    """Re-derive every target from the stock block; assert it matches the table."""
    lo, blk = STOCK_BLOCK
    assert buf[lo:lo + len(blk)] == blk, 'stock tier-2 block 0x124d5fc-0x124d640 differs from the table'
    r1, p1 = dec_adrp(u32(buf, 0x124d608), 0x124d608)
    r3, p3 = dec_adrp(u32(buf, 0x124d60c), 0x124d60c)
    assert (r1, p1, r3, p3) == (X1, 0x3173000, X3, 0x2bac000), (r1, hex(p1), r3, hex(p3))
    assert u32(buf, SITE) == 0xaa0003e2                                   # mov x2, x0
    assert dec_add(u32(buf, 0x124d614)) == (X1, X1, 0x813)                # x1 = "%s|%s" 0x3173813
    assert dec_add(u32(buf, 0x124d618)) == (X3, X3, 0xcc1)                # x3 = "config.json" 0x2baccc1
    assert dec_add(u32(buf, 0x124d61c)) == (X0, SP, 0x200)                # x0 = caller sp+0x200 (CFixedStringT<512>)
    sprintf = dec_bl(u32(buf, 0x124d620), 0x124d620)
    exists = dec_bl(u32(buf, 0x124d638), 0x124d638)
    assert (sprintf, exists) == (0xed8d70, 0xee4c40), (hex(sprintf), hex(exists))
    assert buf[0x124d634:0x124d638] == bytes.fromhex('e1031faa')          # mov x1, xzr  (FileExists(buf, NULL))
    assert buf[0x124d5a4:0x124d5b4] == bytes.fromhex('e003089102408052e1031f2a54e7f397')  # memset(sp+0x200,0,0x200)
    assert buf[0x124d59c:0x124d5a0] == bytes.fromhex('ff0310d1')          # sub sp, sp, #0x400 (frame)
    assert buf[0x124d6ac:0x124d6b0] == bytes.fromhex('ff031091')          # add sp, sp, #0x400
    assert buf[0x124d6b4:0x124d6b8] == bytes.fromhex('fc53c2a8')          # ldp x28,x20,[sp],#0x20
    assert buf[0x3173813:0x3173819] == b'%s|%s\0'
    assert buf[0x2baccc1:0x2baccc1 + 12] == b'config.json\0'
    return dict(sprintf=sprintf, exists=exists, page1=p1, page3=p3)


def build_stub(t):
    """Returns (code_words, blob, string VAs). Caller frame on entry: sp = S, buffer at S+0x200..S+0x3ff."""
    code_n = 15
    s_probe = STUB + 0x40
    s_base = (s_probe + len(PROBE) + 7) & ~7
    pc = [STUB + 4 * i for i in range(code_n)]
    lbl_join = pc[11]
    w = [
        stp_pre(X29, X30, SP, -0x20),     # sp = S-0x20
        str_x(X0, SP, 0x10),              # keep the stock base (local-dir char*)
        add_imm(X0, SP, 0x220),           # x0 = S+0x200, the caller's CFixedStringT<512> scratch
        adr(pc[3], X1, s_probe),
        bl(pc[4], t['sprintf']),          # SPrintF(buf, "/sdcard/readyatdawn/_local/config.json")  (no '%')
        add_imm(X0, SP, 0x220),
        mov_xzr(X1),
        bl(pc[7], t['exists']),           # FileExists(buf, NULL)
        ldr_x(X2, SP, 0x10),              # x2 = stock base
        cbz_w(pc[9], X0, lbl_join),
        adr(pc[10], X2, s_base),          # exists -> x2 = "/sdcard/readyatdawn/_local"
        ldp_post(X29, X30, SP, 0x20),     # sp = S
        adrp(pc[12], X1, t['page1']),     # 0x3173000: the stock ADD at 0x124d614 makes it "%s|%s"
        adrp(pc[13], X3, t['page3']),     # 0x2bac000: the stock ADD at 0x124d618 makes it "config.json"
        b(pc[14], REJOIN),
    ]
    assert len(w) == code_n
    blob = bytearray(struct.pack('<%dI' % code_n, *w))
    blob += bytes(s_probe - STUB - len(blob)) + PROBE
    blob += bytes(s_base - STUB - len(blob)) + BASE
    return w, bytes(blob), (s_probe, s_base)


def patches(stock):
    t = derive(stock)
    _, blob, strs = build_stub(t)
    assert CAVE_LO <= STUB and STUB + len(blob) <= CAVE_HI
    return t, strs, [('site', SITE, stock[SITE:SITE + 4], struct.pack('<I', b(SITE, STUB))),
                     ('stub', STUB, stock[STUB:STUB + len(blob)], blob)]


def listing(t, strs, blob):
    w = struct.unpack_from('<15I', blob)
    lines = [f'  {SITE:#09x}  {struct.pack("<I", b(SITE, STUB)).hex(" ")}   b {STUB:#x}   (was e2 03 00 aa  mov x2, x0)']
    try:
        import capstone
        md = capstone.Cs(capstone.CS_ARCH_ARM64, capstone.CS_MODE_ARM)
        dis = {i.address: f'{i.mnemonic} {i.op_str}' for i in md.disasm(blob[:60], STUB)}
        dis[SITE] = next(f'{i.mnemonic} {i.op_str}' for i in md.disasm(struct.pack('<I', b(SITE, STUB)), SITE))
        assert dis[SITE] == f'b #{STUB:#x}', dis[SITE]
    except ImportError:
        dis = None
    for i, x in enumerate(w):
        va = STUB + 4 * i
        lines.append(f'  {va:#09x}  {struct.pack("<I", x).hex(" ")}   {dis[va] if dis else ""}')
    for va, s in zip(strs, (PROBE, BASE)):
        lines.append(f'  {va:#09x}  "{s[:-1].decode()}\\0" ({len(s)} B)')
    if dis:  # capstone round-trip of the semantic operands
        exp = {STUB + 0x10: f'bl #{t["sprintf"]:#x}', STUB + 0x1c: f'bl #{t["exists"]:#x}',
               STUB + 0x0c: f'adr x1, #{strs[0]:#x}', STUB + 0x28: f'adr x2, #{strs[1]:#x}',
               STUB + 0x24: f'cbz w0, #{STUB + 0x2c:#x}', STUB + 0x30: f'adrp x1, #{t["page1"]:#x}',
               STUB + 0x34: f'adrp x3, #{t["page3"]:#x}', STUB + 0x38: f'b #{REJOIN:#x}',
               STUB + 0x00: 'stp x29, x30, [sp, #-0x20]!', STUB + 0x2c: 'ldp x29, x30, [sp], #0x20',
               STUB + 0x08: 'add x0, sp, #0x220', STUB + 0x14: 'add x0, sp, #0x220',
               STUB + 0x04: 'str x0, [sp, #0x10]', STUB + 0x20: 'ldr x2, [sp, #0x10]', STUB + 0x18: 'mov x1, xzr'}
        bad = {hex(k): (dis.get(k), v) for k, v in exp.items() if dis.get(k) != v}
        assert not bad, f'capstone disagrees with the encoder: {bad}'
        lines.append('  capstone round-trip: OK (15/15 + site)')
    else:
        lines.append('  capstone not importable: encoder-only (no round-trip)')
    return '\n'.join(lines)


# ---- xref scan: nothing in the image may reference [CAVE_LO, CAVE_HI) -----------------------------------------
def xref_scan(buf):
    import numpy as np
    hits, notes = [], []
    # also watch the word before the cave: 0x2a75114 `b 0x2a76114` sits after FUN_02a74fe0's `ret`, outside its FDE
    inr = lambda a: ((a >= CAVE_LO) & (a < CAVE_HI)) | ((a >= DEAD_B) & (a < DEAD_B + 4))
    sx = lambda v, n: np.where(v >> (n - 1) & 1, v - (1 << n), v)
    plo = (CAVE_LO & ~0xfff) - 0x8000
    for name, lo, hi in (('.plt', 0xecfb40, 0xecfb40 + 0x84540), ('.text', 0xf54080, 0xf54080 + 0x1bda6a8)):
        allw = np.frombuffer(buf, dtype='<u4', count=(hi - lo) // 4, offset=lo)
        cnt = dict()
        for c0 in range(0, len(allw), 1 << 20):                 # 1M-word chunks keep the int64 temporaries small
            w = allw[c0:c0 + (1 << 20)].astype(np.int64)
            pc = lo + 4 * (c0 + np.arange(len(w), dtype=np.int64))
            own = (pc >= CAVE_LO) & (pc < CAVE_HI)              # the cave's own zero words (udf); skip them
            kinds = {
                'B/BL imm26': ((w & 0x7c000000) == 0x14000000, lambda: pc + sx(w & 0x3ffffff, 26) * 4),
                'B.cond imm19': ((w & 0xff000010) == 0x54000000, lambda: pc + sx(w >> 5 & 0x7ffff, 19) * 4),
                'CBZ/CBNZ imm19': ((w & 0x7e000000) == 0x34000000, lambda: pc + sx(w >> 5 & 0x7ffff, 19) * 4),
                'LDR/PRFM literal imm19': ((w & 0x3b000000) == 0x18000000, lambda: pc + sx(w >> 5 & 0x7ffff, 19) * 4),
                'TBZ/TBNZ imm14': ((w & 0x7e000000) == 0x36000000, lambda: pc + sx(w >> 5 & 0x3fff, 14) * 4),
                'ADR': ((w & 0x9f000000) == 0x10000000, lambda: pc + sx((w >> 5 & 0x7ffff) << 2 | (w >> 29 & 3), 21)),
            }
            for k, (m, tf) in kinds.items():
                m = m & ~own
                tgt = tf()
                sel = m & inr(tgt)
                for p_, t_ in zip(pc[sel], tgt[sel]):
                    hits.append((int(t_), f'{name} {k} {int(p_):#x} -> {int(t_):#x}'))
                cnt[k] = cnt.get(k, 0) + int(m.sum())
            # ADRP: any page from which a 12-bit ADD or a scaled (<= 0x7ff8) LDR/STR offset can reach the cave
            m = ((w & 0x9f000000) == 0x90000000) & ~own
            page = (pc & ~0xfff) + sx((w >> 5 & 0x7ffff) << 2 | (w >> 29 & 3), 21) * 4096
            cand = m & (page >= plo) & (page < CAVE_HI)
            cnt['ADRP'] = cnt.get('ADRP', 0) + int(m.sum())
            for idx in np.nonzero(cand)[0]:
                gi, rd, pg = c0 + int(idx), int(w[idx] & 31), int(page[idx])
                for j in range(gi + 1, min(gi + 33, len(allw))):   # follow the register through the next 32 words
                    x = int(allw[j])
                    if x & 0xffc00000 == 0x91000000 and (x >> 5 & 31) == rd:          # add xd, rd, #imm12
                        a = pg + (x >> 10 & 0xfff)
                    elif x & 0x3b000000 == 0x39000000 and (x >> 5 & 31) == rd:        # ldr/str unsigned imm12, scaled
                        a = pg + (x >> 10 & 0xfff) * (1 << (x >> 30))
                    else:
                        continue
                    note = f'  adrp {lo + 4 * gi:#x} x{rd} page {pg:#x} + use {lo + 4 * j:#x} -> {a:#x}'
                    notes.append(note)
                    if inr(np.int64(a)):
                        hits.append((a, f'{name} ADRP{note}'))
            del w, pc, own
        notes.insert(len(notes), f'{name}: ' + ', '.join(f'{k} {v}' for k, v in cnt.items()) +
                     f' decoded (ADRP pages {plo:#x}..{CAVE_HI:#x} followed)')
    # data: relocations (targets and addends), init/fini arrays, aligned qwords in every alloc'd data section
    for name, off, sz in (('.rela.dyn', 0xb9a8a8, 0x26eae8), ('.rela.plt', 0xe09390, 0xc67b0)):
        r = np.frombuffer(buf, dtype='<u8', count=sz // 8, offset=off).reshape(-1, 3).astype(np.uint64)
        roff, add = r[:, 0].astype(np.int64), r[:, 2].astype(np.int64)
        for k, a in (('r_offset', roff), ('r_addend', add)):
            for i in np.nonzero(inr(a))[0]:
                hits.append((int(a[i]), f'{name} entry {int(i)} {k} {int(a[i]):#x} (type {int(r[i, 1] & 0xffffffff)})'))
        notes.append(f'{name}: {len(r)} relocs checked (offset + addend)')
    for name, va, off, sz in (('.init_array', 0x35f9eb0, 0x35f8eb0, 0xd90), ('.fini_array', 0x35fac40, 0x35f9c40, 0x10),
                              ('.data.rel.ro', 0x35fac50, 0x35f9c50, 0xc45c0), ('.got', 0x36bf480, 0x36be480, 0x6cb78),
                              ('.data', 0x372c000, 0x372b000, 0x35178), ('.rodata', 0x2b2e730, 0x2b2e730, 0x71aae0)):
        q = np.frombuffer(buf, dtype='<u8', count=sz // 8, offset=off).astype(np.int64)
        for i in np.nonzero(inr(q))[0]:
            hits.append((int(q[i]), f'{name} qword @va {va + 8 * int(i):#x} = {int(q[i]):#x}'))
        notes.append(f'{name}: {len(q)} aligned qwords checked')
    # dynamic symbols whose value lies in the cave
    ds = np.frombuffer(buf, dtype='<u8', count=0x288300 // 8, offset=0xabcd8).reshape(-1, 3)
    sv = ds[:, 1].astype(np.int64)
    for i in np.nonzero(inr(sv))[0]:
        hits.append((int(sv[i]), f'.dynsym #{int(i)} st_value {int(sv[i]):#x}'))
    notes.append(f'.dynsym: {len(sv)} symbols checked (no .symtab in this .so)')
    # unwind: does any .eh_frame_hdr table entry start inside the cave, or an FDE span cover it?
    hdr = 0x3249210
    n = struct.unpack_from('<I', buf, hdr + 8)[0]
    tab = np.frombuffer(buf, dtype='<i4', count=2 * n, offset=hdr + 12).reshape(-1, 2).astype(np.int64) + hdr
    start = tab[:, 0]
    for i in np.nonzero(inr(start))[0]:
        hits.append((int(start[i]), f'.eh_frame_hdr FDE starts in cave: {int(start[i]):#x}'))
    i = int(np.searchsorted(start, CAVE_LO, side='right')) - 1
    fde = int(tab[i, 1])
    pcb = fde + 8 + struct.unpack_from('<i', buf, fde + 8)[0]                       # pcrel|sdata4 pc_begin
    rng = struct.unpack_from('<i', buf, fde + 12)[0]
    if pcb + rng > CAVE_LO:
        hits.append((pcb + rng - 1, f'FDE {pcb:#x}+{rng:#x} overlaps the cave'))
    notes.append(f'.eh_frame_hdr: {n} FDEs; the one before the cave covers {pcb:#x}..{pcb + rng:#x}')
    return hits, notes


def check_cave_zero(buf):
    assert buf[CAVE_LO:CAVE_HI] == bytes(CAVE_HI - CAVE_LO), 'cave is not all-zero in the stock input'
    assert buf[CAVE_LO - 4:CAVE_LO] != b'\0' * 4 and buf[CAVE_HI:CAVE_HI + 4] != b'\0' * 4, 'cave bounds moved'
    e_phoff, = struct.unpack_from('<Q', buf, 0x20)
    ph = [struct.unpack_from('<IIQQQQQQ', buf, e_phoff + 56 * i) for i in range(struct.unpack_from('<H', buf, 0x38)[0])]
    seg = [p for p in ph if p[0] == 1 and p[3] <= CAVE_LO and CAVE_HI <= p[3] + p[6]]
    assert seg and seg[0][1] == 5 and seg[0][2] == seg[0][3], f'cave not in an R+X LOAD with VA == offset: {seg}'


def refuse_out(path, src):
    p = os.path.realpath(path)
    if os.path.exists(p) and (os.path.samefile(p, src) or os.stat(p).st_nlink > 1):
        sys.exit(f'refusing: {p} is the input or a hardlink')


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('--so', required=True, help='stock libr15.so of Quest %s' % BUILD)
    ap.add_argument('--out', help='patched copy to write')
    ap.add_argument('--dry-run', action='store_true', help='print the patched-byte listing; write nothing')
    ap.add_argument('--verify', metavar='PATCHED', help='re-check a patched libr15.so against the stock input')
    ap.add_argument('--xref-scan', action='store_true', help='prove nothing references the cave')
    ap.add_argument('--layered', action='store_true',
                    help='--so is an already-patched libr15.so (e.g. from a modified APK): skip the whole-file stock hash; the '
                         'tier-2 block, cave and per-range checks still apply, and --verify compares against --so')
    a = ap.parse_args()
    src = os.path.realpath(a.so)
    stock = open(src, 'rb').read()
    sha = hashlib.sha256(stock).hexdigest()
    print(f'input {src}  sha256 {sha}')
    if a.layered:
        print('layered: input is not required to be stock; every patched range is still checked against stock bytes')
    elif not sha.startswith(STOCK_SHA):
        sys.exit(f'refusing: input is not stock Quest {BUILD} libr15.so (sha256 {STOCK_SHA}...)')
    check_cave_zero(stock)
    t, strs, P = patches(stock)
    if a.xref_scan:
        hits, notes = xref_scan(stock)
        print('\n'.join(notes))
        used = (STUB, STUB + len(P[1][3]))
        known = [h for h in hits if h[1] == KNOWN_HIT]
        bad = [h for h in hits if h[1] != KNOWN_HIT or used[0] <= h[0] < used[1]]
        print(f'cave {CAVE_LO:#x}-{CAVE_HI - 1:#x} + dead word {DEAD_B:#x}: {len(hits)} reference(s)')
        print('\n'.join(f'  {"known" if h in known else "BAD"}: {h[1]}' for h in hits))
        print(f'stub range {used[0]:#x}-{used[1] - 1:#x}: ' + ('CLEAR' if not bad else f'{len(bad)} unexpected reference(s)'))
        return 1 if bad else 0
    print(f'targets: SPrintF {t["sprintf"]:#x}  FileExists {t["exists"]:#x}  pages x1 {t["page1"]:#x} x3 {t["page3"]:#x}')
    print(listing(t, strs, P[1][3]))
    if a.verify:
        cur = open(a.verify, 'rb').read()
        if len(cur) != len(stock):
            sys.exit('FAIL: size differs from stock')
        masked = bytearray(cur)
        for n, va, s, p in P:
            if cur[va:va + len(p)] != p:
                sys.exit(f'FAIL: {n} @ {va:#x} is not the expected patch')
            masked[va:va + len(s)] = s
        if bytes(masked) != stock:
            sys.exit('FAIL: bytes outside the patched ranges differ from stock')
        print(f'verify OK: {a.verify}  sha256 {hashlib.sha256(cur).hexdigest()}  site {cur[SITE:SITE + 4].hex(" ")}')
        return 0
    if a.dry_run:
        return 0
    if not a.out:
        sys.exit('--out is required (or --dry-run / --verify / --xref-scan)')
    refuse_out(a.out, src)
    out = bytearray(stock)
    for n, va, s, p in P:
        assert stock[va:va + len(s)] == s and len(s) == len(p)
        out[va:va + len(p)] = p
    os.makedirs(os.path.dirname(os.path.abspath(a.out)), exist_ok=True)
    open(a.out, 'wb').write(out)
    print(f'wrote {a.out}  sha256 {hashlib.sha256(out).hexdigest()}  site {out[SITE:SITE + 4].hex(" ")}')
    return 0


if __name__ == '__main__':
    sys.exit(main())
