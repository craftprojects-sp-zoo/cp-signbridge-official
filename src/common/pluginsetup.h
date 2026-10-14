// SignBridge — © 2026 Craftprojects Sp. z o.o. (craftprojects.pl)
// cp-signbridge — instalacja wtyczki mstsc, skrót na pulpicie, autostart (komputer użytkownika)
#pragma once
#include <windows.h>
#include <string>

namespace sb { namespace pluginsetup {

struct Status {
    bool registered = false;    // wpis w HKCU (konto) albo HKLM (cały komputer) ...\Terminal Server Client\Default\AddIns
    bool machine = false;       // rejestracja w HKLM (instalacja MSI / --machine)
    bool fileExists = false;
    std::wstring path;          // ścieżka zarejestrowanej DLL
    std::wstring version;       // wersja pliku DLL
};

Status GetStatus();

// Kopiuje cpsignbridge-rdp.dll (x64 i x86, jeśli są) z katalogu `fromDir` (albo jego podkatalogów x64/x86)
// do %LOCALAPPDATA%\cp-signbridge i rejestruje dla bieżącego użytkownika. msg: opis wyniku.
// machine=true: DLL do targetDir (np. Program Files\cp-signbridge) i rejestracja w HKLM dla wszystkich użytkowników (wymaga administratora)
bool Install(const std::wstring& fromDir, std::wstring& msg, bool machine = false, const std::wstring& targetDir = L"");
bool Uninstall(std::wstring& msg, bool machine = false);

// Czy jakiś mstsc.exe działa (wtyczka ładuje się dopiero przy starcie mstsc)
bool IsMstscRunning();

bool CreateDesktopShortcut(const std::wstring& targetExe, const std::wstring& args, const std::wstring& name, std::wstring& msg, bool allUsers = false);
bool SetAutostart(const std::wstring& valueName, const std::wstring& command, bool on);
bool IsAutostart(const std::wstring& valueName);

std::wstring InstallDir();   // %LOCALAPPDATA%\cp-signbridge
std::wstring ExeDir();
std::wstring ExePath();
std::wstring FileVersion(const std::wstring& path);

}}  // namespace sb::pluginsetup
