#!/usr/bin/env python3
"""Build the community-mod Quest APK from the user's OWN stock store APK (Echo VR Quest 6528386917200980, vc 4987566).

One ordered pipeline. Every patch is written
against STOCK bytes (nothing from any community APK). Each libr15.so step checks the whole-file
sha256 of its input against the pinned chain, applies its sites (each site's stock bytes are asserted by the module
that owns it), and records the output sha256; the last check masks every patched range and requires the rest of the
file to equal stock.

  libr15.so
    1 vulkan-boot     0x18ddaf4 cbz -> b (the current Quest 3 OS boot fix)
    2 sdcard-config   /sdcard/readyatdawn/_local/config.json first      quest_libr15_sdcard_config_patch.patches
    3 api-config      both API connections read config apiservice_host  quest_libr15_api_config_patch.patches
    4 store-api-path  0x126c21c cbz -> b: the store API always takes    quest_libr15_api_config_patch.STORE_API_PATH_PATCHES
                      the (now config-driven) literal path, never https://api-%s.readyatdawn.com
    5 early-quit      client half of the early-quit lockout             quest_libr15_earlyquit_patch.SITES
    6 loader          "libevrmod.so" appended to DT_NEEDED             s_loader: the spare
                      .dynamic slot 34 (NULL) -> NEEDED, DT_STRSZ widened to reach the name, the name in the
                      shared cave above the api-config stubs (0x2a760f0, 13 B); the .dynstr section header's sh_size widened
                      to match (bionic's ElfReader bounds DT_NEEDED names by it)
  libpnsovr.so
    7 pnsovr-login    login prerequisite bypass                         quest_libpnsovr_login_patch.patch_libpnsovr
  APK
    8 plugins         lib/arm64-v8a/libevrmod.so (the loader, --loader-so) and each --plugin <so> (repeatable;
                      default: rad-links' libradlinks.so) as lib/arm64-v8a/libevrmod_<stem>.so (libradlinks.so
                      -> libevrmod_radlinks.so). The loader dlopens libevrmod_*.so in strcmp order. With
                      --skip-loader the plugins keep their own names and ship inert.
    9 repack + sign   v1/JAR signature with --keystore (PKCS12); if it does not exist, keytool generates it there
                      on this machine. No keystore is committed or shipped.

Not steps, on purpose: no service-URL literal rewrite (login / config /
transaction / serverdb hosts are config-driven; the API by step 3), so the literals stay
stock and only act as a fallback. No embedded host: every host comes from the user's /sdcard config (or
--embed-config, a file the user passes). No debuggable flag and no wrap.sh: step 6 is what loads the plugins. A
plugin crash loop is undone with no reinstall by creating /sdcard/readyatdawn/_local/evrmod.disable.

  quest_apk_build_release.py --apk STOCK.apk --out OUT.apk --keystore my.p12
  quest_apk_build_release.py --apk STOCK.apk --out OUT.apk --keystore my.p12 --plugin libradlinks.so --plugin X.so
  quest_apk_build_release.py --apk STOCK.apk --dry-run                        patch in memory, print the step table
Memory: one decompressed APK entry at a time, plus libr15.so (58 MB) and libpnsovr.so (7 MB). Two passes over the
input zip (digest, then write). Refuses to write over its input (or a hardlink of it)."""
import argparse, glob, hashlib, os, struct, subprocess, sys, zipfile
from pathlib import Path

HERE = os.path.dirname(os.path.abspath(__file__))
# The patch modules sit beside this file, or in the early-quit mod's quest/ directory (sdk/quest in echovr-modkit).
for _d in (HERE, os.path.join(HERE, '..', '..', 'mods', 'early-quit', 'quest')):
    if os.path.isdir(_d) and os.path.abspath(_d) not in sys.path:
        sys.path.append(os.path.abspath(_d))
import quest_libr15_sdcard_config_patch as sd                     # noqa: E402
import quest_libr15_api_config_patch as api                       # noqa: E402
import quest_libr15_earlyquit_patch as eq                         # noqa: E402
import quest_apk_v1sign as v1                                     # noqa: E402
import quest_libpnsovr_login_patch as pns                         # noqa: E402

