// SignBridge — © 2026 Craftprojects Sp. z o.o. (craftprojects.pl)
// cp-signbridge — strona serwera: wirtualny kanał RDP (implementacja)
#include "dvc.h"
#include "protocol.h"
#include <wtsapi32.h>
#include <pchannel.h>

#pragma comment(lib, "wtsapi32.lib")

namespace sb { namespace dvc {

bool InRemoteSession() { return GetSystemMetrics(SM_REMOTESESSION) != 0; }

ServerChannel::ServerChannel() : hChannel_(NULL), hFile_(INVALID_HANDLE_VALUE), hEvent_(NULL) {}
ServerChannel::~ServerChannel() { Close(); }

bool ServerChannel::Open(std::wstring& err) {
    if (!InRemoteSession()) { err = L"to nie jest sesja zdalna (RDP) — kanał wirtualny niedostępny"; return false; }
    hChannel_ = WTSVirtualChannelOpenEx(WTS_CURRENT_SESSION, (LPSTR)kChannelName, WTS_CHANNEL_OPTION_DYNAMIC);
    if (!hChannel_) {
        DWORD e = GetLastError();
        wchar_t b[128];
        swprintf_s(b, L"WTSVirtualChannelOpenEx: 0x%08X (czy wtyczka cpsignbridge-rdp jest zainstalowana w kliencie RDP?)", e);
        err = b;
        return false;
    }
    PVOID pInfo = NULL;
    DWORD cb = 0;
    if (!WTSVirtualChannelQuery(hChannel_, WTSVirtualFileHandle, &pInfo, &cb) || !pInfo || cb < sizeof(HANDLE)) {
        err = L"WTSVirtualChannelQuery(FileHandle) nieudane";
        Close();
        return false;
    }
    hFile_ = *(HANDLE*)pInfo;
    WTSFreeMemory(pInfo);
    hEvent_ = CreateEventW(NULL, TRUE, FALSE, NULL);
    return true;
}

void ServerChannel::Close() {
    if (hChannel_) { WTSVirtualChannelClose(hChannel_); hChannel_ = NULL; }
    hFile_ = INVALID_HANDLE_VALUE;  // zamykany przez WTSVirtualChannelClose
    if (hEvent_) { CloseHandle(hEvent_); hEvent_ = NULL; }
    pending_.clear();
}

bool ServerChannel::Send(uint16_t type, const std::vector<uint8_t>& payload) {
    if (hFile_ == INVALID_HANDLE_VALUE) return false;
    std::vector<uint8_t> frame(sizeof(proto::FrameHdr) + payload.size());
    proto::FrameHdr h = { proto::kMagic, (uint32_t)payload.size(), type, 0 };
    memcpy(frame.data(), &h, sizeof(h));
    if (!payload.empty()) memcpy(frame.data() + sizeof(h), payload.data(), payload.size());

    OVERLAPPED ov = {};
    ov.hEvent = hEvent_;
    ResetEvent(hEvent_);
    DWORD written = 0;
    BOOL ok = WriteFile(hFile_, frame.data(), (DWORD)frame.size(), &written, &ov);
    if (!ok && GetLastError() != ERROR_IO_PENDING) return false;
    if (!GetOverlappedResult(hFile_, &ov, &written, TRUE)) return false;
    return written == frame.size();
}

// Jedno ReadFile = jeden fragment. Dla kanałów RDP fragment jest poprzedzony CHANNEL_PDU_HEADER
// (length = długość całej wiadomości, flags FIRST/LAST). Na wszelki wypadek obsługujemy też
// surowe dane (gdy pierwsze 4 bajty to nasz magic).
bool ServerChannel::ReadChunk(std::vector<uint8_t>& out, DWORD timeoutMs) {
    std::vector<uint8_t> buf(CHANNEL_PDU_LENGTH + 4096);
    OVERLAPPED ov = {};
    ov.hEvent = hEvent_;
    ResetEvent(hEvent_);
    DWORD got = 0;
    BOOL ok = ReadFile(hFile_, buf.data(), (DWORD)buf.size(), &got, &ov);
    if (!ok) {
        if (GetLastError() != ERROR_IO_PENDING) return false;
        DWORD w = WaitForSingleObject(hEvent_, timeoutMs ? timeoutMs : INFINITE);
        if (w != WAIT_OBJECT_0) { CancelIo(hFile_); return false; }
        if (!GetOverlappedResult(hFile_, &ov, &got, FALSE)) return false;
    }
    if (got == 0) return false;
    buf.resize(got);
    if (got >= 4 && *(uint32_t*)buf.data() == proto::kMagic) { out = buf; return true; }
    if (got < sizeof(CHANNEL_PDU_HEADER)) return false;
    out.assign(buf.begin() + sizeof(CHANNEL_PDU_HEADER), buf.end());
    return true;
}

bool ServerChannel::Recv(uint16_t& type, std::vector<uint8_t>& payload, DWORD timeoutMs) {
    if (hFile_ == INVALID_HANDLE_VALUE) return false;
    ULONGLONG deadline = timeoutMs ? GetTickCount64() + timeoutMs : 0;
    for (;;) {
        if (pending_.size() >= sizeof(proto::FrameHdr)) {
            proto::FrameHdr h;
            memcpy(&h, pending_.data(), sizeof(h));
            if (h.magic != proto::kMagic || h.cbPayload > proto::kMaxPayload) return false;
            size_t need = sizeof(h) + h.cbPayload;
            if (pending_.size() >= need) {
                type = h.type;
                payload.assign(pending_.begin() + sizeof(h), pending_.begin() + need);
                pending_.erase(pending_.begin(), pending_.begin() + need);
                return true;
            }
        }
        DWORD left = 0;
        if (deadline) {
            ULONGLONG now = GetTickCount64();
            if (now >= deadline) return false;
            left = (DWORD)(deadline - now);
        }
        std::vector<uint8_t> chunk;
        if (!ReadChunk(chunk, left)) return false;
        pending_.insert(pending_.end(), chunk.begin(), chunk.end());
    }
}

}}  // namespace sb::dvc
