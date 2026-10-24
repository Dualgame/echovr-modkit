#!/usr/bin/env python3
"""Quest: make both HTTP API connections take their base URL from the loaded config key `apiservice_host` (the PC key),
falling back to the libr15.so literal at 0x2baa029 (a step of the Quest APK builder). Quest 6528386917200980
vc 4987566, sites found by disassembly; text VA == file offset. Written against STOCK bytes. Layers on top of the
/sdcard stub of quest_libr15_sdcard_config_patch.py (same zero run of .text, a disjoint range).

The API host has no config key in the stock engine: CR15NetGame::InitializeHttp and
CR15NetStoreTransactions::InitializeHttp pass the literal straight to CSysHttp::CreateConnection(u64&, const char*),
which copies it (CString::operator=). Each site's `add x1,x1,#0x29` (the literal's low 12 bits) becomes a branch to a
stub that asks the engine's own getter, as CR15NetGame::Initialize does for configservice_host:
  x1 = CJson::TString(&netgame->config [+0x63240], "apiservice_host", <literal>, 0)   (an empty value -> the literal)
then restores x0 and rejoins at the stock `bl CreateConnection`.
  A  0x128958c  CR15NetGame::InitializeHttp            (x19 = CR15NetGame*; runs right after LoadLocalConfig)
  B  0x126c270  CR15NetStoreTransactions::InitializeHttp (x19 = this, [x19+8] = CR15NetGame*; the env != "live" path
                                                          that builds https://api-%s... is left stock)
Both stubs live at 0x2a75300.. (the /sdcard stub uses 0x2a75200-0x2a75282). Every stock word they rely on is asserted.

  quest_libr15_api_config_patch.py --so <stock libr15.so> --out <p>                  patch a copy of the stock .so
  quest_libr15_api_config_patch.py --so <patched.so> --stock <stock.so> --layered --out <p>   patch an already patched .so
  quest_libr15_api_config_patch.py --so <input> --dry-run | --verify <patched.so> | --xref-scan
Bytes outside the patched ranges must equal the input; the patched ranges must be stock in the stock libr15.so.
Refuses to write over its input or onto a hardlink."""
import argparse, hashlib, os, struct, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import quest_libr15_sdcard_config_patch as sd
from quest_libr15_sdcard_config_patch import (X0, X1, X2, X3, SP, adr, adrp, add_imm, b, bl, cbz_w, dec_adrp, dec_add,
                                              dec_bl, ldr_x, u32)

BUILD = sd.BUILD
X8, X9, X19 = 8, 9, 19
LIT = 0x2baa029                         # "https://api.readyatdawn.com" in stock; modified APKs may rewrite it in place
KEY = b'apiservice_host\0'
# CR15NetStoreTransactions::InitializeHttp() cbz -> b: the store API always takes the (config-driven) literal path,
# never https://api-%s.readyatdawn.com. Release builder step 4.
STORE_API_PATH_PATCHES = {0x126C21C: (bytes.fromhex("68020034"), bytes.fromhex("13000014"))}
RANGE_LO, RANGE_HI = 0x2a75300, 0x2a75410
STUB_A, STUB_B, S_KEY = 0x2a75300, 0x2a75380, 0x2a75400
SITE_A, JOIN_A = 0x128958c, 0x1289590
SITE_B, JOIN_B = 0x126c270, 0x126c274
CFG_OFF = 0x63240                       # CR15NetGame's local-config CJson (LoadLocalConfig decodes into it)
# stock words asserted in the stock libr15.so AND the input (offset: hex)
STOCK = {
    0x1289540: 'f37bbfa9' 'f30300aa',   # InitializeHttp: stp x19,x30,[sp,#-0x10]! ; mov x19,x0
    0x128957c: '080b8952' 'c800a072' '01c900b0' '6002088b' '21a40091' '3c5ff297',
    #            mov w8,#0x4858 ; movk w8,#6,lsl16 ; adrp x1,0x2baa000 ; add x0,x19,x8 ; add x1,x1,#0x29 ; bl 0xf21280
    0x126c1d0: 'f30300aa',              # store InitializeHttp: mov x19, x0
    0x126c1f0: '680640f9',              # ldr x8,[x19,#8]   (CR15NetGame*; +0x63280 is its server-settings CJson)
    0x126c268: 'e1c900d0' '60e20691' '21a40091' '03d4f297',
    #            adrp x1,0x2baa000 ; add x0,x19,#0x1b8 ; add x1,x1,#0x29 ; bl 0xf21280
    0x124d674: '08488652' 'c800a072',   # LoadLocalConfig: mov w8,#0x3240 ; movk w8,#6,lsl16 (the CJson it decodes into)
    0x1286060: '788bf297',              # Initialize: bl 0xf28e40 (CJson::TString PLT), the configservice_host read
}


