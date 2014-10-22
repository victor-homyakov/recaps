[Setup]
AppName=Recaps
AppVerName=Recaps
AppPublisher=gooli.org
DefaultDirName={pf}\Recaps
DisableDirPage=yes
DefaultGroupName=Recaps
DisableProgramGroupPage=yes
SetupIconFile=recaps.ico
InfoBeforeFile=readme.txt
Compression=lzma
SolidCompression=yes
AppMutex=recaps-D3E743A3-E0F9-47f5-956A-CD15C6548789
PrivilegesRequired=poweruser

[Files]
Source: "Release\recaps.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "readme.txt"; DestDir: "{app}"; Flags: ignoreversion
Source: "LICENSE"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{commonstartup}\Recaps"; Filename: "{app}\recaps.exe"

[Registry]
Root: HKCU; Subkey: "Software\Recaps"; Flags: uninsdeletekey

[Run]
Filename: "{app}\recaps.exe"; Description: "{cm:LaunchProgram,Recaps}"; Flags: nowait postinstall skipifsilent
