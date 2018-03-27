echo on
SetLocal EnableDelayedExpansion

if [%Configuration%] NEQ [Release] exit 0
if [%Platform%] NEQ [Win32] exit 0

C:\cygwin\bin\bash -e -l -c "./bootstrap.sh" || exit /B
C:\cygwin\bin\bash -e -l -c "mkdir build-Win32-cygwin" || exit /B
C:\cygwin\bin\bash -e -l -c "cd build-Win32-cygwin && ../configure --enable-examples-build --enable-tests-build" || exit /B
C:\cygwin\bin\bash -e -l -c "cd build-Win32-cygwin && make -j4" || exit /B
C:\cygwin\bin\bash -e -l -c "cd build-Win32-cygwin && make install" || exit /B
