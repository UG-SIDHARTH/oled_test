@echo off
echo Stopping background ESP32 Spotify Streamer...
powershell -Command "Get-CimInstance Win32_Process | Where-Object { $_.CommandLine -like '*spotify_streamer.ps1*' } | ForEach-Object { Stop-Process -Id $_.ProcessId -Force }"
echo ESP32 Spotify Streamer has been stopped.
timeout /t 2 >nul
