@echo off
setlocal
setlocal EnableDelayedExpansion

set TEST_ARGS=%*

set BUILD_ROOT=%~dp0\..\build

:: Unlike building, we don't need to limit ourselves to the Platform of the command window
set COMPILERS=clang msvc
set ARCHITECTURES=32 64
set BUILD_TYPES=debug release relwithdebinfo minsizerel

for %%c in (%COMPILERS%) do (
    for %%a in (%ARCHITECTURES%) do (
        for %%b in (%BUILD_TYPES%) do (
            call :execute_tests %%c%%a%%b
            if !ERRORLEVEL! NEQ 0 ( goto :eof )
        )
    )
)

goto :eof

:execute_tests
set BUILD_DIR=%BUILD_ROOT%\%1
if not exist %BUILD_DIR% ( goto :eof )

pushd %BUILD_DIR%
echo Running tests from %CD%
call :execute_test app witest.app.exe
if %ERRORLEVEL% NEQ 0 ( goto :execute_tests_done )
call :execute_test cpplatest witest.cpplatest.exe
if %ERRORLEVEL% NEQ 0 ( goto :execute_tests_done )
call :execute_test noexcept witest.noexcept.exe
if %ERRORLEVEL% NEQ 0 ( goto :execute_tests_done )
call :execute_test normal witest.exe
if %ERRORLEVEL% NEQ 0 ( goto :execute_tests_done )
call :execute_test sanitize-address witest.asan.exe
if %ERRORLEVEL% NEQ 0 ( goto :execute_tests_done )
call :execute_test sanitize-undefined-behavior witest.ubsan.exe
if %ERRORLEVEL% NEQ 0 ( goto :execute_tests_done )
call :execute_test win7 witest.win7.exe
if %ERRORLEVEL% NEQ 0 ( goto :execute_tests_done )

:execute_tests_done
set EXIT_CODE=%ERRORLEVEL%
popd
exit /B %EXIT_CODE%

:execute_test
if not exist tests\%1\%2 ( goto :eof )
echo Running %1 tests...
tests\%1\%2 %TEST_ARGS%
goto :eof
