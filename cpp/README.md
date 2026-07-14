# Wiresprite

Lightweight, cross-platform (Windows + Linux) SNMP switch/router monitor with a live
web dashboard. Started as a rewrite of an earlier Python/Flask prototype (preserved in
git history, no longer part of the active project — see the root README's History
section). Ships as a single self-contained binary — no Net-SNMP, no OpenSSL, no webroot
directory to deploy alongside it, no runtime dependency beyond what the OS already
provides (see "Runtime dependencies" below for the exact numbers on each platform).

Because it walks standard MIBs (IF-MIB, `ifXTable`, BRIDGE-MIB, MIB-II) rather than
vendor-specific OIDs, it works against any SNMP v1/v2c-speaking switch or router out of
the box — including quite old hardware (this project's own test device is an HP
ProCurve 2512 from the early 2000s).

## What it does

**Polling** (`src/poll/`, background thread, no blocking the dashboard):
- Standard IF-MIB `ifTable` — per-interface status, speed, traffic counters, errors,
  discards, and `ifLastChange` (how long a port has been in its current state).
- `ifXTable`'s `ifAlias` — the admin-assigned port name set on the switch itself (e.g.
  `interface 6 name "Uplink to router"`), when the agent implements it.
- BRIDGE-MIB's `dot1dTpFdbTable` — which MAC addresses the switch has learned on which
  port, translated from bridge-port numbers to `ifIndex` via `dot1dBasePortTable`.
- MIB-II `sysDescr` — the device's model/firmware string, and `sysUpTime`.
- Every device is polled concurrently within a cycle (bounded by
  `max_concurrent_devices`), so one slow or unreachable device doesn't stall the rest.
  Every walk that isn't universally implemented (`ifXTable`, BRIDGE-MIB) degrades to
  "leave that field empty" rather than failing the whole poll.

**Live dashboard** (`/`, pushed over Server-Sent Events — see "The web dashboard" below)
with per-port traffic charts, a MAC-address list per port, an aggregate traffic/error
Overview page, an Up/Down port filter, CSV export, manual light/dark theming, and a
fullscreen kiosk/wallboard mode with slow auto-scroll.

**Settings page** (`/settings`) for adding/editing devices, polling knobs, and login
credentials from the browser — no hand-editing `wiresprite.ini` required. A brand-new
install (no config file yet) starts with zero devices instead of failing, opens your
browser to a first-run setup flow automatically, and walks you through setting a
password and adding your first device from there.

**`/metrics`** in Prometheus text exposition format, for long-term history and alerting
via Prometheus/Grafana/Alertmanager — this project deliberately doesn't reimplement any
of that (see "Prometheus / Grafana" below).

**Auth**: optional session-cookie login (SHA-256 password hash, constant-time compare),
with a "Remember me" option for longer-lived sessions. Leave `password_hash` unset to
run with no login at all.

## Architecture

```
src/
├── snmp/     hand-rolled BER/ASN.1 codec + SNMP v1/v2c client (GET/GETNEXT/GETBULK/WALK)
│             over raw UDP sockets — no Net-SNMP dependency
├── poll/     ifTable/ifXTable/BRIDGE-MIB polling, thread-safe DeviceStateStore +
│             HistoryStore (traffic-rate ring buffer) + SseHub (live-push notifications),
│             background Poller
├── config/   hand-rolled INI parser + writer (no JSON/YAML dependency)
├── auth/     SHA-256 session-cookie auth, live-updatable credentials
├── platform/ OS-specific bits with a single call each way (currently: opening the
│             default browser on first run)
└── http/     cpp-httplib wiring: dashboard, settings page, /api/status, /api/events
              (SSE), /api/config, /metrics, /login, /logout
```

Each layer only knows about the one below it — `ber` has no notion of SNMP PDUs, `pdu`
has no notion of sockets, `client` has no notion of ifTable semantics, and so on — which
is also why most of it is unit-testable without any network I/O at all.

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

Run: `./build/<preset>/wiresprite [path/to/config.ini]` (or `wiresprite.exe` on
Windows). Defaults to `wiresprite.ini` in the current directory if no path is given.

