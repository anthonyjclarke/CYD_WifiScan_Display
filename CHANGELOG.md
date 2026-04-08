# Changelog

All notable changes to this project will be documented in this file.

The format is based on Keep a Changelog and this project uses Semantic Versioning.

## [1.1.0] - 2026-04-08

Documentation and service update.

### Changed

- Corrected the `/api/networks` contract to report `scanAge` seconds instead of an uptime-derived timestamp.
- Updated the web dashboard to display the last scan age correctly.
- Made `/api/networks` fail safely with `503` if shared scan data is temporarily locked.
- Fixed the default `upload_port` value in `platformio.ini`.
- Brought README, CLAUDE notes, and debug API documentation back in sync with the implementation.

## [1.0.0] - 2026-03-15

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
