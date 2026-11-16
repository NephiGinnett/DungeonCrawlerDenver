# Build the Windows game your friends can run, then zip it.
#
#   powershell -ExecutionPolicy Bypass -File Tools\package_game.ps1
#
# Needs the editor closed. Takes about an hour. Writes:
#   D:\Builds\DCD\                          the unpacked game
#   D:\Builds\DungeonCrawlerDenver-<date>.zip
#   D:\Builds\SHARE_THIS.txt
#   D:\Builds\package_status.txt            PACKAGE_OK or PACKAGE_FAILED
# Voice lines are copied in as loose files (the pak cannot be the place they
# are read from). Do not pass -map=/Game/... from Git Bash: it rewrites /Game.

$ErrorActionPreference = 'Stop'
$Status = 'D:\Builds\package_status.txt'
$Log = 'D:\Builds\DCD_build.log'
$Archive = 'D:\Builds\DCD'
$Project = 'C:\Users\kmett\Documents\Unreal Projects\DungeonCrawlerDenver\DungeonCrawlerDenver.uproject'
$UAT = 'D:\UE\UE_5.8\Engine\Build\BatchFiles\RunUAT.bat'
$VoiceSrc = 'C:\Users\kmett\Documents\Unreal Projects\DungeonCrawlerDenver\SourceArt\Voice\NPC'
$Stamp = Get-Date -Format 'yyyy-MM-dd.HHmm'
$Zip = "D:\Builds\DungeonCrawlerDenver-$Stamp.zip"

function Write-Status([string]$Text) {
    Set-Content -Path $Status -Value $Text -Encoding ASCII
}

Remove-Item $Status -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force -Path $Archive | Out-Null

$Editor = Get-Process UnrealEditor -ErrorAction SilentlyContinue
if ($Editor) {
    Write-Status 'PACKAGE_FAILED editor is open'
    exit 1
}

# The build stamp (Session/DCCBuildStamp): the main menu shows it, and a host refuses a join from a
# different build. It goes in DefaultGame.ini as [DCCBuild] Stamp=..., replacing the last one.
$GameIni = Join-Path (Split-Path $Project) 'Config\DefaultGame.ini'
$IniText = [System.IO.File]::ReadAllText($GameIni)
$IniText = [regex]::Replace($IniText, '(?ms)^\[DCCBuild\]\r?\n.*?(?=^\[|\z)', '')
$IniText = $IniText.TrimEnd() + "`r`n`r`n[DCCBuild]`r`nStamp=$Stamp`r`n"
[System.IO.File]::WriteAllText($GameIni, $IniText)

# RunUAT.bat splits %* on spaces, so the project path has to arrive already quoted.
# The .bat path itself has no spaces, so cmd does not need a quoted command string
# (a leading quote makes cmd strip the first and last quote off the whole line).
$CookLine = "$UAT BuildCookRun -project=`"$Project`" -platform=Win64 -clientconfig=Development -build -cook -stage -pak -iostore -compressed -prereqs -archive -archivedirectory=`"$Archive`" -unattended -utf8output -nop4 > `"$Log`" 2>&1"
cmd /c $CookLine
if ($LASTEXITCODE -ne 0) {
    Write-Status "PACKAGE_FAILED cook exit $LASTEXITCODE (see $Log)"
    exit $LASTEXITCODE
}

# UAT archives to <Archive>\Windows. An older DungeonCrawlerDenver folder may
# still sit beside it; do not zip that one.
$Bundle = Join-Path $Archive 'Windows'
$GameRoot = Join-Path $Bundle 'DungeonCrawlerDenver'
if (-not (Test-Path (Join-Path $GameRoot 'Binaries\Win64\DungeonCrawlerDenver.exe'))) {
    Write-Status 'PACKAGE_FAILED staged game not found'
    exit 1
}

if (Test-Path $VoiceSrc) {
    $VoiceDest = Join-Path $GameRoot 'Voice\NPC'
    New-Item -ItemType Directory -Force -Path $VoiceDest | Out-Null
    & robocopy $VoiceSrc $VoiceDest /E /NFL /NDL /NJH /NJS /nc /ns /np | Out-Null
    if ($LASTEXITCODE -ge 8) {
        Write-Status "PACKAGE_FAILED voice copy exit $LASTEXITCODE"
        exit 1
    }
}

$HowTo = @'
DUNGEON CRAWLER DENVER - HOW TO PLAY TOGETHER
==============================================

