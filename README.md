# my_gb

A Game Boy (DMG) emulator written in C++20.

The emulator core has no UI dependencies and is driven by several frontends:
SDL3 for desktop, a headless runner for tests, libretro, and WebAssembly.

## Building

```
cmake -B build -G Ninja
cmake --build build
```

Needs a C++20 compiler. SDL3 is required for the desktop frontend and can be
disabled with `-DGB_BUILD_SDL3=OFF`.

## Tests

Test ROMs aren't vendored. Fetch them once:

```
python3 tools/fetch_test_roms.py
```

Then:

```
ctest --test-dir build --output-on-failure
```
