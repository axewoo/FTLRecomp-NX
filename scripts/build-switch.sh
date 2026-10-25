#!/usr/bin/env bash
# Builds build/ftl.nro for Nintendo Switch homebrew (hbmenu / Atmosphere).
# Needs devkitPro with: switch-dev switch-cmake switch-pkg-config switch-mesa switch-libdrm_nouveau
set -euo pipefail
cd "$(dirname "$0")/.."
bash scripts/fetch-deps.sh
DEVKITPRO="${DEVKITPRO:-/opt/devkitpro}"
export DEVKITPRO
TOOLCHAIN="$DEVKITPRO/cmake/Switch.cmake"
PREFIX="$PWD/build/sdl3-nx-prefix"
# nx-mod/sdl3-nx, branch switch-sdl3-deko3d, tested with this commit.
SDL3_NX_COMMIT=07ea26a3ca7b60bac414823f0f45e8738c61c217

if [ ! -f "$PREFIX/lib/cmake/SDL3/SDL3Config.cmake" ]; then
    echo "== Building SDL3 for Switch (nx-mod/sdl3-nx) =="
    if [ ! -d build/sdl3-nx ]; then
        git clone -q https://github.com/nx-mod/sdl3-nx build/sdl3-nx
        git -C build/sdl3-nx checkout -q "$SDL3_NX_COMMIT"
    fi
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
echo "Built build/ftl.nro"
