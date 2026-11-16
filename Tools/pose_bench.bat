@echo off
rem The pose bench as its own program: the crawler on a stage with the gear bench open, nothing else.
rem Saving writes Content\DCC\Bench\DCCBench.json, which the game reads on its next launch.
rem Uses the editor's engine in game mode, so it needs this project's editor build (no editor window opens).
set "ENGINE=D:\UE\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe"
if not exist "%ENGINE%" (
  echo Unreal Engine 5.8 not found at %ENGINE%
  pause
  exit /b 1
)
set "PROJECT=%~dp0..\DungeonCrawlerDenver.uproject"
start "" "%ENGINE%" "%PROJECT%" "/Engine/Maps/Entry?game=/Script/DungeonCrawlerDenver.DCCBenchGameMode" -game -windowed -ResX=1600 -ResY=900 -log=PoseBench.log
