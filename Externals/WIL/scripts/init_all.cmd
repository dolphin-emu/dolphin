@echo off

:: NOTE: Architecture is picked up from the command window, so we can't control that here :(

call %~dp0\init.cmd -c clang -g ninja -b debug %*
if %ERRORLEVEL% NEQ 0 ( goto :eof )
call %~dp0\init.cmd -c clang -g ninja -b relwithdebinfo %*
if %ERRORLEVEL% NEQ 0 ( goto :eof )

call %~dp0\init.cmd -c msvc -g ninja -b debug %*
if %ERRORLEVEL% NEQ 0 ( goto :eof )
call %~dp0\init.cmd -c msvc -g ninja -b relwithdebinfo %*
if %ERRORLEVEL% NEQ 0 ( goto :eof )
