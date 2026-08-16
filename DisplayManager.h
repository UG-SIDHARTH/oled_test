#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include <Arduino.h>
#include <Wire.h>
#include <U8g2lib.h>
#include "config.h"

// Number of visualizer bars
#define EQ_BAR_COUNT 6
#define MAX_FLOATING_NOTES 4

struct FloatingNote {
    float x;
    float y;
    float speedY;
    float phase;
    bool noteType; // true for single note ♪, false for double note ♫
    bool active;
};

class DisplayManager {
public:
    DisplayManager();

    // Initializes I2C & SSD1306 display with U8g2
    bool begin();

    // Clears screen buffer
    void clear();

    // Main player UI rendering with synced lyrics and dynamic animations
    void renderPlayer(
        const String& trackName,
        const String& artistName,
        const String& activeLyric,
        const String& nextLyric,
        uint32_t progressMs,
        uint32_t durationMs,
        bool isPlaying,
        bool hasLyrics
    );

    // Standby / Idle animated screen (e.g. when Spotify is paused or idle)
    void renderIdleScreen(const String& line1 = "Spotify Ready", const String& line2 = "Play a song to begin");

    // Loading / Connecting animated screen
    void renderConnectingScreen(const String& title, const String& subtitle, uint8_t animStep = 0);

    // Shows static or animated message
    void showStatusMessage(const String& line1, const String& line2 = "", const String& line3 = "");

    // Direct draw helpers for animations
    void drawEqualizer(int x, int y, int width, int height, bool isPlaying);
    void drawSpinningVinyl(int centerX, int centerY, int radius, uint8_t angleStep);
    void drawPixelDancingStickman(int centerX, int centerY, uint8_t animStep, bool isPlaying);
    void drawDancingCharacter(int x, int y, uint8_t animStep, bool isPlaying);
    void drawMiniCassette(int x, int y, uint8_t animFrame);
    void updateAndDrawFloatingNotes(int minX, int maxX, int minY, int maxY, bool isPlaying);

private:
    U8G2_SSD1306_128X64_NONAME_F_HW_I2C _u8g2;

    // Header marquee scroll state
    int _headerScrollOffset = 0;
    uint32_t _lastHeaderScrollMs = 0;
    String _lastHeaderTrack = "";

    // Lyric marquee scroll state
    int _lyricScrollOffset = 0;
    uint32_t _lastLyricScrollMs = 0;
    String _lastActiveLyric = "";

    // Visualizer state
    float _eqHeights[EQ_BAR_COUNT];
    float _eqPeaks[EQ_BAR_COUNT];
    uint32_t _lastEqUpdateMs = 0;

    // Floating notes state
    FloatingNote _notes[MAX_FLOATING_NOTES];
    uint32_t _lastNoteUpdateMs = 0;

    // Animation frame counters
    uint8_t _animFrame = 0;
    uint32_t _lastAnimFrameMs = 0;

    void initFloatingNotes();
    void updateEqualizer(bool isPlaying);
    void drawProgressBar(int x, int y, int width, int height, uint32_t progressMs, uint32_t durationMs);
    String formatTime(uint32_t ms);
    void drawMusicNoteGlyph(int x, int y, bool isDouble);
};

#endif // DISPLAY_MANAGER_H
