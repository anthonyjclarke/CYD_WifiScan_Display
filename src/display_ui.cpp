#include "display_ui.h"
#include "debug.h"
#include <TFT_eSPI.h>

// ============================================================
// TFT instance  (HSPI, ILI9341, 320x240)
// ============================================================
static TFT_eSPI tft;

// ============================================================
// Column X positions for the network list (landscape 320px)
// ============================================================
//  #  |  SSID (20 chars)  | BAND | BARS | RSSI | SEC
static constexpr int COL_IDX  =   2;   // "99" → 12px
static constexpr int COL_SSID =  22;   // 20 chars × 6px = 120px
static constexpr int COL_BAND = 144;   // badge 38px
static constexpr int COL_BARS = 186;   // 5 bars × 7px = 35px
static constexpr int COL_RSSI = 224;   // "-XXX" 4ch = 24px
static constexpr int COL_SEC  = 268;   // lock marker

// Vertical text offset within a ROW_H row so text is centred
static constexpr int ROW_TY   =   8;   // ((ROW_H - 8) / 2) for font sz 1

// ============================================================
// Helpers
// ============================================================

// 5 ascending bars like a phone-signal icon, bottom-aligned
static void drawSignalBars(int x, int y, uint8_t bars, uint16_t color)
{
    constexpr int W = 4;   // bar width
    constexpr int G = 2;   // gap between bars
    constexpr int MH = 12; // tallest bar height

    for (int i = 0; i < 5; i++) {
        int h  = 2 + (i + 1) * 2;           // heights: 4,6,8,10,12
        int bx = x + i * (W + G);
        int by = y + MH - h;                 // align to same baseline
        uint16_t col = (i < bars) ? color : static_cast<uint16_t>(0x2945);
        tft.fillRect(bx, by, W, h, col);
    }
}

// Coloured rounded-rect badge with centred text
static void drawBandBadge(int x, int y, bool is5GHz)
{
    constexpr int BW = 36;
    constexpr int BH = 13;
    uint16_t bgCol = is5GHz ? static_cast<uint16_t>(0x300C)   // dark purple
                            : static_cast<uint16_t>(0x0219);  // dark cyan
    uint16_t fgCol = is5GHz ? COL_BAND_5 : COL_BAND_24;
    const char* label = is5GHz ? " 5G" : "2.4G";

    tft.fillRoundRect(x, y, BW, BH, 2, bgCol);
    tft.setTextColor(fgCol, bgCol);
    tft.setTextSize(1);
    tft.drawString(label, x + 4, y + 3);
}

// ============================================================
// Public functions
// ============================================================

void initDisplay()
{
    tft.init();
    tft.setRotation(1);           // landscape, USB-C on right
    tft.fillScreen(COL_BG);

    // Backlight via LEDC PWM (full brightness)
    ledcSetup(0, 5000, 8);
    ledcAttachPin(TFT_BL_PIN, 0);
    ledcWrite(0, 255);

    DBG_INFO("Display init: %dx%d rotation=1", tft.width(), tft.height());
}

void drawBootScreen(const char* title, const char* subtitle)
{
    tft.fillScreen(COL_BG);
    tft.setTextDatum(MC_DATUM);

    tft.setTextColor(COL_TITLE);
    tft.setTextSize(2);
    tft.drawString(title, SCREEN_W / 2, SCREEN_H / 2 - 18);

    tft.setTextColor(COL_TEXT_DIM);
    tft.setTextSize(1);
    tft.drawString(subtitle, SCREEN_W / 2, SCREEN_H / 2 + 12);

    tft.setTextDatum(TL_DATUM);
}

void drawWiFiManagerScreen(const char* apName)
{
    tft.fillScreen(COL_BG);
    tft.setTextDatum(MC_DATUM);

    // Title
    tft.setTextColor(COL_TITLE);
    tft.setTextSize(2);
    tft.drawString("WiFi Setup", SCREEN_W / 2, 55);

    // Instructions
    tft.setTextColor(COL_TEXT);
    tft.setTextSize(1);
    tft.drawString("Connect your phone to:", SCREEN_W / 2, 105);

    tft.setTextColor(COL_BAND_24);
    tft.drawString(apName, SCREEN_W / 2, 122);

    tft.setTextColor(COL_TEXT_DIM);
    tft.drawString("Then open  192.168.4.1", SCREEN_W / 2, 148);
    tft.drawString("Portal timeout: 120 s", SCREEN_W / 2, 168);

    tft.setTextDatum(TL_DATUM);
}

void drawHeader(const char* ipStr, bool scanning)
{
    tft.fillRect(0, 0, SCREEN_W, HEADER_H, COL_HEADER_BG);

    tft.setTextSize(1);
    tft.setTextDatum(ML_DATUM);
    tft.setTextColor(COL_TITLE, COL_HEADER_BG);
    tft.drawString("WiFi Scanner", 6, HEADER_H / 2);

    if (scanning) {
        tft.setTextColor(COL_SCANNING, COL_HEADER_BG);
        tft.drawString("* SCANNING *", 100, HEADER_H / 2);
    }

    // IP right-aligned
    tft.setTextDatum(MR_DATUM);
    tft.setTextColor(COL_TEXT_DIM, COL_HEADER_BG);
    tft.drawString(ipStr ? ipStr : "--", SCREEN_W - 5, HEADER_H / 2);

    // Bottom separator
    tft.drawFastHLine(0, HEADER_H - 1, SCREEN_W, COL_TEXT_DIM);

    tft.setTextDatum(TL_DATUM);
}

