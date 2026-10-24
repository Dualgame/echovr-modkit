#!/usr/bin/env bash
# Builds build/libevrmod.so (aarch64, Android API 29+), the Quest mod loader; see evrmod_loader.c.
# NDK r26d: NDK=~/toolchains/android-ndk-r26d by default (as the rad-links Quest build).
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
NDK="${NDK:-$HOME/toolchains/android-ndk-r26d}"
BIN="$NDK/toolchains/llvm/prebuilt/linux-x86_64/bin"
CC="$BIN/aarch64-linux-android29-clang"
mkdir -p "$HERE/build"
OUT="$HERE/build/libevrmod.so"
set -x
"$CC" -shared -fPIC -O2 -std=c11 -Wall -Wextra -Werror \
    -fvisibility=hidden -Wl,--no-undefined -Wl,-z,relro -Wl,-z,now -Wl,--build-id=sha1 -Wl,-soname,libevrmod.so \
    -o "$OUT" "$HERE/evrmod_loader.c" -llog -ldl
"$BIN/llvm-strip" --strip-unneeded "$OUT"
set +x
"$BIN/llvm-readelf" -d "$OUT" | grep -E 'NEEDED|SONAME'
sha256sum "$OUT"
