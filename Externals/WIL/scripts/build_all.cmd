@echo off
setlocal
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

set COMPILERS=clang msvc
set BUILD_TYPES=debug release relwithdebinfo minsizerel

for %%c in (%COMPILERS%) do (
    for %%b in (%BUILD_TYPES%) do (
        call :build %%c %%b
        if !ERRORLEVEL! NEQ 0 ( goto :eof )
    )
)

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
set EXIT_CODE=%ERRORLEVEL%
popd
exit /B %EXIT_CODE%
