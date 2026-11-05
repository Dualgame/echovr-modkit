#!/usr/bin/env python3
"""Read Echo VR resources from YOUR game install, under the reader API the menu tools were written against.

The menu tools call `Basis.named(alias)` and then `row` / `read` / `tree_rel` / `cell` / `find` / `name_of`. This
module answers those calls from the packaged store of an install (`manifests/` + `packages/`), reading the base
package and its language package the way the engine does (on a duplicate row the language package wins). Nothing
is unpacked to disk; decompressed frames are kept in memory
(at most FRAME_BUDGET bytes; the menu's frames are small).

Where the store is:
  EVR_GAME_PC     PC install dir (or its `_data/5932408047/rad15/win10` dir)        alias `pcf` (the tools' default)
  EVR_GAME_QUEST  a local copy of the headset's `.../rad15/android` dir             alias `q`
Both stores are checked against sha256 pins first: the menu is built for one game version, and a language package
that is already modified (by another mod) is refused, because its rows would leak into the build.

Class names map to type symbols through `classes.tsv` beside this file (hashes and class names only).
Third-party dependency: `zstandard` (through `radpkg`).
"""
from __future__ import annotations

import collections
import csv
import hashlib
import os
import re
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
if str(HERE) not in sys.path:
    sys.path.insert(0, str(HERE))
import radpkg
from csymbol64 import hash_name

M = (1 << 64) - 1
_HEX = re.compile(r"^(0x)?[0-9a-fA-F]{1,16}$")
Row = collections.namedtuple("Row", "layer cls type_symbol asset_type fsym size sha256 path")
FRAME_BUDGET = 128 << 20
BASE, LANG = "48037dc70b0ecab2", "2b47aab238f60515"
PLATFORMS = {
    "PC": dict(env="EVR_GAME_PC", key="pc", build="6323983201049540", sub="_data/5932408047/rad15/win10",
               pins={BASE: "e2ae781d32a7fdea89b67a638f33dbab997117c157544b76efa20368eb498fd9",
                     LANG: "8490bb1d6e15178d12c4472b57bc7cd0ad9a60e12f591532897086a116f1356f"}),
    "Quest": dict(env="EVR_GAME_QUEST", key="quest", build="6528386917200980", sub="",
                  pins={BASE: "762c16ae58c9bc70eee071d8c0e2e8fc0408d1f0691032164029294c5cb7ec48",
                        LANG: "26becc4814f88436b522a36318463eff60c15e4337f0912d7f21237b17754ebe"}),
}
ALIASES = {"pcf": "PC", "q": "Quest"}
_NAMES: dict[int, str] = {}


def fsym_of(ident):
    """int | '0x...' | 16 hex digits | name -> 64-bit file symbol."""
    if isinstance(ident, int):
        return ident & M
    s = ident.strip()
    if s.lower().startswith("0x") and _HEX.match(s):
        return int(s, 16) & M
    if len(s) == 16 and _HEX.match(s):
        return int(s, 16)
    f = hash_name(s) & M
    _NAMES.setdefault(f, s)
    return f


def _classes(key: str) -> dict:
    """{(layer, class): type_symbol} and its reverse for one platform."""
    fwd, rev = {}, {}
    with open(HERE / "classes.tsv", encoding="utf-8", newline="") as fh:
        for r in csv.DictReader(fh, delimiter="\t"):
            if r["platform"] == key:
                t = int(r["type_symbol"], 16)
                fwd[(r["layer"], r["class"])] = t
                rev[t] = (r["layer"], r["class"])
    return fwd, rev


class Refused(SystemExit):
    pass


