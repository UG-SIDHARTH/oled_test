/*
 * ============================================================================
 * ESP32 Spotify OLED Music Player & Animations (100% OFFLINE / ZERO WIFI)
 * Project: oled_test
 * ============================================================================
 * 
 * Hardware:
 *   - ESP32 Development Board (NodeMCU / DevKit / WROOM)
 *   - SSD1306 OLED Display 128x64 (I2C)
 * 
 * Pinout:
 *   - OLED SDA -> ESP32 GPIO 21
 *   - OLED SCL -> ESP32 GPIO 22
 *   - OLED VCC -> 3.3V / 5V
 *   - OLED GND -> GND
 * 
 * Features:
 *   - 📱 100% OFFLINE BLUETOOTH MODE: Zero WiFi needed!
 *   - 📊 6-Band Spectrum Visualizer: Peak-hold decay animation
 *   - 💿 Spinning Vinyl Record & Tonearm
 *   - 🎵 Floating Musical Notes (♪ ♫)
 *   - 📜 Smart Marquee Scrolling song titles & artist names
 *   - ⏱️ Live Progress Bar & Elapsed Timer
 * ============================================================================
 */

#include "config.h"
#include "SpotifyClient.h"
#include "LyricsClient.h"
#include "DisplayManager.h"

#if USE_BLUETOOTH_MODE
#include "BluetoothManager.h"
BluetoothManager btManager;
#elif USE_LASTFM_AUTO_SYNC
#include <WiFi.h>
#include "LastFmClient.h"
LastFmClient lastfm(LASTFM_API_KEY, LASTFM_USERNAME);
#elif !DEMO_MODE
#include <WiFi.h>
SpotifyClient spotify(SPOTIFY_CLIENT_ID, SPOTIFY_CLIENT_SECRET, SPOTIFY_REFRESH_TOKEN);
#endif

LyricsClient lyrics;
DisplayManager display;

// Global state tracking
SpotifyTrackInfo currentTrack;
String activeTrackTitle = "";
uint32_t lastSpotifyPollMs = 0;
uint32_t lastFrameMs = 0;

// Built-in Demo Song for Offline Demonstration
#if DEMO_MODE
const char* DEMO_TRACK_NAME = "Pixel Dance";
const char* DEMO_ARTIST_NAME = "ESP32 Beat";
const uint32_t DEMO_DURATION_MS = 30000;
const char* DEMO_LRC = 
    "[00:01.00]Watch the little pixel dance\n"
    "[00:04.00]Moving to the byte beat!\n"
    "[00:07.00]ESP32 takes the lead\n"
    "[00:10.00]Feel the rhythm, feel the heat!\n"
    "[00:14.00]Watch the little pixel dance\n"
    "[00:18.00]Moving to the byte beat!\n"
    "[00:22.00]Feel the rhythm, feel the heat!\n"
    "[00:26.00]♪ Byte Beat Solo ♪\n";
uint32_t demoStartTimeMs = 0;
#endif

void setup() {
    Serial.begin(115200);
    delay(300);
    Serial.println("\n=============================================");
    Serial.println(" ESP32 Spotify OLED Player (ZERO WIFI MODE)");
    Serial.println("=============================================");

    // Initialize OLED Display
    if (!display.begin()) {
        Serial.println("[Error] SSD1306 Display initialization failed!");
        while (true) delay(1000); // Halt if OLED missing
    }

#if DEMO_MODE
    Serial.println("[Mode] Running in DEMO_MODE (Offline Animation Preview)");
    currentTrack.hasData = true;
    currentTrack.isPlaying = true;
    currentTrack.trackName = DEMO_TRACK_NAME;
    currentTrack.artistName = DEMO_ARTIST_NAME;
    currentTrack.durationMs = DEMO_DURATION_MS;
    currentTrack.progressMs = 0;
    currentTrack.lastFetchTimeMs = millis();
    lyrics.loadLrcContent(DEMO_LRC);
    demoStartTimeMs = millis();
    display.renderConnectingScreen("Demo Mode Active", "Starting Song...", 1);
    delay(1000);

#elif USE_BLUETOOTH_MODE
    Serial.println("[Mode] Running in PURE OFFLINE BLUETOOTH MODE (Zero WiFi)");
    display.renderConnectingScreen("Starting Bluetooth", BLUETOOTH_DEVICE_NAME, 1);
    btManager.begin(BLUETOOTH_DEVICE_NAME);
    delay(200);
    display.renderConnectingScreen("Pair Bluetooth", BLUETOOTH_DEVICE_NAME, 0);
    Serial.println("[Bluetooth] Ready! Pair your Phone or PC to '" + String(BLUETOOTH_DEVICE_NAME) + "'");

#endif
}

