// SignBridge — © 2026 Craftprojects Sp. z o.o. (craftprojects.pl)
// cp-signbridge — sbctl: narzędzie instalacji, konfiguracji i testów (strona serwera)
//
// Instalacja (admin):
//   sbctl install                kopiuje DLL do System32/SysWOW64, rejestruje KSP w CNG, tworzy katalog logów
//   sbctl uninstall              wyrejestrowuje KSP, usuwa DLL
// Mostek (konto użytkownika, w sesji RDP; kanał wirtualny RDP do wtyczki mstsc):
//   sbctl agent-test             łączy się z wtyczką na komputerze użytkownika i wypisuje jej certyfikaty
//   sbctl sync                   wgrywa certyfikaty z komputera użytkownika do magazynu Osobiste (klucz -> nasz KSP)
//   sbctl unsync                 usuwa wgrane certyfikaty
//   sbctl test-sign [odcisk]     podpisuje testowy skrót zdalnie (pełna ścieżka przez kanał RDP)
// Sonda (Etap 0):
//   sbctl create-cert / delete-cert / sign-test
// Diagnostyka:
//   sbctl status, sbctl logs
#include <windows.h>
#include <bcrypt.h>
#include <ncrypt.h>
#include <wincrypt.h>
#include <shlobj.h>
#include <stdio.h>
#include <io.h>
#include <fcntl.h>
#include <string>
#include <vector>
#include <memory>
#include "../common/protocol.h"
#include "../common/certs.h"
#include "../common/agentclient.h"
#include "../common/dvc.h"
#include "../common/syncops.h"
#include "../common/kspsetup.h"

#pragma comment(lib, "bcrypt.lib")
#pragma comment(lib, "ncrypt.lib")
#pragma comment(lib, "crypt32.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "shell32.lib")

namespace {

const wchar_t* const kProviderName = L"SignBridge Key Storage Provider";
const wchar_t* const kDllName = L"cpsignbridge.dll";
const wchar_t* const kKeyName = L"signbridge-probe";
const wchar_t* const kCertMarker = L"SignBridge Probe";

std::wstring Status(LONG s) {
    wchar_t* msg = nullptr;
    std::wstring out;
    if (FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                       NULL, (DWORD)s, 0, (LPWSTR)&msg, 0, NULL) && msg) {
        out = msg;
        LocalFree(msg);
        while (!out.empty() && (out.back() == L'\r' || out.back() == L'\n' || out.back() == L' ')) out.pop_back();
    }
    wchar_t hex[32];
    swprintf_s(hex, L"0x%08X", (unsigned)s);
    return out.empty() ? std::wstring(hex) : std::wstring(hex) + L" (" + out + L")";
}

std::wstring ExeDir() {
    wchar_t p[MAX_PATH];
    GetModuleFileNameW(NULL, p, MAX_PATH);
    std::wstring s(p);
    return s.substr(0, s.find_last_of(L"\\/"));
}

std::wstring SysDir(bool wow64) {
    wchar_t win[MAX_PATH];
    GetWindowsDirectoryW(win, MAX_PATH);
    return std::wstring(win) + (wow64 ? L"\\SysWOW64" : L"\\System32");
}

std::wstring ProgramData() {
    wchar_t buf[MAX_PATH] = {};
    SHGetFolderPathW(NULL, CSIDL_COMMON_APPDATA, NULL, SHGFP_TYPE_CURRENT, buf);
    return buf;
}

bool IsAdmin() {
    BOOL admin = FALSE;
    PSID sid = NULL;
    SID_IDENTIFIER_AUTHORITY nt = SECURITY_NT_AUTHORITY;
    if (AllocateAndInitializeSid(&nt, 2, SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &sid)) {
        CheckTokenMembership(NULL, sid, &admin);
        FreeSid(sid);
    }
    return admin == TRUE;
}

