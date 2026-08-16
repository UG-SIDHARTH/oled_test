#include "DisplayManager.h"
#include <math.h>

DisplayManager::DisplayManager()
    : _u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE, /* clock=*/ OLED_SCL, /* data=*/ OLED_SDA) {
    for (int i = 0; i < EQ_BAR_COUNT; i++) {
        _eqHeights[i] = 2.0f;
        _eqPeaks[i] = 2.0f;
    }
    initFloatingNotes();
}

bool DisplayManager::begin() {
    Wire.begin(OLED_SDA, OLED_SCL);
    if (!_u8g2.begin()) {
        Serial.println(F("[OLED] U8g2 SSD1306 initialization failed! Check I2C address & wiring."));
        return false;
    }

    _u8g2.setFont(u8g2_font_6x10_tf);
    _u8g2.setFontPosTop();
    _u8g2.setDrawColor(1);
    _u8g2.clearBuffer();
    _u8g2.sendBuffer();
    return true;
}

void DisplayManager::clear() {
    _u8g2.clearBuffer();
    _u8g2.sendBuffer();
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
    _u8g2.drawFrame(x, y, width, height);
    if (durationMs > 0) {
        int fillWidth = map(constrain(progressMs, 0, durationMs), 0, durationMs, 0, width - 2);
        if (fillWidth > 0) {
            _u8g2.drawBox(x + 1, y + 1, fillWidth, height - 2);
        }
    }
}

void DisplayManager::drawMusicNoteGlyph(int x, int y, bool isDouble) {
    if (x < -8 || x > SCREEN_WIDTH || y < -8 || y > SCREEN_HEIGHT) return;
    
    if (isDouble) {
        // Double note ♫ glyph (7x7)
        _u8g2.drawBox(x, y + 4, 3, 3);       // Left head
        _u8g2.drawBox(x + 5, y + 3, 3, 3);   // Right head
        _u8g2.drawVLine(x + 2, y + 1, 4);    // Left stem
        _u8g2.drawVLine(x + 7, y, 4);        // Right stem
        _u8g2.drawHLine(x + 2, y, 6);        // Beam top
        _u8g2.drawHLine(x + 2, y + 1, 6);    // Beam thick
    } else {
        // Single note ♪ glyph (5x6)
        _u8g2.drawBox(x, y + 3, 3, 3);       // Note head
        _u8g2.drawVLine(x + 2, y, 4);        // Stem
        _u8g2.drawPixel(x + 3, y);           // Flag
        _u8g2.drawPixel(x + 4, y + 1);
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
        _u8g2.drawBox(curX, y + height - barH, barWidth, barH);

        // Draw floating peak cap line
        if (peakH > barH && peakH <= height) {
            _u8g2.drawHLine(curX, y + height - peakH, barWidth);
        }
    }
}

void DisplayManager::drawSpinningVinyl(int centerX, int centerY, int radius, uint8_t angleStep) {
    // Outer disc ring
    _u8g2.drawCircle(centerX, centerY, radius);
    // Inner groove ring
    if (radius > 6) {
        _u8g2.drawCircle(centerX, centerY, radius - 3);
    }
    if (radius > 12) {
        _u8g2.drawCircle(centerX, centerY, radius - 7);
    }
    // Center label ring & spindle
    int labelRadius = radius > 10 ? 4 : 2;
    _u8g2.drawDisc(centerX, centerY, labelRadius);
    _u8g2.setDrawColor(0);
    _u8g2.drawPixel(centerX, centerY);
    _u8g2.setDrawColor(1);

    // Rotating strobe reflections (4 spokes)
    float angleRad = (angleStep % 8) * (3.14159f / 4.0f);
    for (int s = 0; s < 4; s++) {
        float a = angleRad + (s * 1.57079f);
        int x1 = centerX + (int)(cos(a) * (labelRadius + 2));
        int y1 = centerY + (int)(sin(a) * (labelRadius + 2));
        int x2 = centerX + (int)(cos(a) * (radius - 1));
        int y2 = centerY + (int)(sin(a) * (radius - 1));
        _u8g2.drawLine(x1, y1, x2, y2);
    }

    // Tonearm indicator
    int armBaseX = centerX + radius + 5;
    int armBaseY = centerY - radius + 2;
    if (armBaseX < SCREEN_WIDTH - 2) {
        _u8g2.drawVLine(armBaseX, armBaseY - 3, 6);
        _u8g2.drawLine(armBaseX, armBaseY, centerX + radius - 2, centerY - 2);
        _u8g2.drawBox(centerX + radius - 3, centerY - 3, 2, 3); // Cartridge
    }
}

