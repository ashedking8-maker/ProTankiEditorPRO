; ProTanki Editor PRO single-user Windows installer.
; This script deliberately has NO Start Menu folder selection page, performs no
; network calls or elevation. The optional Finish checkbox launches ONLY this editor.
Unicode true
!include "MUI2.nsh"
!ifndef INPUT_DIR
  !error "Pass /DINPUT_DIR=absolute-path-to-staged-install"
!endif
!ifndef OUTPUT_FILE
  !error "Pass /DOUTPUT_FILE=absolute-path-to-Setup.exe"
!endif
!ifndef ICON_FILE
  !error "Pass /DICON_FILE=absolute-path-to-icon.ico"
!endif
Name "ProTanki Editor PRO"
OutFile "${OUTPUT_FILE}"
InstallDir "$LOCALAPPDATA\Programs\ProTanki Editor PRO"
RequestExecutionLevel user
BrandingText "ProTanki Editor PRO | Created by Niss"
Icon "${ICON_FILE}"
UninstallIcon "${ICON_FILE}"
!define MUI_ICON "${ICON_FILE}"
!define MUI_UNICON "${ICON_FILE}"
!define MUI_FINISHPAGE_RUN "$INSTDIR\GTanksNextEditor.exe"
!define MUI_FINISHPAGE_RUN_TEXT "Launch ProTanki Editor PRO"
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_LANGUAGE "English"

; Current-user Add/Remove Programs entry. No admin elevation is required.
!define UNINSTALL_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\ProTankiEditorPRO"
Section "Install" SecMain
  SetShellVarContext current
  SetOutPath "$INSTDIR"
  File /r "${INPUT_DIR}\*.*"
  ; Remove shortcuts left by earlier 0.5.x installers before creating the one shortcut.
  Delete "$SMPROGRAMS\GTanks Next Editor\GTanks Next Editor.lnk"
  RMDir "$SMPROGRAMS\GTanks Next Editor"
  Delete "$SMPROGRAMS\ProTanki Editor PRO\ProTanki Editor PRO.lnk"
  RMDir "$SMPROGRAMS\ProTanki Editor PRO"
  Delete "$DESKTOP\GTanks Next Editor.lnk"
  ; Use the currently logged-in user's redirected Desktop (including OneDrive
  ; Desktop redirection). No Start-menu or Documents shortcuts are created.
  IfFileExists "$INSTDIR\GTanksNextEditor.exe" +2 0
    Abort "The editor executable was not installed."
  CreateShortcut "$DESKTOP\ProTanki Editor PRO.lnk" "$INSTDIR\GTanksNextEditor.exe"
  IfFileExists "$DESKTOP\ProTanki Editor PRO.lnk" +2 0
    Abort "Windows did not permit the Desktop shortcut to be created."
  WriteUninstaller "$INSTDIR\Uninstall.exe"
  ; Register in Settings > Apps and the legacy Programs and Features list.
  ; Keep user maps/library and per-user editor settings outside $INSTDIR untouched.
  WriteRegStr HKCU "${UNINSTALL_KEY}" "DisplayName" "ProTanki Editor PRO"
  WriteRegStr HKCU "${UNINSTALL_KEY}" "DisplayVersion" "0.5.18"
  WriteRegStr HKCU "${UNINSTALL_KEY}" "Publisher" "Niss"
  WriteRegStr HKCU "${UNINSTALL_KEY}" "InstallLocation" "$INSTDIR"
  WriteRegStr HKCU "${UNINSTALL_KEY}" "DisplayIcon" "$INSTDIR\GTanksNextEditor.exe,0"
  WriteRegStr HKCU "${UNINSTALL_KEY}" "UninstallString" '"$INSTDIR\Uninstall.exe"'
  WriteRegStr HKCU "${UNINSTALL_KEY}" "QuietUninstallString" '"$INSTDIR\Uninstall.exe" /S'
  WriteRegDWORD HKCU "${UNINSTALL_KEY}" "NoModify" 1
  WriteRegDWORD HKCU "${UNINSTALL_KEY}" "NoRepair" 1
SectionEnd
Section "Uninstall"
  SetShellVarContext current
  Delete "$DESKTOP\ProTanki Editor PRO.lnk"
  Delete "$DESKTOP\GTanks Next Editor.lnk"
  Delete "$SMPROGRAMS\GTanks Next Editor\GTanks Next Editor.lnk"
  RMDir "$SMPROGRAMS\GTanks Next Editor"
  Delete "$INSTDIR\GTanksNextEditor.exe"
  DeleteRegKey HKCU "${UNINSTALL_KEY}"
  Delete "$INSTDIR\Uninstall.exe"
  RMDir /r "$INSTDIR\assets"
  RMDir /r "$INSTDIR\docs"
  RMDir "$INSTDIR"
SectionEnd
