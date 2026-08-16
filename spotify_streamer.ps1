# ==============================================================================
# ESP32 Spotify Live Streamer + Synced Lyrics & Dance Engine for Windows
# ==============================================================================
# Streams Spotify playback + live LRCLIB synced lyrics to ESP32 over COM13 / BT!
# ==============================================================================

param (
    [string]$Port = "COM13",
    [int]$BaudRate = 115200
)

[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12

Write-Host "==========================================================" -ForegroundColor Cyan
Write-Host " 🎵 ESP32 Spotify Live Streamer + Synced Lyrics" -ForegroundColor Green
Write-Host "==========================================================" -ForegroundColor Cyan
Write-Host " Target Port: $($Port) at $($BaudRate) baud" -ForegroundColor Yellow

try {
    $serial = New-Object System.IO.Ports.SerialPort $Port, $BaudRate, None, 8, One
    $serial.Open()
    Write-Host "[Connected] Successfully opened $($Port)!" -ForegroundColor Green
    Write-Host "[Running] Listening to Spotify... Play any song on Spotify!`n" -ForegroundColor Cyan
    Write-Host " [Controls] 'R': Reset 0:00 | 'F' / '+': +5s | 'B' / '-': -5s`n" -ForegroundColor DarkGray
} catch {
    Write-Host "[Error] Could not open $($Port). Details: $($_.Exception.Message)" -ForegroundColor Red
    Write-Host "Please ensure Arduino Serial Monitor is CLOSED so $($Port) is free." -ForegroundColor Yellow
    exit 1
}

$lastTrackKey = ""
$trackDurationMs = 240000
$lrcLines = @()
$accumulatedElapsedMs = 0
$lastTickTime = [DateTime]::UtcNow
$syncOffsetMs = 0

function Clean-Title ($title) {
    # Remove (feat. ...), [Remastered...], - Radio Edit, etc. for 100% accurate LRCLIB matches
    $cleaned = $title -replace '\s*[\(\[](feat|ft|with|remix|remastered|live|official|deluxe|bonus|edit).*?[\)\]]', ''
    $cleaned = $cleaned -replace '\s*-\s*(Remastered|Live|Radio Edit|Deluxe|Single Version|Mono).*$', ''
    return $cleaned.Trim()
}

function Fetch-Lyrics ($trackName, $artistName) {
    $cleanTrack = Clean-Title $trackName
    $cleanArtist = Clean-Title $artistName

    # 1. Try exact match
    try {
        $encodedTrack = [Uri]::EscapeDataString($cleanTrack)
        $encodedArtist = [Uri]::EscapeDataString($cleanArtist)
        $url = "https://lrclib.net/api/get?track_name=$encodedTrack&artist_name=$encodedArtist"
        
        $res = Invoke-RestMethod -Uri $url -Method Get -TimeoutSec 4 -Headers @{ "User-Agent" = "ESP32-Spotify-OLED/1.0" } -ErrorAction Stop
        if ($res -and $res.syncedLyrics) {
            $parsed = Parse-LRC $res.syncedLyrics
            if ($parsed.Count -gt 0) {
                if ($res.duration) { $script:trackDurationMs = [int]($res.duration * 1000) }
                return $parsed
            }
        }
    } catch {}

    # 2. Fallback to Search query
    try {
        $query = [Uri]::EscapeDataString("$cleanTrack $cleanArtist")
        $url = "https://lrclib.net/api/search?q=$query"
        $results = Invoke-RestMethod -Uri $url -Method Get -TimeoutSec 4 -Headers @{ "User-Agent" = "ESP32-Spotify-OLED/1.0" } -ErrorAction Stop
        if ($results -and $results.Count -gt 0) {
            foreach ($item in $results) {
                if ($item.syncedLyrics) {
                    $parsed = Parse-LRC $item.syncedLyrics
                    if ($parsed.Count -gt 0) {
                        if ($item.duration) { $script:trackDurationMs = [int]($item.duration * 1000) }
                        return $parsed
                    }
                }
            }
        }
    } catch {}

    return @()
}

function Parse-LRC ($lrcContent) {
    $parsed = @()
    $lines = $lrcContent -split "`n"
    foreach ($line in $lines) {
        if ($line -match '^\[(\d+):(\d+)\.(\d+)\](.*)$') {
            $mins = [int]$matches[1]
            $secs = [int]$matches[2]
            $msStr = $matches[3]
            if ($msStr.Length -eq 2) { $msStr += "0" }
            $ms = [int]$msStr
            $totalMs = ($mins * 60000) + ($secs * 1000) + $ms
            $text = $matches[4].Trim()
            if ($text.Length -gt 0) {
                $parsed += [PSCustomObject]@{ timeMs = $totalMs; text = $text }
            }
        }
    }
    return $parsed
}

try {
    while ($true) {
        $now = [DateTime]::UtcNow
        
        # Check for keyboard sync adjustments
        if ([Console]::KeyAvailable) {
            $key = [Console]::ReadKey($true).KeyChar
            if ($key -eq 'r' -or $key -eq 'R') {
                $accumulatedElapsedMs = 0
                $syncOffsetMs = 0
                Write-Host "`n[Sync] Reset progress to 0:00" -ForegroundColor Magenta
            } elseif ($key -eq '+' -or $key -eq 'f' -or $key -eq 'F') {
                $syncOffsetMs += 5000
                Write-Host "`n[Sync] Skipped +5s (Offset: +$($syncOffsetMs/1000)s)" -ForegroundColor Magenta
            } elseif ($key -eq '-' -or $key -eq 'b' -or $key -eq 'B') {
                $syncOffsetMs -= 5000
                Write-Host "`n[Sync] Rewound -5s (Offset: $($syncOffsetMs/1000)s)" -ForegroundColor Magenta
            }
        }

        $spotify = Get-Process Spotify -ErrorAction SilentlyContinue | Where-Object { $_.MainWindowTitle -ne "" -and $_.MainWindowTitle -ne "Spotify" -and $_.MainWindowTitle -ne "Spotify Premium" -and $_.MainWindowTitle -ne "Spotify Free" } | Select-Object -First 1

        if ($spotify) {
            $rawTitle = $spotify.MainWindowTitle.Trim()

            # Parse "Artist - Track" (Standard Spotify format)
            $artist = ""
            $track = $rawTitle
            if ($rawTitle -match '^(.*?)\s+-\s+(.*)$') {
                $artist = $matches[1].Trim()
                $track = $matches[2].Trim()
            }

            $trackKey = "$artist - $track"

            # Track changed
            if ($trackKey -ne $lastTrackKey) {
                $lastTrackKey = $trackKey
                $accumulatedElapsedMs = 0
                $syncOffsetMs = 0
                $lastTickTime = $now
                Write-Host "`n[Now Playing ▶] $trackKey" -ForegroundColor Green

                Write-Host "[Lyrics] Fetching synced lyrics from LRCLIB..." -ForegroundColor Yellow
                $lrcLines = Fetch-Lyrics $track $artist
                if ($lrcLines.Count -gt 0) {
                    Write-Host "[Lyrics] Successfully loaded $($lrcLines.Count) synced lines!" -ForegroundColor Green
                } else {
                    Write-Host "[Lyrics] Instrumental / No synced lyrics available" -ForegroundColor DarkGray
                }
            }

            # Accumulate elapsed playback time accurately
            $deltaMs = ($now - $lastTickTime).TotalMilliseconds
            $lastTickTime = $now
            $accumulatedElapsedMs += $deltaMs

            $effectiveTimeMs = [Math]::Max(0, [int]($accumulatedElapsedMs + $syncOffsetMs))

            # Find active and next lyric
            $activeLyric = ""
            $nextLyric = ""
            if ($lrcLines.Count -gt 0) {
                $activeIdx = -1
                for ($i = 0; $i -lt $lrcLines.Count; $i++) {
                    if ($lrcLines[$i].timeMs -le $effectiveTimeMs) {
                        $activeIdx = $i
                    } else {
                        break
                    }
                }

                if ($activeIdx -ge 0) {
                    $activeLyric = $lrcLines[$activeIdx].text
                    if ($activeIdx + 1 -lt $lrcLines.Count) {
                        $nextLyric = $lrcLines[$activeIdx + 1].text
                    }
                } else {
                    # Still in the intro section before vocals start
                    $firstLyricMs = $lrcLines[0].timeMs
                    $remSec = [Math]::Max(0, [int](($firstLyricMs - $effectiveTimeMs) / 1000))
                    $activeLyric = "Intro (${remSec}s)"
                    $nextLyric = $lrcLines[0].text
                }
            }

            # Send full JSON payload to ESP32
            $payloadObj = [ordered]@{
                track = $track
                artist = $artist
                activeLyric = $activeLyric
                nextLyric = $nextLyric
                duration = $trackDurationMs
                progress = $effectiveTimeMs
                playing = $true
            }
            $jsonStr = $payloadObj | ConvertTo-Json -Compress
            $serial.WriteLine("$jsonStr`n")

            $mins = [Math]::Floor($effectiveTimeMs / 60000)
            $secs = [Math]::Floor(($effectiveTimeMs % 60000) / 1000)
            $timeStr = "{0:D2}:{1:D2}" -f [int]$mins, [int]$secs

            $durMins = [Math]::Floor($trackDurationMs / 60000)
            $durSecs = [Math]::Floor(($trackDurationMs % 60000) / 1000)
            $durStr = "{0:D2}:{1:D2}" -f [int]$durMins, [int]$durSecs

            Write-Host -NoNewline "`r[$timeStr / $durStr] > $activeLyric                                " -ForegroundColor Cyan

        } else {
            $lastTickTime = $now
            if ($lastTrackKey -ne "") {
                $lastTrackKey = ""
                Write-Host "`n[Paused ⏸] Spotify paused" -ForegroundColor Yellow
                $serial.WriteLine("PAUSE`n")
            }
        }

        Start-Sleep -Milliseconds 300
    }
} finally {
    if ($serial -and $serial.IsOpen) {
        $serial.Close()
        Write-Host "`n[Closed] Port $($Port) closed." -ForegroundColor Cyan
    }
}
