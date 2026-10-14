// SignBridge — © 2026 Craftprojects Sp. z o.o. (craftprojects.pl)
// cp-signbridge — backend "softkey" (Etap 0: sonda)
//
// Klucz RSA generowany przez BCrypt, zapisywany w
//   %LOCALAPPDATA%\cp-signbridge\keys\<nazwa>.key
// zaszyfrowany DPAPI (zakres użytkownika). Podpis wykonywany lokalnie przez BCrypt.
//
// Cel: zachowywać się jak "prawdziwy" dostawca, żeby program księgowy wykonał pełny
// przebieg podpisu, a my zobaczyli w logu, przez które API idzie.
#include "backend.h"
#include "log.h"
#include <shlobj.h>
#include <wincrypt.h>
#include <dpapi.h>
#include <vector>

#pragma comment(lib, "bcrypt.lib")
#pragma comment(lib, "crypt32.lib")
#pragma comment(lib, "shell32.lib")

namespace sb {
namespace {

std::wstring KeysDir() {
    wchar_t buf[MAX_PATH] = {};
    SHGetFolderPathW(NULL, CSIDL_LOCAL_APPDATA, NULL, SHGFP_TYPE_CURRENT, buf);
    std::wstring dir = std::wstring(buf) + L"\\cp-signbridge";
    CreateDirectoryW(dir.c_str(), NULL);
    dir += L"\\keys";
    CreateDirectoryW(dir.c_str(), NULL);
    return dir;
}

bool ValidName(const std::wstring& n) {
    if (n.empty() || n.size() > 128) return false;
    for (wchar_t c : n) {
        if (c == L'\\' || c == L'/' || c == L':' || c == L'*' || c == L'?' || c == L'"' || c == L'<' || c == L'>' || c == L'|') return false;
    }
    return true;
}

std::wstring KeyPath(const std::wstring& name) { return KeysDir() + L"\\" + name + L".key"; }

bool ReadFileBytes(const std::wstring& path, std::vector<BYTE>& out) {
    HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) return false;
    LARGE_INTEGER sz;
    GetFileSizeEx(h, &sz);
    out.resize((size_t)sz.QuadPart);
    DWORD rd = 0;
    BOOL ok = ReadFile(h, out.data(), (DWORD)out.size(), &rd, NULL);
    CloseHandle(h);
    return ok && rd == out.size();
}

bool WriteFileBytes(const std::wstring& path, const BYTE* data, DWORD cb) {
    HANDLE h = CreateFileW(path.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return false;
    DWORD wr = 0;
    BOOL ok = WriteFile(h, data, cb, &wr, NULL);
    CloseHandle(h);
    return ok && wr == cb;
}

DWORD PadFlagsToBCrypt(DWORD dwFlags) {
    // NCRYPT_PAD_PKCS1_FLAG == BCRYPT_PAD_PKCS1, NCRYPT_PAD_PSS_FLAG == BCRYPT_PAD_PSS,
    // ale przepuszczamy tylko znane bity; NCRYPT_SILENT_FLAG (0x40) nie idzie do BCrypt.
    DWORD f = 0;
    if (dwFlags & NCRYPT_PAD_PKCS1_FLAG) f |= BCRYPT_PAD_PKCS1;
    if (dwFlags & NCRYPT_PAD_PSS_FLAG) f |= BCRYPT_PAD_PSS;
    if (dwFlags & NCRYPT_PAD_OAEP_FLAG) f |= BCRYPT_PAD_OAEP;
    return f;
}

class SoftKey : public IKey {
public:
    SoftKey(const std::wstring& name) : name_(name) {}
    ~SoftKey() override {
        if (hKey_) BCryptDestroyKey(hKey_);
        if (hAlg_) BCryptCloseAlgorithmProvider(hAlg_, 0);
    }

    const std::wstring& Name() const override { return name_; }
    const wchar_t* AlgGroup() const override { return NCRYPT_RSA_ALGORITHM_GROUP; }
    const wchar_t* AlgId() const override { return BCRYPT_RSA_ALGORITHM; }
    DWORD Bits() const override { return bits_; }
    bool IsFinalized() const override { return finalized_; }

    SECURITY_STATUS OpenAlg() {
        if (hAlg_) return ERROR_SUCCESS;
        NTSTATUS st = BCryptOpenAlgorithmProvider(&hAlg_, BCRYPT_RSA_ALGORITHM, NULL, 0);
        if (!BCRYPT_SUCCESS(st)) {
            SB_ERROR(L"softkey: BCryptOpenAlgorithmProvider RSA -> %s", StatusText(st).c_str());
            return NTE_INTERNAL_ERROR;
        }
        return ERROR_SUCCESS;
    }

    // Wczytuje klucz z pliku (DPAPI -> BCRYPT_RSAFULLPRIVATE_BLOB)
    SECURITY_STATUS Load() {
        std::vector<BYTE> enc;
        if (!ReadFileBytes(KeyPath(name_), enc)) {
            SB_WARN(L"softkey: klucz '%s' nie istnieje (%s)", name_.c_str(), KeyPath(name_).c_str());
            return NTE_BAD_KEYSET;
        }
        DATA_BLOB in = { (DWORD)enc.size(), enc.data() };
        DATA_BLOB out = {};
        if (!CryptUnprotectData(&in, NULL, NULL, NULL, NULL, 0, &out)) {
            SB_ERROR(L"softkey: CryptUnprotectData -> %s", StatusText(GetLastError()).c_str());
            return NTE_BAD_KEYSET;
        }
        SECURITY_STATUS s = OpenAlg();
        if (s != ERROR_SUCCESS) { LocalFree(out.pbData); return s; }
        NTSTATUS st = BCryptImportKeyPair(hAlg_, NULL, BCRYPT_RSAFULLPRIVATE_BLOB, &hKey_, out.pbData, out.cbData, 0);
        SecureZeroMemory(out.pbData, out.cbData);
        LocalFree(out.pbData);
        if (!BCRYPT_SUCCESS(st)) {
            SB_ERROR(L"softkey: BCryptImportKeyPair -> %s", StatusText(st).c_str());
            return NTE_BAD_KEYSET;
        }
        DWORD cb = 0;
        BCryptGetProperty(hKey_, BCRYPT_KEY_LENGTH, (PUCHAR)&bits_, sizeof(bits_), &cb, 0);
        finalized_ = true;
        SB_INFO(L"softkey: wczytano klucz '%s' (%lu bit)", name_.c_str(), bits_);
        return ERROR_SUCCESS;
    }

    SECURITY_STATUS SetProperty(LPCWSTR prop, PBYTE pbIn, DWORD cbIn, DWORD) override {
        if (!prop) return NTE_INVALID_PARAMETER;
        if (_wcsicmp(prop, NCRYPT_LENGTH_PROPERTY) == 0 && cbIn == sizeof(DWORD)) {
            bits_ = *(DWORD*)pbIn;
            return ERROR_SUCCESS;
        }
        // Inne właściwości (KEY_USAGE, EXPORT_POLICY, UI_POLICY, WINDOW_HANDLE...) przyjmujemy i ignorujemy
        return ERROR_SUCCESS;
    }

    SECURITY_STATUS Finalize(DWORD) override {
        if (finalized_) return NTE_INVALID_HANDLE;
        SECURITY_STATUS s = OpenAlg();
        if (s != ERROR_SUCCESS) return s;
        if (bits_ == 0) bits_ = 2048;
        NTSTATUS st = BCryptGenerateKeyPair(hAlg_, &hKey_, bits_, 0);
        if (!BCRYPT_SUCCESS(st)) { SB_ERROR(L"softkey: BCryptGenerateKeyPair -> %s", StatusText(st).c_str()); return NTE_INTERNAL_ERROR; }
        st = BCryptFinalizeKeyPair(hKey_, 0);
        if (!BCRYPT_SUCCESS(st)) { SB_ERROR(L"softkey: BCryptFinalizeKeyPair -> %s", StatusText(st).c_str()); return NTE_INTERNAL_ERROR; }

        DWORD cb = 0;
        st = BCryptExportKey(hKey_, NULL, BCRYPT_RSAFULLPRIVATE_BLOB, NULL, 0, &cb, 0);
        if (!BCRYPT_SUCCESS(st)) return NTE_INTERNAL_ERROR;
        std::vector<BYTE> blob(cb);
        st = BCryptExportKey(hKey_, NULL, BCRYPT_RSAFULLPRIVATE_BLOB, blob.data(), cb, &cb, 0);
        if (!BCRYPT_SUCCESS(st)) return NTE_INTERNAL_ERROR;

        DATA_BLOB in = { cb, blob.data() };
        DATA_BLOB out = {};
        if (!CryptProtectData(&in, L"cp-signbridge softkey", NULL, NULL, NULL, 0, &out)) {
            SB_ERROR(L"softkey: CryptProtectData -> %s", StatusText(GetLastError()).c_str());
            SecureZeroMemory(blob.data(), blob.size());
            return NTE_INTERNAL_ERROR;
        }
        SecureZeroMemory(blob.data(), blob.size());
        bool ok = WriteFileBytes(KeyPath(name_), out.pbData, out.cbData);
        LocalFree(out.pbData);
        if (!ok) { SB_ERROR(L"softkey: zapis pliku klucza nieudany"); return NTE_INTERNAL_ERROR; }
        finalized_ = true;
        SB_INFO(L"softkey: wygenerowano i zapisano klucz '%s' (%lu bit)", name_.c_str(), bits_);
        return ERROR_SUCCESS;
    }

    SECURITY_STATUS ExportPublic(LPCWSTR blobType, PBYTE pbOut, DWORD cbOut, DWORD* pcb) override {
        if (!hKey_) return NTE_INVALID_HANDLE;
        LPCWSTR t = blobType;
        if (_wcsicmp(t, BCRYPT_PUBLIC_KEY_BLOB) == 0) t = BCRYPT_RSAPUBLIC_BLOB;
        NTSTATUS st = BCryptExportKey(hKey_, NULL, t, pbOut, cbOut, pcb, 0);
        if (st == STATUS_BUFFER_TOO_SMALL) return NTE_BUFFER_TOO_SMALL;
        if (!BCRYPT_SUCCESS(st)) {
            SB_WARN(L"softkey: ExportKey(%s) -> %s", t, StatusText(st).c_str());
            return NTE_NOT_SUPPORTED;
        }
        return ERROR_SUCCESS;
    }

    SECURITY_STATUS SignHash(VOID* pPad, PBYTE pbHash, DWORD cbHash, PBYTE pbSig, DWORD cbSig, DWORD* pcb, DWORD dwFlags) override {
        if (!hKey_) return NTE_INVALID_HANDLE;
        NTSTATUS st = BCryptSignHash(hKey_, pPad, pbHash, cbHash, pbSig, cbSig, pcb, PadFlagsToBCrypt(dwFlags));
        if (st == STATUS_BUFFER_TOO_SMALL) return NTE_BUFFER_TOO_SMALL;
        if (!BCRYPT_SUCCESS(st)) {
            SB_ERROR(L"softkey: BCryptSignHash -> %s", StatusText(st).c_str());
            return NTE_INTERNAL_ERROR;
        }
        return ERROR_SUCCESS;
    }

    SECURITY_STATUS VerifySignature(VOID* pPad, PBYTE pbHash, DWORD cbHash, PBYTE pbSig, DWORD cbSig, DWORD dwFlags) override {
        if (!hKey_) return NTE_INVALID_HANDLE;
        NTSTATUS st = BCryptVerifySignature(hKey_, pPad, pbHash, cbHash, pbSig, cbSig, PadFlagsToBCrypt(dwFlags));
        if (st == STATUS_INVALID_SIGNATURE) return NTE_BAD_SIGNATURE;
        return BCRYPT_SUCCESS(st) ? ERROR_SUCCESS : NTE_INTERNAL_ERROR;
    }

    SECURITY_STATUS Decrypt(VOID* pPad, PBYTE pbIn, DWORD cbIn, PBYTE pbOut, DWORD cbOut, DWORD* pcb, DWORD dwFlags) override {
        if (!hKey_) return NTE_INVALID_HANDLE;
        NTSTATUS st = BCryptDecrypt(hKey_, pbIn, cbIn, pPad, NULL, 0, pbOut, cbOut, pcb, PadFlagsToBCrypt(dwFlags));
        if (st == STATUS_BUFFER_TOO_SMALL) return NTE_BUFFER_TOO_SMALL;
        return BCRYPT_SUCCESS(st) ? ERROR_SUCCESS : NTE_INTERNAL_ERROR;
    }

private:
    std::wstring name_;
    BCRYPT_ALG_HANDLE hAlg_ = NULL;
    BCRYPT_KEY_HANDLE hKey_ = NULL;
    DWORD bits_ = 0;
    bool finalized_ = false;
};

class SoftKeyBackend : public IBackend {
public:
    const wchar_t* Name() const override { return L"softkey"; }
    DWORD ImplType() const override { return NCRYPT_IMPL_SOFTWARE_FLAG; }

    SECURITY_STATUS Open(const std::wstring& name, IKey** ppKey) override {
        if (!ValidName(name)) return NTE_BAD_KEYSET;
        SoftKey* k = new SoftKey(name);
        SECURITY_STATUS s = k->Load();
        if (s != ERROR_SUCCESS) { delete k; return s; }
        *ppKey = k;
        return ERROR_SUCCESS;
    }

    SECURITY_STATUS Create(const std::wstring& name, LPCWSTR algId, IKey** ppKey) override {
        if (!ValidName(name)) return NTE_INVALID_PARAMETER;
        if (!algId || _wcsicmp(algId, BCRYPT_RSA_ALGORITHM) != 0) {
            SB_WARN(L"softkey: Create nieobsługiwany algorytm '%s'", algId ? algId : L"(null)");
            return NTE_NOT_SUPPORTED;
        }
        *ppKey = new SoftKey(name);
        return ERROR_SUCCESS;
    }

    SECURITY_STATUS Delete(const std::wstring& name) override {
        if (!ValidName(name)) return NTE_BAD_KEYSET;
        if (!DeleteFileW(KeyPath(name).c_str())) return NTE_BAD_KEYSET;
        return ERROR_SUCCESS;
    }

    SECURITY_STATUS Enumerate(std::vector<std::wstring>& names) override {
        WIN32_FIND_DATAW fd;
        HANDLE h = FindFirstFileW((KeysDir() + L"\\*.key").c_str(), &fd);
        if (h == INVALID_HANDLE_VALUE) return ERROR_SUCCESS;
        do {
            std::wstring n = fd.cFileName;
            names.push_back(n.substr(0, n.size() - 4));
        } while (FindNextFileW(h, &fd));
        FindClose(h);
        return ERROR_SUCCESS;
    }
};

}  // namespace

IBackend* CreateSoftKeyBackend() { return new SoftKeyBackend(); }

}  // namespace sb
