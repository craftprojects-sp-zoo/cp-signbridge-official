# SignBridge — jedna aplikacja, jeden plik

`dist\SignBridge Installer.exe` — to jedyny plik do skopiowania (na komputer pracownika
i na serwer). Biblioteki (dostawca kluczy x64/x86, wtyczka pulpitu zdalnego x64/x86) są w nim wbudowane
i wypakowują się przy instalacji. Po instalacji program nazywa się `SignBridge.exe`
(serwer: `C:\Program Files\cp-signbridge\`, komputer: `%LOCALAPPDATA%\cp-signbridge\`) — instalator można
potem skasować.

## Instalacja

Jeden instalator, jedna instalacja — **zawsze obie części** (dostawca podpisu dla serwera + wtyczka pulpitu
zdalnego dla komputera z kartą), dla wszystkich użytkowników komputera (UAC). Ten sam plik uruchamia się na
serwerze i na komputerach pracowników; niepotrzebna część po prostu nie jest używana.

Kreator: **co zostanie zainstalowane** (wybór „ten komputer to: komputer z kartą / serwer” — zapisywany dla całego
komputera w `HKLM\Software\cp-signbridge\Role`; opcja: skrót na pulpicie) → **instalacja** (postęp, dziennik) → **gotowe**.
Instalowane: `cpsignbridge.dll` → System32/SysWOW64 + rejestracja dostawcy; wtyczka mstsc w HKLM (wszyscy
użytkownicy); program → `C:\Program Files\cp-signbridge\SignBridge.exe`; autostart ikony przy
zegarze dla każdego użytkownika (HKLM Run); logi `C:\ProgramData\cp-signbridge\logs`; `HKLM\Software\cp-signbridge`
(Version, InstallDir, InstalledAt) i wpis w „Aplikacje i funkcje” (odinstalowanie `--uninstall`).

Po instalacji instalator proponuje usunięcie własnego pliku (domyślnie zaznaczone) i **uruchamia zainstalowaną kopię**
z Program Files — nie pracuje dalej z pliku instalatora. Ponowne uruchomienie instalatora w tej samej wersji po prostu
otwiera zainstalowany program; nowsza wersja od razu proponuje aktualizację.

Zainstalowany program otwiera się jako **panel** w widoku wybranym przy instalacji. **Ustawienia** (przycisk w panelu):
wybór „ten komputer to: komputer z kartą / serwer” (zapis w HKCU, pierwszeństwo przed rolą z instalacji w HKLM) oraz
opcja „Pokazuj w panelu przełącznik widoku” — dopiero wtedy u góry panelu pojawiają się dwa przyciski (domyślnie ukryte).
Przycisk **PROGRAM NIE DZIAŁA?** w nagłówku (także w menu ikony) pokazuje instrukcję krok po kroku dla użytkownika
(wylogowanie z RDP, ponowne włożenie karty, sprawdzenie w aplikacji karty i w panelu, ustawienia mstsc). „Zainstaluj ponownie” / „Aktualizuj” (gdy instalator jest nowszy) i „Odinstaluj”
są w panelu.

## Ikona przy zegarze

Kolor: zielony = gotowe, żółty = brak karty / certyfikatów, szary = nie zainstalowano. Menu (prawy przycisk):
wersja, **stan karty** („Karta: Imię Nazwisko — wystawca” / „nie wykryto”) lub liczba certyfikatów z komputera,
„Otwórz panel”, „Pobierz certyfikaty teraz” (serwer), „Odśwież”, **Rola tego komputera** (przełączanie),
„Zakończ”. Dymki: żądanie podpisu, podpis wykonany / nieudany, gotowe do podpisu.

## Komputer z kartą (pracownik)

1. Skopiuj `SignBridge Installer.exe` gdziekolwiek (np. Pobrane) i uruchom.
2. Kreator: rola **komputer z kartą / tokenem** → Dalej → **Zainstaluj**. Program:
   * instaluje wtyczkę pulpitu zdalnego dla tego konta (bez administratora),
   * kopiuje się do `%LOCALAPPDATA%\cp-signbridge\SignBridge.exe`, włącza ikonę przy zegarze po zalogowaniu,
   * tworzy skrót „SignBridge” na pulpicie.
3. Zamknij otwarte okna pulpitu zdalnego, połącz się ponownie (mstsc / Podłączanie pulpitu zdalnego).

Okno pokazuje ✔/✘: wtyczka, ikona po zalogowaniu, karta (lista certyfikatów z kluczem).
Powiadomienia przy zegarze: „Żądanie podpisu” → okno PIN sterownika karty → „Podpis wykonany”.

## Serwer pulpitu zdalnego

1. Skopiuj `SignBridge Installer.exe` na serwer, uruchom (na koncie z prawami administratora).
2. Kreator: rola **serwer pulpitu zdalnego** → Dalej → **Zainstaluj** → okno UAC. Program:
   * `cpsignbridge.dll` → `System32` i `SysWOW64`, rejestracja dostawcy „SignBridge Key Storage Provider”,
   * program → `C:\Program Files\cp-signbridge\SignBridge.exe`,
   * autostart panelu **dla każdego użytkownika** (`HKLM\...\Run`), logi `C:\ProgramData\cp-signbridge\logs`.
3. Gotowe. Od tej chwili, gdy ktokolwiek loguje się przez RDP, panel w jego sesji **sam** pobiera certyfikaty
   z jego komputera (przy zalogowaniu, po ponownym połączeniu, co 10 min) i pokazuje dymek
   „Gotowe do podpisu: Imię Nazwisko”. Użytkownik podpisuje w Optimie / Acrobacie, PIN wpisuje u siebie.

Przyciski w sesji: **Pobierz certyfikaty teraz**, **Test podpisu** (PIN na komputerze użytkownika),
**Usuń pobrane**, **Aktualizuj** (gdy `dist\` zawiera nowszą wersję), **Odinstaluj**.

Aktualizacja = uruchomić nowy instalator i kliknąć **Aktualizuj** (pliki w użyciu są podmieniane, stare
znikają po restarcie). Na komputerach pracowników: nowy instalator → **Zainstaluj na tym komputerze**.

## Gdy coś nie działa

| W oknie | Znaczenie |
|---|---|
| ✘ Certyfikaty z Twojego komputera: brak — nie udało się połączyć z wtyczką | Na PC nie zainstalowano wtyczki, nie zrestartowano mstsc, albo połączenie z Maca / Royal TSX / Windows App (kanał działa tylko w mstsc na Windows) |
| • Sesja pulpitu zdalnego: nie | Panel przy konsoli serwera (Hyper-V Connect) — nie ma skąd pobrać |
| ✘ Karta / token: nie widać certyfikatu | Karta wyjęta, aplikacja karty (Sigillum, CloudSigner) nie działa |

Logi: serwer `C:\ProgramData\cp-signbridge\logs\ksp-*.log` (każdy podpis), komputer użytkownika
`...\logs\agent-mstsc-*.log` (wtyczka). Przycisk **Folder logów** otwiera katalog.

Wiersz poleceń (diagnostyka, w `build\x64\`): `sbctl install|uninstall|sync|test-sign|status`,
`sbagent plugin-install|plugin-status|list`. `SignBridge.exe --tray` = start schowany do ikony.

## Odinstalowanie

„Aplikacje i funkcje” / Panel sterowania → *SignBridge* → Odinstaluj (uruchamia program z `--uninstall`),
albo przycisk **Odinstaluj** w panelu, albo `msiexec /x` dla pakietów MSI.

Usuwane jest wszystko: dostawca / wtyczka, pliki programu (`Program Files\cp-signbridge`, `%LOCALAPPDATA%\cp-signbridge`),
logi i pakiet (`ProgramData\cp-signbridge`), autostart, skrót, wpisy `Software\cp-signbridge` i „Aplikacje i funkcje”
(HKLM oraz profile wszystkich użytkowników), pobrane certyfikaty z profilu osoby odinstalowującej. Pliki w użyciu
znikają po restarcie. Jedyne, czego nie da się zrobić bez zalogowania danej osoby: pobrane certyfikaty w magazynach
Osobiste *innych* użytkowników serwera (publiczne, nieszkodliwe; po odinstalowaniu pokazują „brak klucza prywatnego”
— usuwa je „Usuń pobrane” w panelu przed odinstalowaniem albo `certmgr.msc`).

## Instalacja bezokienkowa (GPO, Intune, skrypty)

Ten sam plik, przełączniki (zwraca kod 0 = OK, 2 = błąd; dziennik `install.log` w katalogu instalacji):

```
"SignBridge Installer.exe" --install-all [--shortcut] --quiet     (administrator; obie części, wszyscy użytkownicy)
"SignBridge Installer.exe" --uninstall-all --quiet
```
(Części osobno, w razie potrzeby: `--install-server`, `--install-workstation [--machine] [--no-shortcut]` i odpowiednie `--uninstall-*`.)

## Pakiet MSI

`build.cmd` (gdy jest WiX 5: .NET SDK + `dotnet tool install --global wix --version 5.0.2`) tworzy
`dist\SignBridge.msi` — jeden pakiet per-machine na serwer i komputery, do GPO/Intune:

```
msiexec /i "SignBridge.msi" /qn
msiexec /x "SignBridge.msi" /qn
```

Nowa wersja = nowy MSI o wyższym numerze (MajorUpgrade podmienia w miejscu). Źródło: `installer\Package.wxs`.

## Praca offline i bezpieczeństwo kanału

Program nie łączy się z niczym poza kanałem wirtualnym wewnątrz istniejącej sesji RDP: brak sprawdzania
aktualizacji, telemetrii, pobierania czcionek (używa systemowego Segoe UI) czy innych zasobów z sieci.
Jedyne odwołanie do internetu to link `craftprojects.pl` w stopce — otwiera przeglądarkę tylko po kliknięciu.
Program nie otwiera gniazd sieciowych ani portów — serwer i komputer z kartą komunikują się wyłącznie kanałem RDP.

Dane w kanale (skrót do podpisu, podpis, certyfikaty — bez PIN-u) płyną wewnątrz połączenia RDP, które jest
szyfrowane przez sam protokół (TLS 1.2/1.3 i CredSSP/NLA między mstsc a serwerem). Dodatkowe szyfrowanie
wewnątrz kanału nie podnosi bezpieczeństwa (wymagałoby własnej wymiany kluczy opartej o to samo połączenie),
dlatego go nie ma. Kanał jest dostępny tylko dla sesji, która go otworzyła — inne sesje na serwerze go nie widzą.
