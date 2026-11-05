"""CGSceneResource (v3 Primary) — disassembly-confirmed CGSceneData member walk.

The Primary payload is `NRadEngine::CGSceneData::Serialize(CSerializer&)`
(Quest libr15.so build 3137595842946788 @0x227e3d8): a 22-member,
declaration-order walk. The whole serializer tree was disassembled; this is a
genuine count/size-driven decoder (it replaces an earlier 704-byte constant template).

Every member is a named primitive whose on-disk form is fully confirmed by disasm
of each `*::Serialize` (vtable slots: +0x90 = write u32 count, +0x88 = bulk record
bytes, +0x98/+0xa8 = begin/end-field tags that emit NO bytes):

  CTable<T> / CTableXT<T>      [u32 count][count * stride(T)]   (empty = [u32 0];
                              the bulk path is skipped when count==0 — cbz w8)
  CSimpleKeyTable<K,_,V>       CTable<CSimpleKey<K,V>> (combined key+value records)
                              + a single u32 CFlagsT  (empty = 8 bytes = the same
                              [u32 0 count][u32 0 flags]).  Disasm-corrected:
                              the bucket/hash index is rebuilt IN MEMORY on read
                              (CTable::GrowBy), NOT serialized; an earlier "two parallel
                              CTables" model fit only the empty form (see keytable()).
  CMemBlock                    [u32 size][size bytes]            (verbatim heap)
  CMemBlock (bulk, ivdata)     [u32 size][u64 attach][size bytes]
  inline scalar                u32        CBox  6*f32

THE HEAD CRUX resolved: member 9 `shapelist` IS a `CMemBlock`
(`CMemBlock::Serialize` @0x1385210 in the member sequence) — the per-scene attach
heap that carries the baked node-graph table (the 24B lookup records + i64 relative
offsets + verts). It is carried VERBATIM, segmented by its own `[u32 size]` (exactly
like the mesh module carries CGMeshData/CGVertexFormat records). So the CGraphT
topology that stalled an earlier pass never needs byte-level decoding here: on disk it lives
inside the shapelist CMemBlock. Members 10-14 (shapelists/nodegraph/sound) are the
lightweight index tables; they are empty in the N=9 class and small in N=17/19.

The 22-member sequence (disasm order = on-disk order):
  1 lights 2 vlights 3 avolumes 4 dirlightdirs 5 dirlightindices  (CTable, empty)
  6 ivdata = bulk{ CTable + 4*CTable<CSymbol64> + 2*CMemBlock }     (empty 44B)
  7 particleeffects(CTable)  bonecount(u32)  boundingboxidx(u32)  8 actors(CTable)
  9 shapelist(CMemBlock)  10 shapelists(SShapeLists)  11 nodegraph(SNodeGraph)
  12 soundvolumelist 13 soundportallist 14 soundportalgrouplist
  15 materials(CTable<CSymbol64>) 16 shadersets(CTable<SGMeshShaderSet> st24)
  flags(u32) padding(u32)  17 boundingbox(CBox 6f)  18 sets(CGSceneSetsData)
  19 aovolumes 20 jointoffsets 21 boxtree(bulk) 22 debugdata(3*CTable<CSymbol64>)

Scope: byte-exact on 612/626 (612 of 626 by the count/size walk, before the later additions below). The version-head prefix (N fixed 360B
instance-header blocks; N=word[0], VHEAD_SIG @word[3]/[4]) is peeled verbatim by
_vhead_len() before the same member graph .
Three sub-decodes (DWARF byte_size of the runtime struct = CTable bulk-copy stride,
+ disasm), lifting 591->603:
  (1) LEAD-member strides (lights/vlights/avolumes / ivdata.volumes / actors / 3 sound
      lists) pinned from DWARF (SGLightParams 360 etc.) — +5 small scenes.
  (2) CEDGE 16->8 (DWARF; nodegraph edges empty in per-prop so never exercised) — +4.
  (3) ivdata = 5 CTables + 4 plain CMemBlocks ([u32 size][bytes]) + an 8B bulk-scope
      close marker (disasm CGIrradianceVolumesData::Serialize; an earlier "2 bulk blobs
      w/ u64 attach" only fit the all-zero empty form) — +3.

Then the CSimpleKeyTable non-empty form (+9 whole-map scenes). An earlier
"two parallel CTables (keys + values)" model was byte-exact only for the EMPTY case
(both [u32 0]). Disasm of CSimpleKeyTable<u64,_,CSymbol64>::Serialize @0x3adecfc =
inner CTable<CSimpleKey<K,V>>::Serialize (this+0) + Serialize<u32,1>(CFlagsT&) (this+0x38);
the inner @0x3adee58 writes [u32 count] + count*stride records, then rebuilds the hash
bucket IN MEMORY (CTable::GrowBy — no stream bytes). So on disk a non-empty map is just
[u32 count][count*stride][u32 flags] (an earlier "[u32 1] bucket" after celebration's 311
records is the flags). CSimpleKey<u64,CSymbol64>=16 (probe-confirmed; the finding's "24B"
was a misread), <u32,u32>=8, <u32,CSymbol64>=16.

Then the SShapeListT<SSplineData>/<CConvex> member-10 data tables (+4 files:
arena_a, lobby_b_arena, lobby_b_combat, tutorial_arena — the spline-bearing scenes with EMPTY
sound lists). Disasm: SShapeListT<T>::Serialize -> CTableXT<T>::Serialize (CSerializerEnableBulk
wrap, NO on-disk trailer) -> SerializeArray<T> -> T::Serialize, per element. SSplineData (232B)
= u32 + CTable<float>(knots) + CTable<C3Vector>(controlpoints) + u32 + CTable<C3Vector>(samples)
+ CTable<C3Vector>(derivatives) [the 2 u32 are the +0x40 inline scalars]. CConvex (80B) =
CTable<CPlane>(faces, stride 16) + 2 C3Vector — but CONVEX lists are EMPTY across the whole
shipped corpus, so its element body is disasm-derived and unexercised on disk.

Last, the member-12/13/14 SOUND lists (the last 10 files), so the scene type is
now COMPLETE. All three are structs-OF-CTables (per-element Serialize), NOT the flat PODs the
DWARF byte_size values implied (304/128/56 were RUNTIME sizes). See _soundvol_elem_read /
_portalgroup_elem_read + the stride-128 member-13 ctable:
  * member 12 soundvolumelist  CTableXT<SGSoundVolumeData> — 9-field element (name/nodeid/reverb/
    priority/portalprog/ambient/switches/volumes/connectiondata). The `volumes` CTableXT<CConvex>
    appends a +4 trailer PER CONVEX (the convex element, now stream-validated); the
    element itself has no trailer.
  * member 13 soundportallist  CTable<SGSoundPortalData> — FLAT stride 128 (124 fields + 4 trailer).
  * member 14 soundportalgrouplist CTableXT<SGSoundPortalGroupData> — element = CTable<u64>, no trailer.
The +4 (always 0) is written by the container per-element loop (SerializeArray, GOT-resolved);
empirically pinned + export-validated. Only CConvex/SGSoundPortalData carry it; SGSoundVolumeData/
SAmbientInfo/SSplineData do not.

Evidence: `DWARF-confirmed` (libr15.so 3137595842946788) struct sizes + `ELF-dynsym
confirmed` disasm of CGSceneData/CGIrradianceVolumesData/CGraphT/SNodeGraph/CSimpleKeyTable/
CTable<CSimpleKey>/SShapeListT/SSplineData/CConvex/SGSoundVolumeData/SGSoundPortalData/
SGSoundPortalGroupData::Serialize; `stream-confirmed` + `export-validated` byte-exact
round-trip on 626/626.
"""
import struct

