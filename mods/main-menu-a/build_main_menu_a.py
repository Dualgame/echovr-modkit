#!/usr/bin/env python3
"""Main Menu A: build the redesigned Echo VR main menu from YOUR game install.

The menu is a set of edits (poses, labels, page routes, minted buttons, canvas edits) applied to the menu levels
`mnu_master` and `mnu_master_mp_ingame`. Nothing from the game ships with this mod: every edited resource is read
out of your install at build time, and the result is checked against the sha256 list in `expected_pc.tsv`.

PC (game closed; needs bash, python3 and `zstandard`):
  build_main_menu_a.py pc --game "C:/Program Files/Oculus/Software/Software/ready-at-dawn-echo-arena" --out TREE
      TREE = a loose resource tree (`primary/<type>/v<version>/<file>`), the input of `pc-package` below
      (no loader or extra flag is involved). The output is checked against expected_pc.tsv (sha256 per file).
  build_main_menu_a.py pc-package --game "<PC install>" --pc-tree TREE --out POUT   [--dry-run: write nothing]
      POUT/menu   manifests/ + packages/ : a copy of your language package with one row per tree file
      POUT/stock  your stock language pair, for revert
  build_main_menu_a.py pc-apply  --game "<PC install>" --out POUT    copy POUT/menu into the install
  build_main_menu_a.py pc-revert --game "<PC install>" --out POUT    copy POUT/stock back (stock again)

Check only (seconds; writes nothing): is this the install the build expects?
  build_main_menu_a.py check --game "<PC install>" [--quest-store QDIR]
      verifies the sha256 pins of the base and language manifests (a language package already modified by another mod,
      such as menu-cards, is refused: revert it first) and reports how many resources the build will read and write.

Quest (optional; QDIR = your stock Quest data folder, the one holding `manifests/` and `packages/` (the whole base
package `48037dc70b0ecab2` is read, plus the language pair `2b47aab238f60515`)):
  build_main_menu_a.py quest --pc-tree TREE --quest-store QDIR --out QOUT
      QOUT/quest_tree  the PC tree moved to the Quest's `_lowspec` levels and Android type dirs (the social card's
                       and the tablet REPORT PLAYER layer's global-level files rebuilt in the Quest format)
      QOUT/menu        manifests/ + packages/ : a copy of the language package with one row per tree file;
                       push both files over the headset's own (keep the stock pair to revert)

The rad-menu and rad-links plugins drive the menu's live parts (friends rows, invite code, tab state); build them
from their own folders.
"""
from __future__ import annotations

import argparse
import hashlib
import os
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

HERE = Path(__file__).resolve().parent
LIB = HERE / "lib"
sys.path[:0] = [str(LIB), str(LIB / "menu_ui")]
QUEST_PACKAGE_SHA = "6c2473458173571837679cb2804702198ec931e1b4f32dd39cd404c9d8745e42"
QUEST_MANIFEST_PAYLOAD_SHA = "1b8cac1522f01a2dcda9fcc00c1d2d8110fdbd9c18315829423eec5453036546"


def _sha(p: Path) -> str:
    return hashlib.sha256(p.read_bytes()).hexdigest()


def _tree(root: Path) -> dict:
    return {p.relative_to(root).as_posix(): p for p in sorted(root.rglob("*")) if p.is_file()}


def verify(tree: Path, expected: Path) -> int:
    want = dict(ln.split("\t") for ln in expected.read_text().splitlines()[1:])
    got = _tree(tree)
    bad = [f for f in want if f not in got or _sha(got[f]) != want[f]] + [f for f in got if f not in want]
    print(f"verify: {len(want) - len([f for f in bad if f in want])}/{len(want)} files match {expected.name}"
          + (f"; differ / extra: {bad}" if bad else ""))
    return 1 if bad else 0


