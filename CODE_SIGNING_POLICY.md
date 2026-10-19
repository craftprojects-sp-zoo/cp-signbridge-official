# Polityka podpisywania kodu / Code signing policy

[Polski](#polski) · [English](#english)

<a id="polski"></a>

## Polski

### Podpis

**Docelowo (jeśli podpis przez SignPath zostanie uruchomiony):** *Free code signing provided by [SignPath.io](https://signpath.io), certificate by [SignPath Foundation](https://signpath.org).*

> **Stan obecny:** wydania są podpisywane **certyfikatem Craftprojects Sp. z o.o.** (odcisk SHA-1
> `096EC851CB0D6BE8A30AC1CAC7BE85EFE060683B`) — sposób weryfikacji opisuje [README](README.md#weryfikacja-pobranej-paczki).
> W przyszłości możliwy jest podpis przez SignPath Foundation; wtedy wydawcą w podpisie będzie „SignPath Foundation”,
> a informacja o tym znajdzie się w notach wydania.

### Co jest podpisywane

* Podpisywane są **wyłącznie pliki zbudowane automatycznie z kodu źródłowego tego repozytorium**
  ([github.com/craftprojects-sp-zoo/cp-signbridge-official](https://github.com/craftprojects-sp-zoo/cp-signbridge-official))
  przez workflow GitHub Actions [`.github/workflows/build.yml`](.github/workflows/build.yml) (`build.cmd`, WiX 5).
* Podpisywane pliki: instalator `SignBridge Installer.exe` (w wydaniu: `SignBridge.Installer.exe`) z wbudowanymi
  bibliotekami SignBridge (`cpsignbridge.dll`, `cpsignbridge-rdp.dll`, x64 i x86) oraz pakiet `SignBridge.msi`.
* Nie podpisujemy plików zbudowanych ręcznie, plików osób trzecich ani wersji zmodyfikowanych przez inne osoby.
* **Każde wydanie jest zatwierdzane ręcznie** przez osobę z roli *Approvers* przed podpisaniem.

### Role

| Rola | Kto |
|---|---|
| Committers and reviewers (autorzy i recenzenci zmian) | [Craftprojects Sp. z o.o.](https://craftprojects.pl) — członkowie organizacji [craftprojects-sp-zoo](https://github.com/craftprojects-sp-zoo) |
| Approvers (zatwierdzający podpis wydań) | [Craftprojects Sp. z o.o.](https://craftprojects.pl) — właściciel organizacji [craftprojects-sp-zoo](https://github.com/craftprojects-sp-zoo) |

Zmiany od osób spoza zespołu trafiają do repozytorium tylko przez pull request przejrzany przez *Committers and reviewers*.
Kontakt: [biuro@craftprojects.pl](mailto:biuro@craftprojects.pl).

### Prywatność

Ten program nie przesyła żadnych informacji do innych systemów sieciowych, chyba że użytkownik wyraźnie tego zażąda —
z jednym wyjątkiem opisanym niżej (sprawdzanie nowej wersji), który administrator może wyłączyć.

SignBridge komunikuje się wyłącznie kanałem wirtualnym RDP wewnątrz istniejącej sesji pulpitu zdalnego użytkownika
(skrót do podpisu, gotowy podpis, publiczne części certyfikatów) — między serwerem a komputerem z kartą, na żądanie
użytkownika. Nie ma telemetrii ani automatycznych aktualizacji; jedyny link do internetu (`craftprojects.pl` w stopce
panelu) otwiera przeglądarkę dopiero po kliknięciu. Szczegóły: [README](README.md#bezpieczeństwo-i-sieć).

Sprawdzanie nowej wersji (od 1.1.0): zainstalowany panel najwyżej raz na 24 godziny wysyła do GitHub zwykłe zapytanie
HTTPS o najnowsze wydanie — GitHub otrzymuje tylko adres IP i standardowe nagłówki (`User-Agent: SignBridge/<wersja>`);
bez identyfikatorów komputera i użytkownika, bez ciasteczek. Program niczego nie pobiera ani nie instaluje; przycisk
„Aktualizuj” otwiera stronę wydań w przeglądarce. Wyłączenie: `HKLM\SOFTWARE\Policies\Craftprojects\SignBridge`,
`DisableUpdateCheck` = 1 (REG_DWORD). Szczegóły: [README](README.md#powiadomienie-o-nowej-wersji).

### Zgłaszanie problemów

Nadużycia certyfikatu lub podejrzane pliki podpisane w imieniu SignBridge zgłaszaj zgodnie z [SECURITY.md](SECURITY.md).

---

<a id="english"></a>

## English

### Signing

**Target (if signing through SignPath is enabled):** *Free code signing provided by [SignPath.io](https://signpath.io), certificate by [SignPath Foundation](https://signpath.org).*

> **Current state:** releases are signed with a **Craftprojects Sp. z o.o. certificate** (SHA-1 thumbprint
> `096EC851CB0D6BE8A30AC1CAC7BE85EFE060683B`) — see [README](README.md#verify-a-release-download) for verification.
> Signing through the SignPath Foundation may become possible in the future; releases signed that way will show
> "SignPath Foundation" as the publisher, and the release notes will say so.

### What is signed

* **Only files built automatically from the source code in this repository**
  ([github.com/craftprojects-sp-zoo/cp-signbridge-official](https://github.com/craftprojects-sp-zoo/cp-signbridge-official))
  by the GitHub Actions workflow [`.github/workflows/build.yml`](.github/workflows/build.yml) (`build.cmd`, WiX 5).
* Signed files: the installer `SignBridge Installer.exe` (release asset: `SignBridge.Installer.exe`) with the embedded
  SignBridge libraries (`cpsignbridge.dll`, `cpsignbridge-rdp.dll`, x64 and x86) and the `SignBridge.msi` package.
* We do not sign manually built files, third-party files or versions modified by others.
* **Every release is approved manually** by a member of the *Approvers* role before it is signed.

### Roles

| Role | Who |
|---|---|
| Committers and reviewers | [Craftprojects Sp. z o.o.](https://craftprojects.pl) — members of the [craftprojects-sp-zoo](https://github.com/craftprojects-sp-zoo) organization |
| Approvers | [Craftprojects Sp. z o.o.](https://craftprojects.pl) — owner of the [craftprojects-sp-zoo](https://github.com/craftprojects-sp-zoo) organization |

Contributions from outside the team are merged only via pull requests reviewed by *Committers and reviewers*.
Contact: [biuro@craftprojects.pl](mailto:biuro@craftprojects.pl).

### Privacy policy

This program will not transfer any information to other networked systems unless specifically requested by the user
or the person installing or operating it, with the single exception of the update check described below, which
administrators can disable.

SignBridge communicates only through the RDP virtual channel inside the user's existing Remote Desktop session
(the hash to be signed, the signature and public certificates) — between the server and the PC with the card, on the
user's request. There is no telemetry and no automatic updates; the only internet link (`craftprojects.pl` in the panel
footer) opens a browser only when clicked. Details: [README](README.md#security).

Update check (since 1.1.0): at most once every 24 hours the installed panel sends a plain HTTPS request for the latest
release to GitHub — GitHub receives only the IP address and standard headers (`User-Agent: SignBridge/<version>`);
no machine or user identifiers, no cookies. Nothing is downloaded or installed; the "Aktualizuj" button opens the
releases page in the browser. Disable: `HKLM\SOFTWARE\Policies\Craftprojects\SignBridge`, `DisableUpdateCheck` = 1
(REG_DWORD). Details: [README](README.md#update-notice).

### Reporting

Report certificate misuse or suspicious files signed on behalf of SignBridge as described in [SECURITY.md](SECURITY.md).
