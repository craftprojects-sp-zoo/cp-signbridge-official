// SignBridge — © 2026 Craftprojects Sp. z o.o. (craftprojects.pl)
// cp-signbridge — backend złożony: kieruje nazwę klucza do właściwego backendu
//   odcisk SHA-1 (40 hex)  -> remote  (certyfikat z komputera użytkownika)
//   inna nazwa             -> softkey (klucz programowy sondy)
#include "backend.h"
#include "../common/certs.h"

namespace sb {
namespace {

class CompositeBackend : public IBackend {
public:
    CompositeBackend() : soft_(CreateSoftKeyBackend()), remote_(CreateRemoteBackend()) {}
    ~CompositeBackend() override { delete soft_; delete remote_; }
    const wchar_t* Name() const override { return L"composite(remote+softkey)"; }
    DWORD ImplType() const override { return remote_->ImplType(); }

    SECURITY_STATUS Open(const std::wstring& name, IKey** ppKey) override {
        return certs::IsThumbprint(name) ? remote_->Open(name, ppKey) : soft_->Open(name, ppKey);
    }
    SECURITY_STATUS Create(const std::wstring& name, LPCWSTR algId, IKey** ppKey) override {
        return certs::IsThumbprint(name) ? NTE_NOT_SUPPORTED : soft_->Create(name, algId, ppKey);
    }
    SECURITY_STATUS Delete(const std::wstring& name) override {
        return certs::IsThumbprint(name) ? remote_->Delete(name) : soft_->Delete(name);
    }
    SECURITY_STATUS Enumerate(std::vector<std::wstring>& names) override {
        remote_->Enumerate(names);
        soft_->Enumerate(names);
        return ERROR_SUCCESS;
    }
private:
    IBackend* soft_;
    IBackend* remote_;
};

}  // namespace

IBackend* CreateCompositeBackend() { return new CompositeBackend(); }

}  // namespace sb
