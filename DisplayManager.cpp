#include "DisplayManager.h"

DisplayManager::DisplayManager()
    : _display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET) {
}

bool DisplayManager::begin() {
    Wire.begin(OLED_SDA, OLED_SCL);
    if (!_display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
        Serial.println(F("[OLED] SSD1306 allocation failed!"));
        return false;
    }

    _display.clearDisplay();
    _display.setTextColor(SSD1306_WHITE);
    _display.display();
    return true;
}

void DisplayManager::clear() {
    _display.clearDisplay();
    _display.display();
}

void DisplayManager::showStatusMessage(const String& line1, const String& line2, const String& line3) {
    _display.clearDisplay();
    _display.setTextSize(1);
    _display.setTextColor(SSD1306_WHITE);
    
    _display.setCursor(0, 10);
    _display.println(line1);
    
    if (line2.length() > 0) {
        _display.setCursor(0, 28);
        _display.println(line2);
    }

    if (line3.length() > 0) {
        _display.setCursor(0, 46);
        _display.println(line3);
    }

    _display.display();
}

String DisplayManager::formatTime(uint32_t ms) {
    uint32_t totalSec = ms / 1000;
    uint32_t mins = totalSec / 60;
    uint32_t secs = totalSec % 60;
    char buf[10];
    snprintf(buf, sizeof(buf), "%02u:%02u", mins, secs);
    return String(buf);
}

void DisplayManager::drawProgressBar(int x, int y, int width, int height, uint32_t progressMs, uint32_t durationMs) {
    _display.drawRect(x, y, width, height, SSD1306_WHITE);
    if (durationMs > 0) {
        int fillWidth = map(constrain(progressMs, 0, durationMs), 0, durationMs, 0, width - 2);
        if (fillWidth > 0) {
            _display.fillRect(x + 1, y + 1, fillWidth, height - 2, SSD1306_WHITE);
        }
    }
}

void DisplayManager::renderPlayer(const String& trackName, const String& artistName, const String& lyricText, uint32_t progressMs, uint32_t durationMs, bool isPlaying, bool hasLyrics) {
    _display.clearDisplay();

    // ------------------------------------------------------------------------
    // 1. Header Area (Song & Artist)
    // ------------------------------------------------------------------------
    _display.setTextSize(1);
    _display.setTextColor(SSD1306_WHITE);

    // Format header string: "Song Name - Artist"
    String headerStr = trackName;
    if (artistName.length() > 0) {
        headerStr += " - " + artistName;
    }

    // Scroll header if longer than screen width (21 chars)
    if (headerStr.length() > 21) {
        if (millis() - _lastScrollMs > 250) {
            _scrollOffset++;
            if (_scrollOffset > (int)headerStr.length() - 15) {
                _scrollOffset = 0;
            }
            _lastScrollMs = millis();
        }
        headerStr = headerStr.substring(_scrollOffset);
    } else {
        _scrollOffset = 0;
    }

    _display.setCursor(0, 0);
    _display.print(headerStr);

    // Separator line under header
    _display.drawFastHLine(0, 11, SCREEN_WIDTH, SSD1306_WHITE);

    // ------------------------------------------------------------------------
    // 2. Synced Lyric Area (Center Area y=14..48)
    // ------------------------------------------------------------------------
    _display.setCursor(0, 15);
    _display.setTextSize(1);

    if (lyricText.length() > 0) {
        _display.println(lyricText);
    } else {
        _display.println(hasLyrics ? "..." : "No Synced Lyrics");
    }

    // ------------------------------------------------------------------------
    // 3. Footer Area (Progress Bar & Play/Pause State)
    // ------------------------------------------------------------------------
    // Draw play icon '>' or pause '||'
    if (isPlaying) {
        // Play triangle
        _display.fillTriangle(0, 55, 0, 63, 5, 59, SSD1306_WHITE);
    } else {
        // Pause bars
        _display.fillRect(0, 55, 2, 8, SSD1306_WHITE);
        _display.fillRect(4, 55, 2, 8, SSD1306_WHITE);
    }

    // Progress bar
    drawProgressBar(10, 57, 65, 5, progressMs, durationMs);

    // Time text (e.g. 01:23)
    _display.setCursor(80, 56);
    _display.setTextSize(1);
    _display.print(formatTime(progressMs));

    _display.display();
}
