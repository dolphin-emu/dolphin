@echo off
setlocal EnableDelayedExpansion

set BUILD_ROOT=%~dp0\..\build

:: Unlike building, we don't need to limit ourselves to the Platform of the command window
call :execute_tests clang64debug
if %ERRORLEVEL% NEQ 0 ( goto :eof )
call :execute_tests clang64release
if %ERRORLEVEL% NEQ 0 ( goto :eof )
call :execute_tests clang64relwithdebinfo
if %ERRORLEVEL% NEQ 0 ( goto :eof )
call :execute_tests clang64minsizerel
if %ERRORLEVEL% NEQ 0 ( goto :eof )

call :execute_tests clang32debug
if %ERRORLEVEL% NEQ 0 ( goto :eof )
call :execute_tests clang32release
if %ERRORLEVEL% NEQ 0 ( goto :eof )
call :execute_tests clang32relwithdebinfo
if %ERRORLEVEL% NEQ 0 ( goto :eof )
call :execute_tests clang32minsizerel
if %ERRORLEVEL% NEQ 0 ( goto :eof )

call :execute_tests msvc64debug
if %ERRORLEVEL% NEQ 0 ( goto :eof )
call :execute_tests msvc64release
if %ERRORLEVEL% NEQ 0 ( goto :eof )
call :execute_tests msvc64relwithdebinfo
if %ERRORLEVEL% NEQ 0 ( goto :eof )
call :execute_tests msvc64minsizerel
if %ERRORLEVEL% NEQ 0 ( goto :eof )

call :execute_tests msvc32debug
if %ERRORLEVEL% NEQ 0 ( goto :eof )
call :execute_tests msvc32release
if %ERRORLEVEL% NEQ 0 ( goto :eof )
call :execute_tests msvc32relwithdebinfo
if %ERRORLEVEL% NEQ 0 ( goto :eof )
call :execute_tests msvc32minsizerel
if %ERRORLEVEL% NEQ 0 ( goto :eof )

goto :eof

:execute_tests
set BUILD_DIR=%BUILD_ROOT%\%1
if not exist %BUILD_DIR% ( goto :eof )

pushd %BUILD_DIR%
echo Running tests from %CD%
call :execute_test app witest.app.exe
if %ERRORLEVEL% NEQ 0 ( popd && goto :eof )
call :execute_test cpplatest witest.cpplatest.exe
if %ERRORLEVEL% NEQ 0 ( popd && goto :eof )
call :execute_test noexcept witest.noexcept.exe
if %ERRORLEVEL% NEQ 0 ( popd && goto :eof )
call :execute_test normal witest.exe
if %ERRORLEVEL% NEQ 0 ( popd && goto :eof )
popd

goto :eof

:execute_test
if not exist tests\%1\%2 ( goto :eof )
echo Running %1 tests...
tests\%1\%2
goto :eof
