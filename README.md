<p align="center">
  <picture>
    <source media="(prefers-color-scheme: dark)" srcset="docs/img/logo-dark.svg">
    <img src="docs/img/logo.svg" width="128" alt="SignBridge logo">
  </picture>
</p>

<h1 align="center">SignBridge</h1>

<p align="center">
  <b>Podpis kwalifikowany kartą lub tokenem w sesjach pulpitu zdalnego (RDP / RDS)</b><br>
  <i>Qualified e-signatures with a local smart card inside Remote Desktop sessions</i>
</p>

<p align="center">
  <img alt="Wersja 1.1.1" src="https://img.shields.io/badge/wersja-1.1.1-2563eb">
  <img alt="Licencja MIT" src="https://img.shields.io/badge/licencja-MIT-16a34a">
  <img alt="Windows" src="https://img.shields.io/badge/platforma-Windows%2010%20%7C%2011%20%7C%20Server-0078d4">
  <img alt="Bez telemetrii" src="https://img.shields.io/badge/telemetria-brak-6b7280">
</p>

<p align="center">
  <a href="https://github.com/craftprojects-sp-zoo/cp-signbridge-official/releases/latest"><b>Pobierz najnowszą wersję / Download the latest release</b></a>
</p>

<p align="center">
  <a href="#polski">Polski</a> · <a href="#english">English</a>
</p>

---

<a id="polski"></a>

## Czym jest SignBridge

SignBridge pozwala podpisywać dokumenty **kartą kwalifikowaną lub tokenem włożonym do własnego komputera**,
pracując w programach uruchomionych na **serwerze pulpitu zdalnego** (RDP / RDS).

Karty do podpisu kwalifikowanego często nie działają w sesjach RDP na nowszych wersjach Windows Server:
Windows widzi przekierowany czytnik, ale oprogramowanie dostawcy karty go nie znajduje.
SignBridge omija przekierowanie kart całkowicie.

Program jest **bezpłatny i otwarty** (licencja MIT).

## Dla kogo

* Firmy i biura rachunkowe pracujące w programach (np. księgowych, kadrowych, czytnikach PDF) uruchomionych
  na serwerze pulpitu zdalnego, które podpisują dokumenty kartą kwalifikowaną na komputerze pracownika.
* Administratorzy, którzy chcą wdrożyć podpis w RDS bez otwierania portów i zmian w sieci (GPO / Intune / MSI).

## Jak działa (w skrócie)

```
  SERWER (RDS)                                       KOMPUTER UŻYTKOWNIKA
  ┌──────────────────────────┐                      ┌──────────────────────────┐
  │ program księgowy / PDF   │                      │  mstsc + wtyczka         │
  │   │ "podpisz skrót"      │                      │  SignBridge              │
  │   ▼                      │   kanał wirtualny    │   │ NCryptSignHash       │
  │ Windows CNG              │        RDP           │   ▼                      │
  │   │                      │ ◄══════════════════► │ Windows CNG              │
  │   ▼                      │  (skrót → podpis)    │   │                      │
  │ dostawca kluczy (KSP)    │                      │   ▼  PIN lokalnie        │
  │ SignBridge               │                      │ karta / token / chmura   │
  └──────────────────────────┘                      └──────────────────────────┘
```

1. **Na serwerze** dostawca kluczy Windows (CNG KSP) pokazuje programom certyfikat użytkownika jak zwykły
   certyfikat z kluczem. Gdy program prosi o podpis, **sam skrót** trafia przez sesję RDP do komputera użytkownika.
2. **Na komputerze użytkownika** wtyczka klienta pulpitu zdalnego (mstsc) prosi lokalny Windows o podpis kartą
   — PIN wpisuje się w oknie sterownika karty — i odsyła podpis.
3. **Kanał** to wirtualny kanał tej samej, szyfrowanej sesji RDP. Bez portów, adresów i reguł zapory.

