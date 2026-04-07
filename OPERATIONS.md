# Operations Runbook

## Scope

This runbook describes production operation of MyCo cockpit logging platform built on COVESA `dlt-daemon`, with protocol-compatible AUTOSAR V1/V2 behavior.

## Startup Checklist

1. Validate config syntax and required paths.
2. Confirm control socket and TCP listener become ready.
3. Confirm bridge backend (`fluent-bit` or `vector`) is reachable.
4. Confirm metrics and JSON export paths are writable.

## Health Probes

- Script: `scripts/dlt-healthcheck.sh`
- Expected success output: `OK: daemon pid/socket/listener healthy`
- Suggested interval: every 30s.

## Key Runtime Controls

- Control command auth: `ControlAuthMode`, `ControlAuthAllowlist`
- Rate limiting: `AppRateLimitPerSecond`, `AppRateLimitBurst`
- Backpressure: `BackpressureEnable`, `BackpressureHighWatermark`, `BackpressureHardLimit`
- Degrade mode: `DegradeOnOverload`
- Structured export: `JsonExportEnable`, `JsonExportPath`
- Metrics export: `PrometheusMetricsEnable`, `PrometheusMetricsPath`

## Fault Triage Flow

1. Symptom classification
   - Data loss suspected
   - Delay/backlog suspected
   - Control command rejected
   - Bridge sink unavailable
2. Immediate checks
   - `scripts/dlt-healthcheck.sh`
   - metrics file (`dlt_platform_messages_dropped_*`)
   - daemon internal log path (`LoggingFilename`)
3. Decision
   - If backpressure drops increase, tune ring buffer strategy and high/hard watermarks.
   - If quota drops increase, tune per-app rate limits or burst.
   - If control denied increases, verify allowlist and client source address.
4. Recovery
   - Reduce bridge output pressure.
   - Temporarily raise buffer and burst limits.
   - Keep protocol settings unchanged during incident mitigation.
5. Postmortem
   - Export metrics and timeline.
   - Update tuning profile in deployment config.

## Common Incidents

### 1. High Drop Rate During Burst

- Check:
  - `BackpressureHardLimit`
  - `BackpressureDropMtinThreshold`
  - `RingbufferMaxSize`
- Action:
  - Move from `conservative` to `legacy/aggressive` ring buffer strategy.
  - Increase burst for critical apps only.

### 2. Control Commands Rejected

- Check:
  - `ControlAuthMode`
  - `ControlAuthAllowlist`
- Action:
  - Use `ControlAuthMode=1` for localhost-only operations.
  - For remote operations, enable explicit allowlist entries.

### 3. Bridge Sink Unavailable

- Check:
  - FluentBit/Vector service status
  - Endpoint and TLS cert paths
- Action:
  - Enable degrade mode and preserve core DLT protocol path.
  - Route to local JSON export as fallback.

## Release Gate

A release is blocked unless protocol compatibility regression gate passes:

- workflow: `.github/workflows/protocol-compat-regression.yml`
- script: `scripts/protocol-compat-gate.sh`
