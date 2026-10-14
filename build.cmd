@echo off
rem SignBridge — budowanie (x86 + x64) narzędziami Visual Studio Build Tools
rem Wynik w dist\:
rem   "SignBridge Installer.exe"   — instalator z oknem (kreator) i trybem cichym; biblioteki wbudowane
rem   "SignBridge.msi"             — pakiet MSI (GPO / msiexec /qn), gdy jest WiX 5 (dotnet tool install --global wix --version 5.0.2)
rem Podpis Authenticode (UAC: "Craftprojects Sp. z o.o." zamiast "Unknown publisher"): ustaw SB_SIGN_THUMBPRINT — patrz sign.cmd
rem Narzędzia wiersza poleceń sbctl.exe / sbagent.exe zostają w build\x64\ (diagnostyka).
setlocal EnableDelayedExpansion
set "ROOT=%~dp0"

set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (
  echo [!] Brak vswhere.exe - zainstaluj "Build Tools for Visual Studio" z komponentem "Desktop development with C++".
  exit /b 1
)
set "VSPATH="
for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSPATH=%%i"
if "%VSPATH%"=="" (
  echo [!] Nie znaleziono narzedzi C++ w Visual Studio. Zainstaluj komponent "Desktop development with C++".
  exit /b 1
)
set "VCVARS=%VSPATH%\VC\Auxiliary\Build\vcvarsall.bat"
echo [.] Visual Studio: %VSPATH%
if not exist "%ROOT%build\embed" mkdir "%ROOT%build\embed"

rem 1) biblioteki x86 i x64 -> build\embed\
call :libs x86 x86   || exit /b 1
call :libs x64 amd64 || exit /b 1
rem 1b) podpis bibliotek PRZED wbudowaniem w panel (gdy SB_SIGN_THUMBPRINT)
for %%F in ("%ROOT%build\embed\*.dll") do call "%ROOT%sign.cmd" "%%F" || exit /b 1
rem 2) panel x64 (wbudowuje biblioteki z build\embed\) + narzedzia
call :panel || exit /b 1

if not exist "%ROOT%dist" mkdir "%ROOT%dist"
set "OUT=%ROOT%dist\SignBridge Installer.exe"
copy /y "%ROOT%build\x64\sbpanel.exe" "%OUT%" >nul || exit /b 1
call "%ROOT%sign.cmd" "%OUT%" || exit /b 1

rem 3) pakiety MSI (gdy zainstalowany WiX)
where wix >nul 2>nul && (call "%ROOT%build-msi.cmd" || exit /b 1) || (
  echo.
  echo [~] Brak narzedzia "wix" - pomijam pakiety MSI. Aby je budowac: zainstaluj .NET SDK, potem:  dotnet tool install --global wix
)
echo.
echo [+] Gotowe. Pliki w %ROOT%dist\:
dir /b "%ROOT%dist"
exit /b 0

:libs
setlocal
echo.
echo ===== biblioteki %1 =====
call "%VCVARS%" %2 >nul || (echo [!] vcvarsall %2 nieudane & exit /b 1)
where ninja >nul 2>nul && (set "GEN=Ninja") || (set "GEN=NMake Makefiles")
cmake -S "%ROOT%." -B "%ROOT%build\%1" -G "!GEN!" -DCMAKE_BUILD_TYPE=Release || exit /b 1
cmake --build "%ROOT%build\%1" --config Release --target cpsignbridge cpsignbridge-rdp || exit /b 1
copy /y "%ROOT%build\%1\cpsignbridge.dll" "%ROOT%build\embed\%1-cpsignbridge.dll" >nul || exit /b 1
copy /y "%ROOT%build\%1\cpsignbridge-rdp.dll" "%ROOT%build\embed\%1-cpsignbridge-rdp.dll" >nul || exit /b 1
endlocal
exit /b 0

:panel
setlocal
echo.
echo ===== panel x64 =====
call "%VCVARS%" amd64 >nul || exit /b 1
cmake --build "%ROOT%build\x64" --config Release || exit /b 1
endlocal
exit /b 0
