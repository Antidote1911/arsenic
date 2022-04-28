#define AppName             "Arsenic"
#define AppVerName          "Arsenic"
#define AppVersion          "5.1"
#define AppExeName          "Arsenic"
#define AppDesc             "Arsenic"
#define AppCopyright        "Copyright 2022"
#define AppURL              "https://github.com/Antidote1911/arsenic"
#define AppGroupName        "Arsenic"
#define QtDir               GetEnv("QTDIR")
#define Mingw               GetEnv("MINGW")
#define AppID               "{AF273301-28D0-4DAB-A531-2021022C4C8B}"

[Setup]
AppName={#AppName}
AppVerName={#AppVerName}
AppCopyright={#AppCopyright}
AppId={{#AppID}
DefaultDirName={pf}\{#AppGroupName}
DefaultGroupName={#AppGroupName}
OutputBaseFilename={#AppName}-win32
OutputDir=..\Output
MinVersion=6.0
UserInfoPage=false
AppVersion={#AppVersion}
UninstallDisplayIcon={app}\arsenic_gui.exe
EnableDirDoesntExistWarning=false
DirExistsWarning=no
SolidCompression=true
LicenseFile=.\..\LICENSE

[Files]
Source: ..\release\*; DestDir: {app}; Flags: recursesubdirs      


[Icons]
Name: {group}\Arsenic; Filename: {app}\arsenic_gui.exe; IconFilename: {app}\Arsenic_gui.exe;
Name: {group}\Uninstall; Filename: {uninstallexe}; WorkingDir: {app};

;;; HKEY_CURRENT_USER\Software\arsenic

[UninstallDelete]
Type: filesandordirs; Name: {userappdata}\.arsenic
Type: dirifempty; Name: {app}
