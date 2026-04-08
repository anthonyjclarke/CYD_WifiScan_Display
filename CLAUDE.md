# Project: CYD WiFi Scanner Display

## Overview
ESP32-2432S028R (Cheap Yellow Display) WiFi scanner that shows nearby 2.4 GHz networks on a 320×240 ILI9341 TFT with touch navigation. Displays a scrollable network list (SSID, signal bars, RSSI, channel, band, security) and a channel congestion graph. A built-in ESPAsyncWebServer dashboard mirrors all data over HTTP with live updates. WiFiManager handles first-time credential setup via captive portal.

## Hardware
- **MCU:** ESP32-2432S028R (ESP32-WROOM-32, dual-core 240 MHz, 4 MB flash)
- **Display:** 2.8" ILI9341 TFT — 320×240, SPI on HSPI bus
- **Touch:** XPT2046 resistive touch — SPI on VSPI bus (separate from display)
- **Onboard RGB LED:** active LOW on GPIOs 4 / 16 / 17
- **LDR:** ambient-light sensor on GPIO 34 (unused by firmware currently)
- **Power:** USB-C 5 V

## Build Environment
- **Framework:** Arduino
- **Platform:** espressif32
- **Board:** esp32dev
- **Key Libraries:**
  - `bodmer/TFT_eSPI @ ^2.5.43`
  - `paulstoffregen/XPT2046_Touchscreen` (alpha — quirks below)
  - `tzapu/WiFiManager @ ^2.0.17`
  - `mathieucarbou/ESP Async WebServer @ ^3.0.6`
  - `mathieucarbou/AsyncTCP @ ^3.3.2`
  - `bblanchon/ArduinoJson @ ^6.21.0`

## Project Structure
```
include/
  config.h          — pin #defines, layout constexprs, RGB565 colour #defines
  debug.h           — DBG_ERROR/WARN/INFO/VERBOSE macros; extern debugLevel
  display_ui.h      — NetworkEntry / ChannelStat structs, UIViewMode enum, display API
  web_server.h      — initWebServer() declaration
src/
  main.cpp          — setup/loop, WiFiManager, scan state machine, touch dispatch
  display_ui.cpp    — all TFT_eSPI rendering; owns the static TFT_eSPI instance
  web_server.cpp    — AsyncWebServer routes + embedded single-file HTML/CSS/JS
platformio.ini
```

## Pin Mapping
| Function         | GPIO | Notes                                      |
|------------------|------|--------------------------------------------|
| TFT CLK (HSPI)   | 14   | TFT_eSPI build flag `-DTFT_SCLK=14`        |
| TFT MOSI (HSPI)  | 13   | `-DTFT_MOSI=13`                            |
| TFT MISO (HSPI)  | 12   | `-DTFT_MISO=12`                            |
| TFT CS           | 15   | `-DTFT_CS=15`                              |
| TFT DC           | 2    | `-DTFT_DC=2`                               |
| TFT RST          | —    | `-DTFT_RST=-1` (tied to EN)                |
| TFT Backlight    | 21   | LEDC channel 0 PWM                         |
| Touch CLK (VSPI) | 25   | `SPI.begin()` called before `ts.begin()`   |
| Touch MISO       | 39   | Input-only GPIO                            |
| Touch MOSI       | 32   |                                            |
| Touch CS         | 33   | `TOUCH_CS_PIN`                             |
| Touch IRQ        | 36   | Input-only GPIO, no pull-up                |
| RGB LED R        | 4    | Active LOW                                 |
| RGB LED G        | 16   | Active LOW                                 |
| RGB LED B        | 17   | Active LOW                                 |
| LDR              | 34   | Input-only GPIO (unused in firmware)       |

**Important:** Do NOT define `TOUCH_CS` in TFT_eSPI build flags — touch runs on a separate VSPI bus managed by `XPT2046_Touchscreen`.

## Configuration
- **Config file:** `include/config.h`
- **Key settings:**
  - `SCAN_INTERVAL_MS = 30000` — auto-rescan every 30 s
  - `MAX_NETWORKS = 20` — max AP entries stored
  - `TOUCH_X/Y_MIN/MAX` — calibration constants; adjust after first boot if misaligned
  - `TOUCH_DEBOUNCE_MS = 300`
  - All RGB565 colour constants (`COL_*`)
  - Display layout constants (`SCREEN_W/H`, `HEADER_H`, `FOOTER_H`, `ROW_H`, `MAX_VISIBLE`)

### Display Layout
| Constant      | Value | Purpose                     |
|:--------------|------:|:----------------------------|
| `SCREEN_W`    |   320 | Landscape width             |
| `SCREEN_H`    |   240 | Landscape height            |
| `HEADER_H`    |    30 | Top title/IP bar            |
| `FOOTER_H`    |    26 | Bottom buttons bar          |
| `ROW_H`       |    23 | Height of each network row  |
| `MAX_VISIBLE` |     8 | `NET_AREA_H / ROW_H`        |
| `NET_AREA_Y`  |    30 | `== HEADER_H`               |

