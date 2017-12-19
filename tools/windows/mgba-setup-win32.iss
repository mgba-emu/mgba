;For automation purposes it is highly recommended to copy the files from
;\tools\windows\ to the directory that contains the win32 distribution files!

;Set CurrentReleaseVersion to the number of the latest stable mGBA build.
#define CurrentReleaseVersion = '0.6.1'

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
LicenseFile=LICENSE.txt
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
VersionInfoCopyright=© 2013–2017 Jeffrey Pfau
VersionInfoProductName=mGBA
VersionInfoVersion={#AppVer}
Compression=lzma2/ultra64
SolidCompression=True
VersionInfoTextVersion={#AppVer}
VersionInfoProductVersion={#AppVer}
VersionInfoProductTextVersion={#AppVer}

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"
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
Source: "CHANGES.txt"; DestDir: "{app}\"; Flags: ignoreversion
Source: "LICENSE.txt"; DestDir: "{app}\"; Flags: ignoreversion
Source: "mGBA.exe"; DestDir: "{app}\"; Flags: ignoreversion
Source: "nointro.dat"; DestDir: "{app}\"; Flags: ignoreversion
Source: "README.html"; DestDir: "{app}\"; Flags: ignoreversion                        
Source: "shaders\agb001.shader\agb001.fs"; DestDir: "{app}\shaders\agb001.shader\"; Flags: ignoreversion
Source: "shaders\agb001.shader\manifest.ini"; DestDir: "{app}\shaders\agb001.shader\"; Flags: ignoreversion
Source: "shaders\ags001.shader\ags001-light.fs"; DestDir: "{app}\shaders\ags001.shader\"; Flags: ignoreversion
Source: "shaders\ags001.shader\ags001.fs"; DestDir: "{app}\shaders\ags001.shader\"; Flags: ignoreversion
Source: "shaders\ags001.shader\manifest.ini"; DestDir: "{app}\shaders\ags001.shader\"; Flags: ignoreversion
Source: "shaders\fish.shader\fish.fs"; DestDir: "{app}\shaders\fish.shader\"; Flags: ignoreversion
Source: "shaders\fish.shader\manifest.ini"; DestDir: "{app}\shaders\fish.shader\"; Flags: ignoreversion
Source: "shaders\gba-color.shader\gba-color.fs"; DestDir: "{app}\shaders\gba-color.shader\"; Flags: ignoreversion
Source: "shaders\gba-color.shader\manifest.ini"; DestDir: "{app}\shaders\gba-color.shader\"; Flags: ignoreversion
Source: "shaders\lcd.shader\lcd.fs"; DestDir: "{app}\shaders\lcd.shader\"; Flags: ignoreversion
Source: "shaders\lcd.shader\manifest.ini"; DestDir: "{app}\shaders\lcd.shader\"; Flags: ignoreversion
Source: "shaders\motion_blur.shader\manifest.ini"; DestDir: "{app}\shaders\motion_blur.shader\"; Flags: ignoreversion
Source: "shaders\motion_blur.shader\motion_blur.fs"; DestDir: "{app}\shaders\motion_blur.shader\"; Flags: ignoreversion
Source: "shaders\pixelate.shader\manifest.ini"; DestDir: "{app}\shaders\pixelate.shader\"; Flags: ignoreversion
Source: "shaders\scanlines.shader\manifest.ini"; DestDir: "{app}\shaders\scanlines.shader\"; Flags: ignoreversion
Source: "shaders\scanlines.shader\scanlines.fs"; DestDir: "{app}\shaders\scanlines.shader\"; Flags: ignoreversion
Source: "shaders\soften.shader\manifest.ini"; DestDir: "{app}\shaders\soften.shader\"; Flags: ignoreversion
Source: "shaders\soften.shader\soften.fs"; DestDir: "{app}\shaders\soften.shader\"; Flags: ignoreversion
Source: "shaders\vba_pixelate.shader\manifest.ini"; DestDir: "{app}\shaders\vba_pixelate.shader\"; Flags: ignoreversion
Source: "shaders\vba_pixelate.shader\vba_pixelate.fs"; DestDir: "{app}\shaders\vba_pixelate.shader\"; Flags: ignoreversion
Source: "shaders\vignette.shader\manifest.ini"; DestDir: "{app}\shaders\vignette.shader\"; Flags: ignoreversion
Source: "shaders\vignette.shader\vignette.fs"; DestDir: "{app}\shaders\vignette.shader\"; Flags: ignoreversion
Source: "shaders\wiiu.shader\manifest.ini"; DestDir: "{app}\shaders\wiiu.shader\"; Flags: ignoreversion
Source: "shaders\wiiu.shader\wiiu.fs"; DestDir: "{app}\shaders\wiiu.shader\"; Flags: ignoreversion
Source: "shaders\xbr-lv2.shader\manifest.ini"; DestDir: "{app}\shaders\xbr-lv2.shader\"; Flags: ignoreversion
Source: "shaders\xbr-lv2.shader\xbr.fs"; DestDir: "{app}\shaders\xbr-lv2.shader\"; Flags: ignoreversion
Source: "shaders\xbr-lv2.shader\xbr.vs"; DestDir: "{app}\shaders\xbr-lv2.shader\"; Flags: ignoreversion
Source: "shaders\xbr-lv3.shader\manifest.ini"; DestDir: "{app}\shaders\xbr-lv3.shader\"; Flags: ignoreversion
Source: "shaders\xbr-lv3.shader\xbr.fs"; DestDir: "{app}\shaders\xbr-lv3.shader\"; Flags: ignoreversion
Source: "shaders\xbr-lv3.shader\xbr.vs"; DestDir: "{app}\shaders\xbr-lv3.shader\"; Flags: ignoreversion
Source: "third-party\LICENSE.blip-buf"; DestDir: "{app}\third-party\"; Flags: ignoreversion
Source: "third-party\LICENSE.ffmpeg"; DestDir: "{app}\third-party\"; Flags: ignoreversion
Source: "third-party\LICENSE.imagemagick"; DestDir: "{app}\third-party\"; Flags: ignoreversion
Source: "third-party\LICENSE.inih"; DestDir: "{app}\third-party\"; Flags: ignoreversion
Source: "third-party\LICENSE.lame"; DestDir: "{app}\third-party\"; Flags: ignoreversion
Source: "third-party\LICENSE.libvpx"; DestDir: "{app}\third-party\"; Flags: ignoreversion
Source: "third-party\LICENSE.opus"; DestDir: "{app}\third-party\"; Flags: ignoreversion
Source: "third-party\LICENSE.qt5"; DestDir: "{app}\third-party\"; Flags: ignoreversion
Source: "third-party\LICENSE.sdl2"; DestDir: "{app}\third-party\"; Flags: ignoreversion
Source: "third-party\LICENSE.x264"; DestDir: "{app}\third-party\"; Flags: ignoreversion
Source: "third-party\LICENSE.xvid"; DestDir: "{app}\third-party\"; Flags: ignoreversion

[Icons]
Name: "{commonstartmenu}\mGBA"; Filename: "{app}\mGBA.exe"
Name: "{commondesktop}\mGBA"; Filename: "{app}\mGBA.exe"; Tasks: desktopicon

[Run]
Filename: "{app}\mGBA.exe"; Description: "{cm:LaunchProgram,mGBA}"; Flags: nowait postinstall skipifsilent
Filename: "{app}\README.html"; Description: "View README"; Flags: nowait postinstall skipifsilent unchecked; Languages: english
Filename: "{app}\README.html"; Description: "View README"; Flags: nowait postinstall skipifsilent unchecked; Languages: italian
Filename: "{app}\README.html"; Description: "View README"; Flags: nowait postinstall skipifsilent unchecked; Languages: spanish
Filename: "{app}\README.html"; Description: "README anzeigen"; Flags: nowait postinstall skipifsilent unchecked; Languages: german
Filename: "{app}\CHANGES.txt"; Description: "View Changelog"; Flags: nowait postinstall skipifsilent unchecked; Languages: english
Filename: "{app}\CHANGES.txt"; Description: "View Changelog"; Flags: nowait postinstall skipifsilent unchecked; Languages: italian
Filename: "{app}\CHANGES.txt"; Description: "View Changelog"; Flags: nowait postinstall skipifsilent unchecked; Languages: spanish
Filename: "{app}\CHANGES.txt"; Description: "Changelog anzeigen"; Flags: nowait postinstall skipifsilent unchecked; Languages: german

[Dirs]
Name: "{app}\shaders\"
Name: "{app}\shaders\agb001.shader\"
Name: "{app}\shaders\ags001.shader\"
Name: "{app}\shaders\fish.shader\"
Name: "{app}\shaders\gba-color.shader\"
Name: "{app}\shaders\lcd.shader\"
Name: "{app}\shaders\motion_blur.shader\"
Name: "{app}\shaders\pixelate.shader\"
Name: "{app}\shaders\scanlines.shader\"
Name: "{app}\shaders\soften.shader\"
Name: "{app}\shaders\vba_pixelate.shader\"
Name: "{app}\shaders\vignette.shader\"
Name: "{app}\shaders\wiiu.shader\"
Name: "{app}\shaders\xbr-lv2.shader\"
Name: "{app}\shaders\xbr-lv3.shader\"
Name: "{app}\third-party\"

[CustomMessages]
english.FileAssoc=Register file associations
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
        if ExpandConstant('{language}') = 'italian' then noReleaseWarning := 'You are about to install a development build of mGBA.' + #13#10#13#10 + 'Development builds may contain bugs that are not yet discovered. Please report any issues you can find to the GitHub project page.';
        if ExpandConstant('{language}') = 'spanish' then noReleaseWarning := 'You are about to install a development build of mGBA.' + #13#10#13#10 + 'Development builds may contain bugs that are not yet discovered. Please report any issues you can find to the GitHub project page.';
        if ExpandConstant('{language}') = 'german' then noReleaseWarning := 'Sie möchten eine Entwicklerversion von mGBA installieren.' + #13#10#13#10 + 'Entwicklerversionen können bislang noch nicht endeckte Fehler beinhalten. Bitte melden Sie alle Fehler, die Sie finden können, auf der GitHub-Projektseite.';
        MsgBox(noReleaseWarning, mbInformation, MB_OK);
      end;
  end;
end.