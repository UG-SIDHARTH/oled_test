# ESP32 Spotify Synced Lyrics & OLED Animations (`oled_test`)

An ESP32 program that connects to WiFi, monitors your active playback on **Spotify**, fetches real-time synchronized timestamped lyrics from **LRCLIB**, and displays song titles, animated visualizers, progress bars, and scrolling synced lyrics on an **SSD1306 128x64 I2C OLED display**.

---

## ✨ Features & Display Visuals

- 🎤 **Synchronized Lyrics**: Real-time line-by-line LRC lyrics synced with milliseconds accuracy.
- 📜 **Smart Text Marquee**: Auto-scrolling text for long lyric lines (> 19 characters) and song titles.
- 🔮 **Next-Line Preview**: Previews the upcoming lyric line so you can sing along ahead of time.
- 📊 **Dynamic Audio Visualizer**: 6-band dynamic audio equalizer spectrum bars with realistic peak-hold drop physics.
- 💿 **Spinning Vinyl Record**: Vector-drawn rotating vinyl record with groove rings, center label, strobe spokes, and tonearm indicator during interludes and standby.
- 🎵 **Floating Musical Notes**: Animated floating music particles (♪ and ♫) drifting across the screen.
- 📼 **Retro Cassette Standby & Connecting Screens**: Animated cassette tape reels during WiFi connection and standby.
- ⚡ **Offline Demo Mode**: Test animations and lyrics immediately without configuring Spotify or WiFi!

---

## 🛠️ Hardware Requirements

1. **ESP32 Development Board** (NodeMCU-32S, ESP32 WROOM, or DevKit v1)
2. **SSD1306 0.96" OLED Display** (128x64 resolution, I2C interface)
3. **Breadboard & Jumper Wires**

### 🔌 Wiring Diagram

| SSD1306 OLED Pin | ESP32 GPIO Pin | Description |
| :--- | :--- | :--- |
| **VCC** | `3.3V` or `5V` | Power Supply |
| **GND** | `GND` | Ground |
| **SDA** | `GPIO 21` | I2C Data Line |
| **SCL** | `GPIO 22` | I2C Clock Line |

---

## 📦 Software & Library Dependencies

Open your **Arduino IDE**, go to **Library Manager** (`Ctrl+Shift+I` or `Cmd+Shift+I`), and install:

1. **`U8g2`** by Oliver (high-performance monochrome graphics library)
2. **`ArduinoJson`** (v6.x or v7.x) by Benoit Blanchon

---

## 🚀 Quick Start Guide

### Option A: 📱 Bluetooth Mode (Recommended - No Spotify Premium / Web API needed!)
1. Open [`config.h`](file:///c:/Users/Lenovo/Downloads/oled/oled_test/config.h) and set:
   ```cpp
   #define USE_BLUETOOTH_MODE  true
   #define WIFI_SSID           "Your_WiFi_SSID"
   #define WIFI_PASSWORD       "Your_WiFi_Password"
   ```
2. Upload [`oled_test.ino`](file:///c:/Users/Lenovo/Downloads/oled/oled_test/oled_test.ino) to your ESP32.
3. On your phone or PC, open **Bluetooth Settings** and pair with **`ESP32-Spotify-OLED`**.
4. Play any song on **Spotify (Free or Premium)**, **YouTube Music**, or **Apple Music**!
   - The ESP32 receives track metadata over Bluetooth and fetches real-time synced lyrics from LRCLIB over WiFi.

---

### Option B: ⚡ Offline Demo Mode (Instant Animation Preview)
1. Open [`config.h`](file:///c:/Users/Lenovo/Downloads/oled/oled_test/config.h) and set `DEMO_MODE true`.
2. Upload to ESP32 to preview *Coldplay - Viva La Vida* with lyrics, spinning vinyl, and visualizers.

---

### Option C: 🌐 Spotify Web API Mode (Requires Spotify Premium)
1. Set `USE_BLUETOOTH_MODE false` and `DEMO_MODE false` in [`config.h`](file:///c:/Users/Lenovo/Downloads/oled/oled_test/config.h).
2. Enter your `SPOTIFY_CLIENT_ID`, `SPOTIFY_CLIENT_SECRET`, and `SPOTIFY_REFRESH_TOKEN`.
3. Upload to ESP32.

---

## 📄 License
MIT License