def movz_w(rd, imm):         return 0x52800000 | imm << 5 | rd
def movk_w16(rd, imm):       return 0x72a00000 | imm << 5 | rd
def add_x(rd, rn, rm):       return 0x8b000000 | rm << 16 | rn << 5 | rd
def mov_x(rd, rm):           return 0xaa0003e0 | rm << 16 | rd
def mov_wzr(rd):             return 0x2a1f03e0 | rd
def ldrb_w(rt, rn):          return 0x39400000 | rn << 5 | rt
def cbz_x(pc, rt, t):        return 0xb4000000 | sd._rel(pc, t, 19) << 5 | rt
def cbnz_w(pc, rt, t):       return 0x35000000 | sd._rel(pc, t, 19) << 5 | rt


def derive(buf, ref):
    for va, hx in STOCK.items():
        want = bytes.fromhex(hx)
        assert ref[va:va + len(want)] == want, f'the stock libr15.so differs from the table at {va:#x}'
        assert buf[va:va + len(want)] == want, f'input is not stock at {va:#x} (another patch here?)'
    r, page = dec_adrp(u32(buf, 0x1289584), 0x1289584)
    assert (r, page) == (X1, LIT & ~0xfff) and dec_add(u32(buf, SITE_A)) == (X1, X1, LIT & 0xfff)
    assert dec_adrp(u32(buf, 0x126c268), 0x126c268) == (X1, LIT & ~0xfff) and dec_add(u32(buf, SITE_B)) == (X1, X1, LIT & 0xfff)
    conn = dec_bl(u32(buf, JOIN_A), JOIN_A)
    assert conn == dec_bl(u32(buf, 0x126c274), 0x126c274) == 0xf21280, hex(conn)       # CreateConnection(u64&, const char*)
    tstr = dec_bl(u32(buf, 0x1286060), 0x1286060)
    assert tstr == 0xf28e40, hex(tstr)                                                  # CJson::TString(path, def, flags) const
    assert buf[0x2baec8f:0x2baec8f + 19] == b'configservice_host\0'                     # the getter's stock key, as a check
    end = buf.index(b'\0', LIT)
    assert 8 <= end - LIT < 0x40 and buf[LIT:LIT + 4] == b'http', buf[LIT:end + 1]
    assert buf[RANGE_LO:RANGE_HI] == bytes(RANGE_HI - RANGE_LO), 'stub range is not zero in the input'
    assert ref[RANGE_LO:RANGE_HI] == bytes(RANGE_HI - RANGE_LO)
    assert sd.CAVE_LO + 0x10 <= RANGE_LO and RANGE_HI <= sd.CAVE_HI and RANGE_HI <= sd.DEAD_B + 0x1000
    assert RANGE_LO >= sd.STUB + 0x83                                                   # clear of the /sdcard stub
    return dict(tstring=tstr, conn=conn, literal=buf[LIT:end].decode('latin1'))


def tail(pc0, n0, ctx_join):
    """cbz x0,DEF ; ldrb w9,[x0] ; cbnz w9,J ; DEF: adrp x0,page ; add x0,x0,#lo ; J: mov x1,x0  (6 words from n0)"""
    pc = lambda i: pc0 + 4 * i
    d, j = n0 + 3, n0 + 5
    return [cbz_x(pc(n0), X0, pc(d)), ldrb_w(X9, X0), cbnz_w(pc(n0 + 2), X9, pc(j)),
            adrp(pc(d), X0, LIT & ~0xfff), add_imm(X0, X0, LIT & 0xfff), mov_x(X1, X0)]


