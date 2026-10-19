// SignBridge — © 2026 Craftprojects Sp. z o.o. (craftprojects.pl)
// cp-signbridge — minimalne testy jednostkowe (bez zewnętrznych bibliotek)
#pragma once
#include <windows.h>
#include <cstdio>
#include <string>
#include <vector>

namespace t {
struct Case { const char* name; void (*fn)(); };
std::vector<Case>& Registry();
extern int g_failures;
struct Reg { Reg(const char* n, void (*f)()) { Registry().push_back({ n, f }); } };

// Pusty katalog tymczasowy dla testu
std::wstring TempDir(const wchar_t* name);
bool Exists(const std::wstring& p);
void WriteFileText(const std::wstring& p, const char* text);
// Junction (mklink /J) — tworzenie nie wymaga administratora
bool MakeJunction(const std::wstring& link, const std::wstring& target);
bool NoSkip();
}  // namespace t

#define TEST(name) static void name(); static t::Reg reg_##name(#name, name); static void name()
#define CHECK(cond) do { if (!(cond)) { std::printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); ++t::g_failures; } } while (0)
// SB_TESTS_NO_SKIP=1 (CI): pominięcie testu liczy się jako błąd
#define SKIP(msg) do { std::printf("  SKIP: %s\n", msg); if (t::NoSkip()) ++t::g_failures; return; } while (0)
