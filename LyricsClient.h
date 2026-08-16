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

    // Loads LRC text directly (e.g. for offline demo mode or pre-cached lyrics)
    void loadLrcContent(const String& lrcContent);

    // Returns active lyric line for current progress in milliseconds
    String getActiveLyric(uint32_t currentProgressMs);

    // Returns next upcoming lyric line for preview
    String getNextLyric(uint32_t currentProgressMs);

    // Returns index of active lyric line (-1 if before first line or empty)
    int getActiveLyricIndex(uint32_t currentProgressMs);

    // Returns lyric line by index
    String getLyricTextByIndex(int index);

    // Returns total count of synced lines loaded
    size_t getLineCount() const { return _lineCount; }

    // Returns true if synced lyrics are loaded for current track
    bool hasLyrics() const { return _lineCount > 0; }

    // Returns track duration in milliseconds if found from LRCLIB
    uint32_t getTrackDurationMs() const { return _trackDurationMs; }

    // Clears stored lyrics
    void clearLyrics();

private:
    LyricLine _lyrics[MAX_LYRIC_LINES];
    size_t _lineCount = 0;
    uint32_t _trackDurationMs = 0;

    void parseLRC(const String& lrcContent);
    String urlEncode(const String& str);
};

#endif // LYRICS_CLIENT_H
