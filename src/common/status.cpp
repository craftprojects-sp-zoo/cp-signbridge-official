// SignBridge — © 2026 Craftprojects Sp. z o.o. (craftprojects.pl)
// cp-signbridge — opis kodu błędu (implementacja)
#include "status.h"
#include <stdio.h>

namespace sb {

std::wstring ErrorText(LONG s) {
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

}  // namespace sb
