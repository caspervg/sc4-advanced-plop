!include "MUI2.nsh"
!include "LogicLib.nsh"
!include "nsDialogs.nsh"
!include "FileFunc.nsh"

!define APP_NAME "SC4 Custom Services"
!ifndef APP_VERSION
  !define APP_VERSION "dev"
!endif
!define SERVICE_REG_KEY "Software\SC4CustomServices"
!define UNINSTALL_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\SC4CustomServices"

Name "${APP_NAME} ${APP_VERSION}"
OutFile "SC4CustomServices-${APP_VERSION}-Setup.exe"
Unicode True
RequestExecutionLevel admin
ShowInstDetails show
ShowUninstDetails show

Var Dialog
Var GameRoot
Var SC4PluginsDir
Var HGameRoot
Var HPluginsDir
Var HBrowseGameRoot
Var HBrowsePluginsDir

!define MUI_ABORTWARNING

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "ThirdPartyNotices.txt"
Page Custom ConfigurePathsPage ConfigurePathsPageLeave
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_UNPAGE_FINISH

!insertmacro MUI_LANGUAGE "English"

Function .onInit
  SetShellVarContext current
  Call DetectDefaultGameRoot
  StrCpy $SC4PluginsDir "$DOCUMENTS\SimCity 4\Plugins"
FunctionEnd

Function DetectDefaultGameRoot
  ; SC4 is a 32-bit app, so read from the 32-bit registry view.
  StrCpy $GameRoot "$PROGRAMFILES32\SimCity 4 Deluxe Edition"
  SetRegView 32
  ReadRegStr $0 HKLM "SOFTWARE\Maxis\SimCity 4" "Install Dir"
  ${If} $0 != ""
    StrCpy $GameRoot $0
  ${EndIf}
FunctionEnd

Function ConfigurePathsPage
  nsDialogs::Create 1018
  Pop $Dialog
  ${If} $Dialog == error
    Abort
  ${EndIf}

  ${NSD_CreateLabel} 0u 0u 100% 18u "Choose where to install ${APP_NAME} files."
  ${NSD_CreateLabel} 0u 24u 100% 10u "SimCity 4 game root (contains Apps folder):"
  ${NSD_CreateDirRequest} 0u 36u 82% 12u "$GameRoot"
  Pop $HGameRoot
  ${NSD_CreateButton} 84% 36u 16% 12u "Browse..."
  Pop $HBrowseGameRoot
  ${NSD_OnClick} $HBrowseGameRoot OnBrowseGameRoot

  ${NSD_CreateLabel} 0u 56u 100% 10u "SimCity 4 Plugins directory:"
  ${NSD_CreateDirRequest} 0u 68u 82% 12u "$SC4PluginsDir"
  Pop $HPluginsDir
  ${NSD_CreateButton} 84% 68u 16% 12u "Browse..."
  Pop $HBrowsePluginsDir
  ${NSD_OnClick} $HBrowsePluginsDir OnBrowsePluginsDir

  nsDialogs::Show
FunctionEnd

Function OnBrowseGameRoot
  Pop $0
  nsDialogs::SelectFolderDialog "Select SimCity 4 game root folder" "$GameRoot"
  Pop $0
  ${If} $0 != error
    StrCpy $GameRoot $0
    ${NSD_SetText} $HGameRoot $GameRoot
  ${EndIf}
FunctionEnd

Function OnBrowsePluginsDir
  Pop $0
  nsDialogs::SelectFolderDialog "Select SimCity 4 Plugins folder" "$SC4PluginsDir"
  Pop $0
  ${If} $0 != error
    StrCpy $SC4PluginsDir $0
    ${NSD_SetText} $HPluginsDir $SC4PluginsDir
  ${EndIf}
FunctionEnd

Function ConfigurePathsPageLeave
  ${NSD_GetText} $HGameRoot $GameRoot
  ${NSD_GetText} $HPluginsDir $SC4PluginsDir

  ${If} $GameRoot == ""
    MessageBox MB_OK|MB_ICONEXCLAMATION "Game root cannot be empty."
    Abort
  ${EndIf}

  ${If} $SC4PluginsDir == ""
    MessageBox MB_OK|MB_ICONEXCLAMATION "Plugins directory cannot be empty."
    Abort
  ${EndIf}

  ${IfNot} ${FileExists} "$GameRoot\Apps\*.*"
    MessageBox MB_YESNO|MB_ICONQUESTION "Could not find '$GameRoot\Apps'. Continue anyway?" IDYES +2
    Abort
  ${EndIf}
FunctionEnd

Section "Install"
  SetShellVarContext current

  CreateDirectory "$GameRoot\Apps"
  SetOutPath "$GameRoot\Apps"
  File "PLACE_IN_YOUR_SC4_APPS_FOLDER\imgui.dll"

  CreateDirectory "$SC4PluginsDir"
  SetOutPath "$SC4PluginsDir"
  File "PLACE_IN_YOUR_PLUGINS_FOLDER\SC4CustomServices.dll"

  WriteUninstaller "$SC4PluginsDir\Uninstall-SC4CustomServices.exe"

  WriteRegDWORD HKLM "${SERVICE_REG_KEY}" "Version" 1
  WriteRegStr HKLM "${SERVICE_REG_KEY}" "GameRoot" "$GameRoot"
  WriteRegStr HKLM "${SERVICE_REG_KEY}" "PluginsDir" "$SC4PluginsDir"

  WriteRegStr HKLM "${UNINSTALL_KEY}" "DisplayName" "${APP_NAME} ${APP_VERSION}"
  WriteRegStr HKLM "${UNINSTALL_KEY}" "DisplayVersion" "${APP_VERSION}"
  WriteRegStr HKLM "${UNINSTALL_KEY}" "Publisher" "SC4 Custom Services"
  WriteRegStr HKLM "${UNINSTALL_KEY}" "InstallLocation" "$SC4PluginsDir"
  WriteRegStr HKLM "${UNINSTALL_KEY}" "UninstallString" "$\"$SC4PluginsDir\Uninstall-SC4CustomServices.exe$\""
  WriteRegDWORD HKLM "${UNINSTALL_KEY}" "NoModify" 1
  WriteRegDWORD HKLM "${UNINSTALL_KEY}" "NoRepair" 1
SectionEnd

Function un.onInit
  SetShellVarContext current
  ReadRegStr $GameRoot HKLM "${SERVICE_REG_KEY}" "GameRoot"
  ReadRegStr $SC4PluginsDir HKLM "${SERVICE_REG_KEY}" "PluginsDir"

  ${If} $GameRoot == ""
    ; SC4 is a 32-bit app, so read from the 32-bit registry view.
    StrCpy $GameRoot "$PROGRAMFILES32\SimCity 4 Deluxe Edition"
    SetRegView 32
    ReadRegStr $0 HKLM "SOFTWARE\Maxis\SimCity 4" "Install Dir"
    ${If} $0 != ""
      StrCpy $GameRoot $0
    ${EndIf}
  ${EndIf}
  ${If} $SC4PluginsDir == ""
    StrCpy $SC4PluginsDir "$DOCUMENTS\SimCity 4\Plugins"
  ${EndIf}
FunctionEnd

Section "Uninstall"
  SetShellVarContext current

  Delete "$GameRoot\Apps\imgui.dll"
  Delete "$SC4PluginsDir\SC4CustomServices.dll"
  Delete "$SC4PluginsDir\Uninstall-SC4CustomServices.exe"

  DeleteRegKey HKLM "${SERVICE_REG_KEY}"
  DeleteRegKey HKLM "${UNINSTALL_KEY}"
SectionEnd
