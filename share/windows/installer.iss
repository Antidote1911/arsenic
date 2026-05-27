; Arsenic Inno Setup installer script
; Called from CI with:
;   ISCC.exe /DAPPVERSION=X.Y.Z /DSRCDIR=<deploy_dir> share\windows\installer.iss

#ifndef APPVERSION
  #error "APPVERSION not defined – pass /DAPPVERSION=X.Y.Z on the command line"
#endif
#ifndef SRCDIR
  #error "SRCDIR not defined – pass /DSRCDIR=<path> on the command line"
#endif

#define APPNAME "Arsenic"
#define APPEXE  "arsenic.exe"
#define PUBLISHER "Arsenic"

[Setup]
AppName={#APPNAME}
AppVersion={#APPVERSION}
AppPublisher={#PUBLISHER}
AppId={{B4F2A3C1-7E8D-4F9A-B2C3-D4E5F6A7B8C9}
DefaultDirName={autopf64}\{#APPNAME}
DefaultGroupName={#APPNAME}
OutputDir=.
OutputBaseFilename=arsenic-{#APPVERSION}-windows-setup
SetupIconFile=arsenic.ico
UninstallDisplayIcon={app}\{#APPEXE}
Compression=lzma/ultra64
SolidCompression=yes
PrivilegesRequired=admin
ArchitecturesInstallIn64BitMode=x64compatible
DisableWelcomePage=no
DisableDirPage=no
DisableFinishedPage=no

[Languages]
Name: "french";  MessagesFile: "compiler:Languages\French.isl"
Name: "english"; MessagesFile: "compiler:Default.isl"

[Files]
Source: "{#SRCDIR}\*"; DestDir: "{app}"; Flags: recursesubdirs createallsubdirs

[Icons]
Name: "{group}\{#APPNAME}"; Filename: "{app}\{#APPEXE}"
Name: "{commondesktop}\{#APPNAME}"; Filename: "{app}\{#APPEXE}"

[Run]
Filename: "{app}\{#APPEXE}"; Description: "{cm:LaunchProgram,{#APPNAME}}"; Flags: nowait postinstall skipifsilent
