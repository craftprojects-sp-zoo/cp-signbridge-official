@echo off
rem SignBridge — testy jednostkowe x86 i x64 (Build Tools for Visual Studio, jak build.cmd)
rem Wynik: kod 0, gdy wszystkie testy obu architektur przeszly. Czesc testow (prawa katalogow) wymaga administratora.
setlocal EnableDelayedExpansion
set "ROOT=%~dp0..\"
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (echo [!] Brak vswhere.exe & exit /b 1)
set "VSPATH="
for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSPATH=%%i"
if "%VSPATH%"=="" (echo [!] Brak narzedzi C++ & exit /b 1)
set "VCVARS=%VSPATH%\VC\Auxiliary\Build\vcvarsall.bat"
set "FAILED="
call :arch x86 x86   || set "FAILED=!FAILED! x86"
call :arch x64 amd64 || set "FAILED=!FAILED! x64"
if defined FAILED (echo [!] Testy nieudane:!FAILED! & exit /b 1)
echo [+] Testy x86 i x64: OK
exit /b 0

:arch
setlocal
echo.
echo ===== testy %1 =====
call "%VCVARS%" %2 >nul || exit /b 1
where ninja >nul 2>nul && (set "GEN=Ninja") || (set "GEN=NMake Makefiles")
cmake -S "%ROOT%." -B "%ROOT%build\tests-%1" -G "!GEN!" -DCMAKE_BUILD_TYPE=Release -DSB_BUILD_TESTS=ON || exit /b 1
cmake --build "%ROOT%build\tests-%1" --config Release --target sbtests || exit /b 1
ctest --test-dir "%ROOT%build\tests-%1" -V || exit /b 1
endlocal
exit /b 0
