# Package the game and push it to itch.io, where the itch app updates everyone's copy.
#
#   powershell -ExecutionPolicy Bypass -File Tools\publish_itch.ps1
#   powershell -ExecutionPolicy Bypass -File Tools\publish_itch.ps1 -SkipPackage   # push the last build again
#
# Needs the editor closed (packaging). The first time, run  D:\Tools\butler\butler.exe login  once and approve it in
# the browser. The itch page is Tools\itch_target.txt: "username/project-name:windows".
# Only the changed parts upload, so after the first push an update is quick.
param([switch]$SkipPackage)

$ErrorActionPreference = 'Stop'
$Root = Split-Path $PSScriptRoot
$Butler = 'D:\Tools\butler\butler.exe'
$TargetFile = Join-Path $PSScriptRoot 'itch_target.txt'
$Bundle = 'D:\Builds\DCD\Windows'

if (-not (Test-Path $TargetFile)) { throw "Write your itch page into $TargetFile, like: yourname/dungeon-crawler-denver:windows" }
$Target = (Get-Content $TargetFile -Raw).Trim()

if (-not $SkipPackage) {
    & powershell -ExecutionPolicy Bypass -File (Join-Path $PSScriptRoot 'package_game.ps1')
    $Status = Get-Content 'D:\Builds\package_status.txt' -Raw
    if ($Status -notmatch 'PACKAGE_OK') { throw "Packaging failed: $Status" }
}

# The build stamp the package wrote, so the itch app shows the same number as the main menu.
$Ini = Get-Content (Join-Path $Root 'Config\DefaultGame.ini') -Raw
$Stamp = if ($Ini -match '(?m)^Stamp=(.+)$') { $Matches[1].Trim() } else { Get-Date -Format 'yyyy-MM-dd.HHmm' }

& $Butler push $Bundle $Target --userversion $Stamp --ignore '*.pdb' --ignore 'DungeonCrawlerDenver/Saved' --ignore 'Manifest_*.txt'
if ($LASTEXITCODE -ne 0) { throw "butler push failed ($LASTEXITCODE)" }
& $Butler status $Target
Write-Host "Pushed build $Stamp to $Target. The itch app will offer it to everyone."
