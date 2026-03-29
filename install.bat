@echo off
setlocal

:: Self-elevate to admin if not already
net session >nul 2>&1
if errorlevel 1 (
    powershell -Command "Start-Process -FilePath '%~f0' -Verb RunAs"
    exit /b
)

:: Convert to short 8.3 path to avoid JUCE juceaide crash on non-ASCII chars (e.g. umlauts)
for %%I in ("%~dp0.") do set "REPO_DIR=%%~sI"
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

set "VST3_SRC=%BUILD_DIR%\SnareRhythmGenerator_artefacts\Release\VST3\Snare Rhythm Generator.vst3"

if not exist "%VST3_SRC%" (
    echo ERROR: Could not find VST3 bundle at "%VST3_SRC%"
    pause & exit /b 1
)

echo.
echo Copying to "%VST3_DEST%"...
xcopy /e /i /y "%VST3_SRC%" "%VST3_DEST%\Snare Rhythm Generator.vst3\"
if errorlevel 1 (
    echo ERROR: Copy failed.
    pause & exit /b 1
)

echo.
echo Done! Restart your DAW and rescan plugins.
pause
