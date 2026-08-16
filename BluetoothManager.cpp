#include "BluetoothManager.h"

BluetoothManager::BluetoothManager() {
    _state.connected = false;
    _state.isPlaying = false;
    _state.newTrackAvailable = false;
    _state.trackName = "";
    _state.artistName = "";
    _state.albumName = "";
    _state.durationMs = 0;
    _state.progressMs = 0;
    _state.lastProgressUpdateMs = 0;
    _incomingLine = "";
}

bool BluetoothManager::begin(const char* deviceName) {
    Serial.printf("[Bluetooth] Starting Bluetooth Serial as '%s'...\n", deviceName);
    
    bool ok = _serialBT.begin(deviceName);
    if (ok) {
        Serial.println("[Bluetooth] SUCCESS! The Bluetooth device is broadcasting.");
        Serial.println("[Bluetooth] Search for '" + String(deviceName) + "' on your phone/PC!");
    } else {
        Serial.println("[Bluetooth] FAILED to start Bluetooth Serial!");
    }
    return ok;
}

void BluetoothManager::update() {
    // Check connection status
    bool hasClient = _serialBT.hasClient();
    if (hasClient != _state.connected) {
        _state.connected = hasClient;
        if (hasClient) {
            Serial.println("\n[Bluetooth] >>> Client Connected! <<<");
            _serialBT.println("CONNECTED: ESP32 Spotify OLED Player Ready");
        } else {
            Serial.println("\n[Bluetooth] Client Disconnected.");
        }
    }

    // Read incoming characters from Bluetooth
    while (_serialBT.available()) {
        char c = (char)_serialBT.read();
        if (c == '\n' || c == '\r') {
            if (_incomingLine.length() > 0) {
                processMessage(_incomingLine);
                _incomingLine = "";
            }
        } else {
            if (_incomingLine.length() < 512) {
                _incomingLine += c;
            }
        }
    }
}

void BluetoothManager::processMessage(String msg) {
    msg.trim();
    if (msg.length() == 0) return;

    Serial.printf("[Bluetooth Recv] %s\n", msg.c_str());

    // 1. JSON Payload: {"track":"Song","artist":"Artist","duration":240000,"progress":15000,"playing":true}
    if (msg.startsWith("{") && msg.endsWith("}")) {
        StaticJsonDocument<512> doc;
        DeserializationError error = deserializeJson(doc, msg);
        if (!error) {
            String newTrack = doc["track"] | doc["title"] | "";
            String newArtist = doc["artist"] | "";
            String newAlbum = doc["album"] | "";
            uint32_t newDuration = doc["duration"] | 0;
            uint32_t newProgress = doc["progress"] | 0;
            bool isPlaying = doc.containsKey("playing") ? doc["playing"].as<bool>() : true;

            if (newTrack.length() > 0 && newTrack != _state.trackName) {
                _state.trackName = newTrack;
                _state.artistName = newArtist;
                _state.albumName = newAlbum;
                _state.durationMs = newDuration;
                _state.progressMs = newProgress;
                _state.lastProgressUpdateMs = millis();
                _state.isPlaying = isPlaying;
                _state.newTrackAvailable = true;
                Serial.printf("[Bluetooth Track] %s - %s\n", newTrack.c_str(), newArtist.c_str());
            } else {
                _state.progressMs = newProgress;
                _state.lastProgressUpdateMs = millis();
                _state.isPlaying = isPlaying;
                if (newDuration > 0) _state.durationMs = newDuration;
            }

            _serialBT.println("{\"status\":\"ok\",\"track\":\"" + _state.trackName + "\"}");
            return;
        }
    }

    // 2. Play / Pause commands
    if (msg.equalsIgnoreCase("PAUSE") || msg.equalsIgnoreCase("STOP")) {
        _state.isPlaying = false;
        _serialBT.println("OK:PAUSED");
        return;
    }
    if (msg.equalsIgnoreCase("PLAY") || msg.equalsIgnoreCase("RESUME")) {
        _state.isPlaying = true;
        _state.lastProgressUpdateMs = millis();
        _serialBT.println("OK:PLAYING");
        return;
    }

    // 3. Plain Text Format: "Track Name - Artist Name" or "Artist - Track"
    int sepIdx = msg.indexOf(" - ");
    String track = msg;
    String artist = "";

    if (sepIdx > 0) {
        artist = msg.substring(0, sepIdx);
        track = msg.substring(sepIdx + 3);
        artist.trim();
        track.trim();
    }

    if (track.length() > 0 && track != _state.trackName) {
        _state.trackName = track;
        _state.artistName = artist;
        _state.albumName = "";
        _state.durationMs = 0;
        _state.progressMs = 0;
        _state.lastProgressUpdateMs = millis();
        _state.isPlaying = true;
        _state.newTrackAvailable = true;

        Serial.printf("[Bluetooth Track] %s - %s\n", track.c_str(), artist.c_str());
        _serialBT.println("OK:" + track);
    }
}

bool BluetoothManager::isConnected() {
    return _state.connected;
}

bool BluetoothManager::isPlaying() {
    return _state.isPlaying;
}

bool BluetoothManager::hasNewTrack() {
    return _state.newTrackAvailable;
}

void BluetoothManager::clearNewTrackFlag() {
    _state.newTrackAvailable = false;
}

String BluetoothManager::getTrackName() {
    return _state.trackName;
}

String BluetoothManager::getArtistName() {
    return _state.artistName;
}

String BluetoothManager::getAlbumName() {
    return _state.albumName;
}

uint32_t BluetoothManager::getDurationMs() {
    return _state.durationMs;
}

uint32_t BluetoothManager::getProgressMs() {
    uint32_t p = _state.progressMs;
    if (_state.isPlaying && _state.lastProgressUpdateMs > 0) {
        p += (millis() - _state.lastProgressUpdateMs);
        if (_state.durationMs > 0 && p > _state.durationMs) {
            p = _state.durationMs;
        }
    }
    return p;
}

void BluetoothManager::sendResponse(const String& msg) {
    if (_state.connected) {
        _serialBT.println(msg);
    }
}
