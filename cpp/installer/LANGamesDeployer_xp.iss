#define MyAppName "LAN Games Deployer"
#define MyAppVersion "1.1"
#define MyAppPublisher "xEna"
#define MyAppExeName "LANGamesDeployer.exe"
#define MySourceDir "..\\release\\LAN Games Deployer v1.1"

[Setup]
AppId={{3E88FB87-AB2A-4A3E-923E-6653F4B14729}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
VersionInfoVersion={#MyAppVersion}
DefaultDirName={pf}\\LAN Games Deployer
DefaultGroupName=LAN Games Deployer
DisableProgramGroupPage=yes
OutputBaseFilename=LAN_Games_Deployer_Setup_v1.1_xp32
OutputDir=.\\output
SetupIconFile=..\\..\\assets\\app_icon_xp.ico
UninstallDisplayIcon={app}\\{#MyAppExeName}
Compression=lzma
SolidCompression=yes
WizardStyle=classic
MinVersion=5.1
PrivilegesRequired=admin
AllowNoIcons=yes

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Files]
Source: "{#MySourceDir}\\{#MyAppExeName}"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#MySourceDir}\\data\\*"; DestDir: "{app}\\data"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\\LAN Games Deployer"; Filename: "{app}\\{#MyAppExeName}"; Check: ShouldCreateStartMenuIcon
Name: "{commondesktop}\\LAN Games Deployer"; Filename: "{app}\\{#MyAppExeName}"; Check: ShouldCreateDesktopIcon

[Run]
Filename: "{app}\\{#MyAppExeName}"; Description: "Launch LAN Games Deployer"; Flags: nowait postinstall skipifsilent

[Code]
var
  ShortcutPage: TWizardPage;
  DesktopShortcutCheck: TNewCheckBox;
  StartMenuShortcutCheck: TNewCheckBox;

procedure InitializeWizard;
begin
  ShortcutPage := CreateCustomPage(
    wpSelectDir,
    'Shortcut Options',
    'Choose which shortcuts should be created.'
  );

  StartMenuShortcutCheck := TNewCheckBox.Create(ShortcutPage);
  StartMenuShortcutCheck.Parent := ShortcutPage.Surface;
  StartMenuShortcutCheck.Left := ScaleX(0);
  StartMenuShortcutCheck.Top := ScaleY(8);
  StartMenuShortcutCheck.Width := ShortcutPage.SurfaceWidth;
  StartMenuShortcutCheck.Caption := 'Create Start Menu shortcut';
  StartMenuShortcutCheck.Checked := True;

  DesktopShortcutCheck := TNewCheckBox.Create(ShortcutPage);
  DesktopShortcutCheck.Parent := ShortcutPage.Surface;
  DesktopShortcutCheck.Left := ScaleX(0);
  DesktopShortcutCheck.Top := StartMenuShortcutCheck.Top + StartMenuShortcutCheck.Height + ScaleY(10);
  DesktopShortcutCheck.Width := ShortcutPage.SurfaceWidth;
  DesktopShortcutCheck.Caption := 'Create Desktop shortcut';
  DesktopShortcutCheck.Checked := False;
end;

function ShouldCreateDesktopIcon: Boolean;
begin
  Result := Assigned(DesktopShortcutCheck) and DesktopShortcutCheck.Checked;
end;

function ShouldCreateStartMenuIcon: Boolean;
begin
  Result := Assigned(StartMenuShortcutCheck) and StartMenuShortcutCheck.Checked;
end;