STRUCTURED = True

C3VECTOR = 12
U32 = 4
CSYMBOL64 = 8
CVERTEX = 16
CEDGE = 8
CPAIR_U32 = 8
CSIMPLEKEY_U64_SYM = 16
CSIMPLEKEY_U32_SYM = 16
CSIMPLEKEY_U32_U32 = 8
SGMESHSHADERSET = 24
SSET = 16
SGROUP = 48
CTRANSFQ = 32
CSPHERE = 16
COBB = 40
CPLANE = 16
SGINSTANCEDATA = 168
SGLIGHTPARAMS = 360
SGVOLLIGHTPARAMS = 296
SGATMOVOLPARAMS = 120
SGIRRADVOLDATA = 120
SGACTORDATA = 56
SREVERBBUSINFO = 8
SSOUNDSWITCHDATA = 8
SGSOUNDVOLCONN = 16
CSOUNDSYMBOL = 4
U64 = 8
SGSOUNDPORTALDATA = 128
SGAOVOLUME = 112
CBOXTREE_SNODE = 32

VHEAD_BLOCK = 360
VHEAD_SIG = (0x0127f3c0, 0x314bca1c)


def _vhead_len(buf):
    """Length of the version-head prefix, or 0 if this is a plain zero-head scene.
    RAISE-safe: only fires on the exact landmark; a wrong body still fails the EOF check."""
    if len(buf) < 20:
        return 0
    n, sig0, sig1 = struct.unpack_from("<I", buf, 0)[0], \
        struct.unpack_from("<I", buf, 12)[0], struct.unpack_from("<I", buf, 16)[0]
    if n == 0 or (sig0, sig1) != VHEAD_SIG:
        return 0
    plen = n * VHEAD_BLOCK
    if plen >= len(buf):
        raise AssertionError(f"vhead {n}*{VHEAD_BLOCK} >= len {len(buf)}")
    return plen


