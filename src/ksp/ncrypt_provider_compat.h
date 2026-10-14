// SignBridge — © 2026 Craftprojects Sp. z o.o. (craftprojects.pl)
// cp-signbridge — definicje interfejsu KSP (zamiennik ncrypt_provider.h z CPDK)
//
// Plik ncrypt_provider.h jest częścią Cryptographic Provider Development Kit, nie zwykłego
// Windows SDK. Żeby nie wymagać CPDK na maszynie budującej, deklarujemy tu to, co potrzebne:
// typy wskaźników na funkcje KSP i tablicę NCRYPT_KEY_STORAGE_FUNCTION_TABLE w wersji 1.0.
// Układ i sygnatury są zgodne z dokumentacją Microsoft (CNG Key Storage Provider interface).
#pragma once
#include <windows.h>
#include <bcrypt.h>
#include <ncrypt.h>

#ifndef NCRYPT_KEY_STORAGE_INTERFACE_VERSION
#define NCRYPT_KEY_STORAGE_INTERFACE_VERSION BCRYPT_MAKE_INTERFACE_VERSION(1, 0)
#endif

typedef SECURITY_STATUS (WINAPI *NCryptOpenStorageProviderFn)(NCRYPT_PROV_HANDLE *phProvider, LPCWSTR pszProviderName, DWORD dwFlags);
typedef SECURITY_STATUS (WINAPI *NCryptOpenKeyFn)(NCRYPT_PROV_HANDLE hProvider, NCRYPT_KEY_HANDLE *phKey, LPCWSTR pszKeyName, DWORD dwLegacyKeySpec, DWORD dwFlags);
typedef SECURITY_STATUS (WINAPI *NCryptCreatePersistedKeyFn)(NCRYPT_PROV_HANDLE hProvider, NCRYPT_KEY_HANDLE *phKey, LPCWSTR pszAlgId, LPCWSTR pszKeyName, DWORD dwLegacyKeySpec, DWORD dwFlags);
typedef SECURITY_STATUS (WINAPI *NCryptGetProviderPropertyFn)(NCRYPT_PROV_HANDLE hProvider, LPCWSTR pszProperty, PBYTE pbOutput, DWORD cbOutput, DWORD *pcbResult, DWORD dwFlags);
typedef SECURITY_STATUS (WINAPI *NCryptGetKeyPropertyFn)(NCRYPT_PROV_HANDLE hProvider, NCRYPT_KEY_HANDLE hKey, LPCWSTR pszProperty, PBYTE pbOutput, DWORD cbOutput, DWORD *pcbResult, DWORD dwFlags);
typedef SECURITY_STATUS (WINAPI *NCryptSetProviderPropertyFn)(NCRYPT_PROV_HANDLE hProvider, LPCWSTR pszProperty, PBYTE pbInput, DWORD cbInput, DWORD dwFlags);
typedef SECURITY_STATUS (WINAPI *NCryptSetKeyPropertyFn)(NCRYPT_PROV_HANDLE hProvider, NCRYPT_KEY_HANDLE hKey, LPCWSTR pszProperty, PBYTE pbInput, DWORD cbInput, DWORD dwFlags);
typedef SECURITY_STATUS (WINAPI *NCryptFinalizeKeyFn)(NCRYPT_PROV_HANDLE hProvider, NCRYPT_KEY_HANDLE hKey, DWORD dwFlags);
typedef SECURITY_STATUS (WINAPI *NCryptDeleteKeyFn)(NCRYPT_PROV_HANDLE hProvider, NCRYPT_KEY_HANDLE hKey, DWORD dwFlags);
typedef SECURITY_STATUS (WINAPI *NCryptFreeProviderFn)(NCRYPT_PROV_HANDLE hProvider);
typedef SECURITY_STATUS (WINAPI *NCryptFreeKeyFn)(NCRYPT_PROV_HANDLE hProvider, NCRYPT_KEY_HANDLE hKey);
typedef SECURITY_STATUS (WINAPI *NCryptFreeBufferFn)(PVOID pvInput);
typedef SECURITY_STATUS (WINAPI *NCryptEncryptFn)(NCRYPT_PROV_HANDLE hProvider, NCRYPT_KEY_HANDLE hKey, PBYTE pbInput, DWORD cbInput, VOID *pPaddingInfo, PBYTE pbOutput, DWORD cbOutput, DWORD *pcbResult, DWORD dwFlags);
typedef SECURITY_STATUS (WINAPI *NCryptDecryptFn)(NCRYPT_PROV_HANDLE hProvider, NCRYPT_KEY_HANDLE hKey, PBYTE pbInput, DWORD cbInput, VOID *pPaddingInfo, PBYTE pbOutput, DWORD cbOutput, DWORD *pcbResult, DWORD dwFlags);
typedef SECURITY_STATUS (WINAPI *NCryptIsAlgSupportedFn)(NCRYPT_PROV_HANDLE hProvider, LPCWSTR pszAlgId, DWORD dwFlags);
typedef SECURITY_STATUS (WINAPI *NCryptEnumAlgorithmsFn)(NCRYPT_PROV_HANDLE hProvider, DWORD dwAlgOperations, DWORD *pdwAlgCount, NCryptAlgorithmName **ppAlgList, DWORD dwFlags);
typedef SECURITY_STATUS (WINAPI *NCryptEnumKeysFn)(NCRYPT_PROV_HANDLE hProvider, LPCWSTR pszScope, NCryptKeyName **ppKeyName, PVOID *ppEnumState, DWORD dwFlags);
typedef SECURITY_STATUS (WINAPI *NCryptImportKeyFn)(NCRYPT_PROV_HANDLE hProvider, NCRYPT_KEY_HANDLE hImportKey, LPCWSTR pszBlobType, NCryptBufferDesc *pParameterList, NCRYPT_KEY_HANDLE *phKey, PBYTE pbData, DWORD cbData, DWORD dwFlags);
typedef SECURITY_STATUS (WINAPI *NCryptExportKeyFn)(NCRYPT_PROV_HANDLE hProvider, NCRYPT_KEY_HANDLE hKey, NCRYPT_KEY_HANDLE hExportKey, LPCWSTR pszBlobType, NCryptBufferDesc *pParameterList, PBYTE pbOutput, DWORD cbOutput, DWORD *pcbResult, DWORD dwFlags);
typedef SECURITY_STATUS (WINAPI *NCryptSignHashFn)(NCRYPT_PROV_HANDLE hProvider, NCRYPT_KEY_HANDLE hKey, VOID *pPaddingInfo, PBYTE pbHashValue, DWORD cbHashValue, PBYTE pbSignature, DWORD cbSignature, DWORD *pcbResult, DWORD dwFlags);
typedef SECURITY_STATUS (WINAPI *NCryptVerifySignatureFn)(NCRYPT_PROV_HANDLE hProvider, NCRYPT_KEY_HANDLE hKey, VOID *pPaddingInfo, PBYTE pbHashValue, DWORD cbHashValue, PBYTE pbSignature, DWORD cbSignature, DWORD dwFlags);
typedef SECURITY_STATUS (WINAPI *NCryptPromptUserFn)(NCRYPT_PROV_HANDLE hProvider, NCRYPT_KEY_HANDLE hKey, LPCWSTR pszOperation, DWORD dwFlags);
typedef SECURITY_STATUS (WINAPI *NCryptNotifyChangeKeyFn)(NCRYPT_PROV_HANDLE hProvider, HANDLE *phEvent, DWORD dwFlags);
typedef SECURITY_STATUS (WINAPI *NCryptSecretAgreementFn)(NCRYPT_PROV_HANDLE hProvider, NCRYPT_KEY_HANDLE hPrivKey, NCRYPT_KEY_HANDLE hPubKey, NCRYPT_SECRET_HANDLE *phAgreedSecret, DWORD dwFlags);
typedef SECURITY_STATUS (WINAPI *NCryptDeriveKeyFn)(NCRYPT_PROV_HANDLE hProvider, NCRYPT_SECRET_HANDLE hSharedSecret, LPCWSTR pwszKDF, NCryptBufferDesc *pParameterList, PBYTE pbDerivedKey, DWORD cbDerivedKey, DWORD *pcbResult, ULONG dwFlags);
typedef SECURITY_STATUS (WINAPI *NCryptFreeSecretFn)(NCRYPT_PROV_HANDLE hProvider, NCRYPT_SECRET_HANDLE hSharedSecret);