def check(a) -> int:
    state = _pc_state(_pc_store(a.game), None)
    if state != "stock":
        print("PC language package: " + ("Main Menu A is applied (pc-revert goes back to stock; the builds need "
                                          "the stock pair)" if state == "menu" else
                                          "modified by something else (revert that mod, or repair the install)"))
        return 0 if state == "menu" else 1
    print("PC language package: stock")
    os.environ["EVR_GAME_PC"] = str(Path(a.game).resolve())
    import game_store
    cells = game_store.Basis.named("pcf").cells()
    n = sum(len(v) for v in cells.values())
    want = len((HERE / "expected_pc.tsv").read_text().splitlines()) - 1
    print(f"PC install OK: manifests match the expected build; {n} indexed resources in {len(cells)} classes; "
          f"the pc build will write {want} files (nothing was written now)")
    if a.quest_store:
        os.environ["EVR_GAME_QUEST"] = str(Path(a.quest_store).resolve())
        qn = sum(len(v) for v in game_store.Basis.named("q").cells().values())
        print(f"Quest store OK: manifests match the expected build; {qn} indexed resources")
    return 0


def pc(a) -> int:
    out = Path(a.out).resolve()
    game = Path(a.game).resolve()
    if out.exists():
        sys.exit(f"refusing: {out} exists")
    try:
        out.relative_to(game)
        sys.exit("refusing: --out must be outside the game install")
    except ValueError:
        pass
    env = dict(os.environ, EVR_GAME_PC=str(game), MAIN_MENU_A_OUT_ROOT=str(out.parent), MAIN_MENU_A_ROOT=str(HERE))
    import game_store
    os.environ["EVR_GAME_PC"] = str(game)
    game_store.Basis.named("pcf").cells()
    out.parent.mkdir(parents=True, exist_ok=True)
    r = subprocess.run(["bash", str(LIB / "menu_ui" / "ffa_build.sh"), out.name, "--stage", "latest"], env=env)
    if r.returncode:
        return r.returncode
    return verify(out, HERE / "expected_pc.tsv")


PC_ONLY_GLOBAL = ("0dc32069c96e74a1", "3f9915d3001dc28e", "90950ed3107711a6")


def _quest_twin(qtree: Path, qout: Path) -> None:
    for p in sorted(qtree.rglob("*")):
        if p.is_file() and p.name in PC_ONLY_GLOBAL:
            p.unlink()
    card, twin = qout / "quest_twin_card", qout / "ffa_quest_twin"
    env = dict(os.environ, MAIN_MENU_A_ROOT=str(HERE))
    r = subprocess.run(["bash", str(LIB / "menu_ui" / "w17a" / "quest_twin.sh"), str(card)], env=env)
    if r.returncode:
        sys.exit("refusing: the Quest twin of the social card did not build")
    r = subprocess.run(["bash", str(LIB / "menu_ui" / "w23e" / "quest_twin.sh"), str(card), str(twin)], env=env)
    if r.returncode:
        sys.exit("refusing: the Quest twin of the report layer did not build")
    shutil.rmtree(card)
    for p in sorted(twin.rglob("*")):
        if p.is_file():
            dst = qtree / p.relative_to(twin)
            if dst.exists():
                sys.exit(f"refusing: {dst} already exists in the Quest tree")
            dst.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(p, dst)
    shutil.rmtree(twin)


