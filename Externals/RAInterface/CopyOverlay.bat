@echo off
set SOURCE_DIR=%~dp0
set TARGET_DIR=%1

rem === see if anything needs to be copied ===
xcopy /l /i /d /y "%SOURCE_DIR%\overlay" "%TARGET_DIR%\overlay\" > Temp.txt
set /p FILES=<Temp.txt
del Temp.txt

rem === if result was anything other than "0 Files(s)", do the copy ===
if not "%FILES%"=="0 File(s)" (
    echo Updating %TARGET_DIR%\overlay...
    xcopy /i /d /y /q "%SOURCE_DIR%\overlay" "%TARGET_DIR%\overlay\"
)
