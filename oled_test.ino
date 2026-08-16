/*
 * ============================================================================
 * ESP32 Spotify Synced Lyrics & Dynamic Animations on SSD1306 OLED
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
 * Dependencies (Install via Arduino Library Manager):
 *   1. U8g2 by Oliver (for high-performance SSD1306 graphics)
 *   2. ArduinoJson (v6 or v7)
 * 
 * Features:
 *   - Real-time Synced Lyrics from LRCLIB with Auto-scrolling & Next-line preview
 *   - Dynamic Bouncing Equalizer / Spectrum Visualizer with Peak Hold Decay
 *   - Animated Spinning Vinyl Record & Tonearm
 *   - Floating Musical Notes (♪ ♫)
 *   - Animated Retro Cassette WiFi Connection & Standby Screens
 *   - Offline DEMO_MODE toggle in config.h to test animations immediately
 * ============================================================================
 */

#include <WiFi.h>
#include "config.h"
#include "SpotifyClient.h"
#include "LyricsClient.h"
#include "DisplayManager.h"

// Instantiate clients
#if !DEMO_MODE
SpotifyClient spotify(SPOTIFY_CLIENT_ID, SPOTIFY_CLIENT_SECRET, SPOTIFY_REFRESH_TOKEN);
#endif
LyricsClient lyrics;
DisplayManager display;

// Global state tracking
SpotifyTrackInfo currentTrack;
String activeTrackId = "";
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

void connectWiFi() {
    Serial.printf("[WiFi] Connecting to %s", WIFI_SSID);
    
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 30) {
        display.renderConnectingScreen("Connecting WiFi...", WIFI_SSID, attempts);
        delay(400);
        Serial.print(".");
        attempts++;
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\n[WiFi] Connected!");
        Serial.printf("[WiFi] IP Address: %s\n", WiFi.localIP().toString().c_str());
        display.renderConnectingScreen("WiFi Connected!", WiFi.localIP().toString(), 0);
        delay(1200);
    } else {
        Serial.println("\n[WiFi] Connection Failed!");
        display.showStatusMessage("WiFi Failed!", "Check config.h", "Retrying...");
        delay(2000);
    }
}

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("\n=============================================");
    Serial.println(" ESP32 Spotify Lyrics & Animations Display");
    Serial.println("=============================================");

    // Initialize OLED Display
    if (!display.begin()) {
        Serial.println("[Error] SSD1306 Display initialization failed!");
        while (true) delay(1000); // Halt if OLED missing
    }

    display.renderConnectingScreen("OLED Player", "Initializing...", 0);
    delay(1000);

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
#else
    // Connect to WiFi
    connectWiFi();
#endif
}

void loop() {
    uint32_t currentMs = millis();

#if DEMO_MODE
    // ------------------------------------------------------------------------
    // DEMO MODE: Simulate smooth real-time playback
    // ------------------------------------------------------------------------
    uint32_t elapsed = currentMs - demoStartTimeMs;
    currentTrack.progressMs = elapsed % (DEMO_DURATION_MS + 2000);
    currentTrack.isPlaying = true;
    currentTrack.hasData = true;

#else
    // ------------------------------------------------------------------------
    // LIVE SPOTIFY MODE
    // ------------------------------------------------------------------------
    // Reconnect WiFi if connection dropped
    if (WiFi.status() != WL_CONNECTED) {
        connectWiFi();
        return;
    }

    // Periodic Spotify Playback State Refresh (Every 3 seconds)
    if (currentMs - lastSpotifyPollMs >= SPOTIFY_POLL_INTERVAL || lastSpotifyPollMs == 0) {
        lastSpotifyPollMs = currentMs;

        SpotifyTrackInfo freshTrack;
        if (spotify.getCurrentlyPlaying(freshTrack)) {
            currentTrack = freshTrack;

            // Check if song changed
            if (currentTrack.hasData && currentTrack.trackId != activeTrackId) {
                activeTrackId = currentTrack.trackId;
                Serial.printf("\n[Track Changed] Now Playing: %s by %s\n", 
                              currentTrack.trackName.c_str(), 
                              currentTrack.artistName.c_str());

                display.renderConnectingScreen("Fetching Lyrics...", currentTrack.trackName, 2);

                // Fetch new synced lyrics from LRCLIB
                lyrics.fetchSyncedLyrics(
                    currentTrack.trackName,
                    currentTrack.artistName,
                    currentTrack.albumName,
                    currentTrack.durationMs
                );
            }
        }
    }
#endif

    // ------------------------------------------------------------------------
    // Track Progress Extrapolation (Smooth Millis Interpolation)
    // ------------------------------------------------------------------------
    uint32_t estimatedProgressMs = currentTrack.progressMs;
#if !DEMO_MODE
    if (currentTrack.isPlaying && currentTrack.lastFetchTimeMs > 0) {
        estimatedProgressMs += (currentMs - currentTrack.lastFetchTimeMs);
        if (currentTrack.durationMs > 0 && estimatedProgressMs > currentTrack.durationMs) {
            estimatedProgressMs = currentTrack.durationMs;
        }
    }
#endif

    // ------------------------------------------------------------------------
    // Frame Rate Limiter & OLED UI Rendering (~25 FPS / 40ms frame time)
    // ------------------------------------------------------------------------
    uint32_t frameInterval = 1000 / DISPLAY_FPS;
    if (currentMs - lastFrameMs >= frameInterval) {
        lastFrameMs = currentMs;

        if (currentTrack.hasData) {
            String activeLyric = lyrics.getActiveLyric(estimatedProgressMs);
            String nextLyric = lyrics.getNextLyric(estimatedProgressMs);

            display.renderPlayer(
                currentTrack.trackName,
                currentTrack.artistName,
                activeLyric,
                nextLyric,
                estimatedProgressMs,
                currentTrack.durationMs,
                currentTrack.isPlaying,
                lyrics.hasLyrics()
            );
        } else {
            display.renderIdleScreen("Spotify Ready", "Play a song");
        }
    }

    yield(); // Keep ESP32 watchdog and background WiFi stack happy
}
