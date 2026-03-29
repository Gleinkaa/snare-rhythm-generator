; Snare Rhythm Generator — Inno Setup installer script
; Requires Inno Setup 6+
; Run from repo root: iscc installer\windows\snare_installer.iss

#define AppName     "Snare Rhythm Generator"
#define AppVersion  "1.1"
#define AppPublisher "SnareGen"
#define AppURL      "https://github.com/Gleinkaa/snare-rhythm-generator"

; JUCE CMake output paths (Release build)
#define BuildDir    "..\..\build\SnareRhythmGenerator_artefacts\Release"
#define VST3Dir     BuildDir + "\VST3\SnareRhythmGenerator.vst3"
#define StandaloneExe BuildDir + "\Standalone\SnareRhythmGenerator.exe"

[Setup]
AppId                   = {{A3F2C1D8-5B4E-4F9A-8C7D-1E2F3A4B5C6D}
AppName                 = {#AppName}
AppVersion              = {#AppVersion}
AppPublisher            = {#AppPublisher}
AppPublisherURL         = {#AppURL}
AppSupportURL           = {#AppURL}
DefaultDirName          = {autopf}\SnareGen
DefaultGroupName        = {#AppName}
AllowNoIcons            = yes
OutputDir               = Output
OutputBaseFilename      = SnareRhythmGenerator-{#AppVersion}-Windows
Compression             = lzma2/ultra64
SolidCompression        = yes
WizardStyle             = modern
UninstallDisplayIcon    = {app}\SnareRhythmGenerator.exe
ArchitecturesInstallIn64BitMode = x64compatible
PrivilegesRequired      = lowest
PrivilegesRequiredOverridesAllowed = dialog

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Types]
Name: "full";    Description: "Full installation (VST3 + Standalone)"
Name: "vst3";    Description: "VST3 plugin only"
Name: "standalone"; Description: "Standalone app only"

[Components]
Name: "vst3";       Description: "VST3 Plugin"; Types: full vst3;       Flags: fixed
Name: "standalone"; Description: "Standalone Application"; Types: full standalone

[Tasks]
Name: "desktopicon"; Description: "Create a &desktop shortcut (Standalone)"; \
    Components: standalone; GroupDescription: "Additional icons:"

[Files]
; VST3 bundle (directory tree)
Source: "{#VST3Dir}\*"; \
    DestDir: "{commoncf64}\VST3\SnareRhythmGenerator.vst3"; \
    Components: vst3; Flags: ignoreversion recursesubdirs createallsubdirs

; Standalone exe
Source: "{#StandaloneExe}"; \
    DestDir: "{app}"; \
    Components: standalone; Flags: ignoreversion

[Icons]
Name: "{group}\{#AppName}";      Filename: "{app}\SnareRhythmGenerator.exe"; Components: standalone
Name: "{group}\Uninstall";       Filename: "{uninstallexe}"
Name: "{autodesktop}\{#AppName}"; Filename: "{app}\SnareRhythmGenerator.exe"; \
    Tasks: desktopicon; Components: standalone

[Run]
Filename: "{app}\SnareRhythmGenerator.exe"; \
    Description: "Launch {#AppName}"; \
    Flags: nowait postinstall skipifsilent; \
    Components: standalone

[UninstallDelete]
; Clean up VST3 bundle directory on uninstall
Type: filesandordirs; Name: "{commoncf64}\VST3\SnareRhythmGenerator.vst3"

[Messages]
FinishedLabel=Setup has finished installing [name].\n\nVST3 plugin installed to:\n  C:\Program Files\Common Files\VST3\SnareRhythmGenerator.vst3\n\nRescan plugins in your DAW to pick it up.
