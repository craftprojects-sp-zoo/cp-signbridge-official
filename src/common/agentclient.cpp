// SignBridge — © 2026 Craftprojects Sp. z o.o. (craftprojects.pl)
// cp-signbridge — klient wtyczki mstsc (implementacja)
#include "agentclient.h"

namespace sb {

SECURITY_STATUS AgentClient::Open(const std::wstring& clientName, std::wstring& err) {
    // Kanał wirtualny bieżącej sesji RDP. Wtyczka w kliencie (mstsc) odpowiada na AUTH komunikatem AUTH_OK
    // z nazwą użytkownika i komputera — bez wyzwania, bo tożsamość daje sesja RDP.
    std::unique_ptr<dvc::ServerChannel> ch(new dvc::ServerChannel());
    if (!ch->Open(err)) return NTE_DEVICE_NOT_READY;
    t_ = std::move(ch);
    proto::Auth a; a.clientVersion = proto::PROTO_VERSION; a.clientName = clientName;
    if (!t_->Send(proto::AUTH, proto::EncodeAuth(a))) { err = L"kanał RDP: wysyłka AUTH"; return NTE_DEVICE_NOT_READY; }
    uint16_t type = 0; std::vector<uint8_t> payload;
    if (!t_->Recv(type, payload, 10000)) { err = L"kanał RDP otwarty, ale wtyczka w kliencie nie odpowiada (cpsignbridge-rdp.dll zainstalowana? mstsc zrestartowany?)"; return NTE_DEVICE_NOT_READY; }
    if (type == proto::ERR) { proto::Error e; proto::DecodeError(payload, e); err = L"wtyczka: " + e.message; return NTE_PERM; }
    if (type != proto::AUTH_OK) { err = L"kanał RDP: nieoczekiwana odpowiedź"; return NTE_DEVICE_NOT_READY; }
    proto::AuthOk ok; proto::DecodeAuthOk(payload, ok);
    info_ = ok.agentUser + L"@" + ok.agentHost + L" (kanał RDP)";
    return ERROR_SUCCESS;
}

bool AgentClient::Call(uint16_t type, const std::vector<uint8_t>& payload, uint16_t& rtype, std::vector<uint8_t>& rpayload, DWORD timeoutMs) {
    if (!t_ || !t_->Send(type, payload)) return false;
    return t_->Recv(rtype, rpayload, timeoutMs);
}

SECURITY_STATUS AgentClient::ListCerts(proto::Certs& out, std::wstring& err) {
    uint16_t rtype = 0; std::vector<uint8_t> rpayload;
    if (!Call(proto::LIST_CERTS, {}, rtype, rpayload, 15000)) { err = L"brak odpowiedzi na LIST_CERTS"; return NTE_DEVICE_NOT_READY; }
    if (rtype == proto::ERR) { proto::Error e; proto::DecodeError(rpayload, e); err = e.message; return e.code ? (SECURITY_STATUS)e.code : NTE_FAIL; }
    if (rtype != proto::CERTS || !proto::DecodeCerts(rpayload, out)) { err = L"zła lista certyfikatów"; return NTE_FAIL; }
    return ERROR_SUCCESS;
}

SECURITY_STATUS AgentClient::Sign(const proto::SignReq& req, std::vector<uint8_t>& sig, std::wstring& err, DWORD timeoutMs) {
    uint16_t rtype = 0; std::vector<uint8_t> rpayload;
    if (!Call(proto::SIGN, proto::EncodeSign(req), rtype, rpayload, timeoutMs)) { err = L"brak odpowiedzi na SIGN"; return NTE_DEVICE_NOT_READY; }
    if (rtype == proto::ERR) { proto::Error e; proto::DecodeError(rpayload, e); err = e.message; return e.code ? (SECURITY_STATUS)e.code : NTE_FAIL; }
    proto::SignOk ok;
    if (rtype != proto::SIGN_OK || !proto::DecodeSignOk(rpayload, ok)) { err = L"zła odpowiedź SIGN"; return NTE_FAIL; }
    sig = ok.signature;
    return ERROR_SUCCESS;
}

}  // namespace sb