BUILD = '6528386917200980'                                         # vc 4987566, 2023-04-24
# The store APK as Meta serves it (the same bytes for every owner).
STOCK_APK_SHA = '21c7dc914dba2fa44f8daf019aedffa4a17e14186283ef05805217fc80d30eaf'
SO_ENTRY, OVR_ENTRY, LIB_DIR = 'lib/arm64-v8a/libr15.so', 'lib/arm64-v8a/libpnsovr.so', 'lib/arm64-v8a/'
STOCK_SO_SHA = '8dd9a961b9dca8566069a4f65b3ddee9c65682c4e9c91a6d41e3c5727b1d8b20'
STOCK_OVR_SHA = '26e9a216a710d42a303346a4ca5b84037ff38250ea725dc7112b055fcacada79'
VULKAN_BOOT = (0x18ddaf4, bytes.fromhex('200200b4'), bytes.fromhex('11000014'))    # cbz x0,0x18ddb38 -> b 0x18ddb38


def _find_built(*patterns):
    """First existing build output among `patterns`, each a glob relative to this file's directory or the two
    directory levels above it (the checkout root). Falls back to the first pattern's literal path (reported if missing)."""
    for root in (HERE, os.path.dirname(HERE), os.path.dirname(os.path.dirname(HERE))):
        for pat in patterns:
            hits = sorted(glob.glob(os.path.join(root, pat)))
            if hits:
                return hits[0]
    return os.path.join(HERE, patterns[0])


DEFAULT_PLUGIN = _find_built('*/rad-links/build/libradlinks.so')           # the rad-links Quest build output
DEFAULT_LOADER = _find_built('loader/build/libevrmod.so', '*/*/quest/build/libevrmod.so')   # the loader build output
LOADER_NAME, PLUGIN_PREFIX = 'libevrmod.so', 'libevrmod_'
# step 6 layout (file offsets of the stock libr15.so; LOAD0 has vaddr == offset, .dynamic is in LOAD1 at +0x1000)
DYN_OFF, DYN_SLOTS, DYN_FREE_SLOT = 0x36be210, 39, 34     # 35 entries used (34 = DT_NULL), slots 34..38 all zero
DYNSTR_SHNDX = 4                                           # the .dynstr section header (sh_link of .dynamic)
LOADER_STR = 0x2a760f0                                     # in sd's cave, above api's stubs, below the dead b target
SIG_NAME = 'EVRMOD'
ZIP_TIME = (2026, 10, 23, 0, 0, 0)

# (step, input sha256, output sha256) of libr15.so for the default chain, pinned from a --dry-run on the stock bytes
# A step whose input differs refuses: an earlier step changed, or the input is not stock.
PINS = {
    'vulkan-boot':    ('8dd9a961b9dca8566069a4f65b3ddee9c65682c4e9c91a6d41e3c5727b1d8b20',
                       '1a21dc4ec8563ee068e73d484ef374b68683de9dffabd7a3d2e1039d62e1c3ae'),
    'sdcard-config':  ('1a21dc4ec8563ee068e73d484ef374b68683de9dffabd7a3d2e1039d62e1c3ae',
                       'fca7d1054e23f40bc94c15d78514454a341098a1ed793e40ccb9b85659a8cbe6'),
    'api-config':     ('fca7d1054e23f40bc94c15d78514454a341098a1ed793e40ccb9b85659a8cbe6',
                       'c79ddd4681652ee497d55d279c214c2c58b7726582720adb7b859ec5be6417d6'),
    'store-api-path': ('c79ddd4681652ee497d55d279c214c2c58b7726582720adb7b859ec5be6417d6',
                       '90643f85fca3b87822516a27d63fa6c553dc6febf43d39f7143c744343964ee3'),
    'early-quit':     ('90643f85fca3b87822516a27d63fa6c553dc6febf43d39f7143c744343964ee3',
                       '2f1e9b961682c4ddd4ebe02bbd594e6fdfc00c785df279670a38bdea8666c92e'),
    'loader':         ('2f1e9b961682c4ddd4ebe02bbd594e6fdfc00c785df279670a38bdea8666c92e',
                       '5a6155e3948d4a19a7d0515e260de01fddce9a4eeed534c4a78fb17d9673958f'),
}
PNSOVR_OUT_SHA = 'f69e379d378a443fd2e42988313e36ad9327512a678e7668579fdf6f015c897c'


def sha(b):
    return hashlib.sha256(b).hexdigest()


def s_vulkan(buf, stock):
    va, old, new = VULKAN_BOOT
    return [('vulkan-boot', va, old, new)]


def s_sdcard(buf, stock):
    sd.check_cave_zero(bytes(buf))
    _, _, P = sd.patches(bytes(buf))
    return [(f'sdcard {n}', va, s, p) for n, va, s, p in P]


