@echo off
setlocal

:: Self-elevate to admin if not already
net session >nul 2>&1
if errorlevel 1 (
    powershell -Command "Start-Process -FilePath '%~f0' -Verb RunAs"
    exit /b
)

:: Strip trailing backslash from %~dp0 to avoid quote escape issues
set "REPO_DIR=%~dp0"
if "%REPO_DIR:~-1%"=="\" set "REPO_DIR=%REPO_DIR:~0,-1%"
set "BUILD_DIR=%REPO_DIR%\build"
set "VST3_DEST=C:\Program Files\Common Files\VST3"

echo === Snare Rhythm Generator - Build and Install ===
echo.

cmake -B "%BUILD_DIR%" -S "%REPO_DIR%"
if errorlevel 1 (
    echo ERROR: CMake configuration failed.
    pause & exit /b 1
)

cmake --build "%BUILD_DIR%" --config Release
if errorlevel 1 (
    echo ERROR: Build failed.
    pause & exit /b 1
)

for /r "%BUILD_DIR%" %%f in (SnareRhythmGenerator.vst3) do set "VST3_SRC=%%f"

if not defined VST3_SRC (
    echo ERROR: Could not find SnareRhythmGenerator.vst3 in build output.
    pause & exit /b 1
)

echo.
echo Copying to "%VST3_DEST%"...
xcopy /e /i /y "%VST3_SRC%" "%VST3_DEST%\SnareRhythmGenerator.vst3\"
if errorlevel 1 (
    echo ERROR: Copy failed.
    pause & exit /b 1
)

echo.
echo Done! Restart your DAW and rescan plugins.
pause
