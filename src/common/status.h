// SignBridge — © 2026 Craftprojects Sp. z o.o. (craftprojects.pl)
// cp-signbridge — opis kodu błędu (SECURITY_STATUS / Win32 / NTSTATUS) do logów i komunikatów
#pragma once
#include <windows.h>
#include <string>

namespace sb {

// "0x80090016 (Zestaw kluczy nie istnieje)" albo samo "0x...", gdy system nie zna opisu
std::wstring ErrorText(LONG status);

}  // namespace sb
