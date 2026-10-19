# Historia zmian / Changelog

Format oparty na [Keep a Changelog](https://keepachangelog.com/pl/1.1.0/); wersje według [SemVer](https://semver.org/lang/pl/).

## [1.0.1] — 2026-10-19

Wydanie poprawkowe z poprawkami bezpieczeństwa. **Zalecana aktualizacja wszystkich instalacji 1.0.0**
(serwery i komputery z kartą) — instalatorem EXE albo pakietem MSI; aktualizacja poprawia też ustawienia
istniejącej instalacji. / *Security fix release. **All 1.0.0 installations should be updated** (servers and PCs
with the card) using the EXE installer or the MSI package; the update also corrects the existing installation.*

### Bezpieczeństwo / Security

* Instalator: zaostrzone uprawnienia katalogów programu i danych oraz bezpieczniejsza obsługa plików
  podczas instalacji, aktualizacji i odinstalowania; aktualizacja z 1.0.0 naprawia uprawnienia istniejącej
  instalacji. / *Installer: stricter permissions on program and data folders and safer file handling during
  install, update and uninstall; updating from 1.0.0 fixes the permissions of the existing installation.*
* Dostawca kluczy (KSP) i wtyczka mstsc: dokładniejsze sprawdzanie danych kanału i limit jednoczesnych żądań;
  funkcje diagnostyczne wyłączone w wydaniu. / *KSP and mstsc plug-in: stricter validation of channel data and a
  limit of concurrent requests; diagnostic features disabled in release builds.*
* Dzienniki: mniej zapisywanych danych, każdy użytkownik widzi tylko własne dzienniki, limit rozmiaru i usuwanie
  dzienników starszych niż 30 dni. / *Logs: less data recorded, each user sees only their own logs, size limit and
  removal of logs older than 30 days.*

### Dodano / Added

* Testy jednostkowe (`tests/`, x86 i x64) uruchamiane w GitHub Actions przed budowaniem. / *Unit tests run on
  GitHub Actions (x86 and x64) before the build.*

## [1.0.0] — 2026-10-14

Pierwsze wydanie publiczne. / First public release.

### Dodano

* Dostawca kluczy Windows CNG (KSP, `cpsignbridge.dll`, x64 i x86) na serwerze pulpitu zdalnego — pokazuje
  programom certyfikat użytkownika i przekazuje skrót do podpisu na komputer z kartą.
* Wtyczka klienta pulpitu zdalnego mstsc (`cpsignbridge-rdp.dll`, x64 i x86) — podpis lokalną kartą, tokenem
  lub podpisem chmurowym przez Windows CNG; PIN w oknie sterownika karty.
* Komunikacja wyłącznie przez kanał wirtualny RDP tej samej sesji — bez portów, adresów i reguł zapory.
* Obsługa RSA i ECDSA, SHA-256/384/512.
* `SignBridge.exe` — kreator instalacji, panel i ikona przy zegarze (stan karty, pobieranie certyfikatów,
  test podpisu, instrukcja „Program nie działa?”), jeden plik dla serwera i komputerów z kartą.
* Automatyczne pobieranie certyfikatów z komputera użytkownika do sesji na serwerze (przy logowaniu,
  ponownym połączeniu i co 10 minut).
* Instalacja cicha: przełączniki `--install-all` / `--uninstall-all --quiet` oraz pakiet `SignBridge.msi` (WiX 5)
  dla GPO / Intune.
* Narzędzia diagnostyczne `sbctl` (serwer) i `sbagent` (komputer użytkownika: instalacja wtyczki, lista certyfikatów).
* Skrypty budowania `build.cmd`, `build-msi.cmd` i opcjonalny podpis Authenticode (`sign.cmd`).

### Bezpieczeństwo

* Klucz prywatny nie opuszcza karty, PIN nie jest przesyłany ani zapisywany.
* Brak telemetrii, automatycznych aktualizacji i połączeń z internetem; program nie otwiera gniazd sieciowych
  ani portów — biblioteki nie importują Winsock (`ws2_32`).
* Plik instalatora (opcja „Usuń plik instalatora po zamknięciu”) jest usuwany przez zainstalowany program
  dopiero po zakończeniu procesu instalatora i po sprawdzeniu ścieżki — bez uruchamiania powłoki poleceń.
* Instalatory i wbudowane biblioteki podpisane Authenticode certyfikatem Craftprojects Sp. z o.o.
  (odcisk SHA-1 `096EC851CB0D6BE8A30AC1CAC7BE85EFE060683B`); certyfikat `Craftprojects-CodeSigning.cer` i
  `SHA256SUMS.txt` dołączone do wydania. / Installers and embedded libraries are Authenticode-signed.

[1.0.1]: ../../releases/tag/v1.0.1
[1.0.0]: ../../releases/tag/v1.0.0
