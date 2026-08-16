#ifndef LASTFM_CLIENT_H
#define LASTFM_CLIENT_H

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "SpotifyClient.h" // Reuses SpotifyTrackInfo struct

class LastFmClient {
public:
    LastFmClient(const char* apiKey, const char* username);

    // Queries Last.fm API to fetch the currently playing Spotify track
    bool getCurrentlyPlaying(SpotifyTrackInfo &trackInfo);

private:
    const char* _apiKey;
    const char* _username;
    String _lastKnownTrack;
    uint32_t _trackStartTimeMs;
};

#endif // LASTFM_CLIENT_H
