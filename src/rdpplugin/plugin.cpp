// SignBridge — © 2026 Craftprojects Sp. z o.o. (craftprojects.pl)
// cp-signbridge — wtyczka klienta RDP (mstsc): cpsignbridge-rdp.dll
//
// Ładowana przez mstsc.exe na komputerze użytkownika (rejestracja w HKCU, bez uprawnień admina).
// Nasłuchuje dynamicznego kanału wirtualnego "CPSIGNBR". Gdy KSP na serwerze otworzy kanał,
// wtyczka odbiera żądania (lista certyfikatów, podpis skrótu) i obsługuje je lokalnie:
// certyfikaty z magazynu Osobiste, podpis przez Windows CNG → sterownik karty → PIN tutaj.
//
// Każda operacja KSP = osobna instancja kanału. Podpis wykonywany jest w osobnym wątku, żeby
// okno PIN nie blokowało wątku mstsc.
#include <windows.h>
#include <unknwn.h>
#include <cchannel.h>
#include <tsvirtualchannels.h>
#include <string>
#include <vector>
#include <new>
#include "../common/protocol.h"
#include "../common/signer.h"
#include "../common/slots.h"
#include "../ksp/log.h"

namespace {

const char* const kChannelName = "CPSIGNBR";
LONG g_objects = 0;
// Najwyżej tyle żądań naraz (każde we własnym wątku, podpis może czekać na PIN) — zalew ramkami
// z serwera nie utworzy setek wątków ani okien PIN
const LONG kMaxParallelRequests = 4;
sb::Slots g_requests(kMaxParallelRequests);

std::wstring Who() {
    wchar_t host[256] = {}; DWORD n = 256; GetComputerNameW(host, &n);
    return sb::UserName() + L"@" + host;
}

// ---------------------------------------------------------------------------
// Kanał: jedna instancja na jedno połączenie z serwera
// ---------------------------------------------------------------------------
class ChannelCallback : public IWTSVirtualChannelCallback {
public:
    explicit ChannelCallback(IWTSVirtualChannel* ch) : ref_(1), ch_(ch), authed_(false) {
        ch_->AddRef();
        InterlockedIncrement(&g_objects);
        SB_INFO(L"kanał: nowe połączenie z serwera");
    }
    ~ChannelCallback() { if (ch_) ch_->Release(); InterlockedDecrement(&g_objects); }

    // IUnknown
    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override {
        if (!ppv) return E_POINTER;
        if (riid == IID_IUnknown || riid == __uuidof(IWTSVirtualChannelCallback)) { *ppv = static_cast<IWTSVirtualChannelCallback*>(this); AddRef(); return S_OK; }
        *ppv = nullptr; return E_NOINTERFACE;
    }
    STDMETHODIMP_(ULONG) AddRef() override { return (ULONG)InterlockedIncrement(&ref_); }
    STDMETHODIMP_(ULONG) Release() override { LONG r = InterlockedDecrement(&ref_); if (r == 0) delete this; return (ULONG)r; }

    // IWTSVirtualChannelCallback
    STDMETHODIMP OnDataReceived(ULONG cbSize, BYTE* pBuffer) override {
        pending_.insert(pending_.end(), pBuffer, pBuffer + cbSize);
        for (;;) {
            if (pending_.size() < sizeof(sb::proto::FrameHdr)) return S_OK;
            sb::proto::FrameHdr h; memcpy(&h, pending_.data(), sizeof(h));
            if (h.magic != sb::proto::kMagic || h.cbPayload > sb::proto::kMaxPayload) { SB_ERROR(L"kanał: zła ramka"); pending_.clear(); return S_OK; }
            size_t need = sizeof(h) + h.cbPayload;
            if (pending_.size() < need) return S_OK;
            std::vector<uint8_t> payload(pending_.begin() + sizeof(h), pending_.begin() + need);
            pending_.erase(pending_.begin(), pending_.begin() + need);
            Dispatch(h.type, payload);
        }
    }
    STDMETHODIMP OnClose() override { SB_TRACE(L"kanał: zamknięty"); return S_OK; }

