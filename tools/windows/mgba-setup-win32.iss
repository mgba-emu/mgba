;For automation purposes it is highly recommended to copy the files from
;\tools\windows\ to the directory that contains the win32 distribution files!

;Set CurrentReleaseVersion to the number of the latest stable mGBA build.
#define CurrentReleaseVersion = '0.6.3'

#define VerMajor
#define VerMinor
#define VerRev
#define VerBuild
#define FullVersion=ParseVersion('mGBA.exe', VerMajor, VerMinor, VerRev, VerBuild)
#define AppVer = Str(VerMajor) + "." + Str(VerMinor) + "." + Str(VerRev)

[Setup]
SourceDir=.\
SetupIconFile=mgba-setupiconfile.ico
WizardImageFile=mgba-wizardimagefile.bmp

AppName=mGBA                   
AppVersion={#AppVer}
AppPublisher=Jeffrey Pfau
AppPublisherURL=https://mgba.io
AppSupportURL=https://mgba.io
AppUpdatesURL=https://mgba.io
AppReadmeFile=README.html
OutputDir=.\
DefaultDirName={pf}\mGBA
DefaultGroupName=mGBA
AllowNoIcons=yes
DirExistsWarning=no
ChangesAssociations=True
AppendDefaultDirName=False
UninstallDisplayIcon={app}\mGBA.exe
MinVersion=0,6.0
AlwaysShowDirOnReadyPage=True
UsePreviousSetupType=True
UsePreviousTasks=True
AlwaysShowGroupOnReadyPage=True
LicenseFile=LICENSE
#if CurrentReleaseVersion == AppVer;
  #define IsRelease = 'yes'
  AppVerName=mGBA {#AppVer}
  OutputBaseFilename=mGBA-{#AppVer}-win32
#elif CurrentReleaseVersion != AppVer;
  #define IsRelease = 'no'
  AppVerName=mGBA (Development build)
  OutputBaseFilename=mGBA-setup-latest-win32
  #endif
UsePreviousLanguage=False
DisableWelcomePage=False
VersionInfoDescription=mGBA is an open-source Game Boy Advance emulator
VersionInfoCopyright=© 2013–2018 Jeffrey Pfau
VersionInfoProductName=mGBA
VersionInfoVersion={#AppVer}
Compression=lzma2/ultra64
SolidCompression=True
VersionInfoTextVersion={#AppVer}
VersionInfoProductVersion={#AppVer}
VersionInfoProductTextVersion={#AppVer}

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"
Name: "french"; MessagesFile: "compiler:Languages\French.isl"
Name: "german"; MessagesFile: "compiler:Languages\German.isl"
Name: "italian"; MessagesFile: "compiler:Languages\Italian.isl"
Name: "spanish"; MessagesFile: "compiler:Languages\Spanish.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"
Name: "gbfileassoc"; Description: "{cm:AssocFileExtension,mGBA,Game Boy}"; GroupDescription: "{cm:FileAssoc}"
Name: "gbcfileassoc"; Description: "{cm:AssocFileExtension,mGBA,Game Boy Color}"; GroupDescription: "{cm:FileAssoc}"
Name: "gbafileassoc"; Description: "{cm:AssocFileExtension,mGBA,Game Boy Advance}"; GroupDescription: "{cm:FileAssoc}"

[Files]
Source: "mGBA.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "CHANGES"; DestDir: "{app}\"; Flags: ignoreversion isreadme
Source: "LICENSE"; DestDir: "{app}\"; Flags: ignoreversion
Source: "nointro.dat"; DestDir: "{app}\"; Flags: ignoreversion
Source: "README.html"; DestDir: "{app}\"; Flags: ignoreversion isreadme; Languages: english italian spanish
Source: "README_DE.html"; DestDir: "{app}\"; DestName: "LIESMICH.html"; Flags: ignoreversion isreadme; Languages: german                        
Source: "shaders\*"; DestDir: "{app}\shaders\"; Flags: ignoreversion recursesubdirs
Source: "licenses\*"; DestDir: "{app}\licenses\"; Flags: ignoreversion recursesubdirs

[Icons]
Name: "{commonstartmenu}\mGBA"; Filename: "{app}\mGBA.exe"
Name: "{commondesktop}\mGBA"; Filename: "{app}\mGBA.exe"; Tasks: desktopicon

[Run]
Filename: "{app}\mGBA.exe"; Description: "{cm:LaunchProgram,mGBA}"; Flags: nowait postinstall skipifsilent

[Dirs]
Name: "{app}"

[CustomMessages]
english.FileAssoc=Register file associations
french.FileAssoc=Register file associations
italian.FileAssoc=Register file associations
spanish.FileAssoc=Register file associations
german.FileAssoc=Dateierweiterungen registrieren

[Registry]
Root: HKCR; Subkey: ".gb"; ValueType: string; ValueName: ""; ValueData: "Game Boy ROM"; Flags: uninsdeletevalue; Tasks: gbfileassoc
Root: HKCR; Subkey: ".gb\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\mGBA.exe,0"; Tasks: gbfileassoc
Root: HKCR; Subkey: ".gb\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\mGBA.exe"" ""%1"""; Tasks: gbfileassoc
Root: HKCR; Subkey: ".gbc"; ValueType: string; ValueName: ""; ValueData: "Game Boy Color ROM"; Flags: uninsdeletevalue; Tasks: gbcfileassoc
Root: HKCR; Subkey: ".gbc\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\mGBA.exe,0"; Tasks: gbcfileassoc
Root: HKCR; Subkey: ".gbc\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\mGBA.exe"" ""%1"""; Tasks: gbcfileassoc
Root: HKCR; Subkey: ".gba"; ValueType: string; ValueName: ""; ValueData: "Game Boy Advance ROM"; Flags: uninsdeletevalue; Tasks: gbafileassoc
Root: HKCR; Subkey: ".gba\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\mGBA.exe,0"; Tasks: gbafileassocRoot: HKCR; Subkey: ".gba\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\mGBA.exe"" ""%1"""; Tasks: gbafileassoc

[Code]
var 
  noReleaseWarning: String;

procedure InitializeWizard();
  begin
      if ExpandConstant('{#IsRelease}') = 'no' then
        begin
        if ExpandConstant('{language}') = 'english' then noReleaseWarning := 'You are about to install a development build of mGBA.' + #13#10#13#10 + 'Development builds may contain bugs that are not yet discovered. Please report any issues you can find to the GitHub project page.';
        if ExpandConstant('{language}') = 'french' then noReleaseWarning := 'You are about to install a development build of mGBA.' + #13#10#13#10 + 'Development builds may contain bugs that are not yet discovered. Please report any issues you can find to the GitHub project page.';
        if ExpandConstant('{language}') = 'italian' then noReleaseWarning := 'You are about to install a development build of mGBA.' + #13#10#13#10 + 'Development builds may contain bugs that are not yet discovered. Please report any issues you can find to the GitHub project page.';
        if ExpandConstant('{language}') = 'spanish' then noReleaseWarning := 'You are about to install a development build of mGBA.' + #13#10#13#10 + 'Development builds may contain bugs that are not yet discovered. Please report any issues you can find to the GitHub project page.';
        if ExpandConstant('{language}') = 'german' then noReleaseWarning := 'Sie möchten eine Entwicklerversion von mGBA installieren.' + #13#10#13#10 + 'Entwicklerversionen können bislang noch nicht endeckte Fehler beinhalten. Bitte melden Sie alle Fehler, die Sie finden können, auf der GitHub-Projektseite.';
        MsgBox(noReleaseWarning, mbInformation, MB_OK);
      end;
  end;
end.