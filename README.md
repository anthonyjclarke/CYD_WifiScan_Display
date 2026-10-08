# WiFi Scanner v1.2.0 — ESP32 CYD

Visual WiFi scanner for the **ESP32-2432S028R (Cheap Yellow Display)**.
Displays nearby 2.4 GHz networks with signal strength, channel congestion,
and security on the 320×240 ILI9341 TFT with touch navigation.
A built-in web UI mirrors the scan list, channel graph, and contention details
in any browser on the same network.

Current release: **v1.2.0**

See [CHANGELOG.md](CHANGELOG.md) for release history.

---

## Install

**[anthonyjclarke.github.io/CYD_WifiScan_Display][installer]** installs the
latest release from the browser – no PlatformIO, no drivers to build. It needs
desktop Chrome, Edge or Opera.

1. Pick the board – CYD 2.8″ (ESP32-2432S028R).
2. Plug it in with a USB data cable, click **Connect & install** and choose its
   port.
3. On a new board, say yes to erasing it. When flashing finishes, choose
   **Configure WiFi** and pick your network. (Or skip it and join the
   `WiFiScanner-AP` hotspot as in [First Boot](#first-boot).)
4. **Visit device** opens the scanner's web dashboard.

A board already running this firmware is recognised and offered **Update**,
which keeps its WiFi credentials. Each [release][releases] also carries the
images for flashing by hand. `*-firmware.bin` is the app alone (offset
`0x10000`; this firmware has no web update page). `*-merged.bin` is a clean
install at `0x0` with esptool, and it **erases settings and WiFi**. Nothing
else is needed – no API keys or accounts.

**Upgrading from v1.1.0 or earlier:** older firmware can't identify itself
to the installer, so it is offered **Install** rather than Update. Answer
**No** to erasing and your WiFi credentials are kept. v1.2.0 also moves to the
standard dual-OTA partition table; NVS stays where it was, and the unused
filesystem partition is reformatted.

[installer]: https://anthonyjclarke.github.io/CYD_WifiScan_Display/
[releases]: https://github.com/anthonyjclarke/CYD_WifiScan_Display/releases

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
| GET    | `/api/networks` | JSON network list plus channel summary, including `scanAge` seconds  |
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

## Building

PlatformIO, one env: `pio run -e esp32-cyd -t upload`. The platform is pinned
to `espressif32@6.12.0` (arduino-esp32 2.0.17); an unpinned build pulls
arduino-esp32 3.x and fails.

Release images are built only by CI, from a `v*` tag on `main`
(`.github/workflows/firmware.yml`, using
[cyd-web-installer](https://github.com/anthonyjclarke/cyd-web-installer)).
**Never publish a local build.** To try the installer from a local build,
assemble the site and serve it on localhost (Web Serial needs a secure origin,
and localhost counts):

```bash
python3 ../cyd-web-installer/tools/make_manifests.py --out _site
```

```bash
python3 -m http.server 8000 --directory _site
```

---

## Project Structure

```
src/
  main.cpp          — setup / loop, scan logic, channel summary, touch
  display_ui.cpp    — TFT network list + congestion graph rendering
  web_server.cpp    — ESPAsyncWebServer + embedded HTML dashboard
  network/improv_setup.*  — Improv-Serial for the web installer (copy-in)
lib/ImprovWiFi/     — vendored Improv library with parser fix (copy-in)
tools/merge_bin.py  — post-build: flash_parts.json + merged image
include/
  config.h          — pin definitions, layout constants, RGB565 colours
  debug.h           — levelled debug macros (DBG_ERROR/WARN/INFO/VERBOSE)
  display_ui.h      — display API + scan/channel shared structs
  web_server.h      — web server init signature
platformio.ini      — board, libraries, TFT_eSPI build flags
partitions_custom.csv — 4 MB dual-OTA layout (frozen)
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
