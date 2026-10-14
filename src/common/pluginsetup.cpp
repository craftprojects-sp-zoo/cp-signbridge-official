// SignBridge — © 2026 Craftprojects Sp. z o.o. (craftprojects.pl)
// cp-signbridge — instalacja wtyczki mstsc, skrót, autostart (implementacja)
#include "pluginsetup.h"
#include <shlobj.h>
#include <shobjidl.h>
#include <tlhelp32.h>
#include <stdlib.h>

#pragma comment(lib, "version.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "advapi32.lib")

namespace sb { namespace pluginsetup {
namespace {

const wchar_t* const kAddInKey64 = L"Software\\Microsoft\\Terminal Server Client\\Default\\AddIns\\cpsignbridge";
const wchar_t* const kAddInKey32 = L"Software\\WOW6432Node\\Microsoft\\Terminal Server Client\\Default\\AddIns\\cpsignbridge";
const wchar_t* const kDll = L"cpsignbridge-rdp.dll";

bool FileExists(const std::wstring& p) { return GetFileAttributesW(p.c_str()) != INVALID_FILE_ATTRIBUTES; }

std::wstring LocalAppData() {
    wchar_t buf[MAX_PATH] = {};
    SHGetFolderPathW(NULL, CSIDL_LOCAL_APPDATA, NULL, SHGFP_TYPE_CURRENT, buf);
    return buf;
}

std::wstring BaseDir() { return LocalAppData() + L"\\cp-signbridge"; }

}  // namespace

std::wstring InstallDir() { return BaseDir(); }
std::wstring ExePath() { wchar_t p[MAX_PATH]; GetModuleFileNameW(NULL, p, MAX_PATH); return p; }
std::wstring ExeDir() { std::wstring s = ExePath(); return s.substr(0, s.find_last_of(L"\\/")); }

std::wstring FileVersion(const std::wstring& path) {
    DWORD h = 0;
    DWORD cb = GetFileVersionInfoSizeW(path.c_str(), &h);
    if (!cb) return L"";
    std::wstring buf(cb, L'\0');
    if (!GetFileVersionInfoW(path.c_str(), 0, cb, &buf[0])) return L"";
    VS_FIXEDFILEINFO* ffi = nullptr; UINT len = 0;
    if (!VerQueryValueW(&buf[0], L"\\", (LPVOID*)&ffi, &len) || !ffi) return L"";
    wchar_t v[64];
    swprintf_s(v, L"%u.%u.%u.%u", HIWORD(ffi->dwFileVersionMS), LOWORD(ffi->dwFileVersionMS), HIWORD(ffi->dwFileVersionLS), LOWORD(ffi->dwFileVersionLS));
    return v;
}

Status GetStatus() {
    Status st;
    for (HKEY root : { HKEY_CURRENT_USER, HKEY_LOCAL_MACHINE }) {
        HKEY h;
        if (RegOpenKeyExW(root, kAddInKey64, 0, KEY_READ | KEY_WOW64_64KEY, &h) != ERROR_SUCCESS) continue;
        wchar_t path[MAX_PATH] = {}; DWORD cb = sizeof(path);
        if (RegQueryValueExW(h, L"Name", NULL, NULL, (BYTE*)path, &cb) == ERROR_SUCCESS) { st.registered = true; st.machine = root == HKEY_LOCAL_MACHINE; st.path = path; }
        RegCloseKey(h);
        if (st.registered) break;
    }
    if (st.registered) { st.fileExists = FileExists(st.path); if (st.fileExists) st.version = FileVersion(st.path); }
    return st;
}

bool Install(const std::wstring& fromDir, std::wstring& msg, bool machine, const std::wstring& targetDir) {
    std::wstring src64 = fromDir + L"\\x64\\" + kDll;
    if (!FileExists(src64)) src64 = fromDir + L"\\" + kDll;
    std::wstring src86 = fromDir + L"\\x86\\" + kDll;
    if (!FileExists(src64)) { msg = L"Nie znaleziono " + src64; return false; }

    HKEY root = machine ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
    std::wstring base = machine && !targetDir.empty() ? targetDir : BaseDir();
    CreateDirectoryW(base.c_str(), NULL);
    CreateDirectoryW((base + L"\\x86").c_str(), NULL);
    std::wstring dst64 = base + L"\\" + kDll;
    std::wstring dst86 = base + L"\\x86\\" + kDll;
    if (!CopyFileW(src64.c_str(), dst64.c_str(), FALSE)) {
        DWORD e = GetLastError();
        msg = (e == ERROR_SHARING_VIOLATION || e == ERROR_ACCESS_DENIED)
            ? L"Plik wtyczki jest w użyciu. Zamknij wszystkie okna pulpitu zdalnego i spróbuj ponownie."
            : L"Nie udało się skopiować wtyczki (błąd " + std::to_wstring(e) + L").";
        return false;
    }
    bool have86 = FileExists(src86) && CopyFileW(src86.c_str(), dst86.c_str(), FALSE);

    HKEY h;
    if (RegCreateKeyExW(root, kAddInKey64, 0, NULL, 0, KEY_WRITE | KEY_WOW64_64KEY, NULL, &h, NULL) != ERROR_SUCCESS) { msg = machine ? L"Nie udało się zapisać rejestracji wtyczki w HKLM (uprawnienia administratora?)." : L"Nie udało się zapisać rejestracji wtyczki."; return false; }
    RegSetValueExW(h, L"Name", 0, REG_SZ, (const BYTE*)dst64.c_str(), (DWORD)((dst64.size() + 1) * sizeof(wchar_t)));
    RegCloseKey(h);
    if (have86 && RegCreateKeyExW(root, kAddInKey32, 0, NULL, 0, KEY_WRITE | KEY_WOW64_64KEY, NULL, &h, NULL) == ERROR_SUCCESS) {
        RegSetValueExW(h, L"Name", 0, REG_SZ, (const BYTE*)dst86.c_str(), (DWORD)((dst86.size() + 1) * sizeof(wchar_t)));
        RegCloseKey(h);
    }
    msg = L"Wtyczka zainstalowana (" + FileVersion(dst64) + (machine ? L", dla wszystkich użytkowników" : L"") + L"). ";
    msg += IsMstscRunning() ? L"Zamknij wszystkie okna pulpitu zdalnego i połącz się ponownie — wtyczka ładuje się przy starcie."
                            : L"Przy następnym połączeniu pulpitu zdalnego będzie aktywna.";
    return true;
}

bool Uninstall(std::wstring& msg, bool machine) {
    HKEY root = machine ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
    Status st = GetStatus();
    std::wstring base = (machine && st.machine && !st.path.empty()) ? st.path.substr(0, st.path.find_last_of(L"\\/")) : BaseDir();
    RegDeleteKeyExW(root, kAddInKey64, KEY_WOW64_64KEY, 0);
    RegDeleteKeyExW(root, kAddInKey32, KEY_WOW64_64KEY, 0);
    DeleteFileW((base + L"\\" + kDll).c_str());
    DeleteFileW((base + L"\\x86\\" + kDll).c_str());
    msg = L"Wtyczka wyrejestrowana.";
    return true;
}

bool IsMstscRunning() {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return false;
    PROCESSENTRY32W pe = { sizeof(pe) };
    bool found = false;
    if (Process32FirstW(snap, &pe)) {
        do { if (_wcsicmp(pe.szExeFile, L"mstsc.exe") == 0) { found = true; break; } } while (Process32NextW(snap, &pe));
    }
    CloseHandle(snap);
    return found;
}

bool CreateDesktopShortcut(const std::wstring& targetExe, const std::wstring& args, const std::wstring& name, std::wstring& msg, bool allUsers) {
    wchar_t desktop[MAX_PATH] = {};
    if (FAILED(SHGetFolderPathW(NULL, allUsers ? CSIDL_COMMON_DESKTOPDIRECTORY : CSIDL_DESKTOPDIRECTORY, NULL, SHGFP_TYPE_CURRENT, desktop))) { msg = L"Brak ścieżki pulpitu"; return false; }
    std::wstring lnk = std::wstring(desktop) + L"\\" + name + L".lnk";
    bool coInit = SUCCEEDED(CoInitializeEx(NULL, COINIT_APARTMENTTHREADED));
    IShellLinkW* sl = nullptr;
    bool ok = false;
    if (SUCCEEDED(CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, IID_IShellLinkW, (void**)&sl))) {
        sl->SetPath(targetExe.c_str());
        sl->SetArguments(args.c_str());
        std::wstring dir = targetExe.substr(0, targetExe.find_last_of(L"\\/"));
        sl->SetWorkingDirectory(dir.c_str());
        sl->SetDescription(L"SignBridge — podpis kwalifikowany przez pulpit zdalny");
        IPersistFile* pf = nullptr;
        if (SUCCEEDED(sl->QueryInterface(IID_IPersistFile, (void**)&pf))) {
            ok = SUCCEEDED(pf->Save(lnk.c_str(), TRUE));
            pf->Release();
        }
        sl->Release();
    }
    if (coInit) CoUninitialize();
    msg = ok ? L"Skrót utworzony na pulpicie: " + name : L"Nie udało się utworzyć skrótu.";
    return ok;
}

bool SetAutostart(const std::wstring& valueName, const std::wstring& command, bool on) {
    HKEY h;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_WRITE, &h) != ERROR_SUCCESS) return false;
    LONG r = on ? RegSetValueExW(h, valueName.c_str(), 0, REG_SZ, (const BYTE*)command.c_str(), (DWORD)((command.size() + 1) * sizeof(wchar_t)))
                : RegDeleteValueW(h, valueName.c_str());
    RegCloseKey(h);
    return r == ERROR_SUCCESS || (!on && r == ERROR_FILE_NOT_FOUND);
}

bool IsAutostart(const std::wstring& valueName) {
    HKEY h;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_READ, &h) != ERROR_SUCCESS) return false;
    DWORD cb = 0;
    bool present = RegQueryValueExW(h, valueName.c_str(), NULL, NULL, NULL, &cb) == ERROR_SUCCESS;
    RegCloseKey(h);
    return present;
}

}}  // namespace sb::pluginsetup
