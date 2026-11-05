"""Explicit platform, layer, provenance, and safety context for resource codecs.

The Phase 1 API never infers a disk layout from a filename.  Callers construct a
``ResourceContext`` and pass it to the codec/kernel so diagnostics can identify the
resource class, platform, serialized layer, build, fixture, and source.
"""

from __future__ import annotations

from dataclasses import dataclass, field, replace
from enum import Enum
from pathlib import Path
from typing import Any, Callable


class Platform(str, Enum):
    """Supported serialized platform families."""

    WIN10_DX12 = "win10_dx12"
    ANDROID_VULKAN = "android_vulkan"
    WIN7_DX11 = "win7_dx11"

    @classmethod
    def coerce(cls, value: "Platform | str") -> "Platform":
        if isinstance(value, cls):
            return value
        return cls(value)


class Layer(str, Enum):
    """Serialized resource layer.

    ``SINGLE`` is for formats that do not have a Primary/GPU split.  A missing
    sibling is represented by the resolver returning ``None``; it is not guessed
    from a missing path.
    """

    PRIMARY = "Primary"
    GPU = "GPU"
    SINGLE = "single"

    @classmethod
    def coerce(cls, value: "Layer | str") -> "Layer":
        if isinstance(value, cls):
            return value
        return cls(value)


class LayerState(str, Enum):
    """How one Primary/GPU layer is stored for a resource identity."""

    PRESENT = "present"
    ABSENT = "absent"
    EMBEDDED = "embedded"

    @classmethod
    def coerce(cls, value: "LayerState | str") -> "LayerState":
        if isinstance(value, cls):
            return value
        return cls(value)


@dataclass(frozen=True, slots=True)
class LayerResource:
    """Explicit result for one Primary/GPU layer lookup.

    ``PRESENT`` owns a standalone byte stream. ``ABSENT`` is a confirmed missing
    sibling, not a failed lookup. ``EMBEDDED`` carries bytes sliced from the
    declared owner layer so an inline payload is never confused with absence.
    """

    resource_class: str
    symbol: int
    layer: Layer
    state: LayerState
    provenance: str
    data: bytes | None = None
    owner_layer: Layer | None = None

    def __post_init__(self) -> None:
        resource_class = self.resource_class.strip()
        if not resource_class:
            raise ValueError("resource_class must not be empty")
        object.__setattr__(self, "resource_class", resource_class)
        if not isinstance(self.symbol, int) or isinstance(self.symbol, bool):
            raise ValueError("symbol must be an integer")
        if not 0 <= self.symbol <= 0xFFFF_FFFF_FFFF_FFFF:
            raise ValueError("symbol must fit in an unsigned 64-bit value")
        layer = Layer.coerce(self.layer)
        if layer is Layer.SINGLE:
            raise ValueError("LayerResource requires Primary or GPU, not single")
        object.__setattr__(self, "layer", layer)
        state = LayerState.coerce(self.state)
        object.__setattr__(self, "state", state)
        if not self.provenance.strip():
            raise ValueError("provenance must not be empty")
        owner = None if self.owner_layer is None else Layer.coerce(self.owner_layer)
        object.__setattr__(self, "owner_layer", owner)

        if state is LayerState.PRESENT:
            if self.data is None:
                raise ValueError("present layer requires data")
            if owner is not None:
                raise ValueError("present layer must not declare an owner layer")
        elif state is LayerState.ABSENT:
            if self.data is not None or owner is not None:
                raise ValueError("absent layer must not carry data or an owner layer")
        else:
            if self.data is None:
                raise ValueError("embedded layer requires the embedded payload bytes")
            if owner not in (Layer.PRIMARY, Layer.GPU) or owner is layer:
                raise ValueError("embedded layer requires the other Primary/GPU owner")
        if self.data is not None:
            try:
                object.__setattr__(self, "data", bytes(self.data))
            except (TypeError, ValueError) as exc:
                raise TypeError("layer data must be bytes-like") from exc

    @classmethod
    def present(
        cls,
        resource_class: str,
        symbol: int,
        layer: Layer | str,
        data: bytes,
        *,
        provenance: str,
    ) -> "LayerResource":
        return cls(resource_class, symbol, Layer.coerce(layer), LayerState.PRESENT, provenance, data)

    @classmethod
    def absent(
        cls,
        resource_class: str,
        symbol: int,
        layer: Layer | str,
        *,
        provenance: str,
    ) -> "LayerResource":
        return cls(resource_class, symbol, Layer.coerce(layer), LayerState.ABSENT, provenance)

    @classmethod
    def embedded(
        cls,
        resource_class: str,
        symbol: int,
        layer: Layer | str,
        data: bytes,
        *,
        owner_layer: Layer | str,
        provenance: str,
    ) -> "LayerResource":
        return cls(
            resource_class,
            symbol,
            Layer.coerce(layer),
            LayerState.EMBEDDED,
            provenance,
            data,
            Layer.coerce(owner_layer),
        )


