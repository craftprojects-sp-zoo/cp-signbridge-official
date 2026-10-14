// SignBridge — © 2026 Craftprojects Sp. z o.o. (craftprojects.pl)
// cp-signbridge — backend "remote": podpis na komputerze użytkownika przez wtyczkę mstsc
//
// Nazwa klucza = odcisk SHA-1 certyfikatu (40 hex). Certyfikat musi być w magazynie Osobiste
// użytkownika NA SERWERZE (wgrywa go panel albo `sbctl sync`) — stąd bierzemy klucz publiczny lokalnie.
// Kanał wirtualny bieżącej sesji RDP jest używany wyłącznie przy SignHash: skrót leci do wtyczki, wraca podpis.
#include "backend.h"
#include "log.h"
#include "../common/certs.h"
#include "../common/agentclient.h"
#include <algorithm>
#include <memory>

#pragma comment(lib, "crypt32.lib")

namespace sb {

namespace {

const DWORD kSignTimeoutMs = 180000;  // użytkownik może wpisywać PIN

class RemoteKey : public IKey {
public:
    RemoteKey(const std::wstring& thumb) : thumb_(thumb) {}
    ~RemoteKey() override { if (hPub_) BCryptDestroyKey(hPub_); }

    SECURITY_STATUS Load() {
        HCERTSTORE store = certs::OpenMyStore();
        if (!store) return NTE_BAD_KEYSET;
        PCCERT_CONTEXT cert = certs::FindByThumbprint(store, thumb_);
        if (!cert) { CertCloseStore(store, 0); SB_WARN(L"remote: certyfikatu %s nie ma w magazynie Osobiste (sbctl sync?)", thumb_.c_str()); return NTE_BAD_KEYSET; }
        subject_ = certs::SubjectName(cert);
        std::wstring alg;
        if (!certs::ImportPublicKey(cert, &hPub_, alg, bits_)) { CertFreeCertificateContext(cert); CertCloseStore(store, 0); return NTE_BAD_KEYSET; }
        CertFreeCertificateContext(cert);
        CertCloseStore(store, 0);
        if (_wcsicmp(alg.c_str(), BCRYPT_RSA_ALGORITHM) == 0) { group_ = NCRYPT_RSA_ALGORITHM_GROUP; algId_ = BCRYPT_RSA_ALGORITHM; }
        else if (wcsncmp(alg.c_str(), L"ECDSA", 5) == 0 || wcsncmp(alg.c_str(), L"ECDH", 4) == 0) { group_ = NCRYPT_ECDSA_ALGORITHM_GROUP; algId_ = alg; if (algId_.rfind(L"ECDH", 0) == 0) algId_.replace(0, 4, L"ECDSA"); }
        else { SB_WARN(L"remote: nieobsługiwany algorytm klucza '%s'", alg.c_str()); return NTE_NOT_SUPPORTED; }
        SB_INFO(L"remote: klucz %s = '%s', %s %lu bit", thumb_.c_str(), subject_.c_str(), algId_.c_str(), bits_);
        return ERROR_SUCCESS;
    }

    const std::wstring& Name() const override { return thumb_; }
    const wchar_t* AlgGroup() const override { return group_.c_str(); }
    const wchar_t* AlgId() const override { return algId_.c_str(); }
    DWORD Bits() const override { return bits_; }
    bool IsFinalized() const override { return true; }

    SECURITY_STATUS ExportPublic(LPCWSTR blobType, PBYTE pbOut, DWORD cbOut, DWORD* pcb) override {
        LPCWSTR t = blobType;
        if (_wcsicmp(t, BCRYPT_PUBLIC_KEY_BLOB) == 0) t = (group_ == NCRYPT_RSA_ALGORITHM_GROUP) ? BCRYPT_RSAPUBLIC_BLOB : BCRYPT_ECCPUBLIC_BLOB;
        NTSTATUS st = BCryptExportKey(hPub_, NULL, t, pbOut, cbOut, pcb, 0);
        if (st == STATUS_BUFFER_TOO_SMALL) return NTE_BUFFER_TOO_SMALL;
        return BCRYPT_SUCCESS(st) ? ERROR_SUCCESS : NTE_NOT_SUPPORTED;
    }

