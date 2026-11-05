"""Structured Phase 1 resource-codec error taxonomy."""

from __future__ import annotations

from typing import Mapping

from .context import ResourceContext


class ResourceIOError(Exception):
    """Base class carrying deterministic resource and byte-offset diagnostics."""

    kind = "resource-io"

    def __init__(
        self,
        message: str,
        *,
        context: ResourceContext | None = None,
        offset: int | None = None,
        details: Mapping[str, object] | None = None,
    ) -> None:
        super().__init__(message)
        if offset is not None and (not isinstance(offset, int) or offset < 0):
            raise ValueError("offset must be a non-negative integer or None")
        self.message = message
        self.context = context
        self.offset = offset
        self.details = dict(details or {})

    def diagnostic(self) -> dict[str, object]:
        out: dict[str, object] = {"kind": self.kind, "message": self.message}
        if self.context is not None:
            out.update(self.context.diagnostic_fields())
        if self.offset is not None:
            out["offset"] = self.offset
        out.update(self.details)
        return out

    def __str__(self) -> str:
        fields: list[str] = []
        if self.context is not None:
            fields.extend(f"{key}={value}" for key, value in self.context.diagnostic_fields())
        if self.offset is not None:
            fields.append(f"offset=0x{self.offset:x}")
        fields.extend(f"{key}={value}" for key, value in self.details.items())
        prefix = self.kind
        if fields:
            prefix += " [" + " ".join(fields) + "]"
        return f"{prefix}: {self.message}"


class CorruptResourceError(ResourceIOError):
    kind = "corrupt"


class TruncatedResourceError(ResourceIOError):
    kind = "truncated"


class UnsupportedShapeError(ResourceIOError):
    kind = "unsupported-shape"


class UnsupportedPlatformError(ResourceIOError):
    kind = "unsupported-platform"


class UnresolvedDependencyError(ResourceIOError):
    kind = "unresolved-dependency"


class ConversionPolicyRequiredError(ResourceIOError):
    kind = "conversion-policy-required"
