// SignBridge — © 2026 Craftprojects Sp. z o.o. (craftprojects.pl)
// cp-signbridge — operacje po stronie serwera (implementacja)
#include "status.h"
#include "syncops.h"
#include "agentclient.h"
#include "certs.h"
#include <ncrypt.h>
#include <stdio.h>

#pragma comment(lib, "ncrypt.lib")
#pragma comment(lib, "crypt32.lib")

namespace sb { namespace syncops {

std::wstring CertDate(const FILETIME& ft) {
    SYSTEMTIME st;
    if (!FileTimeToSystemTime(&ft, &st)) return L"";
    wchar_t b[32];
    swprintf_s(b, L"%04u-%02u-%02u", st.wYear, st.wMonth, st.wDay);
    return b;
}

bool IsKspInstalled() {
    NCRYPT_PROV_HANDLE h = 0;
    if (NCryptOpenStorageProvider(&h, kProviderName, 0) != ERROR_SUCCESS) return false;
    NCryptFreeObject(h);
    return true;
}

static CertRow RowFrom(PCCERT_CONTEXT c, const wchar_t* source) {
    CertRow r;
    r.thumbprint = certs::Thumbprint(c);
    r.subject = certs::SubjectName(c);
    wchar_t iss[256] = {};
    CertGetNameStringW(c, CERT_NAME_SIMPLE_DISPLAY_TYPE, CERT_NAME_ISSUER_FLAG, NULL, iss, 256);
    r.issuer = iss;
    r.notAfter = CertDate(c->pCertInfo->NotAfter);
    r.source = source;
    return r;
}

SECURITY_STATUS ListAgentCerts(std::vector<CertRow>& rows, std::wstring& info, std::wstring& err) {
    AgentClient agent;
    SECURITY_STATUS s = agent.Open(L"panel", err);
    if (s != ERROR_SUCCESS) return s;
    info = agent.AgentInfo();
    proto::Certs list;
    s = agent.ListCerts(list, err);
    if (s != ERROR_SUCCESS) return s;
    for (auto& e : list.items) {
        PCCERT_CONTEXT c = CertCreateCertificateContext(X509_ASN_ENCODING, e.der.data(), (DWORD)e.der.size());
        if (!c) continue;
        rows.push_back(RowFrom(c, L"komputer użytkownika"));
        CertFreeCertificateContext(c);
    }
    return ERROR_SUCCESS;
}

void ListSyncedCerts(std::vector<CertRow>& rows) {
    HCERTSTORE store = certs::OpenMyStore();
    if (!store) return;
    PCCERT_CONTEXT c = NULL;
    while ((c = CertEnumCertificatesInStore(store, c)) != NULL) {
        std::wstring prov;
        if (!certs::HasPrivateKey(c, &prov, NULL) || _wcsicmp(prov.c_str(), kProviderName) != 0) continue;
        rows.push_back(RowFrom(c, certs::GetSyncMarker(c, NULL) ? L"serwer (sync)" : L"serwer (sonda)"));
    }
    CertCloseStore(store, 0);
}

SECURITY_STATUS Sync(std::vector<std::wstring>& log, int& added, int& removed) {
    added = removed = 0;
    std::wstring err;
    AgentClient agent;
    SECURITY_STATUS s = agent.Open(L"sync", err);
    if (s != ERROR_SUCCESS) { log.push_back(L"Połączenie z komputerem użytkownika: " + err); return s; }
    std::wstring info = agent.AgentInfo();
    proto::Certs list;
    s = agent.ListCerts(list, err);
    if (s != ERROR_SUCCESS) { log.push_back(L"Lista certyfikatów: " + err); return s; }
    log.push_back(L"Połączono: " + info);

    HCERTSTORE store = certs::OpenMyStore();
    if (!store) { log.push_back(L"Nie można otworzyć magazynu Osobiste"); return NTE_FAIL; }
    std::vector<std::wstring> wanted;
    for (auto& e : list.items) {
        PCCERT_CONTEXT c = CertCreateCertificateContext(X509_ASN_ENCODING, e.der.data(), (DWORD)e.der.size());
        if (!c) continue;
        std::wstring thumb = certs::Thumbprint(c);
        wanted.push_back(thumb);
        CRYPT_KEY_PROV_INFO kpi = {};
        kpi.pwszContainerName = (LPWSTR)thumb.c_str();
        kpi.pwszProvName = (LPWSTR)kProviderName;
        CertSetCertificateContextProperty(c, CERT_KEY_PROV_INFO_PROP_ID, 0, &kpi);
        certs::SetSyncMarker(c, info);
        PCCERT_CONTEXT stored = NULL;
        if (CertAddCertificateContextToStore(store, c, CERT_STORE_ADD_REPLACE_EXISTING_INHERIT_PROPERTIES, &stored)) {
            CertSetCertificateContextProperty(stored, CERT_KEY_PROV_INFO_PROP_ID, 0, &kpi);
            certs::SetSyncMarker(stored, info);
            CertFreeCertificateContext(stored);
            log.push_back(L"+ " + certs::SubjectName(c) + L"  (" + thumb.substr(0, 8) + L"…)");
            ++added;
        } else {
            log.push_back(L"! " + certs::SubjectName(c) + L": błąd " + std::to_wstring(GetLastError()));
        }
        CertFreeCertificateContext(c);
    }
    std::vector<PCCERT_CONTEXT> toDelete;
    PCCERT_CONTEXT c = NULL;
    while ((c = CertEnumCertificatesInStore(store, c)) != NULL) {
        if (!certs::GetSyncMarker(c, NULL)) continue;
        std::wstring t = certs::Thumbprint(c);
        bool keep = false;
        for (auto& w : wanted) if (w == t) { keep = true; break; }
        if (!keep) toDelete.push_back(CertDuplicateCertificateContext(c));
    }
    for (auto d : toDelete) { log.push_back(L"- " + certs::SubjectName(d) + L" (nieaktualny)"); CertDeleteCertificateFromStore(d); ++removed; }
    CertCloseStore(store, 0);
    return ERROR_SUCCESS;
}

int Unsync() {
    HCERTSTORE store = certs::OpenMyStore();
    if (!store) return 0;
    std::vector<PCCERT_CONTEXT> toDelete;
    PCCERT_CONTEXT c = NULL;
    while ((c = CertEnumCertificatesInStore(store, c)) != NULL) if (certs::GetSyncMarker(c, NULL)) toDelete.push_back(CertDuplicateCertificateContext(c));
    for (auto d : toDelete) CertDeleteCertificateFromStore(d);
    CertCloseStore(store, 0);
    return (int)toDelete.size();
}

SECURITY_STATUS TestSign(const std::wstring& thumbprint, std::vector<std::wstring>& log) {
    HCERTSTORE store = certs::OpenMyStore();
    if (!store) return NTE_FAIL;
    PCCERT_CONTEXT cert = NULL;
    if (!thumbprint.empty()) cert = certs::FindByThumbprint(store, thumbprint);
    else {
        PCCERT_CONTEXT c = NULL;
        while ((c = CertEnumCertificatesInStore(store, c)) != NULL) if (certs::GetSyncMarker(c, NULL)) { cert = CertDuplicateCertificateContext(c); CertFreeCertificateContext(c); break; }
    }
    if (!cert) { CertCloseStore(store, 0); log.push_back(L"Brak zsynchronizowanego certyfikatu — najpierw pobierz certyfikaty."); return NTE_NOT_FOUND; }
    log.push_back(L"Certyfikat: " + certs::SubjectName(cert));

    HCRYPTPROV_OR_NCRYPT_KEY_HANDLE hk = 0; DWORD spec = 0; BOOL freeIt = FALSE;
    if (!CryptAcquireCertificatePrivateKey(cert, CRYPT_ACQUIRE_ONLY_NCRYPT_KEY_FLAG, NULL, &hk, &spec, &freeIt)) {
        DWORD e = GetLastError();
        log.push_back(L"Otwarcie klucza: " + ErrorText((LONG)e));
        CertFreeCertificateContext(cert); CertCloseStore(store, 0);
        return (SECURITY_STATUS)e;
    }
    BYTE hash[32]; for (int i = 0; i < 32; ++i) hash[i] = (BYTE)(i * 13 + 7);
    BCRYPT_PKCS1_PADDING_INFO pad = { BCRYPT_SHA256_ALGORITHM };
    BYTE sig[1024]; DWORD cbSig = 0;
    log.push_back(L"Podpisuję przez kanał… (PIN na komputerze użytkownika)");
    ULONGLONG t0 = GetTickCount64();
    SECURITY_STATUS s = NCryptSignHash((NCRYPT_KEY_HANDLE)hk, &pad, hash, 32, sig, sizeof(sig), &cbSig, NCRYPT_PAD_PKCS1_FLAG);
    wchar_t b[64]; swprintf_s(b, L" (%lu B, %llu ms)", cbSig, GetTickCount64() - t0);
    log.push_back(L"Podpis: " + ErrorText(s) + (s == ERROR_SUCCESS ? b : L""));
    if (s == ERROR_SUCCESS) {
        SECURITY_STATUS v = NCryptVerifySignature((NCRYPT_KEY_HANDLE)hk, &pad, hash, 32, sig, cbSig, NCRYPT_PAD_PKCS1_FLAG);
        log.push_back(L"Weryfikacja: " + ErrorText(v));
        if (v != ERROR_SUCCESS) s = v;
    }
    if (freeIt) NCryptFreeObject(hk);
    CertFreeCertificateContext(cert); CertCloseStore(store, 0);
    return s;
}

}}  // namespace sb::syncops
