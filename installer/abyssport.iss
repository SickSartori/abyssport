; Inno Setup 6.6+ script for abyssport. Built by tools/make_package.py:
;   ISCC /DAppVersion=<x.y.z> /DSourceDir=<dist/abyssport-x.y.z> /DOutputDir=dist installer\abyssport.iss
; Installs for the current user (no administrator rights): the launcher makes the game beside itself, in {app}\game,
; and needs to write there. That folder (the game made from the player's copy, about 4 GB, and the saved games in it)
; is not Setup's: uninstalling leaves it.
;
; No [Code] section, as in mc3port's script: with one, antivirus heuristics took that installer for malware. So the
; copy of the game and the firmware file are asked for by the launcher on its first start, not here.

#ifndef AppVersion
  #define AppVersion "0.0.0"
#endif
#ifndef SourceDir
  #define SourceDir "..\dist\stage"
#endif
#ifndef OutputDir
  #define OutputDir "..\dist"
#endif

[Setup]
AppId={{3B7C9E41-6A2D-4F58-8E13-9D0A5C4B7F26}
AppName=abyssport
AppVersion={#AppVersion}
AppVerName=abyssport {#AppVersion}
AppPublisher=SickSartori
AppPublisherURL=https://github.com/SickSartori/abyssport
AppSupportURL=https://github.com/SickSartori/abyssport/issues
AppUpdatesURL=https://github.com/SickSartori/abyssport/releases
AppComments=Unofficial native Windows port of Uncharted: Golden Abyss (PS Vita). No game files included.
AppCopyright=GPL-2.0-or-later. Not affiliated with Sony Interactive Entertainment, Bend Studio or Naughty Dog.
VersionInfoVersion={#AppVersion}
VersionInfoDescription=abyssport setup
DefaultDirName={autopf}\abyssport
DefaultGroupName=abyssport
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
DisableWelcomePage=no
; The programs and tools take 0.6 GB; the game the launcher then makes needs about 9 GB while it is made.
ExtraDiskSpaceRequired=9663676416
SetupIconFile=..\res\abyssport.ico
UninstallDisplayIcon={app}\abyssport.exe
UninstallDisplayName=abyssport - Uncharted: Golden Abyss
WizardStyle=modern dark
ShowLanguageDialog=auto
CloseApplications=yes
RestartApplications=no
OutputDir={#OutputDir}
OutputBaseFilename=abyssport-{#AppVersion}-windows-x64-setup
; With /DSign (tools/make_package.py --sign): the installer and the uninstaller inside it are code signed.
#ifdef Sign
SignTool=sign
SignedUninstaller=yes
#endif
Compression=lzma2/max
SolidCompression=yes

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"
Name: "italian"; MessagesFile: "compiler:Languages\Italian.isl"

[Messages]
english.WelcomeLabel2=This will install [name/ver], an unofficial native Windows port of Uncharted: Golden Abyss (PS Vita).%n%nNo game files are included. On its first start the launcher asks for your own copy of the game (the PCSA00029 folder copied off your console) and the firmware file PSVUPDAT.PUP (3.74), and makes the game on this PC: once only, about 15 minutes and 9 GB of free space.%n%nThe game it makes and your saved games stay in the installation folder when you update or uninstall.%n%nPrivacy: abyssport collects no personal data. The only connection it makes on its own is the update check, which asks GitHub for the latest release when the launcher opens; the launcher's settings turn it off.
italian.WelcomeLabel2=Verrà installato [name/ver], un port nativo non ufficiale per Windows di Uncharted: L'abisso d'oro (PS Vita).%n%nNessun file del gioco è incluso. Al primo avvio il launcher chiede la tua copia del gioco (la cartella PCSA00029 copiata dalla console) e il file del firmware PSVUPDAT.PUP (3.74), e prepara il gioco su questo PC: una volta sola, circa 15 minuti e 9 GB di spazio libero.%n%nIl gioco preparato e i tuoi salvataggi restano nella cartella di installazione quando aggiorni o disinstalli.%n%nPrivacy: abyssport non raccoglie dati personali. L'unica connessione che fa da solo è il controllo degli aggiornamenti, che chiede a GitHub l'ultima versione quando si apre il launcher; si spegne dalle impostazioni del launcher.

[CustomMessages]
english.DesktopShortcut=Create a desktop shortcut
italian.DesktopShortcut=Crea un collegamento sul desktop
english.ShortcutName=Uncharted Golden Abyss (abyssport)
italian.ShortcutName=Uncharted Golden Abyss (abyssport)
english.StartNow=Start abyssport and prepare the game
italian.StartNow=Avvia abyssport e prepara il gioco

[Tasks]
Name: "desktopicon"; Description: "{cm:DesktopShortcut}"

[Files]
; Everything the package holds: the launcher, the runtime, the tools, Python, the compiler, the documents and the
; source (tools/make_package.py). No game file can be in it.
Source: "{#SourceDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{autoprograms}\{cm:ShortcutName}"; Filename: "{app}\abyssport.exe"
Name: "{autodesktop}\{cm:ShortcutName}"; Filename: "{app}\abyssport.exe"; Tasks: desktopicon

[Run]
; Offered on the last page; not after a silent install (see the note on [Code] above).
Filename: "{app}\abyssport.exe"; Description: "{cm:StartNow}"; Flags: nowait postinstall skipifsilent
