// SignBridge — © 2026 Craftprojects Sp. z o.o. (craftprojects.pl)
// cp-signbridge — operacje po stronie serwera używane przez sbctl i panel:
// synchronizacja certyfikatów z komputera użytkownika, test podpisu, stan instalacji KSP
#pragma once
#include <windows.h>
#include <ncrypt.h>
#include <string>
#include <vector>

namespace sb { namespace syncops {

const wchar_t* const kProviderName = L"SignBridge Key Storage Provider";

struct CertRow {
    std::wstring thumbprint;
    std::wstring subject;
    std::wstring issuer;
    std::wstring notAfter;   // yyyy-mm-dd
    std::wstring source;     // "agent" | "serwer (sync)" | "serwer (sonda)"
};

bool IsKspInstalled();

// Łączy się z agentem (wg konfiguracji transportu) i pobiera listę. info = użytkownik@komputer (transport)
SECURITY_STATUS ListAgentCerts(std::vector<CertRow>& rows, std::wstring& info, std::wstring& err);

// Certyfikaty w magazynie Osobiste na serwerze wgrane przez sync (marker)
void ListSyncedCerts(std::vector<CertRow>& rows);

// Pełna synchronizacja. log: linie do pokazania użytkownikowi.
SECURITY_STATUS Sync(std::vector<std::wstring>& log, int& added, int& removed);

// Usuwa wszystkie certyfikaty z markerem
int Unsync();

// Test podpisu zdalnego pierwszym zsynchronizowanym certyfikatem (lub o podanym odcisku)
SECURITY_STATUS TestSign(const std::wstring& thumbprint, std::vector<std::wstring>& log);

std::wstring CertDate(const FILETIME& ft);

}}  // namespace sb::syncops
