// SignBridge — © 2026 Craftprojects Sp. z o.o. (craftprojects.pl)
// SignBridge — jedna aplikacja: kreator instalacji + panel + ikona przy zegarze
// (plik dystrybucyjny "SignBridge Installer.exe", po instalacji "SignBridge.exe")
//
// Kreator (3 kroki): rola komputera → co zostanie zrobione → instalacja (postęp) → gotowe.
// Panel: stan (karta / wtyczka / dostawca / certyfikaty), lista certyfikatów, zdarzenia, akcje.
// Ikona przy zegarze: stan karty, przełączanie roli, pobranie certyfikatów, panel.
// Nowa wersja: zainstalowany panel raz na 24 h sprawdza GitHub Releases i pokazuje „Dostępna wersja X.Y.Z”
//   z przyciskiem „Aktualizuj” (stały adres strony wydań) — niczego nie pobiera ani nie instaluje (updatecheck.h).
//
// Role są niezależne — na jednym komputerze można zainstalować obie; zakładki w panelu i menu ikony
// przełączają widok w obie strony.
// Rola KOMPUTER Z KARTĄ: wtyczka pulpitu zdalnego (HKCU, bez administratora), kopia programu
//   w %LOCALAPPDATA%\cp-signbridge, autostart ikony, skrót, wpis w „Aplikacje i funkcje” (HKCU).
// Rola SERWER: (UAC) dostawca kluczy do System32/SysWOW64, program w Program Files, autostart
//   panelu dla każdego użytkownika (HKLM Run), wpis w „Aplikacje i funkcje” (HKLM). Panel w sesji
//   RDP sam pobiera certyfikaty z komputera użytkownika (logowanie, ponowne połączenie, co 10 min).
//
//   <exe>                      okno (kreator, gdy nie zainstalowano; inaczej panel)
//   <exe> --tray               start schowany do ikony (autostart)
//   <exe> --uninstall          odinstalowanie (z „Aplikacje i funkcje”)
//   <exe> --install-all [--shortcut] | --uninstall-all   [--msi] [--quiet]   — pełna instalacja (obie części), używana przez kreator i MSI
//   <exe> --install-server|--uninstall-server|--install-workstation|--uninstall-workstation [--machine] [--msi] [--no-shortcut] [--quiet]  (części osobno)
//         tryb bezokienkowy (kreator z podniesieniem uprawnień, pakiety MSI, GPO / skrypty) — patrz SilentServer/SilentWorkstation
//   <exe> --cleanup-installer "<ścieżka .exe>" --parent-pid N   (dodawane przez instalator przy uruchomieniu zainstalowanej
//         kopii) czeka na zakończenie procesu instalatora i usuwa jego plik — patrz CleanupInstaller
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <shellapi.h>
#include <shlobj.h>
#include <wtsapi32.h>
#include <versionhelpers.h>
#include <string>
#include <vector>
#include <map>
#include "resource.h"
#include "../common/pluginsetup.h"
#include "../common/kspsetup.h"
#include "../common/syncops.h"
#include "../common/signer.h"
#include "../common/dvc.h"
#include "../common/certs.h"
#include "../common/updatecheck.h"
#include "../ksp/log.h"

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "wtsapi32.lib")
#pragma comment(lib, "msimg32.lib")
#pragma comment(linker, "\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

#define WIDEN2(x) L##x
#define WIDEN(x) WIDEN2(x)

namespace {

// ---------------------------------------------------------------------------
// Stałe, kolory, stan
// ---------------------------------------------------------------------------

const wchar_t* const kClass = L"CraftprojectsSignBridgeWnd";
const wchar_t* const kStatusClass = L"CraftprojectsSignBridgeStatus";
const wchar_t* const kTitle = WIDEN(SB_APP_NAME);
const wchar_t* const kVersion = WIDEN(SB_VERSION_STR);
const wchar_t* const kCompany = WIDEN(SB_COMPANY);
const wchar_t* const kWebsite = WIDEN(SB_WEBSITE);
const wchar_t* const kAutostartName = L"cp-signbridge-panel";
const wchar_t* const kCfgKey = L"Software\\cp-signbridge";
const UINT WM_TRAY = WM_APP + 1;
const UINT WM_WORK_DONE = WM_APP + 2;
const UINT WM_UPDATE_DONE = WM_APP + 3;
const UINT_PTR TIMER_TAIL = 1;
const UINT_PTR TIMER_AUTOSYNC = 2;
const UINT_PTR TIMER_UPDATE = 3;

const COLORREF C_HEADER = RGB(31, 47, 66);
const COLORREF C_HEADER2 = RGB(42, 70, 105);
const COLORREF C_PRIMARY = RGB(58, 110, 173);
const COLORREF C_PRIMARY_DK = RGB(44, 88, 142);
const COLORREF C_TEXT = RGB(33, 37, 41);
const COLORREF C_MUTED = RGB(108, 117, 125);
const COLORREF C_BORDER = RGB(222, 226, 230);
const COLORREF C_BG = RGB(255, 255, 255);
const COLORREF C_PANEL = RGB(247, 249, 251);
const COLORREF C_SEL = RGB(232, 240, 250);
const COLORREF C_OK = RGB(46, 160, 67);
const COLORREF C_WARN = RGB(232, 168, 32);
const COLORREF C_ERR = RGB(220, 53, 69);
const COLORREF C_INFO = RGB(120, 130, 145);

enum Ctl { ID_FIRST = 1000,
           // kreator
           ID_CARD_WS, ID_CARD_SRV, ID_BTN_NEXT, ID_BTN_BACK, ID_BTN_CANCEL, ID_SUMMARY, ID_CHK_SHORTCUT, ID_BTN_INSTALL,
           ID_RAD_WS, ID_RAD_SRV, ID_ROLE_LBL2, ID_BTN_HELP, ID_HELP_TEXT, ID_BTN_HELP_BACK, ID_BTN_HELP_LOGS,
           ID_BTN_SETTINGS, ID_CHK_TABS, ID_RAD_WS2, ID_RAD_SRV2, ID_BTN_SET_BACK, ID_VIEW_LBL,
           ID_BTN_MANUAL, ID_MANUAL_TEXT, ID_BTN_MANUAL_BACK,
           ID_PROGRESS, ID_DONE_TEXT, ID_BTN_OPEN_PANEL, ID_BTN_CLOSE, ID_STEP, ID_CHK_DELSELF,
           // panel
           ID_STATUS, ID_LIST, ID_LOG, ID_LOGLBL, ID_LINK, ID_FOOTER_ICON, ID_TAB_WS, ID_TAB_SRV, ID_ROLE_HINT,
           ID_BTN_SYNC, ID_BTN_TEST, ID_BTN_UNSYNC, ID_BTN_REFRESH, ID_BTN_LOGS, ID_BTN_SETUP, ID_BTN_UPDATE, ID_BTN_UNINSTALL, ID_CHK_AUTOSTART,
           ID_UPD_LBL, ID_BTN_UPD_OPEN, ID_BTN_UPD_LATER,
           // ikona
           ID_TRAY_SHOW = 2001, ID_TRAY_REFRESH, ID_TRAY_SYNC, ID_TRAY_ROLE_WS, ID_TRAY_ROLE_SRV, ID_TRAY_INFO, ID_TRAY_CARD, ID_TRAY_HELP, ID_TRAY_MANUAL, ID_TRAY_EXIT, ID_TRAY_UPDATE };

enum Role { ROLE_WORKSTATION, ROLE_SERVER };
enum Page { PG_ROLE, PG_SUMMARY, PG_PROGRESS, PG_DONE, PG_PANEL, PG_HELP, PG_SETTINGS, PG_MANUAL, PG_COUNT };
enum JobId { JOB_SYNC_AUTO = 1, JOB_SYNC, JOB_TEST, JOB_UNSYNC, JOB_INSTALL, JOB_UNINSTALL };
enum LineKind { LK_OK, LK_WARN, LK_ERR, LK_INFO, LK_NEXT };
struct StatusLine { int kind; std::wstring text; };
struct WorkResult { int job; std::vector<std::wstring> lines; bool ok; bool refresh; bool quiet; };

HINSTANCE g_hinst;
HWND g_wnd;
HWND g_cardWs, g_cardSrv, g_btnNext, g_btnBack, g_btnCancel, g_summary, g_chkShortcut, g_btnInstall, g_progress, g_doneTitle, g_doneText, g_btnOpenPanel, g_btnClose, g_step;
HWND g_status, g_list, g_log, g_logLbl, g_link, g_footerIcon, g_tabWs, g_tabSrv;
HWND g_radWs, g_radSrv, g_btnHelp, g_helpText;
HWND g_btnSettings, g_chkTabs, g_radWs2, g_radSrv2, g_viewLbl, g_chkDelSelf, g_btnManual, g_manualText;
bool g_showTabs = false;   // przełącznik widoku (dwie zakładki) widoczny w panelu — Ustawienia
HWND g_btnSync, g_btnTest, g_btnUnsync, g_btnRefresh, g_btnLogs, g_btnSetup, g_btnUpdate, g_btnUninstall, g_chkAutostart;
HWND g_updLbl, g_btnUpdOpen, g_btnUpdLater;   // „Dostępna wersja X.Y.Z” (CP-SB-19)
std::vector<HWND> g_pageCtls[PG_COUNT];
HFONT g_font, g_fontBold, g_fontH1, g_fontH2, g_fontSmall, g_fontBig;
HICON g_iconApp, g_iconCompany;
NOTIFYICONDATAW g_nid = {};
HANDLE g_mutex = NULL;
double g_scale = 1.0;
const int LW = 860, LH = 680;   // rozmiar logiczny (96 DPI)
int W = 860, H = 640;           // rozmiar w pikselach
const int HEADER_H = 84;

Role g_role = ROLE_WORKSTATION;
Role g_pendingRole = ROLE_WORKSTATION;
Page g_page = PG_PANEL;
bool g_ksp = false;
bool g_busy = false;
bool g_remote = false;
bool g_wsInstalled = false;
bool g_installing = false;        // kreator: trwa instalacja/odinstalowanie
int g_installJob = 0;
std::wstring g_subtitle;
std::vector<StatusLine> g_statusLines;
std::map<std::wstring, ULONGLONG> g_logSizes;
std::wstring g_lastAgentInfo;
std::wstring g_lastSyncSet;
std::wstring g_cardSummary;       // dla menu ikony: "Jan Kowalski — Sigillum" / ""
int g_syncedCount = 0;
int g_syncFailures = 0;
bool g_syncFailBalloonShown = false;
bool g_everSynced = false;
bool g_updChecking = false;       // trwa zapytanie o nową wersję
std::wstring g_updAvailable;      // nowsza wersja z ostatniego sprawdzenia (menu ikony); "" = brak
std::wstring g_updBanner;         // to samo, o ile nie odłożono („Przypomnij później”) — pasek w panelu

// ---------------------------------------------------------------------------
// Pomocnicze
// ---------------------------------------------------------------------------

int S(int v) { return (int)(v * g_scale + 0.5); }
std::wstring LogsDir() { return sb::kspsetup::ProgramDataDir() + L"\\logs"; }

std::wstring ReadCfgFrom(HKEY root, const wchar_t* name) {
    HKEY h; wchar_t buf[256] = {}; DWORD cb = sizeof(buf);
    if (RegOpenKeyExW(root, kCfgKey, 0, KEY_READ | KEY_WOW64_64KEY, &h) != ERROR_SUCCESS) return L"";
    LONG r = RegQueryValueExW(h, name, NULL, NULL, (BYTE*)buf, &cb);
    RegCloseKey(h);
    return r == ERROR_SUCCESS ? buf : L"";
}
std::wstring ReadCfg(const wchar_t* name) {
    HKEY h; wchar_t buf[256] = {}; DWORD cb = sizeof(buf);
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kCfgKey, 0, KEY_READ, &h) != ERROR_SUCCESS) return L"";
    LONG r = RegQueryValueExW(h, name, NULL, NULL, (BYTE*)buf, &cb);
    RegCloseKey(h);
    return r == ERROR_SUCCESS ? buf : L"";
}
void WriteCfg(const wchar_t* name, const std::wstring& v) {
    HKEY h;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, kCfgKey, 0, NULL, 0, KEY_WRITE, NULL, &h, NULL) != ERROR_SUCCESS) return;
    RegSetValueExW(h, name, 0, REG_SZ, (const BYTE*)v.c_str(), (DWORD)((v.size() + 1) * sizeof(wchar_t)));
    RegCloseKey(h);
}

std::wstring Utf8ToW(const std::string& s) {
    int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), NULL, 0);
    std::wstring w(n, L'\0');
    if (n) MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), &w[0], n);
    return w;
}

void AppendLog(const std::wstring& line) {
    int len = GetWindowTextLengthW(g_log);
    if (len > 200000) { SetWindowTextW(g_log, L""); len = 0; }
    SendMessageW(g_log, EM_SETSEL, len, len);
    std::wstring s = line;
    if (s.empty() || s.back() != L'\n') s += L"\r\n";
    SendMessageW(g_log, EM_REPLACESEL, FALSE, (LPARAM)s.c_str());
}
std::wstring Now() {
    SYSTEMTIME st; GetLocalTime(&st);
    wchar_t b[32]; swprintf_s(b, L"%02u.%02u %02u:%02u:%02u", st.wDay, st.wMonth, st.wHour, st.wMinute, st.wSecond);
    return b;
}
void Note(const std::wstring& line) { AppendLog(Now() + L"  " + line); }

void SetTrayIcon(int iconId, const std::wstring& tip) {
    g_nid.hIcon = LoadIconW(g_hinst, MAKEINTRESOURCEW(iconId));
    wcsncpy_s(g_nid.szTip, (std::wstring(kTitle) + L"\n" + tip).c_str(), _TRUNCATE);
    g_nid.uFlags = NIF_ICON | NIF_TIP | NIF_MESSAGE;
    Shell_NotifyIconW(NIM_MODIFY, &g_nid);
}
void Balloon(const std::wstring& title, const std::wstring& text, bool warning) {
    NOTIFYICONDATAW n = g_nid;
    n.uFlags = NIF_INFO;
    n.dwInfoFlags = warning ? NIIF_WARNING : NIIF_INFO;
    wcsncpy_s(n.szInfoTitle, title.c_str(), _TRUNCATE);
    wcsncpy_s(n.szInfo, text.c_str(), _TRUNCATE);
    Shell_NotifyIconW(NIM_MODIFY, &n);
}
void ShowWindowFront() { ShowWindow(g_wnd, SW_SHOW); ShowWindow(g_wnd, SW_RESTORE); SetForegroundWindow(g_wnd); }
bool RunningFromProgramFiles() { return _wcsicmp(sb::pluginsetup::ExeDir().c_str(), sb::kspsetup::ProgramDir().c_str()) == 0; }

