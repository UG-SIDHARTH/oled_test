#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "config.h"

class DisplayManager {
public:
    DisplayManager();

    // Initializes I2C & SSD1306 display
    bool begin();

    // Displays simple full-screen message (e.g. WiFi connecting, error state)
    void showStatusMessage(const String& line1, const String& line2 = "", const String& line3 = "");

    // Main player UI rendering
    void renderPlayer(const String& trackName, const String& artistName, const String& lyricText, uint32_t progressMs, uint32_t durationMs, bool isPlaying, bool hasLyrics);

    // Clears screen
    void clear();

private:
    Adafruit_SSD1306 _display;
    int _scrollOffset = 0;
    uint32_t _lastScrollMs = 0;

    void drawProgressBar(int x, int y, int width, int height, uint32_t progressMs, uint32_t durationMs);
    String formatTime(uint32_t ms);
};

#endif // DISPLAY_MANAGER_H
