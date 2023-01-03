; Usage:
;   get the latest nsis: https://nsis.sourceforge.io/Download
;   probably also want vscode extension: https://marketplace.visualstudio.com/items?itemName=idleberg.nsis
;   makensis /DDOLPHIN_ARCH=<x64|arm64> /PRODUCT_VERSION=<release-name> <this-script>

; Require /DDOLPHIN_ARCH=<x64|arm64> to makensis.
; TODO support packing both archs in single installer.
!ifndef DOLPHIN_ARCH
  !error "DOLPHIN_ARCH must be defined"
!endif
; Require /PRODUCT_VERSION=<release-name> to makensis.
!ifndef PRODUCT_VERSION
  !error "PRODUCT_VERSION must be defined"
!endif

!define PRODUCT_NAME "Dolphin"
!define PRODUCT_PUBLISHER "Dolphin Team"
!define PRODUCT_WEB_SITE "https://dolphin-emu.org/"
!define PRODUCT_DIR_REGKEY "Software\Microsoft\Windows\CurrentVersion\App Paths\${PRODUCT_NAME}.exe"
!define PRODUCT_UNINST_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_NAME}"

!define BINARY_SOURCE_DIR "..\Binary\${DOLPHIN_ARCH}"

Name "${PRODUCT_NAME}"
OutFile "dolphin-${DOLPHIN_ARCH}-${PRODUCT_VERSION}.exe"
SetCompressor /SOLID lzma
ShowInstDetails show
ShowUnInstDetails show

; Setup MultiUser support:
; If launched without ability to elevate, user will not see any extra options.
; If user has ability to elevate, they can choose to install system-wide, with default to CurrentUser.
!define MULTIUSER_EXECUTIONLEVEL Highest
!define MULTIUSER_INSTALLMODE_DEFAULT_CURRENTUSER
!define MULTIUSER_INSTALLMODE_INSTDIR "${PRODUCT_NAME}"
!define MULTIUSER_MUI
!define MULTIUSER_INSTALLMODE_COMMANDLINE
!define MULTIUSER_USE_PROGRAMFILES64
!include "MultiUser.nsh"

!include "MUI2.nsh"

; MUI Settings
!define MUI_ICON "Dolphin.ico"
!define MUI_UNICON "${NSISDIR}\Contrib\Graphics\Icons\modern-uninstall.ico"

; License page
!insertmacro MUI_PAGE_LICENSE "..\Data\license.txt"
; All/Current user selection page
!insertmacro MULTIUSER_PAGE_INSTALLMODE
; Directory page
!insertmacro MUI_PAGE_DIRECTORY
; Instfiles page
!insertmacro MUI_PAGE_INSTFILES
; Finish page
!insertmacro MUI_PAGE_FINISH

; Uninstaller pages
!insertmacro MUI_UNPAGE_INSTFILES

