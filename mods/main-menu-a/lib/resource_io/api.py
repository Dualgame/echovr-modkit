"""Approved Phase 1 codec surface and adapters for existing ``resource_io`` modules."""

from __future__ import annotations

import struct
from dataclasses import dataclass, field
from enum import Enum
from types import ModuleType
from typing import Any, Protocol, runtime_checkable

from .context import Layer, Platform, ResourceContext
from .errors import (
    ConversionPolicyRequiredError,
    CorruptResourceError,
    ResourceIOError,
    UnsupportedPlatformError,
    UnsupportedShapeError,
)


class CapabilityStatus(str, Enum):
    NOT_STARTED = "not-started"
    PARTIAL = "partial"
    PASS = "pass"
    NOT_APPLICABLE = "n/a"
    BLOCKED = "blocked"


CAPABILITY_NAMES = (
    "identify",
    "frame",
    "decode",
    "encode_pc",
    "encode_quest",
    "author",
    "convert",
    "close",
    "runtime",
)


@dataclass(frozen=True, slots=True)
class Capability:
    status: CapabilityStatus = CapabilityStatus.NOT_STARTED
    evidence: tuple[str, ...] = ()

    def __post_init__(self) -> None:
        object.__setattr__(self, "status", CapabilityStatus(self.status))
        evidence = (self.evidence,) if isinstance(self.evidence, str) else tuple(self.evidence)
        if any(not isinstance(link, str) or not link.strip() for link in evidence):
            raise ValueError("capability evidence links must be non-empty strings")
        object.__setattr__(self, "evidence", evidence)
        if self.status is not CapabilityStatus.NOT_STARTED and not self.evidence:
            raise ValueError(f"capability {self.status.value} requires an evidence link")


@dataclass(frozen=True, slots=True)
class CodecCapabilities:
    """Independent capability axes; no stronger state is inferred from import."""

    identify: Capability = field(default_factory=Capability)
    frame: Capability = field(default_factory=Capability)
    decode: Capability = field(default_factory=Capability)
    encode_pc: Capability = field(default_factory=Capability)
    encode_quest: Capability = field(default_factory=Capability)
    author: Capability = field(default_factory=Capability)
    convert: Capability = field(default_factory=Capability)
    close: Capability = field(default_factory=Capability)
    runtime: Capability = field(default_factory=Capability)
    notes: tuple[str, ...] = ()

    def __post_init__(self) -> None:
        notes = (self.notes,) if isinstance(self.notes, str) else tuple(self.notes)
        if any(not isinstance(note, str) or not note.strip() for note in notes):
            raise ValueError("capability notes must be non-empty strings")
        object.__setattr__(self, "notes", notes)

    def as_dict(self) -> dict[str, dict[str, object]]:
        return {
            name: {
                "status": getattr(self, name).status.value,
                "evidence": list(getattr(self, name).evidence),
            }
            for name in CAPABILITY_NAMES
        }


@dataclass(frozen=True, slots=True)
class Dependency:
    """One resource edge with its semantic origin and layer requirement."""

    target_class: str
    symbol: int
    layer: Layer
    required: bool
    origin: str

    def __post_init__(self) -> None:
        if not self.target_class.strip():
            raise ValueError("target_class must not be empty")
        if not isinstance(self.symbol, int) or isinstance(self.symbol, bool):
            raise ValueError("symbol must be an integer")
        if not 0 <= self.symbol <= 0xFFFF_FFFF_FFFF_FFFF:
            raise ValueError("symbol must fit in an unsigned 64-bit value")
        object.__setattr__(self, "layer", Layer.coerce(self.layer))
        if not isinstance(self.required, bool):
            raise ValueError("required must be a boolean")
        if not self.origin.strip():
            raise ValueError("origin must name the field or table that created the edge")


@runtime_checkable
class ResourceCodec(Protocol):
    resource_class: str

    def decode(
        self,
        data: bytes,
        *,
        platform: Platform,
        layer: Layer,
        context: ResourceContext,
    ) -> object: ...

    def encode(
        self,
        resource: object,
        *,
        platform: Platform,
        layer: Layer,
        context: ResourceContext,
    ) -> bytes: ...

    def convert(
        self,
        resource: object,
        *,
        source_platform: Platform,
        target_platform: Platform,
        policy: object | None,
        context: ResourceContext,
    ) -> object: ...

    def dependencies(
        self,
        resource: object,
        *,
        platform: Platform,
        layer: Layer,
        context: ResourceContext,
    ) -> list[Dependency]: ...

    def capabilities(self) -> CodecCapabilities: ...