def s_api(buf, stock):
    _, _, _, P = api.patches(bytes(buf), stock)
    return [(f'api {n}', va, s, p) for n, va, s, p in P]


def s_store_path(buf, stock):
    assert list(api.STORE_API_PATH_PATCHES) == [0x126C21C], 'STORE_API_PATH_PATCHES changed: re-check step 4'
    return [('store-api-path', va, old, new) for va, (old, new) in api.STORE_API_PATH_PATCHES.items()]


def s_earlyquit(buf, stock):
    return [(f'earlyquit {n}', o, s, p) for n, o, s, p in eq.SITES]


def s_loader(buf, stock):
    """Append "libevrmod.so" to DT_NEEDED. bionic walks every DT_NEEDED up to DT_NULL in array order, so a NEEDED
    after RELACOUNT loads like one in the NEEDED block (last, after libc.so). The name lies past .dynstr, in the same
    R-X LOAD (vaddr == offset there), and bionic bounds a string index twice: ElfReader::get_string (load time,
    reading DT_NEEDED from the FILE) by the .dynstr SECTION HEADER's sh_size, and soinfo::get_string (after mapping)
    by DT_STRSZ. Both are widened to end at the name; neither is used as anything but that bound. The .dynamic edit
    is in RELRO, which the linker reads before it protects it."""
    tags = [struct.unpack_from('<QQ', stock, DYN_OFF + 16 * i) for i in range(DYN_SLOTS)]
    if tags[DYN_FREE_SLOT] != (0, 0) or any(t != (0, 0) for t in tags[DYN_FREE_SLOT:]):
        sys.exit('refusing: step 6 loader: .dynamic slots 34..38 are not the stock spare DT_NULLs')
    strtab = [v for t, v in tags if t == 5]
    strsz = [(i, v) for i, (t, v) in enumerate(tags) if t == 10]
    if strtab != [0x333fd8] or len(strsz) != 1 or strsz[0][1] != 0x83081f:
        sys.exit('refusing: step 6 loader: DT_STRTAB / DT_STRSZ are not the stock values')
    shoff, shentsize = struct.unpack_from('<Q', stock, 0x28)[0], struct.unpack_from('<H', stock, 0x3a)[0]
    sh_dynstr = shoff + DYNSTR_SHNDX * shentsize
    if struct.unpack_from('<IQQQQ', stock, sh_dynstr + 4) != (3, 2, 0x333fd8, 0x333fd8, 0x83081f):
        sys.exit('refusing: step 6 loader: section header 4 is not the stock .dynstr')
    name = LOADER_NAME.encode() + b'\0'
    assert api.RANGE_HI <= LOADER_STR and LOADER_STR + len(name) <= sd.DEAD_B + 0x1000 <= sd.CAVE_HI
    off = LOADER_STR - strtab[0]
    size = struct.pack('<Q', off + len(name))
    return [('loader name in cave', LOADER_STR, bytes(len(name)), name),
            ('loader DT_STRSZ', DYN_OFF + 16 * strsz[0][0] + 8, struct.pack('<Q', 0x83081f), size),
            ('loader .dynstr sh_size', sh_dynstr + 0x20, struct.pack('<Q', 0x83081f), size),
            ('loader DT_NEEDED', DYN_OFF + 16 * DYN_FREE_SLOT, bytes(16), struct.pack('<QQ', 1, off))]


LIBR15_STEPS = [('vulkan-boot', s_vulkan), ('sdcard-config', s_sdcard), ('api-config', s_api),
                ('store-api-path', s_store_path), ('early-quit', s_earlyquit), ('loader', s_loader)]


def apply_sites(buf, sites, applied):
    for n, va, s, p in sites:
        if len(s) != len(p):
            sys.exit(f'refusing: {n} @ {va:#x}: stock and patch lengths differ')
        if bytes(buf[va:va + len(s)]) != s:
            sys.exit(f'refusing: {n} @ {va:#x} is {bytes(buf[va:va + len(s)]).hex()}, not the stock {s.hex()}')
        buf[va:va + len(p)] = p
        applied.append((n, va, s, p))


