@echo off
rem Play the game straight from the project: the main menu, then the Den. No editor window, no packaging.
rem Uses this project's latest build, so it is the quickest way to try a change. Pass a game id to jump straight
rem into it, e.g.  play_game.bat Climb   (Climb, Kart, Neon, Haunt).
set "ENGINE=D:\UE\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe"
set "PROJECT=%~dp0..\DungeonCrawlerDenver.uproject"
set "MAP="
if /I "%~1"=="Climb" set "MAP=/Engine/Maps/Entry?game=/Script/DungeonCrawlerDenver.DCCClimbGameMode"
if /I "%~1"=="Kart"  set "MAP=/Engine/Maps/Entry?game=/Script/DungeonCrawlerDenver.DCCKartGameMode"
if /I "%~1"=="Neon"  set "MAP=/Engine/Maps/Entry?game=/Script/DungeonCrawlerDenver.DCCNeonGameMode"
if /I "%~1"=="Haunt" set "MAP=/Engine/Maps/Entry?game=/Script/DungeonCrawlerDenver.DCCHauntGameMode"
start "" "%ENGINE%" "%PROJECT%" %MAP% -game -windowed -ResX=1600 -ResY=900 -log=PlayDev.log
