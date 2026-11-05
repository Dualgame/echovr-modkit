"""RAD CSymbol64 hash — Python port of echopack-main/pkg/echo/symbol64.go.

CRC64 "jones" variant, polynomial 0x95AC9329AC4BC9B5, init value int64(-1),
input normalized to lowercase. Output is a 64-bit unsigned value (mathematical
two's-complement equivalent of the Go int64).
"""

from __future__ import annotations

import argparse
import sys

_POLY = 0x95AC9329AC4BC9B5
_MASK64 = (1 << 64) - 1
_seeds: list[int] | None = None


def _init_seeds() -> list[int]:
    seeds = [0] * 256
    for i in range(256):
        value = 0x2B5926535897936A if (i & 0x80) else 0
        if i & 0x40:
            value ^= _POLY
        shift = 0x20
        while shift != 0:
            value = (2 * value) & _MASK64
            if i & shift:
                value ^= _POLY
            shift >>= 1
        seeds[i] = (2 * value) & _MASK64
    return seeds


def calculate(text: str, base: int = -1) -> int:
    global _seeds
    if _seeds is None:
        _seeds = _init_seeds()
    result = base & _MASK64
    for ch in text.encode("ascii", errors="strict"):
        if 0x41 <= ch <= 0x5A:
            ch += 32
        result = (((result << 8) & _MASK64) ^ _seeds[(result >> 56) & 0xFF] ^ ch) & _MASK64
    return result


def hash_name(name: str) -> int:
    return calculate(name, base=-1)


_KNOWN = {
    "mpl_arena_a": 0x576ED3F8428EBC4B,
    "mpl_arena_a_lowspec": 0x7691D78D414C5701,
    "mpl_combat_fission": 0xDF5CA7B7DFA383D4,
    "mpl_combat_gauss": 0x43E2DA7A0C623A19,
    "mpl_combat_pebbles": 0xDF4AABA6CEA689C9,
    "mpl_combat_combustion": 0x42670F2BED45703C,
    "mpl_combat_dyson": 0x43E2DA7914642604,
    "ncasound": 0x38EE950426EA8A62,
    "ncamodel": 0x38EE951A26FB816A,
    "ncatransform": None,
}


def self_test() -> bool:
    ok = True
    for name, expected in _KNOWN.items():
        if expected is None:
            continue
        got = hash_name(name)
        tag = "ok " if got == expected else "FAIL"
        if got != expected:
            ok = False
        print(f"  {tag}  {name:32s}  got=0x{got:016x}  expected=0x{expected:016x}")
    return ok


def main() -> int:
    parser = argparse.ArgumentParser(description="Compute CSymbol64 hashes")
    parser.add_argument("names", nargs="*", help="Names to hash (lowercased)")
    parser.add_argument("--test", action="store_true", help="Run self-test")
    args = parser.parse_args()

    if args.test or not args.names:
        ok = self_test()
        if not ok:
            return 1
        if not args.names:
            return 0

    for name in args.names:
        h = hash_name(name)
        print(f"0x{h:016x}\t{name}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