def build(t):
    pa = lambda i: STUB_A + 4 * i
    a = [add_imm(X2, X1, LIT & 0xfff),                         # x2 = the literal (default)
         movz_w(X8, CFG_OFF & 0xffff), movk_w16(X8, CFG_OFF >> 16), add_x(X0, X19, X8),   # x0 = &netgame->config
         adr(pa(4), X1, S_KEY), mov_wzr(X3), bl(pa(6), t['tstring'])]
    a += tail(STUB_A, 7, None)
    a += [movz_w(X8, 0x4858), movk_w16(X8, 6), add_x(X0, X19, X8), b(pa(16), JOIN_A)]   # x0 = this+0x64858, as stock
    pb = lambda i: STUB_B + 4 * i
    bb = [add_imm(X2, X1, LIT & 0xfff), ldr_x(X8, X19, 8),
          movz_w(X9, CFG_OFF & 0xffff), movk_w16(X9, CFG_OFF >> 16), add_x(X0, X8, X9),
          adr(pb(5), X1, S_KEY), mov_wzr(X3), bl(pb(7), t['tstring'])]
    bb += tail(STUB_B, 8, None)
    bb += [add_imm(X0, X19, 0x1b8), b(pb(15), JOIN_B)]
    assert len(a) == 17 and len(bb) == 16 and STUB_A + 4 * len(a) <= STUB_B and STUB_B + 4 * len(bb) <= S_KEY
    blob = bytearray(RANGE_HI - RANGE_LO)
    blob[0:4 * len(a)] = struct.pack('<%dI' % len(a), *a)
    blob[STUB_B - RANGE_LO:STUB_B - RANGE_LO + 4 * len(bb)] = struct.pack('<%dI' % len(bb), *bb)
    blob[S_KEY - RANGE_LO:S_KEY - RANGE_LO + len(KEY)] = KEY
    return a, bb, bytes(blob)


def patches(buf, ref):
    t = derive(buf, ref)
    a, bb, blob = build(t)
    return t, a, bb, [('site A', SITE_A, buf[SITE_A:SITE_A + 4], struct.pack('<I', b(SITE_A, STUB_A))),
                      ('site B', SITE_B, buf[SITE_B:SITE_B + 4], struct.pack('<I', b(SITE_B, STUB_B))),
                      ('stubs', RANGE_LO, buf[RANGE_LO:RANGE_HI], blob)]


EXPECT_A = ['add x2, x1, #0x29', 'mov w8, #0x3240', 'movk w8, #6, lsl #16', 'add x0, x19, x8', 'adr x1, #0x2a75400',
            'mov w3, wzr', 'bl #0xf28e40', 'cbz x0, #0x2a75328', 'ldrb w9, [x0]', 'cbnz w9, #0x2a75330',
            'adrp x0, #0x2baa000', 'add x0, x0, #0x29', 'mov x1, x0', 'mov w8, #0x4858', 'movk w8, #6, lsl #16',
            'add x0, x19, x8', 'b #0x1289590']
EXPECT_B = ['add x2, x1, #0x29', 'ldr x8, [x19, #8]', 'mov w9, #0x3240', 'movk w9, #6, lsl #16', 'add x0, x8, x9',
            'adr x1, #0x2a75400', 'mov w3, wzr', 'bl #0xf28e40', 'cbz x0, #0x2a753ac', 'ldrb w9, [x0]',
            'cbnz w9, #0x2a753b4', 'adrp x0, #0x2baa000', 'add x0, x0, #0x29', 'mov x1, x0', 'add x0, x19, #0x1b8',
            'b #0x126c274']


