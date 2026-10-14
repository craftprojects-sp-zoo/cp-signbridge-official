// SignBridge — © 2026 Craftprojects Sp. z o.o. (craftprojects.pl)
// cp-signbridge — CNG Key Storage Provider (KSP)
//
// To jest "wtyczka" do Windows CNG. Program księgowy (Optima, Symfonia, Płatnik...) prosi
// Windows o podpis certyfikatem, a Windows — widząc w certyfikacie nazwę naszego dostawcy —
// ładuje tę bibliotekę i woła poniższe funkcje. My logujemy każde wywołanie (Etap 0: sonda)
// i przekazujemy podpis do backendu (Etap 0: klucz programowy, Etap 1+: agent na komputerze
// użytkownika z prawdziwą kartą).
//
// Nazwa dostawcy: "SignBridge Key Storage Provider"
#include <windows.h>
#include <bcrypt.h>
#include <ncrypt.h>
#include "ncrypt_provider_compat.h"
#include <string>
#include <vector>
#include <new>
#include "backend.h"
#include "log.h"

#pragma comment(lib, "ncrypt.lib")
#pragma comment(lib, "bcrypt.lib")

namespace {

const wchar_t* const kProviderName = L"SignBridge Key Storage Provider";
const DWORD kProvMagic = 0x53425052;  // 'SBPR'
const DWORD kKeyMagic  = 0x53424B59;  // 'SBKY'
const DWORD kMaxNameLen = 128;

struct ProvObj {
    DWORD magic;
    sb::IBackend* backend;
};

struct KeyObj {
    DWORD magic;
    ProvObj* prov;
    sb::IKey* key;
    std::wstring name;
    HWND hwnd;
    DWORD legacySpec;
};

// Stan enumeracji kluczy. Zwalniany przez NCryptFreeBuffer -> KspFreeBuffer (HeapFree),
// więc musi być jednym blokiem HeapAlloc bez destruktorów: nagłówek + tablica wskaźników
// + wszystkie nazwy w tym samym bloku.
struct EnumState {
    DWORD magic;
    DWORD index;
    DWORD count;
    const wchar_t* names[1];  // count elementów, wskazują w głąb tego samego bloku
};
const DWORD kEnumMagic = 0x53424553;  // 'SBES'

EnumState* MakeEnumState(const std::vector<std::wstring>& names) {
    size_t cbStrings = 0;
    for (const auto& n : names) cbStrings += (n.size() + 1) * sizeof(wchar_t);
    size_t cbHeader = sizeof(EnumState) + (names.empty() ? 0 : (names.size() - 1)) * sizeof(const wchar_t*);
    BYTE* mem = (BYTE*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, cbHeader + cbStrings);
    if (!mem) return nullptr;
    EnumState* st = (EnumState*)mem;
    st->magic = kEnumMagic;
    st->index = 0;
    st->count = (DWORD)names.size();
    wchar_t* cursor = (wchar_t*)(mem + cbHeader);
    for (size_t i = 0; i < names.size(); ++i) {
        size_t cb = (names[i].size() + 1) * sizeof(wchar_t);
        memcpy(cursor, names[i].c_str(), cb);
        st->names[i] = cursor;
        cursor += names[i].size() + 1;
    }
    return st;
}

sb::IBackend* g_backend = nullptr;
CRITICAL_SECTION g_init;
bool g_initDone = false;

void EnsureInit() {
    EnterCriticalSection(&g_init);
    if (!g_initDone) {
        sb::LogInit(L"ksp");
#ifdef _WIN64
        const wchar_t* arch = L"x64";
#else
        const wchar_t* arch = L"x86";
#endif
        SB_INFO(L"=== KSP załadowany: proces '%s', użytkownik '%s', sesja %lu, %s",
                sb::ProcessName().c_str(), sb::UserName().c_str(), sb::SessionId(), arch);
        SB_INFO(L"linia poleceń: %s", GetCommandLineW());
        g_backend = sb::CreateCompositeBackend();
        SB_INFO(L"backend: %s", g_backend->Name());
        g_initDone = true;
    }
    LeaveCriticalSection(&g_init);
}

ProvObj* AsProv(NCRYPT_PROV_HANDLE h) {
    ProvObj* p = (ProvObj*)h;
    return (p && p->magic == kProvMagic) ? p : nullptr;
}
KeyObj* AsKey(NCRYPT_KEY_HANDLE h) {
    KeyObj* k = (KeyObj*)h;
    return (k && k->magic == kKeyMagic) ? k : nullptr;
}

// Zapis wyniku właściwości do bufora wywołującego z obsługą "podaj rozmiar"
SECURITY_STATUS OutBytes(PBYTE pbOut, DWORD cbOut, DWORD* pcb, const void* data, DWORD size) {
    if (!pcb) return NTE_INVALID_PARAMETER;
    *pcb = size;
    if (!pbOut) return ERROR_SUCCESS;
    if (cbOut < size) return NTE_BUFFER_TOO_SMALL;
    memcpy(pbOut, data, size);
    return ERROR_SUCCESS;
}
SECURITY_STATUS OutDword(PBYTE pbOut, DWORD cbOut, DWORD* pcb, DWORD v) { return OutBytes(pbOut, cbOut, pcb, &v, sizeof(v)); }
SECURITY_STATUS OutString(PBYTE pbOut, DWORD cbOut, DWORD* pcb, const wchar_t* s) {
    return OutBytes(pbOut, cbOut, pcb, s, (DWORD)((wcslen(s) + 1) * sizeof(wchar_t)));
}

std::wstring Hex(const BYTE* p, DWORD n, DWORD maxBytes = 16) {
    std::wstring s;
    wchar_t b[4];
    for (DWORD i = 0; i < n && i < maxBytes; ++i) { swprintf_s(b, L"%02X", p[i]); s += b; }
    if (n > maxBytes) s += L"…";
    return s;
}

std::wstring DescribePadding(VOID* pPad, DWORD dwFlags) {
    wchar_t buf[256];
    if (dwFlags & NCRYPT_PAD_PKCS1_FLAG) {
        auto* pi = (BCRYPT_PKCS1_PADDING_INFO*)pPad;
        swprintf_s(buf, L"PKCS1(hash=%s)", (pi && pi->pszAlgId) ? pi->pszAlgId : L"raw");
    } else if (dwFlags & NCRYPT_PAD_PSS_FLAG) {
        auto* pi = (BCRYPT_PSS_PADDING_INFO*)pPad;
        swprintf_s(buf, L"PSS(hash=%s, salt=%lu)", (pi && pi->pszAlgId) ? pi->pszAlgId : L"?", pi ? pi->cbSalt : 0);
    } else if (dwFlags & NCRYPT_PAD_OAEP_FLAG) {
        auto* pi = (BCRYPT_OAEP_PADDING_INFO*)pPad;
        swprintf_s(buf, L"OAEP(hash=%s)", (pi && pi->pszAlgId) ? pi->pszAlgId : L"?");
    } else {
        swprintf_s(buf, L"brak-paddingu(flags=0x%X)", dwFlags);
    }
    std::wstring s = buf;
    if (dwFlags & NCRYPT_SILENT_FLAG) s += L" +SILENT";
    return s;
}

// ---------------------------------------------------------------------------
// Implementacja funkcji KSP
// ---------------------------------------------------------------------------

SECURITY_STATUS WINAPI KspOpenProvider(NCRYPT_PROV_HANDLE* phProvider, LPCWSTR pszProviderName, DWORD dwFlags) {
    EnsureInit();
    SB_INFO(L"OpenProvider(name='%s', flags=0x%X)", pszProviderName ? pszProviderName : L"(null)", dwFlags);
    if (!phProvider) return NTE_INVALID_PARAMETER;
    ProvObj* p = new (std::nothrow) ProvObj{ kProvMagic, g_backend };
    if (!p) return NTE_NO_MEMORY;
    *phProvider = (NCRYPT_PROV_HANDLE)p;
    return ERROR_SUCCESS;
}

SECURITY_STATUS WINAPI KspFreeProvider(NCRYPT_PROV_HANDLE hProvider) {
    ProvObj* p = AsProv(hProvider);
    SB_TRACE(L"FreeProvider()");
    if (!p) return NTE_INVALID_HANDLE;
    p->magic = 0;
    delete p;
    return ERROR_SUCCESS;
}

SECURITY_STATUS WINAPI KspOpenKey(NCRYPT_PROV_HANDLE hProvider, NCRYPT_KEY_HANDLE* phKey, LPCWSTR pszKeyName,
                                  DWORD dwLegacyKeySpec, DWORD dwFlags) {
    ProvObj* p = AsProv(hProvider);
    SB_INFO(L"OpenKey(name='%s', legacySpec=%lu, flags=0x%X)", pszKeyName ? pszKeyName : L"(null)", dwLegacyKeySpec, dwFlags);
    if (!p) return NTE_INVALID_HANDLE;
    if (!phKey || !pszKeyName) return NTE_INVALID_PARAMETER;

    sb::IKey* key = nullptr;
    SECURITY_STATUS s = p->backend->Open(pszKeyName, &key);
    if (s != ERROR_SUCCESS) {
        SB_WARN(L"OpenKey: backend -> %s", sb::StatusText(s).c_str());
        return s;
    }
    KeyObj* k = new (std::nothrow) KeyObj{ kKeyMagic, p, key, pszKeyName, NULL, dwLegacyKeySpec };
    if (!k) { delete key; return NTE_NO_MEMORY; }
    *phKey = (NCRYPT_KEY_HANDLE)k;
    SB_INFO(L"OpenKey: OK, %s %lu bit", key->AlgGroup(), key->Bits());
    return ERROR_SUCCESS;
}

SECURITY_STATUS WINAPI KspCreatePersistedKey(NCRYPT_PROV_HANDLE hProvider, NCRYPT_KEY_HANDLE* phKey, LPCWSTR pszAlgId,
                                             LPCWSTR pszKeyName, DWORD dwLegacyKeySpec, DWORD dwFlags) {
    ProvObj* p = AsProv(hProvider);
    SB_INFO(L"CreatePersistedKey(alg='%s', name='%s', legacySpec=%lu, flags=0x%X)",
            pszAlgId ? pszAlgId : L"(null)", pszKeyName ? pszKeyName : L"(null)", dwLegacyKeySpec, dwFlags);
    if (!p) return NTE_INVALID_HANDLE;
    if (!phKey || !pszAlgId) return NTE_INVALID_PARAMETER;
    std::wstring name = pszKeyName ? pszKeyName : L"";
    if (name.empty()) {
        // klucz efemeryczny — nadaj nazwę tymczasową
        wchar_t tmp[64];
        swprintf_s(tmp, L"ephemeral-%lu-%lu", GetCurrentProcessId(), GetTickCount());
        name = tmp;
    }
    sb::IKey* key = nullptr;
    SECURITY_STATUS s = p->backend->Create(name, pszAlgId, &key);
    if (s != ERROR_SUCCESS) return s;
    KeyObj* k = new (std::nothrow) KeyObj{ kKeyMagic, p, key, name, NULL, dwLegacyKeySpec };
    if (!k) { delete key; return NTE_NO_MEMORY; }
    *phKey = (NCRYPT_KEY_HANDLE)k;
    return ERROR_SUCCESS;
}

SECURITY_STATUS WINAPI KspFinalizeKey(NCRYPT_PROV_HANDLE hProvider, NCRYPT_KEY_HANDLE hKey, DWORD dwFlags) {
    KeyObj* k = AsKey(hKey);
    SB_INFO(L"FinalizeKey(flags=0x%X)", dwFlags);
    if (!AsProv(hProvider) || !k) return NTE_INVALID_HANDLE;
    SECURITY_STATUS s = k->key->Finalize(dwFlags);
    if (s != ERROR_SUCCESS) SB_ERROR(L"FinalizeKey -> %s", sb::StatusText(s).c_str());
    return s;
}

SECURITY_STATUS WINAPI KspDeleteKey(NCRYPT_PROV_HANDLE hProvider, NCRYPT_KEY_HANDLE hKey, DWORD dwFlags) {
    ProvObj* p = AsProv(hProvider);
    KeyObj* k = AsKey(hKey);
    SB_INFO(L"DeleteKey(flags=0x%X)", dwFlags);
    if (!p || !k) return NTE_INVALID_HANDLE;
    SECURITY_STATUS s = p->backend->Delete(k->name);
    // Po DeleteKey uchwyt jest zwalniany przez wywołującego (NCryptDeleteKey zwalnia go sam)
    k->magic = 0;
    delete k->key;
    delete k;
    return s;
}

SECURITY_STATUS WINAPI KspFreeKey(NCRYPT_PROV_HANDLE hProvider, NCRYPT_KEY_HANDLE hKey) {
    KeyObj* k = AsKey(hKey);
    SB_TRACE(L"FreeKey()");
    if (!AsProv(hProvider) || !k) return NTE_INVALID_HANDLE;
    k->magic = 0;
    delete k->key;
    delete k;
    return ERROR_SUCCESS;
}

SECURITY_STATUS WINAPI KspFreeBuffer(PVOID pvInput) {
    if (pvInput) HeapFree(GetProcessHeap(), 0, pvInput);
    return ERROR_SUCCESS;
}

SECURITY_STATUS WINAPI KspGetProviderProperty(NCRYPT_PROV_HANDLE hProvider, LPCWSTR pszProperty, PBYTE pbOutput,
                                              DWORD cbOutput, DWORD* pcbResult, DWORD dwFlags) {
    ProvObj* p = AsProv(hProvider);
    SB_TRACE(L"GetProviderProperty('%s', cb=%lu, flags=0x%X)", pszProperty ? pszProperty : L"(null)", cbOutput, dwFlags);
    if (!p) return NTE_INVALID_HANDLE;
    if (!pszProperty || !pcbResult) return NTE_INVALID_PARAMETER;

    if (_wcsicmp(pszProperty, NCRYPT_NAME_PROPERTY) == 0) return OutString(pbOutput, cbOutput, pcbResult, kProviderName);
    if (_wcsicmp(pszProperty, NCRYPT_IMPL_TYPE_PROPERTY) == 0) return OutDword(pbOutput, cbOutput, pcbResult, p->backend->ImplType());
    if (_wcsicmp(pszProperty, NCRYPT_MAX_NAME_LENGTH_PROPERTY) == 0) return OutDword(pbOutput, cbOutput, pcbResult, kMaxNameLen);
    if (_wcsicmp(pszProperty, NCRYPT_VERSION_PROPERTY) == 0) return OutDword(pbOutput, cbOutput, pcbResult, 0x00010000);
    if (_wcsicmp(pszProperty, NCRYPT_SECURITY_DESCR_SUPPORT_PROPERTY) == 0) return OutDword(pbOutput, cbOutput, pcbResult, 0);
    if (_wcsicmp(pszProperty, NCRYPT_USE_CONTEXT_PROPERTY) == 0) return NTE_NOT_FOUND;

    SB_WARN(L"GetProviderProperty: nieobsługiwana właściwość '%s'", pszProperty);
    return NTE_NOT_SUPPORTED;
}

SECURITY_STATUS WINAPI KspSetProviderProperty(NCRYPT_PROV_HANDLE hProvider, LPCWSTR pszProperty, PBYTE pbInput,
                                              DWORD cbInput, DWORD dwFlags) {
    SB_TRACE(L"SetProviderProperty('%s', cb=%lu, flags=0x%X)", pszProperty ? pszProperty : L"(null)", cbInput, dwFlags);
    if (!AsProv(hProvider)) return NTE_INVALID_HANDLE;
    if (!pszProperty) return NTE_INVALID_PARAMETER;
    // USE_CONTEXT, WINDOW_HANDLE itp. — przyjmujemy i ignorujemy
    return ERROR_SUCCESS;
}

SECURITY_STATUS WINAPI KspGetKeyProperty(NCRYPT_PROV_HANDLE hProvider, NCRYPT_KEY_HANDLE hKey, LPCWSTR pszProperty,
                                         PBYTE pbOutput, DWORD cbOutput, DWORD* pcbResult, DWORD dwFlags) {
    KeyObj* k = AsKey(hKey);
    SB_TRACE(L"GetKeyProperty('%s', cb=%lu, flags=0x%X)", pszProperty ? pszProperty : L"(null)", cbOutput, dwFlags);
    if (!AsProv(hProvider) || !k) return NTE_INVALID_HANDLE;
    if (!pszProperty || !pcbResult) return NTE_INVALID_PARAMETER;
    sb::IKey* key = k->key;

    if (_wcsicmp(pszProperty, NCRYPT_ALGORITHM_GROUP_PROPERTY) == 0) return OutString(pbOutput, cbOutput, pcbResult, key->AlgGroup());
    if (_wcsicmp(pszProperty, NCRYPT_ALGORITHM_PROPERTY) == 0) return OutString(pbOutput, cbOutput, pcbResult, key->AlgId());
    if (_wcsicmp(pszProperty, NCRYPT_LENGTH_PROPERTY) == 0) return OutDword(pbOutput, cbOutput, pcbResult, key->Bits());
    if (_wcsicmp(pszProperty, NCRYPT_BLOCK_LENGTH_PROPERTY) == 0) return OutDword(pbOutput, cbOutput, pcbResult, key->Bits() / 8);
    if (_wcsicmp(pszProperty, NCRYPT_LENGTHS_PROPERTY) == 0) {
        NCRYPT_SUPPORTED_LENGTHS l = { 2048, 4096, 8, 2048 };
        return OutBytes(pbOutput, cbOutput, pcbResult, &l, sizeof(l));
    }
    if (_wcsicmp(pszProperty, NCRYPT_KEY_USAGE_PROPERTY) == 0) return OutDword(pbOutput, cbOutput, pcbResult, NCRYPT_ALLOW_ALL_USAGES);
    if (_wcsicmp(pszProperty, NCRYPT_EXPORT_POLICY_PROPERTY) == 0) return OutDword(pbOutput, cbOutput, pcbResult, 0);
    if (_wcsicmp(pszProperty, NCRYPT_KEY_TYPE_PROPERTY) == 0) return OutDword(pbOutput, cbOutput, pcbResult, 0);
    if (_wcsicmp(pszProperty, NCRYPT_NAME_PROPERTY) == 0) return OutString(pbOutput, cbOutput, pcbResult, k->name.c_str());
    if (_wcsicmp(pszProperty, NCRYPT_UNIQUE_NAME_PROPERTY) == 0) return OutString(pbOutput, cbOutput, pcbResult, k->name.c_str());
    if (_wcsicmp(pszProperty, NCRYPT_PROVIDER_HANDLE_PROPERTY) == 0) {
        NCRYPT_PROV_HANDLE h = hProvider;
        return OutBytes(pbOutput, cbOutput, pcbResult, &h, sizeof(h));
    }
    if (_wcsicmp(pszProperty, NCRYPT_UI_POLICY_PROPERTY) == 0) {
        NCRYPT_UI_POLICY pol = {};
        pol.dwVersion = 1;
        pol.dwFlags = 0;
        return OutBytes(pbOutput, cbOutput, pcbResult, &pol, sizeof(pol));
    }
    if (_wcsicmp(pszProperty, NCRYPT_WINDOW_HANDLE_PROPERTY) == 0) return OutBytes(pbOutput, cbOutput, pcbResult, &k->hwnd, sizeof(k->hwnd));
    if (_wcsicmp(pszProperty, NCRYPT_IMPL_TYPE_PROPERTY) == 0) return OutDword(pbOutput, cbOutput, pcbResult, k->prov->backend->ImplType());
    if (_wcsicmp(pszProperty, NCRYPT_CERTIFICATE_PROPERTY) == 0) return NTE_NOT_FOUND;
    if (_wcsicmp(pszProperty, NCRYPT_SECURITY_DESCR_PROPERTY) == 0) return NTE_NOT_SUPPORTED;
    if (_wcsicmp(pszProperty, NCRYPT_SMARTCARD_GUID_PROPERTY) == 0) return NTE_NOT_FOUND;
    if (_wcsicmp(pszProperty, NCRYPT_READER_PROPERTY) == 0) return NTE_NOT_FOUND;
    if (_wcsicmp(pszProperty, NCRYPT_PIN_PROPERTY) == 0) return NTE_NOT_SUPPORTED;

    SB_WARN(L"GetKeyProperty: nieobsługiwana właściwość '%s'", pszProperty);
    return NTE_NOT_SUPPORTED;
}

SECURITY_STATUS WINAPI KspSetKeyProperty(NCRYPT_PROV_HANDLE hProvider, NCRYPT_KEY_HANDLE hKey, LPCWSTR pszProperty,
                                         PBYTE pbInput, DWORD cbInput, DWORD dwFlags) {
    KeyObj* k = AsKey(hKey);
    SB_INFO(L"SetKeyProperty('%s', cb=%lu, flags=0x%X)", pszProperty ? pszProperty : L"(null)", cbInput, dwFlags);
    if (!AsProv(hProvider) || !k) return NTE_INVALID_HANDLE;
    if (!pszProperty) return NTE_INVALID_PARAMETER;
    if (_wcsicmp(pszProperty, NCRYPT_WINDOW_HANDLE_PROPERTY) == 0 && cbInput == sizeof(HWND)) {
        k->hwnd = *(HWND*)pbInput;
        return ERROR_SUCCESS;
    }
    if (_wcsicmp(pszProperty, NCRYPT_PIN_PROPERTY) == 0) {
        // Program próbuje podać PIN "po cichu". Nie obsługujemy — PIN zawsze wpisuje użytkownik lokalnie.
        SB_WARN(L"SetKeyProperty: program przekazuje PIN_PROPERTY — ignorujemy (PIN zawsze lokalnie)");
        return ERROR_SUCCESS;
    }
    if (_wcsicmp(pszProperty, NCRYPT_CERTIFICATE_PROPERTY) == 0) {
        SB_INFO(L"SetKeyProperty: CERTIFICATE_PROPERTY (%lu B) — przyjęte", cbInput);
        return ERROR_SUCCESS;
    }
    return k->key->SetProperty(pszProperty, pbInput, cbInput, dwFlags);
}

SECURITY_STATUS WINAPI KspIsAlgSupported(NCRYPT_PROV_HANDLE hProvider, LPCWSTR pszAlgId, DWORD dwFlags) {
    SB_TRACE(L"IsAlgSupported('%s', flags=0x%X)", pszAlgId ? pszAlgId : L"(null)", dwFlags);
    if (!AsProv(hProvider)) return NTE_INVALID_HANDLE;
    if (!pszAlgId) return NTE_INVALID_PARAMETER;
    if (_wcsicmp(pszAlgId, BCRYPT_RSA_ALGORITHM) == 0) return ERROR_SUCCESS;
    return NTE_NOT_SUPPORTED;
}

SECURITY_STATUS WINAPI KspEnumAlgorithms(NCRYPT_PROV_HANDLE hProvider, DWORD dwAlgOperations, DWORD* pdwAlgCount,
                                         NCryptAlgorithmName** ppAlgList, DWORD dwFlags) {
    SB_TRACE(L"EnumAlgorithms(ops=0x%X, flags=0x%X)", dwAlgOperations, dwFlags);
    if (!AsProv(hProvider)) return NTE_INVALID_HANDLE;
    if (!pdwAlgCount || !ppAlgList) return NTE_INVALID_PARAMETER;
    const DWORD ourOps = NCRYPT_ASYMMETRIC_ENCRYPTION_OPERATION | NCRYPT_SIGNATURE_OPERATION;
    if (dwAlgOperations != 0 && (dwAlgOperations & ourOps) == 0) { *pdwAlgCount = 0; *ppAlgList = NULL; return ERROR_SUCCESS; }

    size_t cbName = (wcslen(BCRYPT_RSA_ALGORITHM) + 1) * sizeof(wchar_t);
    BYTE* mem = (BYTE*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(NCryptAlgorithmName) + cbName);
    if (!mem) return NTE_NO_MEMORY;
    auto* a = (NCryptAlgorithmName*)mem;
    a->pszName = (LPWSTR)(mem + sizeof(NCryptAlgorithmName));
    memcpy(a->pszName, BCRYPT_RSA_ALGORITHM, cbName);
    a->dwClass = NCRYPT_ASYMMETRIC_ENCRYPTION_INTERFACE;
    a->dwAlgOperations = ourOps;
    a->dwFlags = 0;
    *pdwAlgCount = 1;
    *ppAlgList = a;
    return ERROR_SUCCESS;
}

SECURITY_STATUS WINAPI KspEnumKeys(NCRYPT_PROV_HANDLE hProvider, LPCWSTR pszScope, NCryptKeyName** ppKeyName,
                                   PVOID* ppEnumState, DWORD dwFlags) {
    ProvObj* p = AsProv(hProvider);
    SB_TRACE(L"EnumKeys(scope='%s', flags=0x%X)", pszScope ? pszScope : L"(null)", dwFlags);
    if (!p) return NTE_INVALID_HANDLE;
    if (!ppKeyName || !ppEnumState) return NTE_INVALID_PARAMETER;

    EnumState* st = (EnumState*)*ppEnumState;
    if (!st) {
        std::vector<std::wstring> names;
        p->backend->Enumerate(names);
        st = MakeEnumState(names);
        if (!st) return NTE_NO_MEMORY;
        *ppEnumState = st;
    }
    if (st->magic != kEnumMagic) return NTE_INVALID_PARAMETER;
    if (st->index >= st->count) return NTE_NO_MORE_ITEMS;

    std::wstring n = st->names[st->index++];
    size_t cbN = (n.size() + 1) * sizeof(wchar_t);
    size_t cbA = (wcslen(BCRYPT_RSA_ALGORITHM) + 1) * sizeof(wchar_t);
    BYTE* mem = (BYTE*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(NCryptKeyName) + cbN + cbA);
    if (!mem) return NTE_NO_MEMORY;
    auto* kn = (NCryptKeyName*)mem;
    kn->pszName = (LPWSTR)(mem + sizeof(NCryptKeyName));
    memcpy(kn->pszName, n.c_str(), cbN);
    kn->pszAlgid = (LPWSTR)(mem + sizeof(NCryptKeyName) + cbN);
    memcpy(kn->pszAlgid, BCRYPT_RSA_ALGORITHM, cbA);
    kn->dwLegacyKeySpec = 0;
    kn->dwFlags = 0;
    *ppKeyName = kn;
    return ERROR_SUCCESS;
}

SECURITY_STATUS WINAPI KspImportKey(NCRYPT_PROV_HANDLE hProvider, NCRYPT_KEY_HANDLE hImportKey, LPCWSTR pszBlobType,
                                    NCryptBufferDesc* pParameterList, NCRYPT_KEY_HANDLE* phKey, PBYTE pbData, DWORD cbData,
                                    DWORD dwFlags) {
    SB_INFO(L"ImportKey(blob='%s', cb=%lu, flags=0x%X) -> NOT_SUPPORTED", pszBlobType ? pszBlobType : L"(null)", cbData, dwFlags);
    if (!AsProv(hProvider)) return NTE_INVALID_HANDLE;
    return NTE_NOT_SUPPORTED;
}

SECURITY_STATUS WINAPI KspExportKey(NCRYPT_PROV_HANDLE hProvider, NCRYPT_KEY_HANDLE hKey, NCRYPT_KEY_HANDLE hExportKey,
                                    LPCWSTR pszBlobType, NCryptBufferDesc* pParameterList, PBYTE pbOutput, DWORD cbOutput,
                                    DWORD* pcbResult, DWORD dwFlags) {
    KeyObj* k = AsKey(hKey);
    SB_TRACE(L"ExportKey(blob='%s', cb=%lu, flags=0x%X)", pszBlobType ? pszBlobType : L"(null)", cbOutput, dwFlags);
    if (!AsProv(hProvider) || !k) return NTE_INVALID_HANDLE;
    if (!pszBlobType || !pcbResult) return NTE_INVALID_PARAMETER;
    if (_wcsicmp(pszBlobType, BCRYPT_RSAPUBLIC_BLOB) == 0 || _wcsicmp(pszBlobType, BCRYPT_PUBLIC_KEY_BLOB) == 0 ||
        _wcsicmp(pszBlobType, LEGACY_RSAPUBLIC_BLOB) == 0) {
        return k->key->ExportPublic(pszBlobType, pbOutput, cbOutput, pcbResult);
    }
    SB_WARN(L"ExportKey: odmowa eksportu '%s' (klucz prywatny nie opuszcza nośnika)", pszBlobType);
    return NTE_NOT_SUPPORTED;
}

SECURITY_STATUS WINAPI KspSignHash(NCRYPT_PROV_HANDLE hProvider, NCRYPT_KEY_HANDLE hKey, VOID* pPaddingInfo,
                                   PBYTE pbHashValue, DWORD cbHashValue, PBYTE pbSignature, DWORD cbSignature,
                                   DWORD* pcbResult, DWORD dwFlags) {
    KeyObj* k = AsKey(hKey);
    if (!AsProv(hProvider) || !k) return NTE_INVALID_HANDLE;
    if (!pbHashValue || !pcbResult) return NTE_INVALID_PARAMETER;

    if (!pbSignature) {
        // zapytanie o rozmiar podpisu (RSA: n/8; ECDSA: r||s = 2 * rozmiar współrzędnej)
        DWORD n = (k->key->Bits() + 7) / 8;
        *pcbResult = (_wcsicmp(k->key->AlgGroup(), NCRYPT_ECDSA_ALGORITHM_GROUP) == 0) ? n * 2 : n;
        SB_TRACE(L"SignHash: zapytanie o rozmiar -> %lu", *pcbResult);
        return ERROR_SUCCESS;
    }
    SB_INFO(L">>> SignHash: klucz='%s', %s, skrót %lu B [%s], bufor %lu B",
            k->name.c_str(), DescribePadding(pPaddingInfo, dwFlags).c_str(), cbHashValue,
            Hex(pbHashValue, cbHashValue, 8).c_str(), cbSignature);
    ULONGLONG t0 = GetTickCount64();
    SECURITY_STATUS s = k->key->SignHash(pPaddingInfo, pbHashValue, cbHashValue, pbSignature, cbSignature, pcbResult, dwFlags);
    SB_INFO(L"<<< SignHash: %s, %lu B, %llu ms", sb::StatusText(s).c_str(), *pcbResult, GetTickCount64() - t0);
    return s;
}

SECURITY_STATUS WINAPI KspVerifySignature(NCRYPT_PROV_HANDLE hProvider, NCRYPT_KEY_HANDLE hKey, VOID* pPaddingInfo,
                                          PBYTE pbHashValue, DWORD cbHashValue, PBYTE pbSignature, DWORD cbSignature,
                                          DWORD dwFlags) {
    KeyObj* k = AsKey(hKey);
    SB_INFO(L"VerifySignature(%s, hash %lu B, sig %lu B)", DescribePadding(pPaddingInfo, dwFlags).c_str(), cbHashValue, cbSignature);
    if (!AsProv(hProvider) || !k) return NTE_INVALID_HANDLE;
    return k->key->VerifySignature(pPaddingInfo, pbHashValue, cbHashValue, pbSignature, cbSignature, dwFlags);
}

SECURITY_STATUS WINAPI KspEncrypt(NCRYPT_PROV_HANDLE hProvider, NCRYPT_KEY_HANDLE hKey, PBYTE pbInput, DWORD cbInput,
                                  VOID* pPaddingInfo, PBYTE pbOutput, DWORD cbOutput, DWORD* pcbResult, DWORD dwFlags) {
    SB_INFO(L"Encrypt(cb=%lu, %s) -> NOT_SUPPORTED", cbInput, DescribePadding(pPaddingInfo, dwFlags).c_str());
    if (!AsProv(hProvider) || !AsKey(hKey)) return NTE_INVALID_HANDLE;
    return NTE_NOT_SUPPORTED;
}

SECURITY_STATUS WINAPI KspDecrypt(NCRYPT_PROV_HANDLE hProvider, NCRYPT_KEY_HANDLE hKey, PBYTE pbInput, DWORD cbInput,
                                  VOID* pPaddingInfo, PBYTE pbOutput, DWORD cbOutput, DWORD* pcbResult, DWORD dwFlags) {
    KeyObj* k = AsKey(hKey);
    SB_INFO(L"Decrypt(cb=%lu, %s)", cbInput, DescribePadding(pPaddingInfo, dwFlags).c_str());
    if (!AsProv(hProvider) || !k) return NTE_INVALID_HANDLE;
    return k->key->Decrypt(pPaddingInfo, pbInput, cbInput, pbOutput, cbOutput, pcbResult, dwFlags);
}

SECURITY_STATUS WINAPI KspPromptUser(NCRYPT_PROV_HANDLE hProvider, NCRYPT_KEY_HANDLE hKey, LPCWSTR pszOperation, DWORD dwFlags) {
    SB_INFO(L"PromptUser(op='%s', flags=0x%X)", pszOperation ? pszOperation : L"(null)", dwFlags);
    if (!AsProv(hProvider)) return NTE_INVALID_HANDLE;
    return ERROR_SUCCESS;
}

SECURITY_STATUS WINAPI KspNotifyChangeKey(NCRYPT_PROV_HANDLE hProvider, HANDLE* phEvent, DWORD dwFlags) {
    SB_TRACE(L"NotifyChangeKey(flags=0x%X) -> NOT_SUPPORTED", dwFlags);
    if (!AsProv(hProvider)) return NTE_INVALID_HANDLE;
    return NTE_NOT_SUPPORTED;
}

SECURITY_STATUS WINAPI KspSecretAgreement(NCRYPT_PROV_HANDLE hProvider, NCRYPT_KEY_HANDLE hPrivKey, NCRYPT_KEY_HANDLE hPubKey,
                                          NCRYPT_SECRET_HANDLE* phAgreedSecret, DWORD dwFlags) {
    SB_INFO(L"SecretAgreement -> NOT_SUPPORTED");
    return NTE_NOT_SUPPORTED;
}

SECURITY_STATUS WINAPI KspDeriveKey(NCRYPT_PROV_HANDLE hProvider, NCRYPT_SECRET_HANDLE hSharedSecret, LPCWSTR pwszKDF,
                                    NCryptBufferDesc* pParameterList, PBYTE pbDerivedKey, DWORD cbDerivedKey,
                                    DWORD* pcbResult, ULONG dwFlags) {
    SB_INFO(L"DeriveKey -> NOT_SUPPORTED");
    return NTE_NOT_SUPPORTED;
}

SECURITY_STATUS WINAPI KspFreeSecret(NCRYPT_PROV_HANDLE hProvider, NCRYPT_SECRET_HANDLE hSharedSecret) {
    return NTE_NOT_SUPPORTED;
}

NCRYPT_KEY_STORAGE_FUNCTION_TABLE g_table;

}  // namespace

