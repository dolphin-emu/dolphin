@echo off

title Mario Party 4 - DX Patcher

echo = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = =
echo Mario Party 4 - DX Patcher!
echo How do you wish to export your game.
echo.
echo 1: RVZ (Recommended for Dolphin)
echo 2: ISO (Recommended for SwissGC)
echo 3: Exit Patcher
echo = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = =

choice /c 123 > NUL
if errorlevel 3 exit
if errorlevel 2 set PATCHER=ISO
if errorlevel 1 set PATCHER=RVZ

echo = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = =
echo Setting up patcher!
echo = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = =

rmdir /s /q tmp

set "iso_files="
for %%F in ("%~dp0*.iso") do (
    set "iso_files=!iso_files! !%%~F!"
)
for %%F in ("%~dp0*.rvz") do (
    set "iso_files=!iso_files! !%%~F!"
)

if not defined iso_files (
    cls
    echo = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = =
    echo No ISO or RVZ files found in the same directory as the script.
    echo Place the Mario Party 6 (USA^) ISO or RVZ in the script's directory and run it again.
    echo = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = =
    echo.
    echo Press any key to exit...
    pause
    exit /b 0
)

cls
mkdir tmp > NUL
for %%F in ("%~dp0*.rvz") do (
    echo = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = =
    echo Converting RVZ to ISO! This is needed to patch the game...
    echo = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = =
    "tools/dolphintool" convert -i "%%~F" -f "iso" -o "tmp/tmp.iso"

    cls
    echo = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = =
    echo Extracting Gamespace!  This may take awhile depending on computer speed...
    echo = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = =
    "tools/pyisotools" "tmp/tmp.iso" E "--dest=tmp/"

    cls
    echo = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = =
    echo Copying mod data!
    echo = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = =
    xcopy "store" "tmp\root\" /s /y /e

    cls
    echo = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = =
    echo Rebuilding! This may take awhile depending on computer speed...
    echo = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = =
    "tools/pyisotools" tmp/root/ B "--dest=../../game.iso"
    
    rmdir /s /q tmp
)

for %%F in ("%~dp0*.iso") do (
    cls
    echo = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = =
    echo Extracting Gamespace!  This may take awhile depending on computer speed...
    echo = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = =
    "tools/pyisotools" "%%~F" E "--dest=tmp/"

    cls
    echo = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = =
    echo Copying mod data!
    echo = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = =
    xcopy "store" "tmp\root\" /s /y /e

    cls
    echo = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = =
    echo Rebuilding! This may take awhile depending on computer speed...
    echo = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = =
    "tools/pyisotools" tmp/root/ B "--dest=../../game.iso"
    
    rmdir /s /q tmp
)

if "%PATCHER%"=="RVZ" (
    cls
    "tools/dolphintool" convert -i "game.iso" -o "Mario Party 4 (USA) [DX].rvz" -f "rvz" -b "131072" -c "zstd" -l "5"
    del "game.iso"
    echo = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = =
    echo Success! Your game is located in "Mario Party 4 (USA) [DX].rvz" 
    echo = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = =
)

if "%PATCHER%"=="ISO" (
    cls
    move "game.iso" "Mario Party 4 (USA) [DX].iso"
    echo = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = =
    echo Success! Your game is located in "Mario Party 4 (USA) [DX].iso" 
    echo = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = =
)
echo.
echo Press any key to exit...
pause > NUL
exit /b 0
