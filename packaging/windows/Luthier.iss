; Luthier for Windows: Inno Setup 6.3+ script (installer.md 1).
;
; Built by scripts/package_windows.ps1 from the products scripts/ci_build.ps1
; staged in dist\windows. Compile by hand with:
;   iscc /DAppVersion=1.0.0 /DStageDir=..\..\dist\windows /DOutputDir=..\..\dist\installers packaging\windows\Luthier.iss
; Add /DSign plus /Ssigntool="signtool.exe sign /f cert.pfx /p pass /fd sha256 /tr http://timestamp.digicert.com /td sha256 $f"
; to sign the installer and uninstaller.
;
; Layout:
;   VST3        {commoncf64}\VST3\Luthier.vst3          (C:\Program Files\Common Files\VST3, fixed)
;   CLAP        {commoncf64}\CLAP\Luthier.clap          (C:\Program Files\Common Files\CLAP, fixed)
;   Standalone  {autopf}\Luthier\Luthier.exe            (editable: /DIR=...)
;   Content     {commonappdata}\Luthier\Resources        (C:\ProgramData\Luthier, fixed: every
;               format finds it there - IrLibrary::searchForResources)
;   User data   Documents\Luthier - never written by the installer, kept on uninstall
;               unless the user ticks "remove user data" (installer.md 1.3).

#ifndef AppVersion
  #define AppVersion "0.0.0"
#endif
#ifndef StageDir
  #define StageDir "..\..\dist\windows"
#endif
#ifndef OutputDir
  #define OutputDir "..\..\dist\installers"
#endif

#define AppName "Luthier"
#define AppPublisher "Luthier Audio"
#define AppExe "Luthier.exe"

