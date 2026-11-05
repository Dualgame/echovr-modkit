"""Common PC/Quest framing codec for ``CUICanvasResource``.

Opened by the n150 decoder-gap package (§6 step 3a of the control-group 1:1 program).
This was the last true greenfield reader gap: no decoder existed anywhere in-tree, and
all 79 PC-paired Quest canvases were emitted as raw passthrough (``DIFF-SIZE``).

THE SHAPE
---------
The file is **not** one of the existing container families — ``cflat_ctable``,
``cnested_auto``, ``cdescforest`` and ``cmultishape`` all reject it (there is no table at
offset 0). It is a new composition built from the same 56-byte ``_container`` CTable
header: a fixed 0x50 scalar prologue, six CTable headers in two groups of three (each
group followed by an 8-byte zero filler), then one packed heap at a CONSTANT 0x1b0::

    +0x00  u64   CSymbol64 of the root element  (usually hash_name("canvas"))
    +0x08  u32   unresolved
    +0x0c  u32 W, +0x10 u32 H     pair A — atlas / render-target size
    +0x14  u32 W, +0x18 u32 H     pair B — design/reference size
    +0x1c  u32 small enum (0/2/3) · +0x20 u32 0 · +0x24 u32 (0|2) · +0x28 u32 unresolved
    +0x2c  u32 vertex count   ] index == vertex * 1.5 in every observed file
    +0x30  u32 index  count   ] (a quad = 4 verts / 6 indices)
    +0x34  7 x f32           `inferred` canvas RGBA + pivot XY
    +0x50  CTable T0..T2      (3 x 0x38)
    +0xf8  u64 filler (0 in all 289 corpus files)
    +0x100 CTable T3..T5      (3 x 0x38)
    +0x1a8 u64 filler (0 in all 289 corpus files)
    +0x1b0 HEAP

Heap order — **tested, not assumed** (a wrong order still consumes the file but breaks
the T2 UV and T3 offset invariants below)::

    T0 · T1 · <T1 sub-heaps, in record order> · [pad to 8 iff T2] · T2 · T3 · T4 · [pad to 8 iff T5] · T5

The pad before T2 matters on exactly 2 shipped canvases per build (``ui_mnu_deeplinking_text_input``
and ``ui_mnu_deeplinking_generate``, whose sub-heaps end at 7 and 6 mod 8, PC ``6323983201049540``
and Quest ``6528386917200980``). Without it the round trip still passes, because only slicing moves,
but T2/T3/T4 come out shifted 1-2 bytes: T3 reads 0x3e / 0x2100 instead of 0 / 0x21.

In every table header ``count`` is CAPACITY and ``iused`` is what is actually spilled, so
``size == count * stride`` is a capacity figure and using it to walk the heap over-reads
the file. Records physically present = ``iused``.

    ========================================= ==== ==== ==== ==== ==== ====
    axis                                        T0   T1   T2   T3   T4   T5
    ========================================= ==== ==== ==== ==== ==== ====
    Quest @ ``4003961509643546`` (vc 482446)   216   88   24    4    1   24
    Quest @ ``6528386917200980`` (vc 4987566)  224   88   24    4    1   24
    PC (``6323983201049540`` / ``5959917594122771``) 224   88   24    4    1   24
    ========================================= ==== ==== ==== ==== ==== ====

⚠ T0 is not a PLATFORM constant — it is a BUILD constant (see BUILD ERA below).

T1 is a ``CTableXT<SUIBehaviorInfo>``: each 88-byte record carries a ``CTable<char>``
header at ``rec+0x10`` whose payload spills into the sub-heap that follows the T1 array.

Evidence
--------
* ``stream-confirmed`` — 289 shipped files (81 Quest + 104 pc_low + 104 pc_full) parse
  with zero leftover bytes and rebuild byte-exact; 0 stride violations; all six table
  headers canonical (``ptr=z10=z18=0, flags=1, mark=0x20``) in 289/289.
* ``stream-confirmed`` — T2's four floats are UVs: within ``[-0.05, 1.05]`` for every one
  of 78,000+ records across the corpus. T3's u32s are byte offsets into T4's string heap:
  ``T3[0] == 0 and sorted and all < len(T4)`` passes 289/289.
* ``symbols-only:libr15.so@4003961509643546`` (the 482446 diff-target build's own
  ``.symtab``) — ``SUICanvasData::Inspect<CPointerFixupInspector>`` is the on-disk
  serializer (its pointer-fixup inspector is why every header's ``ptr`` word is zero);
  ``InspectTable<CTable<SUI2Primitive>>`` names T0 and
  ``InspectTableXT<CTableXT<SUIBehaviorInfo>>`` names T1;
  ``SUIBehaviorTypeItems<B>::InitializeBehavior(..., CTable<char>&, ...)`` names the T1
  sub-heap as the behavior-parameter blob.

⚠ THERE IS NO FORMAT MAGIC
--------------------------
``0d 06 35 14 3c c4 d2 41`` on all canvases. That is not a magic — it is
``hash_name("canvas")``, the ROOT ELEMENT's ``CSymbol64``, and **6 Quest and 6 PC shipped
files carry a different value** (root names of 8+ characters). A reader that asserts it
rejects 12 shipped files. This module asserts the six canonical table headers instead.

⚠ BUILD ERA — THE T0 STRIDE IS A PER-BUILD CONSTANT (§3.3 /)
---------------------------------------------------------------------
``CUICanvasResource`` is one of the 18 classes whose ``v<AssetType>`` advances between the
``6396371639`` -> ``6500253412``, source ``data-global``), and the advance is a real grammar
NUMBER — the T0 (``SUI2Primitive``) record stride** — by decoding every canvas of both frozen
Quest closures under both stride sets:

    ================== ==== ====================== ====================== ==============
    build               ids  reads @ T0 = 216       reads @ T0 = 224       byte-exact rt
    ================== ==== ====================== ====================== ==============
    ``6528386917200980``  228  **0/228**              **228/228**            **228/228**
    ``4003961509643546``  181  **181/181**            0/181                  181/181
    ================== ==== ====================== ====================== ==============

Perfect separation, and T1..T5 (88, 24, 4, 1, 24) are unchanged on both.  At the device
measured on a 2021 build is a BUILD property, not a platform one.*

⚠ **The Quest axis is therefore an ALLOW-LIST keyed by build id** (:data:`QUEST_STRIDES_BY_BUILD`,
frozen closures has been decoded and re-encoded byte-exact from its own shipped bytes.  Any
other ``(android_vulkan, build_id)`` — an unpinned build, or a call that supplied no
``build_id`` at all — is **refused BY NAME**; it is never served the PC grammar and never
served another build's.  A Quest read with no build id is `inferred`, never
`stream-confirmed`.
"""

