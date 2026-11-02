@echo off
rem Gothic Trainer - sestaveni na tomto pocitaci (spusti build.ps1)
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0build.ps1"
if errorlevel 1 pause
