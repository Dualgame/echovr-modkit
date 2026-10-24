#!/usr/bin/env python3
"""PC retail: restore the client half of the early-quit lockout in a COPY of echovr.exe (PC 6323983201049540,
sites found by disassembly).

RAD stubbed it in the shipped build: the FeatureFlags handler keeps `byte & 0xdd` (UI bits 1 and 5 never on), the
penalty setter drops the Update's second long and stores -1 as lockoutexpiretimestamp, and the penalty expression
writes lockoutcountdownactive = 0. Four sites, 23 bytes, no code cave:
  a  0x1401618b7  and dl,0xdd                  -> and dl,0xff        FeatureFlags keeps every bit
  b1 0x1401ae2f0  cmp rax,-1; je +9            -> mov [rsp],r8; nop  keep the 2nd long (setter home slot)
  b2 0x1401ae372  mov qword [rcx+0x64828],-1   -> push [rsp]; pop [rcx+0x64828]; nop   expiry = Update +0x20
  c  0x140d96319  mov byte [rbx],0             -> setg byte [rbx]    active = expiry != -1 && expiry > now
  pc_exe_earlyquit_patch.py --exe <copy>/echovr.exe --stock <stock>/echovr.exe            apply
  pc_exe_earlyquit_patch.py --exe <copy>/echovr.exe --stock <stock>/echovr.exe --revert   back to stock bytes
  pc_exe_earlyquit_patch.py --exe <copy>/echovr.exe --stock <stock>/echovr.exe --status
Optional test aid, applied only with --session-api, reverted by every --revert:
  s  0x140163db9  cmovne rbx,rcx               -> nop dword [rax]    GET /session (6721) is no longer refused with
                                                                    -6 "restricted in this match type" (social lobby),
                                                                    so a test can read the player's head pose there
Every byte outside the sites must equal the stock exe. Refuses hardlinks, a path under --protect DIR, the stock file itself and anything that looks like a live install
(see looks_like_install; --i-know-this-is-a-copy overrides). The write is atomic (temp file + os.replace)."""
import argparse, os, sys

SITES = [  # (name, file offset, stock bytes, patched bytes)
    ('a', 0x160cb7, bytes.fromhex('80e2dd'), bytes.fromhex('80e2ff')),
    ('b1', 0x1ad6f0, bytes.fromhex('4883f8ff7409'), bytes.fromhex('4c8904246690')),
    ('b2', 0x1ad772, bytes.fromhex('48c78128480600ffffffff'), bytes.fromhex('ff34248f812848060066 90'.replace(' ', ''))),
    ('c', 0xd95719, bytes.fromhex('c60300'), bytes.fromhex('0f9f03')),
]
EXTRA = [('s', 0x1631b9, bytes.fromhex('480f45d9'), bytes.fromhex('0f1f4000'))]   # --session-api only


def state(cur: bytes) -> str:
    on = [n for n, o, s, p in SITES if cur[o:o + len(p)] == p]
    off = [n for n, o, s, p in SITES if cur[o:o + len(s)] == s]
    if len(on) == len(SITES):
        return 'patched'
    if len(off) == len(SITES):
        return 'stock'
    return f'mixed (patched {on}, stock {off})'


def looks_like_install(exe: str) -> str | None:
    """Why `exe` looks like a live install, or None: a Program Files / OculusClient path component, or a `_data` /
    `manifests` / `OculusClient` entry beside the exe or in any of the 3 directories above it."""
    parts = [p.lower() for p in os.path.normpath(exe).replace('\\', '/').split('/')]
    for bad in ('program files', 'program files (x86)', 'oculusclient'):
        if bad in parts[:-1]:
            return f'its path contains `{bad}`'
    d = os.path.dirname(exe)
    for _ in range(4):
        try:
            names = {n.lower() for n in os.listdir(d)}
        except OSError:
            names = set()
        hit = names & {'_data', 'manifests', 'oculusclient'}
        if hit:
            return f'{d} holds `{sorted(hit)[0]}` (a game install keeps its packages there)'
        if os.path.dirname(d) == d:
            break
        d = os.path.dirname(d)
    return None


def write_atomic(path: str, data: bytes) -> None:
    tmp = f'{path}.tmp{os.getpid()}'
    try:
        with open(tmp, 'wb') as f:
            f.write(data)
        os.chmod(tmp, os.stat(path).st_mode & 0o7777)
        os.replace(tmp, path)
    finally:
        if os.path.exists(tmp):
            os.remove(tmp)


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('--exe', dest='target_path', required=True, help='the COPY of echovr.exe to patch / revert / inspect')
    ap.add_argument('--stock', required=True, help='the stock echovr.exe of PC 6323983201049540 (the reference copy)')
    g = ap.add_mutually_exclusive_group()
    g.add_argument('--revert', action='store_true')
    g.add_argument('--status', action='store_true')
    ap.add_argument('--protect', action='append', metavar='DIR', help='refuse to write anywhere under DIR (repeatable)')
    ap.add_argument('--i-know-this-is-a-copy', action='store_true',
                    help='skip the live-install check (a test copy whose folder keeps _data / manifests beside the exe)')
    ap.add_argument('--session-api', action='store_true', help='also site s: /session answers in the social lobby')
    a = ap.parse_args()
    target = os.path.abspath(a.target_path)
    for d in a.protect or []:
        if os.path.commonpath([target, os.path.abspath(d)]) == os.path.abspath(d):
            sys.exit(f'refusing: {target} is under the protected directory {d}')
    why = None if a.i_know_this_is_a_copy else looks_like_install(target)
    if why:
        sys.exit(f'refusing: {target} looks like a live install: {why}. Patch a copy in its own folder, or pass '
                 '--i-know-this-is-a-copy')
    if os.path.exists(a.stock) and os.path.samefile(target, a.stock):
        sys.exit('refusing: --exe is the --stock file; patch a copy')
    if os.stat(target).st_nlink > 1:
        sys.exit(f'refusing: {target} is a hardlink')
    ref = open(a.stock, 'rb').read()
    cur = bytearray(open(target, 'rb').read())
    for n, o, s, p in SITES + EXTRA:
        assert ref[o:o + len(s)] == s and len(s) == len(p), f'site {n}: stock bytes differ from the table'
    masked = bytearray(cur)
    for n, o, s, p in SITES + EXTRA:
        masked[o:o + len(s)] = s
    if len(cur) != len(ref) or bytes(masked) != ref:
        sys.exit('refusing: exe differs from the stock exe outside the early-quit / session-api sites')
    extra = lambda c: ''.join(n for n, o, s, p in EXTRA if c[o:o + len(p)] == p)
    print(f'before: {state(cur)}' + (f' + {extra(cur)}' if extra(cur) else ''))
    if a.status:
        return 0
    for n, o, s, p in SITES + (EXTRA if a.session_api or a.revert else []):
        cur[o:o + len(s)] = s if a.revert else p
    write_atomic(target, bytes(cur))
    print(f'after:  {state(cur)}' + (f' + {extra(cur)}' if extra(cur) else '') + ('  (= stock)' if bytes(cur) == ref else ''))
    return 0


if __name__ == '__main__':
    sys.exit(main())
