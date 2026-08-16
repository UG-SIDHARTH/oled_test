#ifndef BLUETOOTH_MANAGER_H
#define BLUETOOTH_MANAGER_H

#include <Arduino.h>
#include "BluetoothSerial.h"
#include <ArduinoJson.h>

struct BTPlaybackState {
    bool connected;
    bool isPlaying;
    bool newTrackAvailable;
    String trackName;
    String artistName;
    String albumName;
    String activeLyric;
    String nextLyric;
    uint32_t durationMs;
    uint32_t progressMs;
    uint32_t lastProgressUpdateMs;
};

class BluetoothManager {
public:
    BluetoothManager();

    // Initializes Bluetooth Serial with device name
    bool begin(const char* deviceName = "ESP32-Spotify-OLED");

    // Periodic check & packet reading
    void update();

    // Connection & Playback State
    bool isConnected();
    bool isPlaying();
    bool hasNewTrack();
    void clearNewTrackFlag();

    // Track metadata & live lyrics
    String getTrackName();
    String getArtistName();
    String getAlbumName();
    String getActiveLyric();
    String getNextLyric();
    bool hasLyrics();
    uint32_t getDurationMs();
    uint32_t getProgressMs();

    // Sends acknowledgment / response back over Bluetooth
    void sendResponse(const String& msg);

private:
    BluetoothSerial _serialBT;
    BTPlaybackState _state;
    String _incomingLine;
    uint32_t _lastMessageTimeMs = 0;

    void processMessage(String msg);
};

#endif // BLUETOOTH_MANAGER_H
