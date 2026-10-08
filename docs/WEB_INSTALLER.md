# Web installer – CYD_WifiScan_Display

This records how the project adopted the shared
[cyd-web-installer](https://github.com/anthonyjclarke/cyd-web-installer)
tooling in v1.2.0, and the hardware test results. The pilot and the reference
for every rule is CYD_AnimatedPixelClock (`docs/WEB_INSTALLER_PLAN.md`).

---

## Project decisions

| Item              | Choice                                     |
|:------------------|:-------------------------------------------|
| Envs / boards     | One: `esp32-cyd` – CYD 2.8″ ESP32-2432S028R |
| Platform          | `espressif32@6.12.0` (arduino-esp32 2.0.17) |
| Partition table   | `default.csv` → `partitions_custom.csv`    |
| App slot / image  | 1,835,008 B slot · ~1.0 MB image (55%)     |
| Secrets           | None – WiFiManager only, CI needs nothing  |
| OTA               | None – `app1` test case is N/A             |
| Improv            | Always on, in its own FreeRTOS task        |
| Setup AP          | `WiFiScanner-AP`                           |

**Partition switch.** v1.1.0 used `default.csv` (1.25 MB slots). The first
installer release moves to the standard dual-OTA layout so the layout is fixed
from here on. NVS stays at `0x9000`, so an Update keeps WiFi credentials; the
unused data partition is reformatted. The release notes say so.

**Why Improv runs in a task.** `doScan()` uses a synchronous
`WiFi.scanNetworks()` (async scans fail with AsyncTCP running), which blocks
`loop()` for 2–4 s every 30 s. ESP Web Tools waits only 1.5 s for Improv on
Connect, so ticking from `loop()` would miss it during a scan and offer
Install instead of Update. `improvTask` (priority 1, core 1, 10 ms period)
keeps answering through the scan wait, the WiFiManager connect wait and its
blocking portal (which calls `yield()`). WiFiManager stays blocking – the
copy-in's non-blocking portal loop isn't needed. Only that task reads Serial.

---

## Hardware test matrix (RUNBOOK step 5)

Preview: CI `site-preview` artifact from `dev`, served on
`http://localhost:8000`, desktop Chrome on macOS.

| #   | Case                               | Expect                             | Status |
|:----|:-----------------------------------|:-----------------------------------|:-------|
| 1   | Fresh install, erased              | Install + erase; Improv WiFi       | –      |
| 2   | Update on provisioned board        | "Update", WiFi kept                | –      |
| 2a  | Update while a scan is running     | Still offered "Update"             | –      |
| 2b  | Install over v1.1.0, no erase      | Install offered; WiFi kept         | –      |
| 3   | Board on `app1`                    | N/A – no OTA in this firmware      | N/A    |
| 4   | Wrong board                        | N/A – single env                   | N/A    |
| 5   | `*-firmware.bin` via web `/update` | N/A – no `/update` page            | N/A    |
| 6   | macOS Chrome                       | Port found, flash completes        | –      |
| 7   | Windows Edge                       | Optional                           | –      |

Board MAC and dates are recorded per case below as tests run.