void drawFooter(int networkCount, uint32_t lastScanMs)
{
    const int fy = SCREEN_H - FOOTER_H;

    tft.fillRect(0, fy, SCREEN_W, FOOTER_H, COL_FOOTER_BG);
    tft.drawFastHLine(0, fy, SCREEN_W, COL_TEXT_DIM);

    // ---- UP button (scroll arrow) ----
    tft.fillRoundRect(3, fy + 4, 38, 17, 3, COL_BTN);
    tft.setTextColor(COL_TEXT, COL_BTN);
    tft.setTextSize(1);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("^ UP", 22, fy + 13);

    // ---- Network count + age (centre) ----
    char buf[32];
    if (lastScanMs == 0) {
        snprintf(buf, sizeof(buf), "%d nets  --s ago", networkCount);
    } else {
        uint32_t age = (millis() - lastScanMs) / 1000;
        snprintf(buf, sizeof(buf), "%d nets  %lus ago", networkCount, (unsigned long)age);
    }
    tft.setTextColor(COL_TEXT_DIM, COL_FOOTER_BG);
    tft.drawString(buf, SCREEN_W / 2, fy + 13);

    // ---- SCAN button ----
    tft.fillRoundRect(SCREEN_W - 72, fy + 4, 69, 17, 3, COL_BTN);
    tft.setTextColor(COL_TITLE, COL_BTN);
    tft.drawString("SCAN v", SCREEN_W - 37, fy + 13);

    tft.setTextDatum(TL_DATUM);
}

void drawNetworkList(const NetworkEntry* nets, int count, int scrollOffset)
{
    const int netBottom = SCREEN_H - FOOTER_H;

    for (int row = 0; row < MAX_VISIBLE; row++) {
        int netIdx = row + scrollOffset;
        int rowY   = NET_AREA_Y + row * ROW_H;

        if (rowY + ROW_H > netBottom) break;

        if (netIdx >= count) {
            // Clear any leftover row
            tft.fillRect(0, rowY, SCREEN_W - 3, ROW_H, COL_BG);
            continue;
        }

        const NetworkEntry& net = nets[netIdx];
        uint16_t rowBg = (row % 2 == 0) ? static_cast<uint16_t>(COL_ROW_EVEN)
                                         : static_cast<uint16_t>(COL_ROW_ODD);

        tft.fillRect(0, rowY, SCREEN_W - 3, ROW_H, rowBg);

        int ty = rowY + ROW_TY;

        // --- Index ---
        char idxBuf[4];
        snprintf(idxBuf, sizeof(idxBuf), "%2d", netIdx + 1);
        tft.setTextColor(COL_TEXT_DIM, rowBg);
        tft.setTextSize(1);
        tft.drawString(idxBuf, COL_IDX, ty);

        // --- SSID (truncate to 20 chars) ---
        char ssidBuf[21];
        if (net.ssid[0] == '\0') {
            strncpy(ssidBuf, "<hidden>", 21);
        } else {
            strncpy(ssidBuf, net.ssid, 20);
            ssidBuf[20] = '\0';
        }
        tft.setTextColor(COL_TEXT, rowBg);
        tft.drawString(ssidBuf, COL_SSID, ty);

        // --- Band badge ---
        drawBandBadge(COL_BAND, rowY + 5, net.is5GHz);

        // --- Signal bars (bottom-aligned within row) ---
        drawSignalBars(COL_BARS, rowY + 4, net.sigBars, net.sigColor);

        // --- RSSI value ---
        char rssiBuf[6];
        snprintf(rssiBuf, sizeof(rssiBuf), "%4d", (int)net.rssi);
        tft.setTextColor(net.sigColor, rowBg);
        tft.drawString(rssiBuf, COL_RSSI, ty);

        // --- Security indicator ---
        if (net.secure) {
            tft.setTextColor(COL_LOCK_SEC, rowBg);
            tft.drawString("*", COL_SEC, ty);    // locked
        } else {
            tft.setTextColor(COL_LOCK_OPEN, rowBg);
            tft.drawString("o", COL_SEC, ty);    // open
        }
    }

    // ---- Scroll-position indicator (right 3px strip) ----
    const int trackH = NET_AREA_H;
    tft.fillRect(SCREEN_W - 3, NET_AREA_Y, 3, trackH, COL_FOOTER_BG);
    if (count > MAX_VISIBLE) {
        int thumbH = max(6, (MAX_VISIBLE * trackH) / count);
        int thumbY = NET_AREA_Y + (scrollOffset * (trackH - thumbH)) / (count - MAX_VISIBLE);
        tft.fillRect(SCREEN_W - 3, thumbY, 3, thumbH, COL_SCROLLBAR);
    }
}

void drawScanningOverlay()
{
    const int ox = SCREEN_W / 2 - 65;
    const int oy = SCREEN_H / 2 - 14;
    tft.fillRoundRect(ox, oy, 130, 28, 5, COL_BTN);
    tft.drawRoundRect(ox, oy, 130, 28, 5, COL_TITLE);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(COL_SCANNING);
    tft.setTextSize(1);
    tft.drawString("Scanning networks...", SCREEN_W / 2, SCREEN_H / 2);
    tft.setTextDatum(TL_DATUM);
}
