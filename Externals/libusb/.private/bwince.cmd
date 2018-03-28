@echo off
rem produce the Win CE binary files for snapshots
rem !!!THIS SCRIPT IS FOR INTERNAL DEVELOPER USE ONLY!!!

if "x%VSINSTALLDIR%"=="x" goto usage

if "x%WINCE_TARGET_DIR_BASE%"=="x" set WINCE_TARGET_DIR_BASE=E:\dailies
if "x%WINCE_TARGET_ARCHES%"=="x" set WINCE_TARGET_ARCHES=ARMV4I MIPSII MIPSII_FP MIPSIV MIPSIV_FP SH4 x86


set WINCE_TARGET_DIR=%WINCE_TARGET_DIR_BASE%\%DATE:/=-%
set MSBUILD_CMD=msbuild.exe
set MSBUILD_TARGET=Rebuild
set MSBUILD_CONFIGURATION=Release
set WINCE_SLN=msvc\libusb_wince.sln
set PLATFORM_PREFIX=STANDARDSDK_500 (
set PLATFORM_POSTFIX=)


set PWD=%~dp0
cd ..

mkdir %WINCE_TARGET_DIR%
mkdir %WINCE_TARGET_DIR%\include\libusb-1.0
copy libusb\libusb-1.0.def %WINCE_TARGET_DIR%
copy libusb\libusb.h %WINCE_TARGET_DIR%\include\libusb-1.0
for %%A in (%WINCE_TARGET_ARCHES%) do mkdir %WINCE_TARGET_DIR%\%%A
for %%A in (%WINCE_TARGET_ARCHES%) do mkdir %WINCE_TARGET_DIR%\%%A\static
for %%A in (%WINCE_TARGET_ARCHES%) do mkdir %WINCE_TARGET_DIR%\%%A\dll
for %%A in (%WINCE_TARGET_ARCHES%) do mkdir %WINCE_TARGET_DIR%\examples\%%A
mkdir %WINCE_TARGET_DIR%\examples\source
copy examples\listdevs.c %WINCE_TARGET_DIR%\examples\source
copy examples\xusb.c %WINCE_TARGET_DIR%\examples\source
copy msvc\stdint.h %WINCE_TARGET_DIR%\examples\source
copy .private\wbs_wince.txt %WINCE_TARGET_DIR%\README.txt
copy COPYING %WINCE_TARGET_DIR%\COPYING

rem Perform the rebuild
for %%A in (%WINCE_TARGET_ARCHES%) do %MSBUILD_CMD% %WINCE_SLN% /property:Platform="%PLATFORM_PREFIX%%%A%PLATFORM_POSTFIX%" /property:Configuration=%MSBUILD_CONFIGURATION% /target:%MSBUILD_TARGET%"


rem Copy across the binaries
for %%A in (%WINCE_TARGET_ARCHES%) do (
	copy %%A\%MSBUILD_CONFIGURATION%\lib\libusb-1.0.lib %WINCE_TARGET_DIR%\%%A\static
	copy %%A\%MSBUILD_CONFIGURATION%\examples\listdevs.exe %WINCE_TARGET_DIR%\examples\%%A
	copy %%A\%MSBUILD_CONFIGURATION%\examples\xusb.exe %WINCE_TARGET_DIR%\examples\%%A
	copy %%A\%MSBUILD_CONFIGURATION%\dll\libusb-1.0.lib %WINCE_TARGET_DIR%\%%A\dll
	copy %%A\%MSBUILD_CONFIGURATION%\dll\libusb-1.0.dll %WINCE_TARGET_DIR%\%%A\dll
	copy %%A\%MSBUILD_CONFIGURATION%\dll\libusb-1.0.pdb %WINCE_TARGET_DIR%\%%A\dll )

goto done

:usage
echo must be run in a Visual Studio 2005 build environment!

:done
cd %PWD%