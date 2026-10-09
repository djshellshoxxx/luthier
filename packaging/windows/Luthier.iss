; Luthier for Windows: Inno Setup 6.3+ script (installer.md 1).
;
; Built by scripts/package_windows.ps1 from the products scripts/ci_build.ps1
; staged in dist\windows. One edition per build: EditionSlug is Pro or Free, and
; the product name is "Luthier Pro" / "Luthier Free" - the name ci_build.ps1
; gives the staged Standalone, VST3 and CLAP files. Compile by hand with:
;   iscc /DEditionSlug=Pro /DAppVersion=1.0.0 /DStageDir=..\..\dist\windows /DOutputDir=..\..\dist\installers packaging\windows\Luthier.iss
; Add /DSign plus /Ssigntool="signtool.exe sign /f cert.pfx /p pass /fd sha256 /tr http://timestamp.digicert.com /td sha256 $f"
; to sign the installer and uninstaller.
;
; Layout (<Product> is the product name above):
;   VST3        {commoncf64}\VST3\<Product>.vst3        (C:\Program Files\Common Files\VST3, fixed)
;   CLAP        {commoncf64}\CLAP\<Product>.clap        (C:\Program Files\Common Files\CLAP, fixed)
;   Standalone  {autopf}\<Product>\<Product>.exe        (editable: /DIR=...)
;   Content     {commonappdata}\Luthier\Resources        (C:\ProgramData\Luthier, fixed: every
;               format finds it there - IrLibrary::searchForResources. The same
;               folder for both editions until IrLibrary reads edition::contentFolder.)
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

; Passed by scripts/package_windows.ps1 (/DEditionSlug=Pro or Free).
#ifndef EditionSlug
  #define EditionSlug "Pro"
#endif
#if !SameText(EditionSlug, "Pro") && !SameText(EditionSlug, "Free")
  #error EditionSlug must be Pro or Free
#endif

#define ProductName "Luthier " + EditionSlug
#define AppName ProductName
#define AppPublisher "Luthier Audio"
#define AppExe ProductName + ".exe"
; One file-type handler per edition, so installing or removing one never
; takes the other's associations with it.
#define ProgId "Luthier" + EditionSlug + ".File"

; VersionInfoVersion (the Windows file-version resource) must be numeric x.y.z[.w];
; strip any pre-release suffix (e.g. "-beta1") from AppVersion so a tag like
; 1.0.0-beta1 still compiles. AppVersion itself keeps the full string for display.
#define VersionNumeric AppVersion
#if Pos("-", VersionNumeric) > 0
  #define VersionNumeric Copy(VersionNumeric, 1, Pos("-", VersionNumeric) - 1)
#endif

