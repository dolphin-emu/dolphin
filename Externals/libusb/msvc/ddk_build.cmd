@echo off
::# default builds static library. 
::# you can pass the following arguments (case insensitive):
::# - "DLL" to build a DLL instead of a static library
::# - "/MT" to build a static library compatible with MSVC's /MT option (LIBCMT vs MSVCRT)
::# - "USBDK" to build with UsbDk backend

if Test%BUILD_ALT_DIR%==Test goto usage

::# process commandline parameters
set TARGET=LIBRARY
set STATIC_LIBC=
set WITH_USBDK=
set version=1.0
set PWD=%~dp0
set BUILD_CMD=build -bcwgZ -M2

:more_args

if "%1" == "" goto no_more_args
::# /I for case insensitive
if /I Test%1==TestDLL set TARGET=DYNLINK
if /I Test%1==Test/MT set STATIC_LIBC=1
if /I Test%1==TestUSBDK set WITH_USBDK=1

shift
goto more_args

:no_more_args

cd ..\libusb\os
echo TARGETTYPE=%TARGET% > target
copy target+..\..\msvc\libusb_sources sources >NUL 2>&1
del target
@echo on
%BUILD_CMD%
@echo off
if errorlevel 1 goto builderror
cd ..\..

set cpudir=i386
set destType=Win32
if %_BUILDARCH%==x86 goto isI386
set cpudir=amd64
set destType=x64
:isI386

set srcPath=libusb\os\obj%BUILD_ALT_DIR%\%cpudir%

set dstPath=%destType%\Debug
if %DDKBUILDENV%==chk goto isDebug
set dstPath=%destType%\Release
:isDebug

if exist %destType% goto md2
mkdir %destType%
:md2
if exist %dstPath% goto md3
mkdir %dstPath%
:md3
if exist %dstPath%\dll goto md4
mkdir %dstPath%\dll
:md4
if exist %dstPath%\lib goto md5
md %dstPath%\lib
:md5
if exist %dstPath%\examples goto md6
md %dstPath%\examples
:md6
@echo on

@if /I NOT Test%1==TestDLL goto copylib
copy %srcPath%\libusb-%version%.dll %dstPath%\dll
copy %srcPath%\libusb-%version%.pdb %dstPath%\dll
:copylib
copy %srcPath%\libusb-%version%.lib %dstPath%\lib

@echo off

if exist examples\listdevs_ddkbuild goto md7
md examples\listdevs_ddkbuild
:md7

cd examples\listdevs_ddkbuild
copy ..\..\msvc\listdevs_sources sources >NUL 2>&1
@echo on
%BUILD_CMD%
@echo off
if errorlevel 1 goto builderror
cd ..\..

set srcPath=examples\listdevs_ddkbuild\obj%BUILD_ALT_DIR%\%cpudir%
@echo on

copy %srcPath%\listdevs.exe %dstPath%\examples
copy %srcPath%\listdevs.pdb %dstPath%\examples

@echo off

if exist examples\xusb_ddkbuild goto md8
md examples\xusb_ddkbuild
:md8

cd examples\xusb_ddkbuild
copy ..\..\msvc\xusb_sources sources >NUL 2>&1
@echo on
%BUILD_CMD%
@echo off
if errorlevel 1 goto builderror
cd ..\..

set srcPath=examples\xusb_ddkbuild\obj%BUILD_ALT_DIR%\%cpudir%
@echo on

copy %srcPath%\xusb.exe %dstPath%\examples
copy %srcPath%\xusb.pdb %dstPath%\examples

@echo off

if exist examples\getopt\getopt_ddkbuild goto md9
md examples\getopt\getopt_ddkbuild
:md9

cd examples\getopt\getopt_ddkbuild
copy ..\..\..\msvc\getopt_sources sources >NUL 2>&1
@echo on
%BUILD_CMD%
@echo off
if errorlevel 1 goto builderror
cd ..\..\..

if exist examples\fxload_ddkbuild goto md10
md examples\fxload_ddkbuild
:md10

cd examples\fxload_ddkbuild
copy ..\..\msvc\fxload_sources sources >NUL 2>&1
@echo on
%BUILD_CMD%
@echo off
if errorlevel 1 goto builderror
cd ..\..

set srcPath=examples\fxload_ddkbuild\obj%BUILD_ALT_DIR%\%cpudir%
@echo on

copy %srcPath%\fxload.exe %dstPath%\examples
copy %srcPath%\fxload.pdb %dstPath%\examples

@echo off

if exist examples\hotplugtest_ddkbuild goto md11
md examples\hotplugtest_ddkbuild
:md11

cd examples\hotplugtest_ddkbuild
copy ..\..\msvc\hotplugtest_sources sources >NUL 2>&1
@echo on
%BUILD_CMD%
@echo off
if errorlevel 1 goto builderror
cd ..\..

set srcPath=examples\hotplugtest_ddkbuild\obj%BUILD_ALT_DIR%\%cpudir%
@echo on

copy %srcPath%\hotplugtest.exe %dstPath%\examples
copy %srcPath%\hotplugtest.pdb %dstPath%\examples

@echo off

cd msvc
goto done

:usage
echo ddk_build must be run in a WDK build environment
pause
goto done

:builderror
echo Build failed

:done
cd %PWD%
