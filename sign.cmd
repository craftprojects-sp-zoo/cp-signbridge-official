@echo off
rem Podpis Authenticode pliku (exe / dll / msi). Uzywane przez build.cmd, gdy ustawiona jest zmienna
rem   SB_SIGN_THUMBPRINT  = odcisk SHA-1 certyfikatu code-signing w magazynie uzytkownika (token Certum / karta)
rem opcjonalnie:
rem   SB_SIGN_TIMESTAMP   = serwer znacznika czasu (domyslnie http://time.certum.pl)
rem Bez SB_SIGN_THUMBPRINT nic nie robi (build bez podpisu = "Unknown publisher" w UAC).
if "%SB_SIGN_THUMBPRINT%"=="" exit /b 0
if "%SB_SIGN_TIMESTAMP%"=="" set "SB_SIGN_TIMESTAMP=http://time.certum.pl"
rem signtool: SB_SIGNTOOL (pelna sciezka) albo PATH albo najnowszy Windows Kits\10\bin\10.*\x64
set "SIGNTOOL=%SB_SIGNTOOL%"
if not defined SIGNTOOL for /f "delims=" %%p in ('where signtool.exe 2^>nul') do if not defined SIGNTOOL set "SIGNTOOL=%%p"
if not defined SIGNTOOL for /d %%d in ("%ProgramFiles(x86)%\Windows Kits\10\bin\10.*") do if exist "%%d\x64\signtool.exe" set "SIGNTOOL=%%d\x64\signtool.exe"
if "%SIGNTOOL%"=="" (echo [!] Brak signtool.exe ^(Windows SDK^) & exit /b 1)
if /i "%SB_SIGN_TIMESTAMP%"=="none" (
  "%SIGNTOOL%" sign /sha1 %SB_SIGN_THUMBPRINT% /fd sha256 /d "SignBridge" /du "https://craftprojects.pl" %1 || exit /b 1
) else (
  "%SIGNTOOL%" sign /sha1 %SB_SIGN_THUMBPRINT% /fd sha256 /td sha256 /tr %SB_SIGN_TIMESTAMP% /d "SignBridge" /du "https://craftprojects.pl" %1 || exit /b 1
)
echo [+] podpisano: %~nx1
exit /b 0
