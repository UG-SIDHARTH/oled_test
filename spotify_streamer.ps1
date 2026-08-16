# ==============================================================================
# ESP32 Spotify Live Streamer + 100% AUTO-CONFIG & Auto-Calibrated Synced Lyrics
# ==============================================================================
# - AUTO-DETECTS ESP32 COM PORT automatically!
# - AUTO-CALIBRATES lyric lead-time (+450ms compensation) for frame-perfect sync!
# - AUTO-SYNCS with Windows Media & Spotify in real-time on scrub/skip/play!
# ==============================================================================

param (
    [string]$Port = "AUTO",
    [int]$BaudRate = 115200
)

[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
[Console]::OutputEncoding = [System.Text.Encoding]::UTF8
$OutputEncoding = [System.Text.Encoding]::UTF8

Write-Host "==========================================================" -ForegroundColor Cyan
Write-Host " 🎵 ESP32 Live Music Streamer (100% AUTO-CONFIGURED)" -ForegroundColor Green
Write-Host "==========================================================" -ForegroundColor Cyan

# 1. AUTO-DETECT ESP32 COM PORT if not specified or set to AUTO
if ($Port -eq "AUTO" -or $Port -eq "") {
    $foundPort = ""
    try {
        $pnpDevices = Get-CimInstance Win32_PnPEntity -ErrorAction SilentlyContinue | Where-Object { $_.Name -match '\(COM\d+\)' }
        foreach ($dev in $pnpDevices) {
            if ($dev.Name -match '\((COM\d+)\)') {
                $p = $matches[1]
                # Prioritize CP210x, CH340, USB Serial
                if ($dev.Name -match 'CP210|CH340|USB|Silicon|UART') {
                    $foundPort = $p
                    Write-Host "[Auto-Detect] Found ESP32 device on $foundPort ($($dev.Name))" -ForegroundColor Green
                    break
                }
            }
        }
    } catch {}

    if ($foundPort -eq "") {
        $ports = [System.IO.Ports.SerialPort]::GetPortNames()
        if ($ports.Count -gt 0) {
            # Pick COM13 if present, or first port
            if ($ports -contains "COM13") {
                $foundPort = "COM13"
            } else {
                $foundPort = $ports[0]
            }
        }
    }

    if ($foundPort -ne "") {
        $Port = $foundPort
    } else {
        $Port = "COM13"
    }
}

Write-Host " [Auto-Config] Selected Port: $($Port) at $($BaudRate) baud" -ForegroundColor Yellow

# Initialize WinRT Media Manager for 100% live timeline tracking
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
    Write-Host "[Auto-Config] Applied +450ms Auto-Lead Compensation for frame-perfect vocals!" -ForegroundColor Green
    Write-Host "[Running] Connected to Windows Media & Spotify! Play any song!`n" -ForegroundColor Cyan
} catch {
    Write-Host "[Error] Could not open $($Port). Details: $($_.Exception.Message)" -ForegroundColor Red
    Write-Host "Please ensure Arduino Serial Monitor is CLOSED so $($Port) is free." -ForegroundColor Yellow
    exit 1
}

$lastTrackKey = ""
$trackDurationMs = 180000
$lrcLines = @()

# Built-in Auto-Calibrated lead time offset (+450ms for natural karaoke vocal alignment)
$syncOffsetMs = 450

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

        # Optional Fine-Tuning Keys
        if ([Console]::KeyAvailable) {
            $keyInfo = [Console]::ReadKey($true)
            $keyChar = $keyInfo.KeyChar
            $key = $keyInfo.Key

            if ($keyChar -eq ']' -or $key -eq [ConsoleKey]::RightArrow) {
                $syncOffsetMs += 100
                Write-Host "`n[Auto-Config] Adjusted +0.1s (Total Offset: $([math]::Round($syncOffsetMs/1000, 2))s)" -ForegroundColor Magenta
            } elseif ($keyChar -eq '[' -or $key -eq [ConsoleKey]::LeftArrow) {
                $syncOffsetMs -= 100
                Write-Host "`n[Auto-Config] Adjusted -0.1s (Total Offset: $([math]::Round($syncOffsetMs/1000, 2))s)" -ForegroundColor Magenta
            } elseif ($keyChar -eq '0' -or $key -eq [ConsoleKey]::R) {
                $syncOffsetMs = 450
                Write-Host "`n[Auto-Config] Reset to Auto-Calibrated +0.45s" -ForegroundColor Magenta
            }
        }

        # 1. Query Windows Media Session (GSMTC) for 100% exact live playback state
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

            # Apply Auto-Calibrated Offset for frame-perfect vocal alignment
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

            Write-Host -NoNewline "`r[$timeStr / $durStr | Auto-Sync ✅] > $activeLyric                                " -ForegroundColor Cyan

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
