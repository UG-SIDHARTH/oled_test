#ifndef LYRICS_CLIENT_H
#define LYRICS_CLIENT_H

#include <Arduino.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "config.h"

struct LyricLine {
    uint32_t timestampMs = 0;
    String text;
};

class LyricsClient {
public:
    LyricsClient();

    // Fetches synced lyrics from LRCLIB API (lrclib.net)
    bool fetchSyncedLyrics(const String& trackName, const String& artistName, const String& albumName, uint32_t durationMs);

    // Returns active lyric line for current progress in milliseconds
    String getActiveLyric(uint32_t currentProgressMs);

    // Returns true if synced lyrics are loaded for current track
    bool hasLyrics() const { return _lineCount > 0; }

    // Clears stored lyrics
    void clearLyrics();

private:
    LyricLine _lyrics[MAX_LYRIC_LINES];
    size_t _lineCount = 0;

    void parseLRC(const String& lrcContent);
    String urlEncode(const String& str);
};

#endif // LYRICS_CLIENT_H
