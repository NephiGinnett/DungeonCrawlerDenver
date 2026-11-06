@echo off
rem Double-click me once. Puts "Crawler Companion" and "System Voice Studio" icons on your desktop,
rem pointing at wherever this folder is right now. Run it again if you ever move the folder.
powershell -NoProfile -ExecutionPolicy Bypass -Command ^
  "$d='%~dp0'.TrimEnd('\'); $ws=New-Object -ComObject WScript.Shell; $desk=[Environment]::GetFolderPath('Desktop');" ^
  "foreach($s in @(@('Crawler Companion','','companion.ico'),@('System Voice Studio','studio','studio.ico'))){" ^
  "  $l=$ws.CreateShortcut((Join-Path $desk ($s[0]+'.lnk'))); $l.TargetPath=(Join-Path $d 'launch.bat'); $l.Arguments=$s[1];" ^
  "  $l.WorkingDirectory=$d; $l.IconLocation=(Join-Path $d ('assets\'+$s[2]))+',0'; $l.WindowStyle=1; $l.Save() }"
echo.
echo   Done! Look on your desktop for "Crawler Companion" and "System Voice Studio".
echo.
pause