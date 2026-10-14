// SignBridge — © 2026 Craftprojects Sp. z o.o. (craftprojects.pl)
// cp-signbridge — pomocnicze operacje na certyfikatach (wspólne dla agenta, KSP i sbctl)
#pragma once
#include <windows.h>
#include <wincrypt.h>
#include <bcrypt.h>
#include <string>
#include <vector>
#include <cstdint>

namespace sb { namespace certs {

// Odcisk SHA-1 jako 40 wielkich znaków hex. To jest "nazwa klucza" w KSP.
std::wstring Thumbprint(PCCERT_CONTEXT cert);
bool IsThumbprint(const std::wstring& s);

// Nazwa podmiotu do logów/UI (CN albo cały DN)
std::wstring SubjectName(PCCERT_CONTEXT cert);

// Otwiera magazyn Osobiste bieżącego użytkownika
HCERTSTORE OpenMyStore();

// Szuka certyfikatu po odcisku; zwrócony kontekst zwalnia wywołujący
PCCERT_CONTEXT FindByThumbprint(HCERTSTORE store, const std::wstring& thumb);

// Klucz publiczny certyfikatu jako uchwyt BCrypt (do eksportu bloba i weryfikacji).
// algName: BCRYPT_RSA_ALGORITHM / BCRYPT_ECDSA_P256_ALGORITHM ...; bits: długość klucza.
bool ImportPublicKey(PCCERT_CONTEXT cert, BCRYPT_KEY_HANDLE* phKey, std::wstring& algName, DWORD& bits);

// Czy certyfikat ma klucz prywatny dostępny w tym systemie i u jakiego dostawcy
bool HasPrivateKey(PCCERT_CONTEXT cert, std::wstring* providerName, DWORD* provType);

// Właściwość-marker "ten certyfikat dodał cp-signbridge sync" (do czyszczenia)
const DWORD kSyncMarkerPropId = CERT_FIRST_USER_PROP_ID + 0x53;
bool SetSyncMarker(PCCERT_CONTEXT cert, const std::wstring& agentId);
bool GetSyncMarker(PCCERT_CONTEXT cert, std::wstring* agentId);

}}  // namespace sb::certs
