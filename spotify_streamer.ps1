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
    Write-Host "[Running] Listening to Spotify... Hit PLAY on any song!`n" -ForegroundColor Cyan
    Write-Host " [Sync Keys] '1'-'9': Jump to 1:00, 2:00 | 'F'/'+': +5s | 'B'/'-': -5s | 'R': 0:00`n" -ForegroundColor DarkGray
} catch {
    Write-Host "[Error] Could not open $($Port). Details: $($_.Exception.Message)" -ForegroundColor Red
    Write-Host "Please ensure Arduino Serial Monitor is CLOSED so $($Port) is free." -ForegroundColor Yellow
    exit 1
}

$lastTrackKey = ""
$trackDurationMs = 180000
$lrcLines = @()
$accumulatedElapsedMs = 0
$lastTickTime = [DateTime]::UtcNow
$syncOffsetMs = 0
$waitingCounter = 0

function Clean-Title ($title) {
    $cleaned = $title -replace '\s*[\(\[](feat|ft|with|remix|remastered|live|official|deluxe|bonus|edit).*?[\)\]]', ''
    $cleaned = $cleaned -replace '\s*-\s*(Remastered|Live|Radio Edit|Deluxe|Single Version|Mono).*$', ''
    return $cleaned.Trim()
}

function Get-Exact-Duration ($trackName, $artistName) {
    try {
        $cleanTrack = Clean-Title $trackName
        $query = [Uri]::EscapeDataString("$artistName $cleanTrack")
        $url = "https://itunes.apple.com/search?term=$query&entity=song&limit=1"
        $res = Invoke-RestMethod -Uri $url -Method Get -TimeoutSec 3 -ErrorAction Stop
        if ($res.results -and $res.results.Count -gt 0 -and $res.results[0].trackTimeMillis) {
            return [int]$res.results[0].trackTimeMillis
        }
    } catch {}
    return 180000
}

function Fetch-Lyrics ($trackName, $artistName) {
    $cleanTrack = Clean-Title $trackName
    $cleanArtist = Clean-Title $artistName

    # 1. Try exact match from LRCLIB
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

    # 2. Fallback to LRCLIB Search query
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

    # 3. If no lyrics, query Apple Music API for exact track duration
    $script:trackDurationMs = Get-Exact-Duration $trackName $artistName
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
            if ($key -eq 'r' -or $key -eq 'R' -or $key -eq '0') {
                $accumulatedElapsedMs = 0
                $syncOffsetMs = 0
                Write-Host "`n[Sync] Reset progress to 0:00" -ForegroundColor Magenta
            } elseif ($key -ge '1' -and $key -le '9') {
                $minNum = [int]("$key")
                $accumulatedElapsedMs = ($minNum * 60000)
                $syncOffsetMs = 0
                Write-Host "`n[Sync] Jumped to $($minNum):00" -ForegroundColor Magenta
            } elseif ($key -eq '+' -or $key -eq 'f' -or $key -eq 'F') {
                $syncOffsetMs += 5000
                Write-Host "`n[Sync] Skipped +5s (Offset: +$($syncOffsetMs/1000)s)" -ForegroundColor Magenta
            } elseif ($key -eq '-' -or $key -eq 'b' -or $key -eq 'B') {
                $syncOffsetMs -= 5000
                Write-Host "`n[Sync] Rewound -5s (Offset: $($syncOffsetMs/1000)s)" -ForegroundColor Magenta
            } elseif ($key -eq ']') {
                $syncOffsetMs += 1000
                Write-Host "`n[Sync] +1s" -ForegroundColor Magenta
            } elseif ($key -eq '[') {
                $syncOffsetMs -= 1000
                Write-Host "`n[Sync] -1s" -ForegroundColor Magenta
            }
        }

        # 1. Search for active Spotify desktop window
        $activeWindowText = ""
        $spotifyProcs = Get-Process Spotify -ErrorAction SilentlyContinue | Where-Object { $_.MainWindowTitle -ne "" }
        foreach ($p in $spotifyProcs) {
            $t = $p.MainWindowTitle.Trim()
            if ($t -ne "" -and $t -ne "Spotify" -and $t -ne "Spotify Premium" -and $t -ne "Spotify Free") {
                $activeWindowText = $t
                break
            }
        }

        # 2. Search browser windows if Spotify web is used
        if ($activeWindowText -eq "") {
            $webProcs = Get-Process chrome, msedge, brave, firefox -ErrorAction SilentlyContinue | Where-Object { $_.MainWindowTitle -match 'Spotify' -or $_.MainWindowTitle -match ' - ' }
            foreach ($p in $webProcs) {
                if ($p.MainWindowTitle -match '^(.*?)\s+-\s+(.*?)\s*[-|•]\s*Spotify') {
                    $activeWindowText = "$($matches[1]) - $($matches[2])"
                    break
                }
            }
        }

        if ($activeWindowText -ne "") {
            # Parse "Artist - Track" (Standard Spotify format)
            $artist = ""
            $track = $activeWindowText
            if ($activeWindowText -match '^(.*?)\s+-\s+(.*)$') {
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

                Write-Host "[Lyrics] Fetching metadata & lyrics..." -ForegroundColor Yellow
                $lrcLines = Fetch-Lyrics $track $artist
                if ($lrcLines.Count -gt 0) {
                    Write-Host "[Lyrics] Loaded $($lrcLines.Count) synced lines!" -ForegroundColor Green
                } else {
                    $durM = [Math]::Floor($trackDurationMs / 60000)
                    $durS = [Math]::Floor(($trackDurationMs % 60000) / 1000)
                    Write-Host "[Lyrics] Instrumental/No lyrics (Exact duration: {0:D2}:{1:D2})" -f [int]$durM, [int]$durS -ForegroundColor DarkGray
                }
            }

            # Accumulate elapsed playback time accurately
            $deltaMs = ($now - $lastTickTime).TotalMilliseconds
            $lastTickTime = $now
            $accumulatedElapsedMs += $deltaMs

            $effectiveTimeMs = [Math]::Max(0, [int]($accumulatedElapsedMs + $syncOffsetMs))
            if ($trackDurationMs -gt 0 -and $effectiveTimeMs -gt $trackDurationMs) {
                $effectiveTimeMs = $trackDurationMs
            }

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
            } else {
                $waitingCounter++
                if ($waitingCounter % 15 -eq 0) {
                    Write-Host -NoNewline "`r[Waiting] Spotify is paused or idle... Hit PLAY on any song in Spotify!   " -ForegroundColor DarkGray
                }
            }
        }

        Start-Sleep -Milliseconds 250
    }
} finally {
    if ($serial -and $serial.IsOpen) {
        $serial.Close()
        Write-Host "`n[Closed] Port $($Port) closed." -ForegroundColor Cyan
    }
}
