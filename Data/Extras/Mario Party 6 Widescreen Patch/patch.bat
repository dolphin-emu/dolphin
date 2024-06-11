@echo off
setlocal enabledelayedexpansion

REM Initialize the iso_file variable
set "iso_file="

REM Get the first ISO file in the same directory as the script
for %%F in ("%~dp0*.iso") do (
    set "iso_file=%%~F"
    goto :found_iso
)

:not_found
echo.
echo No ISO files found in the same directory as the script.
echo Place the Mario Party 6 (USA) ISO in the script's directory and run it again.
echo.
pause
exit /b 0

:found_iso
echo.
echo Given "%iso_file%"
xdelta3.exe -d -s "%iso_file%" data.xdelta3 "Mario Party 6 [Widescreen].iso"

echo.
echo Press Enter to exit.
pause
exit /b 0