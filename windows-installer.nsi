; NSIS Installer Script for AK API Key Manager
; Creates a Windows installer with Start Menu shortcut and icon integration

!include "MUI2.nsh"
!include "FileFunc.nsh"

; General configuration
Name "AK API Key Manager"
OutFile "build\AK-Installer.exe"
Unicode True

; Default installation directory
InstallDir "$PROGRAMFILES64\AK"

; Request admin privileges
RequestExecutionLevel admin

; Slint environment variables
!define SLINT_ASSET_PATH "$INSTDIR\slint-assets"

; Interface settings
!define MUI_ICON "logo.ico"
!define MUI_UNICON "logo.ico"
!define MUI_WELCOMEFINISHPAGE_BITMAP "installer-sidebar.bmp"
!define MUI_HEADERIMAGE
!define MUI_HEADERIMAGE_RIGHT
!define MUI_HEADERIMAGE_BITMAP "installer-header.bmp"
!define MUI_ABORTWARNING

; Pages
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "LICENSE"
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

; Languages
!insertmacro MUI_LANGUAGE "English"

; Version Information
VIProductVersion "4.2.21.0"
VIAddVersionKey "ProductName" "AK API Key Manager"
VIAddVersionKey "CompanyName" "AK Development Team"
VIAddVersionKey "LegalCopyright" "© 2025 AK Development Team"
VIAddVersionKey "FileDescription" "AK API Key Manager Installer"
VIAddVersionKey "FileVersion" "4.2.21"
VIAddVersionKey "ProductVersion" "4.2.21"

; Create icon from PNG if needed
Function .onInit
  ; Check if icon exists, if not create it from PNG
  IfFileExists "logo.ico" IconExists
    File /oname=$PLUGINSDIR\logo.png "logo.png"
    ; Note: In a real script, you would use a plugin to convert PNG to ICO
    ; For this example, we're assuming the conversion happens elsewhere
    IconExists:
FunctionEnd

; Installation section
Section "Install"
  SetOutPath "$INSTDIR"
  
  ; Install application files
  File "build\ak.exe"
  File "build\ak-gui.exe"
  File "logo.png"
  File "LICENSE"
  File "README.md"
  
  ; Create icon directory if it doesn't exist
  CreateDirectory "$INSTDIR\icons"
  SetOutPath "$INSTDIR\icons"
  File "logo.png"
  
  ; Create Slint assets directory
  CreateDirectory "$INSTDIR\slint-assets"
  SetOutPath "$INSTDIR\slint-assets"
  ; Copy any Slint asset files (if they exist)
  File /nonfatal "build\slint-assets\*.*"
  
  ; Create batch script wrapper for GUI that sets Slint environment
  SetOutPath "$INSTDIR"
  FileOpen $0 "$INSTDIR\ak-gui-launcher.bat" w
  FileWrite $0 "@echo off$\r$\n"
  FileWrite $0 "set SLINT_ASSET_PATH=$INSTDIR\slint-assets$\r$\n"
  FileWrite $0 "start $INSTDIR\ak-gui.exe %*$\r$\n"
  FileClose $0
  
  ; Create Start Menu shortcut
  CreateDirectory "$SMPROGRAMS\AK API Key Manager"
  CreateShortcut "$SMPROGRAMS\AK API Key Manager\AK API Key Manager.lnk" "$INSTDIR\ak-gui-launcher.bat" "" "$INSTDIR\icons\logo.png" 0
  CreateShortcut "$SMPROGRAMS\AK API Key Manager\Uninstall.lnk" "$INSTDIR\uninstall.exe"
  
  ; Create Desktop shortcut
  CreateShortcut "$DESKTOP\AK API Key Manager.lnk" "$INSTDIR\ak-gui-launcher.bat" "" "$INSTDIR\icons\logo.png" 0
  
  ; Add to PATH environment variable
  EnVar::SetHKLM
  EnVar::AddValue "PATH" "$INSTDIR"
  
  ; Write uninstaller
  WriteUninstaller "$INSTDIR\uninstall.exe"
  
  ; Write registry keys for uninstall
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\AK" "DisplayName" "AK API Key Manager"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\AK" "UninstallString" "$\"$INSTDIR\uninstall.exe$\""
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\AK" "QuietUninstallString" "$\"$INSTDIR\uninstall.exe$\" /S"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\AK" "InstallLocation" "$INSTDIR"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\AK" "DisplayIcon" "$INSTDIR\icons\logo.png"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\AK" "Publisher" "AK Development Team"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\AK" "DisplayVersion" "4.2.21"
  
  ${GetSize} "$INSTDIR" "/S=0K" $0 $1 $2
  IntFmt $0 "0x%08X" $0
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\AK" "EstimatedSize" "$0"
SectionEnd

; Uninstallation section
Section "Uninstall"
  ; Remove application files
  Delete "$INSTDIR\ak.exe"
  Delete "$INSTDIR\ak-gui.exe"
  Delete "$INSTDIR\ak-gui-launcher.bat"
  Delete "$INSTDIR\logo.png"
  Delete "$INSTDIR\LICENSE"
  Delete "$INSTDIR\README.md"
  
  ; Remove icon directory and files
  Delete "$INSTDIR\icons\logo.png"
  RMDir "$INSTDIR\icons"
  
  ; Remove Slint assets
  RMDir /r "$INSTDIR\slint-assets"
  
  ; Remove Start Menu shortcuts
  Delete "$SMPROGRAMS\AK API Key Manager\AK API Key Manager.lnk"
  Delete "$SMPROGRAMS\AK API Key Manager\Uninstall.lnk"
  RMDir "$SMPROGRAMS\AK API Key Manager"
  
  ; Remove Desktop shortcut
  Delete "$DESKTOP\AK API Key Manager.lnk"
  
  ; Remove from PATH environment variable
  EnVar::SetHKLM
  EnVar::DeleteValue "PATH" "$INSTDIR"
  
  ; Remove uninstaller
  Delete "$INSTDIR\uninstall.exe"
  
  ; Remove installation directory if empty
  RMDir "$INSTDIR"
  
  ; Remove registry keys
  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\AK"
SectionEnd