[Setup]
; Never change AppId: it is how an upgrade finds the previous install.
AppId={{6E2B7F4A-3C1D-4E8B-9A57-1F0C2D3E4B5A}
AppName={#AppName}
AppVersion={#AppVersion}
AppVerName={#AppName} {#AppVersion}
AppPublisher={#AppPublisher}
AppPublisherURL=https://luthieraudio.com
AppSupportURL=https://luthieraudio.com/support
VersionInfoVersion={#AppVersion}
DefaultDirName={autopf}\{#AppName}
DefaultGroupName={#AppName}
DisableProgramGroupPage=yes
LicenseFile=..\common\EULA.txt
OutputDir={#OutputDir}
OutputBaseFilename=Luthier-{#AppVersion}-Setup-win64
SetupIconFile={#StageDir}\luthier.ico
UninstallDisplayIcon={app}\{#AppExe}
UninstallDisplayName={#AppName} {#AppVersion}
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
; installer.md 1.1.6: required space + 200 MB.
ExtraDiskSpaceRequired=209715200
; A DAW holding the plug-in open blocks install and uninstall until it is closed
; (installer.md 1.3); the Restart Manager finds it by the files in use.
CloseApplications=yes
RestartApplications=no
ChangesAssociations=yes
; Reproducible output (installer.md 0.5) as far as Inno allows.
TimeStampsInUTC=yes
#ifdef Sign
SignTool=signtool
SignedUninstaller=yes
#endif

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"
Name: "french";  MessagesFile: "compiler:Languages\French.isl"
Name: "german";  MessagesFile: "compiler:Languages\German.isl"
Name: "spanish"; MessagesFile: "compiler:Languages\Spanish.isl"
Name: "japanese"; MessagesFile: "compiler:Languages\Japanese.isl"

[Types]
Name: "full";   Description: "Full installation"
Name: "custom"; Description: "Custom installation"; Flags: iscustom

[Components]
Name: "vst3";       Description: "VST3 plug-in";           Types: full custom; Flags: fixed
Name: "clap";       Description: "CLAP plug-in";           Types: full
Name: "standalone"; Description: "Standalone application"; Types: full
; Every format needs the factory content (impulse responses, guitars, parts...).
Name: "content";    Description: "Factory content (presets, IRs, guitars, parts, tunes)"; Types: full custom; Flags: fixed

[Tasks]
Name: "associate"; Description: "Open Luthier files (.luthierpreset, .luthierguitar, .luthiertune, .luthierloop, .luthierset, .midprofile) with Luthier"; Components: standalone
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; Components: standalone; Flags: unchecked

[InstallDelete]
; Upgrades replace the bundles wholesale so no file from an older version survives.
Type: filesandordirs; Name: "{commoncf64}\VST3\Luthier.vst3"
Type: files;          Name: "{commoncf64}\CLAP\Luthier.clap"
Type: filesandordirs; Name: "{commonappdata}\Luthier\Resources"

[Files]
Source: "{#StageDir}\Luthier.vst3\*"; DestDir: "{commoncf64}\VST3\Luthier.vst3"; Components: vst3; Flags: ignoreversion recursesubdirs createallsubdirs
#if FileExists(StageDir + "\Luthier.clap")
Source: "{#StageDir}\Luthier.clap"; DestDir: "{commoncf64}\CLAP"; Components: clap; Flags: ignoreversion
#endif
Source: "{#StageDir}\{#AppExe}"; DestDir: "{app}"; Components: standalone; Flags: ignoreversion
#if FileExists(StageDir + "\luthier-render.exe")
Source: "{#StageDir}\luthier-render.exe"; DestDir: "{app}"; Components: standalone; Flags: ignoreversion
#endif
Source: "{#StageDir}\luthier.ico"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#StageDir}\Resources\*"; DestDir: "{commonappdata}\Luthier\Resources"; Components: content; Flags: ignoreversion recursesubdirs createallsubdirs

; installer.md 7 (IN-34): a managed install can pre-place the policy file the plug-in
; reads (Policy::getPolicyFile): Setup.exe /VERYSILENT /POLICY=C:\path\luthier-policy.json
Source: "{param:POLICY|}"; DestDir: "{commonappdata}\Luthier"; DestName: "luthier-policy.json"; Flags: external ignoreversion; Check: HasPolicyParam

[Dirs]
Name: "{commonappdata}\Luthier"

[Icons]
Name: "{group}\{#AppName}"; Filename: "{app}\{#AppExe}"; Components: standalone
Name: "{group}\Uninstall {#AppName}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#AppName}"; Filename: "{app}\{#AppExe}"; Components: standalone; Tasks: desktopicon

[Registry]
; installer.md 1.2: install location and version for scripted management.
Root: HKLM; Subkey: "Software\Luthier"; ValueType: string; ValueName: "InstallPath"; ValueData: "{app}"; Flags: uninsdeletekey
Root: HKLM; Subkey: "Software\Luthier"; ValueType: string; ValueName: "Version"; ValueData: "{#AppVersion}"
Root: HKLM; Subkey: "Software\Luthier"; ValueType: string; ValueName: "ContentPath"; ValueData: "{commonappdata}\Luthier\Resources"
Root: HKLM; Subkey: "Software\Luthier"; ValueType: string; ValueName: "VST3Path"; ValueData: "{commoncf64}\VST3\Luthier.vst3"
; File associations (installer.md 1.1.9).
Root: HKA; Subkey: "Software\Classes\Luthier.File"; ValueType: string; ValueData: "Luthier file"; Flags: uninsdeletekey; Tasks: associate
Root: HKA; Subkey: "Software\Classes\Luthier.File\DefaultIcon"; ValueType: string; ValueData: "{app}\luthier.ico"; Tasks: associate
Root: HKA; Subkey: "Software\Classes\Luthier.File\shell\open\command"; ValueType: string; ValueData: """{app}\{#AppExe}"" ""%1"""; Tasks: associate
Root: HKA; Subkey: "Software\Classes\.luthierpreset\OpenWithProgids"; ValueType: string; ValueName: "Luthier.File"; ValueData: ""; Flags: uninsdeletevalue; Tasks: associate
Root: HKA; Subkey: "Software\Classes\.luthierguitar\OpenWithProgids"; ValueType: string; ValueName: "Luthier.File"; ValueData: ""; Flags: uninsdeletevalue; Tasks: associate
Root: HKA; Subkey: "Software\Classes\.luthiertune\OpenWithProgids"; ValueType: string; ValueName: "Luthier.File"; ValueData: ""; Flags: uninsdeletevalue; Tasks: associate
Root: HKA; Subkey: "Software\Classes\.luthierloop\OpenWithProgids"; ValueType: string; ValueName: "Luthier.File"; ValueData: ""; Flags: uninsdeletevalue; Tasks: associate
Root: HKA; Subkey: "Software\Classes\.luthierset\OpenWithProgids"; ValueType: string; ValueName: "Luthier.File"; ValueData: ""; Flags: uninsdeletevalue; Tasks: associate
Root: HKA; Subkey: "Software\Classes\.midprofile\OpenWithProgids"; ValueType: string; ValueName: "Luthier.File"; ValueData: ""; Flags: uninsdeletevalue; Tasks: associate

[Run]
; Optional launch, default off (installer.md 1.1.9).
Filename: "{app}\{#AppExe}"; Description: "{cm:LaunchProgram,{#AppName}}"; Components: standalone; Flags: nowait postinstall skipifsilent unchecked

; installer.md 1.1.9 (IN-14): a "What's new" link on the finished page, default off.
Filename: "https://luthieraudio.com/releases/{#AppVersion}"; Description: "What's new in {#AppName} {#AppVersion}"; Flags: shellexec nowait postinstall skipifsilent unchecked

[UninstallDelete]
Type: dirifempty; Name: "{commonappdata}\Luthier"

[Code]
var
  RemoveUserData: Boolean;

{ installer.md 1.1.7: installing over a newer version asks first. }
function CompareVersions(A, B: String): Integer;
var
  PA, PB: Int64;
begin
  if StrToVersion(A, PA) and StrToVersion(B, PB) then
    Result := ComparePackedVersion(PA, PB)
  else
    Result := CompareStr(A, B);
end;

{ True when /POLICY=<file> names an existing file. }
function HasPolicyParam(): Boolean;
begin
  Result := (ExpandConstant('{param:POLICY|}') <> '') and FileExists(ExpandConstant('{param:POLICY|}'));
end;

function InitializeSetup(): Boolean;
var
  Installed: String;
begin
  Result := True;
  if RegQueryStringValue(HKLM, 'Software\Luthier', 'Version', Installed) then
  begin
    if CompareVersions(Installed, '{#AppVersion}') > 0 then
      Result := SuppressibleMsgBox('Luthier ' + Installed + ' is installed, which is newer than ' +
        '{#AppVersion}. Replace it with this older version?', mbConfirmation, MB_YESNO, IDYES) = IDYES
    else if CompareVersions(Installed, '{#AppVersion}') = 0 then
      Result := SuppressibleMsgBox('Luthier {#AppVersion} is already installed. Reinstall it?',
        mbConfirmation, MB_YESNO, IDYES) = IDYES;
  end;
end;

{ installer.md 1.3: user data is kept unless the user explicitly asks. Silent
  uninstalls always keep it. }
procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
var
  UserData: String;
begin
  if CurUninstallStep = usUninstall then
  begin
    UserData := ExpandConstant('{userdocs}\Luthier');
    RemoveUserData := False;
    if (not UninstallSilent) and DirExists(UserData) then
      RemoveUserData := MsgBox('Also remove your Luthier user data?' + #13#10#13#10 +
        UserData + #13#10#13#10 +
        'This deletes your own presets, guitars, parts, tunes, loops, setlists and recordings. ' +
        'Choose No to keep them (recommended).', mbConfirmation, MB_YESNO or MB_DEFBUTTON2) = IDYES;
  end;
  if (CurUninstallStep = usPostUninstall) and RemoveUserData then
  begin
    DelTree(ExpandConstant('{userdocs}\Luthier'), True, True, True);
    DelTree(ExpandConstant('{userappdata}\Luthier'), True, True, True);
  end;
end;
