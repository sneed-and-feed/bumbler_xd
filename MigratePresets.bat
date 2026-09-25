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

rem 1. Attempt to launch silently via pythonw (avoids persistent command window)
where pythonw >nul 2>&1
if %ERRORLEVEL% equ 0 (
    start "" pythonw "%SCRIPT_PATH%" %*
    exit /b 0
)

rem 2. Fallback to standard python interpreter
where python >nul 2>&1
if %ERRORLEVEL% equ 0 (
    python "%SCRIPT_PATH%" %*
    exit /b %ERRORLEVEL%
)

rem 3. Fallback to Windows Python Launcher 'py'
where py >nul 2>&1
if %ERRORLEVEL% equ 0 (
    start "" py -3w "%SCRIPT_PATH%" %* 2>nul
    if %ERRORLEVEL% equ 0 exit /b 0
    py -3 "%SCRIPT_PATH%" %*
    exit /b %ERRORLEVEL%
)

rem 4. Error notification if no Python environment is detected
echo ===========================================================================
echo [ERROR] Python was not found in your system PATH.
echo ===========================================================================
echo Bumbler XD Preset Migrator requires Python 3.9+ with Tkinter support.
echo.
echo Please install Python from https://www.python.org/downloads/
echo Ensure the option "Add Python to PATH" is selected during installation.
echo.
pause
exit /b 1
