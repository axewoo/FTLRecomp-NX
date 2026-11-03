#!/usr/bin/env bash
# Windows 10/11 build, cross-compiled from Linux with MinGW-w64:
# build/FTLRecomp-Windows.zip (FTLRecomp.exe + logging settings + instructions).
set -euo pipefail
cd "$(dirname "$0")/.."
bash scripts/fetch-deps.sh
cmake -S . -B build/win -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE="$PWD/scripts/mingw-w64.cmake"
cmake --build build/win

staging=build/FTLRecomp-Windows
rm -rf "$staging" build/FTLRecomp-Windows.zip
mkdir -p "$staging"
cp build/Release/FTLRecomp.exe "$staging/"
x86_64-w64-mingw32-strip "$staging/FTLRecomp.exe"
# Windows line endings so Notepad shows the text files properly.
for file in dist/windows/*; do
  sed 's/\r*$/\r/' "$file" > "$staging/$(basename "$file")"
done
(cd build && zip -qr FTLRecomp-Windows.zip FTLRecomp-Windows)
ls -la build/FTLRecomp-Windows.zip
