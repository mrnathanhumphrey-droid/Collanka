@echo off
REM Collonka MCE Server launcher
REM Bridges the Collonka VST plugin (AI ASSIST tab) to the Anthropic API.
REM Requires the ANTHROPIC_API_KEY user env var (set once via setx).
REM Listens on http://127.0.0.1:9150 by default.
REM NOTE: only one MCE server can run at a time on this port. If BW BASS or
REM Collatzichord MCE is already running, close that window first.

cd /d "%~dp0"

if "%ANTHROPIC_API_KEY%"=="" (
    echo.
    echo ERROR: ANTHROPIC_API_KEY is not set.
    echo Set it once with:   setx ANTHROPIC_API_KEY "sk-ant-..."
    echo Then open a new shell and re-run this script.
    echo.
    pause
    exit /b 1
)

if not exist node_modules (
    echo node_modules missing, running npm install...
    call npm install
)

echo Starting Collonka MCE server on http://127.0.0.1:9150
echo Close this window to stop the server.
echo.
node server.js
