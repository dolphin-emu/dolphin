@echo off
setlocal
setlocal EnableDelayedExpansion

:: NOTE: Architecture is picked up from the command window, so we can't control that here :(
set COMPILERS=clang msvc
set BUILD_TYPES=debug relwithdebinfo

for %%c in (%COMPILERS%) do (
    for %%b in (%BUILD_TYPES%) do (
        call %~dp0\init.cmd -c %%c -g ninja -b %%b %*
        if !ERRORLEVEL! NEQ 0 ( goto :eof )
    )
)