### Runtime dependencies and binary size

Verified via `dumpbin /dependents` (Windows) and `ldd` (Linux) against Release builds:

| Platform | Binary size | Dynamic dependencies |
|---|---|---|
| Windows (MSVC, static CRT) | ~770KB | `WS2_32.dll`, `SHELL32.dll`, `KERNEL32.dll` only |
| Linux (gcc, `-static-libgcc -static-libstdc++`) | ~2.2MB | `libc.so.6` only (pthread is part of glibc on modern distros) |

No OpenSSL, no Net-SNMP, nothing else on either platform — a deliberate design goal from
the start, not an afterthought. The Linux binary is larger because statically linking
libstdc++/libgcc pulls their code into the binary itself rather than resolving it at
runtime, which is the whole point (no matching-libstdc++-version requirement on the
target machine) but does cost size; MSVC's static CRT is comparatively lighter-weight.

## First run

Point the binary at a config path that doesn't exist yet (or just run it with no
argument in an empty directory) and it creates a default `wiresprite.ini` (no devices,
no password) instead of failing, then opens your default browser to the dashboard.
Because there's no password and no devices configured yet, `/` redirects to `/settings`
with a "Welcome to Wiresprite — let's get set up" banner: set a password, add your first
device, save. Devices/polling changes need the program restarted to take effect (see
"The settings page" below); the password you set applies immediately, so you can log in
right away without restarting anything.

The auto-opened-browser behavior only ever fires on that first run — never on an
ordinary restart, since that would be wrong for a headless/`systemd`-managed deployment
(see [`packaging/`](../packaging/)). Set `open_browser_on_first_run = false` in
`[http]` to disable it entirely (e.g. for scripted/unattended first-time deploys).

## Configuration

Copy `config/wiresprite.ini.example` to `wiresprite.ini` and edit it — or use the
`/settings` page instead, which writes this same file for you. Full schema:

```ini
[http]
listen_address = 0.0.0.0
listen_port = 8080
# Only consulted on a genuine first run (see "First run" above).
open_browser_on_first_run = true

[auth]
username = admin
# The /settings page hashes whatever you type for you. Manual
# alternative: printf '%s' "your-password" | sha256sum
password_hash = <sha256 hex digest, or leave blank to disable login>
session_ttl_minutes = 60
# Session length when the login page's "Remember me" checkbox is used.
remember_me_days = 30

[polling]
interval_seconds = 30
timeout_ms = 1500
retries = 2
max_concurrent_devices = 8
# Ring-buffer size per interface for the dashboard's traffic charts.
# 240 @ the default 30s interval = 2 hours of history.
history_points = 240

[device:some-name]
display_name = Human-readable name
host = 192.168.1.1
port = 161
community = public
version = 1        # "1" or "2c" — no SNMPv3
```

Repeat `[device:name]` for as many devices as you want monitored.

## The web dashboard

Two pages, switchable via the navbar, both driven by the same live SSE stream (see
below) so both update in place with no page reload:

- **Ports** — one card per device, one sub-card per interface. Up ports show a full
  card: name (alias, falling back to the vendor `ifDescr`), status pill, link
  speed/admin state/time-in-state, in/out traffic charts (with gridlines and a
  time-range caption, not just a bare line), a magnitude-tiered error/discard warning,
  and a collapsed `<details>` list of MAC addresses learned on that port. Down/other
  ports collapse into compact badges instead of full dead cards. A summary row up top
  shows ports-up count, device uptime, and a traffic-utilization meter; an alert banner
  surfaces unreachable devices/down ports/errors without having to scan every card. An
  All/Up/Down filter narrows the port list.
- **Overview** — an aggregate in/out traffic chart (every interface's history, summed
  per timestamp) and a table of every port currently showing errors or discards, sorted
  worst-first.

Other dashboard features:
- **Live updates** over `/api/events` (Server-Sent Events) — the server pushes a fresh
  snapshot the moment a poll cycle completes, instead of the browser polling on a timer.
