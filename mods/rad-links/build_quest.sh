#!/usr/bin/env bash
# Builds build/libradlinks.so (aarch64, Android API 29+) for the Echo VR Quest app.
# NDK r26d: NDK=~/toolchains/android-ndk-r26d by default.
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
NDK="${NDK:-$HOME/toolchains/android-ndk-r26d}"
BIN="$NDK/toolchains/llvm/prebuilt/linux-x86_64/bin"
CC="$BIN/aarch64-linux-android29-clang"
mkdir -p "$HERE/build"
OUT="$HERE/build/libradlinks.so"
set -x
"$CC" -shared -fPIC -O2 -std=c11 -Wall -Wextra -Werror \
    -fvisibility=hidden -Wl,--no-undefined -Wl,-z,relro -Wl,-z,now -Wl,--build-id=sha1 -Wl,-soname,libradlinks.so \
    -o "$OUT" "$HERE/src/radlinks_quest.c" -llog -ldl
"$BIN/llvm-strip" --strip-unneeded "$OUT"
set +x
file "$OUT" 2>/dev/null || true
"$BIN/llvm-readelf" -d "$OUT" | grep NEEDED
sha256sum "$OUT"