int Run(const std::wstring& cmd) {
    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi = {};
    std::wstring c = cmd;
    if (!CreateProcessW(NULL, &c[0], NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) return -1;
    WaitForSingleObject(pi.hProcess, 30000);
    DWORD code = 1;
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return (int)code;
}

bool FileExists(const std::wstring& p) { return GetFileAttributesW(p.c_str()) != INVALID_FILE_ATTRIBUTES; }

// ---------------------------------------------------------------------------

int CmdInstall() {
    std::vector<std::wstring> log;
    bool ok = sb::kspsetup::InstallServer(ExeDir(), log);
    for (auto& l : log) wprintf(L"%s\n", l.c_str());
    return ok ? 0 : 2;
}

int CmdUninstall() {
    std::vector<std::wstring> log;
    bool ok = sb::kspsetup::UninstallServer(log);
    for (auto& l : log) wprintf(L"%s\n", l.c_str());
    return ok ? 0 : 2;
}

// Buduje rozszerzenia: KeyUsage(digitalSignature|nonRepudiation) + EKU(e-mail, podpis dokumentów)
bool BuildExtensions(std::vector<BYTE>& ku, std::vector<BYTE>& eku, CERT_EXTENSION out[2]) {
    BYTE usage = CERT_DIGITAL_SIGNATURE_KEY_USAGE | CERT_NON_REPUDIATION_KEY_USAGE;
    CRYPT_BIT_BLOB bits = { 1, &usage, 0 };
    DWORD cb = 0;
    if (!CryptEncodeObject(X509_ASN_ENCODING, X509_KEY_USAGE, &bits, NULL, &cb)) return false;
    ku.resize(cb);
    if (!CryptEncodeObject(X509_ASN_ENCODING, X509_KEY_USAGE, &bits, ku.data(), &cb)) return false;

    LPSTR oids[] = { (LPSTR)szOID_PKIX_KP_EMAIL_PROTECTION, (LPSTR)"1.3.6.1.4.1.311.10.3.12" /* document signing */ };
    CERT_ENHKEY_USAGE eu = { 2, oids };
    cb = 0;
    if (!CryptEncodeObject(X509_ASN_ENCODING, X509_ENHANCED_KEY_USAGE, &eu, NULL, &cb)) return false;
    eku.resize(cb);
    if (!CryptEncodeObject(X509_ASN_ENCODING, X509_ENHANCED_KEY_USAGE, &eu, eku.data(), &cb)) return false;

    out[0].pszObjId = (LPSTR)szOID_KEY_USAGE;
    out[0].fCritical = TRUE;
    out[0].Value.cbData = (DWORD)ku.size();
    out[0].Value.pbData = ku.data();
    out[1].pszObjId = (LPSTR)szOID_ENHANCED_KEY_USAGE;
    out[1].fCritical = FALSE;
    out[1].Value.cbData = (DWORD)eku.size();
    out[1].Value.pbData = eku.data();
    return true;
}

int CmdCreateCert() {
    NCRYPT_PROV_HANDLE hProv = 0;
    SECURITY_STATUS s = NCryptOpenStorageProvider(&hProv, kProviderName, 0);
    if (s != ERROR_SUCCESS) { wprintf(L"[!] NCryptOpenStorageProvider: %s (czy wykonano 'sbctl install'?)\n", Status(s).c_str()); return 2; }

    NCRYPT_KEY_HANDLE hKey = 0;
    s = NCryptCreatePersistedKey(hProv, &hKey, NCRYPT_RSA_ALGORITHM, kKeyName, 0, NCRYPT_OVERWRITE_KEY_FLAG);
    if (s != ERROR_SUCCESS) { wprintf(L"[!] NCryptCreatePersistedKey: %s\n", Status(s).c_str()); return 2; }
    DWORD bits = 2048;
    NCryptSetProperty(hKey, NCRYPT_LENGTH_PROPERTY, (PBYTE)&bits, sizeof(bits), 0);
    s = NCryptFinalizeKey(hKey, 0);
    if (s != ERROR_SUCCESS) { wprintf(L"[!] NCryptFinalizeKey: %s\n", Status(s).c_str()); return 2; }
    wprintf(L"[+] Klucz '%s' utworzony w KSP\n", kKeyName);

    wchar_t user[256] = {};
    DWORD n = 256;
    GetUserNameW(user, &n);
    std::wstring subject = std::wstring(L"CN=") + kCertMarker + L" (" + user + L"), O=cp-signbridge, C=PL";
    DWORD cbName = 0;
    if (!CertStrToNameW(X509_ASN_ENCODING, subject.c_str(), CERT_X500_NAME_STR, NULL, NULL, &cbName, NULL)) { wprintf(L"[!] CertStrToName\n"); return 2; }
    std::vector<BYTE> name(cbName);
    CertStrToNameW(X509_ASN_ENCODING, subject.c_str(), CERT_X500_NAME_STR, NULL, name.data(), &cbName, NULL);
    CERT_NAME_BLOB nameBlob = { cbName, name.data() };

    CRYPT_KEY_PROV_INFO kpi = {};
    kpi.pwszContainerName = (LPWSTR)kKeyName;
    kpi.pwszProvName = (LPWSTR)kProviderName;
    kpi.dwProvType = 0;   // 0 = dostawca CNG (KSP)
    kpi.dwKeySpec = 0;

    CRYPT_ALGORITHM_IDENTIFIER sigAlg = {};
    sigAlg.pszObjId = (LPSTR)szOID_RSA_SHA256RSA;

    SYSTEMTIME expire;
    GetSystemTime(&expire);
    expire.wYear += 2;

    std::vector<BYTE> ku, eku;
    CERT_EXTENSION exts[2] = {};
    if (!BuildExtensions(ku, eku, exts)) { wprintf(L"[!] Budowa rozszerzeń nieudana\n"); return 2; }
    CERT_EXTENSIONS extList = { 2, exts };

    PCCERT_CONTEXT cert = CertCreateSelfSignCertificate(hKey, &nameBlob, 0, &kpi, &sigAlg, NULL, &expire, &extList);
    if (!cert) { wprintf(L"[!] CertCreateSelfSignCertificate: %s\n", Status(GetLastError()).c_str()); return 2; }

    CRYPT_DATA_BLOB fname = { (DWORD)((wcslen(kCertMarker) + 1) * sizeof(wchar_t)), (BYTE*)kCertMarker };
    CertSetCertificateContextProperty(cert, CERT_FRIENDLY_NAME_PROP_ID, 0, &fname);

    HCERTSTORE store = CertOpenStore(CERT_STORE_PROV_SYSTEM_W, 0, 0, CERT_SYSTEM_STORE_CURRENT_USER, L"MY");
    if (!store) { wprintf(L"[!] CertOpenStore MY\n"); return 2; }
    if (!CertAddCertificateContextToStore(store, cert, CERT_STORE_ADD_REPLACE_EXISTING, NULL)) {
        wprintf(L"[!] CertAddCertificateContextToStore: %s\n", Status(GetLastError()).c_str());
        return 2;
    }
    wchar_t thumb[128] = {};
    BYTE hash[20];
    DWORD cbHash = 20;
    CertGetCertificateContextProperty(cert, CERT_SHA1_HASH_PROP_ID, hash, &cbHash);
    for (DWORD i = 0; i < cbHash; ++i) swprintf_s(thumb + i * 2, 128 - i * 2, L"%02X", hash[i]);
    wprintf(L"[+] Certyfikat dodany do magazynu Osobiste (CurrentUser\\My)\n    Podmiot: %s\n    Odcisk:  %s\n    Ważny do: %04u-%02u-%02u\n",
            subject.c_str(), thumb, expire.wYear, expire.wMonth, expire.wDay);
    wprintf(L"\nTeraz w programie księgowym wybierz certyfikat \"%s\" i spróbuj podpisać/wysłać.\n", kCertMarker);
    CertCloseStore(store, 0);
    CertFreeCertificateContext(cert);
    NCryptFreeObject(hKey);
    NCryptFreeObject(hProv);
    return 0;
}

PCCERT_CONTEXT FindProbeCert(HCERTSTORE store, PCCERT_CONTEXT prev) {
    PCCERT_CONTEXT c = prev;
    while ((c = CertEnumCertificatesInStore(store, c)) != NULL) {
        wchar_t subj[512] = {};
        CertGetNameStringW(c, CERT_NAME_SIMPLE_DISPLAY_TYPE, 0, NULL, subj, 512);
        if (wcsstr(subj, kCertMarker)) return c;
    }
    return NULL;
}

int CmdDeleteCert() {
    HCERTSTORE store = CertOpenStore(CERT_STORE_PROV_SYSTEM_W, 0, 0, CERT_SYSTEM_STORE_CURRENT_USER, L"MY");
    if (!store) { wprintf(L"[!] CertOpenStore MY\n"); return 2; }
    int removed = 0;
    PCCERT_CONTEXT c = NULL;
    while ((c = FindProbeCert(store, NULL)) != NULL) {
        PCCERT_CONTEXT dup = CertDuplicateCertificateContext(c);
        CertDeleteCertificateFromStore(dup);
        CertFreeCertificateContext(c);
        ++removed;
        if (removed > 20) break;
    }
    CertCloseStore(store, 0);
    wprintf(L"[+] Usunięto certyfikatów: %d\n", removed);

    NCRYPT_PROV_HANDLE hProv = 0;
    if (NCryptOpenStorageProvider(&hProv, kProviderName, 0) == ERROR_SUCCESS) {
        NCRYPT_KEY_HANDLE hKey = 0;
        if (NCryptOpenKey(hProv, &hKey, kKeyName, 0, 0) == ERROR_SUCCESS) {
            SECURITY_STATUS s = NCryptDeleteKey(hKey, 0);
            wprintf(L"[.] Usunięcie klucza: %s\n", Status(s).c_str());
        }
        NCryptFreeObject(hProv);
    }
    return 0;
}

int CmdSignTest() {
    HCERTSTORE store = CertOpenStore(CERT_STORE_PROV_SYSTEM_W, 0, 0, CERT_SYSTEM_STORE_CURRENT_USER, L"MY");
    PCCERT_CONTEXT cert = store ? FindProbeCert(store, NULL) : NULL;
    if (!cert) { wprintf(L"[!] Brak certyfikatu '%s' — wykonaj 'sbctl create-cert'\n", kCertMarker); return 2; }

    BYTE hash[32];
    for (int i = 0; i < 32; ++i) hash[i] = (BYTE)(i * 7 + 3);

    // 1) Ścieżka nowoczesna (CNG) — tak robią programy .NET (RSA.SignHash) i nowsze aplikacje natywne
    wprintf(L"[1] CNG (CryptAcquireCertificatePrivateKey + NCryptSignHash)\n");
    HCRYPTPROV_OR_NCRYPT_KEY_HANDLE hk = 0;
    DWORD spec = 0;
    BOOL freeIt = FALSE;
    if (!CryptAcquireCertificatePrivateKey(cert, CRYPT_ACQUIRE_ONLY_NCRYPT_KEY_FLAG, NULL, &hk, &spec, &freeIt)) {
        wprintf(L"    [!] CryptAcquireCertificatePrivateKey(ONLY_NCRYPT): %s\n", Status(GetLastError()).c_str());
    } else {
        BCRYPT_PKCS1_PADDING_INFO pad = { BCRYPT_SHA256_ALGORITHM };
        BYTE sig[512];
        DWORD cbSig = 0;
        SECURITY_STATUS s = NCryptSignHash((NCRYPT_KEY_HANDLE)hk, &pad, hash, 32, sig, sizeof(sig), &cbSig, NCRYPT_PAD_PKCS1_FLAG);
        wprintf(L"    NCryptSignHash(SHA256/PKCS1): %s, %lu B\n", Status(s).c_str(), cbSig);
        if (s == ERROR_SUCCESS) {
            s = NCryptVerifySignature((NCRYPT_KEY_HANDLE)hk, &pad, hash, 32, sig, cbSig, NCRYPT_PAD_PKCS1_FLAG);
            wprintf(L"    NCryptVerifySignature: %s\n", Status(s).c_str());
        }
        if (freeIt) NCryptFreeObject(hk);
    }

    // 2) Ścieżka stara (CAPI) — tak robią starsze programy (CryptAcquireContext + CryptSignHash)
    wprintf(L"[2] CAPI (CryptAcquireCertificatePrivateKey bez ALLOW_NCRYPT)\n");
    hk = 0; spec = 0; freeIt = FALSE;
    if (!CryptAcquireCertificatePrivateKey(cert, 0, NULL, &hk, &spec, &freeIt)) {
        wprintf(L"    [i] Program używający wyłącznie CAPI dostałby błąd: %s\n", Status(GetLastError()).c_str());
        wprintf(L"        (to oczekiwane — KSP nie ma warstwy CSP; jeśli Optima/Symfonia tak robią, dopiszemy CSP)\n");
    } else {
        wprintf(L"    [i] Uzyskano uchwyt (spec=%lu) — Windows sam zmostkował do CNG\n", spec);
        if (freeIt) { if (spec == CERT_NCRYPT_KEY_SPEC) NCryptFreeObject(hk); else CryptReleaseContext(hk, 0); }
    }

    // 3) Jak zobaczy to .NET: CryptAcquireCertificatePrivateKey z ALLOW_NCRYPT (domyślne w X509Certificate2)
    wprintf(L"[3] Mieszana (ALLOW_NCRYPT_KEY_FLAG — tak robi .NET/X509Certificate2)\n");
    hk = 0; spec = 0; freeIt = FALSE;
    if (!CryptAcquireCertificatePrivateKey(cert, CRYPT_ACQUIRE_ALLOW_NCRYPT_KEY_FLAG, NULL, &hk, &spec, &freeIt)) {
        wprintf(L"    [!] %s\n", Status(GetLastError()).c_str());
    } else {
        wprintf(L"    OK, spec=%s\n", spec == CERT_NCRYPT_KEY_SPEC ? L"NCRYPT" : L"CAPI");
        if (freeIt) { if (spec == CERT_NCRYPT_KEY_SPEC) NCryptFreeObject(hk); else CryptReleaseContext(hk, 0); }
    }

    CertFreeCertificateContext(cert);
    CertCloseStore(store, 0);
    wprintf(L"\nSzczegóły w logu KSP (sbctl logs).\n");
    return 0;
}

int CmdStatus() {
    wprintf(L"Dostawca: %s\n", kProviderName);
    NCRYPT_PROV_HANDLE h = 0;
    SECURITY_STATUS s = NCryptOpenStorageProvider(&h, kProviderName, 0);
    wprintf(L"  rejestracja CNG: %s\n", s == ERROR_SUCCESS ? L"OK" : Status(s).c_str());
    if (s == ERROR_SUCCESS) NCryptFreeObject(h);
    for (bool w : { false, true }) {
        std::wstring p = SysDir(w) + L"\\" + kDllName;
        wprintf(L"  %s: %s\n", p.c_str(), FileExists(p) ? L"jest" : L"BRAK");
    }
    HCERTSTORE store = CertOpenStore(CERT_STORE_PROV_SYSTEM_W, 0, 0, CERT_SYSTEM_STORE_CURRENT_USER, L"MY");
    PCCERT_CONTEXT c = store ? FindProbeCert(store, NULL) : NULL;
    wprintf(L"  certyfikat '%s' w magazynie Osobiste: %s\n", kCertMarker, c ? L"jest" : L"BRAK");
    if (c) CertFreeCertificateContext(c);
    if (store) CertCloseStore(store, 0);
    wprintf(L"  logi: %s\\cp-signbridge\\logs\n", ProgramData().c_str());
    return 0;
}

int CmdLogs() {
    std::wstring dir = ProgramData() + L"\\cp-signbridge\\logs";
    wprintf(L"%s\n", dir.c_str());
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW((dir + L"\\*.log").c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) { wprintf(L"  (brak plików)\n"); return 0; }
    do {
        ULARGE_INTEGER sz = { fd.nFileSizeLow, fd.nFileSizeHigh };
        wprintf(L"  %-50s %8llu B\n", fd.cFileName, sz.QuadPart);
    } while (FindNextFileW(h, &fd));
    FindClose(h);
    return 0;
}


// ---------------------------------------------------------------------------
// Mostek: synchronizacja certyfikatów, test zdalnego podpisu (kanał wirtualny RDP)
// ---------------------------------------------------------------------------

int CmdAgentTest() {
    std::wstring err;
    sb::AgentClient agent;
    wprintf(L"Kanał wirtualny RDP%s\n", sb::dvc::InRemoteSession() ? L"" : L"  (uwaga: to nie jest sesja RDP)");
    if (agent.Open(L"sbctl", err) != ERROR_SUCCESS) { wprintf(L"[!] %s\n", err.c_str()); return 2; }
    wprintf(L"[+] Połączono z wtyczką: %s\n", agent.AgentInfo().c_str());
    sb::proto::Certs list;
    if (agent.ListCerts(list, err) != ERROR_SUCCESS) { wprintf(L"[!] %s\n", err.c_str()); return 2; }
    wprintf(L"[+] Certyfikaty u użytkownika: %u\n", (unsigned)list.items.size());
    for (auto& e : list.items) {
        PCCERT_CONTEXT c = CertCreateCertificateContext(X509_ASN_ENCODING, e.der.data(), (DWORD)e.der.size());
        if (!c) continue;
        wprintf(L"    %s  %s\n", sb::certs::Thumbprint(c).c_str(), sb::certs::SubjectName(c).c_str());
        CertFreeCertificateContext(c);
    }
    return 0;
}

int CmdSync() {
    std::vector<std::wstring> log; int a = 0, d = 0;
    SECURITY_STATUS st = sb::syncops::Sync(log, a, d);
    for (auto& l : log) wprintf(L"%s\n", l.c_str());
    if (st != ERROR_SUCCESS) return 2;
    wprintf(L"\n[+] Zsynchronizowano: dodano/odświeżono %d, usunięto %d\n", a, d);
    return 0;
}

int CmdUnsync() {
    int n = sb::syncops::Unsync();
    wprintf(L"[+] Usunięto: %d\n", n);
    return 0;
}

int CmdTestSign(int argc, wchar_t** argv) {
    std::vector<std::wstring> log;
    SECURITY_STATUS st = sb::syncops::TestSign(argc > 2 ? argv[2] : L"", log);
    for (auto& l : log) wprintf(L"%s\n", l.c_str());
    return st == ERROR_SUCCESS ? 0 : 3;
}

}  // namespace