- **CSV export** of the current snapshot (client-side, from data already in the browser).
- **Manual light/dark toggle** in the header, independent of the OS theme preference
  (remembered per-browser via `localStorage`).
- **Kiosk / wallboard mode** — a header button requests real fullscreen, hides all
  navigation chrome, forces the port filter to "All", and (only if the page is taller
  than the screen) slowly auto-scrolls top↔bottom so every port cycles into view. Escape
  or the same button exits and restores whatever was showing before.

`/api/status` (plain JSON, same shape as the SSE payload) stays available for
`curl`/scripts that don't want to hold open an SSE connection.

## The settings page

`/settings` — devices, polling knobs, HTTP listen settings, and login credentials, all
editable from the browser. Backed by `GET`/`POST /api/config`:

- **Password changes apply immediately** — no restart needed, so first-run setup
  actually finishes without a restart in between.
- **Everything else (devices, polling, HTTP listen address/port) is file-only** — saving
  writes `wiresprite.ini` and the page tells you to restart wiresprite to apply it. The
  poller and HTTP server's device lists are fixed at process startup and aren't safely
  mutable at runtime; restarting is simpler and safer than building live hot-reload for
  a benefit that's just "skip a one-second restart."
- Submitted config is validated by round-tripping it through the same parser that reads
  `wiresprite.ini` on startup, so anything the settings page will save, a hand-edited
  file in the same shape will also load correctly, and vice versa.

## Prometheus / Grafana

`/metrics` is intentionally left unauthenticated, matching standard Prometheus exporter
convention (a scraper doesn't do cookie/session auth). Point Prometheus at it:

```yaml
scrape_configs:
  - job_name: wiresprite
    static_configs:
      - targets: ["wiresprite-host:8080"]
```

Metrics exposed (see `src/http/routes_metrics.cpp` for exact label sets):

`wiresprite_up`, `wiresprite_scrape_duration_seconds`, `wiresprite_device_uptime_seconds`,
`wiresprite_if_admin_status`, `wiresprite_if_oper_status`, `wiresprite_if_speed_bps`,
`wiresprite_if_in_octets_total`, `wiresprite_if_out_octets_total`,
`wiresprite_if_in_errors_total`, `wiresprite_if_out_errors_total`,
`wiresprite_if_in_discards_total`, `wiresprite_if_out_discards_total`.

Build Grafana panels/alerts against e.g. `rate(wiresprite_if_in_octets_total[5m])` for
long-term traffic history, or `wiresprite_up == 0` for an Alertmanager rule — this
project's own dashboard deliberately doesn't attempt either (see "Dashboard design").

## Dashboard design

The built-in dashboard is intentionally not a Grafana replacement — no charting
library, no client-side dependency, just hand-rolled inline SVG (`createSparkline`/
`updateSparkline` in `src/http/web_assets.cpp`'s embedded `app.js`) driven by the
`history` array `/api/events`/`/api/status` already return per interface. Colors and
mark specs (2px rounded lines, ~10%-opacity area fill, icon+label status pills, hover
crosshair/tooltip, recessive gridlines) come from a validated reference palette rather
than being picked by eye; see that file's CSS custom properties for the exact tokens,
light and dark, plus the `:root[data-theme=...]` overrides the manual theme toggle uses.

## Testing

```sh
cmake --preset <preset> -DWIRESPRITE_BUILD_TESTS=ON
cmake --build --preset <preset>
ctest --test-dir build/<preset> -C Release --output-on-failure
```

Tests use [doctest](https://github.com/doctest/doctest) (vendored) — 100+ test cases,
500+ assertions. Most of the suite is pure unit tests with no I/O; the SNMP client,
poller, and HTTP server are covered by integration tests that run the real UDP/TCP stack
over loopback — including a small in-process fake SNMP agent
(`tests/fake_snmp_agent.hpp`) for exercising WALK/GETBULK without depending on an
external `snmpd` or a reachable physical device.

## Packaging & deployment

See [`packaging/`](../packaging/) for a `systemd` service template (Linux, unattended),
and the root README for pre-built release binaries.