def quest(a) -> int:
    import radpkg
    import quest_stage as qs
    from csymbol64 import hash_name
    os.environ["EVR_GAME_QUEST"] = str(Path(a.quest_store).resolve())
    os.environ["EVR_GAME_PC"] = str(Path(a.game).resolve())
    import game_store
    q = game_store.Basis.named("q")
    qout = Path(a.out).resolve()
    if qout.exists():
        sys.exit(f"refusing: {qout} exists")
    qtree = qout / "quest_tree"
    qout.mkdir(parents=True)
    qs.transform(str(Path(a.pc_tree).resolve()), str(qtree))
    _quest_twin(qtree, qout)
    store = Path(q.dir)
    base = radpkg.parse(radpkg.unwrap((store / "manifests" / qs.BASE).read_bytes()))
    lang = radpkg.parse(radpkg.unwrap((store / "manifests" / qs.LANG).read_bytes()))
    blob = bytearray((store / "packages" / f"{qs.LANG}_0").read_bytes())
    if lang["pkgcount"] != 1 or lang["frames"][-1][3] != 0 or lang["frames"][-1][1] != len(blob):
        sys.exit("language package is not a single package with a terminal frame")
    import zstandard
    frames, contents, metadata = lang["frames"][:-1], list(lang["contents"]), list(lang["metadata"])
    bcont = {(c[0], c[1]): c for c in base["contents"]}
    bmeta = {(m[0], m[1]): m for m in base["metadata"]}
    proto = {}
    for c in base["contents"]:
        proto.setdefault(c[0], (c, bmeta[(c[0], c[1])]))
    H = lambda n: hash_name(n) & (2 ** 64 - 1)
    qtypes = {r.type_symbol for cell in q.cells().values() for r in list(cell.values())[:1]}
    nadd = n = 0
    for layer, t, v, fid, path in sorted(qs.tree_files(str(qtree))):
        if t not in qtypes:
            sys.exit(f"refusing: {t:016x} is not a Quest type dir")
        data = Path(path).read_bytes()
        fr = zstandard.ZstdCompressor(level=3).compress(data)
        frames.append((0, len(blob), len(fr), len(data)))
        blob += fr
        if (t, fid) in bcont:
            c, m = bcont[(t, fid)], bmeta[(t, fid)]
        else:
            if t not in proto:
                sys.exit(f"no stock row of type {t:016x} to copy flags from")
            c, pm = proto[t]
            m = (t, fid, H(f"{fid:016x}|lo"), H(f"{fid:016x}|hi"), pm[4])
            nadd += 1
        contents.append((t, fid, len(frames) - 1, 0, len(data), c[5]))
        metadata.append(m)
        n += 1
    frames.append((0, len(blob), 0, 0))
    man = radpkg.wrap(radpkg.serialize(dict(pkgcount=1, contents=contents, metadata=metadata, frames=frames)))
    (qout / "menu" / "manifests").mkdir(parents=True)
    (qout / "menu" / "packages").mkdir(parents=True)
    (qout / "menu" / "manifests" / qs.LANG).write_bytes(man)
    (qout / "menu" / "packages" / f"{qs.LANG}_0").write_bytes(bytes(blob))
    print(f"{qs.LANG}: +{n} rows ({n - nadd} overrides, {nadd} new ids) -> {qout / 'menu'}")
    ok = (hashlib.sha256(bytes(blob)).hexdigest() == QUEST_PACKAGE_SHA
          and hashlib.sha256(radpkg.unwrap(man)).hexdigest() == QUEST_MANIFEST_PAYLOAD_SHA)
    print("verify: Quest language pair " + ("matches the expected build" if ok else
          "DIFFERS from the expected build (a PC tree from another version of this tool, or another Quest version)"))
    return 0 if ok else 1


PC_SUB = "_data/5932408047/rad15/win10"
PC_BASE, PC_LANG = "48037dc70b0ecab2", "2b47aab238f60515"
PC_PINS = dict(base="e2ae781d32a7fdea89b67a638f33dbab997117c157544b76efa20368eb498fd9",
               lang_manifest="8490bb1d6e15178d12c4472b57bc7cd0ad9a60e12f591532897086a116f1356f",
               lang_package="f2fecc4c595b2188a689b67844d497439ea9a7f32c8af5c1a789c3955aa4dab7")
PC_STOCK_PACKAGE_LEN, PC_STOCK_ROWS = 16564907, 43


def _pc_store(game: str) -> Path:
    g = Path(game).resolve()
    for d in (g, g / PC_SUB):
        if (d / "manifests" / PC_BASE).is_file():
            return d
    sys.exit(f"refusing: no manifests/{PC_BASE} under {g} or {g / PC_SUB}")


