; Arsenic NSIS installer script
; Called from CI with:
;   makensis /DAPPVERSION=X.Y.Z /DSRCDIR=<deploy_dir> share\windows\installer.nsi

!ifndef APPVERSION
  !error "APPVERSION not defined – pass /DAPPVERSION=X.Y.Z on the command line"
!endif
!ifndef SRCDIR
  !error "SRCDIR not defined – pass /DSRCDIR=<path> on the command line"
!endif

!define APPNAME "Arsenic"
!define APPEXE  "arsenic.exe"
!define REGKEY  "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAME}"

; MUI icon defines must come before !include "MUI2.nsh"
!define MUI_ICON   "arsenic.ico"
!define MUI_UNICON "arsenic.ico"

Name    "${APPNAME} ${APPVERSION}"
OutFile "arsenic-${APPVERSION}-windows-setup.exe"

InstallDir          "$PROGRAMFILES64\${APPNAME}"
InstallDirRegKey    HKLM "Software\${APPNAME}" ""
RequestExecutionLevel admin
SetCompressor       /SOLID lzma

!include "MUI2.nsh"

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

; French first → default selection in the language picker
!insertmacro MUI_LANGUAGE "French"
!insertmacro MUI_LANGUAGE "English"

; Show language selection dialog at startup; auto-selects system language
Function .onInit
    !insertmacro MUI_LANGDLL_DISPLAY
FunctionEnd

; ── Install ──────────────────────────────────────────────────────────────────
Section "Install"
    SetOutPath "$INSTDIR"
    File /r "${SRCDIR}\*"

    WriteRegStr HKLM "${REGKEY}" "DisplayName"     "${APPNAME} ${APPVERSION}"
    WriteRegStr HKLM "${REGKEY}" "UninstallString"  "$INSTDIR\Uninstall.exe"
    WriteRegStr HKLM "${REGKEY}" "DisplayVersion"   "${APPVERSION}"
    WriteRegStr HKLM "${REGKEY}" "Publisher"        "Arsenic"
    WriteRegStr HKLM "${REGKEY}" "DisplayIcon"      "$INSTDIR\${APPEXE}"

    WriteUninstaller "$INSTDIR\Uninstall.exe"

    CreateDirectory "$SMPROGRAMS\${APPNAME}"
    ; Use the icon embedded in arsenic.exe for Start Menu and Desktop shortcuts
    CreateShortCut "$SMPROGRAMS\${APPNAME}\${APPNAME}.lnk" \
        "$INSTDIR\${APPEXE}" "" "$INSTDIR\${APPEXE}" 0
    CreateShortCut "$DESKTOP\${APPNAME}.lnk" \
        "$INSTDIR\${APPEXE}" "" "$INSTDIR\${APPEXE}" 0
SectionEnd

; ── Uninstall ─────────────────────────────────────────────────────────────────
Section "Uninstall"
    RMDir  /r "$INSTDIR"
    Delete    "$SMPROGRAMS\${APPNAME}\${APPNAME}.lnk"
    RMDir     "$SMPROGRAMS\${APPNAME}"
    Delete    "$DESKTOP\${APPNAME}.lnk"
    DeleteRegKey HKLM "${REGKEY}"
SectionEnd
