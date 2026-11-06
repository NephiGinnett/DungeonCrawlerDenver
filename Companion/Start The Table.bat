@echo off
rem Double-click me on the DM's PC to open the Table. Keep the window open while you play.
cd /d "%~dp0"
title The Table - Dungeon Crawler Denver
where node >nul 2>nul || (echo Node.js is needed: https://nodejs.org & start "" https://nodejs.org/en/download & pause & exit /b 1)
if not exist "node_modules\ws" call npm install --no-audit --no-fund
node server\table-server.js
pause
