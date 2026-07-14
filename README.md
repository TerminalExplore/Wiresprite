# Wiresprite

A lightweight, cross-platform SNMP switch/router monitor with a live web dashboard,
written in C++. Originally built for an old HP ProCurve 2512, but works with any SNMP
v1/v2c-speaking device — switches, routers, and servers from any vendor. It polls
standard MIBs (IF-MIB, `ifXTable`, BRIDGE-MIB, MIB-II), so no vendor-specific
configuration is needed.

Ships as a single self-contained binary — no Net-SNMP, no OpenSSL, no runtime dependency
beyond what the OS already provides. A fresh install needs no config file: run the
binary, and it opens your browser to a setup page automatically.

**[Live demo](https://terminalexplore.github.io/Wiresprite/)** — a static preview of the
dashboard with simulated homelab traffic (several switches, continuous traffic, no
backend required to view it). See [`cpp/README.md`](cpp/README.md) for the full feature
list, architecture, build instructions, configuration schema, and Prometheus/Grafana
integration details.

## Quick start

```sh
git clone https://github.com/TerminalExplore/Wiresprite.git
cd Wiresprite/cpp
cmake --preset windows-msvc-release   # or linux-gcc-release
cmake --build --preset windows-msvc-release
./build/windows-msvc-release/Release/wiresprite.exe
```

No config file needed to start — it creates one, opens `http://localhost:8080` in your
browser, and walks you through setting a password and adding your first device from
there. (Prefer to configure by hand instead? Copy `cpp/config/wiresprite.ini.example` to
`wiresprite.ini` first and edit it — see `cpp/README.md`'s Configuration section for the
full schema.)

### Pre-built binaries and running as a service

Tagged releases (`v*`) are built and published automatically — see the
[Releases](https://github.com/TerminalExplore/Wiresprite/releases) page for
ready-to-run Windows/Linux binaries, no compiler needed. For running unattended on
Linux, see [`packaging/`](packaging/) for a systemd service template.

### Compatibility with other devices

This project is not limited to the HP ProCurve 2512. It works with any network device
that supports SNMP — switches, routers, and servers from any manufacturer (Cisco,
MikroTik, Netgear, D-Link, and so on) — since it walks standard MIBs rather than
vendor-specific OIDs. You'll just need to enable SNMP on the target device and configure
its community string (e.g. `public`) to match what you enter for it, whether that's in
`wiresprite.ini` or the `/settings` page.

## History

This started as a Python/Flask prototype, was rewritten in C++ for a smaller,
dependency-free binary with multi-device support, a background poller, and Prometheus
metrics, and was later rebranded to Wiresprite (including the GitHub repository itself)
alongside a full dashboard redesign (live updates, MAC-address/port-name lookups, a
browser-based settings page, and more — see `cpp/README.md`). The Python version is
preserved in git history but is no longer part of the active project.
