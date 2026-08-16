# ==============================================================================
# ESP32 Spotify Live Streamer + Automatic Real-Time Timeline & Synced Lyrics
# ==============================================================================
# Uses Windows System Media Controls (GSMTC) for 100% AUTOMATIC live timeline sync!
# Any scrub, skip, pause, or song change syncs instantly with ZERO manual input!
# ==============================================================================

param (
    [string]$Port = "COM13",
    [int]$BaudRate = 115200
)

[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12

Write-Host "==========================================================" -ForegroundColor Cyan
Write-Host " 🎵 ESP32 Spotify Live Streamer (100% Automatic Live Sync)" -ForegroundColor Green
Write-Host "==========================================================" -ForegroundColor Cyan
Write-Host " Target Port: $($Port) at $($BaudRate) baud" -ForegroundColor Yellow

# Initialize WinRT Media Manager
Add-Type -AssemblyName System.Runtime.WindowsRuntime
[Windows.Media.Control.GlobalSystemMediaTransportControlsSessionManager, Windows.Media, ContentType = WindowsRuntime] | Out-Null

$asTaskGeneric = [System.WindowsRuntimeSystemExtensions].GetMethods() | Where-Object { $_.Name -eq 'AsTask' -and $_.GetParameters().Count -eq 1 -and $_.IsGenericMethod } | Select-Object -First 1
$asTaskMgr = $asTaskGeneric.MakeGenericMethod([Windows.Media.Control.GlobalSystemMediaTransportControlsSessionManager])
$asTaskProp = $asTaskGeneric.MakeGenericMethod([Windows.Media.Control.GlobalSystemMediaTransportControlsSessionMediaProperties])

$mgrTask = $asTaskMgr.Invoke($null, @([Windows.Media.Control.GlobalSystemMediaTransportControlsSessionManager]::RequestAsync()))
$mgrTask.Wait()
$mediaManager = $mgrTask.Result

try {
    $serial = New-Object System.IO.Ports.SerialPort $Port, $BaudRate, None, 8, One
    $serial.Open()
    Write-Host "[Connected] Successfully opened $($Port)!" -ForegroundColor Green
    Write-Host "[Running] Connected to Windows Media & Spotify! Play any song!`n" -ForegroundColor Cyan
} catch {
    Write-Host "[Error] Could not open $($Port). Details: $($_.Exception.Message)" -ForegroundColor Red
    Write-Host "Please ensure Arduino Serial Monitor is CLOSED so $($Port) is free." -ForegroundColor Yellow
    exit 1
}

$lastTrackKey = ""
$trackDurationMs = 180000
$lrcLines = @()
$syncOffsetMs = 0

function Clean-Title ($title) {
    $cleaned = $title -replace '\s*[\(\[](feat|ft|with|remix|remastered|live|official|deluxe|bonus|edit).*?[\)\]]', ''
    $cleaned = $cleaned -replace '\s*-\s*(Remastered|Live|Radio Edit|Deluxe|Single Version|Mono).*$', ''
    return $cleaned.Trim()
}

function Fetch-Lyrics ($trackName, $artistName) {
    $cleanTrack = Clean-Title $trackName
    $cleanArtist = Clean-Title $artistName

    # 1. Try exact match from LRCLIB
    try {
        $encodedTrack = [Uri]::EscapeDataString($cleanTrack)
        $encodedArtist = [Uri]::EscapeDataString($cleanArtist)
        $url = "https://lrclib.net/api/get?track_name=$encodedTrack&artist_name=$encodedArtist"
        
        $res = Invoke-RestMethod -Uri $url -Method Get -TimeoutSec 3 -Headers @{ "User-Agent" = "ESP32-Spotify-OLED/1.0" } -ErrorAction Stop
        if ($res -and $res.syncedLyrics) {
            $parsed = Parse-LRC $res.syncedLyrics
            if ($parsed.Count -gt 0) {
                return $parsed
            }
        }
    } catch {}

    # 2. Fallback to LRCLIB Search query
    try {
        $query = [Uri]::EscapeDataString("$cleanTrack $cleanArtist")
        $url = "https://lrclib.net/api/search?q=$query"
        $results = Invoke-RestMethod -Uri $url -Method Get -TimeoutSec 3 -Headers @{ "User-Agent" = "ESP32-Spotify-OLED/1.0" } -ErrorAction Stop
        if ($results -and $results.Count -gt 0) {
            foreach ($item in $results) {
                if ($item.syncedLyrics) {
                    $parsed = Parse-LRC $item.syncedLyrics
                    if ($parsed.Count -gt 0) {
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
        $track = ""
        $artist = ""
        $currentPosMs = 0
        $durationMs = 0
        $isPlaying = $false

        # Live Keyboard Micro-Calibration
        if ([Console]::KeyAvailable) {
            $keyInfo = [Console]::ReadKey($true)
            $keyChar = $keyInfo.KeyChar
            $key = $keyInfo.Key

            if ($keyChar -eq ']' -or $key -eq [ConsoleKey]::RightArrow) {
                $syncOffsetMs += 100
                Write-Host "`n[Calibrate] Lyrics +0.1s (Total: $([math]::Round($syncOffsetMs/1000, 2))s)" -ForegroundColor Magenta
            } elseif ($keyChar -eq '[' -or $key -eq [ConsoleKey]::LeftArrow) {
                $syncOffsetMs -= 100
                Write-Host "`n[Calibrate] Lyrics -0.1s (Total: $([math]::Round($syncOffsetMs/1000, 2))s)" -ForegroundColor Magenta
            } elseif ($keyChar -eq '+' -or $keyChar -eq '}') {
                $syncOffsetMs += 500
                Write-Host "`n[Calibrate] Lyrics +0.5s (Total: $([math]::Round($syncOffsetMs/1000, 2))s)" -ForegroundColor Magenta
            } elseif ($keyChar -eq '-' -or $keyChar -eq '{') {
                $syncOffsetMs -= 500
                Write-Host "`n[Calibrate] Lyrics -0.5s (Total: $([math]::Round($syncOffsetMs/1000, 2))s)" -ForegroundColor Magenta
            } elseif ($keyChar -eq '0' -or $key -eq [ConsoleKey]::R) {
                $syncOffsetMs = 0
                Write-Host "`n[Calibrate] Reset Offset to 0.0s" -ForegroundColor Magenta
            }
        }

        # 1. Query Windows Media Session (GSMTC) for 100% exact playback state
        if ($mediaManager) {
            try {
                $session = $mediaManager.GetCurrentSession()
                if ($session) {
                    $propOp = $session.TryGetMediaPropertiesAsync()
                    $pTask = $asTaskProp.Invoke($null, @($propOp))
                    $pTask.Wait()
                    $props = $pTask.Result

                    $timeline = $session.GetTimelineProperties()
                    $playback = $session.GetPlaybackInfo()

                    if ($props -and $props.Title -ne "") {
                        $track = $props.Title.Trim()
                        $artist = $props.Artist.Trim()
                        $currentPosMs = [int]($timeline.Position.TotalMilliseconds)
                        $durationMs = [int]($timeline.EndTime.TotalMilliseconds)
                        $isPlaying = ($playback.PlaybackStatus -eq [Windows.Media.Control.GlobalSystemMediaTransportControlsSessionPlaybackStatus]::Playing)
                    }
                }
            } catch {}
        }

        # 2. Fallback to process scanning if media session is idle
        if ($track -eq "") {
            $spotifyProcs = Get-Process Spotify -ErrorAction SilentlyContinue | Where-Object { $_.MainWindowTitle -ne "" }
            foreach ($p in $spotifyProcs) {
                $t = $p.MainWindowTitle.Trim()
                if ($t -ne "" -and $t -ne "Spotify" -and $t -ne "Spotify Premium" -and $t -ne "Spotify Free") {
                    if ($t -match '^(.*?)\s+-\s+(.*)$') {
                        $artist = $matches[1].Trim()
                        $track = $matches[2].Trim()
                    } else {
                        $track = $t
                    }
                    $isPlaying = $true
                    break
                }
            }
        }

        if ($track -ne "") {
            $trackKey = "$artist - $track"

            # Track changed
            if ($trackKey -ne $lastTrackKey) {
                $lastTrackKey = $trackKey
                Write-Host "`n[Now Playing ▶] $trackKey" -ForegroundColor Green

                Write-Host "[Lyrics] Fetching synced lyrics..." -ForegroundColor Yellow
                $lrcLines = Fetch-Lyrics $track $artist
                if ($lrcLines.Count -gt 0) {
                    Write-Host "[Lyrics] Loaded $($lrcLines.Count) synced lines!" -ForegroundColor Green
                } else {
                    Write-Host "[Lyrics] Instrumental / No lyrics found" -ForegroundColor DarkGray
                }
            }

            if ($durationMs -gt 0) {
                $trackDurationMs = $durationMs
            }

            $effectivePosMs = [Math]::Max(0, [int]($currentPosMs + $syncOffsetMs))

            # Find active and next lyric for exact calibrated playback position
            $activeLyric = ""
            $nextLyric = ""
            if ($lrcLines.Count -gt 0) {
                $activeIdx = -1
                for ($i = 0; $i -lt $lrcLines.Count; $i++) {
                    if ($lrcLines[$i].timeMs -le $effectivePosMs) {
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
                    $remSec = [Math]::Max(0, [int](($firstLyricMs - $effectivePosMs) / 1000))
                    $activeLyric = "Intro (${remSec}s)"
                    $nextLyric = $lrcLines[0].text
                }
            }

            # Send live packet to ESP32
            $payloadObj = [ordered]@{
                track = $track
                artist = $artist
                activeLyric = $activeLyric
                nextLyric = $nextLyric
                duration = $trackDurationMs
                progress = $effectivePosMs
                playing = $isPlaying
            }
            $jsonStr = $payloadObj | ConvertTo-Json -Compress
            $serial.WriteLine("$jsonStr`n")

            $mins = [Math]::Floor($currentPosMs / 60000)
            $secs = [Math]::Floor(($currentPosMs % 60000) / 1000)
            $timeStr = "{0:D2}:{1:D2}" -f [int]$mins, [int]$secs

            $durMins = [Math]::Floor($trackDurationMs / 60000)
            $durSecs = [Math]::Floor(($trackDurationMs % 60000) / 1000)
            $durStr = "{0:D2}:{1:D2}" -f [int]$durMins, [int]$durSecs

            $offsetSecStr = $([math]::Round($syncOffsetMs/1000, 1))
            Write-Host -NoNewline "`r[$timeStr / $durStr | Offset: ${offsetSecStr}s] > $activeLyric                                " -ForegroundColor Cyan

        } else {
            if ($lastTrackKey -ne "") {
                $lastTrackKey = ""
                Write-Host "`n[Paused ⏸] Spotify paused" -ForegroundColor Yellow
                $serial.WriteLine("PAUSE`n")
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