// Wbudowane biblioteki -> katalog `dir` (x64\, x86\) + kopia programu pod nazwą docelową
bool ExtractRes(int id, const std::wstring& path, std::wstring& err) {
    HRSRC r = FindResourceW(g_hinst, MAKEINTRESOURCEW(id), RT_RCDATA);
    HGLOBAL g = r ? LoadResource(g_hinst, r) : NULL;
    const void* data = g ? LockResource(g) : NULL;
    DWORD sz = r ? SizeofResource(g_hinst, r) : 0;
    if (!data || !sz) { err = L"Brak wbudowanego zasobu " + std::to_wstring(id); return false; }
    HANDLE f = CreateFileW(path.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (f == INVALID_HANDLE_VALUE) {
        std::wstring old = path + L".old"; DeleteFileW(old.c_str());
        if (MoveFileExW(path.c_str(), old.c_str(), MOVEFILE_REPLACE_EXISTING)) MoveFileExW(old.c_str(), NULL, MOVEFILE_DELAY_UNTIL_REBOOT);
        f = CreateFileW(path.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if (f == INVALID_HANDLE_VALUE) { err = L"Nie można zapisać " + path + L" (" + std::to_wstring(GetLastError()) + L")"; return false; }
    }
    DWORD wr = 0; BOOL ok = WriteFile(f, data, sz, &wr, NULL);
    CloseHandle(f);
    if (!ok || wr != sz) { err = L"Błąd zapisu " + path; return false; }
    return true;
}
bool CopyExeTo(const std::wstring& dst) {
    std::wstring me = sb::pluginsetup::ExePath();
    if (_wcsicmp(me.c_str(), dst.c_str()) == 0) return true;
    if (CopyFileW(me.c_str(), dst.c_str(), FALSE)) return true;
    std::wstring old = dst + L".old"; DeleteFileW(old.c_str());
    if (MoveFileExW(dst.c_str(), old.c_str(), MOVEFILE_REPLACE_EXISTING) && CopyFileW(me.c_str(), dst.c_str(), FALSE)) { MoveFileExW(old.c_str(), NULL, MOVEFILE_DELAY_UNTIL_REBOOT); return true; }
    return false;
}
bool ExtractPackage(const std::wstring& dir, std::wstring& err) {
    CreateDirectoryW(dir.c_str(), NULL);
    CreateDirectoryW((dir + L"\\x64").c_str(), NULL);
    CreateDirectoryW((dir + L"\\x86").c_str(), NULL);
    if (!ExtractRes(IDR_KSP_X64, dir + L"\\x64\\cpsignbridge.dll", err)) return false;
    if (!ExtractRes(IDR_KSP_X86, dir + L"\\x86\\cpsignbridge.dll", err)) return false;
    if (!ExtractRes(IDR_RDP_X64, dir + L"\\x64\\cpsignbridge-rdp.dll", err)) return false;
    if (!ExtractRes(IDR_RDP_X86, dir + L"\\x86\\cpsignbridge-rdp.dll", err)) return false;
    if (!CopyExeTo(dir + L"\\" + sb::kspsetup::kAppExe)) { err = L"Nie można skopiować programu do " + dir; return false; }
    return true;
}

// ---------------------------------------------------------------------------
// Rysowanie (nagłówek, przyciski, karty, linie stanu)
// ---------------------------------------------------------------------------

void FillRoundRect(HDC dc, RECT r, COLORREF fill, COLORREF border, int radius) {
    HBRUSH b = CreateSolidBrush(fill); HPEN p = CreatePen(PS_SOLID, 1, border);
    HGDIOBJ ob = SelectObject(dc, b), op = SelectObject(dc, p);
    RoundRect(dc, r.left, r.top, r.right, r.bottom, radius, radius);
    SelectObject(dc, ob); SelectObject(dc, op); DeleteObject(b); DeleteObject(p);
}
void Dot(HDC dc, int cx, int cy, int rad, COLORREF c) {
    HBRUSH b = CreateSolidBrush(c); HPEN p = CreatePen(PS_SOLID, 1, c);
    HGDIOBJ ob = SelectObject(dc, b), op = SelectObject(dc, p);
    Ellipse(dc, cx - rad, cy - rad, cx + rad, cy + rad);
    SelectObject(dc, ob); SelectObject(dc, op); DeleteObject(b); DeleteObject(p);
}
void Text(HDC dc, const std::wstring& s, RECT r, HFONT f, COLORREF c, UINT fmt = DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS) {
    HGDIOBJ of = SelectObject(dc, f);
    SetTextColor(dc, c); SetBkMode(dc, TRANSPARENT);
    DrawTextW(dc, s.c_str(), -1, &r, fmt | DT_NOPREFIX);
    SelectObject(dc, of);
}

void PaintHeader(HDC dc) {
    RECT r = { 0, 0, W, S(HEADER_H) };
    // gradient poziomy
#pragma warning(push)
#pragma warning(disable: 4310)
    auto c16 = [](unsigned v) { return (COLOR16)((v << 8) & 0xFFFFu); };
    TRIVERTEX v[2] = { { 0, 0, c16(GetRValue(C_HEADER)), c16(GetGValue(C_HEADER)), c16(GetBValue(C_HEADER)), 0 },
                       { W, S(HEADER_H), c16(GetRValue(C_HEADER2)), c16(GetGValue(C_HEADER2)), c16(GetBValue(C_HEADER2)), 0 } };
#pragma warning(pop)
    GRADIENT_RECT gr = { 0, 1 };
    GradientFill(dc, v, 2, &gr, 1, GRADIENT_FILL_RECT_H);
    DrawIconEx(dc, S(22), S(18), g_iconApp, S(48), S(48), 0, NULL, DI_NORMAL);
    RECT t = { S(84), S(16), W - S(160), S(46) };
    Text(dc, kTitle, t, g_fontH1, RGB(255, 255, 255));
    RECT st = { S(84), S(46), W - S(160), S(70) };
    Text(dc, g_subtitle, st, g_font, RGB(200, 212, 228));
    RECT ver = { W - S(150), S(20), W - S(22), S(40) };
    Text(dc, std::wstring(L"wersja ") + kVersion, ver, g_fontSmall, RGB(170, 186, 206), DT_RIGHT | DT_SINGLELINE | DT_VCENTER);
    RECT co = { W - S(170), S(40), W - S(22), S(58) };
    Text(dc, kCompany, co, g_fontSmall, RGB(140, 158, 182), DT_RIGHT | DT_SINGLELINE | DT_VCENTER);
}

// Przyciski: BS_OWNERDRAW. Styl z GWLP_USERDATA: 0 = zwykły, 1 = główny (niebieski), 2 = karta roli
void DrawButton(DRAWITEMSTRUCT* d) {
    HDC dc = d->hDC; RECT r = d->rcItem;
    int style = (int)GetWindowLongPtrW(d->hwndItem, GWLP_USERDATA);
    bool pressed = (d->itemState & ODS_SELECTED) != 0, disabled = (d->itemState & ODS_DISABLED) != 0;
    wchar_t txt[512]; GetWindowTextW(d->hwndItem, txt, 512);
    // tło spod spodu (rodzic jest biały)
    HBRUSH bg = CreateSolidBrush(C_BG); FillRect(dc, &r, bg); DeleteObject(bg);
    if (style == 2) {
        bool sel = (d->hwndItem == g_cardWs) == (g_pendingRole == ROLE_WORKSTATION);
        FillRoundRect(dc, r, sel ? C_SEL : C_BG, sel ? C_PRIMARY : C_BORDER, S(10));
        // ikona-kropka + tytuł + opis (tytuł|opis w tekście przycisku)
        std::wstring s = txt; size_t bar = s.find(L'|');
        std::wstring title = s.substr(0, bar), desc = bar == std::wstring::npos ? L"" : s.substr(bar + 1);
        int cx = r.left + S(30), cy = r.top + S(34);
        Dot(dc, cx, cy, S(14), sel ? C_PRIMARY : C_BORDER);
        Dot(dc, cx, cy, S(6), C_BG);
        RECT tr = { r.left + S(58), r.top + S(16), r.right - S(16), r.top + S(42) };
        Text(dc, title, tr, g_fontH2, C_TEXT);
        RECT dr = { r.left + S(58), r.top + S(44), r.right - S(16), r.bottom - S(12) };
        Text(dc, desc, dr, g_font, C_MUTED, DT_LEFT | DT_WORDBREAK | DT_TOP);
        return;
    }
    if (style == 4) {
        HBRUSH hb = CreateSolidBrush(C_HEADER2); FillRect(dc, &r, hb); DeleteObject(hb);
        FillRoundRect(dc, r, pressed ? RGB(58, 92, 134) : C_HEADER2, RGB(200, 212, 228), S(8));
        Text(dc, txt, r, g_fontBold, RGB(255, 255, 255), DT_CENTER | DT_SINGLELINE | DT_VCENTER);
        return;
    }
    if (style == 3) {
        bool sel = (d->hwndItem == g_tabWs) == (g_role == ROLE_WORKSTATION);
        FillRoundRect(dc, r, sel ? C_PRIMARY : C_BG, sel ? C_PRIMARY : C_BORDER, S(18));
        Text(dc, txt, r, sel ? g_fontBold : g_font, sel ? RGB(255, 255, 255) : C_MUTED, DT_CENTER | DT_SINGLELINE | DT_VCENTER);
        return;
    }
    COLORREF fill = style == 1 ? (pressed ? C_PRIMARY_DK : C_PRIMARY) : (pressed ? C_SEL : C_BG);
    COLORREF border = style == 1 ? fill : (disabled ? C_BORDER : RGB(176, 196, 222));
    COLORREF text = style == 1 ? RGB(255, 255, 255) : (disabled ? C_MUTED : C_PRIMARY_DK);
    if (disabled && style == 1) { fill = RGB(170, 190, 215); border = fill; }
    FillRoundRect(dc, r, fill, border, S(8));
    Text(dc, txt, r, style == 1 ? g_fontBold : g_font, text, DT_CENTER | DT_SINGLELINE | DT_VCENTER);
    if (d->itemState & ODS_FOCUS) { RECT f = r; InflateRect(&f, -S(4), -S(4)); DrawFocusRect(dc, &f); }
}

// Linie stanu: własna klasa okna, rysuje kolorową kropkę + tekst
LRESULT CALLBACK StatusProc(HWND h, UINT m, WPARAM wp, LPARAM lp) {
    if (m == WM_PAINT) {
        PAINTSTRUCT ps; HDC dc = BeginPaint(h, &ps);
        RECT rc; GetClientRect(h, &rc);
        HBRUSH bg = CreateSolidBrush(C_PANEL); FillRect(dc, &rc, bg); DeleteObject(bg);
        FillRoundRect(dc, rc, C_PANEL, C_BORDER, S(10));
        int y = S(12), lh = S(24);
        for (auto& l : g_statusLines) {
            COLORREF c = l.kind == LK_OK ? C_OK : l.kind == LK_WARN ? C_WARN : l.kind == LK_ERR ? C_ERR : l.kind == LK_NEXT ? C_PRIMARY : C_INFO;
            if (l.kind == LK_NEXT) { RECT a = { S(10), y, S(30), y + lh }; Text(dc, L"→", a, g_fontBold, c, DT_CENTER | DT_SINGLELINE | DT_VCENTER); }
            else Dot(dc, S(20), y + lh / 2, S(5), c);
            RECT tr = { S(36), y, rc.right - S(12), y + lh };
            Text(dc, l.text, tr, l.kind == LK_NEXT ? g_fontBold : g_font, l.kind == LK_NEXT ? C_PRIMARY : C_TEXT);
            y += lh;
        }
        EndPaint(h, &ps);
        return 0;
    }
    if (m == WM_ERASEBKGND) return 1;
    return DefWindowProcW(h, m, wp, lp);
}

// ---------------------------------------------------------------------------
// Tworzenie kontrolek i stron
// ---------------------------------------------------------------------------

HWND Make(Page pg, const wchar_t* cls, const wchar_t* text, int id, int x, int y, int w, int h, DWORD style, DWORD ex = 0, HFONT font = NULL) {
    HWND c = CreateWindowExW(ex, cls, text, WS_CHILD | style, S(x), S(y), S(w), S(h), g_wnd, (HMENU)(INT_PTR)id, g_hinst, NULL);
    SendMessageW(c, WM_SETFONT, (WPARAM)(font ? font : g_font), TRUE);
    g_pageCtls[pg].push_back(c);
    return c;
}
HWND Btn(Page pg, const wchar_t* text, int id, int x, int y, int w, int styleKind, int h = 36) {
    HWND b = Make(pg, L"BUTTON", text, id, x, y, w, h, WS_TABSTOP | BS_OWNERDRAW);
    SetWindowLongPtrW(b, GWLP_USERDATA, styleKind);
    return b;
}
HWND Label(Page pg, const wchar_t* text, int id, int x, int y, int w, int h, HFONT f = NULL, DWORD extra = 0) {
    return Make(pg, L"STATIC", text, id, x, y, w, h, extra, 0, f);
}

void ShowPage(Page p) {
    g_page = p;
    for (int i = 0; i < PG_COUNT; ++i) for (HWND h : g_pageCtls[i]) ShowWindow(h, i == p ? SW_SHOW : SW_HIDE);
    // log współdzielony: postęp instalacji i panel
    if (p == PG_PROGRESS) { SetWindowPos(g_log, NULL, S(40), S(HEADER_H + 110), S(LW - 80), S(300), SWP_NOZORDER); ShowWindow(g_log, SW_SHOW); }
    else if (p == PG_PANEL) { SetWindowPos(g_log, NULL, S(24), S(HEADER_H + 426), S(LW - 48), S(118), SWP_NOZORDER); ShowWindow(g_log, SW_SHOW); }
    else ShowWindow(g_log, SW_HIDE);
    InvalidateRect(g_wnd, NULL, TRUE);
}

void BuildUi() {
    const int cw = LW;
    // --- strona: rola ---
    Label(PG_ROLE, L"Krok 1 z 3 — gdzie uruchamiasz ten program?", ID_STEP, 40, HEADER_H + 24, cw - 80, 30, g_fontH2);
    Label(PG_ROLE, L"Wybierz rolę tego komputera. Można ją później zmienić w menu ikony przy zegarze.", 0, 40, HEADER_H + 56, cw - 80, 22, g_font);
    int cardW = (cw - 80 - 20) / 2;
    g_cardWs = Btn(PG_ROLE, L"Komputer z kartą / tokenem|Komputer pracownika, do którego podpięty jest czytnik lub karta wirtualna (Sigillum, mSzafir, CloudSigner). Zainstaluje się wtyczka pulpitu zdalnego i ikona przy zegarze.", ID_CARD_WS, 40, HEADER_H + 92, cardW, 2, 150);
    g_cardSrv = Btn(PG_ROLE, L"Serwer pulpitu zdalnego|Serwer, na którym pracuje Optima, Symfonia lub Acrobat. Zainstaluje się dostawca podpisu dla wszystkich użytkowników (wymaga uprawnień administratora).", ID_CARD_SRV, 40 + cardW + 20, HEADER_H + 92, cardW, 2, 150);
    g_btnCancel = Btn(PG_ROLE, L"Anuluj", ID_BTN_CANCEL, cw - 40 - 120 - 12 - 120, HEADER_H + 262, 120, 0);
    g_btnNext = Btn(PG_ROLE, L"Dalej", ID_BTN_NEXT, cw - 40 - 120, HEADER_H + 262, 120, 1);

    // --- strona: podsumowanie ---
    Label(PG_SUMMARY, L"Krok 1 z 2 — co zostanie zainstalowane", ID_STEP + 100, 40, HEADER_H + 24, cw - 80, 30, g_fontH2);
    g_summary = Label(PG_SUMMARY, L"", ID_SUMMARY, 40, HEADER_H + 60, cw - 80, 216, g_font);
    Label(PG_SUMMARY, L"Ten komputer to:", ID_ROLE_LBL2, 40, HEADER_H + 282, 120, 24, g_fontBold);
    g_radWs = Make(PG_SUMMARY, L"BUTTON", L"komputer z kartą / tokenem (pracownik)", ID_RAD_WS, 160, HEADER_H + 282, 290, 24, BS_AUTORADIOBUTTON | WS_GROUP | WS_TABSTOP);
    g_radSrv = Make(PG_SUMMARY, L"BUTTON", L"serwer pulpitu zdalnego (Optima / Acrobat)", ID_RAD_SRV, 460, HEADER_H + 282, 320, 24, BS_AUTORADIOBUTTON | WS_TABSTOP);
    g_chkShortcut = Make(PG_SUMMARY, L"BUTTON", L"Utwórz skrót na pulpicie (wszyscy użytkownicy)", ID_CHK_SHORTCUT, 40, HEADER_H + 312, 360, 24, BS_AUTOCHECKBOX | WS_TABSTOP);
    SendMessageW(g_chkShortcut, BM_SETCHECK, BST_CHECKED, 0);
    g_btnBack = Btn(PG_SUMMARY, L"Anuluj", ID_BTN_BACK, cw - 40 - 140 - 12 - 120, HEADER_H + 350, 120, 0);
    g_btnInstall = Btn(PG_SUMMARY, L"Zainstaluj", ID_BTN_INSTALL, cw - 40 - 140, HEADER_H + 350, 140, 1);

    // --- strona: postęp ---
    Label(PG_PROGRESS, L"Krok 2 z 2 — instalacja", ID_STEP + 200, 40, HEADER_H + 24, cw - 80, 30, g_fontH2);
    Label(PG_PROGRESS, L"Trwa instalacja. Jeśli pojawi się okno kontroli konta użytkownika, potwierdź je.", 0, 40, HEADER_H + 56, cw - 80, 22, g_font);
    g_progress = Make(PG_PROGRESS, PROGRESS_CLASSW, L"", ID_PROGRESS, 40, HEADER_H + 84, cw - 80, 10, PBS_MARQUEE | PBS_SMOOTH);

    // --- strona: gotowe ---
    g_doneTitle = Label(PG_DONE, L"Gotowe", ID_STEP + 300, 40, HEADER_H + 24, cw - 80, 36, g_fontBig);
    g_doneText = Label(PG_DONE, L"", ID_DONE_TEXT, 40, HEADER_H + 70, cw - 80, 190, g_font);
    g_chkDelSelf = Make(PG_DONE, L"BUTTON", L"Usuń plik instalatora po zamknięciu (program jest już w Program Files)", ID_CHK_DELSELF, 40, HEADER_H + 286, cw - 80, 24, BS_AUTOCHECKBOX | WS_TABSTOP);
    SendMessageW(g_chkDelSelf, BM_SETCHECK, BST_CHECKED, 0);
    g_btnClose = Btn(PG_DONE, L"Zamknij", ID_BTN_CLOSE, cw - 40 - 150 - 12 - 120, HEADER_H + 320, 120, 0);
    g_btnOpenPanel = Btn(PG_DONE, L"Otwórz panel", ID_BTN_OPEN_PANEL, cw - 40 - 150, HEADER_H + 320, 150, 1);

    // --- strona: pomoc „Program nie działa?” ---
    Label(PG_HELP, L"Program nie działa? Zrób po kolei:", ID_STEP + 500, 40, HEADER_H + 24, cw - 80, 36, g_fontBig);
    g_helpText = Label(PG_HELP, L"", ID_HELP_TEXT, 40, HEADER_H + 70, cw - 80, 400, g_font);
    SetWindowTextW(g_helpText,
        L"1.  Wyloguj się z pulpitu zdalnego (Start → ikona użytkownika → Wyloguj). Samo zamknięcie okna lub „Rozłącz” niczego nie naprawi — sesja musi zostać zakończona.\r\n\r\n"
        L"2.  Wyjmij kartę z czytnika (lub odłącz token) i włóż ją ponownie.\r\n\r\n"
        L"3.  Sprawdź na swoim komputerze, czy karta została wykryta — pojawi się powiadomienie albo zobaczysz ją w aplikacji karty (Sigillum, mSzafir, CloudSigner).\r\n\r\n"
        L"4.  Otwórz SignBridge (ikona przy zegarze) i sprawdź, czy karta jest na liście. Jeśli nie — jeszcze raz wyjmij i włóż kartę. Jeśli nadal jej nie ma, zgłoś się do administratora.\r\n\r\n"
        L"5.  Gdy karta jest widoczna, połącz się ponownie przez Podłączanie pulpitu zdalnego. W ustawieniach połączenia (Pokaż opcje → Zasoby lokalne) upewnij się, że zaznaczone są karty inteligentne i schowek.\r\n\r\n"
        L"6.  Po zalogowaniu karta powinna być widoczna w SignBridge na serwerze — pojawi się dymek „Gotowe do podpisu”. Możesz podpisywać w programie.");
    Btn(PG_HELP, L"Folder logów (dla administratora)", ID_BTN_HELP_LOGS, 40, HEADER_H + 480, 260, 0);
    Btn(PG_HELP, L"Wstecz", ID_BTN_HELP_BACK, cw - 40 - 120, HEADER_H + 480, 120, 1);

    // --- strona: ustawienia ---
    Label(PG_SETTINGS, L"Ustawienia", ID_STEP + 600, 40, HEADER_H + 24, cw - 80, 36, g_fontBig);
    Label(PG_SETTINGS, L"Ten komputer to:", 0, 40, HEADER_H + 80, 200, 24, g_fontBold);
    g_radWs2 = Make(PG_SETTINGS, L"BUTTON", L"komputer z kartą / tokenem (pracownik)", ID_RAD_WS2, 60, HEADER_H + 108, 400, 24, BS_AUTORADIOBUTTON | WS_GROUP | WS_TABSTOP);
    g_radSrv2 = Make(PG_SETTINGS, L"BUTTON", L"serwer pulpitu zdalnego (Optima / Acrobat)", ID_RAD_SRV2, 60, HEADER_H + 136, 400, 24, BS_AUTORADIOBUTTON | WS_TABSTOP);
    Label(PG_SETTINGS, L"Decyduje, co pokazuje panel i ikona przy zegarze. Zapisywane dla Twojego konta.", 0, 60, HEADER_H + 164, cw - 100, 22, g_fontSmall);
    g_chkTabs = Make(PG_SETTINGS, L"BUTTON", L"Pokazuj w panelu przełącznik widoku (dwa przyciski: komputer z kartą / serwer)", ID_CHK_TABS, 40, HEADER_H + 210, cw - 80, 24, BS_AUTOCHECKBOX | WS_TABSTOP);
    Label(PG_SETTINGS, L"Domyślnie ukryty — zwykły użytkownik nie musi go widzieć.", 0, 60, HEADER_H + 236, cw - 100, 22, g_fontSmall);
    Btn(PG_SETTINGS, L"Wstecz", ID_BTN_SET_BACK, cw - 40 - 120, HEADER_H + 300, 120, 1);

    // --- strona: instrukcja obsługi ---
    Label(PG_MANUAL, L"Instrukcja obsługi", ID_STEP + 700, 40, HEADER_H + 24, cw - 80, 36, g_fontBig);
    g_manualText = Make(PG_MANUAL, L"EDIT", L"", ID_MANUAL_TEXT, 40, HEADER_H + 68, cw - 80, 400, WS_VSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL, 0, g_font);
    SetWindowTextW(g_manualText,
        L"CO TO JEST\r\n"
        L"SignBridge pozwala podpisywać dokumenty podpisem kwalifikowanym (karta Sigillum, mSzafir, CloudSigner, token) w programach uruchomionych na serwerze pulpitu zdalnego — Optima, Symfonia, Acrobat — tak, jakby karta była włożona do serwera. Karta zostaje w Twoim komputerze, PIN wpisujesz zawsze u siebie.\r\n\r\n"
        L"JAK TO DZIAŁA\r\n"
        L"1.  Na Twoim komputerze wtyczka SignBridge dołącza do połączenia pulpitu zdalnego własny kanał (wewnątrz tego samego, szyfrowanego połączenia — żadnych dodatkowych portów ani adresów).\r\n"
        L"2.  Po zalogowaniu na serwer panel SignBridge sam pobiera Twoje certyfikaty i udostępnia je programom. Pojawia się dymek „Gotowe do podpisu”.\r\n"
        L"3.  Gdy program na serwerze podpisuje dokument, prośba o podpis wraca kanałem na Twój komputer, karta podpisuje lokalnie (okno PIN u Ciebie), a gotowy podpis wraca do programu.\r\n\r\n"
        L"CODZIENNA PRACA (komputer z kartą)\r\n"
        L"•  Włóż kartę, sprawdź ikonę SignBridge przy zegarze: zielona = karta widoczna, żółta = brak karty, szara = program nie zainstalowany.\r\n"
        L"•  Połącz się z serwerem przez Podłączanie pulpitu zdalnego (mstsc). W opcjach połączenia (Zasoby lokalne) muszą być zaznaczone karty inteligentne.\r\n"
        L"•  Podpisuj w programie na serwerze jak zwykle. Okno PIN pojawi się na Twoim komputerze — po wpisaniu podpis trafia do dokumentu na serwerze. Dymek przy zegarze potwierdza „Podpis wykonany”.\r\n\r\n"
        L"PANEL NA SERWERZE\r\n"
        L"•  Pokazuje, czy Twoje certyfikaty są pobrane. „Pobierz certyfikaty teraz” wymusza odświeżenie (np. po zmianie karty), „Test podpisu” sprawdza całą drogę (PIN u Ciebie), „Usuń pobrane” czyści certyfikaty z Twojego profilu na serwerze.\r\n"
        L"•  „Zdarzenia” to dziennik podpisów z tej sesji.\r\n\r\n"
        L"GDY COŚ NIE DZIAŁA\r\n"
        L"•  Kliknij „PROGRAM NIE DZIAŁA?” w nagłówku — instrukcja krok po kroku. Najczęstsza przyczyna: połączenie z serwerem nawiązane przed włożeniem karty albo z programu innego niż mstsc (Mac, Royal TSX, Windows App).\r\n"
        L"•  Dla administratora: logi w C:\\ProgramData\\cp-signbridge\\logs (przycisk „Folder logów”).\r\n\r\n"
        L"USTAWIENIA\r\n"
        L"•  „Ten komputer to…” — wybór widoku: komputer z kartą albo serwer. „Pokazuj przełącznik widoku” wyświetla w panelu dwa przyciski do szybkiej zmiany.\r\n"
        L"•  Nowa wersja: raz na dobę program sprawdza na GitHubie (zwykłe zapytanie HTTPS: adres IP, wersja programu), czy jest nowsze wydanie, i pokazuje „Dostępna wersja …”. „Aktualizuj” otwiera stronę wydań w przeglądarce — program sam niczego nie pobiera ani nie instaluje. Administrator może to wyłączyć zasadą grupy (DisableUpdateCheck).\r\n"
        L"•  Poza tym program nie łączy się z internetem. Odinstalowanie: „Aplikacje i funkcje” lub przycisk „Odinstaluj” w panelu — nie zostawia śladów.\r\n\r\n"
        L"SignBridge — Craftprojects Sp. z o.o. · craftprojects.pl");
    Btn(PG_MANUAL, L"Wstecz", ID_BTN_MANUAL_BACK, cw - 40 - 120, HEADER_H + 480, 120, 1);

    // --- strona: panel ---
    g_tabWs = Btn(PG_PANEL, L"Komputer z kartą / tokenem", ID_TAB_WS, 24, HEADER_H + 14, 250, 3, 34);
    g_tabSrv = Btn(PG_PANEL, L"Serwer pulpitu zdalnego", ID_TAB_SRV, 24 + 250 + 8, HEADER_H + 14, 250, 3, 34);
    g_status = Make(PG_PANEL, kStatusClass, L"", ID_STATUS, 24, HEADER_H + 58, cw - 48, 124, 0);
    int y = HEADER_H + 194, x = 24;
    g_btnSync = Btn(PG_PANEL, L"Pobierz certyfikaty teraz", ID_BTN_SYNC, x, y, 220, 1); x += 228;
    g_btnTest = Btn(PG_PANEL, L"Test podpisu", ID_BTN_TEST, x, y, 130, 0); x += 138;
    g_btnUnsync = Btn(PG_PANEL, L"Usuń pobrane", ID_BTN_UNSYNC, x, y, 130, 0); x += 138;
    g_btnRefresh = Btn(PG_PANEL, L"Odśwież", ID_BTN_REFRESH, x, y, 110, 0); x += 118;
    g_btnLogs = Btn(PG_PANEL, L"Folder logów", ID_BTN_LOGS, x, y, 130, 0);
    g_btnUpdate = Btn(PG_PANEL, L"Aktualizuj", ID_BTN_UPDATE, cw - 24 - 120 - 8 - 190, y + 40, 190, 1, 30);
    g_btnSetup = Btn(PG_PANEL, L"Zainstaluj", ID_BTN_SETUP, cw - 24 - 120 - 8 - 190, y + 40, 190, 1, 30);
    g_chkAutostart = Make(PG_PANEL, L"BUTTON", L"Pokazuj ikonę przy zegarze po zalogowaniu", ID_CHK_AUTOSTART, 24, y + 44, 360, 22, BS_AUTOCHECKBOX | WS_TABSTOP);
    g_btnUninstall = Btn(PG_PANEL, L"Odinstaluj", ID_BTN_UNINSTALL, cw - 24 - 120, y + 40, 120, 0, 30);
    g_btnSettings = Btn(PG_PANEL, L"Ustawienia", ID_BTN_SETTINGS, 24, y + 40, 130, 0, 30);
    g_btnManual = Btn(PG_PANEL, L"Instrukcja obsługi", ID_BTN_MANUAL, 24 + 138, y + 40, 160, 0, 30);
    g_btnHelp = Btn(PG_PANEL, L"PROGRAM NIE DZIAŁA?", ID_BTN_HELP, cw - 170 - 230, 26, 220, 4, 34);
    g_viewLbl = Label(PG_PANEL, L"", ID_VIEW_LBL, 24, HEADER_H + 22, cw - 48, 22, g_font);
    y += 74;
    g_list = Make(PG_PANEL, WC_LISTVIEWW, L"", ID_LIST, 24, y, cw - 48, 128, WS_BORDER | LVS_REPORT | LVS_SINGLESEL | LVS_NOSORTHEADER);
    ListView_SetExtendedListViewStyle(g_list, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);
    const wchar_t* cols[] = { L"Certyfikat", L"Wystawca", L"Ważny do", L"Skąd" };
    int widths[] = { 290, 230, 100, 180 };
    for (int i = 0; i < 4; ++i) { LVCOLUMNW c = {}; c.mask = LVCF_TEXT | LVCF_WIDTH; c.pszText = (LPWSTR)cols[i]; c.cx = S(widths[i]); ListView_InsertColumn(g_list, i, &c); }
    y += 134;
    g_logLbl = Label(PG_PANEL, L"Zdarzenia", ID_LOGLBL, 24, y, 200, 18, g_fontSmall);
    // nowa wersja: dyskretnie, w wierszu nad zdarzeniami, po prawej (widoczne tylko, gdy jest nowsza wersja)
    g_btnUpdLater = Btn(PG_PANEL, L"Przypomnij później", ID_BTN_UPD_LATER, cw - 24 - 150, y - 4, 150, 0, 24);
    g_btnUpdOpen = Btn(PG_PANEL, L"Aktualizuj", ID_BTN_UPD_OPEN, cw - 24 - 150 - 8 - 110, y - 4, 110, 1, 24);
    g_updLbl = Label(PG_PANEL, L"", ID_UPD_LBL, 240, y - 4, cw - 24 - 150 - 8 - 110 - 12 - 240, 24, g_fontBold, SS_RIGHT | SS_CENTERIMAGE);
    // g_log tworzony osobno (współdzielony)
    g_log = CreateWindowExW(0, L"EDIT", L"", WS_CHILD | WS_VSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL, 0, 0, 10, 10, g_wnd, (HMENU)ID_LOG, g_hinst, NULL);
    SendMessageW(g_log, WM_SETFONT, (WPARAM)g_fontSmall, TRUE);
    // stopka
    int fy = LH - 36;
    g_footerIcon = Make(PG_PANEL, L"STATIC", L"", ID_FOOTER_ICON, 24, fy, 22, 22, SS_ICON | SS_REALSIZECONTROL);
    SendMessageW(g_footerIcon, STM_SETICON, (WPARAM)g_iconCompany, 0);
    std::wstring foot = std::wstring(kTitle) + L" " + kVersion + L"  ·  " + kCompany + L"  ·  <a href=\"" + kWebsite + L"\">" + std::wstring(kWebsite).substr(8) + L"</a>";
    g_link = Make(PG_PANEL, WC_LINK, foot.c_str(), ID_LINK, 54, fy + 3, cw - 78, 20, 0, 0, g_fontSmall);
}

// ---------------------------------------------------------------------------
// Stan
// ---------------------------------------------------------------------------

void ListClear() { ListView_DeleteAllItems(g_list); }
void ListAdd(const std::wstring& a, const std::wstring& b, const std::wstring& c, const std::wstring& d) {
    LVITEMW it = {}; it.mask = LVIF_TEXT; it.iItem = ListView_GetItemCount(g_list); it.pszText = (LPWSTR)a.c_str();
    int i = ListView_InsertItem(g_list, &it);
    ListView_SetItemText(g_list, i, 1, (LPWSTR)b.c_str());
    ListView_SetItemText(g_list, i, 2, (LPWSTR)c.c_str());
    ListView_SetItemText(g_list, i, 3, (LPWSTR)d.c_str());
}
void ListSetLastColumn(const wchar_t* name) { LVCOLUMNW c = {}; c.mask = LVCF_TEXT; c.pszText = (LPWSTR)name; ListView_SetColumn(g_list, 3, &c); }

bool Installed();
bool InstalledForRole();
bool IsInstallerFile();

void PanelButtons() {
    bool ws = g_role == ROLE_WORKSTATION;
    for (HWND h : { g_btnSync, g_btnTest, g_btnUnsync }) ShowWindow(h, (g_page == PG_PANEL && !ws && g_ksp) ? SW_SHOW : SW_HIDE);
    ShowWindow(g_chkAutostart, SW_HIDE);   // autostart dla wszystkich (HKLM) ustawia instalacja
    bool tabs = g_page == PG_PANEL && g_showTabs;
    ShowWindow(g_tabWs, tabs ? SW_SHOW : SW_HIDE); ShowWindow(g_tabSrv, tabs ? SW_SHOW : SW_HIDE);
    ShowWindow(g_viewLbl, (g_page == PG_PANEL && !g_showTabs) ? SW_SHOW : SW_HIDE);
    SetWindowTextW(g_viewLbl, ws ? L"Widok: komputer z kartą / tokenem   (zmiana: Ustawienia)" : L"Widok: serwer pulpitu zdalnego   (zmiana: Ustawienia)");
    bool installed = InstalledForRole();
    ShowWindow(g_btnUninstall, (g_page == PG_PANEL && (installed || (ws ? g_wsInstalled : g_ksp))) ? SW_SHOW : SW_HIDE);
    std::wstring inst = sb::kspsetup::InstalledPanelVersion();
    bool upd = !ws && g_ksp && !inst.empty() && inst != kVersion && !RunningFromProgramFiles();
    ShowWindow(g_btnUpdate, (g_page == PG_PANEL && upd) ? SW_SHOW : SW_HIDE);
    ShowWindow(g_btnSetup, (g_page == PG_PANEL && !upd) ? SW_SHOW : SW_HIDE);
    SetWindowTextW(g_btnSetup, installed ? L"Zainstaluj ponownie" : L"Zainstaluj");
    SetWindowLongPtrW(g_btnSetup, GWLP_USERDATA, installed ? 0 : 1);
    InvalidateRect(g_btnSetup, NULL, TRUE); InvalidateRect(g_tabWs, NULL, TRUE); InvalidateRect(g_tabSrv, NULL, TRUE);
    bool newer = g_page == PG_PANEL && !g_updBanner.empty();
    for (HWND h : { g_updLbl, g_btnUpdOpen, g_btnUpdLater }) ShowWindow(h, newer ? SW_SHOW : SW_HIDE);
    // Odśwież / logi przesuwamy w lewo, gdy nie ma przycisków serwera
    int x = ws || !g_ksp ? 24 : 24 + 228 + 138 + 138;
    SetWindowPos(g_btnRefresh, NULL, S(x), S(HEADER_H + 194), 0, 0, SWP_NOZORDER | SWP_NOSIZE);
    SetWindowPos(g_btnLogs, NULL, S(x + 118), S(HEADER_H + 194), 0, 0, SWP_NOZORDER | SWP_NOSIZE);
}

void RefreshWorkstation() {
    auto st = sb::pluginsetup::GetStatus();
    sb::proto::Certs certsList;
    sb::signer::CollectCerts(certsList, false);
    g_wsInstalled = st.registered && st.fileExists;
    bool autostart = sb::pluginsetup::IsAutostart(kAutostartName) || Installed();   // HKLM Run po pełnej instalacji

    g_statusLines.clear();
    if (g_wsInstalled) g_statusLines.push_back({ LK_OK, L"Wtyczka pulpitu zdalnego: zainstalowana (wersja " + st.version + L")" });
    else g_statusLines.push_back({ LK_ERR, L"Wtyczka pulpitu zdalnego: nie zainstalowana — kliknij „Zainstaluj”" });
    g_statusLines.push_back({ autostart ? LK_OK : LK_INFO, autostart ? L"Ikona przy zegarze po zalogowaniu: tak" : L"Ikona przy zegarze po zalogowaniu: nie" });
    g_cardSummary.clear();
    ListSetLastColumn(L"Dostawca (karta)");
    ListClear();
    for (auto& e : certsList.items) {
        PCCERT_CONTEXT c = CertCreateCertificateContext(X509_ASN_ENCODING, e.der.data(), (DWORD)e.der.size());
        if (!c) continue;
        std::wstring prov; sb::certs::HasPrivateKey(c, &prov, NULL);
        wchar_t iss[256] = {}; CertGetNameStringW(c, CERT_NAME_SIMPLE_DISPLAY_TYPE, CERT_NAME_ISSUER_FLAG, NULL, iss, 256);
        ListAdd(sb::certs::SubjectName(c), iss, sb::syncops::CertDate(c->pCertInfo->NotAfter), prov);
        if (g_cardSummary.empty()) g_cardSummary = sb::certs::SubjectName(c) + L" — " + iss;
        CertFreeCertificateContext(c);
    }
    if (certsList.items.empty()) g_statusLines.push_back({ LK_WARN, L"Karta / token: nie wykryto certyfikatu z kluczem — włóż kartę lub uruchom aplikację karty" });
    else g_statusLines.push_back({ LK_OK, L"Karta / token: " + std::to_wstring(certsList.items.size()) + L" certyfikat(y) — " + g_cardSummary });
    if (g_wsInstalled && !certsList.items.empty())
        g_statusLines.push_back({ LK_NEXT, sb::pluginsetup::IsMstscRunning() ? L"Jeśli wtyczkę zainstalowano przed chwilą: zamknij okna pulpitu zdalnego i połącz się ponownie"
                                                                             : L"Gotowe — połącz się z serwerem przez Podłączanie pulpitu zdalnego i podpisuj w programie" });
    g_subtitle = L"Komputer z kartą / tokenem";
    SetTrayIcon(!g_wsInstalled ? IDI_OFF : certsList.items.empty() ? IDI_WARN : IDI_OK,
                !g_wsInstalled ? std::wstring(L"nie zainstalowano") : certsList.items.empty() ? std::wstring(L"karta: nie wykryto") : L"karta: " + g_cardSummary);
    SendMessageW(g_chkAutostart, BM_SETCHECK, autostart ? BST_CHECKED : BST_UNCHECKED, 0);
}

void RefreshServer() {
    g_ksp = sb::kspsetup::IsKspRegistered();
    g_remote = sb::dvc::InRemoteSession();
    std::vector<sb::syncops::CertRow> rows;
    if (g_ksp) sb::syncops::ListSyncedCerts(rows);
    g_syncedCount = (int)rows.size();
    std::wstring inst = sb::kspsetup::InstalledPanelVersion();
    bool upd = g_ksp && !inst.empty() && inst != kVersion && !RunningFromProgramFiles();

    g_statusLines.clear();
    if (!g_ksp) g_statusLines.push_back({ LK_ERR, L"Dostawca podpisu na serwerze: nie zainstalowany — kliknij „Zainstaluj”" });
    else if (inst.empty()) g_statusLines.push_back({ LK_WARN, L"SignBridge na serwerze: dostawca z wcześniejszej wersji — kliknij „Zainstaluj”, żeby dokończyć instalację" });
    else g_statusLines.push_back({ LK_OK, L"SignBridge na serwerze: zainstalowany (wersja " + inst + L")" + (upd ? L" — dostępna aktualizacja do " + std::wstring(kVersion) : std::wstring()) });
    g_statusLines.push_back({ g_remote ? LK_OK : LK_INFO, g_remote ? L"Sesja pulpitu zdalnego: tak" : L"Sesja pulpitu zdalnego: nie (przy konsoli serwera nie ma skąd pobrać certyfikatów)" });
    if (g_ksp) {
        if (!rows.empty()) g_statusLines.push_back({ LK_OK, L"Certyfikaty z Twojego komputera: " + std::to_wstring(rows.size()) + L" — " + rows[0].subject });
        else if (g_remote) g_statusLines.push_back({ g_syncFailures ? LK_ERR : LK_INFO, g_syncFailures ? L"Certyfikaty z Twojego komputera: brak — nie udało się połączyć z wtyczką na Twoim komputerze" : L"Certyfikaty z Twojego komputera: pobieram…" });
        else g_statusLines.push_back({ LK_INFO, L"Certyfikaty z Twojego komputera: brak" });
        if (!rows.empty()) g_statusLines.push_back({ LK_NEXT, L"Możesz podpisywać w Optimie / Acrobacie — PIN wpiszesz na swoim komputerze" });
        else if (g_remote && g_syncFailures) g_statusLines.push_back({ LK_NEXT, L"Na swoim komputerze: zainstaluj SignBridge (rola „komputer z kartą”) i łącz się przez mstsc" });
    }
    ListSetLastColumn(L"Skąd");
    ListClear();
    for (auto& r : rows) ListAdd(r.subject, r.issuer, r.notAfter, r.source);
    g_subtitle = L"Serwer pulpitu zdalnego — Twoja sesja";
    SetTrayIcon(!g_ksp ? IDI_OFF : rows.empty() ? IDI_WARN : IDI_OK,
                !g_ksp ? std::wstring(L"nie zainstalowano") : rows.empty() ? std::wstring(L"brak certyfikatów z komputera") : L"gotowy do podpisu: " + rows[0].subject);
    EnableWindow(g_btnSync, !g_busy && g_remote);
    EnableWindow(g_btnTest, !g_busy && !rows.empty());
    EnableWindow(g_btnUnsync, !g_busy && !rows.empty());
}

void Refresh() {
    g_ksp = sb::kspsetup::IsKspRegistered();
    if (g_role == ROLE_SERVER) RefreshServer(); else RefreshWorkstation();
    PanelButtons();
    InvalidateRect(g_status, NULL, TRUE);
    InvalidateRect(g_wnd, NULL, FALSE);
}

void SetBusy(bool busy) {
    g_busy = busy;
    for (HWND b : { g_btnSync, g_btnTest, g_btnUnsync, g_btnRefresh, g_btnSetup, g_btnUpdate, g_btnUninstall, g_btnInstall, g_btnBack, g_btnNext })
        if (b) EnableWindow(b, !busy);
    SetCursor(LoadCursorW(NULL, busy ? IDC_WAIT : IDC_ARROW));
}

// ---------------------------------------------------------------------------
// Śledzenie logów (zdarzenia + powiadomienia o podpisach)
// ---------------------------------------------------------------------------

void TailLogs(bool initial) {
    std::wstring dir = LogsDir();
    std::wstring pattern = dir + (g_role == ROLE_SERVER ? L"\\ksp-*.log" : L"\\agent-*.log");
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW(pattern.c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        std::wstring path = dir + L"\\" + fd.cFileName;
        ULARGE_INTEGER sz = { fd.nFileSizeLow, fd.nFileSizeHigh };
        auto it = g_logSizes.find(path);
        ULONGLONG known = (it == g_logSizes.end()) ? (initial ? (sz.QuadPart > 4000 ? sz.QuadPart - 4000 : 0) : 0) : it->second;
        if (sz.QuadPart <= known) { g_logSizes[path] = sz.QuadPart; continue; }
        HANDLE f = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, 0, NULL);
        if (f == INVALID_HANDLE_VALUE) continue;
        // tylko logi bieżącego użytkownika (procesy innych użytkowników serwera — nie nasze dane)
        if (!sb::FileOwnedByCurrentUser(f)) { CloseHandle(f); g_logSizes[path] = sz.QuadPart; continue; }
        LARGE_INTEGER pos; pos.QuadPart = (LONGLONG)known;
        SetFilePointerEx(f, pos, NULL, FILE_BEGIN);
        std::string buf((size_t)(sz.QuadPart - known), '\0');
        DWORD rd = 0;
        ReadFile(f, &buf[0], (DWORD)buf.size(), &rd, NULL);
        CloseHandle(f);
        buf.resize(rd);
        g_logSizes[path] = sz.QuadPart;
        std::wstring text = Utf8ToW(buf);
        size_t start = 0;
        while (start < text.size()) {
            size_t nl = text.find(L'\n', start);
            std::wstring line = text.substr(start, nl == std::wstring::npos ? std::wstring::npos : nl - start);
            start = (nl == std::wstring::npos) ? text.size() : nl + 1;
            while (!line.empty() && (line.back() == L'\r')) line.pop_back();
            if (line.empty()) continue;
            // "RRRR-MM-DD GG:MM:SS.mmm | pid N | tid N | poziom | treść"
            size_t p = line.find(L" | ", 0); size_t cut = 0; int bars = 0;
            while (p != std::wstring::npos && bars < 4) { cut = p + 3; p = line.find(L" | ", cut); ++bars; }
            if (bars < 4 || line.size() < 19) continue;
            std::wstring body = line.substr(cut);
            // pomiń szum: TRACE oraz otwieranie/zamykanie dostawcy (to m.in. odpytywanie przez sam panel)
            if (line.find(L"| TRACE |") != std::wstring::npos) continue;
            if (body.rfind(L"OpenProvider(", 0) == 0 || body.rfind(L"FreeProvider(", 0) == 0 || body.rfind(L"GetKeyStorageInterface(", 0) == 0
                || body.rfind(L"GetProviderProperty(", 0) == 0 || body.rfind(L"IsAlgSupported(", 0) == 0 || body.rfind(L"EnumAlgorithms(", 0) == 0
                || body.rfind(L"FreeKey(", 0) == 0 || body.rfind(L"GetKeyProperty(", 0) == 0 || body.rfind(L"FreeBuffer(", 0) == 0) continue;
            std::wstring when = line.substr(8, 2) + L"." + line.substr(5, 2) + L" " + line.substr(11, 8);
            AppendLog(when + L"  " + body);
            if (!initial) {
                if (line.find(L"SIGN: OK") != std::wstring::npos || line.find(L"<<< SignHash: 0x00000000") != std::wstring::npos) Balloon(L"Podpis wykonany", body, false);
                else if (line.find(L"SIGN: BŁĄD") != std::wstring::npos || line.find(L"podpis nieudany") != std::wstring::npos) Balloon(L"Podpis nieudany", body, true);
                else if (line.find(L"czekam na podpis") != std::wstring::npos) Balloon(L"Żądanie podpisu", L"Serwer prosi o podpis — jeśli karta wymaga PIN, wpisz go w oknie, które się pojawi.", false);
            }
        }
    } while (FindNextFileW(h, &fd));
    FindClose(h);
}

// ---------------------------------------------------------------------------
// Operacje w tle
// ---------------------------------------------------------------------------

bool RunElevated(const std::wstring& args, std::vector<std::wstring>& lines) {
    bool uninst = args.find(L"--uninstall") != std::wstring::npos;
    wchar_t tmp[MAX_PATH]; GetTempPathW(MAX_PATH, tmp);
    std::wstring logPath = uninst ? std::wstring(tmp) + L"cp-signbridge-uninstall.log" : sb::kspsetup::ProgramDataDir() + L"\\install.log";
    DeleteFileW(logPath.c_str());
    SHELLEXECUTEINFOW sei = { sizeof(sei) };
    sei.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_FLAG_NO_UI;
    sei.lpVerb = L"runas";
    std::wstring exe = sb::pluginsetup::ExePath();
    sei.lpFile = exe.c_str();
    sei.lpParameters = args.c_str();
    sei.nShow = SW_HIDE;
    if (!ShellExecuteExW(&sei) || !sei.hProcess) {
        DWORD e = GetLastError();
        lines.push_back(e == ERROR_CANCELLED ? L"Anulowano — bez uprawnień administratora nie da się zainstalować na serwerze." : L"Nie udało się uruchomić z uprawnieniami administratora (" + std::to_wstring(e) + L").");
        return false;
    }
    WaitForSingleObject(sei.hProcess, 120000);
    DWORD code = 1; GetExitCodeProcess(sei.hProcess, &code);
    CloseHandle(sei.hProcess);
    HANDLE f = CreateFileW(logPath.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    if (f != INVALID_HANDLE_VALUE) {
        DWORD sz = GetFileSize(f, NULL), rd = 0;
        std::string buf(sz, '\0');
        if (sz) ReadFile(f, &buf[0], sz, &rd, NULL);
        CloseHandle(f);
        std::wstring t = Utf8ToW(buf);
        size_t s0 = 0;
        while (s0 < t.size()) { size_t nl = t.find(L'\n', s0); std::wstring l = t.substr(s0, nl == std::wstring::npos ? std::wstring::npos : nl - s0); s0 = nl == std::wstring::npos ? t.size() : nl + 1; while (!l.empty() && l.back() == L'\r') l.pop_back(); if (!l.empty()) lines.push_back(l); }
    }
    if (uninst) DeleteFileW(logPath.c_str());
    if (code != 0 && lines.empty()) lines.push_back(L"Instalacja zakończona błędem (kod " + std::to_wstring(code) + L").");
    return code == 0;
}

std::wstring JoinThumbs() {
    std::vector<sb::syncops::CertRow> rows; sb::syncops::ListSyncedCerts(rows);
    std::wstring s; for (auto& r : rows) s += r.thumbprint + L";";
    return s;
}

bool g_wantShortcut = true;

DWORD WINAPI Worker(LPVOID p) {
    int job = (int)(INT_PTR)p;
    WorkResult* r = new WorkResult{ job, {}, true, true, false };
    switch (job) {
    case JOB_SYNC_AUTO:
    case JOB_SYNC: {
        int a = 0, d = 0;
        SECURITY_STATUS s = sb::syncops::Sync(r->lines, a, d);
        r->ok = s == ERROR_SUCCESS;
        r->quiet = job == JOB_SYNC_AUTO;
        if (r->ok) {
            r->lines.push_back(L"Certyfikaty z komputera: " + std::to_wstring(a) + L" (usunięto nieaktualnych: " + std::to_wstring(d) + L")");
            if (!r->lines.empty() && r->lines[0].rfind(L"Połączono: ", 0) == 0) g_lastAgentInfo = r->lines[0].substr(11);
        }
        break;
    }
    case JOB_TEST:
        r->ok = sb::syncops::TestSign(L"", r->lines) == ERROR_SUCCESS;
        r->refresh = false;
        break;
    case JOB_UNSYNC:
        r->lines.push_back(L"Usunięto pobrane certyfikaty: " + std::to_wstring(sb::syncops::Unsync()));
        break;
    case JOB_INSTALL:
        r->lines.push_back(L"Czekam na potwierdzenie uprawnień administratora…");
        r->ok = RunElevated(std::wstring(L"--install-all --role ") + (g_role == ROLE_SERVER ? L"server" : L"workstation") + (g_wantShortcut ? L" --shortcut" : L""), r->lines);
        break;
    case JOB_UNINSTALL: {
        int n = sb::syncops::Unsync();
        if (n) r->lines.push_back(L"Usunięto pobrane certyfikaty z Twojego profilu: " + std::to_wstring(n));
        r->ok = RunElevated(L"--uninstall-all", r->lines);
        break;
    }
    }
    PostMessageW(g_wnd, WM_WORK_DONE, 0, (LPARAM)r);
    return 0;
}

void StartJob(int job) {
    if (g_busy) return;
    SetBusy(true);
    const wchar_t* what = job == JOB_SYNC ? L"Pobieram certyfikaty z Twojego komputera…" : job == JOB_SYNC_AUTO ? L"Automatyczne pobranie certyfikatów z Twojego komputera…"
                        : job == JOB_TEST ? L"Test podpisu (PIN na Twoim komputerze)…" : job == JOB_INSTALL ? L"Instalacja…"
                        : job == JOB_UNINSTALL ? L"Odinstalowanie…" : L"";
    if (*what) Note(what);
    HANDLE t = CreateThread(NULL, 0, Worker, (LPVOID)(INT_PTR)job, 0, NULL);
    if (t) CloseHandle(t); else SetBusy(false);
}

void ScheduleAutoSync(UINT delayMs) {
    KillTimer(g_wnd, TIMER_AUTOSYNC);
    if (g_ksp && sb::dvc::InRemoteSession())
        SetTimer(g_wnd, TIMER_AUTOSYNC, delayMs, NULL);
}

void StartInstall(int job) {
    g_installing = true; g_installJob = job;
    g_wantShortcut = SendMessageW(g_chkShortcut, BM_GETCHECK, 0, 0) == BST_CHECKED;
    if (job == JOB_INSTALL) { Role r = SendMessageW(g_radSrv, BM_GETCHECK, 0, 0) == BST_CHECKED ? ROLE_SERVER : ROLE_WORKSTATION; g_role = r; WriteCfg(L"Role", r == ROLE_SERVER ? L"server" : L"workstation"); }
    ShowPage(PG_PROGRESS);
    SendMessageW(g_progress, PBM_SETMARQUEE, TRUE, 30);
    StartJob(job);
}

void OnWorkDone(WorkResult* r) {
    for (auto& l : r->lines) Note((r->ok ? L"" : L"[!] ") + l);
    bool refresh = r->refresh, ok = r->ok; int job = r->job;
    std::wstring last = r->lines.empty() ? L"" : r->lines.back();
    delete r;
    SetBusy(false);

    if (job == JOB_SYNC || job == JOB_SYNC_AUTO) {
        if (ok) {
            g_syncFailures = 0; g_syncFailBalloonShown = false;
            std::wstring set = JoinThumbs();
            if (set != g_lastSyncSet || !g_everSynced) {
                std::vector<sb::syncops::CertRow> rows; sb::syncops::ListSyncedCerts(rows);
                if (!rows.empty()) Balloon(L"Gotowe do podpisu", rows[0].subject + (rows.size() > 1 ? L" (+" + std::to_wstring(rows.size() - 1) + L")" : std::wstring()) + L" — podpisuj w programie, PIN wpiszesz na swoim komputerze.", false);
            }
            g_lastSyncSet = set; g_everSynced = true;
            ScheduleAutoSync(10 * 60 * 1000);
        } else {
            ++g_syncFailures;
            if (job == JOB_SYNC || (!g_syncFailBalloonShown && g_syncFailures >= 2)) { Balloon(L"Nie można pobrać certyfikatów z Twojego komputera", last, true); g_syncFailBalloonShown = true; }
            ScheduleAutoSync(g_syncFailures < 3 ? 30 * 1000 : g_syncFailures < 6 ? 60 * 1000 : 5 * 60 * 1000);
        }
    } else if (!ok && !g_installing) {
        Balloon(kTitle, last, true);
    }

    if (g_installing) {
        g_installing = false;
        SendMessageW(g_progress, PBM_SETMARQUEE, FALSE, 0);
        bool uninstall = job == JOB_UNINSTALL;
        if (ok) {
            g_ksp = !uninstall; g_wsInstalled = !uninstall; g_syncFailures = 0;
            std::wstring t;
            if (uninstall) t = L"SignBridge został odinstalowany — bez śladów w rejestrze i na dysku (pliki w użyciu znikną po restarcie).";
            else t = L"Gotowe.\r\n\r\nNa komputerze z kartą: zamknij otwarte okna pulpitu zdalnego i połącz się z serwerem przez Podłączanie pulpitu zdalnego (mstsc) — okno PIN pojawi się tutaj.\r\n\r\nNa serwerze: gdy użytkownik loguje się z komputera z zainstalowanym SignBridge, jego certyfikaty pobierają się automatycznie, a programy (Optima, Symfonia, Acrobat) widzą je jak zwykłą kartę.\r\n\r\nIkona przy zegarze pojawia się po zalogowaniu każdego użytkownika.";
            SetWindowTextW(g_doneTitle, uninstall ? L"Odinstalowano" : L"Gotowe");
            SetWindowTextW(g_doneText, t.c_str());
            SetWindowTextW(g_btnOpenPanel, uninstall ? L"Zakończ program" : L"Otwórz panel");
            ShowPage(PG_DONE);
            ShowWindow(g_chkDelSelf, (!uninstall && IsInstallerFile()) ? SW_SHOW : SW_HIDE);
            if (uninstall && !IsWindowVisible(g_wnd)) { DestroyWindow(g_wnd); return; }
            if (!uninstall) Balloon(kTitle, L"Instalacja zakończona.", false);
            if (!uninstall) ScheduleAutoSync(2000);
        } else {
            Note(L"[!] Nie udało się. Popraw przyczynę i spróbuj ponownie (Wstecz).");
            SetWindowTextW(g_doneTitle, L"Nie udało się");
            SetWindowTextW(g_doneText, (L"Instalacja nie powiodła się:\r\n\r\n" + last + L"\r\n\r\nSzczegóły w zdarzeniach panelu i w folderze logów.").c_str());
            SetWindowTextW(g_btnOpenPanel, L"Spróbuj ponownie");
            ShowPage(PG_DONE);
            ShowWindow(g_chkDelSelf, SW_HIDE);
        }
        return;
    }
    if (refresh) Refresh();
}

// ---------------------------------------------------------------------------
// Nowa wersja (CP-SB-19): tylko informacja; „Aktualizuj” otwiera stały adres strony wydań
// ---------------------------------------------------------------------------

struct UpdateResult { bool ok; std::wstring latest; std::wstring etag; };

ULONGLONG ReadCfgNum(const wchar_t* name) { return _wcstoui64(ReadCfg(name).c_str(), NULL, 10); }

// Wątek roboczy: zapytanie (limit czasu kilka sekund na etap), każdy błąd tylko do logu
DWORD WINAPI UpdateWorker(LPVOID) {
    UpdateResult* r = new UpdateResult{ false, L"", L"" };
    sb::update::FetchResult f = sb::update::Fetch(std::wstring(kTitle) + L"/" + kVersion, ReadCfg(L"UpdateETag"));
    if (!f.ok) {
        if (f.httpStatus) SB_INFO(L"Nowa wersja: nie sprawdzono (HTTP %lu)", f.httpStatus);
        else SB_INFO(L"Nowa wersja: nie sprawdzono (brak połączenia z GitHub: %s)", sb::StatusText((LONG)f.error).c_str());
    } else if (f.notModified) {
        r->ok = true;
        SB_INFO(L"Nowa wersja: bez zmian od ostatniego sprawdzenia");
    } else {
        sb::update::ReleaseInfo ri;
        sb::update::Version cur, latest;
        sb::update::ParseVersion(std::wstring(kVersion), cur);
        if (!sb::update::ParseRelease(f.body.data(), f.body.size(), ri)) SB_WARN(L"Nowa wersja: niepoprawna odpowiedź serwera (%u B) — pominięto", (unsigned)f.body.size());
        else if (sb::update::Decide(ri, cur, latest) == sb::update::Verdict::Ignored) SB_INFO(L"Nowa wersja: wydanie pominięte (wersja robocza, przedpremierowe albo nieznany tag)");
        else {
            r->ok = true; r->latest = sb::update::ToDisplay(latest); r->etag = f.etag;
            SB_INFO(L"Nowa wersja: najnowsze wydanie %s, ten program %s", r->latest.c_str(), kVersion);
        }
    }
    if (!g_wnd || !PostMessageW(g_wnd, WM_UPDATE_DONE, 0, (LPARAM)r)) delete r;
    return 0;
}

// Pasek w panelu i pozycja w menu ikony — z zapamiętanego wyniku (bez sieci)
void ShowUpdateState() {
    g_updAvailable.clear(); g_updBanner.clear();
    if (!sb::update::DisabledByPolicy()) {
        sb::update::Version cur, shown;
        sb::update::ParseVersion(std::wstring(kVersion), cur);
        std::wstring latest = ReadCfg(L"UpdateLatest");
        ULONGLONG now = sb::update::NowSeconds();
        if (sb::update::ShouldShow(latest, cur, L"", 0, now, shown)) g_updAvailable = sb::update::ToDisplay(shown);
        if (sb::update::ShouldShow(latest, cur, ReadCfg(L"UpdateSnoozed"), ReadCfgNum(L"UpdateRemindAfter"), now, shown)) g_updBanner = sb::update::ToDisplay(shown);
    }
    SetWindowTextW(g_updLbl, (L"Dostępna wersja " + g_updBanner).c_str());
    PanelButtons();
}

// Przy starcie panelu i potem co godzinę: zapytanie tylko, gdy minęły 24 h od poprzedniego (także nieudanego)
void MaybeCheckForUpdate() {
    if (g_updChecking || sb::update::DisabledByPolicy()) return;
    ULONGLONG now = sb::update::NowSeconds();
    if (!sb::update::CheckDue(ReadCfgNum(L"UpdateLastCheck"), now)) return;
    WriteCfg(L"UpdateLastCheck", std::to_wstring(now));
    g_updChecking = true;
    HANDLE t = CreateThread(NULL, 0, UpdateWorker, NULL, 0, NULL);
    if (t) CloseHandle(t); else g_updChecking = false;
}

void OnUpdateDone(UpdateResult* r) {
    g_updChecking = false;
    if (r->ok && !r->latest.empty()) {
        WriteCfg(L"UpdateLatest", r->latest);
        WriteCfg(L"UpdateETag", r->etag);
    }
    delete r;
    ShowUpdateState();
}

void OpenReleasePage() {
    // zawsze stały adres (updatecheck.h) — nigdy adres z odpowiedzi serwera
    ShellExecuteW(NULL, L"open", sb::update::UpdateButtonUrl(), NULL, NULL, SW_SHOWNORMAL);
}

// ---------------------------------------------------------------------------
// Kreator
// ---------------------------------------------------------------------------

void SetRole(Role r, bool persist) {
    g_role = r;
    if (persist) WriteCfg(L"Role", r == ROLE_SERVER ? L"server" : L"workstation");
    g_logSizes.clear();
    SetWindowTextW(g_log, L"");
    Refresh();
    TailLogs(true);
    ScheduleAutoSync(3000);
}

void GoSummary();
void GoWizard() { g_subtitle = L"Instalacja"; GoSummary(); }

void GoSummary() {
    bool ws = false;
    std::wstring t;
    {
        std::wstring inst = sb::kspsetup::InstalledPanelVersion();
        t = L"Instalacja dla wszystkich użytkowników tego komputera (wymaga uprawnień administratora — pojawi się okno UAC).\r\n"
            L"Ten sam pakiet działa na serwerze pulpitu zdalnego i na komputerze z kartą — instalowane są obie części:\r\n\r\n"
            L"•  dostawca podpisu „SignBridge” dla programów na serwerze (System32 i SysWOW64)\r\n"
            L"•  wtyczka pulpitu zdalnego (mstsc) dla każdego użytkownika tego komputera\r\n"
            L"•  program w " + sb::kspsetup::ProgramDir() + L", ikona przy zegarze po zalogowaniu\r\n"
            L"•  logi w " + LogsDir() + L", wpis w „Aplikacje i funkcje”\r\n"
            L"•  rola wybrana poniżej (widok panelu) zapisywana dla całego komputera; później zmienisz ją zakładką w panelu\r\n\r\n" +
            (inst.empty() ? std::wstring(L"Nowa instalacja. Nic nie jest wysyłane poza istniejące połączenie pulpitu zdalnego; PIN karty wpisuje się zawsze na komputerze z kartą.")
                          : L"Zainstalowana wersja: " + inst + L" → " + kVersion + L" (aktualizacja w miejscu; pliki w użyciu zostaną podmienione, stare znikną po restarcie).");
    }
    if (false) {
        t = L"Rola: komputer z kartą / tokenem\r\n\r\n"
            L"•  wtyczka pulpitu zdalnego dla Twojego konta (bez uprawnień administratora)\r\n"
            L"•  program w folderze użytkownika: " + sb::pluginsetup::InstallDir() + L"\r\n"
            L"•  ikona przy zegarze uruchamiana po zalogowaniu\r\n"
            L"•  wpis w „Aplikacje i funkcje” (odinstalowanie jednym kliknięciem)\r\n\r\n"
            L"Nic nie jest wysyłane poza istniejące połączenie pulpitu zdalnego. PIN karty wpisujesz zawsze tutaj.";
    } else {
        std::wstring inst = sb::kspsetup::InstalledPanelVersion();
        t = L"Rola: serwer pulpitu zdalnego (wymaga uprawnień administratora — pojawi się okno UAC)\r\n\r\n"
            L"•  dostawca podpisu „SignBridge” dla wszystkich programów (System32 i SysWOW64)\r\n"
            L"•  program w " + sb::kspsetup::ProgramDir() + L"\r\n"
            L"•  panel uruchamiany przy logowaniu każdego użytkownika — sam pobiera jego certyfikaty\r\n"
            L"•  logi w " + LogsDir() + L"\r\n"
            L"•  wpis w „Aplikacje i funkcje”\r\n\r\n" +
            (inst.empty() ? std::wstring(L"Nowa instalacja.") : L"Zainstalowana wersja: " + inst + L" → " + kVersion + L" (aktualizacja w miejscu; pliki w użyciu zostaną podmienione, stare znikną po restarcie).");
    }
    SetWindowTextW(g_summary, t.c_str());
    SetWindowTextW(g_btnInstall, Installed() ? L"Aktualizuj" : L"Zainstaluj");
    SendMessageW(g_radWs, BM_SETCHECK, g_role == ROLE_WORKSTATION ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(g_radSrv, BM_SETCHECK, g_role == ROLE_SERVER ? BST_CHECKED : BST_UNCHECKED, 0);
    ShowPage(PG_SUMMARY);
    (void)ws;
}

// Pełna instalacja z tego programu: dostawca + wtyczka (HKLM) + program w Program Files
bool Installed() {
    auto st = sb::pluginsetup::GetStatus();
    return sb::kspsetup::IsKspRegistered() && !sb::kspsetup::InstalledPanelVersion().empty() && st.fileExists && st.machine;
}
bool InstalledForRole() { return Installed(); }

// Instalator (plik poza Program Files) po instalacji: uruchom zainstalowaną kopię i zakończ; opcjonalnie usuń własny plik
std::wstring InstalledExe() { return sb::kspsetup::ProgramDir() + L"\\" + sb::kspsetup::kAppExe; }
bool IsInstallerFile() { return !RunningFromProgramFiles(); }
void LaunchInstalledAndExit(bool openWindow, bool deleteSelf) {
    std::wstring exe = InstalledExe();
    if (GetFileAttributesW(exe.c_str()) != INVALID_FILE_ATTRIBUTES) {
        // zwolnij mutex i zamknij okno, żeby zainstalowana kopia mogła wystartować (i nie trafiła na nasze okno)
        Shell_NotifyIconW(NIM_DELETE, &g_nid);
        if (g_mutex) { ReleaseMutex(g_mutex); CloseHandle(g_mutex); g_mutex = NULL; }
        if (g_wnd) { DestroyWindow(g_wnd); g_wnd = NULL; }
        // plik instalatora usuwa zainstalowana kopia, gdy ten proces się zakończy (CleanupInstaller)
        std::wstring params = openWindow ? L"" : L"--tray";
        if (deleteSelf) params += (params.empty() ? L"" : L" ") + std::wstring(L"--cleanup-installer \"") + sb::pluginsetup::ExePath()
                                  + L"\" --parent-pid " + std::to_wstring(GetCurrentProcessId());
        ShellExecuteW(NULL, L"open", exe.c_str(), params.c_str(), NULL, SW_SHOW);
    }
    ExitProcess(0);
}

void GoPanel() {
    SetRole(g_role, false);
    ShowPage(PG_PANEL);
    PanelButtons();
}

// ---------------------------------------------------------------------------
// Ikona przy zegarze
// ---------------------------------------------------------------------------

void TrayMenu() {
    POINT pt; GetCursorPos(&pt);
    HMENU m = CreatePopupMenu();
    AppendMenuW(m, MF_STRING | MF_GRAYED, ID_TRAY_INFO, (std::wstring(kTitle) + L" " + kVersion).c_str());
    if (!g_updAvailable.empty()) AppendMenuW(m, MF_STRING, ID_TRAY_UPDATE, (L"Dostępna wersja " + g_updAvailable + L" — Aktualizuj").c_str());
    if (g_role == ROLE_WORKSTATION) {
        std::wstring card = g_cardSummary.empty() ? L"Karta: nie wykryto" : L"Karta: " + g_cardSummary;
        AppendMenuW(m, MF_STRING | MF_GRAYED, ID_TRAY_CARD, card.c_str());
        AppendMenuW(m, MF_STRING | MF_GRAYED, ID_TRAY_CARD + 100, g_wsInstalled ? L"Wtyczka pulpitu zdalnego: aktywna" : L"Wtyczka pulpitu zdalnego: brak");
    } else {
        AppendMenuW(m, MF_STRING | MF_GRAYED, ID_TRAY_CARD, (!g_ksp ? std::wstring(L"Serwer: nie zainstalowano") : g_syncedCount ? L"Certyfikaty z Twojego komputera: " + std::to_wstring(g_syncedCount) : std::wstring(L"Certyfikaty z Twojego komputera: brak")).c_str());
    }
    AppendMenuW(m, MF_SEPARATOR, 0, NULL);
    AppendMenuW(m, MF_STRING, ID_TRAY_SHOW, L"Otwórz panel");
    AppendMenuW(m, MF_STRING, ID_TRAY_HELP, L"Program nie działa? — co zrobić");
    AppendMenuW(m, MF_STRING, ID_TRAY_MANUAL, L"Instrukcja obsługi");
    if (g_role == ROLE_SERVER && g_ksp) AppendMenuW(m, MF_STRING | (g_remote ? 0 : MF_GRAYED), ID_TRAY_SYNC, L"Pobierz certyfikaty teraz");
    AppendMenuW(m, MF_STRING, ID_TRAY_REFRESH, L"Odśwież");
    HMENU role = CreatePopupMenu();
    AppendMenuW(role, MF_STRING | (g_role == ROLE_WORKSTATION ? MF_CHECKED : 0), ID_TRAY_ROLE_WS, L"Komputer z kartą / tokenem");
    AppendMenuW(role, MF_STRING | (g_role == ROLE_SERVER ? MF_CHECKED : 0), ID_TRAY_ROLE_SRV, L"Serwer pulpitu zdalnego");
    AppendMenuW(m, MF_POPUP, (UINT_PTR)role, L"Rola tego komputera");
    AppendMenuW(m, MF_SEPARATOR, 0, NULL);
    AppendMenuW(m, MF_STRING, ID_TRAY_EXIT, L"Zakończ");
    SetForegroundWindow(g_wnd);
    TrackPopupMenu(m, TPM_RIGHTBUTTON | TPM_BOTTOMALIGN, pt.x, pt.y, 0, g_wnd, NULL);
    DestroyMenu(m);
}

// ---------------------------------------------------------------------------
// Okno główne
// ---------------------------------------------------------------------------

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps; HDC dc = BeginPaint(hwnd, &ps);
        PaintHeader(dc);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_ERASEBKGND: {
        RECT r; GetClientRect(hwnd, &r); r.top = S(HEADER_H);
        HBRUSH b = CreateSolidBrush(C_BG); FillRect((HDC)wp, &r, b); DeleteObject(b);
        return 1;
    }
    case WM_DRAWITEM: if (((DRAWITEMSTRUCT*)lp)->CtlType == ODT_BUTTON) { DrawButton((DRAWITEMSTRUCT*)lp); return TRUE; } break;
    case WM_CTLCOLORSTATIC: {
        HDC dc = (HDC)wp; SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, (HWND)lp == g_logLbl ? C_MUTED : (HWND)lp == g_updLbl ? C_PRIMARY_DK : C_TEXT);
        return (LRESULT)GetStockObject(WHITE_BRUSH);
    }
    case WM_COMMAND: {
        int id = LOWORD(wp);
        switch (id) {
        case ID_CARD_WS: g_pendingRole = ROLE_WORKSTATION; InvalidateRect(g_cardWs, NULL, TRUE); InvalidateRect(g_cardSrv, NULL, TRUE); return 0;
        case ID_CARD_SRV: g_pendingRole = ROLE_SERVER; InvalidateRect(g_cardWs, NULL, TRUE); InvalidateRect(g_cardSrv, NULL, TRUE); return 0;
        case ID_BTN_NEXT: GoSummary(); return 0;
        case ID_BTN_BACK: if (Installed()) GoPanel(); else ShowWindow(hwnd, SW_HIDE); return 0;
        case ID_BTN_CANCEL: if (Installed()) GoPanel(); else ShowWindow(hwnd, SW_HIDE); return 0;
        case ID_BTN_INSTALL: StartInstall(JOB_INSTALL); return 0;
        case ID_BTN_OPEN_PANEL: {
            wchar_t t[64]; GetWindowTextW(g_btnOpenPanel, t, 64);
            if (wcscmp(t, L"Zakończ program") == 0) DestroyWindow(hwnd);
            else if (wcscmp(t, L"Spróbuj ponownie") == 0) GoWizard();
            else if (IsInstallerFile()) LaunchInstalledAndExit(true, SendMessageW(g_chkDelSelf, BM_GETCHECK, 0, 0) == BST_CHECKED);
            else GoPanel();
            return 0;
        }
        case ID_BTN_CLOSE:
            if (IsInstallerFile() && Installed()) LaunchInstalledAndExit(false, SendMessageW(g_chkDelSelf, BM_GETCHECK, 0, 0) == BST_CHECKED);
            ShowWindow(hwnd, SW_HIDE); return 0;
        case ID_BTN_SETUP: GoSummary(); return 0;
        case ID_BTN_HELP: case ID_TRAY_HELP: g_subtitle = L"Pomoc"; ShowWindowFront(); ShowPage(PG_HELP); return 0;
        case ID_BTN_HELP_BACK: GoPanel(); return 0;
        case ID_BTN_MANUAL: case ID_TRAY_MANUAL: g_subtitle = L"Instrukcja obsługi"; ShowWindowFront(); ShowPage(PG_MANUAL); return 0;
        case ID_BTN_MANUAL_BACK: GoPanel(); return 0;
        case ID_BTN_SETTINGS:
            g_subtitle = L"Ustawienia";
            SendMessageW(g_radWs2, BM_SETCHECK, g_role == ROLE_WORKSTATION ? BST_CHECKED : BST_UNCHECKED, 0);
            SendMessageW(g_radSrv2, BM_SETCHECK, g_role == ROLE_SERVER ? BST_CHECKED : BST_UNCHECKED, 0);
            SendMessageW(g_chkTabs, BM_SETCHECK, g_showTabs ? BST_CHECKED : BST_UNCHECKED, 0);
            ShowPage(PG_SETTINGS); return 0;
        case ID_RAD_WS2: if (g_role != ROLE_WORKSTATION) SetRole(ROLE_WORKSTATION, true); return 0;
        case ID_RAD_SRV2: if (g_role != ROLE_SERVER) SetRole(ROLE_SERVER, true); return 0;
        case ID_CHK_TABS: g_showTabs = SendMessageW(g_chkTabs, BM_GETCHECK, 0, 0) == BST_CHECKED; WriteCfg(L"ShowRoleTabs", g_showTabs ? L"1" : L"0"); return 0;
        case ID_BTN_SET_BACK: GoPanel(); return 0;
        case ID_BTN_HELP_LOGS: CreateDirectoryW(LogsDir().c_str(), NULL); ShellExecuteW(NULL, L"open", LogsDir().c_str(), NULL, NULL, SW_SHOW); return 0;
        case ID_TAB_WS: if (g_role != ROLE_WORKSTATION) { SetRole(ROLE_WORKSTATION, true); GoPanel(); } return 0;
        case ID_TAB_SRV: if (g_role != ROLE_SERVER) { SetRole(ROLE_SERVER, true); GoPanel(); } return 0;
        case ID_BTN_UPDATE: GoSummary(); return 0;
        case ID_BTN_UPD_OPEN: case ID_TRAY_UPDATE: OpenReleasePage(); return 0;
        case ID_BTN_UPD_LATER:
            WriteCfg(L"UpdateSnoozed", ReadCfg(L"UpdateLatest"));
            WriteCfg(L"UpdateRemindAfter", std::to_wstring(sb::update::NowSeconds() + sb::update::kRemindLaterSec));
            ShowUpdateState();
            return 0;
        case ID_BTN_UNINSTALL:
            if (MessageBoxW(hwnd, L"Odinstalować SignBridge z tego komputera? Podpisywanie kartą przez pulpit zdalny przestanie działać dla wszystkich jego użytkowników.", kTitle, MB_YESNO | MB_ICONWARNING) == IDYES)
                StartInstall(JOB_UNINSTALL);
            return 0;
        case ID_BTN_REFRESH: case ID_TRAY_REFRESH: Refresh(); TailLogs(false); return 0;
        case ID_BTN_LOGS: CreateDirectoryW(LogsDir().c_str(), NULL); ShellExecuteW(NULL, L"open", LogsDir().c_str(), NULL, NULL, SW_SHOW); return 0;
        case ID_TRAY_SHOW: ShowWindowFront(); return 0;
        case ID_TRAY_EXIT: DestroyWindow(hwnd); return 0;
        case ID_TRAY_ROLE_WS: if (g_role != ROLE_WORKSTATION) { SetRole(ROLE_WORKSTATION, true); GoPanel(); if (!InstalledForRole()) ShowWindowFront(); } return 0;
        case ID_TRAY_ROLE_SRV: if (g_role != ROLE_SERVER) { SetRole(ROLE_SERVER, true); GoPanel(); if (!InstalledForRole()) ShowWindowFront(); } return 0;
        case ID_CHK_AUTOSTART: {
            bool on = SendMessageW(g_chkAutostart, BM_GETCHECK, 0, 0) == BST_CHECKED;
            std::wstring exe = sb::pluginsetup::InstallDir() + L"\\" + sb::kspsetup::kAppExe;
            if (GetFileAttributesW(exe.c_str()) == INVALID_FILE_ATTRIBUTES) exe = sb::pluginsetup::ExePath();
            sb::pluginsetup::SetAutostart(kAutostartName, L"\"" + exe + L"\" --tray", on);
            Note(on ? L"Ikona przy zegarze po zalogowaniu: włączona." : L"Ikona przy zegarze po zalogowaniu: wyłączona.");
            return 0;
        }
        case ID_BTN_SYNC: case ID_TRAY_SYNC: if (id == ID_BTN_SYNC) ShowWindowFront(); StartJob(JOB_SYNC); return 0;
        case ID_BTN_TEST: ShowWindowFront(); StartJob(JOB_TEST); return 0;
        case ID_BTN_UNSYNC: StartJob(JOB_UNSYNC); return 0;
        }
        return 0;
    }
    case WM_NOTIFY: {
        NMHDR* h = (NMHDR*)lp;
        if (h->idFrom == ID_LINK && (h->code == NM_CLICK || h->code == NM_RETURN)) { ShellExecuteW(NULL, L"open", ((NMLINK*)lp)->item.szUrl, NULL, NULL, SW_SHOW); return 0; }
        return 0;
    }
    case WM_WORK_DONE: OnWorkDone((WorkResult*)lp); return 0;
    case WM_UPDATE_DONE: OnUpdateDone((UpdateResult*)lp); return 0;
    case WM_TRAY: {
        UINT ev = LOWORD(lp);
        if (ev == WM_LBUTTONUP || ev == WM_LBUTTONDBLCLK || ev == NIN_SELECT || ev == NIN_KEYSELECT) ShowWindowFront();
        else if (ev == WM_RBUTTONUP || ev == WM_CONTEXTMENU) TrayMenu();
        return 0;
    }
    case WM_TIMER:
        if (wp == TIMER_TAIL) { TailLogs(false); if (!g_busy && g_role == ROLE_WORKSTATION && g_page == PG_PANEL) Refresh(); }
        else if (wp == TIMER_AUTOSYNC) { KillTimer(hwnd, TIMER_AUTOSYNC); if (!g_busy) StartJob(JOB_SYNC_AUTO); else ScheduleAutoSync(5000); }
        else if (wp == TIMER_UPDATE) { SetTimer(hwnd, TIMER_UPDATE, 60 * 60 * 1000, NULL); ShowUpdateState(); MaybeCheckForUpdate(); }
        return 0;
    case WM_WTSSESSION_CHANGE:
        if (wp == WTS_REMOTE_CONNECT || wp == WTS_SESSION_UNLOCK || wp == WTS_CONSOLE_CONNECT) { g_syncFailures = 0; g_syncFailBalloonShown = false; ScheduleAutoSync(4000); Refresh(); }
        return 0;
    case WM_CLOSE:
        if (g_installing) return 0;
        ShowWindow(hwnd, SW_HIDE);
        return 0;
    case WM_ENDSESSION:
        // instalator MSI (Restart Manager) zamyka program, żeby podmienić plik: zakończ, zamiast schować okno —
        // inaczej plik zostaje w użyciu, konfigurację robi stara wersja i Windows żąda restartu (CP-SB-20)
        if (wp && (lp & ENDSESSION_CLOSEAPP) && !g_installing) DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        WTSUnRegisterSessionNotification(hwnd);
        Shell_NotifyIconW(NIM_DELETE, &g_nid);
        KillTimer(hwnd, TIMER_TAIL);
        KillTimer(hwnd, TIMER_AUTOSYNC);
        KillTimer(hwnd, TIMER_UPDATE);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

// Tryb bezokienkowy (wywołania z kreatora z podniesieniem uprawnień, z MSI, z GPO / skryptu):
//   --install-server [--msi]                       (administrator)
//   --uninstall-server [--msi]                     (administrator)
//   --install-workstation [--machine] [--no-shortcut] [--msi]   (--machine: HKLM, dla wszystkich; administrator)
//   --uninstall-workstation [--machine] [--msi]
// Wynik: kod 0 / 2 oraz %ProgramData%\cp-signbridge\install.log (serwer, --machine) albo
// %LOCALAPPDATA%\cp-signbridge\install.log (konto użytkownika).
struct Args { bool quiet = false, msi = false, machine = false, noShortcut = false, shortcut = false; std::wstring role; };
std::wstring ArgValue(const std::wstring& cmd, const wchar_t* flag) {
    size_t p = cmd.find(flag); if (p == std::wstring::npos) return L"";
    p += wcslen(flag); while (p < cmd.size() && cmd[p] == L' ') ++p;
    size_t e = cmd.find(L' ', p); return cmd.substr(p, e == std::wstring::npos ? std::wstring::npos : e - p);
}
Args ParseArgs(const std::wstring& cmd) {
    Args a;
    a.quiet = cmd.find(L"--quiet") != std::wstring::npos;
    a.msi = cmd.find(L"--msi") != std::wstring::npos;
    a.machine = cmd.find(L"--machine") != std::wstring::npos;
    a.noShortcut = cmd.find(L"--no-shortcut") != std::wstring::npos;
    a.shortcut = cmd.find(L"--shortcut") != std::wstring::npos && !a.noShortcut;
    a.role = ArgValue(cmd, L"--role");
    return a;
}

void WriteLogFile(const std::wstring& path, const std::vector<std::wstring>& log) {
    HANDLE f = CreateFileW(path.c_str(), GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, 0, NULL);
    if (f == INVALID_HANDLE_VALUE) return;
    for (auto& l : log) {
        std::wstring w = l + L"\r\n";
        int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), NULL, 0, NULL, NULL);
        std::string u(n, '\0'); WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), &u[0], n, NULL, NULL);
        DWORD wr; WriteFile(f, u.data(), (DWORD)u.size(), &wr, NULL);
    }
    CloseHandle(f);
}

