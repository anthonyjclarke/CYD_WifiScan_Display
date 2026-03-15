#pragma once

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include "display_ui.h"   // for NetworkEntry

// ============================================================
// Web Server API
// Endpoints:
//   GET  /                 → HTML dashboard
//   GET  /api/networks     → JSON network list + channel summary
//   GET  /api/status       → JSON device status
//   POST /api/scan         → trigger immediate scan
//   GET  /api/debug        → {"level": N}
//   POST /api/debug        → body {"level": N}  sets runtime level
// ============================================================

void initWebServer(
    NetworkEntry*       nets,
    volatile int*       netCount,
    ChannelStat*        channels,
    SemaphoreHandle_t*  mutex,
    volatile uint32_t*  scanCount,
    volatile bool*      scanning,
    volatile uint32_t*  lastScanMs,
    void (*triggerScanCb)()
);
