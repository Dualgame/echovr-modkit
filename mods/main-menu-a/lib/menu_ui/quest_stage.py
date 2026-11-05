#!/usr/bin/env python3
"""Stage the PC "Main Menu A" loose tree on the Quest.

ffa_build.sh (PC final 6323983201049540, level mnu_master, Win10 type dirs) into the Quest's mnu_master_lowspec
(Quest final 6528386917200980, Android type dirs), then ship every file through the Quest language package.

  quest_stage.py selftest                      # Route B on STOCK PC mnu_master (+ mp_ingame) == stock Quest _lowspec (13/13 each)
  quest_stage.py transform SRC OUT             # PC tree -> Quest tree (refuses an existing OUT)

The only PC-specific bytes are the level-root symbols (recipe section 1):
  CGSceneResource     u64 mnu_master_radlvl 0x2d4ee93ed76dabce -> mnu_master_lowspec_radlvl 0x1aa25241a46fba06 (x1)
  CActorDataResource  u64 0xf93979258e3bf9a6 -> 0x1bbb5ab09c479250 (x2); one copy is a key of the sorted
                      (key u64, value u64) table at +0x310 (count = u64 @ +0x28), ordered as SIGNED i64: re-sorted.
Every other class and every CUICanvasResource is copied unchanged under the Quest type dir of the same class.
0xe962a897e2cb8f07 (Quest final ships it; the 11 other TREE_CLASSES records are byte-identical PC <-> Quest):
  CGSceneResource     u64 mnu_master_mp_ingame_radlvl 0x3a3bb177e613d560 -> _lowspec_radlvl 0x4155f004c615e286 (x1,
                      at an unaligned offset, +0x538c)
  CActorDataResource  u64 0xe97ca699f2da9910 -> 0xa8e0b65e6786d09e (x2; names not in hash_lookup), same re-sort.
`selftest` proves both levels (13/13 each); a tree may carry either or both levels (LEVELS)."""
import argparse, os, struct, sys

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0, os.path.join(REPO, 'lib'))

PC_FILE, Q_FILE = 0xac360e41e4ede056, 0xf927772b9e2aefb1
RADLVL = (0x2d4ee93ed76dabce, 0x1aa25241a46fba06)
ROOTSYM = (0xf93979258e3bf9a6, 0x1bbb5ab09c479250)
MP_FILE, MPQ_FILE = 0x4d82118c7c91b6bb, 0xe962a897e2cb8f07
LEVELS = {PC_FILE: (Q_FILE, RADLVL, ROOTSYM),
          MP_FILE: (MPQ_FILE, (0x3a3bb177e613d560, 0x4155f004c615e286), (0xe97ca699f2da9910, 0xa8e0b65e6786d09e))}
TABLE_COUNT_OFF = 0x28
TREE_CLASSES = ('CR15UIPage2CR', 'CR15UIPage2ElementCR', 'CScriptCR', 'CTransformCR', 'CCanvasUICR', 'CBlackboardCR',
                'CBoundingSphereCR', 'CR15PointerMeshCR', 'CR15VRMenuWidgetCR', 'CTextureOverrideCR',
                'CR15PointerInterfaceCR', 'CGSceneResource', 'CActorDataResource')
LANG, BASE = '2b47aab238f60515', '48037dc70b0ecab2'
CANVAS_Q = 0xa39ab682e439b36a


def u64(x):
    return struct.pack('<Q', x)


def swap_once(data, old, new, want):
    n = data.count(u64(old))
    if n != want:
        raise ValueError(f'expected {want} x {old:#018x}, found {n}')
    return data.replace(u64(old), u64(new))


def find_key_table(data, key):
    """(start, count) of the signed-sorted (key, value) table that holds `key` and has count = u64 @ +0x28."""
    count = struct.unpack_from('<Q', data, TABLE_COUNT_OFF)[0]
    k = lambda o: struct.unpack_from('<q', data, o)[0]
    for pos in (i for i in range(0, len(data) - 7, 8) if data.startswith(u64(key), i)):
        s = pos
        while s - 16 >= 0 and k(s - 16) < k(s):
            s -= 16
        e = pos
        while e + 24 <= len(data) and k(e + 16) > k(e):
            e += 16
        if (e - s) // 16 + 1 == count:
            return s, count
    raise ValueError(f'no sorted key table of {count} holding {key:#018x}')


def fix_actordata(data, rootsym=ROOTSYM):
    s, n = find_key_table(data, rootsym[0])
    out = bytearray(swap_once(data, *rootsym, 2))
    pairs = sorted((struct.unpack_from('<qQ', out, s + 16 * i) for i in range(n)), key=lambda p: p[0])
    for i, p in enumerate(pairs):
        struct.pack_into('<qQ', out, s + 16 * i, *p)
    return bytes(out)


def fix_bytes(cls, data, level=PC_FILE):
    """Route B byte fix of one PC level record (`level` = PC level file id, a LEVELS key)."""
    _, radlvl, rootsym = LEVELS[level]
    if cls == 'CActorDataResource':
        return fix_actordata(data, rootsym)
    if cls == 'CGSceneResource':
        return swap_once(data, *radlvl, 1)
    return data


TYPEMAP_FILE = os.path.join(HERE, 'quest_typemap.tsv')