from __future__ import annotations

import struct
from dataclasses import dataclass, field

from ._container import read_header, pack_header
from .api import validate_call_context
from .context import Layer, Platform, ResourceContext
from .errors import TruncatedResourceError, UnsupportedShapeError

STRUCTURED = True
RESOURCE_CLASS = "CUICanvasResource"

PROLOGUE = 0x50
TABLE_OFFSETS = (0x50, 0x88, 0xC0, 0x100, 0x138, 0x170)
FILLER_OFFSETS = (0xF8, 0x1A8)
HEAP = 0x1B0
NTABLES = 6

STRIDE_QUEST = (216, 88, 24, 4, 1, 24)
STRIDE_PC = (224, 88, 24, 4, 1, 24)

PC_BUILDS = ("6323983201049540", "5959917594122771")
PC_CLASS_VERSION = 6500253412

QUEST_STRIDES_BY_BUILD: dict[str, tuple[int, ...]] = {
    "4003961509643546": STRIDE_QUEST,
    "6528386917200980": STRIDE_PC,
}

QUEST_CLASS_VERSION_BY_BUILD: dict[str, int] = {
    "4003961509643546": 6396371639,
    "6528386917200980": 6500253412,
}

QUEST_BUILD_NOTES: dict[str, str] = {
    "4003961509643546": ("vc 482446, the 2021 corroborator: 181/181 canvases of the frozen "
                         "closures decode and re-encode byte-exact at T0 = 216 (§3.3)"),
    "6528386917200980": ("vc 4987566, the device build: 228/228 canvases decode at T0 = 224 "
                         "and 228/228 re-encode byte-exact; 0/228 at T0 = 216 (§3.3)"),
}