void DisplayManager::drawPixelDancingStickman(int centerX, int centerY, uint8_t animStep, bool isPlaying) {
    // 6-pose dance cycle (cycles smoothly every 200ms)
    uint8_t pose = isPlaying ? ((animStep / 2) % 6) : 0;
    uint8_t bob = isPlaying ? ((animStep % 2 == 0) ? 1 : 0) : 0;

    int headY = centerY - 13 + (pose == 2 ? 4 : (pose == 3 || pose == 4 ? -1 : 0)) + bob;
    int headX = centerX + (pose == 0 ? ((animStep % 4 < 2) ? -1 : 1) : 0);

    // 1. Cute Rounded Pixel Head (11x11 rounded box)
    _u8g2.drawRBox(headX - 6, headY - 6, 13, 12, 3);

    // 2. Black Eyes (draw color 0)
    _u8g2.setDrawColor(0);
    _u8g2.drawBox(headX - 4, headY - 3, 2, 3); // Left Eye
    _u8g2.drawBox(headX + 3, headY - 3, 2, 3); // Right Eye

    // 3. Cute Black Smile
    _u8g2.drawHLine(headX - 3, headY + 2, 7);
    _u8g2.drawPixel(headX - 3, headY + 1);
    _u8g2.drawPixel(headX + 3, headY + 1);
    _u8g2.setDrawColor(1);

    // 4. Torso / Body (2px thick)
    int bodyTopY = headY + 6;
    int bodyBottomY = bodyTopY + 11;
    if (pose == 2) bodyBottomY -= 2; // Squat compression

    _u8g2.drawVLine(centerX, bodyTopY, bodyBottomY - bodyTopY);
    _u8g2.drawVLine(centerX + 1, bodyTopY, bodyBottomY - bodyTopY);

    // 5. Arms & Legs per Pose (Matching Reference Video):
    if (!isPlaying) {
        // Idle standing pose
        _u8g2.drawLine(centerX, bodyTopY + 2, centerX - 7, bodyBottomY - 1);
        _u8g2.drawLine(centerX + 1, bodyTopY + 2, centerX + 8, bodyBottomY - 1);
        _u8g2.drawLine(centerX, bodyBottomY, centerX - 5, bodyBottomY + 11);
        _u8g2.drawLine(centerX + 1, bodyBottomY, centerX + 6, bodyBottomY + 11);
        return;
    }

    switch (pose) {
        case 0: // Pose 0: Hands on hips, hip sway & bounce
            _u8g2.drawLine(centerX, bodyTopY + 2, centerX - 7, bodyTopY + 6);
            _u8g2.drawLine(centerX - 7, bodyTopY + 6, centerX - 2, bodyBottomY - 1);
            _u8g2.drawLine(centerX + 1, bodyTopY + 2, centerX + 8, bodyTopY + 6);
            _u8g2.drawLine(centerX + 8, bodyTopY + 6, centerX + 3, bodyBottomY - 1);
            _u8g2.drawLine(centerX, bodyBottomY, centerX - 5, bodyBottomY + 11);
            _u8g2.drawLine(centerX + 1, bodyBottomY, centerX + 6, bodyBottomY + 11);
            break;

        case 1: // Pose 1: Wavy Arms Up (Groove shaking)
            _u8g2.drawLine(centerX, bodyTopY + 2, centerX - 6, bodyTopY + 5);
            _u8g2.drawLine(centerX - 6, bodyTopY + 5, centerX - 10, bodyTopY - 3);
            _u8g2.drawLine(centerX + 1, bodyTopY + 2, centerX + 7, bodyTopY + 5);
            _u8g2.drawLine(centerX + 7, bodyTopY + 5, centerX + 11, bodyTopY - 3);
            _u8g2.drawLine(centerX, bodyBottomY, centerX - 4, bodyBottomY + 11);
            _u8g2.drawLine(centerX + 1, bodyBottomY, centerX + 8, bodyBottomY + 10);
            break;

        case 2: // Pose 2: Groove Squat (Low dip, hands on knees, wide stance)
            _u8g2.drawLine(centerX, bodyTopY + 2, centerX - 9, bodyTopY + 5);
            _u8g2.drawLine(centerX - 9, bodyTopY + 5, centerX - 5, bodyBottomY + 3);
            _u8g2.drawLine(centerX + 1, bodyTopY + 2, centerX + 10, bodyTopY + 5);
            _u8g2.drawLine(centerX + 10, bodyTopY + 5, centerX + 6, bodyBottomY + 3);
            _u8g2.drawLine(centerX, bodyBottomY, centerX - 9, bodyBottomY + 4);
            _u8g2.drawLine(centerX - 9, bodyBottomY + 4, centerX - 7, bodyBottomY + 7);
            _u8g2.drawLine(centerX + 1, bodyBottomY, centerX + 10, bodyBottomY + 4);
            _u8g2.drawLine(centerX + 10, bodyBottomY + 4, centerX + 8, bodyBottomY + 7);
            break;

        case 3: // Pose 3: Disco Point Right ☝️ (Pointing high to the sky)
            _u8g2.drawLine(centerX + 1, bodyTopY + 2, centerX + 15, bodyTopY - 7);
            _u8g2.drawLine(centerX, bodyTopY + 2, centerX - 8, bodyBottomY + 2);
            _u8g2.drawLine(centerX, bodyBottomY, centerX - 8, bodyBottomY + 11);
            _u8g2.drawLine(centerX + 1, bodyBottomY, centerX + 4, bodyBottomY + 11);
            break;

        case 4: // Pose 4: Disco Point Left ☝️
            _u8g2.drawLine(centerX, bodyTopY + 2, centerX - 15, bodyTopY - 7);
            _u8g2.drawLine(centerX + 1, bodyTopY + 2, centerX + 8, bodyBottomY + 2);
            _u8g2.drawLine(centerX, bodyBottomY, centerX - 4, bodyBottomY + 11);
            _u8g2.drawLine(centerX + 1, bodyBottomY, centerX + 8, bodyBottomY + 11);
            break;

        case 5: // Pose 5: Outstretched Open Arms Groove
            _u8g2.drawLine(centerX, bodyTopY + 3, centerX - 13, bodyTopY + 3);
            _u8g2.drawLine(centerX + 1, bodyTopY + 3, centerX + 14, bodyTopY + 3);
            _u8g2.drawLine(centerX, bodyBottomY, centerX - 7, bodyBottomY + 11);
            _u8g2.drawLine(centerX + 1, bodyBottomY, centerX + 7, bodyBottomY + 11);
            break;
    }
}

