#!/usr/bin/env python3
"""Unlock the ECHO COMBAT card on the CREATE GAME page of the Quest build (vc 4987566): one instruction in the
APK library lib/arm64-v8a/libcab86dd5921bafc7.so, the compiled page script.

The page draws the combat card locked through a single `csel`; replacing it with `mov x0, x10` (ELF offset 0x12fcc,
bytes 20 11 8a 9a -> e0 03 0a aa) unlocks that card only. This is a pure function on the library bytes: it checks the
sha256 of its input against the stock library, checks the site, patches 4 bytes and checks the sha256 of its output.
It reads, writes and signs nothing; the caller owns the APK.

  patch(lib_bytes) -> bytes      stock in, patched out; an already patched library comes back unchanged
  state(lib_bytes) -> 'stock' | 'patched'      raises PatchError for any other library
  python3 quest_libcab86_combat_unlock_patch.py LIB.so [OUT.so]     print the state; with OUT, write the patched copy

Unlocking the card does not make a combat match start: the Quest build ships no combat level.
"""
import hashlib
import sys

LIB_ENTRY = 'lib/arm64-v8a/libcab86dd5921bafc7.so'              # the APK member
LIB_STOCK_SHA = '14342259ad249a7176de5f7c3e9ed1bfd28d1d0d05d88b56a5b6ac8036fc0f5b'    # Quest 6528386917200980
LIB_PATCHED_SHA = 'c4779361ade8be7bdf02f1fab34dba37973ad021e6e2544dd2e6989158e8087c'
SITE, STOCK, PATCH = 0x12fcc, bytes.fromhex('20118a9a'), bytes.fromhex('e0030aaa')   # csel -> mov x0, x10


class PatchError(ValueError):
    """The library is not the pinned stock library (or its patched form), or the site holds other bytes."""


def sha(b):
    return hashlib.sha256(b).hexdigest()


def _pins(stock_sha, patched_sha):
    return (stock_sha or LIB_STOCK_SHA).lower(), (patched_sha or LIB_PATCHED_SHA).lower()


def state(lib, *, stock_sha=None, patched_sha=None, site=SITE):
    """'stock' or 'patched'. The patched form is recognised by restoring the site and matching the stock sha256, so
    a library with any other difference is refused. `stock_sha`, `patched_sha` and `site` are for tests."""
    stock_sha, patched_sha = _pins(stock_sha, patched_sha)
    lib = bytes(lib)
    if len(lib) < site + 4:
        raise PatchError(f'library is {len(lib)} bytes, too short for the site at {site:#x}')
    here = lib[site:site + 4]
    if here == STOCK:
        if sha(lib) != stock_sha:
            raise PatchError(f'not the stock library: sha256 {sha(lib)}, pinned {stock_sha}')
        return 'stock'
    if here == PATCH:
        if sha(lib) == patched_sha:
            return 'patched'
        if sha(lib[:site] + STOCK + lib[site + 4:]) == stock_sha:
            return 'patched'
        raise PatchError(f'site {site:#x} holds the patch but the rest is not the stock library (sha256 {sha(lib)})')
    raise PatchError(f'site {site:#x} holds {here.hex()}, neither the stock {STOCK.hex()} nor the patch {PATCH.hex()}')


def patch(lib, *, stock_sha=None, patched_sha=None, site=SITE):
    """The library with the combat card unlocked. Idempotent: a patched library is returned as it is."""
    stock_sha, patched_sha = _pins(stock_sha, patched_sha)
    lib = bytes(lib)
    if state(lib, stock_sha=stock_sha, patched_sha=patched_sha, site=site) == 'patched':
        return lib
    out = lib[:site] + PATCH + lib[site + 4:]
    if sha(out) != patched_sha:
        raise PatchError(f'output sha256 {sha(out)}, pinned {patched_sha}')
    return out


def main(argv):
    if not argv or len(argv) > 2 or argv[0] in ('-h', '--help'):
        print(__doc__)
        return 0 if argv else 2
    lib = open(argv[0], 'rb').read()
    try:
        st = state(lib)
        out = patch(lib)
    except PatchError as e:
        sys.exit(f'refusing: {argv[0]}: {e}')
    print(f'{argv[0]}: {st}, sha256 {sha(lib)} -> {sha(out)}')
    if len(argv) == 2:
        if argv[1] == argv[0]:
            sys.exit('refusing: OUT must differ from LIB')
        open(argv[1], 'wb').write(out)
        print(f'wrote {argv[1]}')
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
