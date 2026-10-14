// SignBridge — © 2026 Craftprojects Sp. z o.o. (craftprojects.pl)
// cp-signbridge — sbagent: narzędzie diagnostyczne na komputerze użytkownika (komputer z kartą)
//
// Podpis wykonuje wtyczka mstsc (cpsignbridge-rdp.dll) — skrót przychodzi kanałem wirtualnym
// sesji RDP. To narzędzie niczego nie nasłuchuje i z niczym się nie łączy; tylko instaluje
// wtyczkę i pokazuje certyfikaty, które wtyczka wystawi serwerowi.
//
//   sbagent plugin-install           instaluje wtyczkę mstsc (kanał RDP) dla bieżącego użytkownika
//   sbagent plugin-uninstall / plugin-status
//   sbagent list                     pokazuje certyfikaty, które wtyczka wystawi
#include <windows.h>
#include <wincrypt.h>
#include <ncrypt.h>
#include <stdio.h>
#include <io.h>
#include <fcntl.h>
#include <string>
#include "../common/protocol.h"
#include "../ksp/log.h"
#include "../common/signer.h"
#include "../common/pluginsetup.h"

#pragma comment(lib, "ncrypt.lib")
#pragma comment(lib, "crypt32.lib")
#pragma comment(lib, "advapi32.lib")

using namespace sb;

namespace {

int CmdList() {
    wprintf(L"Certyfikaty z kluczem prywatnym w magazynie Osobiste (%s):\n", sb::UserName().c_str());
    proto::Certs list;
    signer::CollectCerts(list, true);
    wprintf(L"\nWystawianych: %u\n", (unsigned)list.items.size());
    return 0;
}

// ---------------------------------------------------------------------------
// Wtyczka mstsc (kanał RDP) — instalacja dla bieżącego użytkownika, bez admina
// ---------------------------------------------------------------------------

int CmdPluginInstall() {
    std::wstring msg;
    bool ok = sb::pluginsetup::Install(sb::pluginsetup::ExeDir(), msg);
    wprintf(L"%s %s\n", ok ? L"[+]" : L"[!]", msg.c_str());
    return ok ? 0 : 2;
}
int CmdPluginUninstall() {
    std::wstring msg;
    sb::pluginsetup::Uninstall(msg);
    wprintf(L"[+] %s\n", msg.c_str());
    return 0;
}
int CmdPluginStatus() {
    auto st = sb::pluginsetup::GetStatus();
    wprintf(L"Wtyczka mstsc (użytkownik %s):\n  rejestracja: %s\n  plik: %s %s\n", sb::UserName().c_str(),
            st.registered ? L"jest" : L"BRAK", st.registered ? st.path.c_str() : L"-", st.fileExists ? st.version.c_str() : (st.registered ? L"(BRAK pliku)" : L""));
    wprintf(L"  mstsc uruchomiony: %s\n  logi: %%ProgramData%%\\cp-signbridge\\logs\\agent-mstsc-*.log\n", sb::pluginsetup::IsMstscRunning() ? L"tak (wtyczka załaduje się po ponownym połączeniu)" : L"nie");
    return 0;
}

}  // namespace

int wmain(int argc, wchar_t** argv) {
    _setmode(_fileno(stdout), _O_U8TEXT);
    std::wstring cmd = argc > 1 ? argv[1] : L"";
    if (cmd == L"list") return CmdList();
    if (cmd == L"plugin-install") return CmdPluginInstall();
    if (cmd == L"plugin-uninstall") return CmdPluginUninstall();
    if (cmd == L"plugin-status") return CmdPluginStatus();
    wprintf(L"cp-signbridge sbagent (komputer z kartą)\n\n"
            L"Wtyczka pulpitu zdalnego (kanał RDP):\n"
            L"  sbagent plugin-install | plugin-uninstall | plugin-status\n"
            L"Diagnostyka:\n  sbagent list\n");
    return 1;
}
