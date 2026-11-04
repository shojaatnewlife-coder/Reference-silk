@echo off
setlocal EnableExtensions
title reference Silk - build and install

rem ---- 1. Run as administrator (needed to copy into the VST3 folder) ----
net session >nul 2>&1
if errorlevel 1 (
    echo Asking for administrator permission...
    powershell -NoProfile -Command "Start-Process -FilePath '%~f0' -Verb RunAs"
    exit /b
)

echo.
echo  =====================================================
echo     reference Silk  -  build and install
echo     This takes 5-15 minutes. Do not close this window.
echo     Please CLOSE your music software (DAW) first.
echo  =====================================================
echo.

rem ---- 2. Find CMake and Git (also when they are not on PATH) ----
where cmake >nul 2>&1 || if exist "%ProgramFiles%\CMake\bin\cmake.exe" set "PATH=%ProgramFiles%\CMake\bin;%PATH%"
where git   >nul 2>&1 || if exist "%ProgramFiles%\Git\cmd\git.exe" set "PATH=%ProgramFiles%\Git\cmd;%PATH%"
where cmake >nul 2>&1 || (
    echo [X] CMake was not found. Install it from https://cmake.org/download/
    echo     and choose "Add CMake to the system PATH" during setup.
    goto fail
)
where git >nul 2>&1 || (
    echo [X] Git was not found. Install it from https://git-scm.com/download/win
    goto fail
)

rem ---- 3. Copy the source to a short, simple path ----
set "WORK=C:\reference-silk"
echo [1/4] Copying files to %WORK% ...
robocopy "%~dp0." "%WORK%" /MIR /NFL /NDL /NJH /NJS /NP /XD build >nul
if errorlevel 8 goto fail
cd /d "%WORK%"

rem ---- 4. Configure (fresh build every time: no leftovers from an older version) ----
if exist "%WORK%\build" rmdir /s /q "%WORK%\build"
echo [2/4] Preparing a clean build (downloads JUCE, needs internet) ...
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
if errorlevel 1 (
    echo.
    echo [X] Visual Studio 2022 with "Desktop development with C++" is needed.
    echo     Open "Visual Studio Installer", click Modify, tick that item, Install.
    goto fail
)

rem ---- 5. Build ----
echo [3/4] Building the plugin ...
cmake --build build --config Release --parallel
if errorlevel 1 goto fail

rem ---- 6. Install ----
set "SRC=%WORK%\build\ReferenceSilk_artefacts\Release\VST3\reference Silk.vst3"
set "DST=%CommonProgramFiles%\VST3\reference Silk.vst3"
echo [4/4] Installing to %CommonProgramFiles%\VST3 ...
if not exist "%SRC%" goto fail
if exist "%DST%" rmdir /s /q "%DST%"
if exist "%DST%" (
    echo [X] The old plugin is still in use. Close your music software and run this file again.
    goto fail
)
robocopy "%SRC%" "%DST%" /E /NFL /NDL /NJH /NJS /NP >nul
if errorlevel 8 goto fail

echo.
echo  =====================================================
echo     DONE!  The plugin is installed.
echo     Open your DAW and rescan plugins:
echo     "reference Silk" by "reference"
echo  =====================================================
echo.
pause
exit /b 0

:fail
echo.
echo  [X] Something went wrong. Take a photo of this window
echo      and send it in the chat.
echo.
pause
exit /b 1
