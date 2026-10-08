# Project: CYD WiFi Scanner Display

ESP32-2432S028R WiFi scanner displaying nearby 2.4 GHz networks on 320×240 ILI9341 TFT with touch navigation and HTTP dashboard. v1.2.0-dev; `FIRMWARE_VERSION` in `config.h`.

## Hardware
- **MCU:** ESP32-2432S028R (dual-core 240 MHz)
- **Display:** ILI9341 TFT 320×240, SPI (HSPI)
- **Touch:** XPT2046 resistive, SPI (VSPI — separate bus from display)
- **RGB LED:** active LOW on GPIOs 4 / 16 / 17
- **Power:** USB-C 5 V

## Pin Mapping
| Function | GPIO | Notes |
|----------|------|-------|
| TFT CLK | 14 | HSPI |
| TFT MOSI | 13 | HSPI |
| TFT MISO | 12 | HSPI |
| TFT CS | 15 | |
| TFT DC | 2 | |
| TFT Backlight | 21 | LEDC PWM |
| Touch CLK | 25 | VSPI (separate) |
| Touch MISO | 39 | Input-only |
| Touch MOSI | 32 | |
| Touch CS | 33 | |
| Touch IRQ | 36 | Input-only |
| RGB LED R | 4 | Active LOW |
| RGB LED G | 16 | Active LOW |
| RGB LED B | 17 | Active LOW |

**Critical:** Do NOT define `TOUCH_CS` in TFT_eSPI build flags — touch runs on separate VSPI managed by `XPT2046_Touchscreen`. Call `SPI.begin()` on global VSPI before `ts.begin()`.

## Libraries
- bodmer/TFT_eSPI @ ^2.5.43
- paulstoffregen/XPT2046_Touchscreen (alpha — requires manual SPI.begin() before ts.begin())
- tzapu/WiFiManager @ ^2.0.17
- mathieucarbou/ESP Async WebServer @ ^3.0.6
- mathieucarbou/AsyncTCP @ ^3.3.2
- bblanchon/ArduinoJson @ ^6.21.0

## Architecture

**Scan state machine:** Synchronous `WiFi.scanNetworks(false)` only. Async scanning fails with `WIFI_SCAN_FAILED` when ESPAsyncWebServer is running — AsyncTCP on core 0 conflicts with radio. Synchronous scan (~2 s) blocks `loop()` but web server task persists.

**Thread safety:** Shared data (`networks[]`, `networkCount`, `channelStats`) protected by `networkMutex` (FreeRTOS semaphore). Web handlers use `pdMS_TO_TICKS(200)` timeout. Cross-task trigger is `scanRequested` flag — web/touch call `requestScan()`, only `loop()` calls `doScan()`.

**Debug level:** Single `debugLevel` in `main.cpp`, `extern` in `debug.h` — all translation units see the same runtime level. Required for `POST /api/debug` to affect logging globally.

**TFT instance:** Static inside `display_ui.cpp`, never expose externally. All drawing via `display_ui.h` API.

**HTML:** Embedded raw string literal in `web_server.cpp` (HTML + CSS + JS single string). Do not move to SPIFFS/LittleFS.

**Web API:** `/api/networks` returns JSON with `networks[]` and `channels[]`. Mutex guard with 200 ms timeout, returns 503 if locked. POST `/api/debug` body: `{"level":N}` (uses 3-arg `server.on()` body handler, not `AsyncCallbackJsonWebHandler`).

## Configuration
- **File:** `include/config.h`
- `SCAN_INTERVAL_MS = 30000`
- `MAX_NETWORKS = 20`
- `TOUCH_DEBOUNCE_MS = 300`
- `TOUCH_X/Y_MIN/MAX` — calibration constants
- Layout: `SCREEN_W=320, SCREEN_H=240, HEADER_H=30, FOOTER_H=26, ROW_H=23, MAX_VISIBLE=8`
- NVS namespace: `wifiscan` (WiFiManager default)

## Hardware Quirks
- `XPT2046_Touchscreen` alpha build does not accept `SPIClass&` in `begin()` — workaround: pre-call `SPI.begin(clk, miso, mosi, cs)` on global VSPI object.
- TFT_eSPI emits `#warning` about missing `TOUCH_CS` — expected and safe.
- Signal bars skip value 3: strong=5, good=4, fair=2, weak=1 (intentional visual weighting).
- WiFiManager portal timeout 120 s; calls `ESP.restart()` on timeout.
- LDR (GPIO 34) wired but unused in firmware.

## Web installer and releases
- Release images come only from CI on a `v*` tag on `main`; never publish a local build. Never put `firmware-merged.bin` in a manifest.
- `PROJECT_NAME` and `partitions_custom.csv` are frozen (a rename turns Update into Install; a layout change needs an erase note).
- Improv is vendored in `lib/ImprovWiFi/` — never add it to `lib_deps`. `improvTick()` runs in `improvTask` (main.cpp), not `loop()`, because a scan blocks `loop()` 2–4 s; it must keep running at least every ~1 s. Only that task reads Serial.