@dataclass(frozen=True, slots=True)
class ResourcePair:
    """One platform-specific resource identity with explicit Primary/GPU states."""

    resource_class: str
    symbol: int
    platform: Platform
    primary: LayerResource
    gpu: LayerResource

    def __post_init__(self) -> None:
        resource_class = self.resource_class.strip()
        if not resource_class:
            raise ValueError("resource_class must not be empty")
        object.__setattr__(self, "resource_class", resource_class)
        if not isinstance(self.symbol, int) or isinstance(self.symbol, bool):
            raise ValueError("symbol must be an integer")
        if not 0 <= self.symbol <= 0xFFFF_FFFF_FFFF_FFFF:
            raise ValueError("symbol must fit in an unsigned 64-bit value")
        object.__setattr__(self, "platform", Platform.coerce(self.platform))
        if not isinstance(self.primary, LayerResource) or not isinstance(
            self.gpu, LayerResource
        ):
            raise TypeError("resource pair members must be LayerResource values")
        if self.primary.layer is not Layer.PRIMARY or self.gpu.layer is not Layer.GPU:
            raise ValueError("resource pair requires Primary then GPU layer records")
        for member in (self.primary, self.gpu):
            if member.resource_class != resource_class or member.symbol != self.symbol:
                raise ValueError("resource pair layer identity mismatch")
        if (
            self.primary.state is LayerState.ABSENT
            and self.gpu.state is LayerState.ABSENT
        ):
            raise ValueError("resource pair cannot have both layers absent")
        if (
            self.primary.state is LayerState.EMBEDDED
            and self.gpu.state is LayerState.EMBEDDED
        ):
            raise ValueError("resource pair cannot contain an embedded ownership cycle")
        for member, owner in ((self.primary, self.gpu), (self.gpu, self.primary)):
            if member.state is LayerState.EMBEDDED and owner.state is LayerState.ABSENT:
                raise ValueError("embedded layer owner cannot be absent")

    def for_layer(self, layer: Layer | str) -> LayerResource:
        actual = Layer.coerce(layer)
        if actual is Layer.PRIMARY:
            return self.primary
        if actual is Layer.GPU:
            return self.gpu
        raise ValueError("resource pair has no single-file layer")


@dataclass(frozen=True, slots=True)
class ResourceLimits:
    """Per-operation safety ceilings.

    These defaults are deliberately finite but can be raised for a reviewed
    fixture through its context.  ``max_address`` also makes integer-overflow
    checks deterministic on Python, whose integers do not overflow naturally.
    """

    max_input_bytes: int = 1 << 32
    max_output_bytes: int = 1 << 32
    max_allocation_bytes: int = 64 << 20
    max_count: int = 1_000_000
    max_depth: int = 64
    max_address: int = (1 << 64) - 1

    def __post_init__(self) -> None:
        for name in (
            "max_input_bytes",
            "max_output_bytes",
            "max_allocation_bytes",
            "max_count",
            "max_depth",
            "max_address",
        ):
            value = getattr(self, name)
            if not isinstance(value, int) or isinstance(value, bool) or value < 0:
                raise ValueError(f"{name} must be a non-negative integer")


SiblingResolver = Callable[[str, int, Layer], LayerResource]
DiagnosticPolicy = Callable[[BaseException], Any]


