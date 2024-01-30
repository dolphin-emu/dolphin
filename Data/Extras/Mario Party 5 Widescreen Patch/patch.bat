@echo off
setlocal enabledelayedexpansion

REM Get a list of ISO files in the same directory as the script
for %%F in ("%~dp0*.iso") do (
    set "iso_files=!iso_files! "%%F""
)

REM Check if there are no ISO files
if not defined iso_files (
    echo.
    echo No ISO files found in the same directory as the script.
    echo Place the Mario Party 5 (USA) ISO in the script's directory and run it again.
    echo.
    pause
    exit /b 0
)

REM Iterate over each ISO file and process it
for %%I in (%iso_files%) do (
    echo.
    echo Given "%%~I"
    xdelta3.exe -d -s "%%~I" data.xdelta3 "Mario Party 5 [Widescreen].iso"
    echo Press Enter to exit.
    set /p "=<" NUL
)

echo.
pause
exit /b 0