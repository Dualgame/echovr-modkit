#!/usr/bin/env python3
"""Quest: restore the client half of the early-quit lockout in a COPY of libr15.so (Quest
6528386917200980 vc 4987566, sites found by disassembly; the PC twin is pc_exe_earlyquit_patch.py).

Same stub as PC, same netgame offsets (+0x64847 flags, +0x64820 / +0x64828 penalty long / expiry). On Quest the
penalty setter is inlined into EarlyQuitUpdateCB 0x126ac50, so b1/b2 patch the handler, not SetEarlyQuitPenaltyLevel
0x126ad54 (which has no static caller). Six sites, 24 bytes, fixed-width in place, no code cave:
  a  0x126ac3c  and w8,w8,#0xdddddddd  -> and w8,w8,#0xff            FeatureFlagsCB keeps every bit
  b1 0x126ac70  ldr x0,[x10,#0x18]     -> ldp x0,x3,[x10,#0x18]      also load the Update's 2nd long (+0x20); x3 dead
  b2 0x126acb0  mov x2,#-1             -> mov x2,x3                  stp x0,x2,[x13] stores expiry = Update +0x20
  c1 0x2321ce0  mov x9,#-1             -> ccmn x22,#1,#4,gt          same flags as the stock ccmp x22,-1 (NE = active)
  c2 0x2321ce4  ccmp x22,x9,#4,gt      -> cset w9,ne                 w9 = expiry != -1 && expiry > now (signed)
  c3 0x2321cf0  strb wzr,[x19]         -> strb w9,[x19]              lockoutcountdownactive (PenaltyExpression 0x2321ba0)
The csel of lockoutcountdownsec at 0x2321cec still reads the same NE flags. Text VA == file offset in this .so.
  quest_libr15_earlyquit_patch.py --so <copy>/libr15.so --stock <stock>/libr15.so            apply
  quest_libr15_earlyquit_patch.py --so <copy>/libr15.so --stock <stock>/libr15.so --revert   back to stock bytes
  quest_libr15_earlyquit_patch.py --so <copy>/libr15.so --stock <stock>/libr15.so --status
  ... --ref <other libr15.so>   compare against an already-patched base (e.g. from a modified APK) instead
Every byte outside the six sites must equal the reference (default: the stock libr15.so); the reference's
site bytes must be the stock bytes. Refuses hardlinks, a path under --protect DIR and the stock file itself."""
import argparse, os, sys

SITES = [  # (name, file offset, stock bytes, patched bytes)
    ('a', 0x126ac3c, bytes.fromhex('08e90212'), bytes.fromhex('081d0012')),
    ('b1', 0x126ac70, bytes.fromhex('400d40f9'), bytes.fromhex('408d41a9')),
    ('b2', 0x126acb0, bytes.fromhex('02008092'), bytes.fromhex('e20303aa')),
    ('c1', 0x2321ce0, bytes.fromhex('09008092'), bytes.fromhex('c4ca41ba')),
    ('c2', 0x2321ce4, bytes.fromhex('c4c249fa'), bytes.fromhex('e9079f1a')),
    ('c3', 0x2321cf0, bytes.fromhex('7f020039'), bytes.fromhex('69020039')),
]


def state(cur: bytes) -> str:
    on = [n for n, o, s, p in SITES if cur[o:o + len(p)] == p]
    off = [n for n, o, s, p in SITES if cur[o:o + len(s)] == s]
    if len(on) == len(SITES):
        return 'patched'
    if len(off) == len(SITES):
        return 'stock'
    return f'mixed (patched {on}, stock {off})'


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('--so', required=True, help='a writable COPY of libr15.so')
    ap.add_argument('--stock', required=True, help='the stock libr15.so of Quest 6528386917200980 (site bytes are checked against it)')
    ap.add_argument('--ref', help='reference libr15.so for the outside-sites check (default: --stock)')
    ap.add_argument('--protect', action='append', metavar='DIR', help='refuse to write anywhere under DIR (repeatable)')
    g = ap.add_mutually_exclusive_group()
    g.add_argument('--revert', action='store_true')
    g.add_argument('--status', action='store_true')
    a = ap.parse_args()
    so = os.path.abspath(a.so)
    for d in a.protect or []:
        if os.path.commonpath([so, os.path.abspath(d)]) == os.path.abspath(d):
            sys.exit(f'refusing: {so} is under the protected directory {d}')
    if os.path.exists(a.stock) and os.path.samefile(so, a.stock):
        sys.exit('refusing: --so is the --stock file; patch a copy')
    if os.stat(so).st_nlink > 1:
        sys.exit(f'refusing: {so} is a hardlink')
    basis = open(a.stock, 'rb').read()
    ref = open(a.ref, 'rb').read() if a.ref else basis
    cur = bytearray(open(so, 'rb').read())
    for n, o, s, p in SITES:
        assert basis[o:o + len(s)] == s and len(s) == len(p), f'site {n}: stock bytes differ from the table'
        if ref[o:o + len(s)] != s:
            sys.exit(f'refusing: site {n} in the reference is not the stock bytes')
    masked = bytearray(cur)
    for n, o, s, p in SITES:
        masked[o:o + len(s)] = s
    if len(cur) != len(ref) or bytes(masked) != ref:
        sys.exit('refusing: libr15.so differs from the reference outside the six early-quit sites')
    print(f'before: {state(cur)}')
    if a.status:
        return 0
    for n, o, s, p in SITES:
        cur[o:o + len(s)] = s if a.revert else p
    open(so, 'wb').write(cur)
    print(f'after:  {state(cur)}' + ('  (= reference)' if bytes(cur) == ref else ''))
    return 0


if __name__ == '__main__':
    sys.exit(main())
