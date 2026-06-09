; NSIS installer for Windows XP x86 compatible package
Unicode false
Name "LAN Games Deployer 1.0"
OutFile "output\LAN_Games_Deployer_Setup_v1.0_xp32.exe"
InstallDir "$PROGRAMFILES\LAN Games Deployer"
RequestExecutionLevel admin
ShowInstDetails show
ShowUnInstDetails show
XPStyle on

!define APP_EXE "LANGamesDeployerCpp.exe"
!define SRC_DIR "..\build-vs2017-xp32\Release"

Icon "..\..\assets\app_icon_xp.ico"
UninstallIcon "..\..\assets\app_icon_xp.ico"

Page directory
Page components
Page instfiles
UninstPage uninstConfirm
UninstPage instfiles

Section "Core Files (required)" SEC_CORE
  SectionIn RO
  SetOutPath "$INSTDIR"
  File "${SRC_DIR}\${APP_EXE}"

  SetOutPath "$INSTDIR\data"
  File /r "${SRC_DIR}\data\*.*"

  WriteUninstaller "$INSTDIR\Uninstall.exe"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\LAN Games Deployer" "DisplayName" "LAN Games Deployer"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\LAN Games Deployer" "UninstallString" "$INSTDIR\Uninstall.exe"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\LAN Games Deployer" "DisplayVersion" "1.0"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\LAN Games Deployer" "Publisher" "xEna"
SectionEnd

Section "Start Menu Shortcut" SEC_SM
  SetOutPath "$INSTDIR"
  CreateDirectory "$SMPROGRAMS\LAN Games Deployer"
  CreateShortcut "$SMPROGRAMS\LAN Games Deployer\LAN Games Deployer.lnk" "$INSTDIR\${APP_EXE}" "" "$INSTDIR\${APP_EXE}" 0
SectionEnd

Section /o "Desktop Shortcut" SEC_DS
  SetOutPath "$INSTDIR"
  CreateShortcut "$DESKTOP\LAN Games Deployer.lnk" "$INSTDIR\${APP_EXE}" "" "$INSTDIR\${APP_EXE}" 0
SectionEnd

Section "Uninstall"
  Delete "$DESKTOP\LAN Games Deployer.lnk"
  Delete "$SMPROGRAMS\LAN Games Deployer\LAN Games Deployer.lnk"
  RMDir "$SMPROGRAMS\LAN Games Deployer"

  RMDir /r "$INSTDIR\data"
  Delete "$INSTDIR\${APP_EXE}"
  Delete "$INSTDIR\Uninstall.exe"
  RMDir "$INSTDIR"

  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\LAN Games Deployer"
SectionEnd
