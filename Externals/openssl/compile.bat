::The current precompiled version of openssl is:
::	openssl-1.0.0g
::	VS2010 SP1 / Windows SDK 7.1
::	using release environment (/MT)
::The following script must be run from a Windows SDK http://www.microsoft.com/download/en/details.aspx?id=8279 prompt,
::and http://www.activestate.com/activeperl/downloads must be the perl interpreter
@echo off

::Win32 release
call setenv /win7 /x86 /release
perl Configure no-bf no-camellia no-cast no-cms no-comp no-des no-idea no-jpake no-krb5 no-md2 no-md4 no-mdc2 no-rc2 no-rc4 no-rc5 no-ripemd no-seed no-store VC-WIN32
call ms\do_ms
nmake -f ms\nt.mak

xcopy /S /I /Y inc32\* openssl

mkdir openssl\Win32\openssl
move openssl\openssl\opensslconf.h openssl\Win32\openssl

xcopy /Y out32\*.lib openssl\Win32
copy /Y tmp32\lib.pdb openssl\Win32

nmake -f ms\nt.mak vclean

::x64 release
call setenv /win7 /x64 /release
perl Configure no-bf no-camellia no-cast no-cms no-comp no-des no-idea no-jpake no-krb5 no-md2 no-md4 no-mdc2 no-rc2 no-rc4 no-rc5 no-ripemd no-seed no-store VC-WIN64A
call ms\do_win64a
nmake -f ms\nt.mak

mkdir openssl\x64\openssl
copy inc32\openssl\opensslconf.h openssl\x64\openssl

xcopy /Y out32\*.lib openssl\x64
copy /Y tmp32\lib.pdb openssl\x64