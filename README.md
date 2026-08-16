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

1. **`Adafruit SSD1306`** by Adafruit
2. **`Adafruit GFX Library`** by Adafruit
3. **`ArduinoJson`** (v6.x or v7.x) by Benoit Blanchon

---

## 🚀 Quick Start Guide

### Option A: Quick Offline Demo (Instant Visualizer & Lyrics Test)
1. Open [`config.h`](file:///c:/Users/Lenovo/Downloads/oled/oled_test/config.h).
2. Set `DEMO_MODE` to `true`:
   ```cpp
   #define DEMO_MODE true
   ```
3. Flash [`oled_test.ino`](file:///c:/Users/Lenovo/Downloads/oled/oled_test/oled_test.ino) to your ESP32.
4. Watch *Coldplay - Viva La Vida* play live with synchronized scrolling lyrics, spinning vinyl record, and dancing audio spectrum visualizer!

---

### Option B: Live Spotify Sync Mode

#### Step 1: Spotify Developer App
1. Go to [Spotify Developer Dashboard](https://developer.spotify.com/dashboard) and log in.
2. Click **Create an App**.
3. Set **Redirect URI** to: `http://localhost:8888/callback`
4. Copy your **Client ID** and **Client Secret**.

#### Step 2: Get `SPOTIFY_REFRESH_TOKEN`
Run the included Python helper script in your terminal:
```bash
python get_refresh_token.py
```
- Enter your **Client ID** and **Client Secret**.
- Authorize your account in the browser.
- Copy the generated `SPOTIFY_REFRESH_TOKEN`.

#### Step 3: Configure `config.h`
Open [`config.h`](file:///c:/Users/Lenovo/Downloads/oled/oled_test/config.h) and set:
```cpp
#define DEMO_MODE false

#define WIFI_SSID       "Your_WiFi_SSID"
#define WIFI_PASSWORD   "Your_WiFi_Password"

#define SPOTIFY_CLIENT_ID     "your_client_id"
#define SPOTIFY_CLIENT_SECRET "your_client_secret"
#define SPOTIFY_REFRESH_TOKEN "your_refresh_token"
```

#### Step 4: Flash & Enjoy
1. Open [`oled_test.ino`](file:///c:/Users/Lenovo/Downloads/oled/oled_test/oled_test.ino) in Arduino IDE.
2. Select Board: **ESP32 Dev Module** and choose your COM Port.
3. Click **Upload**.
4. Play any track on your Spotify app (phone/desktop/web) — watch lyrics, progress, and animations sync live on the OLED display!

---

## 📄 License
MIT License