class _R:
    """Forward byte cursor; every consume is count/size-driven and records are
    carried verbatim. RAISES on any structural surprise (the gate)."""
    __slots__ = ("b", "o")

    def __init__(self, b):
        self.b = b
        self.o = 0

    def u32(self):
        v = struct.unpack_from("<I", self.b, self.o)[0]
        self.o += 4
        return v

    def take(self, n):
        if n < 0 or self.o + n > len(self.b):
            raise AssertionError(f"take {n} past end @0x{self.o:x}/{len(self.b)}")
        s = self.b[self.o:self.o + n]
        self.o += n
        return s

    def ctable(self, stride):
        """[u32 count][count*stride] -> (count, raw_records)."""
        c = self.u32()
        if c and stride == 0:
            raise AssertionError(f"non-empty table count={c} but stride unknown @0x{self.o:x}")
        return c, self.take(c * stride)

    def keytable(self, kstride):
        """CSimpleKeyTable<K,_,V> on disk = CTable<CSimpleKey<K,V>> (the combined
        key+value records) followed by a single u32 CFlagsT — NOT two parallel CTables.

        Disasm (libr15.so 3137595842946788): CSimpleKeyTable<...>::Serialize @0x3adecfc
        calls the inner CTable<CSimpleKey<K,V>>::Serialize (this+0) then
        Serialize<u32,1>(CFlagsT<u32>&) (this+0x38). The inner CTable::Serialize
        @0x3adee58 writes [u32 count] (vtable+0x90) + count*stride bulk records
        (vtable+0x88); everything after is the in-memory hash-bucket rebuild
        (CTable::GrowBy — NO stream bytes), so the bucket index is NOT on disk.
        CSimpleKey<u64,CSymbol64> stride = 16 (probe-confirmed across 9 whole-map
        scenes), CSimpleKey<u32,u32> = 8, CSimpleKey<u32,CSymbol64> = 16.

        An earlier "two parallel CTables (keys + values)" model was byte-exact only for
        the EMPTY case: [u32 0 count][u32 0 flags] = the same 8 bytes. Non-empty maps
        broke it (the second u32 is the flags, never a values count)."""
        c = self.u32()
        if c and kstride == 0:
            raise AssertionError(f"non-empty keytable count={c} but stride unknown @0x{self.o:x}")
        rec = self.take(c * kstride)
        flags = self.u32()
        return (c, rec, flags)

    def memblock(self):
        """non-bulk CMemBlock [u32 size][size bytes]."""
        sz = self.u32()
        return sz, self.take(sz)

    def memblock_bulk(self):
        """bulk CMemBlock [u32 size][u64 attach][size bytes] (ivdata sample blobs)."""
        sz = self.u32()
        attach = struct.unpack_from("<Q", self.b, self.o)[0]
        self.o += 8
        return sz, attach, self.take(sz)


