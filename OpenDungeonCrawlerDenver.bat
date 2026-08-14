@echo off
REM ---------------------------------------------------------------------------
REM Opens DungeonCrawlerDenver directly in UE 5.8, bypassing the project browser.
REM
REM Launching UnrealEditor.exe (or Unreal from the Epic launcher) with no
REM argument lands on the project browser instead of the project. The MCP server
REM is a PER-PROJECT setting, so in that state nothing ever listens on port 8000
REM and Claude cannot see the editor at all.
REM
REM Double-click this, or pin it to the taskbar, and that cannot happen.
REM ---------------------------------------------------------------------------

start "" "D:\UE\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe" "%~dp0DungeonCrawlerDenver.uproject"
