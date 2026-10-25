#!/usr/bin/env bash
# Copies ftl.nro and your own ftl.dat to a Switch SD card.
#
#   scripts/install-sd.sh [SD card path] [path to ftl.nro]
#
# Without arguments it looks for a mounted card that has an atmosphere/ folder,
# uses build/ftl.nro (or ./ftl.nro), and finds ftl.dat in your Steam libraries.
# Set FTL_DAT=/path/to/ftl.dat to skip the Steam search.
set -euo pipefail
cd "$(dirname "$0")/.."

sd="${1:-}"
if [ -z "$sd" ]; then
    for candidate in /run/media/"$USER"/* /media/"$USER"/* /Volumes/*; do
        if [ -d "$candidate/atmosphere" ]; then
            sd="$candidate"
            break
        fi
    done
fi
if [ -z "$sd" ] || [ ! -d "$sd" ]; then
    echo "No SD card found. Mount it and pass its path: $0 /path/to/sd" >&2
    exit 1
fi

nro="${2:-}"
if [ -z "$nro" ]; then
    for candidate in build/ftl.nro ftl.nro; do
        if [ -f "$candidate" ]; then
            nro="$candidate"
            break
        fi
    done
fi
if [ -z "$nro" ] || [ ! -f "$nro" ]; then
    echo "No ftl.nro found. Build it (scripts/build-switch.sh) or pass the downloaded file." >&2
    exit 1
fi

dat="${FTL_DAT:-}"
if [ -z "$dat" ]; then
    libraries=()
    for steam in "$HOME/.steam/steam" "$HOME/.local/share/Steam" \
        "$HOME/.var/app/com.valvesoftware.Steam/.local/share/Steam"; do
        [ -d "$steam/steamapps" ] || continue
        libraries+=("$steam")
        vdf="$steam/steamapps/libraryfolders.vdf"
        if [ -f "$vdf" ]; then
            while IFS= read -r path; do
                libraries+=("$path")
            done < <(sed -n 's/^[[:space:]]*"path"[[:space:]]*"\(.*\)"/\1/p' "$vdf")
        fi
    done
    for library in "${libraries[@]}"; do
        for candidate in "$library/steamapps/common/FTL Faster Than Light/data/ftl.dat" \
            "$library/steamapps/common/FTL Faster Than Light/ftl.dat"; do
            if [ -f "$candidate" ]; then
                dat="$candidate"
                break 2
            fi
        done
    done
fi
if [ -z "$dat" ] || [ ! -f "$dat" ]; then
    echo "ftl.dat not found. Install FTL from Steam, or set FTL_DAT=/path/to/ftl.dat" >&2
    exit 1
fi

mkdir -p "$sd/switch/ftl"
echo "NRO:     $nro"
echo "ftl.dat: $dat"
echo "SD card: $sd/switch/ftl/"
cp "$nro" "$sd/switch/ftl/ftl.nro"
cp "$dat" "$sd/switch/ftl/ftl.dat"
sync
echo "Done. Launch FTL from hbmenu (hold R on a game for full memory)."
