@echo off
rem ===========================================================================
rem Bumbler XD -- Legacy Preset Migrator Desktop Launcher
rem ===========================================================================
setlocal EnableDelayedExpansion

rem Navigate to the directory of this batch file (Bumbler XD project root)
cd /d "%~dp0"

set "SCRIPT_PATH=scripts\migrator_gui.py"

if not exist "%SCRIPT_PATH%" (
    echo [ERROR] Preset migrator script not found: "%SCRIPT_PATH%"
    echo Please ensure MigratePresets.bat is placed in the Bumbler XD root directory.
    echo.
    pause
    exit /b 1
)

rem 1. Prefer the official Windows Python Launcher ('py') after verifying Tkinter
rem    (bypasses Microsoft Store WindowsApps python/pythonw execution alias stubs)
py -3 -c "import tkinter" >nul 2>&1
if %ERRORLEVEL% equ 0 (
    py -3w -c "import sys" >nul 2>&1
    if !ERRORLEVEL! equ 0 (
        start "" py -3w "%SCRIPT_PATH%" %*
        exit /b 0
    )
    py -3 "%SCRIPT_PATH%" %*
    exit /b !ERRORLEVEL!
)

rem 2. Fallback to PATH python/pythonw ONLY after verifying a real interpreter + Tkinter
python -c "import tkinter" >nul 2>&1
if %ERRORLEVEL% equ 0 (
    pythonw -c "import tkinter" >nul 2>&1
    if !ERRORLEVEL! equ 0 (
        start "" pythonw "%SCRIPT_PATH%" %*
        exit /b 0
    )
    python "%SCRIPT_PATH%" %*
    exit /b !ERRORLEVEL!
)

rem 3. Error notification if no usable Python + Tkinter environment is detected
echo ===========================================================================
echo [ERROR] Python 3.9+ with Tkinter was not found on your system.
echo ===========================================================================
echo Bumbler XD Preset Migrator requires a real Python 3.9+ installation with
echo Tcl/Tk (Tkinter) GUI support enabled.
echo.
echo Please install Python from https://www.python.org/downloads/
echo Ensure "tcl/tk and IDLE" and "py launcher" (or "Add Python to PATH") are selected.
echo.
pause
exit /b 1