def _pc_pair(d: Path) -> tuple[Path, Path]:
    return d / "manifests" / PC_LANG, d / "packages" / f"{PC_LANG}_0"


def _pc_state(store: Path, out: Path | None) -> str:
    """'stock', 'menu' (this build's pair from OUT, or the pinned build) or 'other'."""
    m, k = (p.read_bytes() for p in _pc_pair(store))
    if (_sha_b(m), _sha_b(k)) == (PC_PINS["lang_manifest"], PC_PINS["lang_package"]):
        return "stock"
    if out is not None:
        om, ok_ = _pc_pair(out / "menu")
        if om.is_file() and ok_.is_file() and (m, k) == (om.read_bytes(), ok_.read_bytes()):
            return "menu"
    return "menu" if _is_menu_pair(m, k) else "other"


def _is_menu_pair(m: bytes, k: bytes) -> bool:
    """The stock pair plus exactly the pinned tree's rows appended (whatever zstd made of them)."""
    import radpkg
    n = PC_STOCK_PACKAGE_LEN
    if len(k) <= n or _sha_b(k[:n]) != PC_PINS["lang_package"]:
        return False
    try:
        rows = radpkg.parse(radpkg.unwrap(m))["contents"]
    except Exception:
        return False
    want = {(int(r.split("/")[1], 16), int(r.split("/")[3], 16))
            for r in (ln.split("\t")[0] for ln in (HERE / "expected_pc.tsv").read_text().splitlines()[1:])}
    return len(rows) == PC_STOCK_ROWS + len(want) and {(r[0], r[1]) for r in rows[PC_STOCK_ROWS:]} == want


def _sha_b(b: bytes) -> str:
    return hashlib.sha256(b).hexdigest()


def _pc_tree_files(tree: Path):
    """(type, file, path) per `primary/<type>/v<version>/<file>`; anything else in the tree is refused."""
    out = []
    for rel, p in _tree(tree).items():
        parts = rel.split("/")
        if len(parts) != 4 or parts[0] != "primary" or not parts[2].startswith("v"):
            sys.exit(f"refusing: {rel} is not primary/<type>/v<version>/<file>")
        out.append((int(parts[1], 16), int(parts[3], 16), p))
    return sorted(out)


