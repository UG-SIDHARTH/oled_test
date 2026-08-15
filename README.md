# ESP32 Spotify Synced Lyrics Display on OLED (`oled_test`)

An ESP32 program that connects to WiFi, monitors your active playback on **Spotify**, fetches real-time synchronized timestamped lyrics from **LRCLIB**, and displays song titles, progress bars, and scrolling synced lyrics on an **SSD1306 128x64 I2C OLED display**.

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

Open your **Arduino IDE** (or PlatformIO), go to **Library Manager** (`Ctrl+Shift+I` or `Cmd+Shift+I`), and install the following libraries:

1. **`Adafruit SSD1306`** by Adafruit
2. **`Adafruit GFX Library`** by Adafruit
3. **`ArduinoJson`** (version 6.x or 7.x) by Benoit Blanchon

---

## 🚀 Quick Start Guide

### Step 1: Set up Spotify Developer App
1. Go to [Spotify Developer Dashboard](https://developer.spotify.com/dashboard) and log in.
2. Click **Create an App**.
3. Set **Redirect URI** to: `http://localhost:8888/callback`
4. Copy your **Client ID** and **Client Secret**.

### Step 2: Get Your `SPOTIFY_REFRESH_TOKEN`
Run the included Python helper script in your terminal:

```bash
python get_refresh_token.py
```
- Enter your **Client ID** and **Client Secret** when prompted.
- A browser tab will open for Spotify OAuth authorization.
- Once approved, the terminal will print your `SPOTIFY_REFRESH_TOKEN`.

### Step 3: Configure Credentials (`config.h`)
Open [`config.h`](file:///C:/Users/Lenovo/.gemini/antigravity-ide/scratch/oled_test/config.h) and fill in:

```cpp
#define WIFI_SSID       "Your_WiFi_SSID"
#define WIFI_PASSWORD   "Your_WiFi_Password"

#define SPOTIFY_CLIENT_ID     "your_client_id"
#define SPOTIFY_CLIENT_SECRET "your_client_secret"
#define SPOTIFY_REFRESH_TOKEN "your_refresh_token"
```

### Step 4: Hardware Test (Optional but Recommended)
Before flashing the main program, test your OLED hardware connections by flashing the test sketch located at:
[`examples/hardware_test/hardware_test.ino`](file:///C:/Users/Lenovo/.gemini/antigravity-ide/scratch/oled_test/examples/hardware_test/hardware_test.ino)

### Step 5: Flash `oled_test.ino`
1. Open [`oled_test.ino`](file:///C:/Users/Lenovo/.gemini/antigravity-ide/scratch/oled_test/oled_test.ino) in Arduino IDE.
2. Select Board: **ESP32 Dev Module** (or your specific board model).
3. Select your serial COM Port.
4. Click **Upload**.
5. Open Serial Monitor at **115200 baud**.
6. Play music on Spotify (Phone, PC, or Web Player) — watch synced lyrics display live on the OLED screen!

---

## 🎨 Display Features

- **Header**: Song Name + Artist Name with smooth auto-scroll.
- **Body**: Active line of timecoded synced lyrics automatically updated in real-time.
- **Footer**: Play/Pause icon indicator (`>` / `||`), animated progress bar, and elapsed time (`MM:SS`).

---

## 📄 License
MIT License
