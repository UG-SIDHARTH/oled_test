#ifndef CONFIG_H
#define CONFIG_H

// ============================================================================
// Display & Hardware Configuration
// ============================================================================
#define SCREEN_WIDTH        128     // OLED display width in pixels
#define SCREEN_HEIGHT       64      // OLED display height in pixels
#define OLED_RESET          -1      // Reset pin (-1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS      0x3C    // I2C address for SSD1306 (commonly 0x3C or 0x3D)
#define OLED_SDA            21      // ESP32 I2C SDA GPIO pin
#define OLED_SCL            22      // ESP32 I2C SCL GPIO pin

// ============================================================================
// Demo Mode Toggle (Set to true to preview animations & lyrics offline!)
// ============================================================================
#define DEMO_MODE           false   // Set to 'true' for offline animated demo with sample synced lyrics

// ============================================================================
// WiFi Configuration
// ============================================================================
#define WIFI_SSID           "YOUR_WIFI_SSID"
#define WIFI_PASSWORD       "YOUR_WIFI_PASSWORD"

// ============================================================================
// Spotify Developer Credentials
// ============================================================================
#define SPOTIFY_CLIENT_ID     "YOUR_SPOTIFY_CLIENT_ID"
#define SPOTIFY_CLIENT_SECRET "YOUR_SPOTIFY_CLIENT_SECRET"
#define SPOTIFY_REFRESH_TOKEN "YOUR_SPOTIFY_REFRESH_TOKEN"

// ============================================================================
// Timing & Limits
// ============================================================================
#define SPOTIFY_POLL_INTERVAL 3000   // Spotify API poll interval in ms (3000ms recommended)
#define MAX_LYRIC_LINES       300    // Maximum lines of synced lyrics to store in RAM
#define DISPLAY_FPS           25     // Display refresh rate (~40ms per frame for smooth animations)

#endif // CONFIG_H
