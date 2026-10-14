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
void LogInit(const wchar_t* component);

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
