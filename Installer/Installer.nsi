!define DOLPHIN_ARCH x64
!define BASE_INSTALL_DIR "$PROGRAMFILES64"
!define PRODUCT_NAME "Dolphin"
!define PRODUCT_VERSION 5.0

!define BASE_DIR "..\Binary\${DOLPHIN_ARCH}"

; HM NIS Edit Wizard helper defines
!define PRODUCT_PUBLISHER "Dolphin Team"
!define PRODUCT_WEB_SITE "https://dolphin-emu.org/"
!define PRODUCT_DIR_REGKEY "Software\Microsoft\Windows\CurrentVersion\App Paths\${PRODUCT_NAME}.exe"
!define PRODUCT_UNINST_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_NAME}"
!define PRODUCT_UNINST_ROOT_KEY "HKLM"

SetCompressor /SOLID lzma

!include "FileFunc.nsh"

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
!insertmacro MUI_PAGE_LICENSE "..\license.txt"
; Components page
!insertmacro MUI_PAGE_COMPONENTS
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

Name "${PRODUCT_NAME}"
!define UN_NAME "Uninstall $(^Name)"
OutFile "dolphin-${DOLPHIN_ARCH}-${PRODUCT_VERSION}.exe"
InstallDir "${BASE_INSTALL_DIR}\$(^Name)"
InstallDirRegKey HKLM "${PRODUCT_DIR_REGKEY}" ""
ShowInstDetails show
ShowUnInstDetails show

Function .onInit
  !insertmacro MUI_LANGDLL_DISPLAY
FunctionEnd

Section "Base" SEC01
  SectionIn RO
  SetShellVarContext all
  ; Dolphin exe and dlls
  ; TODO: Make a nice subsection-ized display
  SetOutPath "$INSTDIR"
  SetOverwrite ifnewer
  ; Only delete what we put there; all $INSTDIR\User is left as is
  Delete "$INSTDIR\uninst.exe"
  Delete "$INSTDIR\license.txt"
  Delete "$INSTDIR\*.dll"
  Delete "$INSTDIR\Dolphin.exe"
  Delete "$INSTDIR\DSPTool.exe"
  Delete "$INSTDIR\cpack_package_description.txt"
  Delete "$INSTDIR\qt.conf"
  RMDir /r "$INSTDIR\Sys"
  RMDir /r "$INSTDIR\Languages"

  File "${BASE_DIR}\Dolphin.exe"
  File "${BASE_DIR}\license.txt"
  File "${BASE_DIR}\*.dll"
  File /r "${BASE_DIR}\Languages"
  File /r "${BASE_DIR}\Sys"
  
  ; This needs to be done after Dolphin.exe is copied
  CreateDirectory "$SMPROGRAMS\${PRODUCT_NAME}"
  CreateShortCut "$SMPROGRAMS\${PRODUCT_NAME}\${PRODUCT_NAME}.lnk" "$INSTDIR\Dolphin.exe"
  CreateShortCut "$DESKTOP\${PRODUCT_NAME}.lnk" "$INSTDIR\Dolphin.exe"
  
  SetOutPath "$TEMP"
  SetOverwrite on
  File /r "dxredist"
  File /r "vcredist"
SectionEnd

Section "DirectX Runtime" SEC02
   DetailPrint "Running DirectX runtime setup..."
   ExecWait '"$TEMP\dxredist\DXSETUP.exe" /silent'
   DetailPrint "Finished DirectX runtime setup"
SectionEnd

Section "Visual C++ 2015 Redistributable" SEC03
   DetailPrint "Running Visual C++ 2015 Redistributable setup..."
   ExecWait '"$TEMP\vcredist\vc_redist.x64.exe" /install /quiet /norestart'
   DetailPrint "Finished Visual C++ 2015 Redistributable setup"
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
  WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "InstallLocation" "$INSTDIR"
  ${GetSize} "$INSTDIR" "/S=0K" $0 $1 $2
  IntFmt $0 "0x%08X" $0
  WriteRegDWORD ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "EstimatedSize" "$0"
  WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "Comments" "GameCube and Wii emulator"
SectionEnd

; Section descriptions
!insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
  !insertmacro MUI_DESCRIPTION_TEXT ${SEC01} "Installs all files required to run the Dolphin Emulator."
  !insertmacro MUI_DESCRIPTION_TEXT ${SEC02} "Installs the recommended DirectX runtime libraries that are needed by Dolphin."
  !insertmacro MUI_DESCRIPTION_TEXT ${SEC03} "Installs the required Visual C++ 2015 Redistributable that is needed by Dolphin."
!insertmacro MUI_FUNCTION_DESCRIPTION_END

Section Uninstall
  SetShellVarContext all
  ; Only uninstall what we put there; all $INSTDIR\User is left as is
  Delete "$INSTDIR\uninst.exe"
  Delete "$INSTDIR\license.txt"
  Delete "$INSTDIR\*.dll"
  Delete "$INSTDIR\Dolphin.exe"

  Delete "$SMPROGRAMS\${PRODUCT_NAME}\${UN_NAME}.lnk"
  Delete "$DESKTOP\${PRODUCT_NAME}.lnk"
  Delete "$SMPROGRAMS\${PRODUCT_NAME}\${PRODUCT_NAME}.lnk"

  RMDir "$SMPROGRAMS\${PRODUCT_NAME}"
  RMDir /r "$INSTDIR\Sys"
  RMDir /r "$INSTDIR\Languages"
  RMDir "$INSTDIR"

  DeleteRegKey ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}"
  DeleteRegKey HKLM "${PRODUCT_DIR_REGKEY}"
  SetAutoClose true
SectionEnd
