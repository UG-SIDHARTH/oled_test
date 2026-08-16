# ESP32 OLED Companion Server & Web Dashboard

A self-hosted Node.js server and companion web application to host on your server PC. It manages Spotify OAuth authentication, fetches and synchronizes LRCLIB timestamped lyrics, broadcasts live SSE events, and includes a real-time **128x64 SSD1306 OLED hardware simulator**.

---

## 🚀 Quick Start (Running on Server PC)

### 1. Start the Server
In PowerShell or Command Prompt:

```bash
cd server
node server.js
```

The terminal will print your local and LAN URLs:
```text
====================================================================
 ✨ ESP32 OLED Companion Server & Web Dashboard is Running!
====================================================================
 Local Access:      http://localhost:3000
 Network Access:    http://192.168.1.15:3000
====================================================================
```

### 2. Open Web Dashboard
Open `http://localhost:3000` (or `http://<SERVER_IP>:3000` from any phone or laptop on your WiFi network).

---

## ✨ Web Dashboard Features

1. **Hardware OLED Simulator**:
   - Pixel-accurate 128x64 SSD1306 display canvas running at ~25 FPS.
   - Switchable phosphor themes: **White**, **Cyberpunk Cyan**, **Vintage Amber**, **Matrix Green**.
   - Live animations: 6-band dynamic equalizer visualizer, spinning vinyl record, floating music notes, marquee text scrolling.

2. **1-Click Spotify Login**:
   - Paste your **Client ID** & **Client Secret** and click **Login with Spotify** to authorize directly in the browser.

3. **Now Playing & Karaoke Lyrics**:
   - Real-time album artwork and playback progress.
   - Auto-scrolling karaoke view highlighting active and upcoming lyrics.

4. **ESP32 Config Generator**:
   - Click **Download config.h for ESP32** to instantly get an updated `config.h` with your tokens, WiFi, and server IP ready for flashing.

---

## 📡 REST & Streaming APIs for ESP32

Your ESP32 or any smart home device can query the local server directly:

| Endpoint | Method | Description |
| :--- | :--- | :--- |
| **`/api/status`** | `GET` | Lightweight JSON with song title, artist, progress, active lyric, next lyric, and visualizer state. |
| **`/api/lyrics`** | `GET` | Full parsed LRC timestamped lyrics array. |
| **`/api/events`** | `GET` | Server-Sent Events (SSE) stream for instant push updates. |
| **`/api/generate-config`** | `GET` | Download auto-generated `config.h`. |
| **`/api/demo`** | `POST` | Toggle offline demo simulation mode. |

---

## ⚙️ Running in Background (Auto-start on Windows)

To run the server continuously in the background on your Windows Server PC:

### Option A: Using PM2 (Recommended)
```bash
npm install -g pm2
pm2 start server.js --name "oled-server"
pm2 save
pm2 startup
```

### Option B: Using Windows Task Scheduler
Create a task that runs `node server.js` on Windows boot.
