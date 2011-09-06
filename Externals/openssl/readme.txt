#How these openssl libs were compiled:
# VS2010 SP1 from Windows SDK 7.1 prompt
# using debug and release environments
# debug is /MTd, release is /MT

#There is a modification to openssl which removes OPENSSL_cpuid_setup from the x86_64 compile.
# This was a stub function anyways, but it caused errors when linking to x64 dolphin...

#win32 debug
setenv /win7 /x86 /debug
perl Configure no-bf no-camellia no-cast no-cms no-comp no-des no-ec no-ecdh no-ecdsa no-err no-idea no-jpake no-krb5 no-md2 no-md4 no-mdc2 no-rc2 no-rc4 no-rc5 no-ripemd no-seed no-store debug-VC-WIN32
ms\do_ms
nmake -f ms\nt.mak

#win32 release
setenv /win7 /x86 /release
perl Configure no-bf no-camellia no-cast no-cms no-comp no-des no-ec no-ecdh no-ecdsa no-err no-idea no-jpake no-krb5 no-md2 no-md4 no-mdc2 no-rc2 no-rc4 no-rc5 no-ripemd no-seed no-store VC-WIN32
ms\do_ms
nmake -f ms\nt.mak

#win64 debug
setenv /win7 /x64 /debug
perl Configure no-bf no-camellia no-cast no-cms no-comp no-des no-ec no-ecdh no-ecdsa no-err no-idea no-jpake no-krb5 no-md2 no-md4 no-mdc2 no-rc2 no-rc4 no-rc5 no-ripemd no-seed no-store debug-VC-WIN64A
ms\do_win64a
nmake -f ms\nt.mak

#win64 release
setenv /win7 /x64 /release
perl Configure no-bf no-camellia no-cast no-cms no-comp no-des no-ec no-ecdh no-ecdsa no-err no-idea no-jpake no-krb5 no-md2 no-md4 no-mdc2 no-rc2 no-rc4 no-rc5 no-ripemd no-seed no-store VC-WIN64A
ms\do_win64a
nmake -f ms\nt.mak