; Language files
!insertmacro MUI_LANGUAGE "Afrikaans"
!insertmacro MUI_LANGUAGE "Albanian"
!insertmacro MUI_LANGUAGE "Arabic"
!insertmacro MUI_LANGUAGE "Armenian"
!insertmacro MUI_LANGUAGE "Asturian"
!insertmacro MUI_LANGUAGE "Basque"
!insertmacro MUI_LANGUAGE "Belarusian"
!insertmacro MUI_LANGUAGE "Bosnian"
!insertmacro MUI_LANGUAGE "Breton"
!insertmacro MUI_LANGUAGE "Bulgarian"
!insertmacro MUI_LANGUAGE "Catalan"
!insertmacro MUI_LANGUAGE "Corsican"
!insertmacro MUI_LANGUAGE "Croatian"
!insertmacro MUI_LANGUAGE "Czech"
!insertmacro MUI_LANGUAGE "Danish"
!insertmacro MUI_LANGUAGE "Dutch"
!insertmacro MUI_LANGUAGE "English"
!insertmacro MUI_LANGUAGE "Esperanto"
!insertmacro MUI_LANGUAGE "Estonian"
!insertmacro MUI_LANGUAGE "Farsi"
!insertmacro MUI_LANGUAGE "Finnish"
!insertmacro MUI_LANGUAGE "French"
!insertmacro MUI_LANGUAGE "Galician"
!insertmacro MUI_LANGUAGE "Georgian"
!insertmacro MUI_LANGUAGE "German"
!insertmacro MUI_LANGUAGE "Greek"
!insertmacro MUI_LANGUAGE "Hebrew"
!insertmacro MUI_LANGUAGE "Hindi"
!insertmacro MUI_LANGUAGE "Hungarian"
!insertmacro MUI_LANGUAGE "Icelandic"
!insertmacro MUI_LANGUAGE "Indonesian"
!insertmacro MUI_LANGUAGE "Irish"
!insertmacro MUI_LANGUAGE "Italian"
!insertmacro MUI_LANGUAGE "Japanese"
!insertmacro MUI_LANGUAGE "Korean"
!insertmacro MUI_LANGUAGE "Kurdish"
!insertmacro MUI_LANGUAGE "Latvian"
!insertmacro MUI_LANGUAGE "Lithuanian"
!insertmacro MUI_LANGUAGE "Luxembourgish"
!insertmacro MUI_LANGUAGE "Macedonian"
!insertmacro MUI_LANGUAGE "Malay"
!insertmacro MUI_LANGUAGE "Mongolian"
!insertmacro MUI_LANGUAGE "Norwegian"
!insertmacro MUI_LANGUAGE "NorwegianNynorsk"
!insertmacro MUI_LANGUAGE "Pashto"
!insertmacro MUI_LANGUAGE "Polish"
!insertmacro MUI_LANGUAGE "Portuguese"
!insertmacro MUI_LANGUAGE "PortugueseBR"
!insertmacro MUI_LANGUAGE "Romanian"
!insertmacro MUI_LANGUAGE "Russian"
!insertmacro MUI_LANGUAGE "ScotsGaelic"
!insertmacro MUI_LANGUAGE "Serbian"
!insertmacro MUI_LANGUAGE "SerbianLatin"
!insertmacro MUI_LANGUAGE "SimpChinese"
!insertmacro MUI_LANGUAGE "Slovak"
!insertmacro MUI_LANGUAGE "Slovenian"
!insertmacro MUI_LANGUAGE "Spanish"
!insertmacro MUI_LANGUAGE "SpanishInternational"
!insertmacro MUI_LANGUAGE "Swedish"
!insertmacro MUI_LANGUAGE "Tatar"
!insertmacro MUI_LANGUAGE "Thai"
!insertmacro MUI_LANGUAGE "TradChinese"
!insertmacro MUI_LANGUAGE "Turkish"
!insertmacro MUI_LANGUAGE "Ukrainian"
!insertmacro MUI_LANGUAGE "Uzbek"
!insertmacro MUI_LANGUAGE "Vietnamese"
!insertmacro MUI_LANGUAGE "Welsh"

; MUI end ------

!include "WinVer.nsh"
; Declare the installer itself as win10/win11 compatible, so WinVer.nsh works correctly.
ManifestSupportedOS {8e0f7a12-bfb3-4fe8-b9a5-48fd50a15a9a}

Function .onInit
  !insertmacro MULTIUSER_INIT

  ; Keep in sync with build_info.txt
  !define MIN_WIN10_VERSION 1703
  ${IfNot} ${AtLeastwin10}
  ${OrIfNot} ${AtLeastWaaS} ${MIN_WIN10_VERSION}
    MessageBox MB_OK "At least Windows 10 version ${MIN_WIN10_VERSION} is required."
    Abort
  ${EndIf}

  !insertmacro MUI_LANGDLL_DISPLAY
FunctionEnd

Function un.onInit
  !insertmacro MULTIUSER_UNINIT
FunctionEnd

Var DisplayName
!macro UPDATE_DISPLAYNAME
  ${If} $MultiUser.InstallMode == "CurrentUser"
    StrCpy $DisplayName "$(^Name) (User)"
  ${Else}
    StrCpy $DisplayName "$(^Name)"
  ${EndIf}
!macroend

