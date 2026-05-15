@echo off
rem Launches dcn_server.exe in the same directory as this script.
rem Usage:
rem   start_server.bat            (defaults to port 9001)
rem   start_server.bat 9100        (custom port)

setlocal
set "SCRIPT_DIR=%~dp0"
set "PORT=%~1"
if "%PORT%"=="" set "PORT=9001"

pushd "%SCRIPT_DIR%"
if not exist "dcn_server.exe" (
    echo [ERROR] dcn_server.exe not found in %SCRIPT_DIR%
    popd
    pause
    exit /b 1
)

echo [*] Starting dcn_server on port %PORT%
echo [*] Press Ctrl+C to shut down gracefully.
echo.
"dcn_server.exe" %PORT%
set "EXIT_CODE=%ERRORLEVEL%"
popd

if not "%EXIT_CODE%"=="0" (
    echo.
    echo [!] Server exited with code %EXIT_CODE%
    pause
)
endlocal & exit /b %EXIT_CODE%
