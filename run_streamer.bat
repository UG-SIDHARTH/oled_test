@echo off
title ESP32 Spotify Streamer
cd /d "%~dp0"
echo ========================================================
echo   ESP32 Spotify Live Streamer (Zero WiFi / 100%% Offline)
echo ========================================================
echo.
echo Make sure Arduino Serial Monitor is CLOSED so COM13 is free.
echo.
powershell -ExecutionPolicy Bypass -File ".\spotify_streamer.ps1" -Port "COM13"
pause
