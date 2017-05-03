# Microsoft Developer Studio Project File - Name="libusb_static" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=libusb_static - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "libusb_static.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "libusb_static.mak" CFG="libusb_static - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "libusb_static - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "libusb_static - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "libusb_static - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "../Win32/Release/lib"
# PROP Intermediate_Dir "../Win32/Release/lib"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_LIB" /YX /FD /c
# ADD CPP /nologo /MD /W3 /GX /O2 /I "." /I "../libusb" /D "WIN32" /D "NDEBUG" /D "_UNICODE" /D "UNICODE" /U "_MBCS" /D "_LIB" /FR /FD /EHsc /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo /out:"../Win32/Release/lib/libusb-1.0.lib"

!ELSEIF  "$(CFG)" == "libusb_static - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "../Win32/Debug/lib"
# PROP Intermediate_Dir "../Win32/Debug/lib"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_LIB" /YX /FD /GZ /c
# ADD CPP /nologo /MDd /W3 /Gm /GX /ZI /Od /I "." /I "../libusb" /D "WIN32" /D "_DEBUG" /D "_UNICODE" /D "UNICODE" /U "_MBCS" /D "_LIB" /FR /FD /GZ /EHsc /c
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo /n
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo /out:"../Win32/Debug/lib/libusb-1.0.lib"

!ENDIF 

# Begin Target

# Name "libusb_static - Win32 Release"
# Name "libusb_static - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=..\libusb\core.c
# End Source File
# Begin Source File

SOURCE=..\libusb\os\darwin_usb.c
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=..\libusb\descriptor.c
# End Source File
# Begin Source File

SOURCE=..\libusb\io.c
# End Source File
# Begin Source File

SOURCE=..\libusb\os\linux_usbfs.c
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=..\libusb\os\poll_windows.c
# End Source File
# Begin Source File

SOURCE=..\libusb\sync.c
# End Source File
# Begin Source File

SOURCE=..\libusb\os\threads_windows.c
# End Source File
# Begin Source File

SOURCE=..\libusb\os\windows_winusb.c
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=.\config.h
# End Source File
# Begin Source File

SOURCE=..\libusb\os\darwin_usb.h
# End Source File
# Begin Source File

SOURCE=..\libusb\libusb.h
# End Source File
# Begin Source File

SOURCE=..\libusb\libusbi.h
# End Source File
# Begin Source File

SOURCE=..\libusb\os\linux_usbfs.h
# End Source File
# Begin Source File

SOURCE=..\libusb\os\poll_posix.h
# End Source File
# Begin Source File

SOURCE=..\libusb\os\poll_windows.h
# End Source File
# Begin Source File

SOURCE=..\libusb\os\threads_posix.h
# End Source File
# Begin Source File

SOURCE=..\libusb\os\threads_windows.h
# End Source File
# Begin Source File

SOURCE=..\libusb\os\windows_winusb.h
# End Source File
# Begin Source File

SOURCE=..\libusb\os\windows_common.h
# End Source File
# End Group
# End Target
# End Project