void loop() {
    uint32_t currentMs = millis();

#if DEMO_MODE
    // ------------------------------------------------------------------------
    // 1. DEMO MODE
    // ------------------------------------------------------------------------
    uint32_t elapsed = currentMs - demoStartTimeMs;
    currentTrack.progressMs = elapsed % (DEMO_DURATION_MS + 2000);
    currentTrack.isPlaying = true;
    currentTrack.hasData = true;

#elif USE_BLUETOOTH_MODE
    // ------------------------------------------------------------------------
    // 2. OFFLINE BLUETOOTH MODE (Zero WiFi)
    // ------------------------------------------------------------------------
    btManager.update();

    String trackName = btManager.getTrackName();
    if (trackName.length() > 0) {
        currentTrack.hasData = true;
        currentTrack.trackName = trackName;
        currentTrack.artistName = btManager.getArtistName();
        currentTrack.albumName = btManager.getAlbumName();
        currentTrack.durationMs = btManager.getDurationMs();
        currentTrack.isPlaying = btManager.isPlaying();
        currentTrack.progressMs = btManager.getProgressMs();

        if (trackName != activeTrackTitle) {
            activeTrackTitle = trackName;
            Serial.printf("\n[Now Playing] %s by %s\n", trackName.c_str(), currentTrack.artistName.c_str());
        }
    } else {
        currentTrack.hasData = false;
        currentTrack.isPlaying = false;
    }

#endif

    // ------------------------------------------------------------------------
    // Progress Extrapolation
    // ------------------------------------------------------------------------
    uint32_t estimatedProgressMs = currentTrack.progressMs;

    // ------------------------------------------------------------------------
    // OLED UI Rendering (~25 FPS / 40ms frame time)
    // ------------------------------------------------------------------------
    uint32_t frameInterval = 1000 / DISPLAY_FPS;
    if (currentMs - lastFrameMs >= frameInterval) {
        lastFrameMs = currentMs;

        if (currentTrack.hasData && currentTrack.isPlaying) {
            String activeLyric = btManager.hasLyrics() ? btManager.getActiveLyric() : lyrics.getActiveLyric(estimatedProgressMs);
            String nextLyric = btManager.hasLyrics() ? btManager.getNextLyric() : lyrics.getNextLyric(estimatedProgressMs);
            bool hasLyrics = btManager.hasLyrics() || lyrics.hasLyrics();

            display.renderPlayer(
                currentTrack.trackName,
                currentTrack.artistName,
                activeLyric,
                nextLyric,
                estimatedProgressMs,
                currentTrack.durationMs,
                currentTrack.isPlaying,
                hasLyrics
            );
        } else {
#if USE_BLUETOOTH_MODE
            if (btManager.isConnected()) {
                display.renderIdleScreen("Bluetooth Paired", "Play music on PC/Phone");
            } else {
                display.renderConnectingScreen("Pair Bluetooth", BLUETOOTH_DEVICE_NAME, 0);
            }
#else
            display.renderIdleScreen("Spotify Ready", "Play a song");
#endif
        }
    }

    yield(); // Keep ESP32 watchdog responsive
}
