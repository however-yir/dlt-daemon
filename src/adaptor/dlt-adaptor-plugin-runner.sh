#!/usr/bin/env bash
set -euo pipefail

PLUGIN_CONF="${1:-/etc/dlt/adaptor/plugins.conf}"

if [[ ! -f "${PLUGIN_CONF}" ]]; then
  echo "[adaptor-runner] missing plugin config: ${PLUGIN_CONF}" >&2
  exit 1
fi

echo "[adaptor-runner] loading plugins from ${PLUGIN_CONF}"

while IFS= read -r line; do
  line="$(echo "${line}" | sed 's/^[[:space:]]*//;s/[[:space:]]*$//')"

  if [[ -z "${line}" || "${line}" == \#* ]]; then
    continue
  fi

  echo "[adaptor-runner] starting plugin: ${line}"
  bash -lc "${line}" &
done < "${PLUGIN_CONF}"

wait