SHAPELIST_KINDS = ("c3vector", "ctransfq", "csphere", "cobb", "spline", "convex")
_SHAPELIST_POD_STRIDE = {"c3vector": C3VECTOR, "ctransfq": CTRANSFQ,
                         "csphere": CSPHERE, "cobb": COBB}


def _spline_elem_read(r):
    """One SSplineData (SSplineData::Serialize @0x3c1d60c, libr15 3137595842946788):
    u32 + CTable<float>(knots) + CTable<C3Vector>(controlpoints) + u32 +
    CTable<C3Vector>(samples) + CTable<C3Vector>(derivatives). The 2 u32 are the
    +0x40 inline scalars (one leading, one between controlpoints and samples)."""
    s0 = r.u32()
    knots = r.ctable(U32)
    controlpoints = r.ctable(C3VECTOR)
    s1 = r.u32()
    samples = r.ctable(C3VECTOR)
    derivatives = r.ctable(C3VECTOR)
    return (s0, knots, controlpoints, s1, samples, derivatives)


def _convex_elem_read(r):
    """One CConvex: CTable<CPlane>(faces, stride 16) + 2 C3Vector + a 4-byte per-element
    trailer (=0). CConvex::Serialize @0x185ae18 itself writes only faces + 2 C3Vector; the
    +4 is written by the container's per-element loop (SerializeArray<CConvex>, GOT-resolved).
    An earlier pass left this trailer off because member-10 convex shape lists are EMPTY corpus-wide; it is now
    stream-validated via the soundvol `volumes` path (combustion has a 9-convex element;
    the +4 sits between adjacent convexes). Captured verbatim so a non-zero trailer round-trips."""
    planes = r.ctable(CPLANE)
    c0 = r.take(C3VECTOR)
    c1 = r.take(C3VECTOR)
    trailer = r.take(4)
    return (planes, c0, c1, trailer)


def _ambient_elem_read(r):
    """One SAmbientInfo (CTableXT<SAmbientInfo> element): CSoundSymbol(4) + real(4) +
    CTable<CSoundSymbol>. No per-element trailer (unlike CConvex; air_brake stream-confirmed)."""
    head = r.take(8)
    inner = r.ctable(CSOUNDSYMBOL)
    return (head, inner)


def _soundvol_elem_read(r):
    """One SGSoundVolumeData (SGSoundVolumeData::Serialize @0x227c880): 9 fields in
    declaration order — name/nodeid (CSymbol64), reverb (CTable st8), priority+portalprog
    (8 inline bytes), ambient (CTableXT<SAmbientInfo>), switches (CTable st8), volumes
    (CTableXT<CConvex>, each convex carrying its +4 trailer), connectiondata (CTable st16).
    The element itself has NO trailer (fission: the next element's nodeid follows the last
    connectiondata record immediately)."""
    head = r.take(16)
    reverb = r.ctable(SREVERBBUSINFO)
    scalars = r.take(8)
    amb_c = r.u32()
    ambient = [_ambient_elem_read(r) for _ in range(amb_c)]
    switches = r.ctable(SSOUNDSWITCHDATA)
    vol_c = r.u32()
    volumes = [_convex_elem_read(r) for _ in range(vol_c)]
    conn = r.ctable(SGSOUNDVOLCONN)
    return dict(head=head, reverb=reverb, scalars=scalars, ambient=ambient,
                switches=switches, volumes=volumes, conn=conn)


def _portalgroup_elem_read(r):
    """One SGSoundPortalGroupData (SGSoundPortalGroupData::Serialize @0x227d888): a single
    CTable<u64> (the member portal indices). No per-element trailer (combustion: 3 groups
    [0,1]/[2,3]/[4,5,6] index its 7 portals with materials immediately after)."""
    return r.ctable(U64)


def _shapelist(r, kind):
    """SShapeListT<T> = CSimpleKeyTable + CSimpleKeyTable + CTableXT<T> (the data table).
    For spline/convex the data table is a per-element CTableXT (no bulk trailer — the
    CSerializerEnableBulk dtor writes 0 bytes; probe-confirmed to EOF)."""
    kt0 = r.keytable(CSIMPLEKEY_U64_SYM)
    kt1 = r.keytable(CSIMPLEKEY_U64_SYM)
    if kind == "spline":
        c = r.u32()
        data = (c, [_spline_elem_read(r) for _ in range(c)])
    elif kind == "convex":
        c = r.u32()
        data = (c, [_convex_elem_read(r) for _ in range(c)])
    else:
        data = r.ctable(_SHAPELIST_POD_STRIDE[kind])
    return (kind, kt0, kt1, data)


