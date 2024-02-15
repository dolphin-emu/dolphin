@echo off
setlocal
setlocal EnableDelayedExpansion

:: Globals
set BUILD_ROOT=%~dp0\..\build

goto :init

:usage
    echo USAGE:
    echo     init.cmd [--help] [-c^|--compiler ^<clang^|msvc^>] [-g^|--generator ^<ninja^|msbuild^>]
    echo         [-b^|--build-type ^<debug^|release^|relwithdebinfo^|minsizerel^>] [-v^|--version X.Y.Z]
    echo         [--cppwinrt ^<version^>] [--fast]
    echo.
    echo ARGUMENTS
    echo     -c^|--compiler       Controls the compiler used, either 'clang' (the default) or 'msvc'
    echo     -g^|--generator      Controls the CMake generator used, either 'ninja' (the default) or 'msbuild'
    echo     -b^|--build-type     Controls the value of 'CMAKE_BUILD_TYPE', either 'debug' (the default), 'release',
    echo                         'relwithdebinfo', or 'minsizerel'
    echo     -v^|--version        Specifies the version of the NuGet package produced. Primarily only used by the CI
    echo                         build and is typically not necessary when building locally
    echo     --cppwinrt          Manually specifies the version of C++/WinRT to use for generating headers
    echo     --fast              Used to (slightly) reduce compile times and build output size. This is primarily
    echo                         used by the CI build machines where resources are more constrained. This switch is
    echo                         temporary and will be removed once https://github.com/microsoft/wil/issues/9 is fixed
    goto :eof

:init
    :: Initialize values as empty so that we can identify if we are using defaults later for error purposes
    set COMPILER=
    set GENERATOR=
    set BUILD_TYPE=
    set CMAKE_ARGS=
    set BITNESS=
    set VERSION=
    set CPPWINRT_VERSION=
    set FAST_BUILD=0

:parse
    if /I "%~1"=="" goto :execute

    if /I "%~1"=="--help" call :usage & goto :eof

    set COMPILER_SET=0
    if /I "%~1"=="-c" set COMPILER_SET=1
    if /I "%~1"=="--compiler" set COMPILER_SET=1
    if %COMPILER_SET%==1 (
        if "%COMPILER%" NEQ "" echo ERROR: Compiler already specified & call :usage & exit /B 1

        if /I "%~2"=="clang" set COMPILER=clang
        if /I "%~2"=="msvc" set COMPILER=msvc
        if "!COMPILER!"=="" echo ERROR: Unrecognized/missing compiler %~2 & call :usage & exit /B 1

        shift
        shift
        goto :parse
    )

    set GENERATOR_SET=0
    if /I "%~1"=="-g" set GENERATOR_SET=1
    if /I "%~1"=="--generator" set GENERATOR_SET=1
    if %GENERATOR_SET%==1 (
        if "%GENERATOR%" NEQ "" echo ERROR: Generator already specified & call :usage & exit /B 1

        if /I "%~2"=="ninja" set GENERATOR=ninja
        if /I "%~2"=="msbuild" set GENERATOR=msbuild
        if "!GENERATOR!"=="" echo ERROR: Unrecognized/missing generator %~2 & call :usage & exit /B 1

        shift
        shift
        goto :parse
    )

    set BUILD_TYPE_SET=0
    if /I "%~1"=="-b" set BUILD_TYPE_SET=1
    if /I "%~1"=="--build-type" set BUILD_TYPE_SET=1
    if %BUILD_TYPE_SET%==1 (
        if "%BUILD_TYPE%" NEQ "" echo ERROR: Build type already specified & call :usage & exit /B 1

        if /I "%~2"=="debug" set BUILD_TYPE=debug
        if /I "%~2"=="release" set BUILD_TYPE=release
        if /I "%~2"=="relwithdebinfo" set BUILD_TYPE=relwithdebinfo
        if /I "%~2"=="minsizerel" set BUILD_TYPE=minsizerel
        if "!BUILD_TYPE!"=="" echo ERROR: Unrecognized/missing build type %~2 & call :usage & exit /B 1

        shift
        shift
        goto :parse
    )

    set VERSION_SET=0
    if /I "%~1"=="-v" set VERSION_SET=1
    if /I "%~1"=="--version" set VERSION_SET=1
    if %VERSION_SET%==1 (
        if "%VERSION%" NEQ "" echo ERROR: Version already specified & call :usage & exit /B 1
        if /I "%~2"=="" echo ERROR: Version string missing & call :usage & exit /B 1

        set VERSION=%~2

        shift
        shift
        goto :parse
    )

    if /I "%~1"=="--cppwinrt" (
        if "%CPPWINRT_VERSION%" NEQ "" echo ERROR: C++/WinRT version already specified & call :usage & exit /B 1
        if /I "%~2"=="" echo ERROR: C++/WinRT version string missing & call :usage & exit /B 1

        set CPPWINRT_VERSION=%~2

        shift
        shift
        goto :parse
    )

    if /I "%~1"=="--fast" (
        if %FAST_BUILD% NEQ 0 echo ERROR: Fast build already specified
        set FAST_BUILD=1
        shift
        goto :parse
    )

    echo ERROR: Unrecognized argument %~1
    call :usage
    exit /B 1

