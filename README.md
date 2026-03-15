# WiFi Scanner — ESP32 CYD

Visual WiFi scanner for the **ESP32-2432S028R (Cheap Yellow Display)**.
Displays nearby networks with signal strength, band, and security on the
320×240 ILI9341 TFT with colour coding and touch navigation.
A built-in web UI mirrors everything on any browser on the same network.

---

## Features

| Feature       | Detail                                                         |
| :------------ | :------------------------------------------------------------- |
| Display       | Color-coded signal bars, 2.4 / 5 GHz badges, lock indicator   |
| Touch         | Tap centre = scan · top/bottom swipe = scroll · UP button      |
| Web UI        | Dark-themed dashboard, auto-refresh, signal bars, debug ctrl  |
| WiFiManager   | Captive-portal setup on first boot (AP: `WiFiScanner-AP`)      |
| Auto-scan     | Rescans every 30 s; manual trigger via touch or `/api/scan`    |
| Debug levels  | 0–4 at runtime via serial or `POST /api/debug`                 |

---

## Hardware

**Board:** ESP32-2432S028R (CYD — Cheap Yellow Display)

| Peripheral | Bus   | Pins                                    |
| :--------- | :---- | :-------------------------------------- |
| ILI9341    | HSPI  | CLK=14 MOSI=13 MISO=12 CS=15 DC=2      |
| XPT2046    | VSPI  | CLK=25 MOSI=32 MISO=39 CS=33 IRQ=36    |
| Backlight  | LEDC  | GPIO 21 (PWM ch 0, full brightness)     |
| RGB LED    | GPIO  | R=4 G=16 B=17 (active LOW)             |
| LDR        | ADC   | GPIO 34                                 |

---

## Display Layout (landscape 320×240)

```
+--------------------------------------------------+  ← 30 px header
| WiFi Scanner       * SCANNING *   192.168.1.100  |
+--------------------------------------------------+
| #  SSID               BAND   BARS   RSSI   SEC   |  ← 8 rows × 23 px
| 1  MyNetwork          2.4G   █████   -48   *      |
| 2  Neighbour_5G       5G     ████    -61   *      |
| 3  GuestNet           2.4G   ██      -76   o      |
| ...                                               |
+--------------------------------------------------+  ← 26 px footer
| ^ UP    8 nets  12s ago              SCAN v       |
+--------------------------------------------------+
```

**Signal colours:** green > −50 · cyan −50→−65 · yellow −65→−75 · red < −75 dBm

**Security:** `*` = secured (orange) · `o` = open (green)

---

## Web API

| Method | Endpoint        | Description                             |
| :----- | :-------------- | :-------------------------------------- |
| GET    | `/`             | HTML dashboard                          |
| GET    | `/api/networks` | JSON network list                       |
| GET    | `/api/status`   | Heap, uptime, scan count, debug level   |
| POST   | `/api/scan`     | Trigger immediate scan                  |
| GET    | `/api/debug`    | `{"level": N}`                          |
| POST   | `/api/debug`    | Body `{"level": N}` — sets 0–4 runtime  |

---

## First Boot

1. Power on the CYD — display shows **"WiFi Setup"**
2. Connect phone/laptop to **`WiFiScanner-AP`**
3. Browser opens captive portal (or navigate to `192.168.4.1`)
4. Enter your WiFi credentials → device reboots and connects
5. IP address appears in the header; open it in a browser

Credentials are saved to NVS — subsequent boots connect automatically.

---

## Project Structure

```
src/
  main.cpp          — setup / loop, WiFiManager, scan logic, touch
  display_ui.cpp    — TFT rendering (TFT_eSPI)
  web_server.cpp    — ESPAsyncWebServer + embedded HTML dashboard
include/
  config.h          — pin definitions, layout constants, RGB565 colours
  debug.h           — levelled debug macros (DBG_ERROR/WARN/INFO/VERBOSE)
  display_ui.h      — display API + NetworkEntry struct
  web_server.h      — web server init signature
platformio.ini      — board, libraries, TFT_eSPI build flags
```

---

## Dependencies

| Library                  | Version  | Purpose                    |
| :----------------------- | :------- | :------------------------- |
| bodmer/TFT_eSPI          | ^2.5.43  | ILI9341 display driver     |
| paulstoffregen/XPT2046   | latest   | Resistive touch controller |
| tzapu/WiFiManager        | ^2.0.17  | Captive-portal WiFi setup  |
| me-no-dev/ESPAsyncWebServer | ^1.2.3 | Non-blocking HTTP server   |
| me-no-dev/AsyncTCP       | ^1.1.1   | Async TCP layer             |
| bblanchon/ArduinoJson    | ^6.21.0  | JSON serialisation         |

---

## Touch Calibration

Default calibration constants are in `include/config.h`:

```cpp
constexpr int TOUCH_X_MIN = 200;
constexpr int TOUCH_X_MAX = 3800;
constexpr int TOUCH_Y_MIN = 300;
constexpr int TOUCH_Y_MAX = 3700;
```

If touch is misaligned, read raw values via `DBG_VERBOSE` output
(set debug level 4) and adjust these constants.

---

## Debug Levels

| Level | Macro        | Output                            |
| :---: | :----------- | :-------------------------------- |
| 0     | —            | Silent                            |
| 1     | `DBG_ERROR`  | Critical failures only            |
| 2     | `DBG_WARN`   | Warnings + errors                 |
| 3     | `DBG_INFO`   | General flow (default)            |
| 4     | `DBG_VERBOSE`| Touch coords, per-network detail  |

Change at runtime: `POST /api/debug` body `{"level": 4}`
