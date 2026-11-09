@echo off
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0Install-Trainer.ps1" %*
if errorlevel 1 (
    echo.
    echo Instalace se nepovedla. Zkontroluj zpravu vyse a zavri Gothic II.
    pause
    exit /b 1
)
echo.
pause
