; Windows installer (NSIS 3). Built in CI:
;   makensis -DPRODUCT="Kinetik KDSP-16" -DBIN=KinetikKDSP16 -DVERSION=0.6 -DOUTFILE=out.exe packaging/windows/installer.nsi
; Paths are relative to the repository root (makensis is run with /NOCD from the root, see the workflow).
Unicode true
SetCompressor /SOLID lzma
Name "${PRODUCT}"
OutFile "${OUTFILE}"
RequestExecutionLevel admin
!ifdef SIGN
  ; sign the uninstaller and the installer with the same certificate (scripts/sign-windows.sh)
  !uninstfinalize 'sh scripts/sign-windows.sh "%1"'
  !finalize 'sh scripts/sign-windows.sh "%1"'
!endif
InstallDir "$PROGRAMFILES64\Kinetik\${BIN}"
BrandingText "${PRODUCT} ${VERSION}"
VIProductVersion "${VERSION}.0.0"
VIAddVersionKey "ProductName" "${PRODUCT}"
VIAddVersionKey "FileVersion" "${VERSION}"
VIAddVersionKey "FileDescription" "${PRODUCT} installer"
VIAddVersionKey "LegalCopyright" "All rights reserved"

!include "MUI2.nsh"
!include "x64.nsh"
!define MUI_ABORTWARNING
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "EULA.txt"
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_LANGUAGE "English"
!insertmacro MUI_LANGUAGE "French"

!define UNINSTKEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\${BIN}"

Function .onInit
  ${IfNot} ${RunningX64}
    MessageBox MB_OK|MB_ICONSTOP "${PRODUCT} requires 64-bit Windows."
    Abort
  ${EndIf}
  SetRegView 64
FunctionEnd

Section "Install"
  SetRegView 64
  SetOutPath "$COMMONFILES64\VST3\${BIN}.vst3"
  File /r "bin\${BIN}.vst3\*.*"
  CreateDirectory "$COMMONFILES64\CLAP"
  SetOutPath "$COMMONFILES64\CLAP"
  File "bin\${BIN}.clap"
  SetOutPath "$INSTDIR"
  File "EULA.txt"
  File "THIRD_PARTY_NOTICES.txt"
  WriteUninstaller "$INSTDIR\Uninstall.exe"
  WriteRegStr HKLM "${UNINSTKEY}" "DisplayName" "${PRODUCT}"
  WriteRegStr HKLM "${UNINSTKEY}" "DisplayVersion" "${VERSION}"
  WriteRegStr HKLM "${UNINSTKEY}" "Publisher" "Kinetik"
  WriteRegStr HKLM "${UNINSTKEY}" "UninstallString" '"$INSTDIR\Uninstall.exe"'
  WriteRegStr HKLM "${UNINSTKEY}" "InstallLocation" "$INSTDIR"
  WriteRegDWORD HKLM "${UNINSTKEY}" "NoModify" 1
  WriteRegDWORD HKLM "${UNINSTKEY}" "NoRepair" 1
SectionEnd

Section "Uninstall"
  SetRegView 64
  RMDir /r "$COMMONFILES64\VST3\${BIN}.vst3"
  Delete "$COMMONFILES64\CLAP\${BIN}.clap"
  Delete "$INSTDIR\EULA.txt"
  Delete "$INSTDIR\THIRD_PARTY_NOTICES.txt"
  Delete "$INSTDIR\Uninstall.exe"
  RMDir "$INSTDIR"
  RMDir "$PROGRAMFILES64\Kinetik"
  DeleteRegKey HKLM "${UNINSTKEY}"
SectionEnd
