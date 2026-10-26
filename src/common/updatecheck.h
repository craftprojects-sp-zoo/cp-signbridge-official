// SignBridge — © 2026 Craftprojects Sp. z o.o. (craftprojects.pl)
// cp-signbridge — powiadomienie o nowej wersji (CP-SB-19)
//
// Program tylko SPRAWDZA, czy w GitHub Releases jest nowsza wersja, i informuje użytkownika.
// Niczego nie pobiera ani nie instaluje. Przycisk „Aktualizuj” otwiera stały adres strony wydań
// (kReleasePageUrl) — nigdy adres z odpowiedzi serwera.
//
// Zapytanie: HTTPS GET (WinHTTP, weryfikacja certyfikatu przez Windows, bez przekierowań, bez tokenu,
// bez ciasteczek) na stały adres GitHub Releases API. Wysyłane są tylko standardowe dane zapytania HTTPS
// (adres IP, User-Agent „SignBridge/<wersja>”, nagłówki Accept i — przy kolejnych sprawdzeniach — ETag).
//
// Wyłączenie (GPO): HKLM\SOFTWARE\Policies\Craftprojects\SignBridge, DWORD DisableUpdateCheck = 1.
#pragma once
#include <windows.h>
#include <cstdint>
#include <string>

namespace sb::update {

// Stałe adresy (testy sprawdzają, że są dokładnie takie)
extern const wchar_t* const kApiHost;          // api.github.com
extern const wchar_t* const kApiPath;          // /repos/craftprojects-sp-zoo/cp-signbridge-official/releases/latest
extern const wchar_t* const kReleasePageUrl;   // https://github.com/craftprojects-sp-zoo/cp-signbridge-official/releases/latest

// Polityka (GPO)
extern const wchar_t* const kPolicyKey;        // SOFTWARE\Policies\Craftprojects\SignBridge
extern const wchar_t* const kPolicyValue;      // DisableUpdateCheck

const DWORD kTimeoutMs = 5000;                    // każdy etap zapytania (DNS, połączenie, wysłanie, odpowiedź)
const size_t kMaxResponseBytes = 1024 * 1024;     // dłuższa odpowiedź = błąd
const ULONGLONG kCheckIntervalSec = 24ull * 3600; // najwyżej raz na 24 h (na użytkownika)
const ULONGLONG kRemindLaterSec = 7ull * 24 * 3600;

// Adres, który otwiera przycisk „Aktualizuj” — zawsze kReleasePageUrl
const wchar_t* UpdateButtonUrl();

// Wersja: 1–4 liczby 0…65535 rozdzielone kropkami, opcjonalnie z przedrostkiem „v”/„V” („v1.1.0”, „1.1.0.0”).
// Brakujące składowe = 0. Bez spacji, znaków po wersji (np. „-rc1”) i zer wiodących powyżej 5 cyfr.
struct Version { uint16_t v[4] = { 0, 0, 0, 0 }; };
bool ParseVersion(const std::string& s, Version& out);
bool ParseVersion(const std::wstring& s, Version& out);
int Compare(const Version& a, const Version& b);     // <0, 0, >0
std::wstring ToDisplay(const Version& v);            // „1.1.0” (czwarta składowa tylko, gdy ≠ 0)

// Wynik parsera odpowiedzi API: tylko pola najwyższego poziomu tag_name / draft / prerelease.
struct ReleaseInfo {
    std::string tag;            // surowe tag_name (≤ kMaxTagLen znaków)
    bool hasTag = false;
    bool draft = false;
    bool prerelease = false;
};
const size_t kMaxTagLen = 64;
const int kMaxDepth = 32;
// false: niepoprawny / obcięty JSON, zła struktura, za głęboko zagnieżdżony, zbyt długi tag_name, brak tag_name.
bool ParseRelease(const char* data, size_t len, ReleaseInfo& out);

// Rozstrzygnięcie: czy odpowiedź opisuje wydaną (nie draft / prerelease) wersję nowszą niż `current`.
// newer = sparsowana wersja z tag_name.
enum class Verdict { Newer, NotNewer, Ignored /*draft, prerelease, zły tag*/ };
Verdict Decide(const ReleaseInfo& r, const Version& current, Version& newer);

// Czy minęło 24 h od ostatniego sprawdzenia (czasy w sekundach UTC; zegar cofnięty = sprawdź)
bool CheckDue(ULONGLONG lastCheck, ULONGLONG now);
// Czy pokazać powiadomienie: zapamiętana najnowsza wersja `latest` jest nowsza niż `current`
// i użytkownik nie odłożył jej („Przypomnij później”) — odłożenie dotyczy tylko tej wersji i do remindAfter.
bool ShouldShow(const std::wstring& latest, const Version& current, const std::wstring& snoozedVersion,
                ULONGLONG remindAfter, ULONGLONG now, Version& shown);
ULONGLONG NowSeconds();

// Pobrany instalator („SignBridge Installer.exe”) uruchomiony, gdy w tej sesji działa już SignBridge (CP-SB-20):
// czy zamknąć działającą kopię i kontynuować jako instalator? Tak tylko, gdy działa zainstalowany program
// (runningExe == installedExe, bez względu na wielkość liter), a instalator ma inną wersję niż zainstalowana.
// Inaczej (drugi instalator, ta sama wersja) — jak dotąd: tylko pokaż działające okno.
bool CloseRunningForInstall(bool installerFile, const std::wstring& runningExe, const std::wstring& installedExe,
                            const std::wstring& installedVersion, const std::wstring& myVersion);

// Czy sprawdzanie wyłączył administrator (HKLM, GPO)
bool DisabledByPolicy();

// Zapytanie do GitHub (blokujące — wywoływać w wątku roboczym).
struct FetchResult {
    bool ok = false;            // 200 z poprawną treścią albo 304
    bool notModified = false;   // 304 (ETag bez zmian)
    DWORD httpStatus = 0;
    DWORD error = 0;            // GetLastError z WinHTTP
    std::string body;
    std::wstring etag;
};
FetchResult Fetch(const std::wstring& userAgent, const std::wstring& ifNoneMatch);

}  // namespace sb::update
