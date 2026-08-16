#!/usr/bin/env bash
# Builds the WebAssembly frontend into build-wasm/. Serve that directory over
# HTTP and open index.html; file:// will not load the .wasm.
set -euo pipefail

root=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
emsdk="$root/third_party/emsdk"

if [[ ! -f "$emsdk/emsdk_env.sh" ]]; then
    echo "emsdk not found at $emsdk" >&2
    echo "git clone https://github.com/emscripten-core/emsdk.git $emsdk" >&2
    echo "cd $emsdk && ./emsdk install latest && ./emsdk activate latest" >&2
    exit 1
fi

# emsdk_env.sh is noisy and not -u clean.
set +u
# shellcheck disable=SC1091
source "$emsdk/emsdk_env.sh" >/dev/null 2>&1
set -u

emcmake cmake -S "$root" -B "$root/build-wasm" -G Ninja \
    -DGB_BUILD_WASM=ON \
    -DGB_BUILD_SDL3=OFF \
    -DGB_BUILD_HEADLESS=OFF \
    -DGB_BUILD_TESTS=OFF

cmake --build "$root/build-wasm"
