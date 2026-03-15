#pragma once

// ============================================================
// CYD (ESP32-2432S028R) Hardware Pin Definitions
// ============================================================

// TFT backlight (PWM via LEDC channel 0)
#define TFT_BL_PIN       21

// Touch controller XPT2046 on VSPI bus (separate from display HSPI)
#define TOUCH_CS_PIN     33
#define TOUCH_IRQ_PIN    36   // input-only GPIO, no pull-up
#define TOUCH_MOSI_PIN   32
#define TOUCH_MISO_PIN   39   // input-only GPIO
#define TOUCH_CLK_PIN    25

// Onboard RGB LED (active LOW)
#define LED_R_PIN        4
#define LED_G_PIN        16
#define LED_B_PIN        17

// LDR ambient-light sensor
#define LDR_PIN          34

// ============================================================
// Display Layout  (landscape 320x240)
// ============================================================
constexpr int SCREEN_W    = 320;
constexpr int SCREEN_H    = 240;
constexpr int HEADER_H    = 30;
constexpr int FOOTER_H    = 26;
constexpr int ROW_H       = 23;
// Derived: network list area
constexpr int NET_AREA_Y  = HEADER_H;
constexpr int NET_AREA_H  = SCREEN_H - HEADER_H - FOOTER_H;  // 184 px
constexpr int MAX_VISIBLE = NET_AREA_H / ROW_H;               // 8 rows

// ============================================================
// WiFi Scan Settings
// ============================================================
constexpr uint32_t SCAN_INTERVAL_MS  = 30000;  // 30 s auto-rescan
constexpr uint8_t  MAX_NETWORKS      = 20;

// ============================================================
// Touch Calibration  (landscape rotation 1)
// Adjust TOUCH_X/Y_MIN/MAX after first boot if needed.
// ============================================================
constexpr int TOUCH_X_MIN = 200;
constexpr int TOUCH_X_MAX = 3800;
constexpr int TOUCH_Y_MIN = 300;
constexpr int TOUCH_Y_MAX = 3700;
constexpr uint32_t TOUCH_DEBOUNCE_MS = 300;

// ============================================================
// Colors  (RGB565)
// ============================================================

// Backgrounds
#define COL_BG          0x0820   // very dark navy
#define COL_HEADER_BG   0x1082   // dark blue
#define COL_FOOTER_BG   0x0841   // slightly lighter dark
#define COL_ROW_ODD     0x0841
#define COL_ROW_EVEN    0x0420

// General text
#define COL_TEXT        0xFFFF   // white
#define COL_TEXT_DIM    0x8410   // medium gray
#define COL_TITLE       0x07FF   // cyan

// Signal-strength colours (match web UI)
#define COL_SIG_STRONG  0x07E0   // green   > -50 dBm
#define COL_SIG_GOOD    0x07FF   // cyan    -50 to -65 dBm
#define COL_SIG_FAIR    0xFFE0   // yellow  -65 to -75 dBm
#define COL_SIG_WEAK    0xF800   // red     < -75 dBm

// Band badges
#define COL_BAND_24     0x07FF   // cyan    2.4 GHz
#define COL_BAND_5      0xF81F   // magenta 5 GHz

// Security indicator
#define COL_LOCK_OPEN   0x67E0   // green  = open
#define COL_LOCK_SEC    0xFD20   // orange = secured

// UI controls
#define COL_BTN         0x2945   // dark button normal
#define COL_SCANNING    0xFFE0   // yellow scanning indicator
#define COL_SCROLLBAR   0x07FF   // cyan scroll thumb
