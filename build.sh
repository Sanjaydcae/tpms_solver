#!/usr/bin/env bash
set -e

BUILD_DIR="build"
JOBS=$(nproc 2>/dev/null || echo 4)

echo "==> Configuring..."
cmake -S . -B "$BUILD_DIR" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

echo "==> Building with $JOBS jobs..."
cmake --build "$BUILD_DIR" --parallel "$JOBS"

echo ""
echo "==> Build complete."
echo "    Binary: $BUILD_DIR/tpms_solver"
echo "    Run:    cd $BUILD_DIR && ./tpms_solver"
