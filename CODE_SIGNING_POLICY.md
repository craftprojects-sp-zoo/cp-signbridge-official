# Polityka podpisywania kodu / Code signing policy

[Polski](#polski) · [English](#english)

<a id="polski"></a>

## Polski

### Podpis

**Docelowo:** *Free code signing provided by [SignPath.io](https://signpath.io), certificate by [SignPath Foundation](https://signpath.org).*

> **Stan obecny (2026-10-14):** wniosek do SignPath Foundation czeka na rozpatrzenie. Do czasu akceptacji wydania
> są podpisywane **własnym certyfikatem Craftprojects Sp. z o.o.** (odcisk SHA-1
> `096EC851CB0D6BE8A30AC1CAC7BE85EFE060683B`) — sposób weryfikacji opisuje [README](README.md#weryfikacja-pobranej-paczki).
> Od wydania podpisanego przez SignPath wydawcą w podpisie będzie „SignPath Foundation”; informacja o tym znajdzie
> się w notach wydania.

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

Ten program nie przesyła żadnych informacji do innych systemów sieciowych, chyba że użytkownik wyraźnie tego zażąda.

SignBridge komunikuje się wyłącznie kanałem wirtualnym RDP wewnątrz istniejącej sesji pulpitu zdalnego użytkownika
(skrót do podpisu, gotowy podpis, publiczne części certyfikatów) — między serwerem a komputerem z kartą, na żądanie
użytkownika. Nie ma telemetrii ani automatycznych aktualizacji; jedyny link do internetu (`craftprojects.pl` w stopce
panelu) otwiera przeglądarkę dopiero po kliknięciu. Szczegóły: [README](README.md#bezpieczeństwo-i-sieć).

### Zgłaszanie problemów

Nadużycia certyfikatu lub podejrzane pliki podpisane w imieniu SignBridge zgłaszaj zgodnie z [SECURITY.md](SECURITY.md).

---

<a id="english"></a>

## English

### Signing

**Target:** *Free code signing provided by [SignPath.io](https://signpath.io), certificate by [SignPath Foundation](https://signpath.org).*

> **Current state (2026-10-14):** our application to the SignPath Foundation is pending. Until it is accepted,
> releases are signed with **Craftprojects Sp. z o.o.'s own certificate** (SHA-1 thumbprint
> `096EC851CB0D6BE8A30AC1CAC7BE85EFE060683B`) — see [README](README.md#verify-a-release-download) for verification.
> Releases signed via SignPath will show "SignPath Foundation" as the publisher; the release notes will say so.

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
or the person installing or operating it.

SignBridge communicates only through the RDP virtual channel inside the user's existing Remote Desktop session
(the hash to be signed, the signature and public certificates) — between the server and the PC with the card, on the
user's request. There is no telemetry and no automatic updates; the only internet link (`craftprojects.pl` in the panel
footer) opens a browser only when clicked. Details: [README](README.md#security).

### Reporting

Report certificate misuse or suspicious files signed on behalf of SignBridge as described in [SECURITY.md](SECURITY.md).
