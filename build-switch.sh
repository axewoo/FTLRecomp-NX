#!/usr/bin/env bash
# Builds build/ftl.nro for Nintendo Switch homebrew (hbmenu / Atmosphere).
# Needs devkitPro with: switch-dev switch-cmake switch-pkg-config switch-mesa switch-libdrm_nouveau
set -euo pipefail
cd "$(dirname "$0")"
DEVKITPRO="${DEVKITPRO:-/opt/devkitpro}"
export DEVKITPRO
TOOLCHAIN="$DEVKITPRO/cmake/Switch.cmake"
PREFIX="$PWD/build/sdl3-nx-prefix"

if [ ! -f "$PREFIX/lib/cmake/SDL3/SDL3Config.cmake" ]; then
    echo "== Building SDL3 for Switch (nx-mod/sdl3-nx) =="
    [ -d build/sdl3-nx ] || git clone --depth 1 https://github.com/nx-mod/sdl3-nx build/sdl3-nx
    cmake -S build/sdl3-nx -B build/sdl3-nx/build -G Ninja \
        -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN" -DCMAKE_BUILD_TYPE=Release \
        -DSDL_SHARED=OFF -DSDL_STATIC=ON -DSDL_TESTS=OFF -DSDL_EXAMPLES=OFF
    cmake --build build/sdl3-nx/build
    cmake --install build/sdl3-nx/build --prefix "$PREFIX"
fi

echo "== Building FTLRecomp for Switch =="
cmake -S . -B build/switch -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN" -DCMAKE_BUILD_TYPE=Release \
    -DSDL3_DIR="$PREFIX/lib/cmake/SDL3"
cmake --build build/switch
ls -la build/ftl.nro
