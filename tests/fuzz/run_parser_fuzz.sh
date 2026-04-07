#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build-fuzz"
mkdir -p "${BUILD_DIR}"
GEN_BUILD_DIR="${BUILD_DIR}/cmake-gen"

cmake -S "${ROOT_DIR}" -B "${GEN_BUILD_DIR}" -DCMAKE_BUILD_TYPE=Release >/dev/null

clang \
  -std=gnu11 \
  -fsanitize=fuzzer,address,undefined \
  -fno-omit-frame-pointer \
  -O1 -g \
  -I"${ROOT_DIR}" \
  -I"${GEN_BUILD_DIR}/include/dlt" \
  -I"${ROOT_DIR}/include/dlt" \
  -I"${ROOT_DIR}/src/shared" \
  -I"${ROOT_DIR}/src/daemon" \
  "${ROOT_DIR}/tests/fuzz/fuzz_dlt_config_file_parser.c" \
  "${ROOT_DIR}/src/shared/dlt_config_file_parser.c" \
  "${ROOT_DIR}/src/shared/dlt_common.c" \
  "${ROOT_DIR}/src/shared/dlt_log.c" \
  "${ROOT_DIR}/src/shared/dlt_multiple_files.c" \
  -o "${BUILD_DIR}/fuzz_dlt_config_file_parser" \
  && "${BUILD_DIR}/fuzz_dlt_config_file_parser" -runs=200 \
  || true

if [[ ! -x "${BUILD_DIR}/fuzz_dlt_config_file_parser" ]]; then
  clang \
    -std=gnu11 \
    -fsanitize=address,undefined \
    -fno-omit-frame-pointer \
    -DDLT_FUZZ_STANDALONE \
    -O1 -g \
    -I"${ROOT_DIR}" \
    -I"${GEN_BUILD_DIR}/include/dlt" \
    -I"${ROOT_DIR}/include/dlt" \
    -I"${ROOT_DIR}/src/shared" \
    -I"${ROOT_DIR}/src/daemon" \
    "${ROOT_DIR}/tests/fuzz/fuzz_dlt_config_file_parser.c" \
    "${ROOT_DIR}/src/shared/dlt_config_file_parser.c" \
    "${ROOT_DIR}/src/shared/dlt_common.c" \
    "${ROOT_DIR}/src/shared/dlt_log.c" \
    "${ROOT_DIR}/src/shared/dlt_multiple_files.c" \
    -o "${BUILD_DIR}/fuzz_dlt_config_file_parser_standalone"

  "${BUILD_DIR}/fuzz_dlt_config_file_parser_standalone"
fi