int SilentServer(bool install, const Args& a) {
    std::vector<std::wstring> log;
    std::wstring pd = sb::kspsetup::ProgramDataDir();
    CreateDirectoryW(pd.c_str(), NULL);
    bool ok;
    if (install) {
        // pakiet tylko w katalogu administratorów (Program Files), nie w ProgramData
        std::wstring err, pkg = sb::kspsetup::PackageDir();
        ok = sb::kspsetup::PreparePackageDir(pkg, err) && ExtractPackage(pkg, err);
        if (!ok) log.push_back(L"! " + err);
        else ok = sb::kspsetup::InstallServer(pkg, log, a.msi);
        sb::kspsetup::RemoveDirectoryTree(pkg);
        WriteLogFile(pd + L"\\install.log", log);
    } else {
        ok = sb::kspsetup::UninstallServer(log, a.msi);
        wchar_t tmp[MAX_PATH]; GetTempPathW(MAX_PATH, tmp);
        WriteLogFile(std::wstring(tmp) + L"cp-signbridge-uninstall.log", log);
    }
    return ok ? 0 : 2;
}

// Pełna instalacja (domyślna): dostawca + wtyczka dla wszystkich użytkowników + program w Program Files
int SilentAll(bool install, const Args& a) {
    std::vector<std::wstring> log; std::wstring msg;
    std::wstring pd = sb::kspsetup::ProgramDataDir(), pf = sb::kspsetup::ProgramDir();
    bool ok;
    if (install) {
        CreateDirectoryW(pd.c_str(), NULL);
        std::wstring pkg = sb::kspsetup::PackageDir();
        ok = sb::kspsetup::PreparePackageDir(pkg, msg) && ExtractPackage(pkg, msg);
        if (!ok) log.push_back(L"! " + msg);
        else {
            ok = sb::kspsetup::InstallServer(pkg, log, a.msi);
            if (ok) {
                bool pl = sb::pluginsetup::Install(pkg, msg, true, pf);
                log.push_back((pl ? L"+ " : L"! ") + msg);
                ok = pl;
            }
            if (ok && a.shortcut) { sb::pluginsetup::CreateDesktopShortcut(pf + L"\\" + sb::kspsetup::kAppExe, L"", kTitle, msg, true); log.push_back(L"+ " + msg); }
            if (ok && (a.role == L"server" || a.role == L"workstation")) {
                HKEY h;
                if (RegCreateKeyExW(HKEY_LOCAL_MACHINE, kCfgKey, 0, NULL, 0, KEY_WRITE | KEY_WOW64_64KEY, NULL, &h, NULL) == ERROR_SUCCESS) {
                    RegSetValueExW(h, L"Role", 0, REG_SZ, (const BYTE*)a.role.c_str(), (DWORD)((a.role.size() + 1) * sizeof(wchar_t))); RegCloseKey(h);
                    log.push_back(a.role == L"server" ? L"+ Rola komputera: serwer pulpitu zdalnego" : L"+ Rola komputera: komputer z kartą / tokenem");
                }
            }
            sb::kspsetup::RemoveDirectoryTree(pkg);
        }
        WriteLogFile(pd + L"\\install.log", log);
    } else {
        ok = sb::pluginsetup::Uninstall(msg, true); log.push_back((ok ? L"+ " : L"~ ") + msg);
        wchar_t desk[MAX_PATH] = {}; SHGetFolderPathW(NULL, CSIDL_COMMON_DESKTOPDIRECTORY, NULL, SHGFP_TYPE_CURRENT, desk);
        DeleteFileW((std::wstring(desk) + L"\\" + kTitle + L".lnk").c_str());
        ok = sb::kspsetup::UninstallServer(log, a.msi) && ok;
        wchar_t tmp[MAX_PATH]; GetTempPathW(MAX_PATH, tmp);
        WriteLogFile(std::wstring(tmp) + L"cp-signbridge-uninstall.log", log);
    }
    return ok ? 0 : 2;
}

