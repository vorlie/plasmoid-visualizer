#!/usr/bin/env bash
set -euo pipefail

PROJECT_NAME="plasmoid-visualizer"
VERSION="1.0"
BUILD_DIR="build_release"
STAGE_DIR="dist_linux"
ASSETS_DIR="release_assets"

echo "Creating release assets for ${PROJECT_NAME} v${VERSION}..."

rm -rf "${BUILD_DIR}" "${STAGE_DIR}" "${ASSETS_DIR}"
mkdir -p "${ASSETS_DIR}"

echo "Configuring and building standalone application..."
cmake -S . -B "${BUILD_DIR}" -DCMAKE_BUILD_TYPE=Release
cmake --build "${BUILD_DIR}" --parallel

echo "Running unit tests..."
ctest --test-dir "${BUILD_DIR}" --output-on-failure

echo "Staging binary and runtime assets..."
cmake --install "${BUILD_DIR}" --prefix "${STAGE_DIR}"

echo "Packaging standalone archive..."
(cd "${STAGE_DIR}" && zip -r "../${ASSETS_DIR}/${PROJECT_NAME}-standalone-linux.zip" .)

echo "------------------------------------------------"
echo "Done! Assets created in ${ASSETS_DIR}/:"
ls -lh "${ASSETS_DIR}"
echo "------------------------------------------------"
