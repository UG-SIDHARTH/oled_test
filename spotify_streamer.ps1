# ==============================================================================
# ESP32 Spotify Live Streamer for Windows (Zero Installation Required!)
# ==============================================================================
# Opens COM13 and streams whatever song is playing on Spotify directly to ESP32!
# ==============================================================================

param (
    [string]$Port = "COM13",
    [int]$BaudRate = 115200
)

Write-Host "==========================================================" -ForegroundColor Cyan
Write-Host " 🎵 ESP32 Spotify Live Streamer (Zero WiFi / 100% Offline)" -ForegroundColor Green
Write-Host "==========================================================" -ForegroundColor Cyan
Write-Host " Target Port: $Port at $BaudRate baud" -ForegroundColor Yellow

try {
    $serial = New-Object System.IO.Ports.SerialPort $Port, $BaudRate, None, 8, One
    $serial.Open()
    Write-Host "[Connected] Successfully opened $Port!" -ForegroundColor Green
    Write-Host "[Running] Listening to Spotify on Windows... Play any song!`n" -ForegroundColor Cyan
} catch {
    Write-Host "[Error] Could not open $Port: $_" -ForegroundColor Red
    Write-Host "Please ensure Arduino Serial Monitor is CLOSED so $Port is free." -ForegroundColor Yellow
    exit 1
}

$lastTitle = ""

try {
    while ($true) {
        $spotify = Get-Process Spotify -ErrorAction SilentlyContinue | Where-Object { $_.MainWindowTitle -ne "" -and $_.MainWindowTitle -ne "Spotify" -and $_.MainWindowTitle -ne "Spotify Premium" -and $_.MainWindowTitle -ne "Spotify Free" } | Select-Object -First 1

        if ($spotify) {
            $currentTitle = $spotify.MainWindowTitle.Trim()
            
            if ($currentTitle -ne "" -and $currentTitle -ne $lastTitle) {
                $lastTitle = $currentTitle
                Write-Host "[Playing ▶] $currentTitle" -ForegroundColor Green

                # Send track to ESP32
                $serial.WriteLine("$currentTitle`n")
            }
        } else {
            if ($lastTitle -ne "") {
                $lastTitle = ""
                Write-Host "[Paused ⏸] Spotify paused" -ForegroundColor Yellow
                $serial.WriteLine("PAUSE`n")
            }
        }

        Start-Sleep -Milliseconds 1000
    }
} finally {
    if ($serial.IsOpen) {
        $serial.Close()
        Write-Host "`n[Closed] Port $Port closed." -ForegroundColor Cyan
    }
}