typedef struct _NCRYPT_KEY_STORAGE_FUNCTION_TABLE {
    BCRYPT_INTERFACE_VERSION Version;
    NCryptOpenStorageProviderFn OpenProvider;
    NCryptOpenKeyFn OpenKey;
    NCryptCreatePersistedKeyFn CreatePersistedKey;
    NCryptGetProviderPropertyFn GetProviderProperty;
    NCryptGetKeyPropertyFn GetKeyProperty;
    NCryptSetProviderPropertyFn SetProviderProperty;
    NCryptSetKeyPropertyFn SetKeyProperty;
    NCryptFinalizeKeyFn FinalizeKey;
    NCryptDeleteKeyFn DeleteKey;
    NCryptFreeProviderFn FreeProvider;
    NCryptFreeKeyFn FreeKey;
    NCryptFreeBufferFn FreeBuffer;
    NCryptEncryptFn Encrypt;
    NCryptDecryptFn Decrypt;
    NCryptIsAlgSupportedFn IsAlgSupported;
    NCryptEnumAlgorithmsFn EnumAlgorithms;
    NCryptEnumKeysFn EnumKeys;
    NCryptImportKeyFn ImportKey;
    NCryptExportKeyFn ExportKey;
    NCryptSignHashFn SignHash;
    NCryptVerifySignatureFn VerifySignature;
    NCryptPromptUserFn PromptUser;
    NCryptNotifyChangeKeyFn NotifyChangeKey;
    NCryptSecretAgreementFn SecretAgreement;
    NCryptDeriveKeyFn DeriveKey;
    NCryptFreeSecretFn FreeSecret;
} NCRYPT_KEY_STORAGE_FUNCTION_TABLE;
