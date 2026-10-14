// SignBridge — © 2026 Craftprojects Sp. z o.o. (craftprojects.pl)
// cp-signbridge — strona użytkownika: wystawianie certyfikatów i podpis lokalną kartą.
// Używane przez wtyczkę mstsc (kanał RDP), panel i sbagent.exe (lista certyfikatów).
#pragma once
#include <windows.h>
#include <ncrypt.h>
#include <string>
#include <vector>
#include "protocol.h"

namespace sb { namespace signer {

// Certyfikaty z kluczem prywatnym, nie wygasłe, nie od cp-signbridge (ochrona przed pętlą).
// print=true wypisuje listę na konsolę.
void CollectCerts(proto::Certs& out, bool print);

// Podpis skrótu kluczem certyfikatu o podanym odcisku (CNG albo CAPI). hwndOwner: okno
// nadrzędne dla dialogu PIN sterownika karty (może być NULL).
SECURITY_STATUS Sign(const proto::SignReq& req, HWND hwndOwner, std::vector<uint8_t>& sig,
                     std::wstring& subject, std::wstring& err);

// Obsługa jednego żądania protokołu (LIST_CERTS / SIGN / PING) -> odpowiedź.
// Zwraca typ i treść odpowiedzi. logPrefix trafia do logu (np. adres klienta).
void HandleRequest(uint16_t type, const std::vector<uint8_t>& payload, HWND hwndOwner,
                   const std::wstring& logPrefix, uint16_t& rtype, std::vector<uint8_t>& rpayload);

}}  // namespace sb::signer