Klucz prywatny nigdy nie opuszcza karty, a PIN nie jest przesyłany ani zapisywany (wymóg wyłącznej kontroli — eIDAS).
Obsługiwane: RSA i ECDSA, SHA-256/384/512; każda karta, token lub podpis chmurowy działający lokalnie w Windows.

## Wymagania

| Gdzie | Wymagania |
|---|---|
| Serwer | Windows Server z rolą Usług pulpitu zdalnego (RDS) lub Windows 10/11 z dostępem RDP |
| Komputer z kartą | Windows 10/11, klient **Podłączanie pulpitu zdalnego (mstsc)**, sterownik / aplikacja dostawcy karty |
| Instalacja | Uprawnienia administratora (UAC) |

Kanał działa w kliencie mstsc na Windows; inne klienty RDP (macOS, aplikacje przeglądarkowe) go nie obsługują.

## Instalacja

**[Pobierz najnowszą wersję](https://github.com/craftprojects-sp-zoo/cp-signbridge-official/releases/latest)**
(wszystkie wydania: zakładka **[Releases](../../releases)**):

| Plik | Zastosowanie |
|---|---|
| `SignBridge.Installer.exe` | kreator instalacji — ten sam plik na serwer i na komputery z kartą |
| `SignBridge-1.1.1.zip` | ten sam program spakowany (gdy przeglądarka lub poczta blokuje `.exe`); w środku `SignBridge Installer.exe` |
| `SignBridge.msi` | instalacja cicha przez GPO / Intune: `msiexec /i "SignBridge.msi" /qn` |
| `Craftprojects-CodeSigning.cer` | publiczny certyfikat wydawcy (zob. weryfikacja, pkt 3) |
| `SignBridge-1.1.1-zrodla.zip` | dokładnie te źródła, z których zbudowano wydanie |
| `SHA256SUMS.txt` | sumy kontrolne SHA-256 powyższych plików |

GitHub zamienia spację w nazwie pliku na kropkę, dlatego pobrany instalator nazywa się `SignBridge.Installer.exe`,
a ten sam plik w ZIP i po zbudowaniu ze źródeł — `SignBridge Installer.exe`. To ten sam program; w poleceniach
z [dokumentacji](docs/instruction-msi.md) użyj nazwy pliku, który masz.

**Aktualizacja:** uruchom nowszy instalator EXE albo MSI na istniejącej instalacji. Instalator EXE od 1.1.1 sam
zamyka działający program w swojej sesji (także 1.1.0). Pakiet MSI od 1.1.1 sam zamyka działający SignBridge we wszystkich
sesjach (także na serwerze RDS) i nie wymaga restartu. Przy przejściu z 1.1.0 na MSI zalecamy wcześniej zamknąć
program (menu ikony przy zegarze → **Zakończ**) — 1.1.0 nie reaguje na prośbę o zamknięcie, więc pakiet kończy go
dopiero po kilku sekundach.

Szczegóły: [docs/PANEL.md](docs/PANEL.md) (instalacja i obsługa) oraz [docs/instruction-msi.md](docs/instruction-msi.md)
(MSI i wiersz poleceń).

## Weryfikacja pobranej paczki

Przed uruchomieniem sprawdź, że plik pochodzi z tego wydania i nie został zmieniony.

**1. Suma SHA-256** — porównaj z plikiem `SHA256SUMS.txt` dołączonym do wydania (PowerShell):

```powershell
Get-FileHash ".\SignBridge.Installer.exe" -Algorithm SHA256
Get-FileHash ".\SignBridge.msi" -Algorithm SHA256
# albo w CMD:
certutil -hashfile "SignBridge.Installer.exe" SHA256
```

**2. Podpis Authenticode** — pliki wydań (od 1.0.0) są podpisane własnym certyfikatem firmy Craftprojects:

| | |
|---|---|
| Wydawca | `CN=Craftprojects Sp. z o.o., O=Craftprojects Sp. z o.o., C=PL` |
| Odcisk SHA-1 | `096EC851CB0D6BE8A30AC1CAC7BE85EFE060683B` |
| Ważny do | 2036-10-14 (znacznik czasu Certum) |

```powershell
Get-AuthenticodeSignature ".\SignBridge.Installer.exe" | Format-List Status, StatusMessage, SignerCertificate
Get-AuthenticodeSignature ".\SignBridge.msi"            | Format-List Status, StatusMessage, SignerCertificate
# odcisk musi być równy 096EC851CB0D6BE8A30AC1CAC7BE85EFE060683B:
(Get-AuthenticodeSignature ".\SignBridge.Installer.exe").SignerCertificate.Thumbprint
```

Można też kliknąć plik prawym przyciskiem → **Właściwości** → **Podpisy cyfrowe**.

**3. Zaufanie do certyfikatu** — certyfikat nie pochodzi z publicznego urzędu certyfikacji, więc na komputerze,
który go nie zna, Windows pokaże **„Nieznany wydawca”**, a `Get-AuthenticodeSignature` stan `UnknownError`
(plik jest podpisany i niezmieniony, jeśli odcisk się zgadza). Część publiczna certyfikatu,
`Craftprojects-CodeSigning.cer`, jest dołączona do wydania (jej suma jest w `SHA256SUMS.txt`). Po jej zaufaniu stan to `Valid`:

* **GPO:** Konfiguracja komputera → Zasady → Ustawienia systemu Windows → Ustawienia zabezpieczeń →
  Zasady kluczy publicznych → **Zaufane główne urzędy certyfikacji** oraz **Zaufani wydawcy** → Importuj `.cer`.
* **Ręcznie** (CMD jako administrator):
  ```
  certutil -addstore -f Root Craftprojects-CodeSigning.cer
  certutil -addstore -f TrustedPublisher Craftprojects-CodeSigning.cer
  ```

Jeśli suma się nie zgadza, odcisk jest inny albo stan to `HashMismatch` / `NotSigned` — nie uruchamiaj pliku
i zgłoś to (zob. [SECURITY.md](SECURITY.md)).

> Wydania są podpisywane certyfikatem Craftprojects Sp. z o.o. W przyszłości możliwy jest podpis przez SignPath
> Foundation — sposób weryfikacji zostanie wtedy opisany w notach wydania. Zasady podpisywania (co jest podpisywane,
> kto zatwierdza wydania, prywatność): **[CODE_SIGNING_POLICY.md](CODE_SIGNING_POLICY.md)**.

Plik `SignBridge-1.1.1.zip` zawiera `SignBridge Installer.exe` — sprawdź sumę samego ZIP-a, a podpis i odcisk
rozpakowanego pliku tak jak wyżej.

## Budowanie ze źródeł (Windows)

Build wymaga Windows (MSVC, Windows SDK, zasoby `.rc`); na macOS / Linux nie da się go wykonać.

1. Zainstaluj **[Build Tools for Visual Studio 2026](https://visualstudio.microsoft.com/visual-cpp-build-tools/)**
   (wersja 18) z obciążeniem **„Desktop development with C++”** oraz komponentami **„C++ CMake tools for Windows”**
   (zawiera CMake ≥ 3.20 i Ninja) i **Windows SDK** (wydania 1.0.0–1.1.1 zbudowano z MSVC 19.51 i Windows SDK 10.0.28000).
2. *(Opcjonalnie, dla pakietu MSI)* zainstaluj [.NET SDK](https://dotnet.microsoft.com/download), a potem WiX 5:
   ```
   dotnet tool install --global wix --version 5.0.2
   ```
3. Pobierz kod źródłowy (`git clone` lub ZIP z GitHuba) i w katalogu repozytorium uruchom w zwykłym **CMD**:
   ```
   build.cmd
   ```
   Skrypt sam znajduje Visual Studio (`vswhere`), buduje biblioteki x86 i x64, wbudowuje je w program
   i — gdy jest WiX — tworzy MSI.
4. Wynik w `dist\`:
   * `SignBridge Installer.exe` (z wbudowanymi bibliotekami x64 i x86),
   * `SignBridge.msi` (gdy zainstalowano WiX).

   Narzędzia diagnostyczne `sbctl.exe` i `sbagent.exe` zostają w `build\x64\`.

   Testy jednostkowe (`tests\run-tests.cmd`, x86 i x64) i ten sam build (bez podpisu) wykonuje automatycznie **GitHub Actions** ([.github/workflows/build.yml](.github/workflows/build.yml))
   przy każdej zmianie na `main` i w pull requestach — pliki i `SHA256SUMS.txt` są w artefaktach przebiegu
   (zakładka **Actions**). Nie są one wydaniem: są niepodpisane, a ich sumy różnią się od plików wydania
   (build nie jest powtarzalny bit w bit, a pliki wydania są podpisane).
5. *(Opcjonalnie)* podpis Authenticode własnym certyfikatem code-signing:
   ```
   rem odcisk SHA-1 certyfikatu z magazynu użytkownika (40 znaków hex)
   set SB_SIGN_THUMBPRINT=<odcisk>
   rem opcjonalnie: serwer znacznika czasu RFC 3161 (domyślnie http://time.certum.pl; "none" = bez znacznika)
   set SB_SIGN_TIMESTAMP=http://time.certum.pl
   build.cmd
   ```
   Bez `SB_SIGN_THUMBPRINT` build jest niepodpisany. Szczegóły: [sign.cmd](sign.cmd),
   [docs/instruction-msi.md](docs/instruction-msi.md), [tools/make-signing-cert.ps1](tools/make-signing-cert.ps1).

Wersję zmienia się w jednym miejscu: `src/panel/resource.h` (`SB_VERSION_*`).

```
src/ksp/        dostawca kluczy CNG (cpsignbridge.dll) — serwer
src/rdpplugin/  wtyczka mstsc (cpsignbridge-rdp.dll) — komputer użytkownika
src/panel/      SignBridge.exe — kreator, panel, ikona przy zegarze; zasoby w res/ i resource.h
src/sbctl/      narzędzie diagnostyczne (serwer)
src/agent/      narzędzie diagnostyczne (komputer użytkownika)
src/common/     protokół, certyfikaty, instalacja
installer/      pakiet MSI (WiX)
tools/          certyfikat testowy do podpisu kodu, generator ikon
docs/           dokumentacja użytkownika, rejestr zależności
```

## Powiadomienie o nowej wersji

Od wersji 1.1.0 zainstalowany panel SignBridge sam sprawdza, czy na GitHubie jest nowsze wydanie, i pokazuje
w oknie dyskretną informację **„Dostępna wersja X.Y.Z”** z przyciskami **Aktualizuj** i **Przypomnij później**
(w menu ikony przy zegarze: „Dostępna wersja X.Y.Z — Aktualizuj”). Nic nie jest wymuszane.

* **Program niczego nie pobiera ani nie instaluje.** „Aktualizuj” otwiera w przeglądarce stały adres
  <https://github.com/craftprojects-sp-zoo/cp-signbridge-official/releases/latest> (nigdy adres z odpowiedzi serwera);
  nowy instalator pobiera i uruchamia użytkownik albo administrator.
* **Kiedy:** po uruchomieniu panelu (przy autostarcie po zalogowaniu — z losowym opóźnieniem 1–5 min) i potem
  najwyżej raz na 24 godziny na użytkownika; limit czasu 5 s na każdy etap zapytania. Wersje robocze (draft)
  i przedpremierowe (prerelease) są pomijane. „Przypomnij później” ukrywa informację o tej wersji na 7 dni.
* **Bez internetu** (np. serwer RDS bez dostępu do sieci) nic się nie zmienia: błąd trafia tylko do logu
  (`C:\ProgramData\cp-signbridge\logs`), bez okienek i komunikatów.
* **Jak:** jedno zapytanie HTTPS GET do `https://api.github.com/repos/craftprojects-sp-zoo/cp-signbridge-official/releases/latest`
  przez systemowy WinHTTP (TLS 1.2 lub nowszy, certyfikat serwera sprawdza Windows, ustawienia proxy systemu,
  bez przekierowań, bez logowania i tokenu). Z odpowiedzi odczytywane są tylko pola `tag_name`, `draft` i `prerelease`.
* Na serwerze z wieloma użytkownikami każdy użytkownik sprawdza osobno (raz na dobę, rozłożone w czasie). GitHub
  przyjmuje 60 takich zapytań na godzinę z jednego adresu IP — po przekroczeniu sprawdzenie kończy się cichym
  wpisem w logu, a informacja pojawi się przy kolejnej próbie. Na serwerach sprawdzanie można wyłączyć (niżej).

### Wyłączenie sprawdzania (GPO / rejestr)

Domyślnie sprawdzanie jest włączone. Administrator wyłącza je dla całego komputera wartością w rejestrze
(np. zasady grupy → Preferencje → Ustawienia systemu Windows → Rejestr, albo skryptem):

```
HKEY_LOCAL_MACHINE\SOFTWARE\Policies\Craftprojects\SignBridge
    DisableUpdateCheck    REG_DWORD    1
```

```
reg add "HKLM\SOFTWARE\Policies\Craftprojects\SignBridge" /v DisableUpdateCheck /t REG_DWORD /d 1 /f
```

Wartość `1` (lub inna niezerowa) — panel nie wysyła zapytań i nie pokazuje informacji o nowej wersji
(od uruchomienia panelu, w działającym panelu najpóźniej po godzinie). Brak wartości albo `0` — sprawdzanie włączone.
Odinstalowanie SignBridge nie usuwa tej zasady (należy do administratora).

### Prywatność

Sprawdzanie wysyła do GitHub (GitHub, Inc.) tylko to, co zawiera każde zwykłe zapytanie HTTPS: adres IP komputera
(albo serwera proxy) oraz nagłówki `User-Agent: SignBridge/<wersja programu>`, `Accept`, `X-GitHub-Api-Version`
i — przy kolejnych sprawdzeniach — `If-None-Match` (znacznik ETag poprzedniej odpowiedzi, żeby nie pobierać jej
ponownie). Bez identyfikatorów komputera i użytkownika, nazw, certyfikatów, licencji i bez ciasteczek. Craftprojects
tych danych nie otrzymuje; GitHub przetwarza je według własnej polityki prywatności (GitHub General Privacy Statement).
Wynik (najnowsza wersja, czas sprawdzenia, ETag, odłożenie przypomnienia) zapisywany jest tylko lokalnie,
w `HKEY_CURRENT_USER\Software\cp-signbridge`, i usuwany przy odinstalowaniu.

## Bezpieczeństwo i sieć

* Program **nie otwiera żadnych portów** ani nie nasłuchuje w sieci. Jedyne połączenie z internetem to
  **sprawdzanie nowej wersji** (opis wyżej; do wyłączenia zasadą grupy).
* Serwer i komputer z kartą komunikują się **wyłącznie kanałem wirtualnym RDP** wewnątrz istniejącej,
  szyfrowanej sesji pulpitu zdalnego danego użytkownika; inne sesje na serwerze tych danych nie widzą.
  Płyną nim tylko skrót do podpisu, gotowy podpis i certyfikaty (część publiczna).
* **Brak telemetrii** i **brak automatycznych aktualizacji** — program tylko informuje o nowej wersji;
  nowa wersja to nowy instalator uruchomiony przez administratora.
* PIN nigdy nie jest przesyłany ani zapisywany; klucz prywatny zostaje na karcie.
* Link `craftprojects.pl` w stopce panelu i przycisk „Aktualizuj” otwierają przeglądarkę tylko po kliknięciu.
* Błędy bezpieczeństwa zgłaszaj prywatnie — zob. [SECURITY.md](SECURITY.md).

## Licencja i zależności

Kod: **MIT** — zob. [LICENSE](LICENSE). © 2026 Craftprojects Sp. z o.o.
Narzędzia i biblioteki użyte do budowania wraz z licencjami: [docs/compliance/dependency-registry.json](docs/compliance/dependency-registry.json).
Pakiet `SignBridge.msi` (od 1.1.1) zawiera niezmienioną bibliotekę akcji WiX Toolset Util (licencja MS-RL),
która zamyka działający program przed aktualizacją.
Historia zmian: [CHANGELOG.md](CHANGELOG.md).

## Znaki towarowe

Kod jest objęty licencją MIT. Nazwy „Craftprojects” i „SignBridge” oraz logotypy Craftprojects **nie są objęte tą
licencją** i pozostają własnością Craftprojects Sp. z o.o. Wersje zmienione nie mogą używać tych nazw ani logo
w sposób sugerujący, że pochodzą od Craftprojects.

Nazwy innych produktów i firm wymienione w dokumentacji (np. Windows, Comarch Optima, Symfonia, Adobe Acrobat,
Sigillum, mSzafir, CloudSigner, Certum) są znakami ich właścicieli i służą wyłącznie wskazaniu, z czym program
współpracuje. **SignBridge jest projektem niezależnym i nie jest powiązany z producentami wymienionymi w nazwach** —
nie jest ich produktem, partnerem ani rozwiązaniem przez nich zatwierdzonym.

## Kontakt

<a href="https://craftprojects.pl"><img src="docs/img/logo-firmy.svg" width="56" align="left" alt="Craftprojects"></a>

**Craftprojects Sp. z o.o.**<br>
[craftprojects.pl](https://craftprojects.pl) · [biuro@craftprojects.pl](mailto:biuro@craftprojects.pl)

<br clear="left">

---

<a id="english"></a>

## English

### What is SignBridge

SignBridge lets you sign documents with a **qualified smart card or token plugged into your own PC** while working
in applications that run on a **Remote Desktop (RDP / RDS) server**. Smart-card redirection often fails on newer
Windows Server versions — Windows sees the redirected reader, but the card vendor's software does not.
SignBridge bypasses smart-card redirection entirely. It is **free and open source** (MIT).

### Who is it for

* Companies and accounting offices that run their business software on a Remote Desktop server and sign documents
  with a qualified card on the employee's PC.
* Administrators who want signing in RDS without opening ports or changing the network (GPO / Intune / MSI).

### How it works

1. **On the server** a Windows CNG Key Storage Provider exposes the user's certificate to applications as a normal
   certificate with a key. When an application asks for a signature, **only the hash** is sent through the RDP session.
2. **On the user's PC** a plug-in for the Remote Desktop client (mstsc) asks local Windows to sign with the card —
   the PIN is entered in the card driver's own dialog — and returns the signature.
3. **The channel** is an RDP dynamic virtual channel inside the same encrypted session: no ports, addresses or firewall rules.

The private key never leaves the card and the PIN is never transmitted or stored. RSA and ECDSA, SHA-256/384/512.

### Requirements

* Server: Windows Server with Remote Desktop Services (or Windows 10/11 with RDP access).
* User PC: Windows 10/11, the built-in Remote Desktop Connection client (mstsc), the card vendor's driver.
* Administrator rights for installation. The user interface and user documentation are in Polish.

### Install

**[Download the latest release](https://github.com/craftprojects-sp-zoo/cp-signbridge-official/releases/latest)**
(all releases: **[Releases](../../releases)**): `SignBridge.Installer.exe`, the ZIP (`SignBridge-1.1.1.zip`, for when
`.exe` downloads are blocked) or `SignBridge.msi`, plus `Craftprojects-CodeSigning.cer`, the exact build sources
(`SignBridge-1.1.1-zrodla.zip`) and `SHA256SUMS.txt`. Run the same installer on the server and on user PCs
(`msiexec /i "SignBridge.msi" /qn` for silent deployment).

GitHub replaces the space in the file name with a dot, so the downloaded installer is `SignBridge.Installer.exe`,
while the same file inside the ZIP and in a local build is `SignBridge Installer.exe`. Use the name of the file
you have in the commands from the documentation.

**Update:** run the newer EXE installer or MSI over the existing installation. Since 1.1.1 the EXE installer closes the
running program in its own session (1.1.0 too). Since 1.1.1 the MSI closes the running SignBridge in all sessions
(including RDS servers) without a restart. When upgrading from 1.1.0 with the MSI, close the program first
(tray icon menu → **Zakończ** / Exit) — 1.1.0 does not respond to the close request, so the package ends it only
after a few seconds.

### Verify a release download

1. **SHA-256** — compare with `SHA256SUMS.txt` attached to the release:
   `Get-FileHash ".\SignBridge.Installer.exe" -Algorithm SHA256` (or `certutil -hashfile <file> SHA256`).
2. **Authenticode** — releases (since 1.0.0) are signed with Craftprojects' own code-signing certificate:
   publisher `CN=Craftprojects Sp. z o.o., O=Craftprojects Sp. z o.o., C=PL`, SHA-1 thumbprint
   `096EC851CB0D6BE8A30AC1CAC7BE85EFE060683B`, valid until 2036-10-14 (Certum timestamp). Check with
   `Get-AuthenticodeSignature ".\SignBridge.Installer.exe" | Format-List Status, SignerCertificate`
   (or *Properties → Digital Signatures*).
3. **Trust** — the certificate is not issued by a public CA, so on a PC that does not trust it Windows shows
   **"Unknown publisher"** and the status is `UnknownError` (the file is still signed and intact if the thumbprint
   matches). The public certificate `Craftprojects-CodeSigning.cer` is attached to the release (listed in
   `SHA256SUMS.txt`). Trust it via GPO (*Public Key Policies → Trusted Root Certification Authorities* **and**
   *Trusted Publishers*) or with `certutil -addstore -f Root Craftprojects-CodeSigning.cer` and
   `certutil -addstore -f TrustedPublisher Craftprojects-CodeSigning.cer`; the status then becomes `Valid`.

If a check fails (hash mismatch, different thumbprint, `HashMismatch` / `NotSigned`), do not run the file and
report it (see [SECURITY.md](SECURITY.md)).

Releases are signed with a Craftprojects Sp. z o.o. certificate. Signing through the SignPath Foundation may become
possible in the future; the release notes will say so. Signing rules (what is signed, who approves releases, privacy): **[CODE_SIGNING_POLICY.md](CODE_SIGNING_POLICY.md)**.

`SignBridge-1.1.1.zip` contains `SignBridge Installer.exe` — check the hash of the ZIP itself, and the signature and
thumbprint of the extracted file as above.

### Build from source (Windows only)

1. Install **Build Tools for Visual Studio 2026** (version 18) with **"Desktop development with C++"**,
   **"C++ CMake tools for Windows"** and a **Windows SDK** (releases 1.0.0–1.1.1 were built with MSVC 19.51 and
   Windows SDK 10.0.28000).
2. *(Optional, for MSI)* install the .NET SDK and WiX 5: `dotnet tool install --global wix --version 5.0.2`.
3. In the repository folder run `build.cmd` from a regular **CMD** prompt.
4. Output in `dist\`: `SignBridge Installer.exe` and (with WiX) `SignBridge.msi`; diagnostic tools in `build\x64\`.
5. *(Optional)* Authenticode signing: set `SB_SIGN_THUMBPRINT` (SHA-1 thumbprint of your code-signing certificate)
   and optionally `SB_SIGN_TIMESTAMP`, then run `build.cmd` — see `sign.cmd`.

Unit tests (`tests\run-tests.cmd`, x86 and x64) and the same (unsigned) build run on **GitHub Actions** ([.github/workflows/build.yml](.github/workflows/build.yml)) for
every change on `main` and every pull request; the files and `SHA256SUMS.txt` are attached to the run as artifacts.
They are not releases: they are unsigned and their hashes differ from the release files (the build is not
bit-for-bit reproducible, and release files are signed).

The build requires Windows (MSVC, Windows SDK, resource compiler); it cannot be run on macOS or Linux.

### Update notice

Since 1.1.0 the installed SignBridge panel checks whether a newer release exists on GitHub and shows a discreet
"Dostępna wersja X.Y.Z" (new version available) notice with **Aktualizuj** (Update) and **Przypomnij później**
(Remind me later); the tray icon menu shows "Dostępna wersja X.Y.Z — Aktualizuj". Nothing is forced.

* **The program never downloads or installs anything.** "Aktualizuj" opens the fixed address
  <https://github.com/craftprojects-sp-zoo/cp-signbridge-official/releases/latest> in the browser (never a URL
  taken from the server response).
* **When:** after the panel starts (autostart at logon: random 1–5 min delay), then at most once every 24 hours per
  user; 5 s timeout for each stage of the request. Drafts and pre-releases are ignored. "Remind me later" hides that
  version for 7 days.
* **Offline** (e.g. an RDS server without internet access) nothing changes: a failed check is only written to the
  log (`C:\ProgramData\cp-signbridge\logs`), with no dialogs.
* **How:** a single HTTPS GET to `https://api.github.com/repos/craftprojects-sp-zoo/cp-signbridge-official/releases/latest`
  through Windows WinHTTP (TLS 1.2 or newer, server certificate validated by Windows, system proxy settings,
  no redirects, no sign-in or token). Only `tag_name`, `draft` and `prerelease` are read from the response.
* **Disable (GPO / registry):** enabled by default; set `DisableUpdateCheck` = `1` (REG_DWORD) under
  `HKEY_LOCAL_MACHINE\SOFTWARE\Policies\Craftprojects\SignBridge`, e.g.
  `reg add "HKLM\SOFTWARE\Policies\Craftprojects\SignBridge" /v DisableUpdateCheck /t REG_DWORD /d 1 /f`.
* **Privacy:** GitHub receives only what any HTTPS request contains: the IP address (or the proxy's) and the headers
  `User-Agent: SignBridge/<version>`, `Accept`, `X-GitHub-Api-Version` and, on later checks, `If-None-Match` (the ETag
  of the previous response). No machine or user identifiers, names, certificates, licences or cookies. Craftprojects
  does not receive this data; GitHub processes it under its own privacy statement. The result is stored only locally
  in `HKEY_CURRENT_USER\Software\cp-signbridge` and removed on uninstall.

### Security

No open ports or listening sockets, no telemetry and no automatic updates; the only internet connection is the
update check described above (can be disabled by policy). The server and the
PC with the card communicate only through the RDP virtual channel inside the user's existing, encrypted RDP session.
Please report vulnerabilities privately — see [SECURITY.md](SECURITY.md).

### License and trademarks

The code is MIT-licensed — see [LICENSE](LICENSE). Build-time dependencies and their licenses:
[docs/compliance/dependency-registry.json](docs/compliance/dependency-registry.json).
`SignBridge.msi` (since 1.1.1) contains the unmodified WiX Toolset Util custom action library (MS-RL license),
which closes the running program before an update.

The names "Craftprojects" and "SignBridge" and the Craftprojects logos are not covered by the MIT license; modified
versions must not use them in a way that suggests they come from Craftprojects. Other product and company names are
trademarks of their respective owners and are used only to describe compatibility.
**SignBridge is an independent project and is not affiliated with, endorsed by or sponsored by any of the vendors
named in this documentation.**

### Contact

**Craftprojects Sp. z o.o.** — [craftprojects.pl](https://craftprojects.pl) · [biuro@craftprojects.pl](mailto:biuro@craftprojects.pl)
