// SignBridge — © 2026 Craftprojects Sp. z o.o. (craftprojects.pl)
// cp-signbridge — uruchamianie testów: kod wyjścia 0 = wszystkie przeszły
#include "check.h"
#include "kspsetup.h"

namespace t {
std::vector<Case>& Registry() { static std::vector<Case> r; return r; }
int g_failures = 0;

std::wstring TempDir(const wchar_t* name) {
    wchar_t tmp[MAX_PATH]; GetTempPathW(MAX_PATH, tmp);
    std::wstring d = std::wstring(tmp) + L"sbtests-" + std::to_wstring(GetCurrentProcessId()) + L"-" + name;
    sb::kspsetup::RemoveDirectoryTree(d);
    CreateDirectoryW(d.c_str(), NULL);
    return d;
}
bool NoSkip() { wchar_t v[8] = {}; return GetEnvironmentVariableW(L"SB_TESTS_NO_SKIP", v, 8) && v[0] == L'1'; }
bool Exists(const std::wstring& p) { return GetFileAttributesW(p.c_str()) != INVALID_FILE_ATTRIBUTES; }
void WriteFileText(const std::wstring& p, const char* text) {
    HANDLE f = CreateFileW(p.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    if (f == INVALID_HANDLE_VALUE) return;
    DWORD wr; WriteFile(f, text, (DWORD)strlen(text), &wr, NULL);
    CloseHandle(f);
}
bool MakeJunction(const std::wstring& link, const std::wstring& target) {
    wchar_t sys[MAX_PATH]; GetSystemDirectoryW(sys, MAX_PATH);
    std::wstring cmd = L"\"" + std::wstring(sys) + L"\\cmd.exe\" /c mklink /J \"" + link + L"\" \"" + target + L"\" >nul";
    STARTUPINFOW si = { sizeof(si) }; PROCESS_INFORMATION pi = {};
    if (!CreateProcessW(NULL, &cmd[0], NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) return false;
    WaitForSingleObject(pi.hProcess, 30000);
    CloseHandle(pi.hThread); CloseHandle(pi.hProcess);
    DWORD a = GetFileAttributesW(link.c_str());
    return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_REPARSE_POINT);
}
}  // namespace t

int main() {
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    setvbuf(stdout, NULL, _IONBF, 0);   // wynik widoczny także, gdy test zakończy proces awarią
    std::printf("SignBridge — testy (%s)\n", sizeof(void*) == 8 ? "x64" : "x86");
    for (auto& c : t::Registry()) {
        int before = t::g_failures;
        std::printf("[ RUN  ] %s\n", c.name);
        c.fn();
        std::printf(t::g_failures == before ? "[  OK  ] %s\n" : "[ FAIL ] %s\n", c.name);
    }
    std::printf("%d test(ów), błędów: %d\n", (int)t::Registry().size(), t::g_failures);
    return t::g_failures ? 1 : 0;
}
