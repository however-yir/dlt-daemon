# Adaptor Plugin Mechanism

This directory hosts the runtime plugin manifest for adaptor processes.

- `plugins.conf` contains one command per line.
- `src/adaptor/dlt-adaptor-plugin-runner.sh` starts each enabled plugin.
- Existing adaptors (`dlt-adaptor-udp`, `dlt-adaptor-stdin`) are still supported.

This keeps protocol behavior unchanged while allowing deployment-specific adaptor extension without patching daemon core paths.
