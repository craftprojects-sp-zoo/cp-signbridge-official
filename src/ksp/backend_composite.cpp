// SignBridge — © 2026 Craftprojects Sp. z o.o. (craftprojects.pl)
// cp-signbridge — backend złożony: kieruje nazwę klucza do właściwego backendu
//   odcisk SHA-1 (40 hex)  -> remote  (certyfikat z komputera użytkownika)
//   inna nazwa             -> softkey (klucz programowy sondy) — tylko z SB_ENABLE_SOFTKEY;
//                             w wydaniu: brak klucza, tworzenie kluczy niedostępne
#include "backend.h"
#include "../common/certs.h"

namespace sb {
namespace {

class CompositeBackend : public IBackend {
public:
#ifdef SB_ENABLE_SOFTKEY
    CompositeBackend() : soft_(CreateSoftKeyBackend()), remote_(CreateRemoteBackend()) {}
    const wchar_t* Name() const override { return L"composite(remote+softkey)"; }
#else
    CompositeBackend() : soft_(nullptr), remote_(CreateRemoteBackend()) {}
    const wchar_t* Name() const override { return L"composite(remote)"; }
#endif
    ~CompositeBackend() override { delete soft_; delete remote_; }
    DWORD ImplType() const override { return remote_->ImplType(); }
    // klucz programowy nie może udawać karty (HARDWARE | REMOVABLE tylko dla kluczy zdalnych)
    DWORD KeyImplType(const std::wstring& name) const override {
        return certs::IsThumbprint(name) ? remote_->ImplType() : (DWORD)NCRYPT_IMPL_SOFTWARE_FLAG;
    }

    SECURITY_STATUS Open(const std::wstring& name, IKey** ppKey) override {
        if (certs::IsThumbprint(name)) return remote_->Open(name, ppKey);
        return soft_ ? soft_->Open(name, ppKey) : NTE_BAD_KEYSET;
    }
    SECURITY_STATUS Create(const std::wstring& name, LPCWSTR algId, IKey** ppKey) override {
        if (certs::IsThumbprint(name) || !soft_) return NTE_NOT_SUPPORTED;
        return soft_->Create(name, algId, ppKey);
    }
    SECURITY_STATUS Delete(const std::wstring& name) override {
        if (certs::IsThumbprint(name)) return remote_->Delete(name);
        return soft_ ? soft_->Delete(name) : NTE_BAD_KEYSET;
    }
    SECURITY_STATUS Enumerate(std::vector<std::wstring>& names) override {
        remote_->Enumerate(names);
        if (soft_) soft_->Enumerate(names);
        return ERROR_SUCCESS;
    }
private:
    IBackend* soft_;
    IBackend* remote_;
};

}  // namespace

IBackend* CreateCompositeBackend() { return new CompositeBackend(); }

}  // namespace sb
