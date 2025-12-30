#!/bin/bash
# Meant to be run from project root folder and in docker:rootless mode
set -euo pipefail

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUTPUT_DIR="${PROJECT_DIR}/output"
CCACHE_DIR="${PROJECT_DIR}/ccache"
IMAGE_NAME="mgba-builder"
BUILD_LOG="${OUTPUT_DIR}/build.log"
BUILD_AREA="${PROJECT_DIR}"
LOG_FILE="/workspace/output/build-$(date +%Y%m%d-%H%M%S).log"

mkdir -p "$PROJECT_DIR/build"
mkdir -p "$OUTPUT_DIR"
mkdir -p "$CCACHE_DIR"

# On the host
mkdir -p "${OUTPUT_DIR}/build"
mkdir -p "${BUILD_AREA}/build"
chown -R $(id -u):$(id -g) "${BUILD_AREA}/build"
chmod -R u+rwX "${BUILD_AREA}"

# Build the Docker image (rootless)
docker build -t "$IMAGE_NAME" .

# Run the build in a rootless container
docker run --rm -it --cap-drop=ALL --memory=2g --cpus=2 -v "$PROJECT_DIR:/workspace/src:rw" -v "$PROJECT_DIR/build:/workspace/build:rw" -v "$OUTPUT_DIR:/workspace/output:rw" -v "$CCACHE_DIR:/home/builder/.ccache:rw" "$IMAGE_NAME" bash -c "cd /workspace/build && cmake ../src -DCMAKE_BUILD_TYPE=Release -DMGBA_WITH_QT=ON && make -j\$(nproc) 2>&1 | tee \"$LOG_FILE\""
echo "Build complete. Logs saved to $BUILD_LOG"
