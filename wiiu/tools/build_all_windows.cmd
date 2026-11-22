@echo off
rem GPL-3.0-or-later. Windows starter for wiiu\tools\build_all.sh using the msys2 shell that devkitPro installs.
rem Usage:  wiiu\tools\build_all_windows.cmd [path-to-devkitPro]     (default C:\devkitPro)
setlocal
set "DKP=%~1"
if "%DKP%"=="" set "DKP=C:\devkitPro"
set "BASH=%DKP%\msys2\usr\bin\bash.exe"
if not exist "%BASH%" echo devkitPro msys2 bash not found at "%BASH%". Pass the devkitPro folder as the first argument.& exit /b 1
set "REPO=%~dp0..\.."
for %%I in ("%REPO%") do set "REPO=%%~fI"
set "CHERE_INVOKING=1"
"%BASH%" -lc "export DEVKITPRO=/opt/devkitpro; export DEVKITPPC=/opt/devkitpro/devkitPPC; cd \"$(cygpath -u '%REPO%')\" && bash wiiu/tools/build_all.sh"
echo.
echo Results: %REPO%\wiiu\out
endlocal