def pc_package_build(store: Path, tree: Path) -> dict:
    """Pin-check the stock inputs, append one row per tree file, read every row back. Returns the new pair."""
    import radpkg
    import zstandard
    from csymbol64 import hash_name
    base_raw = (store / "manifests" / PC_BASE).read_bytes()
    lm, lk = _pc_pair(store)
    lang_raw, lang_pkg = lm.read_bytes(), lk.read_bytes()
    for what, data, pin in (("base manifest", base_raw, "base"), ("language manifest", lang_raw, "lang_manifest"),
                            ("language package", lang_pkg, "lang_package")):
        if _sha_b(data) != PC_PINS[pin]:
            sys.exit(f"refusing: {what} sha256 {_sha_b(data)[:16]}... is not the stock {PC_PINS[pin][:16]}... "
                     "(another game version, or already modified by a mod: revert it or repair the install first)")
    base = radpkg.parse(radpkg.unwrap(base_raw))
    lang = radpkg.parse(radpkg.unwrap(lang_raw))
    blob = bytearray(lang_pkg)
    if lang["pkgcount"] != 1 or lang["frames"][-1][3] != 0 or lang["frames"][-1][1] != len(blob):
        sys.exit("refusing: language package is not a single package with a terminal frame")
    frames, contents, metadata = lang["frames"][:-1], list(lang["contents"]), list(lang["metadata"])
    in_lang = {(c[0], c[1]) for c in contents}
    bcont = {(c[0], c[1]): c for c in base["contents"]}
    bmeta = {(m[0], m[1]): m for m in base["metadata"]}
    proto = {}
    for c in base["contents"]:
        proto.setdefault(c[0], (c, bmeta[(c[0], c[1])]))
    H = lambda n: hash_name(n) & (2 ** 64 - 1)
    added, nadd = [], 0
    for t, fid, path in _pc_tree_files(tree):
        if (t, fid) in in_lang:
            sys.exit(f"refusing: {t:016x}/{fid:016x} already has a language-package row")
        if t not in proto:
            sys.exit(f"refusing: {t:016x} is not a PC type of this build")
        data = path.read_bytes()
        fr = zstandard.ZstdCompressor(level=3).compress(data)
        frames.append((0, len(blob), len(fr), len(data)))
        blob += fr
        if (t, fid) in bcont:
            c, m = bcont[(t, fid)], bmeta[(t, fid)]
        else:
            c, pm = proto[t]
            m = (t, fid, H(f"{fid:016x}|lo"), H(f"{fid:016x}|hi"), pm[4])
            nadd += 1
        contents.append((t, fid, len(frames) - 1, 0, len(data), c[5]))
        metadata.append(m)
        added.append((t, fid, data))
    frames.append((0, len(blob), 0, 0))
    payload = radpkg.serialize(dict(pkgcount=1, contents=contents, metadata=metadata, frames=frames))
    man, pkg = radpkg.wrap(payload), bytes(blob)
    new = radpkg.parse(radpkg.unwrap(man))
    if new["contents"][:len(lang["contents"])] != lang["contents"] or pkg[:len(lang_pkg)] != lang_pkg:
        sys.exit("refusing: rebuilt language pair changed a stock row")
    rows = {(r[0], r[1]): r for r in new["contents"][len(lang["contents"]):]}
    dz = zstandard.ZstdDecompressor()
    for t, fid, data in added:
        r = rows[(t, fid)]
        _pk, off, csz, ln = new["frames"][r[2]]
        if dz.decompress(pkg[off:off + csz], max_output_size=ln)[r[3]:r[3] + r[4]] != data:
            sys.exit(f"refusing: row {t:016x}/{fid:016x} does not read back")
    return dict(manifest=man, package=pkg, stock_manifest=lang_raw, stock_package=lang_pkg, rows=len(added),
                new_ids=nadd, payload_sha=_sha_b(payload), package_sha=_sha_b(pkg))


def _write_file(p: Path, data: bytes):
    """temp + rename (never writes through a hardlink); refuses a hardlinked target."""
    p.parent.mkdir(parents=True, exist_ok=True)
    if p.exists() and os.stat(p).st_nlink > 1:
        sys.exit(f"refusing: {p} is a hardlink")
    tmp = p.with_name(p.name + ".tmp")
    tmp.write_bytes(data)
    os.replace(tmp, p)


def pc_package(a) -> int:
    store, tree = _pc_store(a.game), Path(a.pc_tree).resolve()
    pinned = verify(tree, HERE / "expected_pc.tsv") == 0
    if not pinned and not a.unpinned_tree:
        sys.exit("refusing: the tree is not the pinned build (pass --unpinned-tree to package it anyway)")
    res = pc_package_build(store, tree)
    print(f"{PC_LANG}: +{res['rows']} rows ({res['rows'] - res['new_ids']} overrides, {res['new_ids']} new ids); "
          f"package {len(res['stock_package'])} -> {len(res['package'])} B; package sha256 {res['package_sha']}; "
          f"manifest payload sha256 {res['payload_sha']}")
    print(f"verify: stock rows unchanged, {res['rows']}/{res['rows']} new rows read back"
          + ("" if pinned else " (unpinned tree)"))
    if a.dry_run:
        print("dry run: nothing was written")
        return 0
    out = Path(a.out).resolve()
    if out.exists():
        sys.exit(f"refusing: {out} exists")
    for g in (Path(a.game).resolve(), store):
        try:
            out.relative_to(g)
            sys.exit("refusing: --out must be outside the game install")
        except ValueError:
            pass
    for sub, m, k in (("menu", res["manifest"], res["package"]), ("stock", res["stock_manifest"], res["stock_package"])):
        pm, pk = _pc_pair(out / sub)
        _write_file(pm, m)
        _write_file(pk, k)
    print(f"wrote {out / 'menu'} (Main Menu A pair) and {out / 'stock'} (your stock pair, for revert)")
    return 0


