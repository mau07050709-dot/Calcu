@echo off
setlocal
cd /d "%~dp0" || goto :error
if not exist "%~dp0scripts\Compilar Calcu.ps1" (
    if exist "%~dp0Calcu.exe" goto :launch
    echo Falta el archivo scripts\Compilar Calcu.ps1.
    goto :error
)
where powershell.exe >nul 2>nul
if errorlevel 1 (
    if exist "%~dp0Calcu.exe" goto :launch
    echo Falta Windows PowerShell para compilar Calcu.
    goto :error
)
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\Compilar Calcu.ps1"
if errorlevel 1 goto :error
:launch
if not exist "%~dp0Calcu.exe" goto :error
start "" "%~dp0Calcu.exe"
if errorlevel 1 goto :error
exit /b 0
:error
echo No se pudo abrir Calcu. Consulta README.md para mas detalles.
pause
exit /b 1
