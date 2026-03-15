# CLAUDE.md — WiFi Scanner CYD

Project-specific instructions for Claude Code.

---

## Project Summary

ESP32-2432S028R (CYD) WiFi scanner with TFT display, XPT2046 touch,
ESPAsyncWebServer dashboard, and WiFiManager credential management.

---

## Architecture

```
main.cpp          — orchestrator: WiFiManager, async scan loop, touch dispatch
display_ui.cpp    — all TFT_eSPI rendering; owns the static TFT_eSPI instance
web_server.cpp    — AsyncWebServer routes + embedded single-file HTML/CSS/JS
config.h          — single source of truth for pins, layout, RGB565 colours
debug.h           — levelled macros (DBG_ERROR/WARN/INFO/VERBOSE)
```

**Key pattern:** `debugLevel` is defined once in `main.cpp` and `extern`-declared
in `debug.h` so all TUs share the same runtime level (required for `/api/debug`
to actually affect log output globally).

---

## Hardware Facts

- **Display SPI bus:** HSPI — CLK=14 MOSI=13 MISO=12 CS=15 DC=2
- **Touch SPI bus:** VSPI (global `SPI` object re-pinned) — CLK=25 MISO=39 MOSI=32 CS=33 IRQ=36
- **Rotation:** `tft.setRotation(1)` + `ts.setRotation(1)` — landscape, USB-C right
- **Backlight:** LEDC channel 0 on GPIO 21 (`ledcSetup` / `ledcWrite`)
- **RGB LED:** active LOW — `LOW` = on, `HIGH` = off

TFT_eSPI auto-selects HSPI when CLK=14/MOSI=13/MISO=12 are used in build flags.
Do NOT define `TOUCH_CS` in TFT_eSPI build flags — touch is handled separately
by `XPT2046_Touchscreen` on the VSPI bus.

---

## Display Layout Constants (config.h)

| Constant      | Value | Purpose                          |
| :------------ | ----: | :------------------------------- |
| `SCREEN_W`    |   320 | Landscape width                  |
| `SCREEN_H`    |   240 | Landscape height                 |
| `HEADER_H`    |    30 | Top title/IP bar                 |
| `FOOTER_H`    |    26 | Bottom buttons bar               |
| `ROW_H`       |    23 | Height of each network row       |
| `MAX_VISIBLE` |     8 | `NET_AREA_H / ROW_H`             |
| `NET_AREA_Y`  |    30 | `== HEADER_H`                    |

Column X positions in `display_ui.cpp`:
`COL_IDX=2 · COL_SSID=22 · COL_BAND=144 · COL_BARS=186 · COL_RSSI=224 · COL_SEC=268`

---

## Touch Zones

- `ty >= SCREEN_H - FOOTER_H` → footer: `tx <= 45` = UP scroll; `tx >= SCREEN_W-75` = SCAN
- `ty < HEADER_H` → header: ignored
- Network area top 25 % → scroll up; bottom 25 % → scroll down; middle → scan

Debounce: `TOUCH_DEBOUNCE_MS = 300` in `config.h`.
Calibration constants `TOUCH_X/Y_MIN/MAX` in `config.h` — adjust if misaligned.

---

## Web API

| Method | Path            | Notes                                        |
| :----- | :-------------- | :------------------------------------------- |
| GET    | `/`             | Embedded HTML — do NOT split to SPIFFS       |
| GET    | `/api/networks` | JSON, reads under `networkMutex`             |
| GET    | `/api/status`   | Heap, uptime, scan count, debug level        |
| POST   | `/api/scan`     | Returns 409 if scan already in progress      |
| GET    | `/api/debug`    | `{"level":N}`                                |
| POST   | `/api/debug`    | Body `{"level":N}`, uses body-handler lambda |

The POST `/api/debug` uses ESPAsyncWebServer's 3-argument `server.on()` overload
with a body handler — **not** `AsyncCallbackJsonWebHandler` (not available in
the installed ESPAsyncWebServer build).

---

## Scan State Machine

Async WiFi scan (`WiFi.scanNetworks(true)`) is polled each `loop()` via
`WiFi.scanComplete()`. No FreeRTOS scan task — the web server runs in its
own task (AsyncTCP), main loop handles display + touch.

Shared data (`networks[]`, `networkCount`, etc.) is protected by
`networkMutex` (FreeRTOS semaphore). Web handlers use `pdMS_TO_TICKS(200)`
timeout to avoid blocking.

---

## Coding Conventions

- Follow user's global style: explicit types, `constexpr` for constants,
  `#define` for pin mappings, `millis()`-based timers, `DBG_*` throughout
- Keep all display logic in `display_ui.cpp` / `display_ui.h`
- Keep all HTTP logic in `web_server.cpp` / `web_server.h`
- The `TFT_eSPI tft` instance is `static` inside `display_ui.cpp` — do not
  expose it externally; add new draw functions to the display API instead
- HTML is embedded in `web_server.cpp` as a raw string literal — keep it
  as a single file (HTML + CSS + JS); do not move to SPIFFS/LittleFS

---

## Known Quirks

- `XPT2046_Touchscreen` (paulstoffregen alpha on PIO) does not accept a
  `SPIClass&` in `begin()`. The fix is to call `SPI.begin(clk, miso, mosi, cs)`
  on the global VSPI object before `ts.begin()`.
- TFT_eSPI emits a `#warning` about missing `TOUCH_CS` — this is expected and
  safe; suppress it by noting it in reviews.
- WiFiManager portal timeout is 120 s; on timeout `ESP.restart()` is called.
