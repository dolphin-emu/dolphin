!system "GetSVNRev.exe" ; ATTENTION: This MUST be run before this script
!include "svnrev.txt" ; !defines PRODUCT_VERSION
!define BASE_DIR "..\Binary\win32"

; HM NIS Edit Wizard helper defines
!define PRODUCT_NAME "Dolphin"
!define PRODUCT_PUBLISHER "Dolphin Team"
!define PRODUCT_WEB_SITE "http://www.dolphin-emu.com"
!define PRODUCT_DIR_REGKEY "Software\Microsoft\Windows\CurrentVersion\App Paths\Dolphin.exe"
!define PRODUCT_UNINST_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_NAME}"
!define PRODUCT_UNINST_ROOT_KEY "HKLM"

SetCompressor lzma

; MUI 1.67 compatible ------
!include "MUI.nsh"

; MUI Settings
!define MUI_ABORTWARNING
!define MUI_ICON "Dolphin.ico"
!define MUI_UNICON "${NSISDIR}\Contrib\Graphics\Icons\modern-uninstall.ico"

; Language Selection Dialog Settings
!define MUI_LANGDLL_REGISTRY_ROOT "${PRODUCT_UNINST_ROOT_KEY}"
!define MUI_LANGDLL_REGISTRY_KEY "${PRODUCT_UNINST_KEY}"
!define MUI_LANGDLL_REGISTRY_VALUENAME "NSIS:Language"

; License page
!insertmacro MUI_PAGE_LICENSE "Licence.txt"
; Components page
!insertmacro MUI_PAGE_COMPONENTS
; Directory page
!insertmacro MUI_PAGE_DIRECTORY
; Instfiles page
!insertmacro MUI_PAGE_INSTFILES
; Finish page
; We launch the desktop shortcut to set the working dir
!define MUI_FINISHPAGE_RUN
!define MUI_FINISHPAGE_RUN_TEXT "Start $(^Name)"
!define MUI_FINISHPAGE_RUN_FUNCTION "LaunchDolphin"
!insertmacro MUI_PAGE_FINISH

; Uninstaller pages
!insertmacro MUI_UNPAGE_INSTFILES

; Language files
!insertmacro MUI_LANGUAGE "Afrikaans"
!insertmacro MUI_LANGUAGE "Albanian"
!insertmacro MUI_LANGUAGE "Arabic"
!insertmacro MUI_LANGUAGE "Basque"
!insertmacro MUI_LANGUAGE "Belarusian"
!insertmacro MUI_LANGUAGE "Bosnian"
!insertmacro MUI_LANGUAGE "Breton"
!insertmacro MUI_LANGUAGE "Bulgarian"
!insertmacro MUI_LANGUAGE "Catalan"
!insertmacro MUI_LANGUAGE "Croatian"
!insertmacro MUI_LANGUAGE "Czech"
!insertmacro MUI_LANGUAGE "Danish"
!insertmacro MUI_LANGUAGE "Dutch"
!insertmacro MUI_LANGUAGE "English"
!insertmacro MUI_LANGUAGE "Estonian"
!insertmacro MUI_LANGUAGE "Farsi"
!insertmacro MUI_LANGUAGE "Finnish"
!insertmacro MUI_LANGUAGE "French"
!insertmacro MUI_LANGUAGE "Galician"
!insertmacro MUI_LANGUAGE "German"
!insertmacro MUI_LANGUAGE "Greek"
!insertmacro MUI_LANGUAGE "Hebrew"
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
!insertmacro MUI_LANGUAGE "Polish"
!insertmacro MUI_LANGUAGE "Portuguese"
!insertmacro MUI_LANGUAGE "PortugueseBR"
!insertmacro MUI_LANGUAGE "Romanian"
!insertmacro MUI_LANGUAGE "Russian"
!insertmacro MUI_LANGUAGE "Serbian"
!insertmacro MUI_LANGUAGE "SerbianLatin"
!insertmacro MUI_LANGUAGE "SimpChinese"
!insertmacro MUI_LANGUAGE "Slovak"
!insertmacro MUI_LANGUAGE "Slovenian"
!insertmacro MUI_LANGUAGE "Spanish"
!insertmacro MUI_LANGUAGE "SpanishInternational"
!insertmacro MUI_LANGUAGE "Swedish"
!insertmacro MUI_LANGUAGE "Thai"
!insertmacro MUI_LANGUAGE "TradChinese"
!insertmacro MUI_LANGUAGE "Turkish"
!insertmacro MUI_LANGUAGE "Ukrainian"
!insertmacro MUI_LANGUAGE "Uzbek"
!insertmacro MUI_LANGUAGE "Welsh"

