#include <Arduino.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <SPI.h>
#include <XPT2046_Touchscreen.h>

#include "debug.h"
#include "config.h"
#include "display_ui.h"
#include "web_server.h"

// ============================================================
// debugLevel definition  (extern-declared in debug.h)
// ============================================================
uint8_t debugLevel = DEBUG_LEVEL;

// ============================================================
// Shared scan state  (accessed from loop + web server task)
// ============================================================
static NetworkEntry       networks[MAX_NETWORKS];
static volatile int       networkCount   = 0;
static volatile uint32_t  scanCount      = 0;
static volatile bool      scanInProgress = false;
static volatile bool      scanRequested  = false;   // set by web/touch, consumed by loop
static volatile uint32_t  lastScanMs     = 0;
static SemaphoreHandle_t  networkMutex;

// ============================================================
// Touch  — XPT2046 via global SPI (VSPI) reconfigured to CYD pins
// TFT_eSPI uses HSPI internally, so global SPI (VSPI) is free.
// ============================================================
static XPT2046_Touchscreen ts(TOUCH_CS_PIN, TOUCH_IRQ_PIN);

// ============================================================
// UI state
// ============================================================
static int      scrollOffset = 0;
static uint32_t lastTouchMs  = 0;
static char     deviceIP[20] = "--";

// ============================================================
// Forward declarations
// ============================================================
static void requestScan();   // safe to call from any task/context
static void doScan();        // blocking synchronous scan — call from loop() only
static void redrawList();

// ============================================================
// WiFiManager AP-mode callback
// ============================================================
static void onWiFiManagerAP(WiFiManager* wm)
{
    DBG_WARN("WiFiManager: AP mode — SSID: %s", wm->getConfigPortalSSID().c_str());
    drawWiFiManagerScreen(wm->getConfigPortalSSID().c_str());
}

// ============================================================
// Scan — split into a flag-setter (any context) + a blocking
// worker (loop() only).
//
// WHY synchronous:
//   WiFi.scanNetworks(async=true) starts correctly but
//   WiFi.scanComplete() immediately returns WIFI_SCAN_FAILED
//   when ESPAsyncWebServer is running — AsyncTCP on core 0
//   shares the radio with the scan and aborts it.
//   Synchronous scan (~2 s) blocks loop() but ESPAsyncWebServer
//   lives on its own FreeRTOS task so the web server stays live.
// ============================================================

// Safe to call from web server task, touch handler, or loop()
static void requestScan()
{
    if (!scanInProgress) {
        scanRequested = true;
    }
}

// Called only from loop() — may block up to ~4 s
static void doScan()
{
    scanInProgress = true;
    scanRequested  = false;

    DBG_INFO("Scan start  mode=%d connected=%d ch=%d heap=%u",
        (int)WiFi.getMode(), (int)WiFi.isConnected(),
        (int)WiFi.channel(), ESP.getFreeHeap());

    WiFi.scanDelete();   // clear any stale results

    drawScanningOverlay();
    drawHeader(deviceIP, true);

    // Synchronous, show hidden networks
    int n = WiFi.scanNetworks(false /*sync*/, true /*hidden*/);

    scanInProgress = false;

    if (n < 0) {
        DBG_ERROR("Scan failed (n=%d)  mode=%d connected=%d",
            n, (int)WiFi.getMode(), (int)WiFi.isConnected());
        drawHeader(deviceIP, false);
        return;
    }

    DBG_INFO("Scan complete: %d networks", n);

    // ---- Populate shared array ----
    xSemaphoreTake(networkMutex, portMAX_DELAY);
    networkCount = min(n, (int)MAX_NETWORKS);

    for (int i = 0; i < networkCount; i++) {
        NetworkEntry& e = networks[i];
        strncpy(e.ssid, WiFi.SSID(i).c_str(), 32);
        e.ssid[32] = '\0';
        e.rssi     = WiFi.RSSI(i);
        e.channel  = static_cast<uint8_t>(WiFi.channel(i));
        e.secure   = (WiFi.encryptionType(i) != WIFI_AUTH_OPEN);
        e.is5GHz   = (e.channel > 14);

        if      (e.rssi > -50) { e.sigColor = COL_SIG_STRONG; e.sigBars = 5; }
        else if (e.rssi > -65) { e.sigColor = COL_SIG_GOOD;   e.sigBars = 4; }
        else if (e.rssi > -75) { e.sigColor = COL_SIG_FAIR;   e.sigBars = 2; }
        else                   { e.sigColor = COL_SIG_WEAK;   e.sigBars = 1; }

        DBG_VERBOSE("  [%2d] %-20s ch%3d %4d dBm  %s  %s",
            i + 1, e.ssid[0] ? e.ssid : "<hidden>",
            e.channel, (int)e.rssi,
            e.is5GHz ? "5GHz" : "2.4G",
            e.secure  ? "sec"  : "open");
    }
    xSemaphoreGive(networkMutex);

    WiFi.scanDelete();
    lastScanMs = millis();
    scanCount++;

    int maxScroll = max(0, (int)networkCount - MAX_VISIBLE);
    if (scrollOffset > maxScroll) scrollOffset = maxScroll;

    redrawList();
    DBG_INFO("Heap after scan: %u free", ESP.getFreeHeap());
}

// ============================================================
// Display helper
// ============================================================
static void redrawList()
{
    xSemaphoreTake(networkMutex, portMAX_DELAY);
    drawHeader(deviceIP, scanInProgress);
    drawNetworkList(networks, networkCount, scrollOffset);
    drawFooter(networkCount, lastScanMs);
    xSemaphoreGive(networkMutex);
}