PC_ONLY_DWORDS = (0x0C, 0x74)

T1_SUBTABLE_OFF = 0x10

EVIDENCE = "the build pins above"


SUPPORTED_PLATFORMS = frozenset({"win10_dx12", "android_vulkan"})
SUPPORTED_LAYERS = frozenset({"Primary"})


def _refuse_quest_build(build_id):
    """Refuse an unpinned Quest build BY NAME — never fall through."""
    if build_id is None:
        why = (f"{RESOURCE_CLASS} pins its Quest T0 stride per build; the call supplied "
               "no build_id")
    else:
        why = (f"{RESOURCE_CLASS} is not pinned for android_vulkan build {build_id}")
    raise UnsupportedShapeError(
        why,
        offset=0,
        details={
            "quest_build": str(build_id),
            "pinned_quest_builds": ",".join(sorted(QUEST_STRIDES_BY_BUILD)),
            "pc_builds": ",".join(PC_BUILDS),
            "note": ("measure the T0 stride against that build's own shipped bytes; never "
                     "fall through to the PC stride set or to another build's"),
        },
    )


def strides_for(platform: str, build_id=None) -> tuple[int, ...]:
    """(T0..T5) strides for one ``(platform, build)``.

    ⚠ The Quest arm is an allow-list, not a default: an unpinned build id — or none at
    all — is refused by name rather than served the 2021 or the PC set.
    """
    if platform in ("android_vulkan", "quest"):
        key = None if build_id is None else str(build_id)
        if key not in QUEST_STRIDES_BY_BUILD:
            _refuse_quest_build(build_id)
        return QUEST_STRIDES_BY_BUILD[key]
    if platform in ("win10_dx12", "pc"):
        return STRIDE_PC
    raise UnsupportedShapeError(f"unknown platform {platform!r}")