def patch_libr15(stock, skip_loader, record):
    if sha(stock) != STOCK_SO_SHA:
        sys.exit(f'refusing: {SO_ENTRY} is not stock {BUILD} (sha256 {sha(stock)})')
    buf, applied = bytearray(stock), []
    for i, (name, fn) in enumerate(LIBR15_STEPS, 1):
        h_in = sha(buf)
        if name == 'loader' and skip_loader:
            record.append((i, name, 'SKIPPED', h_in, h_in, 0))
            continue
        pin = PINS.get(name)
        if pin and pin[0] != h_in:
            sys.exit(f'refusing: step {i} {name}: input sha256 {h_in}, pinned {pin[0]}')
        n0 = len(applied)
        apply_sites(buf, fn(buf, stock), applied)
        h_out = sha(buf)
        if pin and pin[1] != h_out:
            sys.exit(f'refusing: step {i} {name}: output sha256 {h_out}, pinned {pin[1]}')
        record.append((i, name, 'pinned' if pin else 'UNPINNED', h_in, h_out,
                       sum(len(p) for _, _, _, p in applied[n0:])))
    masked = bytearray(buf)
    for n, va, s, p in applied:
        masked[va:va + len(s)] = s
    if bytes(masked) != stock:
        sys.exit('refusing: bytes outside the patched ranges differ from stock')
    return bytes(buf), applied


def patch_pnsovr(data, record):
    if sha(data) != STOCK_OVR_SHA:
        sys.exit(f'refusing: {OVR_ENTRY} is not stock {BUILD} (sha256 {sha(data)})')
    out = pns.patch_libpnsovr(data)
    if sha(out) != PNSOVR_OUT_SHA:
        sys.exit(f'refusing: step 7 pnsovr-login: output sha256 {sha(out)}, pinned {PNSOVR_OUT_SHA}')
    record.append((7, 'pnsovr-login', 'pinned', sha(data), sha(out), 4 * len(pns.PNSOVR_CODE_PATCHES)))
    return out


def check_plugin(path):
    data = open(path, 'rb').read()
    if data[:4] != b'\x7fELF' or data[4] != 2 or int.from_bytes(data[16:18], 'little') != 3 \
            or int.from_bytes(data[18:20], 'little') != 183:
        sys.exit(f'refusing plugin {path}: not an aarch64 ELF shared object')
    return data


