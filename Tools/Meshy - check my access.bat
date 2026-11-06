@echo off
rem Double-click me. Checks whether your Meshy API key works and lists the animation catalog with prices.
rem This never spends credits. Put your key in Tools\.meshy_key first (one line, just the key).
cd /d "%~dp0.."
python Tools\meshy_animation_pull.py --list
echo.
echo If you see a list above, your key works. Nothing was charged.
pause