[Setup]
; Never change an AppId: it is how an upgrade finds the previous install. Pro and
; Free are separate products with their own AppId, so they install, upgrade and
; uninstall independently (editions.md 6: Pro keeps the existing GUID).
#if SameText(EditionSlug, "Free")
AppId={{53A88AE9-17B2-4F9E-BCBC-307CB43EAB31}
#else
AppId={{6E2B7F4A-3C1D-4E8B-9A57-1F0C2D3E4B5A}
#endif
AppName={#AppName}
AppVersion={#AppVersion}
AppVerName={#AppName} {#AppVersion}
AppPublisher={#AppPublisher}
AppPublisherURL=https://luthieraudio.com
AppSupportURL=https://luthieraudio.com/support
VersionInfoVersion={#VersionNumeric}
DefaultDirName={autopf}\{#AppName}
DefaultGroupName={#AppName}
DisableProgramGroupPage=yes
LicenseFile=..\common\EULA.txt
OutputDir={#OutputDir}
OutputBaseFilename=Luthier-{#EditionSlug}-{#AppVersion}-Setup-win64
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
Name: "associate"; Description: "Open Luthier files (.luthierpreset, .luthierguitar, .luthiertune, .luthierloop, .luthierset, .midprofile) with {#AppName}"; Components: standalone
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; Components: standalone; Flags: unchecked

[InstallDelete]
; Upgrades replace the bundles wholesale so no file from an older version survives.
Type: filesandordirs; Name: "{commoncf64}\VST3\{#ProductName}.vst3"
Type: files;          Name: "{commoncf64}\CLAP\{#ProductName}.clap"
Type: filesandordirs; Name: "{commonappdata}\Luthier\Resources"

[Files]
Source: "{#StageDir}\{#ProductName}.vst3\*"; DestDir: "{commoncf64}\VST3\{#ProductName}.vst3"; Components: vst3; Flags: ignoreversion recursesubdirs createallsubdirs
#if FileExists(StageDir + "\" + ProductName + ".clap")
Source: "{#StageDir}\{#ProductName}.clap"; DestDir: "{commoncf64}\CLAP"; Components: clap; Flags: ignoreversion
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
; Per edition: HKLM\Software\Luthier\Pro and HKLM\Software\Luthier\Free.
Root: HKLM; Subkey: "Software\Luthier"; Flags: uninsdeletekeyifempty
Root: HKLM; Subkey: "Software\Luthier\{#EditionSlug}"; ValueType: string; ValueName: "InstallPath"; ValueData: "{app}"; Flags: uninsdeletekey
Root: HKLM; Subkey: "Software\Luthier\{#EditionSlug}"; ValueType: string; ValueName: "Version"; ValueData: "{#AppVersion}"
Root: HKLM; Subkey: "Software\Luthier\{#EditionSlug}"; ValueType: string; ValueName: "ContentPath"; ValueData: "{commonappdata}\Luthier\Resources"
Root: HKLM; Subkey: "Software\Luthier\{#EditionSlug}"; ValueType: string; ValueName: "VST3Path"; ValueData: "{commoncf64}\VST3\{#ProductName}.vst3"
; File associations (installer.md 1.1.9).
Root: HKA; Subkey: "Software\Classes\{#ProgId}"; ValueType: string; ValueData: "{#AppName} file"; Flags: uninsdeletekey; Tasks: associate
Root: HKA; Subkey: "Software\Classes\{#ProgId}\DefaultIcon"; ValueType: string; ValueData: "{app}\luthier.ico"; Tasks: associate
Root: HKA; Subkey: "Software\Classes\{#ProgId}\shell\open\command"; ValueType: string; ValueData: """{app}\{#AppExe}"" ""%1"""; Tasks: associate
Root: HKA; Subkey: "Software\Classes\.luthierpreset\OpenWithProgids"; ValueType: string; ValueName: "{#ProgId}"; ValueData: ""; Flags: uninsdeletevalue; Tasks: associate
Root: HKA; Subkey: "Software\Classes\.luthierguitar\OpenWithProgids"; ValueType: string; ValueName: "{#ProgId}"; ValueData: ""; Flags: uninsdeletevalue; Tasks: associate
Root: HKA; Subkey: "Software\Classes\.luthiertune\OpenWithProgids"; ValueType: string; ValueName: "{#ProgId}"; ValueData: ""; Flags: uninsdeletevalue; Tasks: associate
Root: HKA; Subkey: "Software\Classes\.luthierloop\OpenWithProgids"; ValueType: string; ValueName: "{#ProgId}"; ValueData: ""; Flags: uninsdeletevalue; Tasks: associate
Root: HKA; Subkey: "Software\Classes\.luthierset\OpenWithProgids"; ValueType: string; ValueName: "{#ProgId}"; ValueData: ""; Flags: uninsdeletevalue; Tasks: associate
Root: HKA; Subkey: "Software\Classes\.midprofile\OpenWithProgids"; ValueType: string; ValueName: "{#ProgId}"; ValueData: ""; Flags: uninsdeletevalue; Tasks: associate

[Run]
; Optional launch, default off (installer.md 1.1.9).
Filename: "{app}\{#AppExe}"; Description: "{cm:LaunchProgram,{#AppName}}"; Components: standalone; Flags: nowait postinstall skipifsilent unchecked
; installer.md 1.1.9: a "What's new" link on the finished page, unchecked (SPEC-SWEEP IN-14).
Filename: "https://luthieraudio.com/releases/{#AppVersion}"; Description: "Show what's new in {#AppName} {#AppVersion}"; Flags: shellexec postinstall skipifsilent unchecked nowait

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
  if RegQueryStringValue(HKLM, 'Software\Luthier\{#EditionSlug}', 'Version', Installed) then
  begin
    if CompareVersions(Installed, '{#AppVersion}') > 0 then
      Result := SuppressibleMsgBox('{#AppName} ' + Installed + ' is installed, which is newer than ' +
        '{#AppVersion}. Replace it with this older version?', mbConfirmation, MB_YESNO, IDYES) = IDYES
    else if CompareVersions(Installed, '{#AppVersion}') = 0 then
      Result := SuppressibleMsgBox('{#AppName} {#AppVersion} is already installed. Reinstall it?',
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
