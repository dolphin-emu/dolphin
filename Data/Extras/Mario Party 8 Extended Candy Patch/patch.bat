@echo off
setlocal enabledelayedexpansion

REM Initialize the iso_files variable
set "iso_files="

REM Get a list of ISO and WBFS files in the same directory as the script
for %%F in ("%~dp0*.iso") do (
    set "iso_files=!iso_files! \"%%~F\""
)
for %%F in ("%~dp0*.wbfs") do (
    set "iso_files=!iso_files! \"%%~F\""
)

REM Check if there are no ISO or WBFS files
if not defined iso_files (
    echo.
    echo No ISO or WBFS files found in the same directory as the script.
    echo Place the Mario Party 8 (USA) (Revision 2) ISO or WBFS in the script's directory and run it again.
    echo.
    pause
    exit /b 0
)

REM Iterate over each ISO or WBFS file and process it
for %%I in (!iso_files!) do (
    echo.
    echo Given %%~I
    wit extract %%~I --dest=temp
    IF exist "temp\DATA" (
        xcopy "..\mp8candy" "temp\DATA" /s /y /e
    ) ELSE (
        xcopy "..\mp8candy" "temp" /s /y /e
    )
    wit copy "temp" "..\Mario Party 8 (USA) [Extended Candy].wbfs"
    rmdir /s /q temp
    echo Press Enter to continue.
    pause
)

echo.
echo Press Enter to exit.
pause
exit /b 0