echo on
SetLocal EnableDelayedExpansion

if [%Configuration%] NEQ [Release] exit 0

if [%Platform%] NEQ [x64] goto Win32
C:\msys64\usr\bin\bash -e -l -c "./bootstrap.sh" || exit /B
C:\msys64\usr\bin\bash -e -l -c "mkdir build-x64" || exit /B
C:\msys64\usr\bin\bash -e -l -c "cd build-x64 && ../configure --prefix=/mingw64 --build=x86_64-w64-mingw32 --host=x86_64-w64-mingw32" || exit /B
C:\msys64\usr\bin\bash -e -l -c "cd build-x64 && make -j4" || exit /B
C:\msys64\usr\bin\bash -e -l -c "cd build-x64 && make install" || exit /B

:Win32
if [%Platform%] NEQ [Win32] exit 0
C:\msys64\usr\bin\bash -e -l -c "./bootstrap.sh" || exit /B
C:\msys64\usr\bin\bash -e -l -c "mkdir build-Win32" || exit /B
C:\msys64\usr\bin\bash -e -l -c "cd build-Win32 && ../configure --prefix=/mingw32 --build=i686-w64-mingw32 --host=i686-w64-mingw32" || exit /B
C:\msys64\usr\bin\bash -e -l -c "cd build-Win32 && make -j4" || exit /B
C:\msys64\usr\bin\bash -e -l -c "cd build-Win32 && make install" || exit /B