; Reserve files
!insertmacro MUI_RESERVEFILE_INSTALLOPTIONS

; MUI end ------

Name "${PRODUCT_NAME} ${PRODUCT_VERSION}"
!define UN_NAME "Uninstall $(^Name)"
OutFile "Dolphin_Installer_win32.exe"
InstallDir "$PROGRAMFILES\$(^Name)"
InstallDirRegKey HKLM "${PRODUCT_DIR_REGKEY}" ""
ShowInstDetails show
ShowUnInstDetails show

Function .onInit
  !insertmacro MUI_LANGDLL_DISPLAY
FunctionEnd

Section "Base" SEC01
  SetShellVarContext all
  ; Dolphin exe and dlls
  ; TODO: cg is only for OGL, SDL is only for nJoy
  ; TODO: Make a nice subsection-ized display
  SetOutPath "$INSTDIR"
  SetOverwrite ifnewer
  File "${BASE_DIR}\Dolphin.exe"
  File "${BASE_DIR}\DolphinIL.exe"
  File "..\Externals\Cg\cg.dll"
  File "..\Externals\Cg\cgGL.dll"
  ; File "..\Externals\Cg\cgD3D9.dll"
  File "..\Externals\WiiUse\Win32\wiiuse.dll"
  File "..\Externals\SDL\win32\SDL.dll"
  File "..\Externals\OpenAL\win32\OpenAL32.dll"
  File "..\Externals\OpenAL\win32\wrap_oal.dll"
  ; This needs to be done after Dolphin.exe is copied
  CreateDirectory "$SMPROGRAMS\${PRODUCT_NAME}"
  CreateShortCut "$SMPROGRAMS\${PRODUCT_NAME}\Dolphin.lnk" "$INSTDIR\Dolphin.exe"
  CreateShortCut "$DESKTOP\Dolphin.lnk" "$INSTDIR\Dolphin.exe"
  CreateShortCut "$SMPROGRAMS\${PRODUCT_NAME}\DolphinIL.lnk" "$INSTDIR\DolphinIL.exe"
  CreateShortCut "$DESKTOP\DolphinIL.lnk" "$INSTDIR\DolphinIL.exe"

  ; Plugins
  SetOutPath "$INSTDIR\Plugins"
  SetOverwrite ifnewer
  File "${BASE_DIR}\Plugins\Plugin_DSP_HLE.dll"
  File "${BASE_DIR}\Plugins\Plugin_DSP_LLE.dll"
  File "${BASE_DIR}\Plugins\Plugin_nJoy_SDL.dll"
  File "${BASE_DIR}\Plugins\Plugin_nJoy_SDL_Test.dll"
  File "${BASE_DIR}\Plugins\Plugin_PadSimple.dll"
  File "${BASE_DIR}\Plugins\Plugin_VideoDX9.dll"
  File "${BASE_DIR}\Plugins\Plugin_VideoOGL.dll"
  File "${BASE_DIR}\Plugins\Plugin_Wiimote.dll"
  File "${BASE_DIR}\Plugins\Plugin_VideoSW.dll"

  ; GC/Wii static settings
  SetOutPath "$INSTDIR\Sys\GC"
  SetOverwrite ifnewer
  File "..\Data\Sys\GC\font_ansi.bin"
  File "..\Data\Sys\GC\font_sjis.bin"
  SetOutPath "$INSTDIR\Sys\Wii"
  SetOverwrite ifnewer
  File "..\Data\Sys\Wii\setting-eur.txt"
  File "..\Data\Sys\Wii\setting-jpn.txt"
  File "..\Data\Sys\Wii\setting-usa.txt"

  ; GC/Wii User settings
  SetOutPath "$INSTDIR\User\GC"
  SetOutPath "$INSTDIR\User\Wii\shared2\sys"
  SetOverwrite ifnewer
  File "..\Data\User\Wii\shared2\sys\readme.txt"
  File "..\Data\User\Wii\shared2\sys\SYSCONF"

  ; GameConfigs
  SetOutPath "$INSTDIR\User\GameConfig"
  SetOverwrite ifnewer
  File "..\Data\User\GameConfig\*.*"
