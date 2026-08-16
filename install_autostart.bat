@echo off
echo ==========================================================
echo  ESP32 Spotify OLED - Windows Auto-Start Installer
echo ==========================================================
echo.
echo Installing background streamer into your Windows Startup folder...
copy /Y "%~dp0start_silent.vbs" "%APPDATA%\Microsoft\Windows\Start Menu\Programs\Startup\ESP32_Spotify_OLED.vbs" >nul
echo.
echo [SUCCESS] Installed!
echo The streamer will now launch invisibly in the background
echo every time you start your PC. You will never need to open any file!
echo.
pause