// Komputer z kartą: --machine = dla wszystkich użytkowników (Program Files, HKLM), inaczej dla bieżącego konta
int SilentWorkstation(bool install, const Args& a) {
    std::vector<std::wstring> log;
    std::wstring msg;
    bool ok = true;
    std::wstring dir = a.machine ? sb::kspsetup::ProgramDir() : sb::pluginsetup::InstallDir();
    std::wstring exe = dir + L"\\" + sb::kspsetup::kAppExe;
    CreateDirectoryW(dir.c_str(), NULL);
    if (install) {
        std::wstring pkg = dir + L"\\pkg";
        if (a.machine && !sb::kspsetup::PreparePackageDir(pkg, msg)) { log.push_back(L"! " + msg); ok = false; }
        else if (!ExtractPackage(pkg, msg)) { log.push_back(L"! " + msg); ok = false; }
        else {
            ok = sb::pluginsetup::Install(pkg, msg, a.machine, dir); log.push_back((ok ? L"+ " : L"! ") + msg);
            if (ok) {
                if (!a.msi) { if (CopyExeTo(exe)) log.push_back(L"+ " + exe); else { exe = sb::pluginsetup::ExePath(); log.push_back(L"~ Nie udało się skopiować programu; używam " + exe); } }
                std::wstring run = L"\"" + exe + L"\" --tray";
                if (a.machine) {
                    HKEY h;
                    if (RegCreateKeyExW(HKEY_LOCAL_MACHINE, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, NULL, 0, KEY_WRITE | KEY_WOW64_64KEY, NULL, &h, NULL) == ERROR_SUCCESS) {
                        RegSetValueExW(h, kAutostartName, 0, REG_SZ, (const BYTE*)run.c_str(), (DWORD)((run.size() + 1) * sizeof(wchar_t))); RegCloseKey(h);
                        log.push_back(L"+ Ikona przy zegarze dla każdego użytkownika (HKLM Run).");
                    }
                } else {
                    sb::pluginsetup::SetAutostart(kAutostartName, run, true);
                    log.push_back(L"+ Ikona przy zegarze po zalogowaniu.");
                    if (!a.noShortcut) { sb::pluginsetup::CreateDesktopShortcut(exe, L"", kTitle, msg); log.push_back(L"+ " + msg); }
                    WriteCfg(L"Role", L"workstation");
                }
                if (!a.msi) sb::kspsetup::WriteInstallRegistry(a.machine ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER, dir, exe, kVersion, L"workstation");
            }
        }
    } else {
        ok = sb::pluginsetup::Uninstall(msg, a.machine); log.push_back((ok ? L"+ " : L"! ") + msg);
        if (a.machine) {
            HKEY h;
            if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_WRITE | KEY_WOW64_64KEY, &h) == ERROR_SUCCESS) { RegDeleteValueW(h, kAutostartName); RegCloseKey(h); }
        } else {
            sb::pluginsetup::SetAutostart(kAutostartName, L"", false);
            wchar_t desktop[MAX_PATH] = {}; SHGetFolderPathW(NULL, CSIDL_DESKTOPDIRECTORY, NULL, SHGFP_TYPE_CURRENT, desktop);
            DeleteFileW((std::wstring(desktop) + L"\\" + kTitle + L".lnk").c_str());
        }
        if (a.machine) sb::kspsetup::RemoveAllUsersConfig(); else sb::kspsetup::RemoveCurrentUserConfig();
        if (!a.msi) {
            sb::kspsetup::RemoveInstallRegistry(a.machine ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER);
            sb::kspsetup::RemoveDirectoryTree(dir);
        } else { sb::kspsetup::RemoveDirectoryTree(dir + L"\\pkg"); DeleteFileW((dir + L"\\cpsignbridge-rdp.dll").c_str()); sb::kspsetup::RemoveDirectoryTree(dir + L"\\x86"); }
        if (!sb::kspsetup::IsKspRegistered()) sb::kspsetup::RemoveDirectoryTree(sb::kspsetup::ProgramDataDir());
        log.push_back(L"+ Odinstalowano.");
    }
    if (install) WriteLogFile(dir + L"\\install.log", log);
    else { wchar_t tmp[MAX_PATH]; GetTempPathW(MAX_PATH, tmp); WriteLogFile(std::wstring(tmp) + L"cp-signbridge-uninstall.log", log); }
    return ok ? 0 : 2;
}