int wmain(int argc, wchar_t** argv) {
    _setmode(_fileno(stdout), _O_U8TEXT);
    std::wstring cmd = argc > 1 ? argv[1] : L"";
    if (cmd == L"install") return CmdInstall();
    if (cmd == L"uninstall") return CmdUninstall();
    if (cmd == L"create-cert") return CmdCreateCert();
    if (cmd == L"delete-cert") return CmdDeleteCert();
    if (cmd == L"sign-test") return CmdSignTest();
    if (cmd == L"status") return CmdStatus();
    if (cmd == L"logs") return CmdLogs();
    if (cmd == L"agent-test") return CmdAgentTest();
    if (cmd == L"sync") return CmdSync();
    if (cmd == L"unsync") return CmdUnsync();
    if (cmd == L"test-sign") return CmdTestSign(argc, argv);
    wprintf(L"cp-signbridge sbctl\n\n"
            L"Instalacja (admin):\n"
            L"  sbctl install | uninstall\n"
            L"Mostek (konto użytkownika w sesji RDP, kanał wirtualny sesji):\n"
            L"  sbctl agent-test\n"
            L"  sbctl sync | unsync\n"
            L"  sbctl test-sign [odcisk]\n"
            L"Sonda:\n"
            L"  sbctl create-cert | delete-cert | sign-test      (certyfikat \"%s\")\n"
            L"Diagnostyka:\n"
            L"  sbctl status | logs\n", kCertMarker);
    return 1;
}
