@echo off
setlocal EnableDelayedExpansion

set BUILD_ROOT=%~dp0\..\build

if "%Platform%"=="x64" (
    set BUILD_ARCH=64
) else if "%Platform%"=="x86" (
    set BUILD_ARCH=32
) else if [%Platform%]==[] (
    echo ERROR: The build_all.cmd script must be run from a Visual Studio command window
    exit /B 1
) else (
    echo ERROR: Unrecognized/unsupported platform %Platform%
    exit /B 1
)

call :build clang debug
if %ERRORLEVEL% NEQ 0 ( goto :eof )
call :build clang release
if %ERRORLEVEL% NEQ 0 ( goto :eof )
call :build clang relwithdebinfo
if %ERRORLEVEL% NEQ 0 ( goto :eof )
call :build clang minsizerel
if %ERRORLEVEL% NEQ 0 ( goto :eof )

call :build msvc debug
if %ERRORLEVEL% NEQ 0 ( goto :eof )
call :build msvc release
if %ERRORLEVEL% NEQ 0 ( goto :eof )
call :build msvc relwithdebinfo
if %ERRORLEVEL% NEQ 0 ( goto :eof )
call :build msvc minsizerel
if %ERRORLEVEL% NEQ 0 ( goto :eof )

echo All build completed successfully!

goto :eof

:: build [compiler] [type]
:build
set BUILD_DIR=%BUILD_ROOT%\%1%BUILD_ARCH%%2
if not exist %BUILD_DIR% (
    goto :eof
)

pushd %BUILD_DIR%
echo Building from %CD%
ninja
popd
goto :eof
