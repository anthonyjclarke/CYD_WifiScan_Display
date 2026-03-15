# WiFi Scanner v1.0.0 — ESP32 CYD

Visual WiFi scanner for the **ESP32-2432S028R (Cheap Yellow Display)**.
Displays nearby 2.4 GHz networks with signal strength, channel congestion,
and security on the 320×240 ILI9341 TFT with touch navigation.
A built-in web UI mirrors the scan list, channel graph, and contention details
in any browser on the same network.

Current release: **v1.0.0**

See [CHANGELOG.md](CHANGELOG.md) for release history.

---

## Features

| Feature       | Detail                                                         |
| :------------ | :------------------------------------------------------------- |
| Display       | Network list view plus 2.4 GHz channel congestion graph        |
| Touch         | Footer `VIEW` toggle · tap graph/list centre = scan · top/bottom list scroll |
| Web UI        | Dashboard, channel graph, contributor drilldown, debug control |
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

### List View

```
+--------------------------------------------------+  ← 30 px header
| WiFi Scanner       * SCANNING *   192.168.1.100  |
+--------------------------------------------------+
| #  SSID               BAND   BARS   RSSI   SEC   |  ← 8 rows × 23 px
| 1  MyNetwork          2.4G   █████   -48   *      |
| 2  GuestNet           2.4G   ████    -61   o      |
| 3  GuestNet           2.4G   ██      -76   o      |
| ...                                               |
+--------------------------------------------------+  ← 26 px footer
| VIEW  LIST  8 nets  12s ago          SCAN v       |
+--------------------------------------------------+
```

### Channel View

The alternate display view renders a 14-channel 2.4 GHz congestion graph with:

- colour-coded bars for low to severe contention
- channel counts above active bars
- emphasis on channels 1, 6, and 11
- hottest channel label in the graph header

**Security:** `*` = secured (orange) · `o` = open (green)

**Congestion colours:** green low · yellow moderate · orange high · red severe

---

## Web API

| Method | Endpoint        | Description                             |
| :----- | :-------------- | :-------------------------------------- |
| GET    | `/`             | HTML dashboard                          |
| GET    | `/api/networks` | JSON network list plus channel summary  |
| GET    | `/api/status`   | Heap, uptime, scan count, debug level   |
| POST   | `/api/scan`     | Trigger immediate scan                  |
| GET    | `/api/debug`    | `{"level": N}`                          |
| POST   | `/api/debug`    | Body `{"level": N}` — sets 0–4 runtime  |

The web dashboard includes a channel congestion graph. Clicking a channel
shows the specific scanned networks contributing to that channel's overlap,
grouped into on-channel and adjacent-channel contributors with overlap weights.

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
  main.cpp          — setup / loop, scan logic, channel summary, touch
  display_ui.cpp    — TFT network list + congestion graph rendering
  web_server.cpp    — ESPAsyncWebServer + embedded HTML dashboard
include/
  config.h          — pin definitions, layout constants, RGB565 colours
  debug.h           — levelled debug macros (DBG_ERROR/WARN/INFO/VERBOSE)
  display_ui.h      — display API + scan/channel shared structs
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
| mathieucarbou/ESP Async WebServer | ^3.0.6 | Non-blocking HTTP server |
| mathieucarbou/AsyncTCP   | ^3.3.2   | Async TCP layer            |
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
