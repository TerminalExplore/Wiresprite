# SNMP-Monitor

A lightweight, cross-platform SNMP switch/router monitor, written in C++. Originally built
for an old HP ProCurve 2512, but works with any SNMP v1/v2c-speaking device — switches,
routers, and servers from any vendor. It polls the standard IF-MIB (`ifTable`), so no
vendor-specific configuration is needed. The project collects statistics about ports,
transmitted traffic, and errors, and exposes them through a live web dashboard and a
Prometheus `/metrics` endpoint.

Ships as a single self-contained binary — no Net-SNMP, no OpenSSL, no runtime dependency
beyond what the OS already provides.

See [`cpp/README.md`](cpp/README.md) for architecture, build instructions, configuration
format, and Prometheus/Grafana integration details.

## Quick start

```sh
git clone https://github.com/TerminalExploit/SNMP-Monitor.git
cd SNMP-Monitor/cpp
cmake --preset windows-msvc-release   # or linux-gcc-release
cmake --build --preset windows-msvc-release
cp config/wiresprite.ini.example wiresprite.ini   # edit with your device(s)
./build/windows-msvc-release/Release/wiresprite.exe wiresprite.ini
```

Then open `http://localhost:8080` in a browser.

### Pre-built binaries and running as a service

Tagged releases (`v*`) are built and published automatically — see the
[Releases](https://github.com/TerminalExploit/SNMP-Monitor/releases) page for
ready-to-run Windows/Linux binaries, no compiler needed. For running unattended on
Linux, see [`packaging/`](packaging/) for a systemd service template.

### Compatibility with other devices

This project is not limited to the HP ProCurve 2512. It works with any network device
that supports SNMP — switches, routers, and servers from any manufacturer (Cisco,
MikroTik, Netgear, D-Link, and so on) — since it walks the standard IF-MIB rather than
vendor-specific OIDs. You'll just need to enable SNMP on the target device and configure
its community string (e.g. `public`) to match your `wiresprite.ini`.

## History

This started as a Python/Flask prototype and was later rewritten in C++ for a smaller,
dependency-free binary, multi-device support, a background poller, and Prometheus
metrics. The Python version is preserved in git history but is no longer part of the
active project.