// Usunięcie pliku instalatora: instalator uruchamia zainstalowaną kopię z
//   --cleanup-installer "<ścieżka instalatora>" --parent-pid <PID instalatora>
// i od razu się kończy. Kopia czeka (z limitem czasu) na koniec procesu instalatora i usuwa plik (DeleteFileW);
// gdy plik jest nadal zablokowany — usunięcie po restarcie (MoveFileExW, tylko z uprawnieniami administratora),
// inaczej plik zostaje. Bez powłoki poleceń, skryptów i innych procesów pomocniczych.
const DWORD kCleanupWaitMs = 30000;
struct Cleanup { std::wstring path; DWORD parentPid = 0; };

// Wycina z linii poleceń --cleanup-installer "<ścieżka>" i --parent-pid N. Ścieżka może zawierać spacje i tekst
// podobny do przełączników — dlatego reszta linii jest parsowana (ParseArgs, --tray…) dopiero po wycięciu.
bool ExtractCleanupArgs(std::wstring& cmd, Cleanup& c) {
    const std::wstring flag = L"--cleanup-installer";
    size_t p = cmd.find(flag);
    if (p == std::wstring::npos) return false;
    size_t q = p + flag.size(), end = 0;
    while (q < cmd.size() && cmd[q] == L' ') ++q;
    if (q < cmd.size() && cmd[q] == L'"') {
        size_t close = cmd.find(L'"', q + 1);
        if (close == std::wstring::npos) { cmd.erase(p); return false; }
        c.path = cmd.substr(q + 1, close - q - 1); end = close + 1;
    } else {
        end = cmd.find(L' ', q); if (end == std::wstring::npos) end = cmd.size();
        c.path = cmd.substr(q, end - q);
    }
    cmd.erase(p, end - p);
    const std::wstring pidFlag = L"--parent-pid";
    size_t pp = cmd.find(pidFlag);
    if (pp != std::wstring::npos) {
        size_t v = pp + pidFlag.size(); while (v < cmd.size() && cmd[v] == L' ') ++v;
        size_t e = v; while (e < cmd.size() && cmd[e] >= L'0' && cmd[e] <= L'9') ++e;
        unsigned long long pid = 0;
        for (size_t i = v; i < e && pid <= 0xFFFFFFFFull; ++i) pid = pid * 10 + (cmd[i] - L'0');
        if (e > v && pid <= 0xFFFFFFFFull) c.parentPid = (DWORD)pid;
        cmd.erase(pp, e - pp);
    }
    return true;
}

