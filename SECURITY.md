# Bezpieczeństwo / Security

## Zgłaszanie podatności

Jeśli znajdziesz błąd bezpieczeństwa w SignBridge, **nie zgłaszaj go w publicznym zgłoszeniu (Issue)**.
Napisz prywatnie na adres **[biuro@craftprojects.pl](mailto:biuro@craftprojects.pl)** z tematem
„SignBridge — bezpieczeństwo”. Możesz też użyć prywatnego zgłoszenia w GitHubie
(**Security → Report a vulnerability**), jeśli jest włączone w tym repozytorium.

W zgłoszeniu podaj, jeśli to możliwe:

* wersję SignBridge (panel → nagłówek lub `HKLM\Software\cp-signbridge\Version`) i wersję Windows (serwer / komputer),
* opis problemu i jego możliwy wpływ,
* kroki do odtworzenia lub dowód koncepcji,
* fragmenty logów z `C:\ProgramData\cp-signbridge\logs\` — **bez** danych osobowych, certyfikatów i PIN-ów.

Potwierdzimy otrzymanie zgłoszenia, ocenimy je i poinformujemy o planowanej poprawce.
Prosimy o nieujawnianie szczegółów publicznie, dopóki poprawka nie zostanie wydana.

## Wspierane wersje

| Wersja | Wsparcie |
|---|---|
| 1.0.x | ✅ |
| < 1.0 | ❌ |

## Zakres

W zakresie: dostawca kluczy CNG (`cpsignbridge.dll`), wtyczka mstsc (`cpsignbridge-rdp.dll`), program
`SignBridge.exe`, instalatory EXE i MSI z oficjalnych wydań oraz skrypty budowania z tego repozytorium.

Poza zakresem: podatności Windows, klienta RDP, sterowników i aplikacji dostawców kart oraz wersje
zmodyfikowane przez osoby trzecie.

---

## Reporting a vulnerability

Please **do not open a public issue**. Report security problems privately by e-mail to
**[biuro@craftprojects.pl](mailto:biuro@craftprojects.pl)** (subject: "SignBridge — security"), or via GitHub
private vulnerability reporting if it is enabled for this repository. Include the SignBridge and Windows versions,
a description, reproduction steps and relevant log excerpts (without personal data, certificates or PINs).
We will acknowledge the report and keep you informed about the fix; please do not disclose details publicly
until a fixed release is available.