def ensure_keystore(path, storepass):
    if os.path.exists(path):
        return
    print(f'generating a new signing key on this machine: {path} (keep it: updates must be signed with it)')
    os.makedirs(os.path.dirname(os.path.abspath(path)), exist_ok=True)
    subprocess.run(['keytool', '-genkeypair', '-noprompt', '-storetype', 'PKCS12', '-keystore', path,
                    '-storepass', storepass, '-keypass', storepass, '-alias', 'evrmod', '-keyalg', 'RSA',
                    '-keysize', '3072', '-validity', '10000', '-dname', 'CN=echovr-modkit local signing key'],
                   check=True, timeout=120)


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0],
                                 formatter_class=argparse.RawDescriptionHelpFormatter, epilog=__doc__)
    ap.add_argument('--apk', required=True, help='your own stock store APK (vc 4987566)')
    ap.add_argument('--out', help='the APK to write')
    ap.add_argument('--keystore', help='PKCS12 signing key; generated with keytool if the file does not exist')
    ap.add_argument('--storepass', default='android', help='keystore password (default: android)')
    ap.add_argument('--plugin', action='append', default=None, metavar='SO',
                    help='plugin .so to add under lib/arm64-v8a/ (repeatable; default: rad-links)')
    ap.add_argument('--no-plugins', action='store_true', help='add no plugin library')
    ap.add_argument('--embed-config', metavar='JSON',
                    help='optional APK fallback config (assets/_local/config.json); /sdcard still wins')
    ap.add_argument('--skip-loader', action='store_true',
                    help='skip step 6: no libevrmod.so; plugins keep their names and ship inert (dev only)')
    ap.add_argument('--loader-so', default=DEFAULT_LOADER, metavar='SO',
                    help='the loader library (default: the build output of the loader, if found)')
    ap.add_argument('--dry-run', action='store_true', help='patch in memory and print the step table; write nothing')
    a = ap.parse_args()
    if not a.dry_run and not (a.out and a.keystore):
        sys.exit('--out and --keystore are required (or --dry-run)')
    src = os.path.realpath(a.apk)
    h = hashlib.sha256()
    with open(src, 'rb') as f:
        for chunk in iter(lambda: f.read(1 << 22), b''):
            h.update(chunk)
    print(f'input {src}  sha256 {h.hexdigest()}')
    if h.hexdigest() != STOCK_APK_SHA:
        sys.exit(f'refusing: not the stock store APK of Quest {BUILD} vc 4987566 (sha256 {STOCK_APK_SHA})')
    if a.out:
        sd.refuse_out(a.out, src)
    plugins = [] if a.no_plugins else [os.path.abspath(p) for p in (a.plugin or [DEFAULT_PLUGIN])]
    adds = {}
    if not a.skip_loader:
        adds[LIB_DIR + LOADER_NAME] = check_plugin(a.loader_so)
    for p in plugins:
        b = os.path.basename(p)
        if not a.skip_loader:
            stem = b[3:] if b.startswith('lib') else b
            b = stem if stem.startswith(PLUGIN_PREFIX) else PLUGIN_PREFIX + stem
        if LIB_DIR + b in adds:
            sys.exit(f'refusing: two plugins install as {LIB_DIR + b}')
        adds[LIB_DIR + b] = check_plugin(p)
    if a.embed_config:
        adds['assets/_local/config.json'] = open(a.embed_config, 'rb').read()
    record = []
    with zipfile.ZipFile(src) as zin:
        names = {i.filename for i in zin.infolist()}
        clash = sorted(set(adds) & names)
        if clash:
            sys.exit(f'refusing: would replace stock entries {clash}')
        so, applied = patch_libr15(zin.read(SO_ENTRY), a.skip_loader, record)
        ovr = patch_pnsovr(zin.read(OVR_ENTRY), record)
        for e, d in adds.items():
            record.append((8, 'plugin', e, '-', sha(d), len(d)))
        print(f'\n{"#":>2} {"step":<15} {"state":<14} {"in sha256":<16} {"out sha256":<64} bytes')
        for i, n, st, hi, ho, nb in record:
            print(f'{i:>2} {n:<15} {st:<14} {hi[:16]:<16} {ho:<64} {nb}')
        print(f'libr15.so: {len(applied)} sites, {sum(len(p) for *_, p in applied)} bytes; outside them = stock')
        if a.dry_run:
            return 0
        repl = {SO_ENTRY: so, OVR_ENTRY: ovr}
        keep = [i for i in zin.infolist() if not v1.is_signature_entry(i.filename)]
        entries = []                                   # pass 1: manifest digests, one entry in memory at a time
        for info in keep:
            if not info.is_dir():
                data = repl[info.filename] if info.filename in repl else zin.read(info.filename)
                entries.append((info.filename, v1.manifest_section(info.filename, data)))
                del data
        for e, d in adds.items():
            entries.append((e, v1.manifest_section(e, d)))
        main_attr = b'Manifest-Version: 1.0\r\nCreated-By: echovr-modkit quest_apk_build_release.py\r\n\r\n'
        manifest = main_attr + b''.join(s for _, s in entries)
        sf = v1.build_sf(manifest, [(None, main_attr), *entries])
        ensure_keystore(a.keystore, a.storepass)
        rsa = v1.sign_sf(sf, Path(a.keystore), a.storepass.encode())
        tmp = a.out + '.tmp'
        os.makedirs(os.path.dirname(os.path.abspath(a.out)), exist_ok=True)
        with zipfile.ZipFile(tmp, 'w') as zout:      # pass 2: write, one entry in memory at a time
            for name, data in (('META-INF/MANIFEST.MF', manifest), (f'META-INF/{SIG_NAME}.SF', sf),
                               (f'META-INF/{SIG_NAME}.RSA', rsa)):
                zi = zipfile.ZipInfo(name, ZIP_TIME)
                zi.compress_type, zi.external_attr = zipfile.ZIP_DEFLATED, 0o644 << 16
                zout.writestr(zi, data)
            for info in keep:
                zout.writestr(v1.clone_info(info), b'' if info.is_dir() else
                              (repl[info.filename] if info.filename in repl else zin.read(info.filename)))
            for e, d in adds.items():
                zi = zipfile.ZipInfo(e, ZIP_TIME)
                zi.compress_type = zipfile.ZIP_DEFLATED
                zi.external_attr = (0o755 if e.endswith('.so') else 0o644) << 16
                zout.writestr(zi, d)
    os.replace(tmp, a.out)
    h = hashlib.sha256()
    with open(a.out, 'rb') as f:
        for chunk in iter(lambda: f.read(1 << 22), b''):
            h.update(chunk)
    print(f'wrote {a.out} ({os.path.getsize(a.out)} B)  sha256 {h.hexdigest()}')
    return 0


if __name__ == '__main__':
    sys.exit(main())