std::wstring KnownDir(int csidl) {
    wchar_t buf[MAX_PATH] = {};
    return SUCCEEDED(SHGetFolderPathW(NULL, csidl, NULL, SHGFP_TYPE_CURRENT, buf)) ? std::wstring(buf) : std::wstring();
}
std::wstring LongPath(const std::wstring& p) {
    wchar_t buf[MAX_PATH * 2];
    DWORD n = GetLongPathNameW(p.c_str(), buf, _countof(buf));
    return (n && n < _countof(buf)) ? std::wstring(buf) : p;
}
bool IsInDir(const std::wstring& path, std::wstring dir) {
    if (dir.empty()) return false;
    dir = LongPath(dir);
    if (dir.back() != L'\\') dir += L'\\';
    return path.size() > dir.size() && _wcsnicmp(path.c_str(), dir.c_str(), dir.size()) == 0;
}

// Tylko istniejący zwykły plik .exe pod pełną ścieżką z literą dysku; nie w folderach systemowych, nie w Program Files
// ani w katalogach instalacji SignBridge, nie ten program. out: ścieżka znormalizowana (długie nazwy).
bool InstallerPathAllowed(const std::wstring& raw, std::wstring& out) {
    if (raw.size() < 8 || raw.size() >= MAX_PATH || raw.find_first_of(L"*?<>|\"/") != std::wstring::npos) return false;
    wchar_t d = raw[0];
    if (!((d >= L'A' && d <= L'Z') || (d >= L'a' && d <= L'z')) || raw[1] != L':' || raw[2] != L'\\') return false;
    wchar_t full[MAX_PATH * 2];
    DWORD n = GetFullPathNameW(raw.c_str(), _countof(full), full, NULL);
    if (!n || n >= _countof(full) || _wcsicmp(full, raw.c_str()) != 0) return false;   // bez „..\”, „.\” itp.
    std::wstring path = LongPath(full);
    size_t slash = path.find_last_of(L'\\'), dot = path.find_last_of(L'.');
    if (dot == std::wstring::npos || dot < slash || _wcsicmp(path.c_str() + dot, L".exe") != 0) return false;
    DWORD attr = GetFileAttributesW(path.c_str());
    if (attr == INVALID_FILE_ATTRIBUTES || (attr & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT | FILE_ATTRIBUTE_SYSTEM))) return false;
    if (_wcsicmp(path.c_str(), LongPath(sb::pluginsetup::ExePath()).c_str()) == 0) return false;
    for (const std::wstring& dir : { sb::kspsetup::ProgramDir(), sb::kspsetup::ProgramDataDir(), sb::pluginsetup::InstallDir(),
                                     KnownDir(CSIDL_WINDOWS), KnownDir(CSIDL_SYSTEM), KnownDir(CSIDL_PROGRAM_FILES),
                                     KnownDir(CSIDL_PROGRAM_FILESX86), KnownDir(CSIDL_PROGRAM_FILES_COMMON) })
        if (IsInDir(path, dir)) return false;
    out = path;
    return true;
}

