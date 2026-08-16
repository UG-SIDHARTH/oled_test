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

void DisplayManager::drawDancingCharacter(int x, int y, uint8_t animStep, bool isPlaying) {
    // x, y is top-left anchor (approx 20x34 pixels)
    uint8_t step = isPlaying ? (animStep % 4) : 0;
    
    // Head position with dance bounce
    int headY = y + 4 + (step == 1 ? 2 : (step == 3 ? -2 : 0));
    int headX = x + 10 + (step == 0 ? -1 : (step == 2 ? 1 : 0));
    
    // 1. Head & Headphones
    _u8g2.drawDisc(headX, headY, 3);
    // Headphones cups on left/right
    _u8g2.drawBox(headX - 4, headY - 2, 2, 4);
    _u8g2.drawBox(headX + 3, headY - 2, 2, 4);
    _u8g2.drawHLine(headX - 3, headY - 4, 7); // Headband
    
    // Sunglasses / visor
    _u8g2.setDrawColor(0);
    _u8g2.drawHLine(headX - 2, headY, 4);
    _u8g2.setDrawColor(1);
    
    // 2. Torso / Shirt
    int bodyTopY = headY + 4;
    int bodyBottomY = bodyTopY + 8;
    _u8g2.drawLine(headX, bodyTopY, headX, bodyBottomY);
    _u8g2.drawBox(headX - 2, bodyTopY + 1, 5, 5); // Shirt
    
    // 3. Arms & Poses
    if (!isPlaying) {
        // Idle chill pose (hands down)
        _u8g2.drawLine(headX - 2, bodyTopY + 2, headX - 5, bodyBottomY);
        _u8g2.drawLine(headX + 2, bodyTopY + 2, headX + 5, bodyBottomY);
        _u8g2.drawLine(headX, bodyBottomY, headX - 3, bodyBottomY + 9);
        _u8g2.drawLine(headX, bodyBottomY, headX + 3, bodyBottomY + 9);
        return;
    }
    
    switch (step) {
        case 0: // Disco Point Left ☝️
            _u8g2.drawLine(headX - 2, bodyTopY + 2, headX - 8, bodyTopY - 4); // Left arm points up
            _u8g2.drawLine(headX + 2, bodyTopY + 2, headX + 5, bodyBottomY);     // Right arm on hip
            _u8g2.drawLine(headX, bodyBottomY, headX - 6, bodyBottomY + 9);     // Left leg kicks out
            _u8g2.drawLine(headX, bodyBottomY, headX + 3, bodyBottomY + 9);     // Right leg straight
            break;
            
        case 1: // Groove Squat / Body Wave
            _u8g2.drawLine(headX - 2, bodyTopY + 2, headX - 7, bodyTopY + 7);  // Left arm waving
            _u8g2.drawLine(headX + 2, bodyTopY + 2, headX + 7, bodyTopY + 1);  // Right arm up
            _u8g2.drawLine(headX, bodyBottomY, headX - 5, bodyBottomY + 7);     // Wide stance left
            _u8g2.drawLine(headX, bodyBottomY, headX + 5, bodyBottomY + 7);     // Wide stance right
            break;
            
        case 2: // Disco Point Right ☝️
            _u8g2.drawLine(headX + 2, bodyTopY + 2, headX + 8, bodyTopY - 4); // Right arm points up
            _u8g2.drawLine(headX - 2, bodyTopY + 2, headX - 5, bodyBottomY);     // Left arm on hip
            _u8g2.drawLine(headX, bodyBottomY, headX + 6, bodyBottomY + 9);     // Right leg kicks out
            _u8g2.drawLine(headX, bodyBottomY, headX - 3, bodyBottomY + 9);     // Left leg straight
            break;
            
        case 3: // Double Pump / Hands Up 🙌
            _u8g2.drawLine(headX - 2, bodyTopY + 2, headX - 7, bodyTopY - 4);  // Left hand up
            _u8g2.drawLine(headX + 2, bodyTopY + 2, headX + 7, bodyTopY - 4);  // Right hand up
            _u8g2.drawLine(headX, bodyBottomY, headX - 4, bodyBottomY + 6);     // Jump tuck left
            _u8g2.drawLine(headX, bodyBottomY, headX + 4, bodyBottomY + 6);     // Jump tuck right
            _u8g2.drawPixel(headX - 8, bodyTopY - 6); // Sparkle note
            _u8g2.drawPixel(headX + 8, bodyTopY - 6);
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
    // 1. TOP HEADER: Mini Equalizer + Song Title Marquee (y = 0..11)
    // ========================================================================
    // Mini 3-bar animated audio badge on the left (8px wide)
    if (isPlaying) {
        int b1 = (int)(sin(now * 0.010f) * 3.0f + 4.0f);
        int b2 = (int)(cos(now * 0.013f) * 3.5f + 4.5f);
        int b3 = (int)(sin(now * 0.008f) * 3.0f + 4.0f);
        _u8g2.drawBox(0, 9 - constrain(b1, 1, 8), 2, constrain(b1, 1, 8));
        _u8g2.drawBox(3, 9 - constrain(b2, 1, 8), 2, constrain(b2, 1, 8));
        _u8g2.drawBox(6, 9 - constrain(b3, 1, 8), 2, constrain(b3, 1, 8));
    } else {
        _u8g2.drawBox(0, 7, 2, 2);
        _u8g2.drawBox(3, 7, 2, 2);
        _u8g2.drawBox(6, 7, 2, 2);
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

    _u8g2.setCursor(11, 0);

    // Header max visible characters: ~19 chars at 6px each
    if (fullTitle.length() > 19) {
        if (now - _lastHeaderScrollMs > 220) {
            _headerScrollOffset++;
            if (_headerScrollOffset > (int)fullTitle.length() - 14) {
                _headerScrollOffset = 0;
            }
            _lastHeaderScrollMs = now;
        }
        _u8g2.print(fullTitle.substring(_headerScrollOffset));
    } else {
        _u8g2.print(fullTitle);
    }

    // Header dividing line
    _u8g2.drawHLine(0, 11, SCREEN_WIDTH);

    // ========================================================================
    // 2. CENTER SECTION: Synced Lyrics & Dynamic Visualizer (y = 14..50)
    // ========================================================================
    bool isInstrumental = (!hasLyrics || activeLyric == "♪ ♪ ♪" || activeLyric == "..." || activeLyric.length() == 0);

    if (isInstrumental) {
        // --------------------------------------------------------------------
        // Instrumental / Visualizer + Groovy Dancing Character Mode:
        // Spinning Vinyl Disc + 6-Band Equalizer + Dancing Character + Notes
        // --------------------------------------------------------------------
        drawSpinningVinyl(16, 31, 12, _animFrame);
        drawEqualizer(34, 18, 42, 26, isPlaying);
        drawDancingCharacter(84, 13, _animFrame, isPlaying);
        updateAndDrawFloatingNotes(112, 126, 14, 48, isPlaying);

        // Display small badge
        _u8g2.setCursor(38, 45);
        if (!hasLyrics) {
            _u8g2.print(F("[No Lyrics]"));
        } else {
            _u8g2.print(F("[Music ♪]"));
        }
    } else {
        // --------------------------------------------------------------------
        // Synced Lyrics + Dancing Character Mode:
        // Highlighted Active Lyric + Next Line Preview + Dancing Character
        // --------------------------------------------------------------------
        if (activeLyric != _lastActiveLyric) {
            _lastActiveLyric = activeLyric;
            _lyricScrollOffset = 0;
            _lastLyricScrollMs = now;
        }

        // Active Lyric Line (y = 15..28) - Leave room for dancer on right
        _u8g2.setCursor(0, 15);
        _u8g2.print(F("> "));

        // Auto marquee scroll if active lyric > 16 chars
        if (activeLyric.length() > 16) {
            if (now - _lastLyricScrollMs > 200) {
                _lyricScrollOffset++;
                if (_lyricScrollOffset > (int)activeLyric.length() - 13) {
                    _lyricScrollOffset = 0;
                }
                _lastLyricScrollMs = now;
            }
            _u8g2.print(activeLyric.substring(_lyricScrollOffset));
        } else {
            _u8g2.print(activeLyric);
        }

        // Next Lyric Preview Line (y = 28..39)
        if (nextLyric.length() > 0) {
            _u8g2.setCursor(4, 28);
            _u8g2.print(F("» "));
            if (nextLyric.length() > 15) {
                _u8g2.print(nextLyric.substring(0, 15) + "..");
            } else {
                _u8g2.print(nextLyric);
            }
        }

        // Mini visualizer spectrum bar accent on bottom left
        drawEqualizer(0, 39, 42, 10, isPlaying);

        // Groovy Dancing Character on the right!
        drawDancingCharacter(105, 13, _animFrame, isPlaying);
    }

    // ========================================================================
    // 3. FOOTER: Play/Pause Icon, Sleek Progress Bar, Time Display (y = 52..63)
    // ========================================================================
    _u8g2.drawHLine(0, 51, SCREEN_WIDTH);

    // Play / Pause Icon at (0, 54)
    if (isPlaying) {
        _u8g2.drawTriangle(1, 54, 1, 62, 7, 58);
    } else {
        _u8g2.drawBox(1, 54, 2, 8);
        _u8g2.drawBox(5, 54, 2, 8);
    }

    // Progress Bar (x=12, y=56, width=64, height=5)
    drawProgressBar(12, 56, 64, 5, progressMs, durationMs);

    // Time Elapsed: e.g. "02:45"
    _u8g2.setCursor(80, 54);
    _u8g2.print(formatTime(progressMs));

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
