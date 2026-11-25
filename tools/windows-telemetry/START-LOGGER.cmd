@echo off
cd /d "%~dp0"
where py >nul 2>nul
if errorlevel 1 (python receiver.py) else (py -3 receiver.py)
pause