def _nodegraph(r):
    """SNodeGraph = CGraphT + roots + lookup + types + names."""
    verts = r.ctable(CVERTEX)
    sedges = r.ctable(CEDGE)
    tedges = r.ctable(CEDGE)
    srcleafs = r.keytable(CSIMPLEKEY_U32_U32)
    tgtleafs = r.keytable(CSIMPLEKEY_U32_U32)
    idxremap = r.ctable(U32)
    deadlist = r.ctable(CPAIR_U32)
    inline4 = struct.unpack_from("<4I", r.b, r.o)
    r.o += 16
    roots = r.ctable(U32)
    lookup = r.keytable(CSIMPLEKEY_U64_SYM)
    types = r.ctable(U32)
    names = r.ctable(CSYMBOL64)
    return dict(verts=verts, sedges=sedges, tedges=tedges, srcleafs=srcleafs,
                tgtleafs=tgtleafs, idxremap=idxremap, deadlist=deadlist,
                inline4=inline4, roots=roots, lookup=lookup, types=types, names=names)


def read(buf):
    r = _R(buf)

    vlen = _vhead_len(buf)
    vhead = r.take(vlen)

    lead = []
    for stride in (SGLIGHTPARAMS, SGVOLLIGHTPARAMS, SGATMOVOLPARAMS, C3VECTOR, U32):
        lead.append(r.ctable(stride))

    iv_tables = [r.ctable(SGIRRADVOLDATA if i == 0 else CSYMBOL64) for i in range(5)]
    iv_blobs = [r.memblock() for _ in range(4)]
    iv_bulk_trailer = r.take(8)

    particleeffects = r.ctable(SGINSTANCEDATA)
    bonecount = r.u32()
    boundingboxidx = r.u32()
    actors = r.ctable(SGACTORDATA)

    shapelist = r.memblock()

    shapelists = [_shapelist(r, kind) for kind in SHAPELIST_KINDS]

    nodegraph = _nodegraph(r)

    sv_c = r.u32()
    soundvol = (sv_c, [_soundvol_elem_read(r) for _ in range(sv_c)])
    soundportal = r.ctable(SGSOUNDPORTALDATA)
    spg_c = r.u32()
    soundportalgroup = (spg_c, [_portalgroup_elem_read(r) for _ in range(spg_c)])

    materials = r.ctable(CSYMBOL64)
    shadersets = r.ctable(SGMESHSHADERSET)
    flags = r.u32()
    padding = r.u32()
    bbox = r.take(24)

    sets_set = r.ctable(SSET)
    sets_setnames = r.keytable(CSIMPLEKEY_U32_SYM)
    sets_groups = r.ctable(SGROUP)
    sets_groupnames = r.keytable(CSIMPLEKEY_U32_SYM)

    aovolumes = r.ctable(SGAOVOLUME)
    jointoffsets = r.ctable(CTRANSFQ)
    boxtree = r.ctable(CBOXTREE_SNODE)
    debugdata = [r.ctable(CSYMBOL64) for _ in range(3)]

    if r.o != len(buf):
        raise AssertionError(f"did not consume to EOF: 0x{r.o:x} != 0x{len(buf):x}")

    return dict(vhead=vhead, lead=lead, iv_tables=iv_tables, iv_blobs=iv_blobs,
                iv_bulk_trailer=iv_bulk_trailer,
                particleeffects=particleeffects, bonecount=bonecount,
                boundingboxidx=boundingboxidx, actors=actors, shapelist=shapelist,
                shapelists=shapelists, nodegraph=nodegraph, soundvol=soundvol,
                soundportal=soundportal, soundportalgroup=soundportalgroup,
                materials=materials, shadersets=shadersets, flags=flags,
                padding=padding, bbox=bbox, sets_set=sets_set,
                sets_setnames=sets_setnames, sets_groups=sets_groups,
                sets_groupnames=sets_groupnames, aovolumes=aovolumes,
                jointoffsets=jointoffsets, boxtree=boxtree, debugdata=debugdata)


