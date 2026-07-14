# snmpmon (C++ rewrite)

Lightweight, cross-platform (Windows + Linux) SNMP switch monitor. Successor to the
Python/Flask prototype in `../app`, which stays untouched as a reference implementation.
Ships as a single self-contained binary — no Net-SNMP, no OpenSSL, no webroot directory to
deploy alongside it, no runtime dependency beyond what the OS already provides (see
"Runtime dependencies" below for the exact numbers on each platform).

## What it does

- Polls one or more SNMP v1/v2c devices' standard IF-MIB `ifTable` on a configurable
  interval, in the background, concurrently (bounded by `max_concurrent_devices`).
- Serves a small live dashboard (`/`) and JSON API (`/api/status`) over HTTP.
- Exposes `/metrics` in Prometheus text exposition format — point a Prometheus at it and
  graph everything in Grafana; this project doesn't store history or draw charts itself.
- Optional session-cookie login guarding the dashboard.

Because it walks the standard IF-MIB rather than vendor-specific OIDs, it works against
any SNMP v1/v2c-speaking switch or router out of the box — including quite old hardware
(this project's own test device is an HP ProCurve 2512 from the early 2000s).

## Architecture

```
src/
├── snmp/     hand-rolled BER/ASN.1 codec + SNMP v1/v2c client (GET/GETNEXT/GETBULK/WALK)
│             over raw UDP sockets — no Net-SNMP dependency
├── poll/     ifTable polling, thread-safe DeviceStateStore, background Poller
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

Run: `./build/<preset>/snmpmon [path/to/config.ini]` (or `snmpmon.exe` on Windows). Defaults
to `snmpmon.ini` in the current directory if no path is given.

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

Copy `config/snmpmon.ini.example` to `snmpmon.ini` and edit it. Full schema:

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
  - job_name: snmpmon
    static_configs:
      - targets: ["snmpmon-host:8080"]
```

Then build Grafana panels/alerts against `snmpmon_up`, `snmpmon_if_oper_status`,
`rate(snmpmon_if_in_octets_total[5m])`, etc. — see `src/http/routes_metrics.cpp` for the
full metric list.

## Testing

```sh
cmake --preset <preset> -DSNMPMON_BUILD_TESTS=ON
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
