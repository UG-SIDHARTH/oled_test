/*
 * ============================================================================
 * ESP32 Spotify Synced Lyrics Display on SSD1306 OLED
 * Project: oled_test
 * ============================================================================
 * 
 * Hardware:
 *   - ESP32 Development Board (NodeMCU / DevKit)
 *   - SSD1306 OLED Display 128x64 (I2C)
 * 
 * Pinout:
 *   - OLED SDA -> ESP32 GPIO 21
 *   - OLED SCL -> ESP32 GPIO 22
 *   - OLED VCC -> 3.3V / 5V
 *   - OLED GND -> GND
 * 
 * Dependencies (Install via Arduino Library Manager):
 *   1. Adafruit SSD1306
 *   2. Adafruit GFX Library
 *   3. ArduinoJson (v6 or higher)
 */

#include <WiFi.h>
#include "config.h"
#include "SpotifyClient.h"
#include "LyricsClient.h"
#include "DisplayManager.h"

// Instantiate clients
SpotifyClient spotify(SPOTIFY_CLIENT_ID, SPOTIFY_CLIENT_SECRET, SPOTIFY_REFRESH_TOKEN);
LyricsClient lyrics;
DisplayManager display;

// Global state tracking
SpotifyTrackInfo currentTrack;
String activeTrackId = "";
uint32_t lastSpotifyPollMs = 0;

void connectWiFi() {
    Serial.printf("[WiFi] Connecting to %s...", WIFI_SSID);
    display.showStatusMessage("Connecting WiFi...", WIFI_SSID, "Please wait...");

    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 30) {
        delay(500);
        Serial.print(".");
        attempts++;
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\n[WiFi] Connected!");
        Serial.printf("[WiFi] IP Address: %s\n", WiFi.localIP().toString().c_str());
        display.showStatusMessage("WiFi Connected!", WiFi.localIP().toString(), "Syncing Spotify...");
        delay(1000);
    } else {
        Serial.println("\n[WiFi] Connection Failed!");
        display.showStatusMessage("WiFi Failed!", "Check config.h", "Retrying...");
    }
}

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("\n=============================================");
    Serial.println(" ESP32 Spotify Lyrics Display (oled_test)");
    Serial.println("=============================================");

    // Initialize OLED Display
    if (!display.begin()) {
        Serial.println("[Error] Display initialization failed!");
        while (true) delay(1000); // Halt if OLED missing
    }

    display.showStatusMessage("ESP32 Spotify Lyrics", "Initializing...", "oled_test v1.0");
    delay(1500);

    // Connect to WiFi
    connectWiFi();
}

void loop() {
    // Reconnect WiFi if connection dropped
    if (WiFi.status() != WL_CONNECTED) {
        connectWiFi();
        return;
    }

    uint32_t currentMs = millis();

    // ------------------------------------------------------------------------
    // 1. Periodic Spotify Playback State Refresh (Every 3 seconds)
    // ------------------------------------------------------------------------
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

                display.showStatusMessage("Fetching Lyrics...", currentTrack.trackName, currentTrack.artistName);

                // Fetch new synced lyrics from LRCLIB
                lyrics.fetchSyncedLyrics(currentTrack.trackName, currentTrack.artistName, currentTrack.albumName, currentTrack.durationMs);
            }
        }
    }

    // ------------------------------------------------------------------------
    // 2. Extrapolate Track Progress locally using millis()
    // ------------------------------------------------------------------------
    uint32_t estimatedProgressMs = currentTrack.progressMs;
    if (currentTrack.isPlaying && currentTrack.lastFetchTimeMs > 0) {
        estimatedProgressMs += (currentMs - currentTrack.lastFetchTimeMs);
        if (currentTrack.durationMs > 0 && estimatedProgressMs > currentTrack.durationMs) {
            estimatedProgressMs = currentTrack.durationMs;
        }
    }

    // ------------------------------------------------------------------------
    // 3. Render OLED UI
    // ------------------------------------------------------------------------
    if (currentTrack.hasData) {
        String currentLyric = lyrics.getActiveLyric(estimatedProgressMs);
        display.renderPlayer(
            currentTrack.trackName,
            currentTrack.artistName,
            currentLyric,
            estimatedProgressMs,
            currentTrack.durationMs,
            currentTrack.isPlaying,
            lyrics.hasLyrics()
        );
    } else {
        display.showStatusMessage("Spotify Idle", "Play music on your", "Spotify app!");
    }

    delay(100); // 10 FPS refresh rate for smooth display update
}
