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

struct ChannelStat {
    uint8_t channel;
    uint8_t apCount;
    int16_t peakRssi;
    uint16_t score;
};

enum UIViewMode : uint8_t {
    VIEW_LIST = 0,
    VIEW_CHANNELS
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

// Persistent footer bar with VIEW toggle / status / SCAN button
void drawFooter(int networkCount, uint32_t lastScanMs, UIViewMode viewMode);

// Redraws the scrollable network list area
void drawNetworkList(const NetworkEntry* nets, int count, int scrollOffset);

// Redraws the 2.4 GHz channel congestion graph
void drawCongestionGraph(const ChannelStat* stats, int count, uint8_t hottestChannel);

// Small "Scanning…" overlay in the centre of the network area
void drawScanningOverlay();
