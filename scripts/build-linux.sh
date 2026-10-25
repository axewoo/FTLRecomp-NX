#!/usr/bin/env bash
# Native Linux build for testing on the PC: build/Release/FTLRecomp
set -euo pipefail
cd "$(dirname "$0")/.."
bash scripts/fetch-deps.sh
cmake -S . -B build/linux -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/linux
ls -la build/Release/FTLRecomp