def validate_call_context(
    resource_class: str,
    *,
    platform: Platform | str,
    layer: Layer | str,
    context: ResourceContext,
    supported_platforms: frozenset[Platform] | None = None,
    supported_layers: frozenset[Layer] | None = None,
) -> tuple[Platform, Layer]:
    """Validate duplicate call axes so no implicit layout selection can occur."""

    try:
        actual_platform = Platform.coerce(platform)
    except ValueError as exc:
        raise UnsupportedPlatformError(
            f"unknown platform {platform!r}", context=context
        ) from exc
    try:
        actual_layer = Layer.coerce(layer)
    except ValueError as exc:
        raise UnsupportedPlatformError(
            f"unknown serialized layer {layer!r}", context=context
        ) from exc
    if resource_class != context.resource_class:
        raise CorruptResourceError(
            f"codec class {resource_class} does not match context class "
            f"{context.resource_class}",
            context=context,
        )
    if actual_platform is not context.platform or actual_layer is not context.layer:
        raise CorruptResourceError(
            "call platform/layer do not match the explicit resource context",
            context=context,
            details={
                "call_platform": actual_platform.value,
                "call_layer": actual_layer.value,
            },
        )
    if supported_platforms is not None and actual_platform not in supported_platforms:
        raise UnsupportedPlatformError(
            f"{resource_class} does not support {actual_platform.value}",
            context=context,
            details={
                "supported_platforms": ",".join(sorted(p.value for p in supported_platforms))
            },
        )
    if supported_layers is not None and actual_layer not in supported_layers:
        raise UnsupportedPlatformError(
            f"{resource_class} does not support layer {actual_layer.value}",
            context=context,
            details={"supported_layers": ",".join(sorted(layer.value for layer in supported_layers))},
        )
    return actual_platform, actual_layer


class LegacyModuleAdapter:
    """Compatibility wrapper for existing PC ``read``/``write`` modules.

    The adapter is deliberately limited to explicitly requested Win10 Primary
    bytes by default.  Importability does not promote any Phase 0 capability cell;
    each module still needs the new fixture gates before stronger metadata is set.
    Legacy assertion/value failures are reported as unsupported shapes because the
    old modules cannot reliably distinguish corruption from an unmodeled grammar.
    """

    def __init__(
        self,
        resource_class: str,
        module: ModuleType | Any,
        *,
        supported_platforms: frozenset[Platform] = frozenset({Platform.WIN10_DX12}),
        supported_layers: frozenset[Layer] = frozenset({Layer.PRIMARY}),
        capability_metadata: CodecCapabilities | None = None,
    ) -> None:
        self.resource_class = resource_class
        self.module = module
        self.supported_platforms = frozenset(
            Platform.coerce(platform) for platform in supported_platforms
        )
        self.supported_layers = frozenset(Layer.coerce(layer) for layer in supported_layers)
        self._capabilities = capability_metadata or CodecCapabilities(
            notes=(
                "legacy compatibility adapter; Phase 1 fixture validation not yet promoted",
            )
        )

    def _validate(
        self,
        *,
        platform: Platform | str,
        layer: Layer | str,
        context: ResourceContext,
    ) -> tuple[Platform, Layer]:
        return validate_call_context(
            self.resource_class,
            platform=platform,
            layer=layer,
            context=context,
            supported_platforms=self.supported_platforms,
            supported_layers=self.supported_layers,
        )

    def _legacy_failure(self, action: str, exc: BaseException, context: ResourceContext) -> None:
        if isinstance(exc, ResourceIOError):
            raise exc
        if isinstance(exc, (AssertionError, ValueError, IndexError, KeyError, struct.error)):
            raise UnsupportedShapeError(
                f"legacy {action} rejected this shape: {exc}",
                context=context,
                details={"legacy_error": type(exc).__name__},
            ) from exc
        raise exc

    def decode(
        self,
        data: bytes,
        *,
        platform: Platform,
        layer: Layer,
        context: ResourceContext,
    ) -> object:
        self._validate(platform=platform, layer=layer, context=context)
        reader = getattr(self.module, "read", None)
        if reader is None:
            raise UnsupportedShapeError(
                f"legacy module for {self.resource_class} has no read()",
                context=context,
            )
        try:
            return reader(data)
        except Exception as exc:
            self._legacy_failure("decode", exc, context)
        raise AssertionError("unreachable")

    def encode(
        self,
        resource: object,
        *,
        platform: Platform,
        layer: Layer,
        context: ResourceContext,
    ) -> bytes:
        self._validate(platform=platform, layer=layer, context=context)
        writer = getattr(self.module, "write", None)
        if writer is None:
            raise UnsupportedShapeError(
                f"legacy module for {self.resource_class} has no write()",
                context=context,
            )
        try:
            return bytes(writer(resource))
        except Exception as exc:
            self._legacy_failure("encode", exc, context)
        raise AssertionError("unreachable")

    def roundtrip(
        self,
        data: bytes,
        *,
        platform: Platform,
        layer: Layer,
        context: ResourceContext,
    ) -> bytes:
        resource = self.decode(data, platform=platform, layer=layer, context=context)
        return self.encode(resource, platform=platform, layer=layer, context=context)

    def convert(
        self,
        resource: object,
        *,
        source_platform: Platform,
        target_platform: Platform,
        policy: object | None,
        context: ResourceContext,
    ) -> object:
        raise ConversionPolicyRequiredError(
            "legacy adapters do not infer or perform platform conversion",
            context=context,
            details={
                "source_platform": Platform.coerce(source_platform).value,
                "target_platform": Platform.coerce(target_platform).value,
            },
        )

    def dependencies(
        self,
        resource: object,
        *,
        platform: Platform,
        layer: Layer,
        context: ResourceContext,
    ) -> list[Dependency]:
        self._validate(platform=platform, layer=layer, context=context)
        dependency_fn = getattr(self.module, "dependencies", None)
        if dependency_fn is None:
            return []
        result = dependency_fn(resource)
        if not isinstance(result, list) or not all(isinstance(edge, Dependency) for edge in result):
            raise CorruptResourceError(
                "legacy dependencies() did not return a list of Dependency values",
                context=context,
            )
        return result

    def capabilities(self) -> CodecCapabilities:
        return self._capabilities
