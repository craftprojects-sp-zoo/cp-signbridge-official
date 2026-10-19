// SignBridge — © 2026 Craftprojects Sp. z o.o. (craftprojects.pl)
// cp-signbridge — logowanie KSP
// Prosty, bezpieczny wątkowo i międzyprocesowo log tekstowy.
// Każda linia: czas | pid | tid | proces | poziom | komunikat
#pragma once
#include <windows.h>
#include <string>

namespace sb {

enum class Level { Trace, Info, Warn, Error };

// Inicjalizacja: ustala katalog logów (%ProgramData%\cp-signbridge\logs) i nazwę pliku
// ksp-<nazwa_procesu>-<pid>.log. Bezpieczna do wielokrotnego wywołania.
// Przy starcie usuwa własne pliki tego składnika starsze niż kLogKeepDays.
void LogInit(const wchar_t* component);
void LogInitAt(const std::wstring& dir, const wchar_t* component);  // inny katalog (testy)
std::wstring LogPath();

// Limit pliku: po przekroczeniu bieżący plik przechodzi do <nazwa>.log.old (jedna kopia)
const ULONGLONG kMaxLogBytes = 5ull * 1024 * 1024;
const int kLogKeepDays = 30;

// Czy właścicielem pliku jest bieżący użytkownik (albo domyślny właściciel jego tokenu).
// Log piszemy tylko do własnego pliku, a panel pokazuje tylko własne logi.
bool FileOwnedByCurrentUser(HANDLE h);

void LogWrite(Level level, const wchar_t* fmt, ...);

// Pomocnicze: nazwa procesu, użytkownik, sesja
std::wstring ProcessName();
std::wstring UserName();
DWORD SessionId();

// Zamiana HRESULT/SECURITY_STATUS na tekst
std::wstring StatusText(LONG status);

}  // namespace sb

#define SB_TRACE(...) ::sb::LogWrite(::sb::Level::Trace, __VA_ARGS__)
#define SB_INFO(...)  ::sb::LogWrite(::sb::Level::Info,  __VA_ARGS__)
#define SB_WARN(...)  ::sb::LogWrite(::sb::Level::Warn,  __VA_ARGS__)
#define SB_ERROR(...) ::sb::LogWrite(::sb::Level::Error, __VA_ARGS__)
