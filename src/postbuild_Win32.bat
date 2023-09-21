@echo off

echo Kopiere DLL
copy %1 %AppData%\TS3Client\plugins

echo.
echo Kopiere Icons
if not exist %AppData%\TS3Client\plugins\lh2mqtt mkdir %AppData%\TS3Client\plugins\lh2mqtt
copy icons\lh2mqtt\ %AppData%\TS3Client\plugins\lh2mqtt

echo.
echo Bereite Installer vor
if not exist %AppData%\TS3Client\plugins\installer mkdir %AppData%\TS3Client\plugins\installer
if not exist %AppData%\TS3Client\plugins\installer\plugins mkdir %AppData%\TS3Client\plugins\installer\plugins
if not exist %AppData%\TS3Client\plugins\installer\plugins\lh2mqtt mkdir %AppData%\TS3Client\plugins\installer\plugins\lh2mqtt
copy icons\lh2mqtt\*.png %AppData%\TS3Client\plugins\installer\plugins\lh2mqtt
copy icons\lh2mqtt\*.crt %AppData%\TS3Client\plugins\installer\plugins\lh2mqtt

echo.
echo Erstelle package.ini
type icons\lh2mqtt\package_%2_begin.tmpl > %AppData%\TS3Client\plugins\installer\package.ini
echo Version = 1.%2.%3 >> %AppData%\TS3Client\plugins\installer\package.ini
type icons\lh2mqtt\package_end.tmpl >> %AppData%\TS3Client\plugins\installer\package.ini

copy %AppData%\TS3Client\plugins\lh2mqtt_x86.dll %AppData%\TS3Client\plugins\installer\plugins\lh2mqtt_x86.dll
copy %AppData%\TS3Client\plugins\lh2mqtt_x64.dll %AppData%\TS3Client\plugins\installer\plugins\lh2mqtt_x64.dll

cd /d %AppData%\TS3Client\plugins\installer\

echo.
echo Loesche vorhandenen TS3-Plugin Installer
del /Q lh2mqtt_1_%2_%3.ts3_plugin

echo.
echo Erstelle TS3-Plugin Installer
"C:\Program Files\7-Zip\7z.exe" a -tzip lh2mqtt_1_%2_%3.ts3_plugin "plugins/lh2mqtt_x64.dll" "plugins/lh2mqtt_x86.dll" "plugins/lh2mqtt/*.png" "plugins/lh2mqtt/*.crt" "package.ini"

echo.
echo.
REM timeout /t 10
exit 0