#!/usr/bin/env bash
set -euo pipefail

DRY_RUN=0

if [[ "${1:-}" == "--dry-run" ]]; then
  DRY_RUN=1
fi

run_cmd() {
  if [[ "${DRY_RUN}" -eq 1 ]]; then
    echo "[dry-run] $*"
  else
    eval "$*"
  fi
}

if command -v apt-get >/dev/null 2>&1; then
  run_cmd "sudo apt-get update"
  run_cmd "sudo apt-get install -y build-essential cmake ninja-build pkg-config zlib1g-dev libdbus-1-dev libsystemd-dev libjson-c-dev libcap-dev"
  echo "Installed dependencies via apt-get."
  exit 0
fi

if command -v dnf >/dev/null 2>&1; then
  run_cmd "sudo dnf install -y gcc gcc-c++ make cmake ninja-build pkgconf-pkg-config zlib-devel dbus-devel systemd-devel json-c-devel libcap-devel"
  echo "Installed dependencies via dnf."
  exit 0
fi

if command -v yum >/dev/null 2>&1; then
  run_cmd "sudo yum install -y gcc gcc-c++ make cmake ninja-build pkgconfig zlib-devel dbus-devel systemd-devel json-c-devel libcap-devel"
  echo "Installed dependencies via yum."
  exit 0
fi

if [[ "${DRY_RUN}" -eq 1 ]]; then
  echo "[dry-run] No supported Linux package manager found in current environment."
  exit 0
fi

echo "Unsupported package manager. Please install dependencies manually." >&2
exit 1
