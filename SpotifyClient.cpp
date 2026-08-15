#include "SpotifyClient.h"

SpotifyClient::SpotifyClient(const char* clientId, const char* clientSecret, const char* refreshToken)
    : _clientId(clientId), _clientSecret(clientSecret), _refreshToken(refreshToken) {
}

bool SpotifyClient::refreshAccessToken() {
    Serial.println("[Spotify] Refreshing access token...");
    
    WiFiClientSecure client;
    client.setInsecure(); // Skip certificate verification for SSL on ESP32

    HTTPClient http;
    http.begin(client, "https://accounts.spotify.com/api/token");
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");

    String body = "grant_type=refresh_token";
    body += "&refresh_token=" + String(_refreshToken);
    body += "&client_id=" + String(_clientId);
    body += "&client_secret=" + String(_clientSecret);

    int httpCode = http.POST(body);

    if (httpCode == HTTP_CODE_OK) {
        String payload = http.getString();
        DynamicJsonDocument doc(2048);
        DeserializationError error = deserializeJson(doc, payload);

        if (!error) {
            _accessToken = doc["access_token"].as<String>();
            int expiresIn = doc["expires_in"] | 3600;
            _tokenExpirationMs = expiresIn * 1000;
            _tokenObtainedTimeMs = millis();
            
            Serial.println("[Spotify] Access token refreshed successfully!");
            http.end();
            return true;
        } else {
            Serial.printf("[Spotify] JSON deserialization failed: %s\n", error.c_str());
        }
    } else {
        Serial.printf("[Spotify] Token refresh failed HTTP code: %d\n", httpCode);
        if (httpCode > 0) {
            Serial.println(http.getString());
        }
    }

    http.end();
    return false;
}

bool SpotifyClient::getCurrentlyPlaying(SpotifyTrackInfo &trackInfo) {
    // Check if token is missing or near expiration (5 minutes buffer)
    if (_accessToken.length() == 0 || (millis() - _tokenObtainedTimeMs > (_tokenExpirationMs - 300000))) {
        if (!refreshAccessToken()) {
            return false;
        }
    }

    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient http;
    http.begin(client, "https://api.spotify.com/v1/me/player/currently-playing");
    http.addHeader("Authorization", "Bearer " + _accessToken);

    int httpCode = http.GET();

    if (httpCode == 200) {
        String payload = http.getString();
        DynamicJsonDocument doc(4096);
        DeserializationError error = deserializeJson(doc, payload);

        if (!error) {
            trackInfo.isPlaying = doc["is_playing"] | false;
            trackInfo.progressMs = doc["progress_ms"] | 0;
            trackInfo.lastFetchTimeMs = millis();

            JsonObject item = doc["item"];
            if (!item.isNull()) {
                trackInfo.hasData = true;
                trackInfo.trackId = item["id"].as<String>();
                trackInfo.trackName = item["name"].as<String>();
                trackInfo.durationMs = item["duration_ms"] | 0;
                
                // Artist name
                if (item["artists"].is<JsonArray>() && item["artists"].size() > 0) {
                    trackInfo.artistName = item["artists"][0]["name"].as<String>();
                } else {
                    trackInfo.artistName = "Unknown Artist";
                }

                // Album name
                if (!item["album"].isNull()) {
                    trackInfo.albumName = item["album"]["name"].as<String>();
                } else {
                    trackInfo.albumName = "";
                }
            } else {
                trackInfo.hasData = false;
            }

            http.end();
            return true;
        } else {
            Serial.printf("[Spotify] Parse track JSON failed: %s\n", error.c_str());
        }
    } else if (httpCode == 204) {
        // 204 No Content -> Nothing currently playing
        trackInfo.isPlaying = false;
        trackInfo.hasData = false;
        http.end();
        return true;
    } else if (httpCode == 401) {
        // Token expired -> refresh token and retry next loop
        Serial.println("[Spotify] 401 Unauthorized - Refreshing token...");
        _accessToken = "";
        refreshAccessToken();
    } else {
        Serial.printf("[Spotify] HTTP Error on currently playing: %d\n", httpCode);
    }

    http.end();
    return false;
}
