# Wiresprite

Lightweight, cross-platform (Windows + Linux) SNMP switch monitor. Started as a rewrite
of an earlier Python/Flask prototype (preserved in git history, no longer part of the
active project — see the root README's History section).
Ships as a single self-contained binary — no Net-SNMP, no OpenSSL, no webroot directory to
deploy alongside it, no runtime dependency beyond what the OS already provides (see
"Runtime dependencies" below for the exact numbers on each platform).

## What it does

- Polls one or more SNMP v1/v2c devices' standard IF-MIB `ifTable` (plus `ifXTable`'s
  `ifAlias` for admin-assigned port names, when the agent supports it) on a configurable
  interval, in the background, concurrently (bounded by `max_concurrent_devices`).
- Serves a live dashboard (`/`) with a per-interface traffic sparkline (in/out, hover for
  exact values), an in-page banner summarizing unreachable devices/down ports/errors, a
  CSV export of the current snapshot, and a JSON API (`/api/status`) over HTTP.
- Keeps a small in-memory ring buffer (`history_points`, default 240 samples/interface —
  ~2 hours at the default 30s poll interval) purely so the dashboard has something to plot
  the moment it's opened — not a time-series database; long-term history and real alerting
  (delivery, rules, silencing) are Prometheus/Grafana/Alertmanager's job, not this
  project's (see below).
- Exposes `/metrics` in Prometheus text exposition format for that long-term story.
- Optional session-cookie login guarding the dashboard.

Because it walks the standard IF-MIB rather than vendor-specific OIDs, it works against
any SNMP v1/v2c-speaking switch or router out of the box — including quite old hardware
(this project's own test device is an HP ProCurve 2512 from the early 2000s).

## Architecture

```
src/
├── snmp/     hand-rolled BER/ASN.1 codec + SNMP v1/v2c client (GET/GETNEXT/GETBULK/WALK)
│             over raw UDP sockets — no Net-SNMP dependency
├── poll/     ifTable polling, thread-safe DeviceStateStore + HistoryStore, background Poller
├── config/   hand-rolled INI parser (no JSON/YAML dependency)
├── auth/     SHA-256 session-cookie auth
└── http/     cpp-httplib wiring: dashboard, /api/status, /metrics, /login, /logout
```

Each layer only knows about the one below it — `ber` has no notion of SNMP PDUs, `pdu` has
no notion of sockets, `client` has no notion of ifTable semantics, and so on — which is
also why most of it is unit-testable without any network I/O at all.

## Build

Requires a C++17 compiler and CMake 3.16+.

```sh
# Windows (Visual Studio 2022 generator)
cmake --preset windows-msvc-release
cmake --build --preset windows-msvc-release

# Linux (Ninja + gcc)
cmake --preset linux-gcc-release
cmake --build --preset linux-gcc-release
```

Run: `./build/<preset>/wiresprite [path/to/config.ini]` (or `wiresprite.exe` on Windows). Defaults
to `wiresprite.ini` in the current directory if no path is given.

### Runtime dependencies and binary size

Verified via `dumpbin /dependents` (Windows) and `ldd` (Linux) against Release builds:

| Platform | Binary size | Dynamic dependencies |
|---|---|---|
| Windows (MSVC, static CRT) | ~650KB | `WS2_32.dll`, `KERNEL32.dll` only |
| Linux (gcc, `-static-libgcc -static-libstdc++`) | ~2.2MB | `libc.so.6` only (pthread is part of glibc on modern distros) |

No OpenSSL, no Net-SNMP, nothing else on either platform — a deliberate design goal from
the start, not an afterthought. The Linux binary is larger because statically linking
libstdc++/libgcc pulls their code into the binary itself rather than resolving it at
runtime, which is the whole point (no matching-libstdc++-version requirement on the target
machine) but does cost size; MSVC's static CRT is comparatively lighter-weight.

## Configuration

Copy `config/wiresprite.ini.example` to `wiresprite.ini` and edit it. Full schema:

```ini
[http]
listen_address = 0.0.0.0
listen_port = 8080

[auth]
username = admin
# Generate with: printf '%s' "your-password" | sha256sum
password_hash = <sha256 hex digest>
# Leave password_hash blank to disable login entirely (open dashboard).
session_ttl_minutes = 60

[polling]
interval_seconds = 30
timeout_ms = 1500
retries = 2
max_concurrent_devices = 8

[device:some-name]
display_name = Human-readable name
host = 192.168.1.1
port = 161
community = public
version = 1        # "1" or "2c" — no SNMPv3
```

Repeat `[device:name]` for as many devices as you want monitored.

## Prometheus / Grafana

`/metrics` is intentionally left unauthenticated, matching standard Prometheus exporter
convention (a scraper doesn't do cookie/session auth). Point Prometheus at it:

```yaml
scrape_configs:
  - job_name: wiresprite
    static_configs:
      - targets: ["wiresprite-host:8080"]
```

Then build Grafana panels/alerts against `wiresprite_up`, `wiresprite_if_oper_status`,
`rate(wiresprite_if_in_octets_total[5m])`, etc. — see `src/http/routes_metrics.cpp` for the
full metric list.

## Dashboard design

The built-in dashboard is intentionally not a Grafana replacement — no charting library,
no client-side dependency, just hand-rolled inline SVG (`renderSparkline` in
`src/http/web_assets.cpp`'s embedded `app.js`) driven by the `history` array `/api/status`
already returns per interface. Colors and mark specs (2px rounded lines, ~10%-opacity area
fill, icon+label status pills, hover crosshair/tooltip) come from a validated reference
palette rather than being picked by eye; see that file's CSS custom properties for the
exact tokens, light and dark.

## Testing

```sh
cmake --preset <preset> -DWIRESPRITE_BUILD_TESTS=ON
cmake --build --preset <preset>
ctest --test-dir build/<preset> -C Release --output-on-failure
```

Tests use [doctest](https://github.com/doctest/doctest) (vendored). Most of the suite is
pure unit tests with no I/O; the SNMP client, poller, and HTTP server are covered by
integration tests that run the real UDP/TCP stack over loopback — including a small
in-process fake SNMP agent (`tests/fake_snmp_agent.hpp`) for exercising WALK/GETBULK
without depending on an external `snmpd` or a reachable physical device.

## Status

All planned phases are complete:

| Phase | What |
|---|---|
| 0 | CMake scaffolding, vendored dependencies |
| 1 | BER/ASN.1 codec, `Oid` type |
| 2 | SNMP GET over UDP |
| 3 | GETNEXT/GETBULK/WALK, multi-device INI config |
| 4 | Background poller, thread-safe device state |
| 5 | HTTP dashboard |
| 6 | `/metrics` (Prometheus) |
| 7 | Session-cookie auth |
| 8 | Cross-platform packaging & verification |
