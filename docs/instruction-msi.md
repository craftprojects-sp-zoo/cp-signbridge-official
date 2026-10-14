# SignBridge — instalacja z wiersza poleceń (MSI i EXE)

Jeden pakiet dla serwera pulpitu zdalnego i komputerów z kartą — instaluje zawsze obie części
(dostawca podpisu + wtyczka pulpitu zdalnego) dla wszystkich użytkowników komputera. Wymaga uprawnień administratora.

## Budowanie (Windows z Build Tools)

```
cd C:\src\signbridge          (katalog z kodem źródłowym — przykład)
.\build.cmd
```
Wynik w `dist\`: `SignBridge Installer.exe` oraz `SignBridge.msi`
(MSI powstaje, gdy jest WiX 5: `dotnet tool install --global wix --version 5.0.2`).

## Podpis cyfrowy (UAC „Craftprojects Sp. z o.o.” zamiast „Unknown publisher”)

Nazwę wydawcy UAC bierze wyłącznie z podpisu cyfrowego, któremu komputer ufa. Dwie drogi:

* **Docelowo**: certyfikat code-signing od publicznego CA (np. Certum, token USB) — działa na każdym komputerze.
* **Za darmo, w domenie Active Directory**: własny certyfikat Craftprojects + zaufanie wgrane przez GPO:
  ```
  powershell -ExecutionPolicy Bypass -File tools\make-signing-cert.ps1     (raz, na komputerze budującym)
  ```
  Skrypt tworzy certyfikat „Craftprojects Sp. z o.o.”, zapisuje `dist\Craftprojects-CodeSigning.cer` (klucz
  publiczny) i kopię PFX (klucz prywatny — poza repo, menedżer haseł). Plik `.cer` wgraj GPO do **Zaufane główne
  urzędy certyfikacji** i **Zaufani wydawcy** (lub `certutil -addstore -f Root …` + `… TrustedPublisher …` na komputerze).
  Poza domeną (komputery bez tego certyfikatu) nadal „Unknown publisher”.

Po jednym lub drugim, na komputerze budującym:
```
$env:SB_SIGN_THUMBPRINT = "<odcisk SHA-1 Twojego certyfikatu, 40 znaków hex>"
.\build.cmd
```
`build.cmd` podpisze biblioteki, instalator i MSI (`sign.cmd`). Bez tej zmiennej build jest niepodpisany.
Uwaga: dla własnego certyfikatu znacznik czasu nie jest potrzebny; sign.cmd próbuje time.certum.pl — bez internetu
na komputerze budującym ustaw `$env:SB_SIGN_TIMESTAMP = "none"`.

## MSI — msiexec

| Co | Polecenie (CMD / PowerShell jako administrator) |
|---|---|
| Instalacja cicha | `msiexec /i "SignBridge.msi" /qn` |
| Instalacja z paskiem postępu, bez pytań | `msiexec /i "SignBridge.msi" /qb` |
| Instalacja z dziennikiem | `msiexec /i "SignBridge.msi" /qn /l*v C:\Temp\signbridge-install.log` |
| Aktualizacja | ta sama komenda z nowszym MSI (wyższy numer wersji podmienia w miejscu) |
| Odinstalowanie ciche | `msiexec /x "SignBridge.msi" /qn` |
| Odinstalowanie bez pliku MSI | `msiexec /x {ProductCode} /qn` — kod z `Get-Package "SignBridge"` lub z HKLM\…\Uninstall |
| Bez restartu, nawet gdy pliki były w użyciu | dodać `/norestart` (stare pliki znikają po najbliższym restarcie) |

Kody wyjścia: `0` OK, `3010` OK — wymagany restart, `1603` błąd (szczegóły w dzienniku `/l*v`
i w `C:\ProgramData\cp-signbridge\install.log`).

Przykład GPO / Intune / skrypt logowania:
```
msiexec /i "\\serwer\udzial\SignBridge.msi" /qn /norestart
```

## EXE — ten sam instalator bez okna

| Co | Polecenie (administrator) |
|---|---|
| Instalacja cicha | `"SignBridge Installer.exe" --install-all --quiet` |
| … ze skrótem na pulpicie (wszyscy użytkownicy) | `"SignBridge Installer.exe" --install-all --shortcut --quiet` |
| … z ustawioną rolą (domyślny widok panelu) | `… --install-all --role server --quiet` albo `--role workstation` |
| Odinstalowanie ciche | `"SignBridge Installer.exe" --uninstall-all --quiet` |
| Odinstalowanie z „Aplikacje i funkcje” | robi to Windows (`--uninstall`) |

Kody wyjścia: `0` OK, `2` błąd. Dziennik: `C:\ProgramData\cp-signbridge\install.log` (instalacja),
`%TEMP%\cp-signbridge-uninstall.log` (odinstalowanie).

Części osobno (tylko w szczególnych przypadkach): `--install-server`, `--install-workstation [--machine] [--no-shortcut]`
oraz odpowiednie `--uninstall-*`.

## Co zostaje zainstalowane

* `C:\Windows\System32\cpsignbridge.dll`, `C:\Windows\SysWOW64\cpsignbridge.dll` + rejestracja dostawcy CNG
  „SignBridge Key Storage Provider”
* `C:\Program Files\cp-signbridge\` — `SignBridge.exe`, `cpsignbridge-rdp.dll` (+ `x86\`)
* `HKLM\Software\Microsoft\Terminal Server Client\Default\AddIns\cpsignbridge` (+ `WOW6432Node`) — wtyczka mstsc
* `HKLM\…\Run\cp-signbridge-panel` — ikona przy zegarze po zalogowaniu każdego użytkownika
* `HKLM\Software\cp-signbridge` (Version, InstallDir, InstalledAt) i wpis w „Aplikacje i funkcje”
* `C:\ProgramData\cp-signbridge\logs\` — logi

Odinstalowanie usuwa wszystko powyżej (pliki w użyciu — po restarcie) oraz klucze `Software\cp-signbridge`
w profilach wszystkich użytkowników. Program nie łączy się z siecią poza kanałem wewnątrz sesji RDP.
