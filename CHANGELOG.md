# Historia zmian / Changelog

Format oparty na [Keep a Changelog](https://keepachangelog.com/pl/1.1.0/); wersje według [SemVer](https://semver.org/lang/pl/).

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

[1.0.0]: ../../releases/tag/v1.0.0
