#pragma once

#include <Arduino.h>
#include "config.h"

// ============================================================
// Shared network entry (populated by scan, read by display + web)
// ============================================================
struct NetworkEntry {
    char     ssid[33];     // null-terminated, 32 chars max
    int32_t  rssi;
    uint8_t  channel;
    bool     secure;
    bool     is5GHz;       // channel > 14
    uint16_t sigColor;     // RGB565 colour matching signal strength
    uint8_t  sigBars;      // 0-5 filled bars
};

// ============================================================
// Display API
// ============================================================

// Call once in setup() - inits TFT, sets rotation, enables backlight
void initDisplay();

// Full-screen boot splash  (call before WiFiManager)
void drawBootScreen(const char* title, const char* subtitle);

// Full-screen WiFiManager portal instructions
void drawWiFiManagerScreen(const char* apName);

// Persistent header bar  (ipStr may be nullptr → shows "--")
void drawHeader(const char* ipStr, bool scanning);

// Persistent footer bar with UP / network count / SCAN buttons
void drawFooter(int networkCount, uint32_t lastScanMs);

// Redraws the scrollable network list area
void drawNetworkList(const NetworkEntry* nets, int count, int scrollOffset);

// Small "Scanning…" overlay in the centre of the network area
void drawScanningOverlay();
