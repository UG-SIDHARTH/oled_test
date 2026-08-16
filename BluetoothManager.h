#ifndef BLUETOOTH_MANAGER_H
#define BLUETOOTH_MANAGER_H

#include <Arduino.h>
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_bt_device.h"
#include "esp_gap_bt_api.h"
#include "esp_a2dp_api.h"
#include "esp_avrc_api.h"

struct BTPlaybackState {
    bool connected;
    bool isPlaying;
    bool newTrackAvailable;
    String trackName;
    String artistName;
    String albumName;
    uint32_t durationMs;
    uint32_t progressMs;
    uint32_t lastProgressUpdateMs;
};

class BluetoothManager {
public:
    BluetoothManager();

    // Initializes Bluetooth Classic stack, A2DP Sink, and AVRCP Controller
    bool begin(const char* deviceName = "ESP32-Spotify-OLED");

    // Periodic check / update routine
    void update();

    // Connection & Playback State
    bool isConnected();
    bool isPlaying();
    bool hasNewTrack();
    void clearNewTrackFlag();

    // Track metadata
    String getTrackName();
    String getArtistName();
    String getAlbumName();
    uint32_t getDurationMs();
    uint32_t getProgressMs();

    // Internal callbacks & event dispatchers
    static void handleGAPEvent(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param);
    static void handleA2DEvent(esp_a2d_cb_event_t event, esp_a2d_cb_param_t *param);
    static void handleAVRCEvent(esp_avrc_ct_cb_event_t event, esp_avrc_ct_cb_param_t *param);
    static void handleA2DData(const uint8_t *data, uint32_t len);

private:
    static BluetoothManager* _instance;
    BTPlaybackState _state;
    portMUX_TYPE _mux;

    uint32_t _lastStatusPollMs;
    String _lastKnownTrack;
};

#endif // BLUETOOTH_MANAGER_H
