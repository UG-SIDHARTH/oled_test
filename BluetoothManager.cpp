#include "BluetoothManager.h"

BluetoothManager* BluetoothManager::_instance = nullptr;

BluetoothManager::BluetoothManager() {
    _instance = this;
    _mux = portMUX_INITIALIZER_UNLOCKED;
    _state.connected = false;
    _state.isPlaying = false;
    _state.newTrackAvailable = false;
    _state.trackName = "";
    _state.artistName = "";
    _state.albumName = "";
    _state.durationMs = 0;
    _state.progressMs = 0;
    _state.lastProgressUpdateMs = 0;
    _lastStatusPollMs = 0;
    _lastKnownTrack = "";
}

bool BluetoothManager::begin(const char* deviceName) {
    Serial.printf("[Bluetooth] Initializing Bluetooth A2DP & AVRCP (%s)...\n", deviceName);

    // Release BLE memory if unused to save RAM for Classic BT + WiFi
    esp_bt_controller_mem_release(ESP_BT_MODE_BLE);

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    esp_err_t err = esp_bt_controller_init(&bt_cfg);
    if (err != ESP_OK) {
        Serial.printf("[Bluetooth] Controller init failed: %s\n", esp_err_to_name(err));
        return false;
    }

    err = esp_bt_controller_enable(ESP_BT_MODE_CLASSIC_BT);
    if (err != ESP_OK) {
        Serial.printf("[Bluetooth] Controller enable failed: %s\n", esp_err_to_name(err));
        return false;
    }

    err = esp_bluedroid_init();
    if (err != ESP_OK) {
        Serial.printf("[Bluetooth] Bluedroid init failed: %s\n", esp_err_to_name(err));
        return false;
    }

    err = esp_bluedroid_enable();
    if (err != ESP_OK) {
        Serial.printf("[Bluetooth] Bluedroid enable failed: %s\n", esp_err_to_name(err));
        return false;
    }

    // Set device name and discoverable scan mode
    esp_bt_dev_set_device_name(deviceName);
    esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_GENERAL_DISCOVERABLE);

    // Initialize A2DP Sink
    esp_a2d_register_callback(BluetoothManager::handleA2DEvent);
    esp_a2d_sink_register_data_callback(BluetoothManager::handleA2DData);
    esp_a2d_sink_init();

    // Initialize AVRCP Controller
    esp_avrc_ct_init();
    esp_avrc_ct_register_callback(BluetoothManager::handleAVRCEvent);
    
    // Initialize AVRCP Target (to support standard remote volume/control responses)
    esp_avrc_tg_init();

    Serial.println("[Bluetooth] Ready! Pair your phone or PC to '" + String(deviceName) + "'");
    return true;
}

void BluetoothManager::handleA2DData(const uint8_t *data, uint32_t len) {
    // Audio stream data received - we don't output to DAC to keep playback on phone/speaker,
    // or audio can be forwarded to I2S if desired.
}

void BluetoothManager::handleA2DEvent(esp_a2d_cb_event_t event, esp_a2d_cb_param_t *param) {
    if (!_instance) return;

    switch (event) {
        case ESP_A2D_CONNECTION_STATE_EVT: {
            uint8_t state = param->conn_stat.state;
            if (state == ESP_A2D_CONNECTION_STATE_CONNECTED) {
                Serial.println("[Bluetooth] Device Connected!");
                portENTER_CRITICAL(&_instance->_mux);
                _instance->_state.connected = true;
                portEXIT_CRITICAL(&_instance->_mux);

                // Request initial metadata
                esp_avrc_ct_send_metadata_cmd(0, ESP_AVRC_MD_ATTR_TITLE | ESP_AVRC_MD_ATTR_ARTIST | ESP_AVRC_MD_ATTR_ALBUM | ESP_AVRC_MD_ATTR_PLAYING_TIME);
                esp_avrc_ct_send_register_notification_cmd(0, ESP_AVRC_RN_TRACK_CHANGE, 0);
                esp_avrc_ct_send_register_notification_cmd(0, ESP_AVRC_RN_PLAY_STATUS_CHANGE, 0);
                esp_avrc_ct_send_register_notification_cmd(0, ESP_AVRC_RN_PLAY_POS_CHANGED, 1);
            } else if (state == ESP_A2D_CONNECTION_STATE_DISCONNECTED) {
                Serial.println("[Bluetooth] Device Disconnected!");
                portENTER_CRITICAL(&_instance->_mux);
                _instance->_state.connected = false;
                _instance->_state.isPlaying = false;
                portEXIT_CRITICAL(&_instance->_mux);
            }
            break;
        }

        case ESP_A2D_AUDIO_STATE_EVT: {
            uint8_t state = param->audio_stat.state;
            portENTER_CRITICAL(&_instance->_mux);
            if (state == ESP_A2D_AUDIO_STATE_STARTED) {
                _instance->_state.isPlaying = true;
                _instance->_state.lastProgressUpdateMs = millis();
            } else {
                _instance->_state.isPlaying = false;
            }
            portEXIT_CRITICAL(&_instance->_mux);

            // Re-request metadata when playback starts
            if (state == ESP_A2D_AUDIO_STATE_STARTED) {
                esp_avrc_ct_send_metadata_cmd(0, ESP_AVRC_MD_ATTR_TITLE | ESP_AVRC_MD_ATTR_ARTIST | ESP_AVRC_MD_ATTR_ALBUM | ESP_AVRC_MD_ATTR_PLAYING_TIME);
            }
            break;
        }

        default:
            break;
    }
}

