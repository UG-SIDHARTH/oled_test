"""
===============================================================================
ESP32 Spotify Bluetooth Streamer (Zero WiFi / 100% Offline)
===============================================================================
Streams whatever song is playing on your Windows PC (Spotify Desktop/Web,
YouTube, Apple Music, etc.) directly to your ESP32 OLED over Bluetooth Serial.

Setup:
  1. Pair your Windows PC with 'ESP32-Spotify-OLED' in Bluetooth settings.
  2. Run: pip install pyserial winsdk
  3. Run: python bluetooth_spotify_bridge.py
===============================================================================
"""

import sys
import time
import json
import asyncio

try:
    import serial
    import serial.tools.list_ports
except ImportError:
    print("[Error] 'pyserial' not found. Install it with: pip install pyserial")
    sys.exit(1)

try:
    from winsdk.windows.media.control import GlobalSystemMediaTransportControlsSessionManager as MediaManager
except ImportError:
    print("[Error] 'winsdk' not found. Install it with: pip install winsdk")
    sys.exit(1)


async def get_active_media():
    """Reads currently playing media directly from Windows Media Session."""
    sessions = await MediaManager.request_async()
    current_session = sessions.get_current_session()
    if not current_session:
        return None

    media_properties = await current_session.try_get_media_properties_async()
    timeline = current_session.get_timeline_properties()
    playback_info = current_session.get_playback_info()

    if not media_properties:
        return None

    title = media_properties.title
    artist = media_properties.artist
    album = media_properties.album_title

    duration_ms = int(timeline.end_time.total_seconds() * 1000) if timeline.end_time else 0
    progress_ms = int(timeline.position.total_seconds() * 1000) if timeline.position else 0
    is_playing = (playback_info.playback_status.value == 4) if playback_info else True

    return {
        "track": title,
        "artist": artist,
        "album": album,
        "duration": duration_ms,
        "progress": progress_ms,
        "playing": is_playing
    }


async def main():
    print("=========================================================")
    print(" 🎵 ESP32 Spotify Bluetooth Streamer (100% Offline)")
    print("=========================================================")

    ports = list(serial.tools.list_ports.comports())
    if not ports:
        print("[Error] No COM ports found. Pair your PC with 'ESP32-Spotify-OLED' first!")
        return

    print("\nAvailable COM Ports:")
    for idx, p in enumerate(ports):
        print(f" [{idx + 1}] {p.device} - {p.description}")

    if len(ports) == 1:
        selected_port = ports[0].device
    else:
        choice = input(f"\nSelect COM port (1-{len(ports)}) [Default 1]: ").strip()
        selected_idx = int(choice) - 1 if choice.isdigit() and 1 <= int(choice) <= len(ports) else 0
        selected_port = ports[selected_idx].device

    print(f"\n[Connecting] Opening {selected_port} at 115200 baud...")
    try:
        ser = serial.Serial(selected_port, 115200, timeout=1)
        time.sleep(1.5)
        print(f"[Connected] Successfully opened {selected_port}!")
    except Exception as e:
        print(f"[Error] Could not open {selected_port}: {e}")
        return

    print("\n[Streaming] Listening to Spotify / Media playback...")
    print("Play any song on Spotify! (Press Ctrl+C to stop)\n")

    last_track = ""
    last_send_time = 0

    while True:
        try:
            info = await get_active_media()
            now = time.time()

            if info and info.get("track"):
                track = info["track"]
                is_new = (track != last_track)

                # Send update immediately on track change, or every 3 seconds for progress
                if is_new or (now - last_send_time >= 3.0):
                    last_track = track
                    last_send_time = now

                    payload = json.dumps(info) + "\n"
                    ser.write(payload.encode('utf-8'))

                    icon = "▶" if info.get("playing") else "⏸"
                    print(f"[{icon}] {info.get('artist')} - {info.get('track')} ({info.get('progress') // 1000}s / {info.get('duration') // 1000}s)")

            await asyncio.sleep(1.0)
        except KeyboardInterrupt:
            print("\n[Exiting] Goodbye!")
            break
        except Exception:
            await asyncio.sleep(1.0)

    ser.close()


if __name__ == "__main__":
    asyncio.run(main())