def _wtable(out, t):
    out += struct.pack("<I", t[0])
    out += t[1]


def _wkeytable(out, kt):
    out += struct.pack("<I", kt[0]); out += kt[1]
    out += struct.pack("<I", kt[2])


def _wspline_elem(out, e):
    s0, knots, controlpoints, s1, samples, derivatives = e
    out += struct.pack("<I", s0)
    _wtable(out, knots); _wtable(out, controlpoints)
    out += struct.pack("<I", s1)
    _wtable(out, samples); _wtable(out, derivatives)


def _wconvex_elem(out, e):
    planes, c0, c1, trailer = e
    _wtable(out, planes); out += c0; out += c1; out += trailer


def _wambient_elem(out, e):
    head, inner = e
    out += head; _wtable(out, inner)


def _wsoundvol_elem(out, e):
    out += e["head"]
    _wtable(out, e["reverb"])
    out += e["scalars"]
    out += struct.pack("<I", len(e["ambient"]))
    for a in e["ambient"]:
        _wambient_elem(out, a)
    _wtable(out, e["switches"])
    out += struct.pack("<I", len(e["volumes"]))
    for cv in e["volumes"]:
        _wconvex_elem(out, cv)
    _wtable(out, e["conn"])


def _wsoundvol(out, sv):
    count, elems = sv
    out += struct.pack("<I", count)
    for e in elems:
        _wsoundvol_elem(out, e)


def _wportalgroup(out, spg):
    count, groups = spg
    out += struct.pack("<I", count)
    for g in groups:
        _wtable(out, g)


def _wshapelist(out, sl):
    kind, kt0, kt1, data = sl
    _wkeytable(out, kt0); _wkeytable(out, kt1)
    if kind == "spline":
        out += struct.pack("<I", data[0])
        for e in data[1]:
            _wspline_elem(out, e)
    elif kind == "convex":
        out += struct.pack("<I", data[0])
        for e in data[1]:
            _wconvex_elem(out, e)
    else:
        _wtable(out, data)


def write(obj):
    out = bytearray()
    out += obj["vhead"]
    for t in obj["lead"]:
        _wtable(out, t)
    for t in obj["iv_tables"]:
        _wtable(out, t)
    for sz, blob in obj["iv_blobs"]:
        out += struct.pack("<I", sz); out += blob
    out += obj["iv_bulk_trailer"]
    _wtable(out, obj["particleeffects"])
    out += struct.pack("<II", obj["bonecount"], obj["boundingboxidx"])
    _wtable(out, obj["actors"])
    out += struct.pack("<I", obj["shapelist"][0]); out += obj["shapelist"][1]
    for sl in obj["shapelists"]:
        _wshapelist(out, sl)
    ng = obj["nodegraph"]
    for k in ("verts", "sedges", "tedges"):
        _wtable(out, ng[k])
    _wkeytable(out, ng["srcleafs"]); _wkeytable(out, ng["tgtleafs"])
    _wtable(out, ng["idxremap"]); _wtable(out, ng["deadlist"])
    out += struct.pack("<4I", *ng["inline4"])
    _wtable(out, ng["roots"]); _wkeytable(out, ng["lookup"])
    _wtable(out, ng["types"]); _wtable(out, ng["names"])
    _wsoundvol(out, obj["soundvol"])
    _wtable(out, obj["soundportal"])
    _wportalgroup(out, obj["soundportalgroup"])
    _wtable(out, obj["materials"]); _wtable(out, obj["shadersets"])
    out += struct.pack("<II", obj["flags"], obj["padding"])
    out += obj["bbox"]
    _wtable(out, obj["sets_set"]); _wkeytable(out, obj["sets_setnames"])
    _wtable(out, obj["sets_groups"]); _wkeytable(out, obj["sets_groupnames"])
    _wtable(out, obj["aovolumes"]); _wtable(out, obj["jointoffsets"])
    _wtable(out, obj["boxtree"])
    for t in obj["debugdata"]:
        _wtable(out, t)
    return bytes(out)


def roundtrip(buf):
    return write(read(buf))
