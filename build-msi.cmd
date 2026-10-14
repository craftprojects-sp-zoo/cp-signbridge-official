@echo off
rem SignBridge — pakiety MSI (po build.cmd). Wymaga WiX 5: .NET SDK + "dotnet tool install --global wix"
rem Wynik: dist\"SignBridge.msi" (jeden pakiet, obie części)
setlocal
set "ROOT=%~dp0"
if not exist "%ROOT%dist\SignBridge Installer.exe" (
  echo [!] Najpierw uruchom build.cmd
  exit /b 1
)
where wix >nul 2>nul || (
  echo [!] Brak narzedzia "wix". Zainstaluj .NET SDK, potem:  dotnet tool install --global wix
  exit /b 1
)
rem wersja z src\panel\resource.h (SB_VERSION_STR "a.b.c.d")
set "VER="
for /f "tokens=3 delims= " %%v in ('findstr /C:"#define SB_VERSION_STR" "%ROOT%src\panel\resource.h"') do set "VER=%%~v"
if "%VER%"=="" set "VER=0.0.0.0"
echo [.] Wersja %VER%
rem katalog repo bez koncowego ukosnika (ukosnik przed cudzyslowem psuje argument)
set "ROOTNB=%ROOT:~0,-1%"
wix build -arch x64 -pdbtype none -d Version=%VER% -d Root="%ROOTNB%" "%ROOT%installer\Package.wxs" -o "%ROOT%dist\SignBridge.msi" || exit /b 1
call "%ROOT%sign.cmd" "%ROOT%dist\SignBridge.msi" || exit /b 1
echo.
echo [+] Gotowe:
dir /b "%ROOT%dist\*.msi"
exit /b 0