void DisplayManager::drawMiniCassette(int x, int y, uint8_t animFrame) {
    // Outer cassette shell (36x22)
    _u8g2.drawRFrame(x, y, 36, 22, 2);
    // Center label area
    _u8g2.drawFrame(x + 4, y + 4, 28, 14);
    
    // Left spool
    int sp1X = x + 11;
    int sp1Y = y + 11;
    _u8g2.drawCircle(sp1X, sp1Y, 3);
    _u8g2.drawPixel(sp1X, sp1Y);
    uint8_t step = animFrame % 4;
    if (step == 0 || step == 2) {
        _u8g2.drawHLine(sp1X - 2, sp1Y, 5);
    } else {
        _u8g2.drawVLine(sp1X, sp1Y - 2, 5);
    }

    // Right spool
    int sp2X = x + 25;
    int sp2Y = y + 11;
    _u8g2.drawCircle(sp2X, sp2Y, 3);
    _u8g2.drawPixel(sp2X, sp2Y);
    if (step == 0 || step == 2) {
        _u8g2.drawHLine(sp2X - 2, sp2Y, 5);
    } else {
        _u8g2.drawVLine(sp2X, sp2Y - 2, 5);
    }

    // Tape window bridge between spools
    _u8g2.drawHLine(sp1X + 4, sp1Y, 6);
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

    _u8g2.clearBuffer();
    _u8g2.setFont(u8g2_font_6x10_tf);
    _u8g2.setFontPosTop();
    _u8g2.setDrawColor(1);

    // ========================================================================
    // 1. TOP HEADER: Song Name & Artist Marquee (y = 0..10)
    // ========================================================================
    String fullTitle = trackName;
    if (artistName.length() > 0) {
        fullTitle += " - " + artistName;
    }

    if (fullTitle != _lastHeaderTrack) {
        _lastHeaderTrack = fullTitle;
        _headerScrollOffset = 0;
        _lastHeaderScrollMs = now;
    }

    // Header text display
    _u8g2.setCursor(0, 0);
    if (fullTitle.length() > 21) {
        if (now - _lastHeaderScrollMs > 220) {
            _headerScrollOffset++;
            if (_headerScrollOffset > (int)fullTitle.length() - 16) {
                _headerScrollOffset = 0;
            }
            _lastHeaderScrollMs = now;
        }
        _u8g2.print(fullTitle.substring(_headerScrollOffset));
    } else {
        // Centered header
        int tWidth = fullTitle.length() * 6;
        int startX = (SCREEN_WIDTH - tWidth) / 2;
        if (startX < 0) startX = 0;
        _u8g2.setCursor(startX, 0);
        _u8g2.print(fullTitle);
    }

    // Top dividing line
    _u8g2.drawHLine(0, 10, SCREEN_WIDTH);

    // ========================================================================
    // 2. CENTER SECTION: Cute Pixel Dancing Stickman + Floating Notes (y = 11..50)
    // ========================================================================
    drawPixelDancingStickman(64, 30, _animFrame, isPlaying);
    updateAndDrawFloatingNotes(14, 36, 12, 46, isPlaying);
    updateAndDrawFloatingNotes(92, 114, 12, 46, isPlaying);

    // ========================================================================
    // 3. BOTTOM FOOTER: Synced Karaoke Lyrics Line (y = 51..63)
    // ========================================================================
    _u8g2.drawHLine(0, 51, SCREEN_WIDTH);

    String lyricText = activeLyric;
    if (lyricText.length() == 0) {
        if (isPlaying) {
            lyricText = "♪ Moving to the byte beat! ♪";
        } else {
            lyricText = "Paused ⏸";
        }
    }

    if (lyricText != _lastActiveLyric) {
        _lastActiveLyric = lyricText;
        _lyricScrollOffset = 0;
        _lastLyricScrollMs = now;
    }

    _u8g2.setCursor(0, 53);
    if (lyricText.length() > 21) {
        if (now - _lastLyricScrollMs > 200) {
            _lyricScrollOffset++;
            if (_lyricScrollOffset > (int)lyricText.length() - 17) {
                _lyricScrollOffset = 0;
            }
            _lastLyricScrollMs = now;
        }
        _u8g2.print(lyricText.substring(_lyricScrollOffset));
    } else {
        // Centered text
        int textWidth = lyricText.length() * 6;
        int startX = (SCREEN_WIDTH - textWidth) / 2;
        if (startX < 0) startX = 0;
        _u8g2.setCursor(startX, 53);
        _u8g2.print(lyricText);
    }

    _u8g2.sendBuffer();
}

