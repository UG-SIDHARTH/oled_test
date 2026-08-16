#include "LyricsClient.h"

LyricsClient::LyricsClient() {
    clearLyrics();
}

void LyricsClient::clearLyrics() {
    _lineCount = 0;
}

String LyricsClient::urlEncode(const String& str) {
    String encoded = "";
    char buf[4];
    for (size_t i = 0; i < str.length(); i++) {
        char c = str.charAt(i);
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            encoded += c;
        } else if (c == ' ') {
            encoded += "%20";
        } else {
            sprintf(buf, "%%%02X", (unsigned char)c);
            encoded += buf;
        }
    }
    return encoded;
}

void LyricsClient::loadLrcContent(const String& lrcContent) {
    parseLRC(lrcContent);
}

bool LyricsClient::fetchSyncedLyrics(const String& trackName, const String& artistName, const String& albumName, uint32_t durationMs) {
    clearLyrics();
    Serial.printf("[Lyrics] Fetching lyrics for '%s' by '%s'...\n", trackName.c_str(), artistName.c_str());

    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient http;
    uint32_t durationSeconds = durationMs / 1000;
    
    String url = "https://lrclib.net/api/get?track_name=" + urlEncode(trackName) +
                 "&artist_name=" + urlEncode(artistName);
    
    if (albumName.length() > 0) {
        url += "&album_name=" + urlEncode(albumName);
    }
    if (durationSeconds > 0) {
        url += "&duration=" + String(durationSeconds);
    }

    http.begin(client, url);
    int httpCode = http.GET();

    if (httpCode == 200) {
        String payload = http.getString();
        DynamicJsonDocument doc(16384);
        DeserializationError error = deserializeJson(doc, payload);

        if (!error) {
            String syncedLyrics = doc["syncedLyrics"].as<String>();
            if (syncedLyrics.length() > 0 && syncedLyrics != "null") {
                parseLRC(syncedLyrics);
                Serial.printf("[Lyrics] Successfully loaded %d synced lyric lines!\n", _lineCount);
                http.end();
                return true;
            } else {
                Serial.println("[Lyrics] Song found, but no synced lyrics available.");
            }
        } else {
            Serial.printf("[Lyrics] Failed to parse LRCLIB response JSON: %s\n", error.c_str());
        }
    } else {
        Serial.printf("[Lyrics] LRCLIB API returned HTTP code %d\n", httpCode);
    }

    http.end();
    return false;
}

void LyricsClient::parseLRC(const String& lrcContent) {
    clearLyrics();
    int startIdx = 0;
    int endIdx = 0;

    while (startIdx < lrcContent.length() && _lineCount < MAX_LYRIC_LINES) {
        endIdx = lrcContent.indexOf('\n', startIdx);
        if (endIdx == -1) endIdx = lrcContent.length();

        String line = lrcContent.substring(startIdx, endIdx);
        line.trim();

        // Check for timestamp tag pattern: [mm:ss.xx] or [mm:ss.xxx]
        int bracketOpen = line.indexOf('[');
        int bracketClose = line.indexOf(']');

        if (bracketOpen != -1 && bracketClose != -1 && bracketClose > bracketOpen) {
            String timeStr = line.substring(bracketOpen + 1, bracketClose);
            String textStr = line.substring(bracketClose + 1);
            textStr.trim();

            int colonIdx = timeStr.indexOf(':');
            int dotIdx = timeStr.indexOf('.');

            if (colonIdx != -1) {
                uint32_t mins = timeStr.substring(0, colonIdx).toInt();
                uint32_t secs = 0;
                uint32_t ms = 0;

                if (dotIdx != -1 && dotIdx > colonIdx) {
                    secs = timeStr.substring(colonIdx + 1, dotIdx).toInt();
                    String msStr = timeStr.substring(dotIdx + 1);
                    if (msStr.length() == 1) {
                        ms = msStr.toInt() * 100;
                    } else if (msStr.length() == 2) {
                        ms = msStr.toInt() * 10;
                    } else if (msStr.length() >= 3) {
                        ms = msStr.substring(0, 3).toInt();
                    }
                } else {
                    secs = timeStr.substring(colonIdx + 1).toInt();
                }

                uint32_t totalMs = (mins * 60 + secs) * 1000 + ms;
                
                // Only store lines with meaningful timestamp
                _lyrics[_lineCount].timestampMs = totalMs;
                _lyrics[_lineCount].text = textStr;
                _lineCount++;
            }
        }

        startIdx = endIdx + 1;
    }
}

int LyricsClient::getActiveLyricIndex(uint32_t currentProgressMs) {
    if (_lineCount == 0) return -1;
    
    int activeIdx = -1;
    for (size_t i = 0; i < _lineCount; i++) {
        if (_lyrics[i].timestampMs <= currentProgressMs) {
            activeIdx = (int)i;
        } else {
            break; // Sorted chronologically
        }
    }
    return activeIdx;
}

String LyricsClient::getActiveLyric(uint32_t currentProgressMs) {
    if (_lineCount == 0) return "No Synced Lyrics";

    int idx = getActiveLyricIndex(currentProgressMs);
    if (idx >= 0 && idx < (int)_lineCount) {
        String t = _lyrics[idx].text;
        return t.length() > 0 ? t : "♪ ♪ ♪";
    }
    return "...";
}

String LyricsClient::getNextLyric(uint32_t currentProgressMs) {
    if (_lineCount == 0) return "";

    int idx = getActiveLyricIndex(currentProgressMs);
    int nextIdx = idx + 1;
    if (nextIdx >= 0 && nextIdx < (int)_lineCount) {
        return _lyrics[nextIdx].text;
    }
    return "";
}

String LyricsClient::getLyricTextByIndex(int index) {
    if (index >= 0 && index < (int)_lineCount) {
        return _lyrics[index].text;
    }
    return "";
}