extern "C" SECURITY_STATUS WINAPI GetKeyStorageInterface(LPCWSTR pszProviderName,
                                                          NCRYPT_KEY_STORAGE_FUNCTION_TABLE** ppFunctionTable,
                                                          DWORD dwFlags) {
    EnsureInit();
    SB_INFO(L"GetKeyStorageInterface(name='%s', flags=0x%X)", pszProviderName ? pszProviderName : L"(null)", dwFlags);
    if (!ppFunctionTable) return NTE_INVALID_PARAMETER;

    ZeroMemory(&g_table, sizeof(g_table));
    g_table.Version = NCRYPT_KEY_STORAGE_INTERFACE_VERSION;
    g_table.OpenProvider = KspOpenProvider;
    g_table.OpenKey = KspOpenKey;
    g_table.CreatePersistedKey = KspCreatePersistedKey;
    g_table.GetProviderProperty = KspGetProviderProperty;
    g_table.GetKeyProperty = KspGetKeyProperty;
    g_table.SetProviderProperty = KspSetProviderProperty;
    g_table.SetKeyProperty = KspSetKeyProperty;
    g_table.FinalizeKey = KspFinalizeKey;
    g_table.DeleteKey = KspDeleteKey;
    g_table.FreeProvider = KspFreeProvider;
    g_table.FreeKey = KspFreeKey;
    g_table.FreeBuffer = KspFreeBuffer;
    g_table.Encrypt = KspEncrypt;
    g_table.Decrypt = KspDecrypt;
    g_table.IsAlgSupported = KspIsAlgSupported;
    g_table.EnumAlgorithms = KspEnumAlgorithms;
    g_table.EnumKeys = KspEnumKeys;
    g_table.ImportKey = KspImportKey;
    g_table.ExportKey = KspExportKey;
    g_table.SignHash = KspSignHash;
    g_table.VerifySignature = KspVerifySignature;
    g_table.PromptUser = KspPromptUser;
    g_table.NotifyChangeKey = KspNotifyChangeKey;
    g_table.SecretAgreement = KspSecretAgreement;
    g_table.DeriveKey = KspDeriveKey;
    g_table.FreeSecret = KspFreeSecret;
    *ppFunctionTable = &g_table;
    return ERROR_SUCCESS;
}

BOOL WINAPI DllMain(HINSTANCE hinst, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hinst);
        InitializeCriticalSection(&g_init);
    } else if (reason == DLL_PROCESS_DETACH) {
        DeleteCriticalSection(&g_init);
    }
    return TRUE;
}
