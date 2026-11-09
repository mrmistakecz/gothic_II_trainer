@echo off
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0Install-Trainer.ps1" -Minimap %*
if errorlevel 1 (
    echo Instalace se nepovedla. Zavri Gothic II a zkontroluj zpravu vyse.
    pause
    exit /b 1
)
pause
