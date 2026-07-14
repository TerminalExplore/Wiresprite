# Packaging

## Pre-built binaries

Pushing a tag matching `v*` (e.g. `v1.0.0`) triggers
`.github/workflows/release.yml`, which builds Windows and Linux release
binaries and publishes them as a GitHub Release, each zipped up with
`wiresprite.ini.example` and the root `README.md`. This is separate from
`ci.yml` (which runs on every push/PR and never publishes anything) so
cutting a release stays a deliberate, explicit action — nothing gets
published just from merging to `master`.

## Running as a systemd service (Linux)

`wiresprite.service` is a template unit, not something the build installs
automatically. To use it:

```sh
sudo useradd --system --no-create-home wiresprite
sudo mkdir -p /opt/wiresprite
sudo cp wiresprite /opt/wiresprite/
sudo cp wiresprite.ini.example /opt/wiresprite/wiresprite.ini   # then edit it
sudo chown -R wiresprite:wiresprite /opt/wiresprite

sudo cp packaging/wiresprite.service /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl enable --now wiresprite
```

Check it with `systemctl status wiresprite` / `journalctl -u wiresprite -f`.
