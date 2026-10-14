// SignBridge — © 2026 Craftprojects Sp. z o.o. (craftprojects.pl)
// cp-signbridge — logowanie KSP (implementacja)
#include "log.h"
#include <shlobj.h>
#include <stdio.h>
#include <stdarg.h>
#include <string>
#include <vector>

#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "advapi32.lib")

namespace sb {

namespace {

CRITICAL_SECTION g_cs;
bool g_csInit = false;
std::wstring g_logPath;
std::wstring g_component;
bool g_enabled = false;

std::wstring ProgramDataDir() {
    wchar_t buf[MAX_PATH] = {};
    if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_COMMON_APPDATA, NULL, SHGFP_TYPE_CURRENT, buf))) {
        return buf;
    }
    return L"C:\\ProgramData";
}

void EnsureDir(const std::wstring& dir) {
    // Tworzy katalogi po kolei (bez SHCreateDirectoryEx, żeby nie zależeć od ACL-i)
    std::wstring cur;
    for (size_t i = 0; i < dir.size(); ++i) {
        cur += dir[i];
        if ((dir[i] == L'\\' || i + 1 == dir.size()) && cur.size() > 3) {
            CreateDirectoryW(cur.c_str(), NULL);
        }
    }
}

const wchar_t* LevelName(Level l) {
    switch (l) {
        case Level::Trace: return L"TRACE";
        case Level::Info:  return L"INFO ";
        case Level::Warn:  return L"WARN ";
        case Level::Error: return L"ERROR";
    }
    return L"?";
}

}  // namespace

std::wstring ProcessName() {
    wchar_t path[MAX_PATH] = {};
    GetModuleFileNameW(NULL, path, MAX_PATH);
    std::wstring p(path);
    size_t pos = p.find_last_of(L"\\/");
    return pos == std::wstring::npos ? p : p.substr(pos + 1);
}

std::wstring UserName() {
    wchar_t buf[256] = {};
    DWORD n = 256;
    if (GetUserNameW(buf, &n)) return buf;
    return L"?";
}

DWORD SessionId() {
    DWORD sid = 0;
    ProcessIdToSessionId(GetCurrentProcessId(), &sid);
    return sid;
}

std::wstring StatusText(LONG status) {
    wchar_t* msg = nullptr;
    std::wstring out;
    DWORD n = FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                             NULL, (DWORD)status, 0, (LPWSTR)&msg, 0, NULL);
    if (!n) {
        // kody NTSTATUS (0xC000....) opisuje ntdll
        HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
        if (ntdll) n = FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_HMODULE | FORMAT_MESSAGE_IGNORE_INSERTS,
                                      ntdll, (DWORD)status, 0, (LPWSTR)&msg, 0, NULL);
    }
    if (n && msg) {
        out = msg;
        LocalFree(msg);
        while (!out.empty() && (out.back() == L'\r' || out.back() == L'\n' || out.back() == L' ')) out.pop_back();
    }
    wchar_t hex[32];
    swprintf_s(hex, L"0x%08X", (unsigned)status);
    if (out.empty()) return hex;
    return std::wstring(hex) + L" (" + out + L")";
}

void LogInit(const wchar_t* component) {
    if (!g_csInit) {
        InitializeCriticalSection(&g_cs);
        g_csInit = true;
    }
    EnterCriticalSection(&g_cs);
    if (g_logPath.empty()) {
        g_component = component ? component : L"ksp";
        std::wstring dir = ProgramDataDir() + L"\\cp-signbridge\\logs";
        EnsureDir(dir);
        std::wstring proc = ProcessName();
        // usuń rozszerzenie .exe dla czytelności
        size_t dot = proc.find_last_of(L'.');
        if (dot != std::wstring::npos) proc = proc.substr(0, dot);
        wchar_t name[512];
        swprintf_s(name, L"%s\\%s-%s-%lu.log", dir.c_str(), g_component.c_str(), proc.c_str(), GetCurrentProcessId());
        g_logPath = name;
        g_enabled = true;
    }
    LeaveCriticalSection(&g_cs);
}

void LogWrite(Level level, const wchar_t* fmt, ...) {
    if (!g_enabled) LogInit(L"ksp");
    if (!g_enabled) return;

    wchar_t msg[4096];
    va_list ap;
    va_start(ap, fmt);
    _vsnwprintf_s(msg, _countof(msg), _TRUNCATE, fmt, ap);
    va_end(ap);

    SYSTEMTIME st;
    GetLocalTime(&st);
    wchar_t line[4600];
    _snwprintf_s(line, _countof(line), _TRUNCATE, L"%04u-%02u-%02u %02u:%02u:%02u.%03u | pid %5lu | tid %5lu | %s | %s\r\n",
                 st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds,
                 GetCurrentProcessId(), GetCurrentThreadId(), LevelName(level), msg);

    // UTF-8 do pliku
    int need = WideCharToMultiByte(CP_UTF8, 0, line, -1, NULL, 0, NULL, NULL);
    std::vector<char> utf8(need > 0 ? need : 1);
    WideCharToMultiByte(CP_UTF8, 0, line, -1, utf8.data(), need, NULL, NULL);

    EnterCriticalSection(&g_cs);
    HANDLE h = CreateFileW(g_logPath.c_str(), FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                           OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h != INVALID_HANDLE_VALUE) {
        DWORD written = 0;
        WriteFile(h, utf8.data(), need > 0 ? (DWORD)(need - 1) : 0, &written, NULL);
        CloseHandle(h);
    }
    LeaveCriticalSection(&g_cs);
}

}  // namespace sb
