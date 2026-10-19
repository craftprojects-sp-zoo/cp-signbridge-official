// SignBridge — © 2026 Craftprojects Sp. z o.o. (craftprojects.pl)
// KSP: brak klucza programowego w wydaniu, typ klucza, limit żądań, bezpieczny log
#include "check.h"
#include "backend.h"
#include "log.h"
#include "slots.h"

// S5: w buildzie bez SB_ENABLE_SOFTKEY nie da się utworzyć klucza ani otworzyć klucza o nazwie innej niż odcisk
TEST(ksp_no_softkey_in_release) {
    sb::IBackend* b = sb::CreateCompositeBackend();
    sb::IKey* k = nullptr;
    CHECK(b->Create(L"sonda", BCRYPT_RSA_ALGORITHM, &k) == NTE_NOT_SUPPORTED);
    CHECK(k == nullptr);
    CHECK(b->Open(L"sonda", &k) != ERROR_SUCCESS);
    CHECK(b->KeyImplType(L"sonda") == NCRYPT_IMPL_SOFTWARE_FLAG);
    CHECK((b->KeyImplType(L"00112233445566778899AABBCCDDEEFF00112233") & NCRYPT_IMPL_HARDWARE_FLAG) != 0);
    delete b;
}

TEST(plugin_parallel_request_limit) {
    sb::Slots s(4);
    for (int i = 0; i < 4; ++i) CHECK(s.TryAcquire());
    CHECK(!s.TryAcquire());
    CHECK(s.Used() == 4);
    s.Release();
    CHECK(s.TryAcquire());
}

// S7: log nie pisze do cudzego / współdzielonego pliku, ma limit rozmiaru i usuwa stare własne pliki
TEST(log_safety_rotation_and_prune) {
    std::wstring dir = t::TempDir(L"logs");
    std::wstring path = dir + L"\\t-sbtests-" + std::to_wstring(GetCurrentProcessId()) + L".log";
    std::wstring victim = dir + L"\\ofiara.txt", old = dir + L"\\t-sbtests-1.log";
    t::WriteFileText(victim, "");
    CHECK(CreateHardLinkW(path.c_str(), victim.c_str(), NULL));   // dowiązanie twarde w miejscu przyszłego logu
    t::WriteFileText(old, "stary");
    HANDLE h = CreateFileW(old.c_str(), FILE_WRITE_ATTRIBUTES, 0, NULL, OPEN_EXISTING, 0, NULL);
    FILETIME ft; GetSystemTimeAsFileTime(&ft);
    ULARGE_INTEGER u = { ft.dwLowDateTime, ft.dwHighDateTime };
    u.QuadPart -= 40ull * 24 * 3600 * 10000000ull;
    ft.dwLowDateTime = u.LowPart; ft.dwHighDateTime = u.HighPart;
    SetFileTime(h, NULL, NULL, &ft);
    CloseHandle(h);

    sb::LogInitAt(dir, L"t");
    CHECK(sb::LogPath() == path);
    CHECK(!t::Exists(old));   // starszy niż kLogKeepDays
    SB_INFO(L"pierwsza linia");
    WIN32_FILE_ATTRIBUTE_DATA fa = {};
    GetFileAttributesExW(victim.c_str(), GetFileExInfoStandard, &fa);
    CHECK(fa.nFileSizeLow == 0);   // zapis nie poszedł przez dowiązanie

    DeleteFileW(path.c_str());
    std::wstring big(3000, L'x');
    for (int i = 0; i < 2000; ++i) SB_INFO(L"%s", big.c_str());
    GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &fa);
    ULARGE_INTEGER sz = { fa.nFileSizeLow, fa.nFileSizeHigh };
    CHECK(sz.QuadPart > 0 && sz.QuadPart <= sb::kMaxLogBytes);
    CHECK(t::Exists(path + L".old"));
    HANDLE f = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    CHECK(f != INVALID_HANDLE_VALUE && sb::FileOwnedByCurrentUser(f));
    if (f != INVALID_HANDLE_VALUE) CloseHandle(f);
}
