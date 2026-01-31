; Plasmoid Visualizer - Inno Setup Script
; This script creates a Windows installer for the application

#define AppName "Plasmoid Visualizer"
#define AppVersion "1.0.2"
#define AppPublisher "Vorlie"
#define AppURL "https://github.com/vorlie/plasmoid-visualizer"
#define AppExeName "PlasmoidVisualizer.exe"

[Setup]
; App information
AppId={{8F5A7B3C-9D2E-4F1A-8C6B-5E3A9D7F2B4C}}
AppName={#AppName}
AppVersion={#AppVersion}
AppPublisher={#AppPublisher}
AppPublisherURL={#AppURL}
AppSupportURL={#AppURL}
AppUpdatesURL={#AppURL}
DefaultDirName={autopf}\{#AppName}
DefaultGroupName={#AppName}
AllowNoIcons=yes
; LicenseFile=LICENSE.txt
; InfoBeforeFile=README.txt
OutputDir=installer_output
OutputBaseFilename=PlasmoidVisualizer_Setup_{#AppVersion}
; SetupIconFile=icon.ico
Compression=lzma
SolidCompression=yes
WizardStyle=modern
PrivilegesRequired=admin
ArchitecturesAllowed=x64
ArchitecturesInstallIn64BitMode=x64

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
; Main executable
Source: "dist\{#AppExeName}"; DestDir: "{app}"; Flags: ignoreversion
; Runtime dependencies
Source: "dist\glew32.dll"; DestDir: "{app}"; Flags: ignoreversion
; Shaders
Source: "dist\shaders\*"; DestDir: "{app}\shaders"; Flags: ignoreversion recursesubdirs createallsubdirs
; Documentation (if exists)
; Source: "README.md"; DestDir: "{app}"; Flags: ignoreversion isreadme

[Icons]
Name: "{group}\{#AppName}"; Filename: "{app}\{#AppExeName}"
Name: "{group}\{cm:UninstallProgram,{#AppName}}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#AppName}"; Filename: "{app}\{#AppExeName}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#AppExeName}"; Description: "{cm:LaunchProgram,{#StringChange(AppName, '&', '&&')}}"; Flags: nowait postinstall skipifsilent

[UninstallDelete]
Type: filesandordirs; Name: "{userappdata}\PlasmoidVisualizer"