Everyone needs this same zip. The build number is at the bottom of the main menu;
an older build will be told to update when it tries to join.

INSTALL (everyone)
  1. Unzip the folder anywhere you have a few free gigabytes.
  2. Run DungeonCrawlerDenver.exe.
     First launch may install the Microsoft Visual C++ runtime. Say yes.
     If Windows says "Windows protected your PC", click More info, then Run anyway.
  3. The game opens on the main menu.

PICK YOUR CRAWLER
  Click your name: CJ, Scott, Marcel, Vaughn, Kyle, or Zoe.
  Type that crawler's password in the box under the names.
  The starting password is: bidet
  The game remembers it on this PC after that.

THE HOST (one person, usually Zoe)
  1. Pick your crawler and click HOST A PARTY.
  2. Windows asks about the firewall. Tick BOTH Private and Public networks
     and click Allow. Many home networks count as Public, so Private alone
     can block your friends. If you missed it, fix it in Windows Security,
     Firewall, Allow an app: tick DungeonCrawlerDenver for Private and Public.
  3. The Den opens and the party menu shows your invites.
     Click Copy invite and paste the message into the group chat.
     Internet: the game asks your router to open its port. If it can, this
       code works for friends anywhere, nothing else to install.
     Tailscale: works from anywhere once everyone is on Tailscale.
     Same Wi-Fi: only works in the same house.

EVERYONE ELSE
  1. Copy the host's whole message (Ctrl+C). Do this before you click join.
  2. Pick your crawler, type the password, and click JOIN A PARTY.
  3. If it fails, the menu says why. Copy the message again and click join again.
     If it times out, your own firewall may be dropping the host's replies:
     the first time the game ran, Windows asked about the firewall. In
     Windows Security, Firewall, Allow an app, tick DungeonCrawlerDenver for
     both Private and Public (and delete any Block rules for it).

IN THE DEN
  Walk onto a coloured pad to vote for a game. Majority wins, and everyone goes.
  F2 opens the party menu again (invite, who's here, leave).

NOT IN THE SAME HOUSE
  First try the Internet invite. If the party menu says the router would not
  open the port (or your provider shares addresses), use Tailscale instead:
  everyone installs it from tailscale.com, signs into the same tailnet, and the
  host sends the Tailscale invite. Or the host forwards UDP port 7777 by hand.

UPDATES
  If you installed through the itch.io app, it updates the game for you.
  Open the itch app before a session so everyone has the same build.

CONTROLS
  WASD move, mouse look, left click attack or climb (hold, with Climbing equipped),
  E interact, Tab menu, F2 party, F3 pose bench, O loot boxes, P pause.
'@
Set-Content -Path (Join-Path $Bundle 'HOW_TO_PLAY_TOGETHER.txt') -Value $HowTo -Encoding ASCII

# The credits, and the file that tells the itch.io app what to launch.
$Credits = Join-Path (Split-Path $Project) 'CREDITS.txt'
if (Test-Path $Credits) { Copy-Item $Credits (Join-Path $Bundle 'CREDITS.txt') -Force }
$ItchToml = @'
[[actions]]
name = "play"
path = "DungeonCrawlerDenver.exe"
'@
Set-Content -Path (Join-Path $Bundle '.itch.toml') -Value $ItchToml -Encoding ASCII

if (Test-Path $Zip) { Remove-Item $Zip -Force }
# Zip the build itself, not the parent folder. *.pdb is the debugger symbols
# (hundreds of MB) and friends do not need it.
& (Join-Path $env:SystemRoot 'System32\tar.exe') -a -cf $Zip --exclude '*.pdb' -C $Bundle .
if ($LASTEXITCODE -ne 0) {
    Write-Status "PACKAGE_FAILED zip exit $LASTEXITCODE"
    exit $LASTEXITCODE
}

$Bytes = (Get-Item $Zip).Length
$Share = @"
Send this one file to Scott, CJ, Marcel, Vaughn and Kyle:

  $Zip
  Build $Stamp (shown at the bottom of the main menu; everyone needs the same one)
  $([math]::Round($Bytes / 1GB, 2)) GB

They unzip it and run DungeonCrawlerDenver.exe.
You click HOST A PARTY. They copy your invite message and click JOIN A PARTY.
The starting crawler password is bidet, unless you changed it.

Unpacked game (same contents): $Bundle
"@
Set-Content -Path 'D:\Builds\SHARE_THIS.txt' -Value $Share -Encoding ASCII
Write-Status "PACKAGE_OK $Zip $Bytes"
exit 0