    void SendFrame(uint16_t type, const std::vector<uint8_t>& payload) {
        std::vector<uint8_t> frame(sizeof(sb::proto::FrameHdr) + payload.size());
        sb::proto::FrameHdr h = { sb::proto::kMagic, (uint32_t)payload.size(), type, 0 };
        memcpy(frame.data(), &h, sizeof(h));
        if (!payload.empty()) memcpy(frame.data() + sizeof(h), payload.data(), payload.size());
        HRESULT hr = ch_->Write((ULONG)frame.size(), frame.data(), NULL);
        if (FAILED(hr)) SB_ERROR(L"kanał: Write -> 0x%08X", hr);
    }

private:
    struct Job { ChannelCallback* self; uint16_t type; std::vector<uint8_t> payload; };

    static DWORD WINAPI Worker(LPVOID p) {
        Job* j = (Job*)p;
        uint16_t rtype = 0; std::vector<uint8_t> rpayload;
        sb::signer::HandleRequest(j->type, j->payload, NULL, L"rdp", rtype, rpayload);
        j->self->SendFrame(rtype, rpayload);
        g_requests.Release();
        j->self->Release();
        delete j;
        return 0;
    }

    void Dispatch(uint16_t type, const std::vector<uint8_t>& payload) {
        if (!authed_) {
            // pierwszy komunikat: AUTH (bez wyzwania — tożsamość daje sesja RDP)
            if (type != sb::proto::AUTH) { sb::proto::Error e = { (uint32_t)NTE_PERM, L"oczekiwano AUTH" }; SendFrame(sb::proto::ERR, sb::proto::EncodeError(e)); return; }
            sb::proto::Auth a;
            if (!sb::proto::DecodeAuth(payload, a) || a.clientVersion != sb::proto::PROTO_VERSION) {
                sb::proto::Error e = { (uint32_t)ERROR_REVISION_MISMATCH, L"niezgodna wersja protokołu — zaktualizuj wtyczkę lub KSP" };
                SendFrame(sb::proto::ERR, sb::proto::EncodeError(e)); return;
            }
            authed_ = true;
            SB_INFO(L"kanał: serwer '%s'", a.clientName.c_str());
            sb::proto::AuthOk ok;
            wchar_t host[256] = {}; DWORD n = 256; GetComputerNameW(host, &n);
            ok.agentUser = sb::UserName(); ok.agentHost = host;
            SendFrame(sb::proto::AUTH_OK, sb::proto::EncodeAuthOk(ok));
            return;
        }
        if (!g_requests.TryAcquire()) {
            SB_WARN(L"kanał: za dużo równoległych żądań — odrzucono");
            sb::proto::Error e = { (uint32_t)HRESULT_FROM_WIN32(ERROR_BUSY), L"komputer z kartą obsługuje już inne żądania — spróbuj ponownie" };
            SendFrame(sb::proto::ERR, sb::proto::EncodeError(e)); return;
        }
        // Podpis może czekać na PIN — poza wątkiem mstsc
        Job* j = new (std::nothrow) Job{ this, type, payload };
        if (!j) { g_requests.Release(); return; }
        AddRef();
        HANDLE t = CreateThread(NULL, 0, Worker, j, 0, NULL);
        if (!t) { Release(); delete j; g_requests.Release(); return; }
        CloseHandle(t);
    }

