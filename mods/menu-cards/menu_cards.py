#!/usr/bin/env python3
"""Menu cards: add CREATE GAME and JOIN VIA CODE cards to the Echo VR main menu (page 24), built from YOUR install.

The cards are data. The main menu page resource is copied out of your install, two card elements are inserted, and
the result is appended as one row to a copy of the language package (`2b47aab238f60515`). The engine loads that
package after the base package (`48037dc70b0ecab2`) and the later row wins, on PC and on Quest. Every stock input is
checked against a sha256 pin first: a different game version, or a language package that is already modified, is
refused. The build step writes only into --out; your install is changed only by `apply` / `push`, and `revert`
puts the stock pair back.

PC (close Echo VR first):
  menu_cards.py pc build  --game "C:/Program Files/Oculus/Software/Software/ready-at-dawn-echo-arena" --out DIR
  menu_cards.py pc apply  --game ... --out DIR      # copy DIR/cards into the install
  menu_cards.py pc revert --game ... --out DIR      # copy DIR/stock back
  (--loose also writes DIR/loose/primary/<type>/v<version>/<file>, for a loose-resource loader instead)

Quest (headset connected by adb, Echo VR closed on the headset; ADB from --adb, $ADB or PATH):
  menu_cards.py quest pull   --work WORK          # copies the stock pair + three small frames off the headset
  menu_cards.py quest build  --work WORK --out DIR
  menu_cards.py quest push   --out DIR
  menu_cards.py quest revert --out DIR
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
import shutil
import struct
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
for p in (HERE, HERE.parents[1] / "sdk" / "tools"):
    if p.is_dir() and str(p) not in sys.path:
        sys.path.insert(0, str(p))
import radpkg  # noqa: E402
from cards_page import TILES, add_cards, element_uid  # noqa: E402

BASE, LANG = "48037dc70b0ecab2", "2b47aab238f60515"
PAGE = 24
CARDS = [(0xff5375e5a4dee29d, TILES["tile_create"], "CREATE GAME", "", "Create a private match"),
         (0x097b785350a61c5f, TILES["tile_join"], "JOIN VIA CODE", "", "Join a match with an invite code")]
# The resources are byte-identical on both platforms; only the type symbols (and the page's file id) differ.
RES_SHA = dict(page="af396701cfe27cb5d9fd512943b1ad732477b6399c4df5a2b36971375a6919eb",
               elcr="912c85818a4f82890164f2a8bdad37aaf28f45ad739b7d4c32f728b20a3f73cc",
               canvas="53dd03121fe5779eb6e3605fd8ee7d270189ad8da0b8f3f41ef27aec91d83ac5")
PLATFORMS = {
    "pc": dict(
        label="PC build 6323983201049540",
        data_subdir="_data/5932408047/rad15/win10",
        page=(0x4a3fe67c1ff9ccc4, 0xac360e41e4ede056),      # CR15UIPage2CR, mnu_master
        elcr=(0xc1b1fca890787b30, 0xac360e41e4ede056),      # CR15UIPage2ElementCR, mnu_master
        canvas=(0x59a9bd6e4525ecc4, 0x86300ac7254c2eb8),    # CUICanvasResource, ui_mnu_card
        pins=dict(base="e2ae781d32a7fdea89b67a638f33dbab997117c157544b76efa20368eb498fd9",
                  lang_manifest="8490bb1d6e15178d12c4472b57bc7cd0ad9a60e12f591532897086a116f1356f",
                  lang_package="f2fecc4c595b2188a689b67844d497439ea9a7f32c8af5c1a789c3955aa4dab7")),
    "quest": dict(
        label="Quest build 6528386917200980 (version code 4987566)",
        device_dir="/sdcard/readyatdawn/_data/5932408047/rad15/android",
        package_name="com.readyatdawn.r15",
        page=(0x43433aacbeeab166, 0xf927772b9e2aefb1),      # CR15UIPage2CR (Android type), mnu_master_lowspec
        elcr=(0x6edbc955fe4c81a4, 0xf927772b9e2aefb1),
        canvas=(0xa39ab682e439b36a, 0x86300ac7254c2eb8),
        pins=dict(base="762c16ae58c9bc70eee071d8c0e2e8fc0408d1f0691032164029294c5cb7ec48",
                  lang_manifest="26becc4814f88436b522a36318463eff60c15e4337f0912d7f21237b17754ebe",
                  lang_package="dd0a1087acfe15b0563c17298963b707f949ad4efb42d0ae85b8faa0b4b47e12")),
}
PATCHED_PAGE_SHA = "ac3a7173bbc78c75ba22fc9ad2f7bf12003d27bdc1b8ff1d215cdd8cae97f8b5"   # page with CARDS


class Refused(Exception):
    pass


def _sha(b: bytes) -> str:
    return hashlib.sha256(b).hexdigest()


def _pin(what: str, data: bytes, want: str):
    got = _sha(data)
    if got != want:
        raise Refused(f"{what}: sha256 {got[:16]}... is not the stock {want[:16]}... "
                      "(different game version, or already modified: revert or reinstall first)")


# ---- the build, in memory -----------------------------------------------------------------------------------------
def build(plat: str, base_raw: bytes, lang_manifest_raw: bytes, lang_package: bytes, read_frame=None,
          resources: dict | None = None, cards=CARDS, log=print) -> dict:
    """Verify the stock inputs, patch the page, append it to a copy of the language pair.
    `read_frame(pk, off, csz)` reads base-package frames; `resources` (name -> bytes) may replace it."""
    spec = PLATFORMS[plat]
    pins = spec["pins"]
    _pin(f"base manifest {BASE}", base_raw, pins["base"])
    _pin(f"language manifest {LANG}", lang_manifest_raw, pins["lang_manifest"])
    _pin(f"language package {LANG}_0", lang_package, pins["lang_package"])
    base = radpkg.parse(radpkg.unwrap(base_raw))
    got = {}
    for name in ("page", "elcr", "canvas"):
        t, f = spec[name]
        if resources is not None:
            got[name] = resources[name]
        else:
            got[name] = radpkg.read_resource(base, t, f, read_frame)
        _pin(f"resource {name} (0x{t:016x}/0x{f:016x})", got[name], RES_SHA[name])
    for c in cards:
        uid = element_uid(got["elcr"], c[0])
        if uid is None:
            raise Refused(f"owner 0x{c[0]:016x} has no page-element record")
        if struct.pack("<Q", c[1]) not in got["canvas"]:
            raise Refused(f"sprite 0x{c[1]:016x} is not in the card canvas sprite list")
        log(f"card 0x{c[0]:016x} uid 0x{uid:016x} sprite 0x{c[1]:016x} {c[2]!r}")
    try:
        page, rep = add_cards(got["page"], PAGE, list(cards))
    except ValueError as e:
        raise Refused(str(e))
    if cards == CARDS:
        _pin("patched page", page, PATCHED_PAGE_SHA)
    pt, pf = spec["page"]
    manifest, package = radpkg.append_rows(base, lang_manifest_raw, lang_package, [(pt, pf, page)])
    # read the new row back out of the new pair; the stock rows must be untouched
    new = radpkg.parse(radpkg.unwrap(manifest))
    old = radpkg.parse(radpkg.unwrap(lang_manifest_raw))
    if new["contents"][:len(old["contents"])] != old["contents"] or package[:len(lang_package)] != lang_package:
        raise Refused("rebuilt language pair changed a stock row")
    row = new["contents"][-1]
    pk, off, csz, ln = new["frames"][row[2]]
    import zstandard
    back = zstandard.ZstdDecompressor().decompress(package[off:off + csz], max_output_size=ln)[row[3]:row[3] + row[4]]
    if (row[0], row[1]) != (pt, pf) or back != page:
        raise Refused("rebuilt language pair does not read back the patched page")
    version = next(m[4] for m in base["metadata"] if (m[0], m[1]) == (pt, pf))
    rep.update(platform=spec["label"], page_sha256=_sha(page), manifest_sha256=_sha(manifest),
               package_sha256=_sha(package), rows=f"{len(old['contents'])}->{len(new['contents'])}")
    log(json.dumps(rep))
    return dict(page=page, manifest=manifest, package=package, version=version, report=rep,
                stock_manifest=lang_manifest_raw, stock_package=lang_package)


# ---- file helpers -------------------------------------------------------------------------------------------------
def _read(p: Path) -> bytes:
    return p.read_bytes()


def _write(p: Path, data: bytes):
    """Write via a temp file + rename, so an existing hardlink is never written through; refuse one anyway."""
    p.parent.mkdir(parents=True, exist_ok=True)
    if p.exists() and os.stat(p).st_nlink > 1:
        raise Refused(f"{p} is a hardlink; refusing to write")
    tmp = p.with_name(p.name + ".tmp")
    tmp.write_bytes(data)
    os.replace(tmp, p)


def _pair(d: Path) -> tuple[Path, Path]:
    return d / "manifests" / LANG, d / "packages" / f"{LANG}_0"


def _write_outputs(res: dict, out: Path, plat: str, loose: bool):
    for sub, m, k in (("cards", res["manifest"], res["package"]), ("stock", res["stock_manifest"], res["stock_package"])):
        pm, pp = _pair(out / sub)
        _write(pm, m)
        _write(pp, k)
    if loose:
        t, f = PLATFORMS[plat]["page"]
        _write(out / "loose" / "primary" / f"{t:016x}" / f"v{res['version']}" / f"{f:016x}", res["page"])
    _write(out / "cards.json", (json.dumps(res["report"], indent=2) + "\n").encode())
    print(f"wrote {out / 'cards'} (patched pair) and {out / 'stock'} (stock pair, for revert)")


def _inside(child: Path, parent: Path) -> bool:
    try:
        child.resolve().relative_to(parent.resolve())
        return True
    except ValueError:
        return False


# ---- PC -------------------------------------------------------------------------------------------------------------
def _pc_store(game: str) -> Path:
    g = Path(game)
    for d in (g, g / PLATFORMS["pc"]["data_subdir"]):
        if (d / "manifests" / BASE).is_file():
            return d
    raise Refused(f"no manifests/{BASE} under {g} or {g / PLATFORMS['pc']['data_subdir']}")


def pc_build(game: str, out: str, loose: bool):
    store, outp = _pc_store(game), Path(out)
    if _inside(outp, Path(game)) or _inside(outp, store):
        raise Refused("--out must be outside the game install")
    res = build("pc", _read(store / "manifests" / BASE), _read(store / "manifests" / LANG),
                _read(store / "packages" / f"{LANG}_0"),
                read_frame=radpkg.file_frame_reader(str(store / "packages"), BASE))
    _write_outputs(res, outp, "pc", loose)


def _install_state(m: bytes, k: bytes, out: Path) -> str:
    pins = PLATFORMS["pc"]["pins"]
    if (_sha(m), _sha(k)) == (pins["lang_manifest"], pins["lang_package"]):
        return "stock"
    cm, ck = _pair(out / "cards")
    if cm.is_file() and (m, k) == (_read(cm), _read(ck)):
        return "cards"
    return "unknown"


def pc_copy(game: str, out: str, which: str):
    store, outp = _pc_store(game), Path(out)
    tm, tk = _pair(store)
    state = _install_state(_read(tm), _read(tk), outp)
    if state == "unknown":
        raise Refused("the install's language package is neither stock nor this build's cards; refusing to touch it")
    sm, sk = _pair(outp / which)
    m, k = _read(sm), _read(sk)
    if which == "stock":
        pins = PLATFORMS["pc"]["pins"]
        _pin("stock manifest copy", m, pins["lang_manifest"])
        _pin("stock package copy", k, pins["lang_package"])
    _write(tk, k)
    _write(tm, m)
    print(f"install language package: {state} -> {which}")


# ---- Quest ----------------------------------------------------------------------------------------------------------
def _adb_bin(arg: str | None) -> str:
    a = arg or os.environ.get("ADB") or shutil.which("adb") or shutil.which("adb.exe")
    if not a:
        raise Refused("adb not found: pass --adb, set $ADB, or put adb on PATH")
    return a


def _local(adb: str, p: Path) -> str:
    """A local path as adb sees it (a Windows adb.exe driven from WSL needs a Windows path)."""
    if adb.lower().endswith(".exe") and shutil.which("wslpath"):
        return subprocess.run(["wslpath", "-w", str(p)], capture_output=True, text=True, check=True).stdout.strip()
    return str(p)


def _adb(adb: str, *a: str, binary: bool = False):
    r = subprocess.run([adb, *a], capture_output=True)
    if r.returncode:
        raise Refused(f"adb {' '.join(a)}: {(r.stderr or r.stdout).decode(errors='replace').strip()}")
    return r.stdout if binary else r.stdout.decode(errors="replace")


def _app_closed(adb: str):
    if _adb(adb, "shell", f"pidof {PLATFORMS['quest']['package_name']} || true").strip():
        raise Refused("Echo VR is running on the headset; close it first")


def quest_pull(work: str, adb_arg: str | None):
    adb, w, dev, spec = _adb_bin(adb_arg), Path(work), PLATFORMS["quest"]["device_dir"], PLATFORMS["quest"]
    files = {w / "manifests" / BASE: f"{dev}/manifests/{BASE}", w / "manifests" / LANG: f"{dev}/manifests/{LANG}",
             w / "packages" / f"{LANG}_0": f"{dev}/packages/{LANG}_0"}
    for loc, remote in files.items():
        _write(loc, _adb(adb, "exec-out", "cat", remote, binary=True))
    base_raw = _read(w / "manifests" / BASE)
    _pin("headset base manifest", base_raw, spec["pins"]["base"])
    _pin("headset language manifest", _read(w / "manifests" / LANG), spec["pins"]["lang_manifest"])
    _pin("headset language package", _read(w / "packages" / f"{LANG}_0"), spec["pins"]["lang_package"])
    base = radpkg.parse(radpkg.unwrap(base_raw))
    for name in ("page", "elcr", "canvas"):
        r = radpkg.find_row(base, *spec[name])
        pk, off, csz, _ = base["frames"][r[2]]
        dest = w / "frames" / f"{BASE}_{pk}.{off}.{csz}"
        if dest.is_file():
            continue
        bs = 4096   # plain dd (bs/skip/count only): whole blocks, sliced here
        first, nblk = off // bs, (off % bs + csz + bs - 1) // bs
        raw = _adb(adb, "exec-out", f"dd if={dev}/packages/{BASE}_{pk} bs={bs} skip={first} count={nblk} 2>/dev/null",
                   binary=True)
        frame = raw[off % bs:off % bs + csz]
        if len(frame) != csz:
            raise Refused(f"short frame read for {name}")
        _write(dest, frame)
    print(f"pulled the stock language pair, base manifest and 3 frames into {w}")


def _work_frame_reader(w: Path):
    def read(pk: int, off: int, csz: int) -> bytes:
        f = w / "frames" / f"{BASE}_{pk}.{off}.{csz}"
        if f.is_file():
            return _read(f)
        return radpkg.file_frame_reader(str(w / "packages"), BASE)(pk, off, csz)
    return read


def quest_build(work: str, out: str):
    w = Path(work)
    res = build("quest", _read(w / "manifests" / BASE), _read(w / "manifests" / LANG),
                _read(w / "packages" / f"{LANG}_0"), read_frame=_work_frame_reader(w))
    _write_outputs(res, Path(out), "quest", False)


def quest_push(out: str, adb_arg: str | None, which: str):
    adb, dev = _adb_bin(adb_arg), PLATFORMS["quest"]["device_dir"]
    _app_closed(adb)
    m, k = _pair(Path(out) / which)
    if which == "stock":
        _pin("stock manifest copy", _read(m), PLATFORMS["quest"]["pins"]["lang_manifest"])
        _pin("stock package copy", _read(k), PLATFORMS["quest"]["pins"]["lang_package"])
    _adb(adb, "push", _local(adb, m), f"{dev}/manifests/{LANG}")
    _adb(adb, "push", _local(adb, k), f"{dev}/packages/{LANG}_0")
    got = _adb(adb, "shell", f"sha256sum {dev}/manifests/{LANG} {dev}/packages/{LANG}_0").split()
    ok = got[0] == _sha(_read(m)) and got[2] == _sha(_read(k))
    print(f"{which} language package {'pushed' if ok else 'MISMATCH after push'}")
    if not ok:
        raise Refused("headset copy does not match")


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sp = ap.add_subparsers(dest="plat", required=True)
    pc = sp.add_parser("pc")
    pc.add_argument("action", choices=["build", "apply", "revert"])
    pc.add_argument("--game", required=True, help="Echo VR install dir (or its _data/.../win10 dir)")
    pc.add_argument("--out", required=True, help="output dir, outside the install")
    pc.add_argument("--loose", action="store_true", help="build: also write the page as a loose-tree file")
    q = sp.add_parser("quest")
    q.add_argument("action", choices=["pull", "build", "push", "revert"])
    q.add_argument("--work", help="pull/build: local copy of the headset inputs")
    q.add_argument("--out", help="build/push/revert: output dir")
    q.add_argument("--adb", help="adb executable (default: $ADB, then PATH)")
    a = ap.parse_args(argv)
    try:
        if a.plat == "pc":
            if a.action == "build":
                pc_build(a.game, a.out, a.loose)
            else:
                pc_copy(a.game, a.out, "cards" if a.action == "apply" else "stock")
        else:
            need = {"pull": ("work",), "build": ("work", "out"), "push": ("out",), "revert": ("out",)}[a.action]
            for n in need:
                if not getattr(a, n):
                    ap.error(f"quest {a.action} needs --{n}")
            if a.action == "pull":
                quest_pull(a.work, a.adb)
            elif a.action == "build":
                quest_build(a.work, a.out)
            else:
                quest_push(a.out, a.adb, "cards" if a.action == "push" else "stock")
    except Refused as e:
        print(f"refused: {e}; the install was not changed", file=sys.stderr)
        return 2
    return 0


if __name__ == "__main__":
    sys.exit(main())