void BluetoothManager::handleAVRCEvent(esp_avrc_ct_cb_event_t event, esp_avrc_ct_cb_param_t *param) {
    if (!_instance) return;

    switch (event) {
        case ESP_AVRC_CT_METADATA_RSP_EVT: {
            uint8_t attr = param->meta_rsp.attr_id;
            uint8_t *text = param->meta_rsp.attr_text;
            uint16_t len = param->meta_rsp.attr_length;

            if (text && len > 0) {
                char buffer[256];
                uint16_t copyLen = len < sizeof(buffer) - 1 ? len : sizeof(buffer) - 1;
                memcpy(buffer, text, copyLen);
                buffer[copyLen] = '\0';
                String val = String(buffer);
                val.trim();

                portENTER_CRITICAL(&_instance->_mux);
                if (attr == ESP_AVRC_MD_ATTR_TITLE) {
                    if (val != _instance->_state.trackName) {
                        _instance->_state.trackName = val;
                        _instance->_state.newTrackAvailable = true;
                        _instance->_state.progressMs = 0;
                        _instance->_state.lastProgressUpdateMs = millis();
                        Serial.printf("[Bluetooth] Title: %s\n", val.c_str());
                    }
                } else if (attr == ESP_AVRC_MD_ATTR_ARTIST) {
                    _instance->_state.artistName = val;
                    Serial.printf("[Bluetooth] Artist: %s\n", val.c_str());
                } else if (attr == ESP_AVRC_MD_ATTR_ALBUM) {
                    _instance->_state.albumName = val;
                } else if (attr == ESP_AVRC_MD_ATTR_PLAYING_TIME) {
                    _instance->_state.durationMs = val.toInt();
                }
                portEXIT_CRITICAL(&_instance->_mux);
            }
            break;
        }

        case ESP_AVRC_CT_CHANGE_NOTIFY_EVT:
        case ESP_AVRC_CT_REMOTE_FEATURES_EVT: {
            // Request metadata and status notifications
            esp_avrc_ct_send_metadata_cmd(0, ESP_AVRC_MD_ATTR_TITLE | ESP_AVRC_MD_ATTR_ARTIST | ESP_AVRC_MD_ATTR_ALBUM | ESP_AVRC_MD_ATTR_PLAYING_TIME);
            esp_avrc_ct_send_register_notification_cmd(0, ESP_AVRC_RN_TRACK_CHANGE, 0);
            esp_avrc_ct_send_register_notification_cmd(0, ESP_AVRC_RN_PLAY_STATUS_CHANGE, 0);
            esp_avrc_ct_send_register_notification_cmd(0, ESP_AVRC_RN_PLAY_POS_CHANGED, 1);
            break;
        }

        default:
            break;
    }
}

void BluetoothManager::update() {
    uint32_t now = millis();
    if (_state.connected && (now - _lastStatusPollMs >= 3000 || _lastStatusPollMs == 0)) {
        _lastStatusPollMs = now;
        // Poll metadata in case of silent track change
        esp_avrc_ct_send_metadata_cmd(0, ESP_AVRC_MD_ATTR_TITLE | ESP_AVRC_MD_ATTR_ARTIST | ESP_AVRC_MD_ATTR_ALBUM | ESP_AVRC_MD_ATTR_PLAYING_TIME);
    }
}

bool BluetoothManager::isConnected() {
    portENTER_CRITICAL(&_mux);
    bool c = _state.connected;
    portEXIT_CRITICAL(&_mux);
    return c;
}

bool BluetoothManager::isPlaying() {
    portENTER_CRITICAL(&_mux);
    bool p = _state.isPlaying;
    portEXIT_CRITICAL(&_mux);
    return p;
}

bool BluetoothManager::hasNewTrack() {
    portENTER_CRITICAL(&_mux);
    bool n = _state.newTrackAvailable;
    portEXIT_CRITICAL(&_mux);
    return n;
}

void BluetoothManager::clearNewTrackFlag() {
    portENTER_CRITICAL(&_mux);
    _state.newTrackAvailable = false;
    portEXIT_CRITICAL(&_mux);
}

String BluetoothManager::getTrackName() {
    portENTER_CRITICAL(&_mux);
    String t = _state.trackName;
    portEXIT_CRITICAL(&_mux);
    return t;
}

String BluetoothManager::getArtistName() {
    portENTER_CRITICAL(&_mux);
    String a = _state.artistName;
    portEXIT_CRITICAL(&_mux);
    return a;
}

String BluetoothManager::getAlbumName() {
    portENTER_CRITICAL(&_mux);
    String al = _state.albumName;
    portEXIT_CRITICAL(&_mux);
    return al;
}

uint32_t BluetoothManager::getDurationMs() {
    portENTER_CRITICAL(&_mux);
    uint32_t d = _state.durationMs;
    portEXIT_CRITICAL(&_mux);
    return d;
}

uint32_t BluetoothManager::getProgressMs() {
    portENTER_CRITICAL(&_mux);
    uint32_t p = _state.progressMs;
    if (_state.isPlaying && _state.lastProgressUpdateMs > 0) {
        p += (millis() - _state.lastProgressUpdateMs);
        if (_state.durationMs > 0 && p > _state.durationMs) {
            p = _state.durationMs;
        }
    }
    portEXIT_CRITICAL(&_mux);
    return p;
}
