@echo off
setlocal
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\Cargar ESP32.ps1" %*
set "result=%errorlevel%"
if "%~1"=="" pause
exit /b %result%