:execute
    :: Check for conflicting arguments
    if "%GENERATOR%"=="msbuild" (
        if "%COMPILER%"=="clang" echo ERROR: Cannot use Clang with MSBuild & exit /B 1
    )

    :: Select defaults
    if "%GENERATOR%"=="" set GENERATOR=ninja
    if %GENERATOR%==msbuild set COMPILER=msvc

    if "%COMPILER%"=="" set COMPILER=clang

    if "%BUILD_TYPE%"=="" set BUILD_TYPE=debug

    :: Formulate CMake arguments
    if %GENERATOR%==ninja set CMAKE_ARGS=%CMAKE_ARGS% -G Ninja

    if %COMPILER%==clang set CMAKE_ARGS=%CMAKE_ARGS% -DCMAKE_C_COMPILER=clang-cl -DCMAKE_CXX_COMPILER=clang-cl
    if %COMPILER%==msvc set CMAKE_ARGS=%CMAKE_ARGS% -DCMAKE_C_COMPILER=cl -DCMAKE_CXX_COMPILER=cl

    if %GENERATOR% NEQ msbuild (
        if %BUILD_TYPE%==debug set CMAKE_ARGS=%CMAKE_ARGS% -DCMAKE_BUILD_TYPE=Debug
        if %BUILD_TYPE%==release set CMAKE_ARGS=%CMAKE_ARGS% -DCMAKE_BUILD_TYPE=Release
        if %BUILD_TYPE%==relwithdebinfo set CMAKE_ARGS=%CMAKE_ARGS% -DCMAKE_BUILD_TYPE=RelWithDebInfo
        if %BUILD_TYPE%==minsizerel set CMAKE_ARGS=%CMAKE_ARGS% -DCMAKE_BUILD_TYPE=MinSizeRel
    ) else (
        :: The Visual Studio generator, by default, will use the most recent Windows SDK version installed that is not
        :: greater than the host OS version. This decision is to ensure that anything built will be able to run on the
        :: machine that built it. This experience is generally okay, if not desired, but affects us since we build with
        :: '/permissive-' etc. and older versions of the SDK are typically not as clean as the most recent versions.
        :: This flag will force the generator to select the most recent SDK version independent of host OS version.
        set CMAKE_ARGS=%CMAKE_ARGS% -DCMAKE_SYSTEM_VERSION=10.0
    )

    if "%VERSION%" NEQ "" set CMAKE_ARGS=%CMAKE_ARGS% -DWIL_BUILD_VERSION=%VERSION%

    if "%CPPWINRT_VERSION%" NEQ "" set CMAKE_ARGS=%CMAKE_ARGS% -DCPPWINRT_VERSION=%CPPWINRT_VERSION%

    if %FAST_BUILD%==1 set CMAKE_ARGS=%CMAKE_ARGS% -DFAST_BUILD=ON

    set CMAKE_ARGS=%CMAKE_ARGS% -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

    :: Figure out the platform
    if "%Platform%"=="" echo ERROR: The init.cmd script must be run from a Visual Studio command window & exit /B 1
    if "%Platform%"=="x86" (
        set BITNESS=32
        if %COMPILER%==clang set CFLAGS=-m32 & set CXXFLAGS=-m32
    )
    if "%Platform%"=="x64" set BITNESS=64
    if "%BITNESS%"=="" echo ERROR: Unrecognized/unsupported platform %Platform% & exit /B 1

    :: Set up the build directory
    set BUILD_DIR=%BUILD_ROOT%\%COMPILER%%BITNESS%%BUILD_TYPE%
    mkdir %BUILD_DIR% > NUL 2>&1

    :: Run CMake
    pushd %BUILD_DIR%
    echo Using compiler....... %COMPILER%
    echo Using architecture... %Platform%
    echo Using build type..... %BUILD_TYPE%
    echo Using build root..... %CD%
    echo.
    cmake %CMAKE_ARGS% ..\..
    popd

    goto :eof
