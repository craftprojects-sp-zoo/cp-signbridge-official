// SignBridge — © 2026 Craftprojects Sp. z o.o. (craftprojects.pl)
// cp-signbridge — strona użytkownika: certyfikaty i podpis (implementacja)
#include <windows.h>
#include <wincrypt.h>
#include <ncrypt.h>
#include <stdio.h>
#include <algorithm>
#include "signer.h"
#include "certs.h"
#include "status.h"
#include "../ksp/log.h"

#pragma comment(lib, "ncrypt.lib")
#pragma comment(lib, "crypt32.lib")

namespace sb { namespace signer {
namespace {

const wchar_t* const kOurProvider = L"SignBridge Key Storage Provider";

bool Exposable(PCCERT_CONTEXT cert, std::wstring* why) {
    std::wstring prov;
    if (!certs::HasPrivateKey(cert, &prov, NULL)) { if (why) *why = L"brak klucza prywatnego"; return false; }
    if (_wcsicmp(prov.c_str(), kOurProvider) == 0) { if (why) *why = L"klucz u cp-signbridge (pominięty)"; return false; }
    FILETIME now; GetSystemTimeAsFileTime(&now);
    if (CompareFileTime(&cert->pCertInfo->NotAfter, &now) < 0) { if (why) *why = L"wygasł"; return false; }
    return true;
}

ALG_ID CalgFromName(const std::wstring& h) {
    if (_wcsicmp(h.c_str(), L"SHA256") == 0) return CALG_SHA_256;
    if (_wcsicmp(h.c_str(), L"SHA384") == 0) return CALG_SHA_384;
    if (_wcsicmp(h.c_str(), L"SHA512") == 0) return CALG_SHA_512;
    if (_wcsicmp(h.c_str(), L"SHA1") == 0) return CALG_SHA1;
    return 0;
}

SECURITY_STATUS SignWithCert(PCCERT_CONTEXT cert, const proto::SignReq& req, HWND hwnd, std::vector<uint8_t>& sig, std::wstring& err) {
    HCRYPTPROV_OR_NCRYPT_KEY_HANDLE hk = 0;
    DWORD spec = 0;
    BOOL freeIt = FALSE;
    if (!CryptAcquireCertificatePrivateKey(cert, CRYPT_ACQUIRE_ALLOW_NCRYPT_KEY_FLAG, NULL, &hk, &spec, &freeIt)) {
        DWORD e = GetLastError();
        err = L"CryptAcquireCertificatePrivateKey: " + ErrorText((LONG)e);
        return (SECURITY_STATUS)e;
    }
    SECURITY_STATUS s = ERROR_SUCCESS;
    if (spec == CERT_NCRYPT_KEY_SPEC) {
        NCRYPT_KEY_HANDLE hKey = (NCRYPT_KEY_HANDLE)hk;
        if (hwnd) NCryptSetProperty(hKey, NCRYPT_WINDOW_HANDLE_PROPERTY, (PBYTE)&hwnd, sizeof(hwnd), 0);
        BCRYPT_PKCS1_PADDING_INFO p1 = {};
        BCRYPT_PSS_PADDING_INFO pss = {};
        VOID* pad = NULL;
        DWORD flags = 0;
        std::wstring halg = req.hashAlg;
        if (req.padding == proto::PAD_PKCS1) { p1.pszAlgId = halg.empty() ? NULL : halg.c_str(); pad = &p1; flags = NCRYPT_PAD_PKCS1_FLAG; }
        else if (req.padding == proto::PAD_PSS) { pss.pszAlgId = halg.c_str(); pss.cbSalt = req.saltLen; pad = &pss; flags = NCRYPT_PAD_PSS_FLAG; }
        DWORD cb = 0;
        s = NCryptSignHash(hKey, pad, (PBYTE)req.hash.data(), (DWORD)req.hash.size(), NULL, 0, &cb, flags);
        if (s == ERROR_SUCCESS) {
            sig.resize(cb);
            s = NCryptSignHash(hKey, pad, (PBYTE)req.hash.data(), (DWORD)req.hash.size(), sig.data(), cb, &cb, flags);
            sig.resize(cb);
        }
        if (s != ERROR_SUCCESS) err = L"NCryptSignHash: " + ErrorText(s);
        if (freeIt) NCryptFreeObject(hKey);
    } else {
        HCRYPTPROV hProv = (HCRYPTPROV)hk;
        if (req.padding == proto::PAD_PSS) { s = NTE_NOT_SUPPORTED; err = L"PSS niewspierany przez dostawcę CAPI"; }
        else {
            ALG_ID calg = CalgFromName(req.hashAlg);
            if (!calg) { s = NTE_BAD_ALGID; err = L"nieznany skrót " + req.hashAlg; }
            else {
                HCRYPTHASH hHash = 0;
                if (!CryptCreateHash(hProv, calg, 0, 0, &hHash)) { s = (SECURITY_STATUS)GetLastError(); err = L"CryptCreateHash: " + ErrorText(s); }
                else {
                    if (!CryptSetHashParam(hHash, HP_HASHVAL, req.hash.data(), 0)) { s = (SECURITY_STATUS)GetLastError(); err = L"CryptSetHashParam: " + ErrorText(s); }
                    else {
                        DWORD cb = 0;
                        if (!CryptSignHashW(hHash, spec, NULL, 0, NULL, &cb)) { s = (SECURITY_STATUS)GetLastError(); err = L"CryptSignHash(size): " + ErrorText(s); }
                        else {
                            sig.resize(cb);
                            if (!CryptSignHashW(hHash, spec, NULL, 0, sig.data(), &cb)) { s = (SECURITY_STATUS)GetLastError(); err = L"CryptSignHash: " + ErrorText(s); }
                            else { sig.resize(cb); std::reverse(sig.begin(), sig.end()); }
                        }
                    }
                    CryptDestroyHash(hHash);
                }
            }
        }
        if (freeIt) CryptReleaseContext(hProv, 0);
    }
    return s;
}

}  // namespace

void CollectCerts(proto::Certs& out, bool print) {
    HCERTSTORE store = certs::OpenMyStore();
    if (!store) return;
    PCCERT_CONTEXT c = NULL;
    while ((c = CertEnumCertificatesInStore(store, c)) != NULL) {
        std::wstring why;
        bool ok = Exposable(c, &why);
        if (print) {
            std::wstring prov; certs::HasPrivateKey(c, &prov, NULL);
            wprintf(L"  %s  %-40s  %s\n", ok ? L"[+]" : L"[ ]", certs::SubjectName(c).c_str(), ok ? prov.c_str() : why.c_str());
            if (ok) wprintf(L"      odcisk %s\n", certs::Thumbprint(c).c_str());
        }
        if (ok) {
            proto::CertEntry e;
            e.der.assign(c->pbCertEncoded, c->pbCertEncoded + c->cbCertEncoded);
            out.items.push_back(e);
        }
    }
    CertCloseStore(store, 0);
}

SECURITY_STATUS Sign(const proto::SignReq& req, HWND hwnd, std::vector<uint8_t>& sig, std::wstring& subject, std::wstring& err) {
    HCERTSTORE store = certs::OpenMyStore();
    PCCERT_CONTEXT cert = store ? certs::FindByThumbprint(store, req.thumbprint) : NULL;
    if (!cert || !Exposable(cert, NULL)) {
        if (cert) CertFreeCertificateContext(cert);
        if (store) CertCloseStore(store, 0);
        err = L"certyfikat nie jest dostępny na komputerze użytkownika";
        return NTE_NOT_FOUND;
    }
    subject = certs::SubjectName(cert);
    SECURITY_STATUS s = SignWithCert(cert, req, hwnd, sig, err);
    CertFreeCertificateContext(cert);
    CertCloseStore(store, 0);
    return s;
}

void HandleRequest(uint16_t type, const std::vector<uint8_t>& payload, HWND hwnd, const std::wstring& who,
                   uint16_t& rtype, std::vector<uint8_t>& rpayload) {
    if (type == proto::PING) { rtype = proto::PONG; rpayload.clear(); return; }
    if (type == proto::LIST_CERTS) {
        proto::Certs list;
        CollectCerts(list, false);
        SB_INFO(L"[%s] LIST_CERTS -> %u certyfikatów", who.c_str(), (unsigned)list.items.size());
        rtype = proto::CERTS; rpayload = proto::EncodeCerts(list);
        return;
    }
    if (type == proto::SIGN) {
        proto::SignReq req;
        if (!proto::DecodeSign(payload, req)) { proto::Error e = { (uint32_t)NTE_INVALID_PARAMETER, L"zły SIGN" }; rtype = proto::ERR; rpayload = proto::EncodeError(e); return; }
        SB_INFO(L"[%s] SIGN: %s, %s/%s, skrót %u B — czekam na podpis (PIN?)", who.c_str(), req.thumbprint.c_str(),
                req.padding == proto::PAD_PKCS1 ? L"PKCS1" : req.padding == proto::PAD_PSS ? L"PSS" : L"raw",
                req.hashAlg.c_str(), (unsigned)req.hash.size());
        ULONGLONG t0 = GetTickCount64();
        std::vector<uint8_t> sig; std::wstring subject, err;
        SECURITY_STATUS s = Sign(req, hwnd, sig, subject, err);
        if (s == ERROR_SUCCESS) {
            SB_INFO(L"[%s] SIGN: OK '%s', %u B, %llu ms", who.c_str(), subject.c_str(), (unsigned)sig.size(), GetTickCount64() - t0);
            proto::SignOk r; r.signature = sig;
            rtype = proto::SIGN_OK; rpayload = proto::EncodeSignOk(r);
        } else {
            SB_ERROR(L"[%s] SIGN: BŁĄD %s", who.c_str(), err.c_str());
            proto::Error e = { (uint32_t)s, err };
            rtype = proto::ERR; rpayload = proto::EncodeError(e);
        }
        return;
    }
    proto::Error e = { (uint32_t)NTE_NOT_SUPPORTED, L"nieznany komunikat" };
    rtype = proto::ERR; rpayload = proto::EncodeError(e);
}

}}  // namespace sb::signer
