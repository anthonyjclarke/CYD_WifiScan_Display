# Changelog

All notable changes to this project will be documented in this file.

The format is based on Keep a Changelog and this project uses Semantic Versioning.

## [1.2.0] 09-10-2026

Browser installer release.

### Added

- Web installer at https://anthonyjclarke.github.io/CYD_WifiScan_Display/ (ESP Web Tools, via cyd-web-installer): install, Update and Configure WiFi from Chrome or Edge.
- Improv-Serial, always on, in its own FreeRTOS task so it answers during the 2–4 s synchronous scans. A provisioned board is offered **Update** (settings kept); a new one gets **Configure WiFi** over USB.
- `Firmware` CI workflow: builds every push; a `v*` tag on `main` publishes the release (`*-firmware.bin`, `*-merged.bin`, `SHA256SUMS.txt`) and the installer page.
- Vendored Improv library at cyd-web-installer 1.0.1: each packet starts on a new line, so Connect reliably offers **Update** even when the serial stream opens mid-line.
- `FIRMWARE_VERSION`, `PROJECT_NAME` and `AP_NAME` in `config.h`; the boot screen and log show the version, and the log shows the running OTA partition.

### Changed

- Pinned `platform = espressif32@6.12.0` (arduino-esp32 2.0.17). Unpinned builds now pull arduino-esp32 3.x and fail.
- Partition table: `default.csv` → standard dual-OTA `partitions_custom.csv` (2 × 1.75 MB app slots). NVS stays at `0x9000`, so WiFi credentials survive an Update; the unused filesystem partition is reformatted.

### Removed

- Stray `.vscode/* (from New Work Laptop).json` copies and `CLAUDE.md.bak`.

## [1.1.0] 08-04-2026

Documentation and service update.

### Changed

- Corrected the `/api/networks` contract to report `scanAge` seconds instead of an uptime-derived timestamp.
- Updated the web dashboard to display the last scan age correctly.
- Made `/api/networks` fail safely with `503` if shared scan data is temporarily locked.
- Fixed the default `upload_port` value in `platformio.ini`.
- Brought README, CLAUDE notes, and debug API documentation back in sync with the implementation.

## [1.0.0] 15-03-2026

Initial release.

### Added

- WiFi scanning for nearby 2.4 GHz networks on the ESP32 CYD.
- TFT list view with RSSI bars, security indicator, and touch navigation.
- TFT channel congestion graph with colour-coded contention levels.
- Web dashboard with live network table, congestion chart, and per-channel contributor drilldown.
- WiFiManager captive portal for first-time setup and stored credentials.
- Debug level control over serial and HTTP.

### Notes

- Hardware target is the ESP32-2432S028R CYD using 2.4 GHz Wi-Fi.
- Channel contention is modelled from scanned AP channel overlap on channels 1-14.
