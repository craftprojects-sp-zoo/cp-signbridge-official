// SignBridge — © 2026 Craftprojects Sp. z o.o. (craftprojects.pl)
// Instalator serwera (kspsetup): kasowanie bez wchodzenia w dowiązania, prawa katalogów danych i pakietu
#include "check.h"
#include "kspsetup.h"
#include <aclapi.h>
#include <sddl.h>

using namespace sb::kspsetup;

namespace {

// SID-y „wszyscy zwykli użytkownicy”: Authenticated Users, Users, Everyone, INTERACTIVE
bool IsBroadSid(PSID sid) {
    for (WELL_KNOWN_SID_TYPE w : { WinAuthenticatedUserSid, WinBuiltinUsersSid, WinWorldSid, WinInteractiveSid })
        if (IsWellKnownSid(sid, w)) return true;
    return false;
}

const DWORD kWriteBits = FILE_WRITE_DATA | FILE_APPEND_DATA | FILE_WRITE_EA | FILE_WRITE_ATTRIBUTES | FILE_DELETE_CHILD |
                         DELETE | WRITE_DAC | WRITE_OWNER | GENERIC_WRITE | GENERIC_ALL;

struct AclInfo { bool ok = false; bool isProtected = false; DWORD broadMask = 0; bool broadInherits = false; };

AclInfo ReadAcl(const std::wstring& path) {
    AclInfo r;
    PACL dacl = nullptr; PSECURITY_DESCRIPTOR sd = nullptr;
    if (GetNamedSecurityInfoW(path.c_str(), SE_FILE_OBJECT, DACL_SECURITY_INFORMATION, NULL, NULL, &dacl, NULL, &sd) != ERROR_SUCCESS) return r;
    SECURITY_DESCRIPTOR_CONTROL ctl = 0; DWORD rev = 0;
    GetSecurityDescriptorControl(sd, &ctl, &rev);
    r.isProtected = (ctl & SE_DACL_PROTECTED) != 0;
    for (DWORD i = 0; dacl && i < dacl->AceCount; ++i) {
        ACE_HEADER* h = nullptr;
        if (!GetAce(dacl, i, (LPVOID*)&h) || h->AceType != ACCESS_ALLOWED_ACE_TYPE) continue;
        ACCESS_ALLOWED_ACE* a = (ACCESS_ALLOWED_ACE*)h;
        if (!IsBroadSid((PSID)&a->SidStart)) continue;
        r.broadMask |= a->Mask;
        if (h->AceFlags & (OBJECT_INHERIT_ACE | CONTAINER_INHERIT_ACE)) r.broadInherits = true;
    }
    LocalFree(sd);
    r.ok = true;
    return r;
}

// Prawa jak po instalacji 1.0.0: icacls <dir> /grant *S-1-5-11:(OI)(CI)M /T
void Grant100(const std::wstring& dir) {
    wchar_t sys[MAX_PATH]; GetSystemDirectoryW(sys, MAX_PATH);
    std::wstring cmd = L"\"" + std::wstring(sys) + L"\\icacls.exe\" \"" + dir + L"\" /grant *S-1-5-11:(OI)(CI)M /T /Q";
    STARTUPINFOW si = { sizeof(si) }; PROCESS_INFORMATION pi = {};
    if (!CreateProcessW(NULL, &cmd[0], NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) return;
    WaitForSingleObject(pi.hProcess, 30000);
    CloseHandle(pi.hThread); CloseHandle(pi.hProcess);
}

}  // namespace

// S2: junction w kasowanym katalogu nie może prowadzić do kasowania plików poza nim
TEST(remove_tree_does_not_follow_junction) {
    std::wstring root = t::TempDir(L"rmtree");
    std::wstring target = root + L"\\cel", tree = root + L"\\drzewo";
    CreateDirectoryW(target.c_str(), NULL);
    t::WriteFileText(target + L"\\zostaje.txt", "x");
    CreateDirectoryW(tree.c_str(), NULL);
    CreateDirectoryW((tree + L"\\sub").c_str(), NULL);
    t::WriteFileText(tree + L"\\sub\\plik.txt", "x");
    if (!t::MakeJunction(tree + L"\\sub\\link", target)) SKIP("nie udało się utworzyć junction");
    RemoveDirectoryTree(tree);
    CHECK(t::Exists(target + L"\\zostaje.txt"));
    CHECK(!t::Exists(tree));
    // katalog będący dowiązaniem: znika samo dowiązanie
    std::wstring link = root + L"\\link2";
    CHECK(t::MakeJunction(link, target));
    RemoveDirectoryTree(link);
    CHECK(!t::Exists(link));
    CHECK(t::Exists(target + L"\\zostaje.txt"));
    RemoveDirectoryTree(root);
}

// S1 + migracja 1.0.0 -> 1.0.1: zdjęcie Modify dla wszystkich, usunięcie starego pkg\ i dowiązań
TEST(secure_data_dir_migrates_1_0_0) {
    if (!IsAdmin()) SKIP("wymaga administratora");
    std::wstring root = t::TempDir(L"acl");
    std::wstring pd = root + L"\\cp-signbridge", logs = pd + L"\\logs", target = root + L"\\cel";
    CreateDirectoryW(target.c_str(), NULL);
    t::WriteFileText(target + L"\\systemowy.dll", "x");
    CreateDirectoryW(pd.c_str(), NULL);
    CreateDirectoryW(logs.c_str(), NULL);
    CreateDirectoryW((pd + L"\\pkg").c_str(), NULL);
    CreateDirectoryW((pd + L"\\pkg\\x64").c_str(), NULL);
    t::WriteFileText(pd + L"\\pkg\\x64\\cpsignbridge.dll", "podstawiona");
    t::WriteFileText(pd + L"\\install.log", "stary");
    t::WriteFileText(logs + L"\\ksp-optima-1234.log", "stary log");
    Grant100(pd);
    CHECK(ReadAcl(logs + L"\\ksp-optima-1234.log").broadMask & FILE_WRITE_DATA);   // stan wyjściowy jak w 1.0.0
    bool j1 = t::MakeJunction(logs + L"\\evil", target);
    bool j2 = t::MakeJunction(pd + L"\\evil2", target);
    CHECK(j1 && j2);

    std::vector<std::wstring> log;
    CHECK(SecureDataDir(pd, log));
    for (auto& l : log) std::wprintf(L"    %s\n", l.c_str());

    CHECK(t::Exists(target + L"\\systemowy.dll"));
    CHECK(!t::Exists(pd + L"\\pkg"));
    CHECK(!t::Exists(logs + L"\\evil"));
    CHECK(!t::Exists(pd + L"\\evil2"));
    CHECK(t::Exists(logs + L"\\ksp-optima-1234.log"));

    AclInfo a = ReadAcl(pd);
    CHECK(a.ok && a.isProtected);
    CHECK((a.broadMask & kWriteBits) == 0);
    AclInfo l = ReadAcl(logs);
    CHECK(l.ok && l.isProtected);
    CHECK((l.broadMask & ~(FILE_GENERIC_READ | FILE_GENERIC_EXECUTE | FILE_ADD_FILE)) == 0);   // bez podkatalogów i usuwania
    CHECK(!l.broadInherits);                                                                    // pliki: tylko właściciel
    AclInfo f = ReadAcl(logs + L"\\ksp-optima-1234.log");
    CHECK(f.ok && f.broadMask == 0);
    AclInfo i = ReadAcl(pd + L"\\install.log");
    CHECK(i.ok && (i.broadMask & kWriteBits) == 0);

    // drugi raz (ponowna instalacja) — bez błędów
    log.clear();
    CHECK(SecureDataDir(pd, log));
    RemoveDirectoryTree(root);
}

// S1: pakiet tylko dla administratorów; dowiązanie w miejscu katalogu pakietu nie prowadzi poza niego
TEST(prepare_package_dir_admin_only) {
    if (!IsAdmin()) SKIP("wymaga administratora");
    std::wstring root = t::TempDir(L"pkg");
    std::wstring pf = root + L"\\cp-signbridge", pkg = pf + L"\\pkg", target = root + L"\\cel";
    CreateDirectoryW(pf.c_str(), NULL);
    CreateDirectoryW(target.c_str(), NULL);
    t::WriteFileText(target + L"\\zostaje.txt", "x");
    CHECK(t::MakeJunction(pkg, target));
    std::wstring err;
    CHECK(PreparePackageDir(pkg, err));
    DWORD a = GetFileAttributesW(pkg.c_str());
    CHECK(a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY) && !(a & FILE_ATTRIBUTE_REPARSE_POINT));
    CHECK(t::Exists(target + L"\\zostaje.txt"));
    AclInfo p = ReadAcl(pkg);
    CHECK(p.ok && p.isProtected && p.broadMask == 0);
    CHECK(PreparePackageDir(pkg, err));   // ponownie, katalog już jest
    RemoveDirectoryTree(root);
}
