// SignBridge — © 2026 Craftprojects Sp. z o.o. (craftprojects.pl)
// cp-signbridge — interfejs backendu klucza
//
// KSP (ksp.cpp) nie wie, gdzie fizycznie jest klucz. Rozmawia z backendem przez ten interfejs.
//   Etap 0 (sonda):   backend_softkey  — klucz RSA w pliku, podpis lokalnie przez BCrypt
//   Etap 1+ (mostek): backend_remote   — skrót leci kanałem wirtualnym RDP do wtyczki mstsc na
//                                        komputerze użytkownika, a wraca gotowy podpis z prawdziwej karty
// Dzięki temu zamiana sondy na mostek nie dotyka kodu KSP.
#pragma once
#include <windows.h>
#include <bcrypt.h>
#include <ncrypt.h>
#include <string>
#include <vector>
#include <cstdint>

// Kody NTSTATUS, których nie ma w windows.h (a używa ich BCrypt)
#ifndef STATUS_SUCCESS
#define STATUS_SUCCESS ((NTSTATUS)0x00000000L)
#endif
#ifndef STATUS_BUFFER_TOO_SMALL
#define STATUS_BUFFER_TOO_SMALL ((NTSTATUS)0xC0000023L)
#endif
#ifndef STATUS_INVALID_SIGNATURE
#define STATUS_INVALID_SIGNATURE ((NTSTATUS)0xC000A000L)
#endif

namespace sb {

class IKey {
public:
    virtual ~IKey() {}
    virtual const std::wstring& Name() const = 0;
    virtual const wchar_t* AlgGroup() const = 0;   // NCRYPT_RSA_ALGORITHM_GROUP / NCRYPT_ECDSA_ALGORITHM_GROUP
    virtual const wchar_t* AlgId() const = 0;      // BCRYPT_RSA_ALGORITHM / BCRYPT_ECDSA_P256_ALGORITHM ...
    virtual DWORD Bits() const = 0;                // długość klucza w bitach
    virtual bool IsFinalized() const = 0;

    // Eksport klucza publicznego w formacie CNG (BCRYPT_RSAPUBLIC_BLOB itd.)
    virtual SECURITY_STATUS ExportPublic(LPCWSTR blobType, PBYTE pbOut, DWORD cbOut, DWORD* pcbResult) = 0;

    // Podpis skrótu. pPadding/dwFlags dokładnie jak w NCryptSignHash.
    virtual SECURITY_STATUS SignHash(VOID* pPadding, PBYTE pbHash, DWORD cbHash,
                                     PBYTE pbSig, DWORD cbSig, DWORD* pcbResult, DWORD dwFlags) = 0;

    // Weryfikacja podpisu (kluczem publicznym — zawsze da się zrobić lokalnie)
    virtual SECURITY_STATUS VerifySignature(VOID* pPadding, PBYTE pbHash, DWORD cbHash,
                                            PBYTE pbSig, DWORD cbSig, DWORD dwFlags) = 0;

    // Deszyfrowanie (potrzebne tylko dla kluczy szyfrujących; dla podpisu kwalifikowanego zwykle NTE_NOT_SUPPORTED)
    virtual SECURITY_STATUS Decrypt(VOID* pPadding, PBYTE pbIn, DWORD cbIn,
                                    PBYTE pbOut, DWORD cbOut, DWORD* pcbResult, DWORD dwFlags) = 0;

    // Właściwości ustawiane przed FinalizeKey (np. NCRYPT_LENGTH_PROPERTY)
    virtual SECURITY_STATUS SetProperty(LPCWSTR name, PBYTE pbIn, DWORD cbIn, DWORD dwFlags) = 0;

    // Domknięcie klucza tworzonego przez CreatePersistedKey
    virtual SECURITY_STATUS Finalize(DWORD dwFlags) = 0;
};

class IBackend {
public:
    virtual ~IBackend() {}
    virtual const wchar_t* Name() const = 0;   // do logów

    // Otwiera istniejący klucz o podanej nazwie
    virtual SECURITY_STATUS Open(const std::wstring& name, IKey** ppKey) = 0;

    // Tworzy nowy (niesfinalizowany) klucz — tylko backendy, które umieją generować klucze
    virtual SECURITY_STATUS Create(const std::wstring& name, LPCWSTR algId, IKey** ppKey) = 0;

    virtual SECURITY_STATUS Delete(const std::wstring& name) = 0;

    virtual SECURITY_STATUS Enumerate(std::vector<std::wstring>& names) = 0;

    // Czy backend jest "sprzętowy" z punktu widzenia CNG (NCRYPT_IMPL_TYPE_PROPERTY)
    virtual DWORD ImplType() const = 0;
};

// Fabryki backendów (wybór w ksp.cpp)
IBackend* CreateSoftKeyBackend();   // Etap 0: klucz programowy (sonda)
IBackend* CreateRemoteBackend();    // Etap 1+: wtyczka mstsc na komputerze użytkownika (kanał RDP)
IBackend* CreateCompositeBackend(); // nazwa = odcisk SHA-1 -> remote, inaczej -> softkey

}  // namespace sb
