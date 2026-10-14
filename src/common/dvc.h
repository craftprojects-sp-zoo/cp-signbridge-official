// SignBridge — © 2026 Craftprojects Sp. z o.o. (craftprojects.pl)
// cp-signbridge — strona serwera: wirtualny kanał RDP (Dynamic Virtual Channel)
//
// Otwiera dynamiczny kanał "CPSIGNBR" w BIEŻĄCEJ sesji RDP. Drugi koniec kanału to wtyczka
// mstsc na komputerze, z którego użytkownik się zalogował (cpsignbridge-rdp.dll).
// Żadnych adresów, portów ani reguł zapory — kanał żyje wewnątrz połączenia RDP.
#pragma once
#include <windows.h>
#include <string>
#include <vector>
#include <cstdint>

namespace sb { namespace dvc {

const char* const kChannelName = "CPSIGNBR";

// Czy proces działa w sesji zdalnej (tylko wtedy kanał ma sens)
bool InRemoteSession();

class ServerChannel {
public:
    ServerChannel();
    ~ServerChannel();
    ServerChannel(const ServerChannel&) = delete;
    ServerChannel& operator=(const ServerChannel&) = delete;
    bool Open(std::wstring& err);
    void Close();
    bool Send(uint16_t type, const std::vector<uint8_t>& payload);
    // Czeka na ramkę; timeoutMs = 0 -> bez limitu
    bool Recv(uint16_t& type, std::vector<uint8_t>& payload, DWORD timeoutMs);

private:
    bool ReadChunk(std::vector<uint8_t>& out, DWORD timeoutMs);
    HANDLE hChannel_;
    HANDLE hFile_;
    HANDLE hEvent_;
    std::vector<uint8_t> pending_;  // dane już odebrane, jeszcze nie sparsowane
};

}}  // namespace sb::dvc
