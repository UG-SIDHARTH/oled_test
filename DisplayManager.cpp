#include "DisplayManager.h"
#include <math.h>

DisplayManager::DisplayManager()
    : _display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET) {
    for (int i = 0; i < EQ_BAR_COUNT; i++) {
        _eqHeights[i] = 2.0f;
        _eqPeaks[i] = 2.0f;
    }
    initFloatingNotes();
}

bool DisplayManager::begin() {
    Wire.begin(OLED_SDA, OLED_SCL);
    if (!_display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
        Serial.println(F("[OLED] SSD1306 allocation failed! Check I2C address & wiring."));
        return false;
    }

    _display.clearDisplay();
    _display.setTextColor(SSD1306_WHITE);
    _display.setTextWrap(false);
    _display.display();
    return true;
}

void DisplayManager::clear() {
    _display.clearDisplay();
    _display.display();
}

void DisplayManager::initFloatingNotes() {
    for (int i = 0; i < MAX_FLOATING_NOTES; i++) {
        _notes[i].x = 10 + (i * 28);
        _notes[i].y = 48 - (i * 10);
        _notes[i].speedY = 0.5f + (float)(i % 3) * 0.25f;
        _notes[i].phase = (float)i * 1.57f;
        _notes[i].noteType = (i % 2 == 0);
        _notes[i].active = true;
    }
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

void DisplayManager::drawMusicNoteGlyph(int x, int y, bool isDouble) {
    if (x < -8 || x > SCREEN_WIDTH || y < -8 || y > SCREEN_HEIGHT) return;
    
    if (isDouble) {
        // Double note ♫ glyph (7x7)
        _display.fillRect(x, y + 4, 3, 3, SSD1306_WHITE);       // Left head
        _display.fillRect(x + 5, y + 3, 3, 3, SSD1306_WHITE);   // Right head
        _display.drawFastVLine(x + 2, y + 1, 4, SSD1306_WHITE);  // Left stem
        _display.drawFastVLine(x + 7, y, 4, SSD1306_WHITE);      // Right stem
        _display.drawFastHLine(x + 2, y, 6, SSD1306_WHITE);      // Beam top
        _display.drawFastHLine(x + 2, y + 1, 6, SSD1306_WHITE);  // Beam thick
    } else {
        // Single note ♪ glyph (5x6)
        _display.fillRect(x, y + 3, 3, 3, SSD1306_WHITE);       // Note head
        _display.drawFastVLine(x + 2, y, 4, SSD1306_WHITE);      // Stem
        _display.drawPixel(x + 3, y, SSD1306_WHITE);             // Flag
        _display.drawPixel(x + 4, y + 1, SSD1306_WHITE);
    }
}

void DisplayManager::updateAndDrawFloatingNotes(int minX, int maxX, int minY, int maxY, bool isPlaying) {
    uint32_t now = millis();
    float dt = (now - _lastNoteUpdateMs) / 1000.0f;
    if (dt > 0.1f) dt = 0.1f;
    _lastNoteUpdateMs = now;

    for (int i = 0; i < MAX_FLOATING_NOTES; i++) {
        if (!isPlaying) continue;

        _notes[i].y -= _notes[i].speedY * (dt * 25.0f);
        _notes[i].phase += dt * 3.0f;

        float swayX = _notes[i].x + sin(_notes[i].phase) * 4.0f;

        if (_notes[i].y < minY) {
            _notes[i].y = maxY;
            _notes[i].x = minX + random(0, maxX - minX);
            _notes[i].speedY = 0.4f + (float)random(10, 40) / 100.0f;
        }

        drawMusicNoteGlyph((int)swayX, (int)_notes[i].y, _notes[i].noteType);
    }
}

void DisplayManager::updateEqualizer(bool isPlaying) {
    uint32_t now = millis();
    if (now - _lastEqUpdateMs < 30) return;
    _lastEqUpdateMs = now;

    for (int i = 0; i < EQ_BAR_COUNT; i++) {
        if (isPlaying) {
            // Dynamic rhythmic simulation with harmonic harmonics
            float phase = (now * 0.006f * (i + 1)) + (i * 0.8f);
            float target = (sin(phase) * 0.5f + 0.5f) * 14.0f + (float)random(0, 5);
            target = constrain(target, 2.0f, 18.0f);

            // Smooth interpolation
            _eqHeights[i] = _eqHeights[i] * 0.6f + target * 0.4f;

            // Peak cap physics with gravity
            if (_eqHeights[i] > _eqPeaks[i]) {
                _eqPeaks[i] = _eqHeights[i];
            } else {
                _eqPeaks[i] -= 0.5f; // Fall speed
                if (_eqPeaks[i] < _eqHeights[i]) _eqPeaks[i] = _eqHeights[i];
            }
        } else {
            // Settle down to baseline when paused
            _eqHeights[i] = _eqHeights[i] * 0.8f;
            if (_eqHeights[i] < 1.0f) _eqHeights[i] = 1.0f;
            _eqPeaks[i] = _eqHeights[i];
        }
    }
}

void DisplayManager::drawEqualizer(int x, int y, int width, int height, bool isPlaying) {
    updateEqualizer(isPlaying);

    int barWidth = (width - (EQ_BAR_COUNT - 1)) / EQ_BAR_COUNT;
    if (barWidth < 2) barWidth = 2;

    for (int i = 0; i < EQ_BAR_COUNT; i++) {
        int curX = x + i * (barWidth + 1);
        int barH = (int)_eqHeights[i];
        barH = constrain(barH, 1, height);

        int peakH = (int)_eqPeaks[i];
        peakH = constrain(peakH, 1, height);

        // Draw spectrum bar
        _display.fillRect(curX, y + height - barH, barWidth, barH, SSD1306_WHITE);

        // Draw floating peak cap line
        if (peakH > barH && peakH <= height) {
            _display.drawFastHLine(curX, y + height - peakH, barWidth, SSD1306_WHITE);
        }
    }
}

void DisplayManager::drawSpinningVinyl(int centerX, int centerY, int radius, uint8_t angleStep) {
    // Outer disc ring
    _display.drawCircle(centerX, centerY, radius, SSD1306_WHITE);
    // Inner groove ring
    if (radius > 6) {
        _display.drawCircle(centerX, centerY, radius - 3, SSD1306_WHITE);
    }
    if (radius > 12) {
        _display.drawCircle(centerX, centerY, radius - 7, SSD1306_WHITE);
    }
    // Center label ring & spindle
    int labelRadius = radius > 10 ? 4 : 2;
    _display.fillCircle(centerX, centerY, labelRadius, SSD1306_WHITE);
    _display.drawPixel(centerX, centerY, SSD1306_BLACK);

    // Rotating strobe reflections (4 spokes)
    float angleRad = (angleStep % 8) * (3.14159f / 4.0f);
    for (int s = 0; s < 4; s++) {
        float a = angleRad + (s * 1.57079f);
        int x1 = centerX + (int)(cos(a) * (labelRadius + 2));
        int y1 = centerY + (int)(sin(a) * (labelRadius + 2));
        int x2 = centerX + (int)(cos(a) * (radius - 1));
        int y2 = centerY + (int)(sin(a) * (radius - 1));
        _display.drawLine(x1, y1, x2, y2, SSD1306_WHITE);
    }

    // Tonearm indicator
    int armBaseX = centerX + radius + 5;
    int armBaseY = centerY - radius + 2;
    if (armBaseX < SCREEN_WIDTH - 2) {
        _display.drawFastVLine(armBaseX, armBaseY - 3, 6, SSD1306_WHITE);
        _display.drawLine(armBaseX, armBaseY, centerX + radius - 2, centerY - 2, SSD1306_WHITE);
        _display.fillRect(centerX + radius - 3, centerY - 3, 2, 3, SSD1306_WHITE); // Cartridge
    }
}

void DisplayManager::drawMiniCassette(int x, int y, uint8_t animFrame) {
    // Outer cassette shell (36x22)
    _display.drawRoundRect(x, y, 36, 22, 2, SSD1306_WHITE);
    // Center label area
    _display.drawRect(x + 4, y + 4, 28, 14, SSD1306_WHITE);
    
    // Left spool
    int sp1X = x + 11;
    int sp1Y = y + 11;
    _display.drawCircle(sp1X, sp1Y, 3, SSD1306_WHITE);
    _display.drawPixel(sp1X, sp1Y, SSD1306_WHITE);
    // Spool spokes rotation
    uint8_t step = animFrame % 4;
    if (step == 0 || step == 2) {
        _display.drawFastHLine(sp1X - 2, sp1Y, 5, SSD1306_WHITE);
    } else {
        _display.drawFastVLine(sp1X, sp1Y - 2, 5, SSD1306_WHITE);
    }

    // Right spool
    int sp2X = x + 25;
    int sp2Y = y + 11;
    _display.drawCircle(sp2X, sp2Y, 3, SSD1306_WHITE);
    _display.drawPixel(sp2X, sp2Y, SSD1306_WHITE);
    if (step == 0 || step == 2) {
        _display.drawFastHLine(sp2X - 2, sp2Y, 5, SSD1306_WHITE);
    } else {
        _display.drawFastVLine(sp2X, sp2Y - 2, 5, SSD1306_WHITE);
    }

    // Tape window bridge between spools
    _display.drawFastHLine(sp1X + 4, sp1Y, 6, SSD1306_WHITE);
}

void DisplayManager::renderPlayer(
    const String& trackName,
    const String& artistName,
    const String& activeLyric,
    const String& nextLyric,
    uint32_t progressMs,
    uint32_t durationMs,
    bool isPlaying,
    bool hasLyrics
) {
    uint32_t now = millis();
    if (now - _lastAnimFrameMs > 100) {
        _animFrame++;
        _lastAnimFrameMs = now;
    }

    _display.clearDisplay();
    _display.setTextColor(SSD1306_WHITE);

    // ========================================================================
    // 1. TOP HEADER: Mini Equalizer + Song Title Marquee (y = 0..11)
    // ========================================================================
    // Mini 3-bar animated audio badge on the left (8px wide)
    if (isPlaying) {
        int b1 = (int)(sin(now * 0.010f) * 3.0f + 4.0f);
        int b2 = (int)(cos(now * 0.013f) * 3.5f + 4.5f);
        int b3 = (int)(sin(now * 0.008f) * 3.0f + 4.0f);
        _display.fillRect(0, 9 - constrain(b1, 1, 8), 2, constrain(b1, 1, 8), SSD1306_WHITE);
        _display.fillRect(3, 9 - constrain(b2, 1, 8), 2, constrain(b2, 1, 8), SSD1306_WHITE);
        _display.fillRect(6, 9 - constrain(b3, 1, 8), 2, constrain(b3, 1, 8), SSD1306_WHITE);
    } else {
        _display.fillRect(0, 7, 2, 2, SSD1306_WHITE);
        _display.fillRect(3, 7, 2, 2, SSD1306_WHITE);
        _display.fillRect(6, 7, 2, 2, SSD1306_WHITE);
    }

    // Build header text: "Track Name - Artist Name"
    String fullTitle = trackName;
    if (artistName.length() > 0) {
        fullTitle += " - " + artistName;
    }

    if (fullTitle != _lastHeaderTrack) {
        _lastHeaderTrack = fullTitle;
        _headerScrollOffset = 0;
        _lastHeaderScrollMs = now;
    }

    _display.setTextSize(1);
    _display.setCursor(11, 0);

    // Header max visible characters: ~19 chars at 6px each
    if (fullTitle.length() > 19) {
        if (now - _lastHeaderScrollMs > 220) {
            _headerScrollOffset++;
            if (_headerScrollOffset > (int)fullTitle.length() - 14) {
                _headerScrollOffset = 0;
            }
            _lastHeaderScrollMs = now;
        }
        _display.print(fullTitle.substring(_headerScrollOffset));
    } else {
        _display.print(fullTitle);
    }

    // Header dividing line
    _display.drawFastHLine(0, 11, SCREEN_WIDTH, SSD1306_WHITE);

    // ========================================================================
    // 2. CENTER SECTION: Synced Lyrics & Dynamic Visualizer (y = 14..50)
    // ========================================================================
    bool isInstrumental = (!hasLyrics || activeLyric == "♪ ♪ ♪" || activeLyric == "..." || activeLyric.length() == 0);

    if (isInstrumental) {
        // --------------------------------------------------------------------
        // Instrumental / Solo / Interlude Animated Mode:
        // Spinning Vinyl Disc in Center + Dancing Visualizer Bars + Floating Notes
        // --------------------------------------------------------------------
        drawSpinningVinyl(24, 31, 14, _animFrame);
        drawEqualizer(52, 18, 38, 26, isPlaying);
        updateAndDrawFloatingNotes(96, 122, 14, 48, isPlaying);

        // Display small badge if no lyrics vs instrumental
        _display.setTextSize(1);
        _display.setCursor(54, 45);
        if (!hasLyrics) {
            _display.print(F("[No Lyrics]"));
        } else {
            _display.print(F("[Music ♪]"));
        }
    } else {
        // --------------------------------------------------------------------
        // Synced Lyrics Mode:
        // Highlighted Active Lyric (with auto-scroll) + Next Line Preview
        // --------------------------------------------------------------------
        if (activeLyric != _lastActiveLyric) {
            _lastActiveLyric = activeLyric;
            _lyricScrollOffset = 0;
            _lastLyricScrollMs = now;
        }

        // Active Lyric Line (y = 15..28)
        _display.setTextSize(1);
        _display.setCursor(0, 15);

        // Print active line indicator icon
        _display.print(F("> "));

        // Auto marquee scroll if active lyric > 19 chars
        if (activeLyric.length() > 19) {
            if (now - _lastLyricScrollMs > 200) {
                _lyricScrollOffset++;
                if (_lyricScrollOffset > (int)activeLyric.length() - 14) {
                    _lyricScrollOffset = 0;
                }
                _lastLyricScrollMs = now;
            }
            _display.println(activeLyric.substring(_lyricScrollOffset));
        } else {
            _display.println(activeLyric);
        }

        // Next Lyric Preview Line (y = 28..39)
        if (nextLyric.length() > 0) {
            _display.setCursor(6, 28);
            _display.setTextSize(1);
            _display.print(F("» "));
            if (nextLyric.length() > 18) {
                _display.print(nextLyric.substring(0, 18) + "..");
            } else {
                _display.print(nextLyric);
            }
        }

        // Mini visualizer spectrum bar accent on bottom right of lyrics area
        drawEqualizer(96, 38, 30, 11, isPlaying);
    }

    // ========================================================================
    // 3. FOOTER: Play/Pause Icon, Sleek Progress Bar, Time Display (y = 52..63)
    // ========================================================================
    _display.drawFastHLine(0, 51, SCREEN_WIDTH, SSD1306_WHITE);

    // Play / Pause Icon at (0, 54)
    if (isPlaying) {
        _display.fillTriangle(1, 54, 1, 62, 7, 58, SSD1306_WHITE);
    } else {
        _display.fillRect(1, 54, 2, 8, SSD1306_WHITE);
        _display.fillRect(5, 54, 2, 8, SSD1306_WHITE);
    }

    // Progress Bar (x=12, y=56, width=64, height=5)
    drawProgressBar(12, 56, 64, 5, progressMs, durationMs);

    // Time Elapsed: e.g. "02:45"
    _display.setTextSize(1);
    _display.setCursor(80, 55);
    _display.print(formatTime(progressMs));

    _display.display();
}

void DisplayManager::renderIdleScreen(const String& line1, const String& line2) {
    uint32_t now = millis();
    if (now - _lastAnimFrameMs > 100) {
        _animFrame++;
        _lastAnimFrameMs = now;
    }

    _display.clearDisplay();
    _display.setTextColor(SSD1306_WHITE);

    // Header
    _display.setTextSize(1);
    _display.setCursor(14, 2);
    _display.print(F("SPOTIFY OLED PLAYER"));
    _display.drawFastHLine(0, 12, SCREEN_WIDTH, SSD1306_WHITE);

    // Animated spinning vinyl disc on left
    drawSpinningVinyl(24, 37, 16, _animFrame);

    // Equalizer bars on the right
    drawEqualizer(50, 20, 36, 18, true);

    // Floating notes
    updateAndDrawFloatingNotes(92, 120, 16, 44, true);

    // Status text in footer
    _display.drawFastHLine(0, 50, SCREEN_WIDTH, SSD1306_WHITE);
    _display.setCursor(2, 54);
    _display.print(line1);

    // Animated pulsing dots
    uint8_t dots = (_animFrame / 3) % 4;
    for (uint8_t d = 0; d < dots; d++) {
        _display.print('.');
    }

    _display.display();
}

void DisplayManager::renderConnectingScreen(const String& title, const String& subtitle, uint8_t animStep) {
    _display.clearDisplay();
    _display.setTextColor(SSD1306_WHITE);

    // Cassette animation
    drawMiniCassette(46, 6, animStep);

    // Title
    _display.setTextSize(1);
    int titleX = max(0, (int)(128 - (title.length() * 6)) / 2);
    _display.setCursor(titleX, 34);
    _display.print(title);

    // Subtitle / IP / Detail
    int subX = max(0, (int)(128 - (subtitle.length() * 6)) / 2);
    _display.setCursor(subX, 47);
    _display.print(subtitle);

    _display.display();
}

void DisplayManager::showStatusMessage(const String& line1, const String& line2, const String& line3) {
    _display.clearDisplay();
    _display.setTextSize(1);
    _display.setTextColor(SSD1306_WHITE);

    _display.setCursor(0, 8);
    _display.println(line1);

    if (line2.length() > 0) {
        _display.setCursor(0, 26);
        _display.println(line2);
    }

    if (line3.length() > 0) {
        _display.setCursor(0, 44);
        _display.println(line3);
    }

    _display.display();
}
