@echo off
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0Install-Minimap.ps1" %*
if errorlevel 1 (
    echo Instalace se nepovedla. Zavri hru a zkontroluj zpravu vyse.
    pause
    exit /b 1
)
pause
