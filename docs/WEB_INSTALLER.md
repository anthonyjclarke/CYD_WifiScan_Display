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

## Smoke test (RUNBOOK 5a) – passed 09-10-2026

Board: CYD 2.8″ ESP32-2432S028R, ESP32-D0WD-V3 rev 3.1, MAC
`b0:cb:d8:da:ae:8c`. Preview: CI `site-preview` from `dev` at `cbac2c8`
(run 37891754178, green), served on localhost, desktop Chrome on macOS.

| Step                                   | Result                                    |
|:---------------------------------------|:------------------------------------------|
| CI green, every env (`esp32-cyd`)      | Pass                                      |
| `pio run -t erase`, install with erase | Pass – flashed `1.2.0-dev`                |
| Configure WiFi over Improv             | Pass – joined, 192.168.1.95               |
| Boot log                               | Pass – `Running from app0`, no crash      |
| First scan, web dashboard              | Pass – 19 networks; `/` 200, `/api/status` |
| Connect again                          | Pass – "Connected to WiFiScan-CBB0"       |

The second Connect showed `CYD_WifiScan_Display 1.2.0-dev (ESP32)` with Visit
Device, Change Wi-Fi, Logs & Console and Erase User Data – the same check that
makes **Update** appear. The boot log also shows a harmless
`addApbChangeCallback(): duplicate func` core message at about 1.2 s.

The Improv device name is `WiFiScan-CBB0`, not the MAC's last four digits
(`AE8C`): the shared `improv_setup.cpp` masks `getEfuseMac()`, whose low bytes
are the first MAC bytes. Reported to cyd-web-installer; cosmetic only.

**Release check (RUNBOOK 7.4) – passed 09-10-2026.** Tag `v1.2.0` run
37893762595 built and published. The live page, `index.json` (1.2.0) and all
four parts load; the release has `-firmware.bin`, `-merged.bin` and
`SHA256SUMS.txt`. An Update from the live page took the bench board from
`1.2.0-dev` to `1.2.0` with no erase question; it booted `app0` and rejoined
its saved WiFi with no portal.

---

## Tests owed

Smoke-tested only. Run these on the next real work on this project, or before
the next release, and tick them off with date and board MAC.

- [x] Case 1 – fresh install, erased (only one board env) – 09-10-2026, `b0:cb:d8:da:ae:8c`
- [x] Case 2 – Update on a provisioned board (settings kept) – 09-10-2026, `b0:cb:d8:da:ae:8c`, live page 1.2.0-dev → 1.2.0, no erase, saved WiFi rejoined
- [ ] Case 2a – Update offered while a scan is running (Improv task)
- [ ] Case 2b – Install over v1.1.0 with no erase keeps WiFi (partition switch)
- N/A Case 3 – Update from `app1` (no OTA in this firmware)
- N/A Case 4 – wrong board image (single env)
