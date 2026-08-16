#!/usr/bin/env bash
# The emulator core must stay free of frontend dependencies so the same
# translation units serve SDL3, wasm, libretro and the test runner.
set -u

root="$(cd "$(dirname "$0")/.." && pwd)"
core="$root/src/core"
status=0

for pattern in 'SDL' 'imgui' 'ImGui' 'emscripten' 'libretro' 'GLFW'; do
    if hits=$(grep -rn --include='*.h' --include='*.cpp' --include='*.inl' "$pattern" "$core"); then
        echo "core depends on $pattern:"
        echo "$hits"
        status=1
    fi
done

if [ "$status" -eq 0 ]; then
    echo "core is clean"
fi
exit "$status"