class Basis:
    _cache: dict = {}

    def __init__(self, plat: str):
        spec = PLATFORMS[plat]
        root = os.environ.get(spec["env"])
        if not root:
            raise Refused(f"set {spec['env']} to your Echo VR {plat} store (see build_main_menu_a.py --help)")
        g = Path(root)
        store = next((d for d in (g, g / spec["sub"]) if (d / "manifests" / BASE).is_file()), None)
        if store is None:
            raise Refused(f"{spec['env']}={root}: no manifests/{BASE} there or under {spec['sub'] or '.'}")
        self.plat, self.build, self.dir = plat, spec["build"], str(store)
        self.tree = f"<{plat} install>"
        self._fwd, self._rev = _classes(spec["key"])
        self._store = store
        self._cells = None
        self._m: dict = {}
        self._frames: collections.OrderedDict = collections.OrderedDict()

    @classmethod
    def get(cls, plat):
        if plat not in cls._cache:
            cls._cache[plat] = cls(plat)
        return cls._cache[plat]

    @classmethod
    def pc_final(cls):
        return cls.get("PC")

    @classmethod
    def quest_final(cls):
        return cls.get("Quest")

    @classmethod
    def named(cls, alias):
        if alias in ALIASES:
            return cls.get(ALIASES[alias])
        for plat, spec in PLATFORMS.items():
            if alias == spec["build"]:
                return cls.get(plat)
        raise Refused(f"unknown build {alias!r} (aliases: {', '.join(ALIASES)})")

    def _load(self):
        cells = collections.defaultdict(dict)
        pins = PLATFORMS[self.plat]["pins"]
        for name in (BASE, LANG):
            raw = (self._store / "manifests" / name).read_bytes()
            got = hashlib.sha256(raw).hexdigest()
            if got != pins[name]:
                raise Refused(f"{self.plat} manifests/{name}: sha256 {got[:16]}... is not the stock "
                              f"{pins[name][:16]}... (another game version, or a mod is applied: revert it first)")
            m = radpkg.parse(radpkg.unwrap(raw))
            self._m[name] = m
            ver = {(r[0], r[1]): r[4] for r in m["metadata"]}
            for t, f, frame, off, size, _al in m["contents"]:
                lc = self._rev.get(t)
                if lc is None:
                    continue
                cells[lc][f] = Row(lc[0], lc[1], t, ver[(t, f)], f, size, None, (name, frame, off, size))
        self._cells = dict(cells)

    def cells(self):
        if self._cells is None:
            self._load()
        return self._cells

    def cell(self, layer, cls):
        return self.cells().get((layer.lower(), cls), {})

    def index(self, layer, cls):
        return {f: self.tree_rel(r) for f, r in self.cell(layer, cls).items()}

    def row(self, layer, cls, ident):
        return self.cell(layer, cls).get(fsym_of(ident))

    def path(self, layer, cls, ident):
        r = self.row(layer, cls, ident)
        return self.tree_rel(r) if r else None

    def tree_rel(self, row):
        """`<layer>/<type>/v<asset version>/<file>`: the loose-tree path of the row."""
        return f"{row.layer}/{row.type_symbol:016x}/v{row.asset_type}/{row.fsym:016x}"

    def _frame(self, name, idx):
        m = self._m[name]
        k = (name, idx)
        if k not in self._frames:
            pk, off, csz, ln = m["frames"][idx]
            if ln > radpkg.CAP:
                raise Refused(f"{name} frame {idx} declares {ln} bytes, over the cap")
            comp = radpkg.file_frame_reader(str(self._store / "packages"), name)(pk, off, csz)
            import zstandard
            self._frames[k] = zstandard.ZstdDecompressor().decompress(comp, max_output_size=ln)
            while len(self._frames) > 1 and sum(map(len, self._frames.values())) > FRAME_BUDGET:
                self._frames.popitem(last=False)
        self._frames.move_to_end(k)
        return self._frames[k]

    def read(self, layer, cls, ident):
        r = self.row(layer, cls, ident)
        if r is None:
            return None
        name, frame, off, size = r.path
        return self._frame(name, frame)[off:off + size]

    def name_of(self, sym):
        return _NAMES.get(sym)

    def find(self, text):
        """Rows whose file symbol is `text` (a name or 0x-hex); substring search needs names an install lacks."""
        want = fsym_of(text)
        for cell in self.cells().values():
            r = cell.get(want)
            if r is not None:
                yield r
