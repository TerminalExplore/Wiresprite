# snmpmon (C++ rewrite)

Lightweight, cross-platform (Windows + Linux) SNMP switch monitor. Successor to the
Python/Flask prototype in `../app`, which stays untouched as a reference implementation.

## Design

- **SNMP client**: hand-rolled SNMP v1/v2c over UDP (own BER/ASN.1 codec, no Net-SNMP
  dependency) — GET, GETNEXT, GETBULK, subtree WALK.
- **Discovery**: walks the standard IF-MIB `ifTable`, so any SNMP-speaking switch works
  out of the box without per-vendor profiles.
- **HTTP**: [cpp-httplib](https://github.com/yhirose/cpp-httplib) (vendored,
  `third_party/httplib/httplib.h`, pinned at v0.15.3) serves the dashboard and a
  Prometheus-format `/metrics` endpoint. No TLS/OpenSSL dependency.
- **Metrics & alerting**: `/metrics` is scraped by an externally run Prometheus; graphing
  and alert routing (email/Telegram/etc.) are delegated to Grafana + Alertmanager rather
  than reimplemented here.
- **Auth**: session-cookie login guarding the dashboard, password hashed with
  [PicoSHA2](https://github.com/okdshin/PicoSHA2) (vendored,
  `third_party/sha256/picosha2.h`).

See the project plan for the full phase breakdown. This module is being built
incrementally; `snmpmon_core` sources are added to `CMakeLists.txt` as each phase lands.

## Build

```sh
# Windows (Visual Studio 2022 generator)
cmake --preset windows-msvc-release
cmake --build --preset windows-msvc-release

# Linux (Ninja + gcc)
cmake --preset linux-gcc-release
cmake --build --preset linux-gcc-release
```

Run: `./build/<preset>/snmpmon` (or `snmpmon.exe` on Windows).

## Status

Phase 0 (scaffolding) — builds and runs a placeholder entrypoint on both platforms.
