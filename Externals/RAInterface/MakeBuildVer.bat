@echo off
setlocal

set TARGET_FILE=%1
if "%TARGET_FILE%"=="" GOTO usage
set GIT_TAG=%2
if "%GIT_TAG%"=="" GOTO usage
set DEFINE_PREFIX=%3
if "%DEFINE_PREFIX%"=="" GOTO usage

goto get_parts

:usage
echo Usage: MakeBuildVer [TargetFile] [GitTag] [DefinePrefix]
exit

:get_parts

rem === Get the current branch ===
git rev-parse --abbrev-ref HEAD > Temp.txt
set /p ACTIVE_BRANCH=<Temp.txt

rem === Get the most recent tag matching our prefix ===
git describe --tags --match "%GIT_TAG%.*" > Temp.txt 2>&1
set /p ACTIVE_TAG=<Temp.txt
if "%ACTIVE_TAG:~0,5%" == "fatal" goto no_tag

rem === Get the number of commits since the tag and remove the hash PREFIX-COMMITS-HASH ===
for /f "tokens=1,2 delims=-" %%a in ("%ACTIVE_TAG%") do set ACTIVE_TAG=%%a&set VERSION_REVISION=%%b
if "%VERSION_REVISION%" == "" set VERSION_REVISION=0

rem === Extract the major/minor/patch version from the tag (append 0s if necessary) ===
for /f "tokens=1,2,3,4 delims=." %%a in ("%ACTIVE_TAG%.0.0") do set VERSION_MAJOR=%%b&set VERSION_MINOR=%%c&set VERSION_PATCH=%%d
for /f "tokens=1,* delims=." %%a in ("%ACTIVE_TAG%") do set VERSION_TAG=%%b

goto have_parts

:no_tag
set VERSION_TAG=NO TAG
set VERSION_MAJOR=0
set VERSION_MINOR=0
set VERSION_PATCH=0
set VERSION_REVISION=0

:have_parts

set BRANCH_INFO=%ACTIVE_BRANCH%

rem === Build the product version. If on a branch, include the branch name ===
set VERSION_PRODUCT=%VERSION_TAG%

if "%ACTIVE_BRANCH:~0,5%" == "alpha" (
    set /A "PRERELEASE_VERSION_MINOR=%VERSION_MINOR%+1"
) else if "%ACTIVE_BRANCH:~0,4%" == "beta" (
    set /A "PRERELEASE_VERSION_MINOR=%VERSION_MINOR%+1"
)

if not "%ACTIVE_BRANCH%" == "master" (
    if not "%PRERELEASE_VERSION_MINOR%" == "" (
        set VERSION_PRODUCT=%VERSION_MAJOR%.%PRERELEASE_VERSION_MINOR%-%ACTIVE_BRANCH%
    ) else if not "%ACTIVE_BRANCH:~-6%" == "-fixes" (
        set VERSION_PRODUCT=%VERSION_PRODUCT%-%ACTIVE_BRANCH%
    )
)

rem === If there are any local modifications, increment revision ===
git diff HEAD > Temp.txt
for /F "usebackq" %%A in ('"Temp.txt"') do set DIFF_FILE_SIZE=%%~zA
if %DIFF_FILE_SIZE% GTR 0 (
    set BRANCH_INFO=%BRANCH_INFO% [modified]
    set /A VERSION_REVISION=VERSION_REVISION+1
)

rem === Generate a new version file ===
@echo Branch: %BRANCH_INFO% (%VERSION_TAG%)

echo #define %DEFINE_PREFIX%_VERSION "%VERSION_MAJOR%.%VERSION_MINOR%.%VERSION_PATCH%.%VERSION_REVISION%" > Temp.txt
echo #define %DEFINE_PREFIX%_VERSION_SHORT "%VERSION_TAG%" >> Temp.txt
echo #define %DEFINE_PREFIX%_VERSION_MAJOR %VERSION_MAJOR% >> Temp.txt
echo #define %DEFINE_PREFIX%_VERSION_MINOR %VERSION_MINOR% >> Temp.txt
echo #define %DEFINE_PREFIX%_VERSION_PATCH %VERSION_PATCH% >> Temp.txt
echo #define %DEFINE_PREFIX%_VERSION_REVISION %VERSION_REVISION% >> Temp.txt
echo #define %DEFINE_PREFIX%_VERSION_PRODUCT "%VERSION_PRODUCT%" >> Temp.txt

rem === Update the existing file only if the new file differs (fc requires backslashes) ===
set TARGET_FILE=%TARGET_FILE:/=\%
if not exist "%TARGET_FILE%" (
    rem === File doesn't exist ===
    move Temp.txt "%TARGET_FILE%" > nul
) else (
    fc "%TARGET_FILE%" Temp.txt > nul
    if errorlevel 1 (
        rem === File has changed ===
        del "%TARGET_FILE%"
        move Temp.txt "%TARGET_FILE%" > nul
    ) else (
        rem === File has not changed ===
        del Temp.txt
    )
)
