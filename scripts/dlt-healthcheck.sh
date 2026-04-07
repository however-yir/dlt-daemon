#!/usr/bin/env bash
set -euo pipefail

CONTROL_SOCKET="${1:-/tmp/dlt-ctrl.sock}"
TCP_PORT="${2:-3490}"

if ! pgrep -x dlt-daemon >/dev/null 2>&1; then
  echo "[healthcheck] dlt-daemon process not running" >&2
  exit 1
fi

if [[ ! -S "${CONTROL_SOCKET}" ]]; then
  echo "[healthcheck] control socket missing: ${CONTROL_SOCKET}" >&2
  exit 2
fi

if command -v ss >/dev/null 2>&1; then
  if ! ss -ltn "( sport = :${TCP_PORT} )" | grep -q ":${TCP_PORT}"; then
    echo "[healthcheck] tcp listener missing on port ${TCP_PORT}" >&2
    exit 3
  fi
fi

echo "[healthcheck] OK: daemon pid/socket/listener healthy"