    LONG ref_;
    IWTSVirtualChannel* ch_;
    bool authed_;
    std::vector<uint8_t> pending_;
};

// ---------------------------------------------------------------------------
// Listener: przyjmuje każde połączenie na "CPSIGNBR"
// ---------------------------------------------------------------------------
class ListenerCallback : public IWTSListenerCallback {
public:
    ListenerCallback() : ref_(1) {}
    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override {
        if (!ppv) return E_POINTER;
        if (riid == IID_IUnknown || riid == __uuidof(IWTSListenerCallback)) { *ppv = static_cast<IWTSListenerCallback*>(this); AddRef(); return S_OK; }
        *ppv = nullptr; return E_NOINTERFACE;
    }
    STDMETHODIMP_(ULONG) AddRef() override { return (ULONG)InterlockedIncrement(&ref_); }
    STDMETHODIMP_(ULONG) Release() override { LONG r = InterlockedDecrement(&ref_); if (r == 0) delete this; return (ULONG)r; }

    STDMETHODIMP OnNewChannelConnection(IWTSVirtualChannel* pChannel, BSTR, BOOL* pbAccept, IWTSVirtualChannelCallback** ppCallback) override {
        ChannelCallback* cb = new (std::nothrow) ChannelCallback(pChannel);
        if (!cb) { *pbAccept = FALSE; return E_OUTOFMEMORY; }
        *ppCallback = cb;
        *pbAccept = TRUE;
        return S_OK;
    }
private:
    LONG ref_;
};

// ---------------------------------------------------------------------------
// Plugin
// ---------------------------------------------------------------------------
class Plugin : public IWTSPlugin {
public:
    Plugin() : ref_(1), listener_(nullptr) {}
    ~Plugin() { if (listener_) listener_->Release(); }

    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override {
        if (!ppv) return E_POINTER;
        if (riid == IID_IUnknown || riid == __uuidof(IWTSPlugin)) { *ppv = static_cast<IWTSPlugin*>(this); AddRef(); return S_OK; }
        *ppv = nullptr; return E_NOINTERFACE;
    }
    STDMETHODIMP_(ULONG) AddRef() override { return (ULONG)InterlockedIncrement(&ref_); }
    STDMETHODIMP_(ULONG) Release() override { LONG r = InterlockedDecrement(&ref_); if (r == 0) delete this; return (ULONG)r; }

    STDMETHODIMP Initialize(IWTSVirtualChannelManager* mgr) override {
        sb::LogInit(L"agent");
        SB_INFO(L"=== wtyczka RDP załadowana w '%s' (%s)", sb::ProcessName().c_str(), Who().c_str());
        ListenerCallback* cb = new (std::nothrow) ListenerCallback();
        if (!cb) return E_OUTOFMEMORY;
        HRESULT hr = mgr->CreateListener(kChannelName, 0, cb, &listener_);
        cb->Release();
        if (FAILED(hr)) SB_ERROR(L"CreateListener(%S) -> 0x%08X", kChannelName, hr);
        else SB_INFO(L"nasłuch kanału %S gotowy", kChannelName);
        return hr;
    }
    STDMETHODIMP Connected() override { SB_INFO(L"sesja RDP połączona"); return S_OK; }
    STDMETHODIMP Disconnected(DWORD code) override { SB_INFO(L"sesja RDP rozłączona (0x%08X)", code); return S_OK; }
    STDMETHODIMP Terminated() override { SB_INFO(L"wtyczka kończy pracę"); return S_OK; }

private:
    LONG ref_;
    IWTSListener* listener_;
};

}  // namespace

extern "C" HRESULT VCAPITYPE VirtualChannelGetInstance(REFIID refiid, ULONG* pNumObjs, VOID** ppObjArray) {
    if (refiid != __uuidof(IWTSPlugin)) return E_NOINTERFACE;
    if (!pNumObjs) return E_POINTER;
    if (!ppObjArray) { *pNumObjs = 1; return S_OK; }
    if (*pNumObjs < 1) return E_INVALIDARG;
    Plugin* p = new (std::nothrow) Plugin();
    if (!p) return E_OUTOFMEMORY;
    ppObjArray[0] = static_cast<IWTSPlugin*>(p);
    *pNumObjs = 1;
    return S_OK;
}

BOOL WINAPI DllMain(HINSTANCE hinst, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) DisableThreadLibraryCalls(hinst);
    return TRUE;
}
