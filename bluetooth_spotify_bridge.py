"""
===============================================================================
ESP32 Spotify Bluetooth Live Streamer (Windows PC)
===============================================================================
Reads whatever is currently playing on your Windows PC (Spotify Free, Premium,
YouTube, etc.) and streams the live song title, artist, and progress directly
over Bluetooth Serial to your ESP32 OLED display!

Requirements:
  pip install pyserial winsdk
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


async def get_active_media_info():
    """Gets currently playing song info from Windows Media Manager."""
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


def find_bluetooth_com_port():
    """Finds outgoing Bluetooth COM port or lets user choose."""
    ports = list(serial.tools.list_ports.comports())
    bt_ports = [p.device for p in ports if 'Bluetooth' in p.description or 'BTHENUM' in p.hwid or 'Standard Serial' in p.description]
    
    if bt_ports:
        return bt_ports[0]
    elif ports:
        return ports[0].device
    return None


async def main():
    print("=====================================================")
    print(" 🎵 ESP32 Spotify Bluetooth Streamer")
    print("=====================================================")

    # Select COM port
    ports = list(serial.tools.list_ports.comports())
    print("\nAvailable COM Ports:")
    for idx, p in enumerate(ports):
        print(f" [{idx + 1}] {p.device} - {p.description}")

    selected_port = None
    if len(ports) == 1:
        selected_port = ports[0].device
    elif len(ports) > 1:
        choice = input(f"\nSelect COM port (1-{len(ports)}) [Default 1]: ").strip()
        selected_idx = int(choice) - 1 if choice.isdigit() and 1 <= int(choice) <= len(ports) else 0
        selected_port = ports[selected_idx].device
    else:
        selected_port = input("\nEnter ESP32 Bluetooth COM port (e.g. COM4): ").strip()

    print(f"\n[Connecting] Opening {selected_port} at 115200 baud...")
    try:
        ser = serial.Serial(selected_port, 115200, timeout=1)
        time.sleep(2)
        print(f"[Connected] Successfully opened {selected_port}!")
    except Exception as e:
        print(f"[Error] Could not open {selected_port}: {e}")
        print("Tip: In Windows Settings -> Bluetooth & devices -> COM Ports, check which COM port is paired with ESP32-Spotify-OLED.")
        return

    print("\n[Running] Now listening to Windows / Spotify playback...")
    print("Play any song on Spotify, YouTube, or Apple Music!\n")

    last_sent_track = ""
    last_sent_time = 0

    while True:
        try:
            info = await get_active_media_info()
            now = time.time()

            if info and info.get("track"):
                track = info["track"]
                is_new_track = (track != last_sent_track)

                # Send update immediately on track change, or every 3 seconds for progress sync
                if is_new_track or (now - last_sent_time >= 3.0):
                    last_sent_track = track
                    last_sent_time = now

                    payload = json.dumps(info) + "\n"
                    ser.write(payload.encode('utf-8'))
                    
                    status_icon = "▶" if info.get("playing") else "⏸"
                    print(f"[Stream {status_icon}] {info.get('artist')} - {info.get('track')} ({info.get('progress') // 1000}s / {info.get('duration') // 1000}s)")

            await asyncio.sleep(1.0)

        except KeyboardInterrupt:
            print("\n[Stopped] Exiting...")
            break
        except Exception as e:
            await asyncio.sleep(1.5)

    ser.close()


if __name__ == "__main__":
    asyncio.run(main())
