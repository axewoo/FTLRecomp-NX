#!/usr/bin/env bash
# Downloads the pinned third-party libraries into third_party/.
# Safe to re-run: anything already present is kept.
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p third_party
cd third_party

SDL3_VERSION=3.2.26
FREETYPE_TAG=VER-2-13-3
PUGIXML_TAG=v1.14
JSON_TAG=v3.11.3
STB_COMMIT=003501f6d7a8668ce8b6d875e34dd97ad0d5d5b5

fetch() { curl -fsSL --retry 3 -o "$2" "$1"; }

if [ ! -d "SDL3-$SDL3_VERSION" ]; then
    echo "== SDL $SDL3_VERSION (PC build only)"
    curl -fsSL --retry 3 "https://github.com/libsdl-org/SDL/releases/download/release-$SDL3_VERSION/SDL3-$SDL3_VERSION.tar.gz" | tar xz
fi
if [ ! -d "freetype-$FREETYPE_TAG" ]; then
    echo "== FreeType $FREETYPE_TAG"
    git -c advice.detachedHead=false clone -q --depth 1 -b "$FREETYPE_TAG" https://github.com/freetype/freetype "freetype-$FREETYPE_TAG"
fi
for f in pugixml.cpp pugixml.hpp pugiconfig.hpp; do
    [ -f "$f" ] || fetch "https://raw.githubusercontent.com/zeux/pugixml/$PUGIXML_TAG/src/$f" "$f"
done
[ -f json.hpp ] || fetch "https://github.com/nlohmann/json/releases/download/$JSON_TAG/json.hpp" json.hpp
for f in stb_image.h stb_vorbis.c; do
    [ -f "$f" ] || fetch "https://raw.githubusercontent.com/nothings/stb/$STB_COMMIT/$f" "$f"
done
echo "third_party/ is ready."