def pc_copy(a, which: str) -> int:
    store, out = _pc_store(a.game), Path(a.out).resolve()
    state = _pc_state(store, out)
    if state == "other":
        sys.exit("refusing: the install's language package is neither stock nor this Main Menu A pair "
                 "(another mod changed it: revert that mod first, or repair the install)")
    sm, sk = _pc_pair(out / which)
    if not (sm.is_file() and sk.is_file()):
        sys.exit(f"refusing: {out / which} has no language pair (run pc-package first)")
    m, k = sm.read_bytes(), sk.read_bytes()
    if which == "stock" and (_sha_b(m), _sha_b(k)) != (PC_PINS["lang_manifest"], PC_PINS["lang_package"]):
        sys.exit("refusing: the saved stock pair does not match the stock pins")
    tm, tk = _pc_pair(store)
    _write_file(tk, k)
    _write_file(tm, m)
    now = _pc_state(store, out)
    print(f"install language package: {state} -> {now}")
    return 0 if now == which else 1


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sp = ap.add_subparsers(dest="cmd", required=True)
    p = sp.add_parser("pc")
    p.add_argument("--game", required=True, help="Echo VR PC install dir (or its _data/.../win10 dir)")
    p.add_argument("--out", required=True, help="new loose tree dir, outside the install")
    pp = sp.add_parser("pc-package", help="append the PC tree to a copy of your language package pair")
    pp.add_argument("--game", required=True, help="Echo VR PC install dir (or its _data/.../win10 dir)")
    pp.add_argument("--pc-tree", required=True, help="a tree from the `pc` command")
    pp.add_argument("--out", help="new output dir, outside the install (not needed with --dry-run)")
    pp.add_argument("--dry-run", action="store_true", help="build and verify in memory; write nothing")
    pp.add_argument("--unpinned-tree", action="store_true", help="package a tree that is not the pinned build")
    for name, hlp in (("pc-apply", "copy OUT/menu over the install's language pair"),
                      ("pc-revert", "copy OUT/stock back over the install's language pair")):
        q = sp.add_parser(name, help=hlp)
        q.add_argument("--game", required=True, help="Echo VR PC install dir (or its _data/.../win10 dir)")
        q.add_argument("--out", required=True, help="the pc-package output dir")
    cp = sp.add_parser("check", help="verify the install against the build's pins; writes nothing")
    cp.add_argument("--game", required=True, help="Echo VR PC install dir (or its _data/.../win10 dir)")
    cp.add_argument("--quest-store", help="local copy of the headset's rad15/android dir (optional)")
    qp = sp.add_parser("quest")
    qp.add_argument("--pc-tree", required=True, help="a tree from the `pc` command")
    qp.add_argument("--quest-store", required=True, help="local copy of the headset's rad15/android dir")
    qp.add_argument("--game", required=True, help="the PC install (class table for the PC tree)")
    qp.add_argument("--out", required=True, help="new output dir")
    a = ap.parse_args(argv)
    if a.cmd == "pc-package" and not (a.dry_run or a.out):
        ap.error("pc-package needs --out (or --dry-run)")
    return {"pc": pc, "pc-package": pc_package, "pc-apply": lambda x: pc_copy(x, "menu"),
            "pc-revert": lambda x: pc_copy(x, "stock"), "quest": quest, "check": check}[a.cmd](a)


if __name__ == "__main__":
    sys.exit(main())