// ============================================================
// Touch processing
// ============================================================
static void processTouch()
{
    if (!ts.tirqTouched() || !ts.touched()) return;
    if (millis() - lastTouchMs < TOUCH_DEBOUNCE_MS) return;
    lastTouchMs = millis();

    TS_Point p = ts.getPoint();
    int tx = map(p.x, TOUCH_X_MIN, TOUCH_X_MAX, 0, SCREEN_W - 1);
    int ty = map(p.y, TOUCH_Y_MIN, TOUCH_Y_MAX, 0, SCREEN_H - 1);
    tx = constrain(tx, 0, SCREEN_W - 1);
    ty = constrain(ty, 0, SCREEN_H - 1);

    DBG_VERBOSE("Touch raw(%d,%d) -> screen(%d,%d)", p.x, p.y, tx, ty);

    const int footerY   = SCREEN_H - FOOTER_H;
    const int maxScroll = max(0, (int)networkCount - MAX_VISIBLE);

    if (ty >= footerY) {
        if (tx >= SCREEN_W - 75) {
            DBG_INFO("Touch: SCAN button");
            requestScan();
        } else if (tx <= 45 && scrollOffset > 0) {
            scrollOffset--;
            redrawList();
        }
        return;
    }

    if (ty < HEADER_H) return;

    int relY     = ty - HEADER_H;
    int quarterH = NET_AREA_H / 4;

    if (relY < quarterH) {
        if (scrollOffset > 0) { scrollOffset--; redrawList(); }
    } else if (relY > NET_AREA_H - quarterH) {
        if (scrollOffset < maxScroll) { scrollOffset++; redrawList(); }
    } else {
        DBG_INFO("Touch: centre area — scan requested");
        requestScan();
    }
}

// ============================================================
// setup
// ============================================================
void setup()
{
    Serial.begin(115200);
    DBG_INFO("=================================");
    DBG_INFO("  WiFi Scanner  ESP32 CYD");
    DBG_INFO("  Build: %s %s", __DATE__, __TIME__);
    DBG_INFO("=================================");

    // RGB LED off (active LOW)
    pinMode(LED_R_PIN, OUTPUT); digitalWrite(LED_R_PIN, HIGH);
    pinMode(LED_G_PIN, OUTPUT); digitalWrite(LED_G_PIN, HIGH);
    pinMode(LED_B_PIN, OUTPUT); digitalWrite(LED_B_PIN, HIGH);

    // Display
    initDisplay();
    drawBootScreen("WiFi Scanner", "ESP32 CYD  v1.0");
    delay(800);

    // Touch — VSPI with CYD-specific pins (TFT_eSPI owns HSPI)
    SPI.begin(TOUCH_CLK_PIN, TOUCH_MISO_PIN, TOUCH_MOSI_PIN, TOUCH_CS_PIN);
    ts.begin();
    ts.setRotation(1);
    DBG_INFO("Touch controller initialised");

    // Mutex for shared network array
    networkMutex = xSemaphoreCreateMutex();

    // WiFiManager
    DBG_INFO("Starting WiFiManager...");
    drawBootScreen("WiFi Scanner", "Connecting to WiFi...");

    WiFiManager wm;
    wm.setAPCallback(onWiFiManagerAP);
    wm.setConfigPortalTimeout(120);
    wm.setConnectTimeout(15);

    if (!wm.autoConnect("WiFiScanner-AP")) {
        DBG_ERROR("WiFiManager timed out — restarting");
        delay(2000);
        ESP.restart();
    }

    strncpy(deviceIP, WiFi.localIP().toString().c_str(), sizeof(deviceIP) - 1);
    DBG_INFO("WiFi connected  IP: %s  RSSI: %d dBm", deviceIP, WiFi.RSSI());

    // WiFiManager leaves AP_STA mode — force STA-only and wait to reconnect
    WiFi.mode(WIFI_STA);
    {
        uint32_t t0 = millis();
        while (!WiFi.isConnected() && (millis() - t0) < 8000) { delay(100); }
    }
    DBG_INFO("WiFi mode=STA connected=%d channel=%d",
             (int)WiFi.isConnected(), (int)WiFi.channel());

    // Green LED pulse — connected
    digitalWrite(LED_G_PIN, LOW); delay(300); digitalWrite(LED_G_PIN, HIGH);

    // Web server — pass requestScan (not doScan) so it's safe to call from the web task
    initWebServer(networks, &networkCount, &networkMutex,
                  &scanCount, &scanInProgress, &lastScanMs, requestScan);

    // Initial display
    drawHeader(deviceIP, false);
    drawNetworkList(networks, 0, 0);
    drawFooter(0, 0);

    // Queue first scan — doScan() runs at the top of loop()
    scanRequested = true;

    DBG_INFO("Setup complete  Heap: %u bytes free", ESP.getFreeHeap());
}

// ============================================================
// loop
// ============================================================
void loop()
{
    // Run a scan if requested or interval elapsed
    if (!scanInProgress) {
        bool intervalElapsed = (lastScanMs > 0) &&
                               ((millis() - lastScanMs) >= SCAN_INTERVAL_MS);

        if (scanRequested || intervalElapsed) {
            if (intervalElapsed && !scanRequested) {
                DBG_INFO("Auto-scan (interval %lu ms)", (unsigned long)SCAN_INTERVAL_MS);
            }
            doScan();
        }
    }

    processTouch();
}
