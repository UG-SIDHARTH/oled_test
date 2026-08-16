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
const char* DEMO_TRACK_NAME = "Viva La Vida";
const char* DEMO_ARTIST_NAME = "Coldplay";
const uint32_t DEMO_DURATION_MS = 242000;
const char* DEMO_LRC = 
    "[00:01.00]♪ Instrumental Intro ♪\n"
    "[00:13.50]I used to rule the world\n"
    "[00:17.20]Seas would rise when I gave the word\n"
    "[00:21.00]Now in the morning I sleep alone\n"
    "[00:25.30]Sweep the streets I used to own\n"
    "[00:29.50]I used to roll the dice\n"
    "[00:33.20]Feel the fear in my enemy's eyes\n"
    "[00:37.00]Listen as the crowd would sing\n"
    "[00:41.00]Now the old king is dead! Long live the king!\n"
    "[00:45.00]One minute I held the key\n"
    "[00:49.00]Next the walls were closed on me\n"
    "[00:53.00]And I discovered that my castles stand\n"
    "[00:57.00]Upon pillars of salt and pillars of sand\n"
    "[01:01.00]I hear Jerusalem bells a-ringing\n"
    "[01:05.00]Roman Cavalry choirs are singing\n"
    "[01:09.00]Be my mirror, my sword and shield\n"
    "[01:13.00]My missionaries in a foreign field\n"
    "[01:17.00]For some reason I can't explain\n"
    "[01:21.00]Once you'd gone there was never\n"
    "[01:23.00]Never an honest word\n"
    "[01:25.00]And that was when I ruled the world\n"
    "[01:30.00]♪ Violin Orchestral Solo ♪\n";

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