void CleanupInstaller(const Cleanup& c) {
    std::wstring path;
    if (!c.parentPid || c.parentPid == GetCurrentProcessId() || !InstallerPathAllowed(c.path, path)) return;
    HANDLE h = OpenProcess(SYNCHRONIZE | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, c.parentPid);
    if (h) {
        // PID musi nadal należeć do instalatora (ten sam plik) — inaczej numer dostał już inny proces
        wchar_t img[MAX_PATH * 2]; DWORD n = _countof(img);
        bool same = QueryFullProcessImageNameW(h, 0, img, &n) && _wcsicmp(LongPath(img).c_str(), path.c_str()) == 0;
        if (same) WaitForSingleObject(h, kCleanupWaitMs);
        CloseHandle(h);
        if (!same) return;
    } else if (GetLastError() != ERROR_INVALID_PARAMETER) {
        return;   // proces istnieje, ale nie da się go sprawdzić — plik zostaje
    }             // ERROR_INVALID_PARAMETER: instalator już się zakończył
    if (DeleteFileW(path.c_str())) return;
    if (sb::kspsetup::IsAdmin()) MoveFileExW(path.c_str(), NULL, MOVEFILE_DELAY_UNTIL_REBOOT);   // inaczej plik zostaje
}

DWORD WINAPI CleanupThread(LPVOID p) {
    Cleanup* c = (Cleanup*)p;
    CleanupInstaller(*c);
    delete c;
    return 0;
}

