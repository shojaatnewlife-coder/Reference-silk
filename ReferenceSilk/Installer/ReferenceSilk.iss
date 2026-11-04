; Inno Setup script for reference Silk (free version, no license)
#define AppName "Reference Silk"
#define AppVersion "2.3.0"
#define Publisher "Reference"
#ifndef BuildDir
  #define BuildDir "..\build\ReferenceSilk_artefacts\Release"
#endif

[Setup]
AppId={{4B7D2E19-8C35-4A60-9F1E-2D6C8A5B7E31}
AppName={#AppName}
AppVersion={#AppVersion}
AppPublisher={#Publisher}
AppVerName={#AppName} {#AppVersion}
DefaultDirName={autopf}\{#Publisher}\{#AppName}
DisableDirPage=yes
DisableProgramGroupPage=yes
DisableReadyPage=yes
UninstallDisplayName={#AppName}
SetupIconFile=..\Resources\icon.ico
OutputDir=Output
OutputBaseFilename=Reference-Silk-{#AppVersion}-Windows-Setup
Compression=lzma2/ultra64
SolidCompression=yes
WizardStyle=modern
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
CloseApplications=yes

[Files]
Source: "{#BuildDir}\VST3\{#AppName}.vst3\*"; DestDir: "{commoncf64}\VST3\{#AppName}.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs

[InstallDelete]
; a fresh copy of the bundle on every update (removes v1.1)
Type: filesandordirs; Name: "{commoncf64}\VST3\{#AppName}.vst3"

[UninstallDelete]
Type: filesandordirs; Name: "{commoncf64}\VST3\{#AppName}.vst3"
