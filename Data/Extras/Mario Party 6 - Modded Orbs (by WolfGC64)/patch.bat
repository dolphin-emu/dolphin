@echo off
title Mario Party 6 - Modded Orbs Patcher

set VERSION=24.02.08

echo = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = =
echo Mario Party 6 - Modded Orbs Patcher!
echo How do you wish to export your game.
echo.
echo 1: RVZ (Recommended for Dolphin)
echo 2: ISO (Recommended for SwissGC)
echo 3: Exit Patcher
echo = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = =

choice /c 123 > NUL
if errorlevel 1 set PATCHER=RVZ
if errorlevel 2 set PATCHER=ISO
if errorlevel 3 exit

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
    "tools/pyisotools" tmp/root/ B "--dest=../game.iso"
    goto end
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
    "tools/pyisotools" tmp/root/ B "--dest=../../tmp/game.iso"
    goto end
)

:end
if "%PATCHER%"=="RVZ" (
    cls
    "tools/dolphintool" convert -i "tmp/game.iso" -o "Mario Party 6 (USA) [Modded Orbs] (%VERSION%).rvz" -f "rvz" -b "131062" -c "zstd" -l "5"
    del "tmp\game.iso"
    cls
    echo = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = =
    echo Success! Your game is located in "Mario Party 6 (USA) [Modded Orbs] (%VERSION%).rvz" 
    echo = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = =
)

if "%PATCHER%"=="ISO" (
    cls
    move "tmp\game.iso" "Mario Party 6 (USA) [Modded Orbs] (%VERSION%).iso"
    cls
    echo = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = =
    echo Success! Your game is located in "Mario Party 6 (USA) [Modded Orbs] (%VERSION%).iso" 
    echo = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = =
)

rmdir /s /q tmp

echo.
echo Press any key to exit...
pause > NUL
exit /b 0
