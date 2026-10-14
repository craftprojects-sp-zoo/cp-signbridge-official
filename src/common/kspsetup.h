// SignBridge — © 2026 Craftprojects Sp. z o.o. (craftprojects.pl)
// cp-signbridge — instalacja po stronie serwera (wymaga administratora):
//   * cpsignbridge.dll -> System32 / SysWOW64, rejestracja dostawcy (KSP) w CNG
//   * sbpanel.exe, sbctl.exe -> %ProgramFiles%\cp-signbridge
//   * autostart panelu dla wszystkich użytkowników (HKLM\...\Run) — panel w sesji RDP
//     sam pobiera certyfikaty z komputera użytkownika
//   * katalog logów %ProgramData%\cp-signbridge\logs z prawami zapisu dla wszystkich
// Używane przez sbctl install/uninstall i sbpanel --install-server/--uninstall-server.
#pragma once
#include <windows.h>
#include <string>
#include <vector>

namespace sb { namespace kspsetup {

const wchar_t* const kProviderName = L"SignBridge Key Storage Provider";
// nazwa zainstalowanego programu (plik dystrybucyjny nazywa się "... Installer.exe", po instalacji — tak)
const wchar_t* const kAppExe = L"SignBridge.exe";

bool IsAdmin();
bool IsKspRegistered();                 // NCryptOpenStorageProvider działa
std::wstring ProgramDir();              // %ProgramFiles%\cp-signbridge
std::wstring ProgramDataDir();          // %ProgramData%\cp-signbridge
std::wstring InstalledPanelVersion();   // wersja kAppExe w ProgramDir ("" gdy brak)

// fromDir: katalog pakietu (kAppExe, x64\, x86\; opcjonalnie sbctl.exe). log: kolejne komunikaty.
// msiMode=true (wywołanie z pakietu MSI): program już leży w ProgramDir i wpis w „Aplikacje i funkcje” robi MSI
bool InstallServer(const std::wstring& fromDir, std::vector<std::wstring>& log, bool msiMode = false);

// Wpis w „Aplikacje i funkcje” + Software\cp-signbridge (Version, InstallDir, InstalledAt). root: HKLM (serwer) / HKCU (komputer)
void WriteInstallRegistry(HKEY root, const std::wstring& installDir, const std::wstring& exePath, const std::wstring& version, const wchar_t* role);
void RemoveInstallRegistry(HKEY root);
bool UninstallServer(std::vector<std::wstring>& log, bool msiMode = false);

// Sprzątanie: cały katalog (pliki w użyciu — usunięcie po restarcie); klucz Software\cp-signbridge
// we wszystkich profilach użytkowników (HKEY_USERS) — wymaga administratora; w bieżącym profilu (HKCU)
void RemoveDirectoryTree(const std::wstring& dir);
void RemoveAllUsersConfig();
void RemoveCurrentUserConfig();

}}  // namespace sb::kspsetup
