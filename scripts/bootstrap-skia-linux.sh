#!/usr/bin/env bash
set -euo pipefail

configuration="${1:-Release}"
jobs="${2:-4}"
case "$configuration" in Release|Debug) ;; *) echo 'Configuration must be Release or Debug' >&2; exit 2 ;; esac
if [[ ! "$jobs" =~ ^[1-9][0-9]?$ ]] || (( jobs > 64 )); then
    echo 'Jobs must be between 1 and 64' >&2; exit 2
fi
if [[ "$(uname -s)" != Linux ]]; then
    echo 'This bootstrap is for Linux; use bootstrap-skia.ps1 on Windows.' >&2; exit 2
fi
for tool in git python3 ninja clang-18 clang++-18; do command -v "$tool" >/dev/null; done
project_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
skia_root="$project_root/.deps/skia"
revision=ed427fd003ba3bc6eb4a8ae0337f9cdafc39e5fb

if [[ ! -e "$skia_root" ]]; then
    mkdir -p "$skia_root"
    git -C "$skia_root" init
    git -C "$skia_root" remote add origin https://skia.googlesource.com/skia.git
    git -C "$skia_root" fetch --depth 1 origin "$revision"
    git -C "$skia_root" checkout --detach "$revision"
fi
if [[ "$(git -C "$skia_root" rev-parse HEAD)" != "$revision" ]]; then
    echo "Skia must be at $revision; existing checkout left unchanged." >&2; exit 1
fi
if [[ -n "$(git -C "$skia_root" status --porcelain --untracked-files=no)" ]]; then
    echo 'Skia has local source changes; inspect them before building.' >&2; exit 1
fi
cd "$skia_root"
if [[ ! -x bin/gn ]]; then python3 bin/fetch-gn; fi
output=out/photoastra
if [[ "$configuration" == Debug ]]; then output=out/photoastra-debug; fi
mkdir -p "$output"
if [[ "$configuration" == Debug ]]; then
    sed -e 's/is_debug = false/is_debug = true/' \
        -e 's/is_official_build = true/is_official_build = false/' \
        "$project_root/cmake/skia-common.gn" > "$output/args.gn"
else
    cp "$project_root/cmake/skia-common.gn" "$output/args.gn"
fi
cat >> "$output/args.gn" <<'GN'

cc = "clang-18"
cxx = "clang++-18"
skia_enable_spirv_validation = false
skia_enable_gpu_debug_layers = false
GN
bin/gn gen "$output" --script-executable="$(command -v python3)"
ninja -C "$output" -j "$jobs" skia
printf 'Skia %s (%s) available in %s/%s\n' "$revision" "$configuration" "$skia_root" "$output"