def listing(a, bb):
    out = []
    try:
        import capstone
        md = capstone.Cs(capstone.CS_ARCH_ARM64, capstone.CS_MODE_ARM)
        dis = lambda words, va: [f'{i.mnemonic} {i.op_str}' for i in md.disasm(struct.pack('<%dI' % len(words), *words), va)]
    except ImportError:
        dis = None
    for name, site, stub, words, exp in (('A', SITE_A, STUB_A, a, EXPECT_A), ('B', SITE_B, STUB_B, bb, EXPECT_B)):
        sw = b(site, stub)
        out.append(f'  {site:#09x}  {struct.pack("<I", sw).hex(" ")}   b {stub:#x}   (was add x1, x1, #0x29)  site {name}')
        got = dis(words, stub) if dis else [''] * len(words)
        for i, w in enumerate(words):
            out.append(f'  {stub + 4 * i:#09x}  {struct.pack("<I", w).hex(" ")}   {got[i]}')
        if dis:
            assert dis([sw], site) == [f'b #{stub:#x}'], dis([sw], site)
            bad = [(i, g, e) for i, (g, e) in enumerate(zip(got, exp)) if g != e]
            assert len(got) == len(exp) and not bad, f'capstone disagrees with the encoder (stub {name}): {bad}'
    out.append(f'  {S_KEY:#09x}  "{KEY[:-1].decode()}\\0" ({len(KEY)} B)')
    out.append(f'  capstone round-trip: OK ({len(a)} + {len(bb)} + 2 sites)' if dis else '  capstone not importable')
    return '\n'.join(out)


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('--so', required=True, help='input libr15.so of Quest %s (stock; or already patched with --layered)' % BUILD)
    ap.add_argument('--stock', help='the stock libr15.so, the reference for the stock-bytes checks (default: --so; required with --layered)')
    ap.add_argument('--out', help='patched copy to write')
    ap.add_argument('--dry-run', action='store_true')
    ap.add_argument('--verify', metavar='PATCHED', help='re-check a patched .so against the input')
    ap.add_argument('--xref-scan', action='store_true', help='nothing in the stock image may reference the cave')
    ap.add_argument('--layered', action='store_true', help='--so is already patched (e.g. from a modified APK); skip its whole-file hash; needs --stock')
    a = ap.parse_args()
    if a.layered and not a.stock:
        sys.exit('--layered needs --stock (the stock libr15.so to compare against)')
    src = os.path.realpath(a.so)
    buf = open(src, 'rb').read()
    ref = open(a.stock, 'rb').read() if a.stock else buf
    if not hashlib.sha256(ref).hexdigest().startswith(sd.STOCK_SHA):
        sys.exit('refusing: the stock libr15.so is not Quest %s (sha256 %s...)' % (BUILD, sd.STOCK_SHA))
    sha = hashlib.sha256(buf).hexdigest()
    print(f'input {src}  sha256 {sha}')
    if not a.layered and not sha.startswith(sd.STOCK_SHA):
        sys.exit('refusing: input is not stock (pass --layered and --stock for an already patched .so)')
    if len(buf) != len(ref):
        sys.exit('refusing: input size differs from the stock libr15.so')
    if a.xref_scan:
        hits, notes = sd.xref_scan(ref)
        print('\n'.join(notes))
        bad = [h for h in hits if h[1] != sd.KNOWN_HIT or RANGE_LO <= h[0] < RANGE_HI]
        print(f'stub range {RANGE_LO:#x}-{RANGE_HI - 1:#x}: ' + ('CLEAR' if not bad else f'{len(bad)} reference(s)'))
        for h in hits:
            print(f'  {"BAD" if h in bad else "known"}: {h[1]}')
        return 1 if bad else 0
    t, aw, bw, P = patches(buf, ref)
    print(f'targets: CJson::TString PLT {t["tstring"]:#x}  CreateConnection PLT {t["conn"]:#x}  '
          f'literal (fallback) "{t["literal"]}"  key "{KEY[:-1].decode()}"')
    print(listing(aw, bw))
    if a.verify:
        cur = open(a.verify, 'rb').read()
        if len(cur) != len(buf):
            sys.exit('FAIL: size differs')
        masked = bytearray(cur)
        for n, va, s, p in P:
            if cur[va:va + len(p)] != p:
                sys.exit(f'FAIL: {n} @ {va:#x} is not the expected patch')
            masked[va:va + len(s)] = s
        if bytes(masked) != buf:
            sys.exit('FAIL: bytes outside the patched ranges differ from the input')
        print(f'verify OK: {a.verify}  sha256 {hashlib.sha256(cur).hexdigest()}')
        return 0
    if a.dry_run or not a.out:
        return 0
    sd.refuse_out(a.out, src)
    out = bytearray(buf)
    for n, va, s, p in P:
        assert out[va:va + len(s)] == s
        out[va:va + len(p)] = p
    tmp = a.out + '.tmp'
    with open(tmp, 'wb') as f:
        f.write(out)
    os.replace(tmp, a.out)
    print(f'wrote {a.out}  sha256 {hashlib.sha256(out).hexdigest()}')
    return 0


if __name__ == '__main__':
    sys.exit(main())
