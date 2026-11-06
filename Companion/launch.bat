@echo off
rem ---------------------------------------------------------------------------
rem  One launcher for both apps. The shortcuts call it as:
rem      launch.bat            -> Crawler Companion
rem      launch.bat studio     -> System Voice Studio
rem  It sets itself up the first time (needs Node.js), then starts the app and
rem  closes this window. Nobody should ever need to type a command.
rem ---------------------------------------------------------------------------
setlocal
cd /d "%~dp0"
title Dungeon Crawler Denver

set "APPNAME=Crawler Companion"
set "APPARGS=."
if /i "%~1"=="studio" (
  set "APPNAME=System Voice Studio"
  set "APPARGS=. --studio"
)

where node >nul 2>nul
if errorlevel 1 (
  echo.
  echo   %APPNAME% needs one free program first: Node.js.
  echo.
  echo   Your browser will open the download page now.
  echo   Click the big green "LTS" button, run the installer, keep pressing Next,
  echo   then double-click this shortcut again.
  echo.
  start "" "https://nodejs.org/en/download"
  pause
  exit /b 1
)

if not exist "node_modules\electron\dist\electron.exe" (
  echo.
  echo   Setting up %APPNAME% for the first time.
  echo   This takes a minute or two and only happens once...
  echo.
  call npm install --no-audit --no-fund
  if errorlevel 1 (
    echo.
    echo   Setup didn't finish. Check you're online, then try the shortcut again.
    echo   If it keeps failing, send Zoe a photo of this window.
    pause
    exit /b 1
  )
)

start "" "node_modules\electron\dist\electron.exe" %APPARGS%
exit /b 0