Section "Base"
  SectionIn RO

  SetOutPath "$INSTDIR"

  ; Clean up some old files possibly leftover from previous installs
  ; We only need to care about files which would cause problems if they were to
  ; be unexpectedly dynamically picked up by the incoming install.
  ; Note NSIS will happily install the same thing even if already installed.
  RMDir /r "$INSTDIR\Sys"
  RMDir /r "$INSTDIR\Languages"

  ; The binplaced build output will be included verbatim.
  File /r "${BINARY_SOURCE_DIR}\*"

  !insertmacro UPDATE_DISPLAYNAME

  ; Create start menu and desktop shortcuts
  ; This needs to be done after Dolphin.exe is copied
  CreateDirectory "$SMPROGRAMS\${PRODUCT_NAME}"
  CreateShortCut "$SMPROGRAMS\${PRODUCT_NAME}\$DisplayName.lnk" "$INSTDIR\Dolphin.exe"
  CreateShortCut "$DESKTOP\$DisplayName.lnk" "$INSTDIR\Dolphin.exe"

  ; ??
  SetOutPath "$TEMP"
SectionEnd

Section -AdditionalIcons
  ; Create start menu shortcut for the uninstaller
  CreateShortCut "$SMPROGRAMS\${PRODUCT_NAME}\Uninstall $DisplayName.lnk" "$INSTDIR\uninst.exe" "/$MultiUser.InstallMode"
SectionEnd

!include "FileFunc.nsh"

Section -Post
  WriteUninstaller "$INSTDIR\uninst.exe"

  WriteRegStr SHCTX "${PRODUCT_DIR_REGKEY}" "" "$INSTDIR\Dolphin.exe"

  ; Write metadata for add/remove programs applet
  WriteRegStr SHCTX "${PRODUCT_UNINST_KEY}" "DisplayName" "$DisplayName"
  WriteRegStr SHCTX "${PRODUCT_UNINST_KEY}" "UninstallString" "$INSTDIR\uninst.exe /$MultiUser.InstallMode"
  WriteRegStr SHCTX "${PRODUCT_UNINST_KEY}" "DisplayIcon" "$INSTDIR\Dolphin.exe"
  WriteRegStr SHCTX "${PRODUCT_UNINST_KEY}" "DisplayVersion" "${PRODUCT_VERSION}"
  WriteRegStr SHCTX "${PRODUCT_UNINST_KEY}" "URLInfoAbout" "${PRODUCT_WEB_SITE}"
  WriteRegStr SHCTX "${PRODUCT_UNINST_KEY}" "Publisher" "${PRODUCT_PUBLISHER}"
  WriteRegStr SHCTX "${PRODUCT_UNINST_KEY}" "InstallLocation" "$INSTDIR"
  ${GetSize} "$INSTDIR" "/S=0K" $0 $1 $2
  IntFmt $0 "0x%08X" $0
  WriteRegDWORD SHCTX "${PRODUCT_UNINST_KEY}" "EstimatedSize" "$0"
  WriteRegStr SHCTX "${PRODUCT_UNINST_KEY}" "Comments" "GameCube and Wii emulator"
SectionEnd

Section Uninstall
  !insertmacro UPDATE_DISPLAYNAME

  Delete "$SMPROGRAMS\${PRODUCT_NAME}\Uninstall $DisplayName.lnk"

  Delete "$DESKTOP\$DisplayName.lnk"
  Delete "$SMPROGRAMS\${PRODUCT_NAME}\$DisplayName.lnk"
  RMDir "$SMPROGRAMS\${PRODUCT_NAME}"

  ; Be a bit careful to not delete files a user may have put into the install directory.
  Delete "$INSTDIR\*.dll"
  Delete "$INSTDIR\build_info.txt"
  Delete "$INSTDIR\Dolphin.exe"
  Delete "$INSTDIR\DolphinTool.exe"
  Delete "$INSTDIR\DSPTool.exe"
  Delete "$INSTDIR\license.txt"
  Delete "$INSTDIR\qt.conf"
  Delete "$INSTDIR\uninst.exe"
  Delete "$INSTDIR\Updater.exe"
  Delete "$INSTDIR\Updater.log"
  RMDir /r "$INSTDIR\Sys"
  RMDir /r "$INSTDIR\Languages"
  RMDir /r "$INSTDIR\QtPlugins"
  RMDir "$INSTDIR"

  DeleteRegKey SHCTX "${PRODUCT_UNINST_KEY}"

  DeleteRegKey SHCTX "${PRODUCT_DIR_REGKEY}"

  SetAutoClose true
SectionEnd