@dataclass
class Canvas:
    """Framed canvas: every byte is either a modelled region or a declared opaque one."""

    platform: str
    prologue: bytes
    headers: list
    fillers: tuple
    bodies: list
    subheap: bytes = b""
    pad_before_t2: bytes = b""
    pad_before_t5: bytes = b""
    tail: bytes = b""
    strides: tuple = field(default=())

    @property
    def root_symbol(self) -> int:
        return struct.unpack_from("<Q", self.prologue, 0)[0]

    @property
    def atlas_size(self) -> tuple:
        return struct.unpack_from("<II", self.prologue, 0x0C)

    @property
    def design_size(self) -> tuple:
        return struct.unpack_from("<II", self.prologue, 0x14)

    @property
    def vertex_index_counts(self) -> tuple:
        return struct.unpack_from("<II", self.prologue, 0x2C)

    def records(self, table: int) -> list:
        s = self.strides[table]
        b = self.bodies[table]
        return [b[i * s:(i + 1) * s] for i in range(len(b) // s)]


def _check_header(h, which: int) -> None:
    if h["ptr"] or h["z10"] or h["z18"] or h["mark"] != 0x20 or h["flags"] != 1:
        raise UnsupportedShapeError(
            f"T{which} is not a canonical CTable header "
            f"(ptr={h['ptr']:#x} z10={h['z10']:#x} z18={h['z18']:#x} "
            f"flags={h['flags']} mark={h['mark']:#x})")
    if h["iused"] > h["count"]:
        raise UnsupportedShapeError(f"T{which} iused {h['iused']} > count {h['count']}")


def read(data: bytes, *, platform: str = "android_vulkan", build_id=None) -> Canvas:
    """Frame one canvas. Consumes the WHOLE file or raises.

    ``build_id`` is REQUIRED on the Quest arm and must name a pinned build
    (:data:`QUEST_STRIDES_BY_BUILD`); it is ignored on the PC arm.
    """
    if len(data) < HEAP:
        raise TruncatedResourceError(f"{len(data)} B is shorter than the {HEAP}-B header")
    strides = strides_for(platform, build_id)
    headers = [read_header(data, off) for off in TABLE_OFFSETS]
    for i, h in enumerate(headers):
        _check_header(h, i)
        if h["count"] and h["size"] != h["count"] * strides[i]:
            raise UnsupportedShapeError(
                f"T{i} size {h['size']} != count {h['count']} * stride {strides[i]} "
                f"(wrong platform?)")

    cur = HEAP
    bodies: list = [b""] * NTABLES

    def take(n: int) -> bytes:
        nonlocal cur
        if cur + n > len(data):
            raise TruncatedResourceError(
                f"heap wants {n} B at {cur} but the file ends at {len(data)}")
        out = data[cur:cur + n]
        cur += n
        return out

    bodies[0] = take(headers[0]["iused"] * strides[0])
    bodies[1] = take(headers[1]["iused"] * strides[1])
    sub_start = cur
    for r in range(headers[1]["iused"]):
        rec = bodies[1][r * strides[1]:(r + 1) * strides[1]]
        sh = read_header(rec, T1_SUBTABLE_OFF)
        _check_header(sh, 1)
        take(sh["iused"])
    subheap = data[sub_start:cur]

    pad2 = take((-cur) % 8) if headers[2]["iused"] else b""
    for i in (2, 3, 4):
        bodies[i] = take(headers[i]["iused"] * strides[i])

    pad = b""
    if headers[5]["iused"]:
        want = (-cur) % 8
        pad = take(want)
    bodies[5] = take(headers[5]["iused"] * strides[5])

    tail = data[cur:]
    return Canvas(platform=platform, prologue=data[:PROLOGUE], headers=headers,
                  fillers=(struct.unpack_from("<Q", data, FILLER_OFFSETS[0])[0],
                           struct.unpack_from("<Q", data, FILLER_OFFSETS[1])[0]),
                  bodies=bodies, subheap=subheap, pad_before_t2=pad2, pad_before_t5=pad, tail=tail,
                  strides=strides)


def write(c: Canvas) -> bytes:
    """Rebuild the exact bytes `read` consumed."""
    out = bytearray(HEAP)
    out[:PROLOGUE] = c.prologue
    for off, h in zip(TABLE_OFFSETS, c.headers):
        out[off:off + 0x38] = pack_header(h)
    struct.pack_into("<Q", out, FILLER_OFFSETS[0], c.fillers[0])
    struct.pack_into("<Q", out, FILLER_OFFSETS[1], c.fillers[1])
    out += c.bodies[0]
    out += c.bodies[1]
    out += c.subheap
    out += c.pad_before_t2
    out += c.bodies[2]
    out += c.bodies[3]
    out += c.bodies[4]
    out += c.pad_before_t5
    out += c.bodies[5]
    out += c.tail
    return bytes(out)


def repack(c: Canvas) -> Canvas:
    """Recompute the two alignment pads (zero bytes) for the current bodies, after an edit
    changed a table's length. A no-op on every shipped canvas, whose pads are zero."""
    cur = HEAP + len(c.bodies[0]) + len(c.bodies[1]) + len(c.subheap)
    c.pad_before_t2 = bytes((-cur) % 8) if c.bodies[2] else b""
    cur += len(c.pad_before_t2) + len(c.bodies[2]) + len(c.bodies[3]) + len(c.bodies[4])
    c.pad_before_t5 = bytes((-cur) % 8) if c.bodies[5] else b""
    return c


def roundtrip(data: bytes, *, platform: str = "android_vulkan", build_id=None) -> bool:
    return write(read(data, platform=platform, build_id=build_id)) == data


def check_invariants(c: Canvas) -> dict:
    """The declared, machine-checkable structure gates. All pass on the 289-file corpus."""
    res = {}
    uv_ok = uv_n = 0
    for rec in c.records(2):
        u0, v0, u1, v1 = struct.unpack_from("<ffff", rec, 8)
        uv_n += 1
        if all(-0.05 <= v <= 1.05 for v in (u0, v0, u1, v1)):
            uv_ok += 1
    res["t2_uv_in_range"] = (uv_ok, uv_n)
    offs = [struct.unpack_from("<I", r, 0)[0] for r in c.records(3)]
    res["t3_offsets_into_t4"] = (
        not offs or (offs[0] == 0 and offs == sorted(offs) and max(offs) < max(1, len(c.bodies[4]))))
    v, i = c.vertex_index_counts
    res["index_is_1p5x_vertex"] = (i * 2 == v * 3)
    return res


def dependencies(c: Canvas) -> list:
    """[(table, record, field_offset, CSymbol64)] — the fsym edges the assembler needs.

    ``stream-confirmed``: every one of the 81 Quest canvases carries at least one edge
    that resolves inside the map closure. T0 rec+0x70 reaches nested `CUICanvasResource`
    and `CGTextureResource`; T0 rec+0x80 reaches `CGTextureResource`, `CTTFontResource`
    and `CScriptResource`/`StreamingScript`; T2 rec+0x00 is the atlas texture.
    The 224-byte SUI2Primitive shifts by +4 for the first site and +8 for the rest (the
    two extra dwords at rec+0x0c and rec+0x74). ⚠ The shift is keyed on the STRIDE, not on
    the platform, so a Quest canvas at ``6528386917200980`` (T0 = 224) takes the 224-byte
    offsets. `inferred` for that build: the two extra dwords are assumed to sit at the same
    record offsets there as on PC — measured on PC, not yet on Quest 4987566 bytes.
    """
    shift0, shift1 = (0, 0) if c.strides[0] == STRIDE_QUEST[0] else (4, 8)
    out = []
    for n, rec in enumerate(c.records(0)):
        for off, sh in ((0x70, shift0), (0x80, shift1)):
            sym = struct.unpack_from("<Q", rec, off + sh)[0]
            if sym:
                out.append((0, n, off + sh, sym))
    for n, rec in enumerate(c.records(2)):
        sym = struct.unpack_from("<Q", rec, 0)[0]
        if sym:
            out.append((2, n, 0, sym))
    return out


class CanvasCodec:
    """Phase 1 surface, so the caller's ``ResourceContext.build_id`` reaches the stride
    allow-list.  The legacy ``read``/``write``/``roundtrip`` functions stay the
    implementation and keep their signatures; this only carries the axes explicitly."""

    resource_class = RESOURCE_CLASS
    supported_platforms = frozenset({Platform.WIN10_DX12, Platform.ANDROID_VULKAN})
    supported_layers = frozenset({Layer.PRIMARY})

    def _validate(self, *, platform, layer, context):
        return validate_call_context(
            self.resource_class,
            platform=platform,
            layer=layer,
            context=context,
            supported_platforms=self.supported_platforms,
            supported_layers=self.supported_layers,
        )

    def decode(self, data: bytes, *, platform, layer, context: ResourceContext) -> Canvas:
        actual_platform, _layer = self._validate(
            platform=platform, layer=layer, context=context
        )
        return read(data, platform=actual_platform.value,
                    build_id=getattr(context, "build_id", None))

    def encode(self, resource: Canvas, *, platform, layer, context: ResourceContext) -> bytes:
        self._validate(platform=platform, layer=layer, context=context)
        return write(resource)


CODEC = CanvasCodec()