void DisplayManager::renderIdleScreen(const String& line1, const String& line2) {
    uint32_t now = millis();
    if (now - _lastAnimFrameMs > 100) {
        _animFrame++;
        _lastAnimFrameMs = now;
    }

    _u8g2.clearBuffer();
    _u8g2.setFont(u8g2_font_6x10_tf);
    _u8g2.setFontPosTop();
    _u8g2.setDrawColor(1);

    // Header
    _u8g2.setCursor(14, 2);
    _u8g2.print(F("SPOTIFY OLED PLAYER"));
    _u8g2.drawHLine(0, 12, SCREEN_WIDTH);

    // Animated spinning vinyl disc on left
    drawSpinningVinyl(24, 37, 16, _animFrame);

    // Equalizer bars on the right
    drawEqualizer(50, 20, 36, 18, true);

    // Floating notes
    updateAndDrawFloatingNotes(92, 120, 16, 44, true);

    // Status text in footer
    _u8g2.drawHLine(0, 50, SCREEN_WIDTH);
    _u8g2.setCursor(2, 54);
    _u8g2.print(line1);

    // Animated pulsing dots
    uint8_t dots = (_animFrame / 3) % 4;
    for (uint8_t d = 0; d < dots; d++) {
        _u8g2.print('.');
    }

    _u8g2.sendBuffer();
}

void DisplayManager::renderConnectingScreen(const String& title, const String& subtitle, uint8_t animStep) {
    _u8g2.clearBuffer();
    _u8g2.setFont(u8g2_font_6x10_tf);
    _u8g2.setFontPosTop();
    _u8g2.setDrawColor(1);

    // Cassette animation
    drawMiniCassette(46, 6, animStep);

    // Title
    int titleX = max(0, (int)(128 - (title.length() * 6)) / 2);
    _u8g2.setCursor(titleX, 34);
    _u8g2.print(title);

    // Subtitle / IP / Detail
    int subX = max(0, (int)(128 - (subtitle.length() * 6)) / 2);
    _u8g2.setCursor(subX, 47);
    _u8g2.print(subtitle);

    _u8g2.sendBuffer();
}

void DisplayManager::showStatusMessage(const String& line1, const String& line2, const String& line3) {
    _u8g2.clearBuffer();
    _u8g2.setFont(u8g2_font_6x10_tf);
    _u8g2.setFontPosTop();
    _u8g2.setDrawColor(1);

    _u8g2.setCursor(0, 8);
    _u8g2.print(line1);

    if (line2.length() > 0) {
        _u8g2.setCursor(0, 26);
        _u8g2.print(line2);
    }

    if (line3.length() > 0) {
        _u8g2.setCursor(0, 44);
        _u8g2.print(line3);
    }

    _u8g2.sendBuffer();
}
