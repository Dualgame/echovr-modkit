"""v1 / JAR signing helpers for a rebuilt Quest APK: MANIFEST.MF sections, the .SF file and a PKCS#7 signature from
a PKCS12 keystore. Used by the release builder
(quest_apk_build_release.py)."""
from __future__ import annotations

import base64
import hashlib
import zipfile
from pathlib import Path

from cryptography.hazmat.primitives import hashes, serialization
from cryptography.hazmat.primitives.serialization import pkcs12, pkcs7


def fold_line(name: str, value: str) -> bytes:
    line = f"{name}: {value}"
    chunks = []
    first = True
    while line:
        width = 70 if first else 69
        prefix = "" if first else " "
        chunks.append((prefix + line[:width]).encode("utf-8") + b"\r\n")
        line = line[width:]
        first = False
    return b"".join(chunks)


def digest_b64(data: bytes) -> str:
    return base64.b64encode(hashlib.sha384(data).digest()).decode("ascii")


def manifest_section(name: str, data: bytes) -> bytes:
    return fold_line("Name", name) + fold_line("SHA-384-Digest", digest_b64(data)) + b"\r\n"


def is_signature_entry(name: str) -> bool:
    upper = name.upper()
    if not upper.startswith("META-INF/"):
        return False
    if upper == "META-INF/MANIFEST.MF":
        return True
    return upper.endswith((".SF", ".RSA", ".DSA", ".EC"))


def build_manifest(entries: list[tuple[str, bytes]]) -> tuple[bytes, list[tuple[str, bytes]]]:
    main = b"Manifest-Version: 1.0\r\nCreated-By: 25.0.2 (Eclipse Adoptium)\r\n\r\n"
    sections = [(name, manifest_section(name, data)) for name, data in entries]
    return main + b"".join(section for _, section in sections), [(None, main), *sections]


def build_sf(manifest: bytes, sections: list[tuple[str | None, bytes]]) -> bytes:
    out = (
        b"Signature-Version: 1.0\r\n"
        b"Created-By: 25.0.2 (Eclipse Adoptium)\r\n"
        + fold_line("SHA-384-Digest-Manifest", digest_b64(manifest))
        + fold_line("SHA-384-Digest-Manifest-Main-Attributes", digest_b64(sections[0][1]))
        + b"\r\n"
    )
    for name, section in sections[1:]:
        out += fold_line("Name", name) + fold_line("SHA-384-Digest", digest_b64(section)) + b"\r\n"
    return out


def sign_sf(sf: bytes, keystore: Path, password: bytes) -> bytes:
    key, cert, _cas = pkcs12.load_key_and_certificates(keystore.read_bytes(), password)
    if key is None or cert is None:
        raise ValueError(f"keystore did not contain a private key and cert: {keystore}")
    builder = pkcs7.PKCS7SignatureBuilder().set_data(sf).add_signer(cert, key, hashes.SHA384())
    return builder.sign(
        serialization.Encoding.DER,
        [
            pkcs7.PKCS7Options.Binary,
            pkcs7.PKCS7Options.DetachedSignature,
            pkcs7.PKCS7Options.NoCapabilities,
        ],
    )


def clone_info(src: zipfile.ZipInfo, name: str | None = None) -> zipfile.ZipInfo:
    dst = zipfile.ZipInfo(name or src.filename, src.date_time)
    dst.compress_type = src.compress_type
    dst.comment = src.comment
    dst.extra = src.extra
    dst.internal_attr = src.internal_attr
    dst.external_attr = src.external_attr
    dst.create_system = src.create_system
    return dst
