#include "LastFmClient.h"

LastFmClient::LastFmClient(const char* apiKey, const char* username) {
    _apiKey = apiKey;
    _username = username;
    _lastKnownTrack = "";
    _trackStartTimeMs = 0;
}

bool LastFmClient::getCurrentlyPlaying(SpotifyTrackInfo &trackInfo) {
    if (WiFi.status() != WL_CONNECTED) {
        return false;
    }

    HTTPClient http;
    // URL encode username if needed
    String url = "http://ws.audioscrobbler.com/2.0/?method=user.getrecenttracks&user=" + String(_username) + 
                 "&api_key=" + String(_apiKey) + "&format=json&limit=1";

    http.begin(url);
    http.setTimeout(4000);

    int httpCode = http.GET();
    if (httpCode != 200) {
        Serial.printf("[Last.fm] HTTP Error: %d\n", httpCode);
        http.end();
        return false;
    }

    String payload = http.getString();
    http.end();

    DynamicJsonDocument doc(4096);
    DeserializationError error = deserializeJson(doc, payload);
    if (error) {
        Serial.printf("[Last.fm] JSON Parse error: %s\n", error.c_str());
        return false;
    }

    JsonObject trackObj;
    if (doc["recenttracks"]["track"].is<JsonArray>()) {
        if (doc["recenttracks"]["track"].size() == 0) return false;
        trackObj = doc["recenttracks"]["track"][0];
    } else if (doc["recenttracks"]["track"].is<JsonObject>()) {
        trackObj = doc["recenttracks"]["track"].as<JsonObject>();
    } else {
        return false;
    }

    String trackName = trackObj["name"] | "";
    String artistName = trackObj["artist"]["#text"] | trackObj["artist"]["name"] | "";
    String albumName = trackObj["album"]["#text"] | "";

    bool isNowPlaying = false;
    if (trackObj.containsKey("@attr")) {
        String np = trackObj["@attr"]["nowplaying"] | "false";
        isNowPlaying = (np == "true");
    }

    if (trackName.length() == 0) {
        return false;
    }

    trackInfo.trackName = trackName;
    trackInfo.artistName = artistName;
    trackInfo.albumName = albumName;
    trackInfo.isPlaying = isNowPlaying;
    trackInfo.hasData = true;
    trackInfo.trackId = trackName + " - " + artistName;

    // Track progress interpolation
    if (trackInfo.trackId != _lastKnownTrack) {
        _lastKnownTrack = trackInfo.trackId;
        _trackStartTimeMs = millis();
        trackInfo.progressMs = 0;
    } else {
        if (isNowPlaying) {
            trackInfo.progressMs = millis() - _trackStartTimeMs;
        }
    }

    trackInfo.lastFetchTimeMs = millis();
    return true;
}
