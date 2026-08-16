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
} catch {
    Write-Host "[Error] Could not open $($Port). Details: $($_.Exception.Message)" -ForegroundColor Red
    Write-Host "Please ensure Arduino Serial Monitor is CLOSED so $($Port) is free." -ForegroundColor Yellow
    exit 1
}

$lastTrackKey = ""
$trackStartTime = [DateTime]::UtcNow
$trackDurationMs = 240000
$lrcLines = @()

function Fetch-Lyrics ($trackName, $artistName) {
    try {
        $encodedTrack = [Uri]::EscapeDataString($trackName)
        $encodedArtist = [Uri]::EscapeDataString($artistName)
        $url = "https://lrclib.net/api/get?track_name=$encodedTrack&artist_name=$encodedArtist"
        
        $res = Invoke-RestMethod -Uri $url -Method Get -TimeoutSec 4 -ErrorAction Stop
        if ($res -and $res.syncedLyrics) {
            $parsed = @()
            $lines = $res.syncedLyrics -split "`n"
            foreach ($line in $lines) {
                if ($line -match '^\[(\d+):(\d+)\.(\d+)\](.*)$') {
                    $mins = [int]$matches[1]
                    $secs = [int]$matches[2]
                    $msStr = $matches[3]
                    if ($msStr.Length -eq 2) { $msStr += "0" }
                    $ms = [int]$msStr
                    $totalMs = ($mins * 60000) + ($secs * 1000) + $ms
                    $text = $matches[4].Trim()
                    $parsed += [PSCustomObject]@{ timeMs = $totalMs; text = $text }
                }
            }
            if ($res.duration) {
                $script:trackDurationMs = [int]($res.duration * 1000)
            }
            return $parsed
        }
    } catch {
        # Fallback if lyrics not found or network error
    }
    return @()
}

try {
    while ($true) {
        $spotify = Get-Process Spotify -ErrorAction SilentlyContinue | Where-Object { $_.MainWindowTitle -ne "" -and $_.MainWindowTitle -ne "Spotify" -and $_.MainWindowTitle -ne "Spotify Premium" -and $_.MainWindowTitle -ne "Spotify Free" } | Select-Object -First 1

        if ($spotify) {
            $rawTitle = $spotify.MainWindowTitle.Trim()

            # Parse "Artist - Track" or "Track - Artist"
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
                $trackStartTime = [DateTime]::UtcNow
                Write-Host "`n[Now Playing ▶] $trackKey" -ForegroundColor Green

                Write-Host "[Lyrics] Fetching synced lyrics from LRCLIB..." -ForegroundColor Yellow
                $lrcLines = Fetch-Lyrics $track $artist
                if ($lrcLines.Count -gt 0) {
                    Write-Host "[Lyrics] Loaded $($lrcLines.Count) synced lines!" -ForegroundColor Green
                } else {
                    Write-Host "[Lyrics] Instrumental / No lyrics found" -ForegroundColor DarkGray
                }
            }

            # Calculate progress
            $elapsedMs = [int](([DateTime]::UtcNow - $trackStartTime).TotalMilliseconds)

            # Find active and next lyric
            $activeLyric = ""
            $nextLyric = ""
            if ($lrcLines.Count -gt 0) {
                $activeIdx = -1
                for ($i = 0; $i -lt $lrcLines.Count; $i++) {
                    if ($lrcLines[$i].timeMs -le $elapsedMs) {
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
                    $activeLyric = "♪ Intro ♪"
                    if ($lrcLines.Count -gt 0) {
                        $nextLyric = $lrcLines[0].text
                    }
                }
            }

            # Send full JSON payload to ESP32
            $payloadObj = [ordered]@{
                track = $track
                artist = $artist
                activeLyric = $activeLyric
                nextLyric = $nextLyric
                duration = $trackDurationMs
                progress = $elapsedMs
                playing = $true
            }
            $jsonStr = $payloadObj | ConvertTo-Json -Compress
            $serial.WriteLine("$jsonStr`n")

            if ($activeLyric -ne "") {
                Write-Host -NoNewline "`r[Lyrics] > $activeLyric                                " -ForegroundColor Cyan
            }

        } else {
            if ($lastTrackKey -ne "") {
                $lastTrackKey = ""
                Write-Host "`n[Paused ⏸] Spotify paused" -ForegroundColor Yellow
                $serial.WriteLine("PAUSE`n")
            }
        }

        Start-Sleep -Milliseconds 500
    }
} finally {
    if ($serial -and $serial.IsOpen) {
        $serial.Close()
        Write-Host "`n[Closed] Port $($Port) closed." -ForegroundColor Cyan
    }
}