SectionEnd

Section -AdditionalIcons
  CreateShortCut "$SMPROGRAMS\${PRODUCT_NAME}\${UN_NAME}.lnk" "$INSTDIR\uninst.exe"
SectionEnd

Section -Post
  WriteUninstaller "$INSTDIR\uninst.exe"
  WriteRegStr HKLM "${PRODUCT_DIR_REGKEY}" "" "$INSTDIR\Dolphin.exe"
  WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "DisplayName" "$(^Name)"
  WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "UninstallString" "$INSTDIR\uninst.exe"
  WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "DisplayIcon" "$INSTDIR\Dolphin.exe"
  WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "DisplayVersion" "${PRODUCT_VERSION}"
  WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "URLInfoAbout" "${PRODUCT_WEB_SITE}"
  WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "Publisher" "${PRODUCT_PUBLISHER}"
SectionEnd

; Section descriptions
!insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
  !insertmacro MUI_DESCRIPTION_TEXT ${SEC01} "It is recommended that you install all of the included files."
!insertmacro MUI_FUNCTION_DESCRIPTION_END


Function un.onUninstSuccess
  HideWindow
  MessageBox MB_ICONINFORMATION|MB_OK "$(^Name) was uninstalled successfully.$\r$\n\
    ATTENTION: You must manually delete$\r$\n$INSTDIR"
FunctionEnd

Function un.onInit
!insertmacro MUI_UNGETLANGUAGE
  MessageBox MB_ICONQUESTION|MB_YESNO|MB_DEFBUTTON2 "Are you sure you want to remove $(^Name)?" IDYES +2
  Abort
FunctionEnd

Section Uninstall
  SetShellVarContext all
  ; Only uninstall what we put there; all $INSTDIR\User is left as is
  Delete "$INSTDIR\uninst.exe"
  Delete "$INSTDIR\*.dll"
  Delete "$INSTDIR\Plugins\*.dll"
  Delete "$INSTDIR\Sys\Wii\setting-usa.txt"
  Delete "$INSTDIR\Sys\Wii\setting-jpn.txt"
  Delete "$INSTDIR\Sys\Wii\setting-eur.txt"
  Delete "$INSTDIR\Sys\GC\font_sjis.bin"
  Delete "$INSTDIR\Sys\GC\font_ansi.bin"
  Delete "$INSTDIR\Dolphin.exe"

  Delete "$SMPROGRAMS\${PRODUCT_NAME}\${UN_NAME}.lnk"
  Delete "$DESKTOP\Dolphin.lnk"
  Delete "$SMPROGRAMS\${PRODUCT_NAME}\Dolphin.lnk"
  Delete "$DESKTOP\DolphinIL.lnk"
  Delete "$SMPROGRAMS\${PRODUCT_NAME}\DolphinIL.lnk"

  RMDir "$SMPROGRAMS\${PRODUCT_NAME}"
  RMDir "$INSTDIR\Sys\GC"
  RMDir "$INSTDIR\Sys\Wii"
  RMDir "$INSTDIR\Sys"
  RMDir "$INSTDIR\Plugins"
  RMDir "$INSTDIR"

  DeleteRegKey ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}"
  DeleteRegKey HKLM "${PRODUCT_DIR_REGKEY}"
  SetAutoClose true
SectionEnd

Function LaunchDolphin
  ExecShell "" "$DESKTOP\$(^Name).lnk"
FunctionEnd