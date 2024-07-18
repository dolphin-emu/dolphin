@echo off

REM Initialize the iso_files variable
set "iso_files="

REM Get a list of ISO and WBFS files in the same directory as the script
for %%F in ("%~dp0*.iso") do (
    set "iso_files=!iso_files! "%%~F""
)
for %%F in ("%~dp0*.wbfs") do (
    set "iso_files=!iso_files! "%%~F""
)

REM Check if there are no ISO or WBFS files
if not defined iso_files (
    echo.
    echo No ISO or WBFS files found in the same directory as the script.
    echo Place the Mario Party 8 USA Revision 2 ISO or WBFS in the script's directory and run it again.
    echo.
    pause
    exit /b 0
)

REM Iterate over each ISO or WBFS file and process it
for %%F in (%iso_files%) do (
    echo.
    echo Given %%F

    "tools/wit" extract %%F --dest=temp

    REM Determine the destination directory
    set "dest_dir=temp"
    if exist "temp\DATA" (
        set "dest_dir=temp\DATA"
    )
    
    REM Copy files using xcopy
    xcopy "mp8motion" "!dest_dir!" /s /y /e

    "tools/wit" copy "temp" "..\Mario Party 8 (USA) [GameCube Controller v6].wbfs"
    rmdir /s /q temp
    echo Press Enter to continue.
    pause
)

echo.
echo Press Enter to exit.
pause
exit /b 0
