#ifndef SPOTIFY_CLIENT_H
#define SPOTIFY_CLIENT_H

#include <Arduino.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

struct SpotifyTrackInfo {
    bool isPlaying = false;
    bool hasData = false;
    String trackId;
    String trackName;
    String artistName;
    String albumName;
    uint32_t progressMs = 0;
    uint32_t durationMs = 0;
    uint32_t lastFetchTimeMs = 0;
};

class SpotifyClient {
public:
    SpotifyClient(const char* clientId, const char* clientSecret, const char* refreshToken);
    
    // Authenticates and refreshes the access token
    bool refreshAccessToken();

    // Fetches current playback status from Spotify API
    bool getCurrentlyPlaying(SpotifyTrackInfo &trackInfo);

private:
    const char* _clientId;
    const char* _clientSecret;
    const char* _refreshToken;
    String _accessToken;
    uint32_t _tokenExpirationMs = 0;
    uint32_t _tokenObtainedTimeMs = 0;
};

#endif // SPOTIFY_CLIENT_H
