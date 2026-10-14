// SignBridge — © 2026 Craftprojects Sp. z o.o. (craftprojects.pl)
// cp-signbridge — pomocnicze operacje na certyfikatach (implementacja)
#include "certs.h"
#include <stdio.h>
#include <wctype.h>

#pragma comment(lib, "crypt32.lib")
#pragma comment(lib, "bcrypt.lib")

namespace sb { namespace certs {

std::wstring Thumbprint(PCCERT_CONTEXT cert) {
    BYTE hash[20];
    DWORD cb = sizeof(hash);
    if (!CertGetCertificateContextProperty(cert, CERT_SHA1_HASH_PROP_ID, hash, &cb)) return L"";
    std::wstring s;
    wchar_t b[4];
    for (DWORD i = 0; i < cb; ++i) { swprintf_s(b, L"%02X", hash[i]); s += b; }
    return s;
}

bool IsThumbprint(const std::wstring& s) {
    if (s.size() != 40) return false;
    for (wchar_t c : s) if (!iswxdigit(c)) return false;
    return true;
}

std::wstring SubjectName(PCCERT_CONTEXT cert) {
    wchar_t buf[512] = {};
    CertGetNameStringW(cert, CERT_NAME_SIMPLE_DISPLAY_TYPE, 0, NULL, buf, 512);
    return buf;
}

HCERTSTORE OpenMyStore() {
    return CertOpenStore(CERT_STORE_PROV_SYSTEM_W, 0, 0, CERT_SYSTEM_STORE_CURRENT_USER, L"MY");
}

PCCERT_CONTEXT FindByThumbprint(HCERTSTORE store, const std::wstring& thumb) {
    if (!IsThumbprint(thumb)) return NULL;
    BYTE hash[20];
    for (int i = 0; i < 20; ++i) {
        unsigned v = 0;
        swscanf_s(thumb.substr(i * 2, 2).c_str(), L"%2x", &v);
        hash[i] = (BYTE)v;
    }
    CRYPT_HASH_BLOB blob = { 20, hash };
    return CertFindCertificateInStore(store, X509_ASN_ENCODING | PKCS_7_ASN_ENCODING, 0, CERT_FIND_SHA1_HASH, &blob, NULL);
}

bool ImportPublicKey(PCCERT_CONTEXT cert, BCRYPT_KEY_HANDLE* phKey, std::wstring& algName, DWORD& bits) {
    *phKey = NULL;
    if (!CryptImportPublicKeyInfoEx2(X509_ASN_ENCODING, &cert->pCertInfo->SubjectPublicKeyInfo, 0, NULL, phKey)) return false;
    wchar_t name[64] = {};
    DWORD cb = 0;
    if (BCryptGetProperty(*phKey, BCRYPT_ALGORITHM_NAME, (PUCHAR)name, sizeof(name), &cb, 0) == 0) algName = name;
    cb = 0;
    bits = 0;
    BCryptGetProperty(*phKey, BCRYPT_KEY_LENGTH, (PUCHAR)&bits, sizeof(bits), &cb, 0);
    return true;
}

bool HasPrivateKey(PCCERT_CONTEXT cert, std::wstring* providerName, DWORD* provType) {
    DWORD cb = 0;
    if (!CertGetCertificateContextProperty(cert, CERT_KEY_PROV_INFO_PROP_ID, NULL, &cb)) return false;
    std::vector<BYTE> buf(cb);
    if (!CertGetCertificateContextProperty(cert, CERT_KEY_PROV_INFO_PROP_ID, buf.data(), &cb)) return false;
    auto* kpi = (CRYPT_KEY_PROV_INFO*)buf.data();
    if (providerName) *providerName = kpi->pwszProvName ? kpi->pwszProvName : L"";
    if (provType) *provType = kpi->dwProvType;
    return true;
}

bool SetSyncMarker(PCCERT_CONTEXT cert, const std::wstring& agentId) {
    CRYPT_DATA_BLOB blob = { (DWORD)((agentId.size() + 1) * sizeof(wchar_t)), (BYTE*)agentId.c_str() };
    return CertSetCertificateContextProperty(cert, kSyncMarkerPropId, 0, &blob) == TRUE;
}

bool GetSyncMarker(PCCERT_CONTEXT cert, std::wstring* agentId) {
    DWORD cb = 0;
    if (!CertGetCertificateContextProperty(cert, kSyncMarkerPropId, NULL, &cb) || cb < sizeof(wchar_t)) return false;
    std::vector<BYTE> buf(cb);
    if (!CertGetCertificateContextProperty(cert, kSyncMarkerPropId, buf.data(), &cb)) return false;
    if (agentId) *agentId = (const wchar_t*)buf.data();
    return true;
}

}}  // namespace sb::certs
