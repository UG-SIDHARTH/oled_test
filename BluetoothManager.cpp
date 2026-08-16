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

    // Initialize NVS flash memory (mandatory for Bluetooth key storage on ESP32)
    esp_err_t nvs_err = nvs_flash_init();
    if (nvs_err == ESP_ERR_NVS_NO_FREE_PAGES || nvs_err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_err = nvs_flash_init();
    }
    if (nvs_err != ESP_OK) {
        Serial.printf("[Bluetooth] NVS flash init failed: %s\n", esp_err_to_name(nvs_err));
        return false;
    }

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

    // Set device name on both DEV and GAP layers
    esp_bt_dev_set_device_name(deviceName);
    esp_bt_gap_set_device_name(deviceName);

    // Register GAP callback for pairing & authentication
    esp_bt_gap_register_callback(BluetoothManager::handleGAPEvent);

    // Set Class of Device (CoD) to Audio / Stereo Headphones (0x240404)
    // This is required for iOS (iPhone/iPad) and Windows to immediately recognize and list the device
    esp_bt_cod_t cod;
    cod.major = ESP_BT_COD_MAJOR_DEV_AV;
    cod.minor = 0x06; // 0x06 = Headphones / Stereo Audio Sink (fully recognized by iOS)
    cod.service = ESP_BT_COD_SRVC_AUDIO | ESP_BT_COD_SRVC_RENDERING;
    esp_bt_gap_set_cod(cod, ESP_BT_INIT_COD);

    // Set Simple Secure Pairing (SSP) to "Just Works" (no PIN required)
    esp_bt_sp_param_t param_type = ESP_BT_SP_IOCAP_MODE;
    esp_bt_io_cap_t iocap = ESP_BT_IO_CAP_NONE;
    esp_bt_gap_set_security_param(param_type, &iocap, sizeof(uint8_t));

    // Enable discoverability and connectability
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

    Serial.println("[Bluetooth] Broadcasting! Search for '" + String(deviceName) + "' on your phone/PC.");
    return true;
}

void BluetoothManager::handleGAPEvent(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param) {
    switch (event) {
        case ESP_BT_GAP_AUTH_CMPL_EVT: {
            if (param->auth_cmpl.stat == ESP_BT_STATUS_SUCCESS) {
                Serial.printf("[Bluetooth GAP] Authenticated with: %s\n", param->auth_cmpl.device_name);
            } else {
                Serial.printf("[Bluetooth GAP] Authentication failed, status: %d\n", param->auth_cmpl.stat);
            }
            break;
        }

        case ESP_BT_GAP_CFM_REQ_EVT: {
            // Confirm pairing without user confirmation (Just Works SSP)
            Serial.println("[Bluetooth GAP] Auto-confirming pairing request...");
            esp_bt_gap_ssp_confirm_reply(param->cfm_req.bda, true);
            break;
        }

        case ESP_BT_GAP_KEY_NOTIF_EVT: {
            Serial.printf("[Bluetooth GAP] Passkey: %d\n", param->key_notif.passkey);
            break;
        }

        case ESP_BT_GAP_KEY_REQ_EVT: {
            Serial.println("[Bluetooth GAP] Passkey requested");
            break;
        }

        default:
            break;
    }
}

void BluetoothManager::handleA2DData(const uint8_t *data, uint32_t len) {
    // Audio stream data received
}

void BluetoothManager::handleA2DEvent(esp_a2d_cb_event_t event, esp_a2d_cb_param_t *param) {
    if (!_instance) return;

    switch (event) {
        case ESP_A2D_CONNECTION_STATE_EVT: {
            uint8_t state = param->conn_stat.state;
            if (state == ESP_A2D_CONNECTION_STATE_CONNECTED) {
                Serial.println("\n[Bluetooth] >>> Phone/PC Connected! <<<");
                portENTER_CRITICAL(&_instance->_mux);
                _instance->_state.connected = true;
                portEXIT_CRITICAL(&_instance->_mux);

                // Request initial metadata & register notifications
                esp_avrc_ct_send_metadata_cmd(0, ESP_AVRC_MD_ATTR_TITLE | ESP_AVRC_MD_ATTR_ARTIST | ESP_AVRC_MD_ATTR_ALBUM | ESP_AVRC_MD_ATTR_PLAYING_TIME);
                esp_avrc_ct_send_register_notification_cmd(0, ESP_AVRC_RN_TRACK_CHANGE, 0);
                esp_avrc_ct_send_register_notification_cmd(0, ESP_AVRC_RN_PLAY_STATUS_CHANGE, 0);
                esp_avrc_ct_send_register_notification_cmd(0, ESP_AVRC_RN_PLAY_POS_CHANGED, 1);
            } else if (state == ESP_A2D_CONNECTION_STATE_DISCONNECTED) {
                Serial.println("\n[Bluetooth] Device Disconnected!");
                portENTER_CRITICAL(&_instance->_mux);
                _instance->_state.connected = false;
                _instance->_state.isPlaying = false;
                _instance->_state.trackName = "";
                _instance->_state.artistName = "";
                portEXIT_CRITICAL(&_instance->_mux);

                // Re-enable discoverable mode for new connections
                esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_GENERAL_DISCOVERABLE);
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
                    if (val.length() > 0 && val != _instance->_state.trackName) {
                        _instance->_state.trackName = val;
                        _instance->_state.newTrackAvailable = true;
                        _instance->_state.progressMs = 0;
                        _instance->_state.lastProgressUpdateMs = millis();
                        Serial.printf("[Bluetooth Song] Title: %s\n", val.c_str());
                    }
                } else if (attr == ESP_AVRC_MD_ATTR_ARTIST) {
                    _instance->_state.artistName = val;
                    Serial.printf("[Bluetooth Artist] %s\n", val.c_str());
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
            // Request metadata on track change
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
