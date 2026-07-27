#!/usr/bin/env bash
# extract.sh — one command: build the headless extractor and turn an official Meta Horizon Home
# environment APK into a daoshu-ready officialBase (glTF 2.0 + textures), Quest-optimized.
#
# Verified end-to-end on vista_calming.apk: 184 meshes / 184 materials / 248 textures,
# raw 515 MB -> optimized ~103 MB (Quest-loadable). Assets are Meta-proprietary: personal-device
# use only, never distribute — the repo .gitignore excludes daoshu/private/ and *.apk.
#
# Usage:
#   tools/env-extract/extract.sh <env.apk> <outDir> [name] [--max 1024] [--quality 85] [--raw]
#
# Requires (Debian/Ubuntu): build-essential cmake ninja-build git libvulkan-dev
#   libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libwayland-dev
#   libxkbcommon-dev pkg-config python3-pil
set -euo pipefail

APK="${1:?usage: extract.sh <env.apk> <outDir> [name] [--max N] [--quality N] [--raw]}"
OUT="${2:?missing outDir}"; NAME="${3:-env}"
MAX=1024; Q=85; RAW=0
shift $(( $# < 3 ? $# : 3 )) || true
while [ $# -gt 0 ]; do case "$1" in
  --max) MAX="$2"; shift 2;; --quality) Q="$2"; shift 2;; --raw) RAW=1; shift;;
  *) echo "unknown arg: $1"; exit 2;; esac; done

HERE="$(cd "$(dirname "$0")" && pwd)"
WORK="${QHE_DIR:-$HOME/Quest-Home-Editor}"

# 1. Fetch + patch Quest-Home-Editor (reverse-engineered libshell scene loader / gltf export).
if [ ! -d "$WORK/.git" ]; then
  git clone --depth 1 https://github.com/xAstroBoy/Quest-Home-Editor.git "$WORK"
fi
cd "$WORK"
git apply --check "$HERE/qhe-parser-fixes.patch" 2>/dev/null && \
  git apply "$HERE/qhe-parser-fixes.patch" || echo "[extract.sh] patch already applied — skipping"
mkdir -p QuestHomeEditor/src/tools
cp "$HERE/hsl_extract.cpp" "$HERE/hsl_bulk.cpp" QuestHomeEditor/src/tools/

# 2. Build the headless extractor (no Vulkan/GLFW/PhysX needed for extraction).
cmake -S QuestHomeEditor -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DHSR_BUILD_TESTS=OFF -DHSR_HAVE_PHYSX=OFF
cmake --build build --target hsl_extract -j"$(nproc)"

# 3. Extract every sublevel to raw glTF (set HSL_SCENEONLY=1 for the draw-faithful subset only).
RAWOUT="$OUT/_raw"
"$WORK/build/hsl_extract" "$APK" "$RAWOUT" "$NAME"

# 4. Quest-optimize the textures unless --raw (cap resolution + JPEG opaque / PNG alpha).
if [ "$RAW" -eq 1 ]; then
  echo "[extract.sh] --raw: leaving full-resolution dump at $RAWOUT"
  echo "[extract.sh] deploy: copy $RAWOUT/* to daoshu/private/official/ on the serving machine"
else
  python3 "$HERE/optimize_textures.py" "$RAWOUT/$NAME.gltf" "$OUT" --max "$MAX" --quality "$Q"
  rm -rf "$RAWOUT"
  echo "[extract.sh] done: $OUT/$NAME.gltf  (+ $NAME.bin, textures/)"
  echo "[extract.sh] deploy: copy $OUT/* to daoshu/private/official/ on the serving machine,"
  echo "[extract.sh]         then open daoshu/index.html?base to load it as officialBase."
fi
