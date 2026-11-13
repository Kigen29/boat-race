@echo off
rem Builds the Windows copy of Boat Race. Double-click this on a Windows PC
rem that has Unreal Engine 5.9 and Visual Studio 2022 (C++ game workload).
rem The result is dist\BoatRace-Windows.zip, ready to send to friends.
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0package_windows.ps1" %*
echo.
pause
