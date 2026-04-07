#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build-protocol-gate"

cmake -S "${ROOT_DIR}" -B "${BUILD_DIR}" \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DWITH_DLT_UNIT_TESTS=ON \
  -DBUILD_GMOCK=OFF

cmake --build "${BUILD_DIR}" -- -j"$(nproc 2>/dev/null || sysctl -n hw.ncpu)"

ctest --test-dir "${BUILD_DIR}" --output-on-failure -R "gtest_dlt_(common|daemon|daemon_common|user)$"
ctest --test-dir "${BUILD_DIR}" --output-on-failure -R "gtest_dlt_(common_v2|daemon_v2|daemon_common_v2|user_v2)$"

echo "[protocol-gate] AUTOSAR V1/V2 compatibility regression gate passed"
