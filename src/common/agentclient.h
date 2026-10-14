// SignBridge — © 2026 Craftprojects Sp. z o.o. (craftprojects.pl)
// cp-signbridge — klient wtyczki mstsc (strona serwera): kanał wirtualny bieżącej sesji RDP
//
// Jedyny transport: kanał wirtualny RDP (dvc.h). Bez adresów, portów i tokenów —
// uwierzytelnieniem jest samo logowanie do sesji RDP.
#pragma once
#include <windows.h>
#include <ncrypt.h>
#include <string>
#include <vector>
#include <memory>
#include "dvc.h"
#include "protocol.h"

namespace sb {

class AgentClient {
public:
    // Otwiera kanał i przedstawia się wtyczce. clientName trafia do logu wtyczki.
    SECURITY_STATUS Open(const std::wstring& clientName, std::wstring& err);

    // Żądanie + odpowiedź
    bool Call(uint16_t type, const std::vector<uint8_t>& payload, uint16_t& rtype, std::vector<uint8_t>& rpayload, DWORD timeoutMs);

    // Wygodne operacje
    SECURITY_STATUS ListCerts(proto::Certs& out, std::wstring& err);
    SECURITY_STATUS Sign(const proto::SignReq& req, std::vector<uint8_t>& sig, std::wstring& err, DWORD timeoutMs);

    const std::wstring& AgentInfo() const { return info_; }

private:
    std::unique_ptr<dvc::ServerChannel> t_;
    std::wstring info_;
};

}  // namespace sb