def _load_typemap(path=TYPEMAP_FILE):
    """(m, qtypes) from the static data file, or None when it is missing. Rows: `T layer pc_sym class q_sym q_dir`
    (PC Win10 type symbol -> class, Quest Android type symbol, Quest asset-type dir) and `Q q_sym` (Quest type dirs)."""
    if not os.path.exists(path):
        return None
    m, qtypes = {}, set()
    for ln in open(path):
        f = ln.split()
        if f[:1] == ['T']:
            m[(f[1], int(f[2], 16))] = (f[3], int(f[4], 16), int(f[5], 16))
        elif f[:1] == ['Q']:
            qtypes.add(int(f[1], 16))
    return m, qtypes


def _basis_typemap():
    """(m, qtypes) read live from the game install (hub only; `--regen-typemap` writes it to TYPEMAP_FILE)."""
    from game_store import Basis
    pc, q = Basis.pc_final(), Basis.quest_final()
    qt, qtypes, m = {}, set(), {}
    for (layer, cls), cell in q.cells().items():
        for r in cell.values():
            qt[(layer, cls)] = (r.type_symbol, r.asset_type)
            qtypes.add(r.type_symbol); break
    for (layer, cls), cell in pc.cells().items():
        for r in cell.values():
            if (layer, cls) in qt:
                m[(layer, r.type_symbol)] = (cls,) + qt[(layer, cls)]
            break
    return m, qtypes


def regen_typemap(path=TYPEMAP_FILE):
    m, qtypes = _basis_typemap()
    with open(path, 'w') as f:
        for (layer, pt), (cls, qs, qd) in sorted(m.items(), key=lambda kv: (str(kv[0][0]), kv[0][1])):
            f.write(f'T\t{layer}\t{pt:#018x}\t{cls}\t{qs:#018x}\t{qd:#x}\n')
        for qs in sorted(qtypes):
            f.write(f'Q\t{qs:#018x}\n')
    return len(m), len(qtypes)


def typemap_data():
    return _load_typemap() or _basis_typemap()


class TypeMap:
    """PC Win10 type symbol -> (class, Quest Android type symbol, Quest asset-type dir); static file, basis fallback."""

    def __init__(self):
        self.m = typemap_data()[0]

    @property
    def pc(self):
        from game_store import Basis
        return Basis.pc_final()

    @property
    def q(self):
        from game_store import Basis
        return Basis.quest_final()

    def __call__(self, layer, pctype):
        if (layer, pctype) not in self.m:
            raise KeyError(f'no Quest twin for {layer} type {pctype:#018x}')
        return self.m[(layer, pctype)]


def tree_files(root):
    for dp, _, fs in os.walk(root):
        for f in fs:
            rel = os.path.relpath(os.path.join(dp, f), root).split(os.sep)
            if len(rel) != 4 or not rel[2].startswith('v'):
                raise ValueError(f'unexpected tree path {"/".join(rel)}')
            yield rel[0], int(rel[1], 16), int(rel[2][1:]), int(rel[3], 16), os.path.join(dp, f)


def transform(src, out, tm=None):
    if os.path.exists(out):
        sys.exit(f'refusing: {out} exists')
    tm = tm or TypeMap()
    rows = []
    for layer, t, v, fid, path in sorted(tree_files(src)):
        cls, qt, qv = tm(layer, t)
        if v != qv:
            sys.exit(f'{cls}: asset type v{v} on PC, v{qv} on Quest')
        data = open(path, 'rb').read()
        nfid = LEVELS[fid][0] if fid in LEVELS else fid
        if fid in LEVELS:
            if cls not in TREE_CLASSES:
                sys.exit(f'refusing: {cls} {fid:016x} is not a verified Route B class')
            data = fix_bytes(cls, data, fid)
        d = os.path.join(out, layer, f'{qt:016x}', f'v{qv}')
        os.makedirs(d, exist_ok=True)
        open(os.path.join(d, f'{nfid:016x}'), 'wb').write(data)
        rows.append((cls, nfid, len(data)))
    for cls, nfid, n in rows:
        print(f'{cls:28s} {nfid:016x} {n}')
    print(f'{len(rows)} files -> {out}')
    return rows


def selftest():
    tm = TypeMap()
    ok = 0
    for pf, (qf, _, _) in LEVELS.items():
        for cls in TREE_CLASSES:
            a = tm.pc.read('primary', cls, pf)
            b = tm.q.read('primary', cls, qf)
            same = a is not None and fix_bytes(cls, a, pf) == b
            ok += same
            print(f'{"OK " if same else "BAD"} {pf:016x} {cls:28s} pc {len(a) if a else None} quest {len(b) if b else None}')
    want = len(LEVELS) * len(TREE_CLASSES)
    print(f'selftest: {ok}/{want} identical')
    return 0 if ok == want else 1



def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('action', choices=['selftest', 'transform', 'regen-typemap'])
    ap.add_argument('paths', nargs='*')
    a = ap.parse_args(argv)
    if a.action == 'selftest':
        return selftest()
    if a.action == 'regen-typemap':
        print('wrote %d type rows, %d Quest type dirs -> %s' % (*regen_typemap(), TYPEMAP_FILE))
        return 0
    transform(*a.paths)
    return 0


if __name__ == '__main__':
    sys.exit(main())