// Pobrany instalator innej wersji, a w tej sesji działa zainstalowany program (aktualizacja z powiadomienia przy
// ikonie): zamknij go poleceniem „Zakończ” z menu ikony (rozumie je też 1.1.0) i przejmij mutex — wtedy instalator
// pokaże kreator, a nie tylko stare okno (CP-SB-20). false = zostaw działającą kopię (dotychczasowe zachowanie).
bool CloseRunningAppForUpdate(HWND other, HANDLE mutex) {
    DWORD pid = 0;
    if (!other || !GetWindowThreadProcessId(other, &pid) || !pid) return false;
    std::wstring running;
    if (HANDLE p = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid)) {
        wchar_t buf[MAX_PATH * 2]; DWORD n = ARRAYSIZE(buf);
        if (QueryFullProcessImageNameW(p, 0, buf, &n)) running.assign(buf, n);
        CloseHandle(p);
    }
    if (!sb::update::CloseRunningForInstall(IsInstallerFile(), running, InstalledExe(),
                                            sb::kspsetup::InstalledPanelVersion(), kVersion)) return false;
    SB_INFO(L"Aktualizacja: zamykam działający program %s (pid %lu), żeby uruchomić instalator %s", running.c_str(), pid, kVersion);
    PostMessageW(other, WM_COMMAND, ID_TRAY_EXIT, 0);
    DWORD w = WaitForSingleObject(mutex, 10000);
    if (w == WAIT_OBJECT_0 || w == WAIT_ABANDONED) return true;
    SB_WARN(L"Aktualizacja: działający program nie zakończył się w 10 s — pokazuję jego okno");
    return false;
}

HFONT MakeFont(int pt, int weight) {
    return CreateFontW(-MulDiv(pt, (int)(96 * g_scale), 72), 0, 0, 0, weight, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                       OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
}

}  // namespace

int WINAPI wWinMain(HINSTANCE hinst, HINSTANCE, LPWSTR cmdLine, int) {
    g_hinst = hinst;
    std::wstring cmd = cmdLine ? cmdLine : L"";
    Cleanup cleanup;
    bool wantCleanup = ExtractCleanupArgs(cmd, cleanup);
    Args args = ParseArgs(cmd);
    if (cmd.find(L"--install-all") != std::wstring::npos) return SilentAll(true, args);
    if (cmd.find(L"--uninstall-all") != std::wstring::npos) return SilentAll(false, args);
    if (cmd.find(L"--install-server") != std::wstring::npos) return SilentServer(true, args);
    if (cmd.find(L"--uninstall-server") != std::wstring::npos) return SilentServer(false, args);
    if (cmd.find(L"--install-workstation") != std::wstring::npos) return SilentWorkstation(true, args);
    if (cmd.find(L"--uninstall-workstation") != std::wstring::npos) return SilentWorkstation(false, args);

    typedef BOOL (WINAPI *PFN_SetCtx)(HANDLE);
    if (HMODULE u32 = GetModuleHandleW(L"user32.dll"))
        if (auto f = (PFN_SetCtx)GetProcAddress(u32, "SetProcessDpiAwarenessContext")) f((HANDLE)-4 /*PER_MONITOR_AWARE_V2*/);
    { HDC dc = GetDC(NULL); g_scale = GetDeviceCaps(dc, LOGPIXELSX) / 96.0; ReleaseDC(NULL, dc); }
    W = S(860); H = S(640);

    // usunięcie pliku instalatora w tle (czeka na zakończenie procesu instalatora)
    HANDLE cleanupThread = NULL;
    if (wantCleanup) {
        Cleanup* c = new Cleanup(cleanup);
        cleanupThread = CreateThread(NULL, 0, CleanupThread, c, 0, NULL);
        if (!cleanupThread) delete c;
    }
    auto finishCleanup = [&]() { if (cleanupThread) { WaitForSingleObject(cleanupThread, kCleanupWaitMs + 5000); CloseHandle(cleanupThread); cleanupThread = NULL; } };

    HANDLE mutex = CreateMutexW(NULL, TRUE, L"Local\\cp-signbridge-panel");
    g_mutex = mutex;
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        HWND other = FindWindowW(kClass, NULL);
        if (!CloseRunningAppForUpdate(other, mutex)) {
            if (other) { ShowWindow(other, SW_SHOW); ShowWindow(other, SW_RESTORE); SetForegroundWindow(other); }
            finishCleanup();   // program już działa (np. aktualizacja przy otwartej ikonie) — dokończ sprzątanie przed wyjściem
            return 0;
        }
    }

    INITCOMMONCONTROLSEX icc = { sizeof(icc), ICC_LISTVIEW_CLASSES | ICC_STANDARD_CLASSES | ICC_LINK_CLASS | ICC_PROGRESS_CLASS };
    InitCommonControlsEx(&icc);
    bool startHidden = cmd.find(L"--tray") != std::wstring::npos;
    g_showTabs = ReadCfg(L"ShowRoleTabs") == L"1";
    bool wantUninstall = cmd.find(L"--uninstall") != std::wstring::npos;

    g_ksp = sb::kspsetup::IsKspRegistered();
    std::wstring saved = ReadCfg(L"Role");                       // wybór tego użytkownika (zakładka / menu ikony)
    if (saved.empty()) saved = ReadCfgFrom(HKEY_LOCAL_MACHINE, L"Role");   // ustawione przy instalacji dla całego komputera
    if (saved == L"server" || saved == L"workstation") g_role = saved == L"server" ? ROLE_SERVER : ROLE_WORKSTATION;
    else g_role = IsWindowsServer() ? ROLE_SERVER : ROLE_WORKSTATION;

    g_font = MakeFont(10, FW_NORMAL); g_fontBold = MakeFont(10, FW_SEMIBOLD); g_fontH1 = MakeFont(16, FW_SEMIBOLD);
    g_fontH2 = MakeFont(12, FW_SEMIBOLD); g_fontSmall = MakeFont(9, FW_NORMAL); g_fontBig = MakeFont(20, FW_SEMIBOLD);
    g_iconApp = (HICON)LoadImageW(hinst, MAKEINTRESOURCEW(IDI_APP_DARK), IMAGE_ICON, S(48), S(48), 0);   // nagłówek ma ciemne tło
    g_iconCompany = (HICON)LoadImageW(hinst, MAKEINTRESOURCEW(IDI_COMPANY), IMAGE_ICON, S(24), S(24), 0);

    WNDCLASSW wc = {};
    wc.lpfnWndProc = WndProc; wc.hInstance = hinst; wc.lpszClassName = kClass;
    wc.hIcon = LoadIconW(hinst, MAKEINTRESOURCEW(IDI_APP)); wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
    RegisterClassW(&wc);
    WNDCLASSW sc = {};
    sc.lpfnWndProc = StatusProc; sc.hInstance = hinst; sc.lpszClassName = kStatusClass; sc.hCursor = LoadCursorW(NULL, IDC_ARROW);
    RegisterClassW(&sc);

    RECT r = { 0, 0, W, H };
    AdjustWindowRect(&r, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, FALSE);
    g_wnd = CreateWindowW(kClass, kTitle, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
                          CW_USEDEFAULT, CW_USEDEFAULT, r.right - r.left, r.bottom - r.top, NULL, NULL, hinst, NULL);
    BuildUi();
    WTSRegisterSessionNotification(g_wnd, NOTIFY_FOR_THIS_SESSION);

    g_nid.cbSize = sizeof(g_nid); g_nid.hWnd = g_wnd; g_nid.uID = 1;
    g_nid.uFlags = NIF_ICON | NIF_TIP | NIF_MESSAGE; g_nid.uCallbackMessage = WM_TRAY;
    g_nid.hIcon = LoadIconW(hinst, MAKEINTRESOURCEW(IDI_OFF));
    wcscpy_s(g_nid.szTip, kTitle);
    Shell_NotifyIconW(NIM_ADD, &g_nid);
    g_nid.uVersion = NOTIFYICON_VERSION_4;
    Shell_NotifyIconW(NIM_SETVERSION, &g_nid);

    SetRole(g_role, false);
    SetTimer(g_wnd, TIMER_TAIL, 3000, NULL);

    bool installed = InstalledForRole();
    if (wantUninstall) {
        if (args.quiet || MessageBoxW(NULL, L"Odinstalować SignBridge?", kTitle, MB_YESNO | MB_ICONQUESTION) == IDYES) { if (!args.quiet) ShowWindowFront(); StartInstall(JOB_UNINSTALL); }
        else { finishCleanup(); return 0; }
    } else if (!installed) {
        GoWizard();
        ShowWindowFront();
    } else if (IsInstallerFile() && sb::kspsetup::InstalledPanelVersion() == kVersion) {
        // ten sam instalator uruchomiony ponownie: nie pracuj z pliku instalatora — otwórz zainstalowany program
        LaunchInstalledAndExit(true, false);
    } else if (IsInstallerFile()) {
        GoWizard();            // nowsza / inna wersja: od razu aktualizacja
        ShowWindowFront();
    } else {
        GoPanel();
        Note(g_role == ROLE_SERVER ? L"Panel serwera. Certyfikaty z Twojego komputera pobierają się same — podpisuj w programie jak zwykle."
                                   : L"Panel gotowy. Połącz się z serwerem przez Podłączanie pulpitu zdalnego i podpisuj w programie.");
        if (!startHidden || !installed) ShowWindowFront();
        // nowa wersja: od razu z zapamiętanego wyniku; zapytanie po kilku sekundach, a przy autostarcie
        // (--tray, logowanie wielu użytkowników serwera naraz) z losowym opóźnieniem 1–5 min
        ShowUpdateState();
        if (sb::update::DisabledByPolicy()) SB_INFO(L"Nowa wersja: sprawdzanie wyłączone przez administratora (HKLM\\%s\\%s)", sb::update::kPolicyKey, sb::update::kPolicyValue);
        else SetTimer(g_wnd, TIMER_UPDATE, startHidden ? 60000 + (GetTickCount() ^ GetCurrentProcessId()) % 240000 : 3000, NULL);
    }

    MSG m;
    while (GetMessageW(&m, NULL, 0, 0)) {
        if (!IsDialogMessageW(g_wnd, &m)) { TranslateMessage(&m); DispatchMessageW(&m); }
    }
    finishCleanup();
    CloseHandle(mutex);
    return 0;
}