    SECURITY_STATUS SignHash(VOID* pPad, PBYTE pbHash, DWORD cbHash, PBYTE pbSig, DWORD cbSig, DWORD* pcb, DWORD dwFlags) override {
        proto::SignReq req;
        req.thumbprint = thumb_;
        req.saltLen = 0;
        if (dwFlags & NCRYPT_PAD_PKCS1_FLAG) { auto* p = (BCRYPT_PKCS1_PADDING_INFO*)pPad; req.padding = proto::PAD_PKCS1; if (p && p->pszAlgId) req.hashAlg = p->pszAlgId; }
        else if (dwFlags & NCRYPT_PAD_PSS_FLAG) { auto* p = (BCRYPT_PSS_PADDING_INFO*)pPad; req.padding = proto::PAD_PSS; if (p) { if (p->pszAlgId) req.hashAlg = p->pszAlgId; req.saltLen = p->cbSalt; } }
        else req.padding = proto::PAD_NONE;
        req.hash.assign(pbHash, pbHash + cbHash);

        AgentClient agent;
        std::wstring err;
        SECURITY_STATUS s = agent.Open(sb::ProcessName() + L"@" + sb::UserName(), err);
        if (s != ERROR_SUCCESS) { SB_ERROR(L"remote: połączenie z agentem: %s", err.c_str()); return s; }
        SB_INFO(L"remote: agent %s, wysyłam SIGN (%s)", agent.AgentInfo().c_str(), subject_.c_str());

        std::vector<uint8_t> sig;
        s = agent.Sign(req, sig, err, kSignTimeoutMs);
        if (s != ERROR_SUCCESS) { SB_ERROR(L"remote: podpis nieudany: %s (0x%08X)", err.c_str(), (unsigned)s); return s; }

        *pcb = (DWORD)sig.size();
        if (cbSig < sig.size()) return NTE_BUFFER_TOO_SMALL;
        memcpy(pbSig, sig.data(), sig.size());
        return ERROR_SUCCESS;
    }

    SECURITY_STATUS VerifySignature(VOID* pPad, PBYTE pbHash, DWORD cbHash, PBYTE pbSig, DWORD cbSig, DWORD dwFlags) override {
        DWORD f = 0;
        if (dwFlags & NCRYPT_PAD_PKCS1_FLAG) f |= BCRYPT_PAD_PKCS1;
        if (dwFlags & NCRYPT_PAD_PSS_FLAG) f |= BCRYPT_PAD_PSS;
        NTSTATUS st = BCryptVerifySignature(hPub_, pPad, pbHash, cbHash, pbSig, cbSig, f);
        if (st == STATUS_INVALID_SIGNATURE) return NTE_BAD_SIGNATURE;
        return BCRYPT_SUCCESS(st) ? ERROR_SUCCESS : NTE_INTERNAL_ERROR;
    }

    SECURITY_STATUS Decrypt(VOID*, PBYTE, DWORD, PBYTE, DWORD, DWORD*, DWORD) override { return NTE_NOT_SUPPORTED; }
    SECURITY_STATUS SetProperty(LPCWSTR, PBYTE, DWORD, DWORD) override { return ERROR_SUCCESS; }
    SECURITY_STATUS Finalize(DWORD) override { return NTE_NOT_SUPPORTED; }

private:
    std::wstring thumb_, subject_, group_, algId_;
    BCRYPT_KEY_HANDLE hPub_ = NULL;
    DWORD bits_ = 0;
};

class RemoteBackend : public IBackend {
public:
    const wchar_t* Name() const override { return L"remote"; }
    DWORD ImplType() const override { return NCRYPT_IMPL_HARDWARE_FLAG | NCRYPT_IMPL_REMOVABLE_FLAG; }

    SECURITY_STATUS Open(const std::wstring& name, IKey** ppKey) override {
        if (!certs::IsThumbprint(name)) return NTE_BAD_KEYSET;
        RemoteKey* k = new RemoteKey(name);
        SECURITY_STATUS s = k->Load();
        if (s != ERROR_SUCCESS) { delete k; return s; }
        *ppKey = k;
        return ERROR_SUCCESS;
    }
    SECURITY_STATUS Create(const std::wstring&, LPCWSTR, IKey**) override { return NTE_NOT_SUPPORTED; }
    SECURITY_STATUS Delete(const std::wstring&) override { return NTE_NOT_SUPPORTED; }

    // Klucze zdalne = certyfikaty w magazynie Osobiste wskazujące na nasz dostawca z odciskiem jako kontenerem
    SECURITY_STATUS Enumerate(std::vector<std::wstring>& names) override {
        HCERTSTORE store = certs::OpenMyStore();
        if (!store) return ERROR_SUCCESS;
        PCCERT_CONTEXT c = NULL;
        while ((c = CertEnumCertificatesInStore(store, c)) != NULL) {
            if (certs::GetSyncMarker(c, NULL)) names.push_back(certs::Thumbprint(c));
        }
        CertCloseStore(store, 0);
        return ERROR_SUCCESS;
    }
};

}  // namespace

IBackend* CreateRemoteBackend() { return new RemoteBackend(); }

}  // namespace sb