@dataclass(frozen=True, slots=True)
class ResourceContext:
    """Identity and policy supplied explicitly to every Phase 1 codec call."""

    resource_class: str
    platform: Platform
    layer: Layer
    build_id: str | None = None
    fixture_id: str | None = None
    source_path: str | Path | None = None
    source_sha256: str | None = None
    limits: ResourceLimits = field(default_factory=ResourceLimits)
    sibling_resolver: SiblingResolver | None = field(
        default=None, repr=False, compare=False
    )
    diagnostic_policy: DiagnosticPolicy | None = field(
        default=None, repr=False, compare=False
    )

    def __post_init__(self) -> None:
        resource_class = self.resource_class.strip()
        if not resource_class:
            raise ValueError("resource_class must not be empty")
        object.__setattr__(self, "resource_class", resource_class)
        object.__setattr__(self, "platform", Platform.coerce(self.platform))
        object.__setattr__(self, "layer", Layer.coerce(self.layer))
        if self.source_sha256 is not None:
            digest = self.source_sha256.lower()
            if len(digest) != 64 or any(c not in "0123456789abcdef" for c in digest):
                raise ValueError("source_sha256 must be a 64-character hexadecimal digest")
            object.__setattr__(self, "source_sha256", digest)

    def with_fixture(
        self,
        fixture_id: str,
        *,
        source_path: str | Path | None = None,
        source_sha256: str | None = None,
    ) -> "ResourceContext":
        """Return a context specialized to one manifest fixture."""

        changes: dict[str, object] = {"fixture_id": fixture_id}
        if source_path is not None:
            changes["source_path"] = source_path
        if source_sha256 is not None:
            changes["source_sha256"] = source_sha256
        return replace(self, **changes)

    def resolve_sibling(
        self,
        resource_class: str,
        symbol: int,
        layer: Layer | str,
        *,
        required: bool,
    ) -> LayerResource:
        """Resolve one layer without conflating absent, embedded, and lookup failure."""

        from .errors import CorruptResourceError, UnresolvedDependencyError

        if not isinstance(resource_class, str) or not resource_class.strip():
            raise ValueError("resource_class must be a non-empty string")
        resource_class = resource_class.strip()
        if not isinstance(symbol, int) or isinstance(symbol, bool):
            raise ValueError("symbol must be an integer")
        if not 0 <= symbol <= 0xFFFF_FFFF_FFFF_FFFF:
            raise ValueError("symbol must fit in an unsigned 64-bit value")
        if not isinstance(required, bool):
            raise ValueError("required must be a boolean")
        actual_layer = Layer.coerce(layer)
        if actual_layer is Layer.SINGLE:
            raise ValueError("a Primary/GPU sibling cannot use the single-file layer")
        if self.sibling_resolver is None:
            raise UnresolvedDependencyError(
                "no sibling resolver is configured",
                context=self,
                details={
                    "target_class": resource_class,
                    "symbol": f"0x{symbol:016x}",
                    "target_layer": actual_layer.value,
                },
            )
        result = self.sibling_resolver(resource_class, symbol, actual_layer)
        if not isinstance(result, LayerResource):
            raise CorruptResourceError(
                "sibling resolver must return an explicit LayerResource state",
                context=self,
                details={"resolver_result": type(result).__name__},
            )
        if (
            result.resource_class != resource_class
            or result.symbol != symbol
            or result.layer is not actual_layer
        ):
            raise CorruptResourceError(
                "sibling resolver returned a mismatched identity",
                context=self,
                details={
                    "target_class": resource_class,
                    "target_symbol": f"0x{symbol:016x}",
                    "target_layer": actual_layer.value,
                },
            )
        if required and result.state is LayerState.ABSENT:
            raise UnresolvedDependencyError(
                "required sibling is explicitly absent",
                context=self,
                details={
                    "target_class": resource_class,
                    "symbol": f"0x{symbol:016x}",
                    "target_layer": actual_layer.value,
                    "provenance": result.provenance,
                },
            )
        return result

    def diagnostic_fields(self) -> tuple[tuple[str, str], ...]:
        """Stable key order used by structured error rendering."""

        fields = [
            ("class", self.resource_class),
            ("platform", self.platform.value),
            ("layer", self.layer.value),
        ]
        if self.build_id is not None:
            fields.append(("build", str(self.build_id)))
        if self.fixture_id is not None:
            fields.append(("fixture", self.fixture_id))
        if self.source_path is not None:
            fields.append(("source", str(self.source_path)))
        return tuple(fields)