Column X positions in `display_ui.cpp`:
`COL_IDX=2 · COL_SSID=22 · COL_BAND=144 · COL_BARS=186 · COL_RSSI=224 · COL_SEC=268`

### Touch Zones
- `ty >= SCREEN_H - FOOTER_H` → footer: `tx <= 45` = VIEW toggle; `tx >= SCREEN_W-75` = SCAN
- `ty < HEADER_H` → header: ignored
- Network area top 25% → scroll up; bottom 25% → scroll down; middle → trigger scan
- Channel view: tap graph area → trigger scan

### Web API
| Method | Path            | Response / Notes                                                             |
|:-------|:----------------|:-----------------------------------------------------------------------------|
| GET    | `/`             | Embedded HTML (~7 KB in RAM) — do NOT split to SPIFFS                       |
| GET    | `/api/networks` | JSON object: `{count, scanAge, scanning, networks[], channels[]}` — mutex or 503 if busy |
| GET    | `/api/status`   | JSON: `{ip, heap, uptime, scanCount, scanning, debugLevel}`                  |
| POST   | `/api/scan`     | 202 `scan_started` or 409 `scan_already_running`                             |
| GET    | `/api/debug`    | `{"level":N}`                                                                |
| POST   | `/api/debug`    | Body `{"level":N}` (0–4); body-handler lambda                               |

`/api/networks` includes both the network list and the per-channel stats (used by the web congestion chart). Web UI auto-refreshes: networks every **10 s**, status every **5 s**.

POST `/api/debug` uses ESPAsyncWebServer's 3-argument `server.on()` overload with a body handler — **not** `AsyncCallbackJsonWebHandler` (not available in the installed build).

**Note:** `debug.h` has an outdated comment saying `POST /api/debug body: level=N` — the actual implementation uses JSON `{"level":N}`.

## Current State
v1.1.0 — current development release (2026-04-08). Core features complete and working:
- WiFi scanning with list and channel congestion views on TFT
- Touch navigation (scroll, scan trigger, view toggle)
- Web dashboard with live network table, congestion chart, per-channel drilldown
- WiFiManager captive portal for credential management
- Runtime debug level control via serial and HTTP
- Correct web scan-age reporting and safe mutex handling in `/api/networks`

## Architecture Notes

**Scan state machine:** Uses synchronous `WiFi.scanNetworks(false)` rather than async. Async scanning (`WiFi.scanNetworks(true)`) fails with `WIFI_SCAN_FAILED` immediately when ESPAsyncWebServer is running — AsyncTCP on core 0 shares the radio and aborts the scan. Synchronous scan (~2 s) blocks `loop()` but the web server task stays live.

**Thread safety:** Shared data (`networks[]`, `networkCount`, `channelStats`) is protected by `networkMutex` (FreeRTOS semaphore). Web handlers use `pdMS_TO_TICKS(200)` timeout. `scanRequested` flag is the safe cross-task trigger — web/touch call `requestScan()`, only `loop()` calls `doScan()`.

**Debug level:** `debugLevel` is defined once in `main.cpp` and `extern`-declared in `debug.h` so all translation units share the same runtime level. This is required for `POST /api/debug` to affect log output globally.

**TFT instance:** `static TFT_eSPI tft` lives inside `display_ui.cpp` — never expose it externally. Add new drawing functions to the display API declared in `display_ui.h`.

**HTML:** Embedded in `web_server.cpp` as a raw string literal (HTML + CSS + JS in one string). Do not move to SPIFFS/LittleFS.

**Coding conventions:** Explicit types over `auto`, `constexpr` for computed constants, `#define` for pin mappings, `millis()`-based timers, `DBG_*` macros throughout.

## Known Issues
- `XPT2046_Touchscreen` (paulstoffregen alpha on PIO) does not accept a `SPIClass&` in `begin()`. Fix: call `SPI.begin(clk, miso, mosi, cs)` on the global VSPI object before `ts.begin()`.
- TFT_eSPI emits a `#warning` about missing `TOUCH_CS` — expected and safe; ignore in reviews.
- WiFiManager portal timeout is 120 s; on timeout `ESP.restart()` is called.
- LDR on GPIO 34 is wired but not used by current firmware.
- `platformio.ini` `upload_port` has a doubled path prefix: `/dev/cu./dev/cu.usbserial-210` — fix to `/dev/cu.usbserial-210` before uploading on a different machine.
- `debug.h` comment says `POST /api/debug body: level=N` but the implementation uses JSON `{"level":N}` — comment is stale.
- Signal bars skip value 3: strong=5, good=4, fair=2, weak=1 bars (intentional visual weighting).

## TODO
- [ ] Use LDR reading to auto-dim backlight
- [ ] Add 5 GHz network detection/display (hardware is 2.4 GHz only, but channel > 14 logic exists)
- [ ] Persist WiFi credentials reset option via long-press touch
- [ ] Add OTA update support
