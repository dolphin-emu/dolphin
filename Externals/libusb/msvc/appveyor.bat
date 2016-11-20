echo on
SetLocal EnableDelayedExpansion

if [%Configuration%] NEQ [Debug] goto releasex64
if [%Configuration%] NEQ [Release] goto debugx64

:debugx64
if [%Platform%] NEQ [x64] goto debugWin32
if [%Configuration%] NEQ [Debug] exit 0
call "C:\Program Files\Microsoft SDKs\Windows\v7.1\Bin\SetEnv.cmd" /Debug /x64
msbuild %libusb_2010% /p:Configuration=Debug,Platform=x64 /logger:"C:\Program Files\AppVeyor\BuildAgent\Appveyor.MSBuildLogger.dll"

:releasex64
if [%Platform%] NEQ [x64] goto releaseWin32
if [%Configuration%] NEQ [Release] exit 0
call "C:\Program Files\Microsoft SDKs\Windows\v7.1\Bin\SetEnv.cmd" /Release /x64
msbuild %libusb_2010% /p:Configuration=Release,Platform=x64 /logger:"C:\Program Files\AppVeyor\BuildAgent\Appveyor.MSBuildLogger.dll"

:debugWin32
if [%Platform%] NEQ [Win32] exit 0
if [%Configuration%] NEQ [Debug] exit 0
msbuild %libusb_2010% /p:Configuration=Debug,Platform=Win32 /logger:"C:\Program Files\AppVeyor\BuildAgent\Appveyor.MSBuildLogger.dll"

:releaseWin32
if [%Platform%] NEQ [Win32] exit 0
if [%Configuration%] NEQ [Release] exit 0
msbuild %libusb_2010% /p:Configuration=Release,Platform=Win32 /logger:"C:\Program Files\AppVeyor\BuildAgent\Appveyor.MSBuildLogger.dll"

