// SignBridge — © 2026 Craftprojects Sp. z o.o. (craftprojects.pl)
// cp-signbridge — powiadomienie o nowej wersji: porównanie wersji, minimalny parser JSON, zapytanie WinHTTP
#include "updatecheck.h"
#include <winhttp.h>
#include <vector>

#pragma comment(lib, "winhttp.lib")

#ifndef WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_3
#define WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_3 0x00002000
#endif

namespace sb::update {

const wchar_t* const kApiHost = L"api.github.com";
const wchar_t* const kApiPath = L"/repos/craftprojects-sp-zoo/cp-signbridge-official/releases/latest";
const wchar_t* const kReleasePageUrl = L"https://github.com/craftprojects-sp-zoo/cp-signbridge-official/releases/latest";
const wchar_t* const kPolicyKey = L"SOFTWARE\\Policies\\Craftprojects\\SignBridge";
const wchar_t* const kPolicyValue = L"DisableUpdateCheck";

const wchar_t* UpdateButtonUrl() { return kReleasePageUrl; }

// ---------------------------------------------------------------------------
// Wersje
// ---------------------------------------------------------------------------

namespace {
template <class S>
bool ParseVersionT(const S& s, Version& out) {
    Version v;
    size_t i = 0, n = s.size();
    if (i < n && (s[i] == 'v' || s[i] == 'V')) ++i;
    int part = 0;
    while (true) {
        if (part >= 4) return false;
        size_t start = i;
        uint32_t val = 0;
        while (i < n && s[i] >= '0' && s[i] <= '9') {
            if (i - start >= 5) return false;
            val = val * 10 + (uint32_t)(s[i] - '0');
            ++i;
        }
        if (i == start || val > 0xFFFF) return false;
        v.v[part++] = (uint16_t)val;
        if (i == n) break;
        if (s[i] != '.') return false;
        ++i;
    }
    out = v;
    return true;
}
}  // namespace

bool ParseVersion(const std::string& s, Version& out) { return ParseVersionT(s, out); }
bool ParseVersion(const std::wstring& s, Version& out) { return ParseVersionT(s, out); }

int Compare(const Version& a, const Version& b) {
    for (int i = 0; i < 4; ++i)
        if (a.v[i] != b.v[i]) return a.v[i] < b.v[i] ? -1 : 1;
    return 0;
}

std::wstring ToDisplay(const Version& v) {
    std::wstring s = std::to_wstring(v.v[0]) + L"." + std::to_wstring(v.v[1]) + L"." + std::to_wstring(v.v[2]);
    if (v.v[3]) s += L"." + std::to_wstring(v.v[3]);
    return s;
}

// ---------------------------------------------------------------------------
// Minimalny parser JSON: sprawdza poprawność całego dokumentu (bez budowania drzewa), z ograniczeniem
// głębokości; wartości zapamiętuje tylko dla tag_name / draft / prerelease w obiekcie najwyższego poziomu.
// Wszystkie odczyty przez wskaźnik z kontrolą końca bufora (bez arytmetyki na długościach z danych).
// ---------------------------------------------------------------------------

namespace {
struct Json {
    const char* p;
    const char* e;

    bool more() const { return p < e; }
    void ws() { while (p < e && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) ++p; }
    bool eat(char c) { ws(); if (p < e && *p == c) { ++p; return true; } return false; }
    bool lit(const char* s) {
        const char* q = p;
        for (; *s; ++s, ++q) if (q >= e || *q != *s) return false;
        p = q;
        return true;
    }
    static int hex(char c) {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    }
    // Ciąg znaków; out (opcjonalnie) dostaje treść, najwyżej maxLen bajtów — dłuższy = błąd.
    bool str(std::string* out, size_t maxLen) {
        ws();
        if (p >= e || *p != '"') return false;
        ++p;
        while (p < e) {
            unsigned char c = (unsigned char)*p++;
            if (c == '"') return true;
            if (c < 0x20) return false;
            char add = (char)c;
            if (c == '\\') {
                if (p >= e) return false;
                char x = *p++;
                switch (x) {
                case '"': case '\\': case '/': add = x; break;
                case 'b': add = '\b'; break;
                case 'f': add = '\f'; break;
                case 'n': add = '\n'; break;
                case 'r': add = '\r'; break;
                case 't': add = '\t'; break;
                case 'u': {
                    unsigned u = 0;
                    for (int k = 0; k < 4; ++k) {
                        if (p >= e) return false;
                        int h = hex(*p++);
                        if (h < 0) return false;
                        u = (u << 4) | (unsigned)h;
                    }
                    add = u < 0x80 ? (char)u : '?';   // poza ASCII — i tak nie jest poprawną wersją
                    break;
                }
                default: return false;
                }
            }
            if (out) {
                if (out->size() >= maxLen) return false;
                out->push_back(add);
            }
        }
        return false;   // obcięty
    }
    bool num() {
        ws();
        const char* s = p;
        if (p < e && *p == '-') ++p;
        const char* d = p;
        while (p < e && *p >= '0' && *p <= '9') ++p;
        if (p == d) return false;
        if (p < e && *p == '.') { ++p; const char* f = p; while (p < e && *p >= '0' && *p <= '9') ++p; if (p == f) return false; }
        if (p < e && (*p == 'e' || *p == 'E')) {
            ++p;
            if (p < e && (*p == '+' || *p == '-')) ++p;
            const char* x = p; while (p < e && *p >= '0' && *p <= '9') ++p;
            if (p == x) return false;
        }
        return p > s;
    }
    // Dowolna wartość (sprawdzana, pomijana). depth = poziom zagnieżdżenia tej wartości.
    bool value(int depth) {
        ws();
        if (p >= e) return false;
        char c = *p;
        if (c == '"') return str(nullptr, 0);
        if (c == '{' || c == '[') {
            if (depth >= kMaxDepth) return false;
            char close = c == '{' ? '}' : ']';
            ++p;
            if (eat(close)) return true;
            while (true) {
                if (c == '{') { if (!str(nullptr, 0) || !eat(':')) return false; }
                if (!value(depth + 1)) return false;
                if (eat(',')) continue;
                return eat(close);
            }
        }
        if (c == 't') return lit("true");
        if (c == 'f') return lit("false");
        if (c == 'n') return lit("null");
        return num();
    }
    // true / false; inne wartości = błąd
    bool boolean(bool& out) {
        ws();
        if (lit("true")) { out = true; return true; }
        if (lit("false")) { out = false; return true; }
        return false;
    }
};
}  // namespace

bool ParseRelease(const char* data, size_t len, ReleaseInfo& out) {
    out = ReleaseInfo();
    if (!data || !len) return false;
    Json j{ data, data + len };
    // BOM UTF-8 dopuszczalny na początku
    if (len >= 3 && (unsigned char)data[0] == 0xEF && (unsigned char)data[1] == 0xBB && (unsigned char)data[2] == 0xBF) j.p += 3;
    if (!j.eat('{')) return false;
    ReleaseInfo r;
    bool seenDraft = false, seenPre = false;
    if (!j.eat('}')) {
        while (true) {
            std::string key;
            if (!j.str(&key, 256) || !j.eat(':')) return false;   // klucze API GitHub są krótkie
            if (key == "tag_name") {
                if (r.hasTag) return false;   // duplikat — niejednoznaczne
                if (!j.str(&r.tag, kMaxTagLen)) return false;
                r.hasTag = true;
            } else if (key == "draft") {
                if (seenDraft || !j.boolean(r.draft)) return false;
                seenDraft = true;
            } else if (key == "prerelease") {
                if (seenPre || !j.boolean(r.prerelease)) return false;
                seenPre = true;
            } else if (!j.value(1)) {
                return false;
            }
            if (j.eat(',')) continue;
            if (j.eat('}')) break;
            return false;
        }
    }
    j.ws();
    if (j.more()) return false;   // coś po dokumencie
    if (!r.hasTag) return false;
    out = r;
    return true;
}

Verdict Decide(const ReleaseInfo& r, const Version& current, Version& newer) {
    if (!r.hasTag || r.draft || r.prerelease) return Verdict::Ignored;
    Version v;
    if (!ParseVersion(r.tag, v)) return Verdict::Ignored;
    newer = v;
    return Compare(v, current) > 0 ? Verdict::Newer : Verdict::NotNewer;
}

bool CheckDue(ULONGLONG lastCheck, ULONGLONG now) {
    return lastCheck == 0 || now < lastCheck || now - lastCheck >= kCheckIntervalSec;
}

bool ShouldShow(const std::wstring& latest, const Version& current, const std::wstring& snoozedVersion,
                ULONGLONG remindAfter, ULONGLONG now, Version& shown) {
    Version v;
    if (!ParseVersion(latest, v) || Compare(v, current) <= 0) return false;
    if (snoozedVersion == latest && now < remindAfter && remindAfter - now <= kRemindLaterSec) return false;
    shown = v;
    return true;
}

ULONGLONG NowSeconds() {
    FILETIME ft; GetSystemTimeAsFileTime(&ft);
    ULARGE_INTEGER u = { ft.dwLowDateTime, ft.dwHighDateTime };
    return u.QuadPart / 10000000ull;
}

// ---------------------------------------------------------------------------
// Polityka i zapytanie
// ---------------------------------------------------------------------------

bool DisabledByPolicy() {
    DWORD v = 0, cb = sizeof(v);
    LONG rc = RegGetValueW(HKEY_LOCAL_MACHINE, kPolicyKey, kPolicyValue, RRF_RT_REG_DWORD | RRF_SUBKEY_WOW6464KEY, NULL, &v, &cb);
    return rc == ERROR_SUCCESS && v != 0;
}

namespace {
// ETag do nagłówka If-None-Match: tylko widoczne znaki ASCII, rozsądna długość (bez CR/LF — bez wstrzykiwania nagłówków)
bool SafeHeaderValue(const std::wstring& s) {
    if (s.empty() || s.size() > 200) return false;
    for (wchar_t c : s) if (c < 0x21 || c > 0x7E) return false;
    return true;
}
struct Handle {
    HINTERNET h = NULL;
    explicit Handle(HINTERNET x) : h(x) {}
    ~Handle() { if (h) WinHttpCloseHandle(h); }
    Handle(const Handle&) = delete;
    Handle& operator=(const Handle&) = delete;
};
}  // namespace

FetchResult Fetch(const std::wstring& userAgent, const std::wstring& ifNoneMatch) {
    FetchResult r;
    // Proxy z ustawień systemu (Windows 8.1+), inaczej ustawienia WinHTTP (netsh winhttp)
    HINTERNET hs = WinHttpOpen(userAgent.c_str(), WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hs) hs = WinHttpOpen(userAgent.c_str(), WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    Handle session(hs);
    if (!session.h) { r.error = GetLastError(); return r; }
    WinHttpSetTimeouts(session.h, (int)kTimeoutMs, (int)kTimeoutMs, (int)kTimeoutMs, (int)kTimeoutMs);
    DWORD proto = WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2 | WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_3;
    if (!WinHttpSetOption(session.h, WINHTTP_OPTION_SECURE_PROTOCOLS, &proto, sizeof(proto))) {
        proto = WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2;   // starsze systemy bez TLS 1.3
        WinHttpSetOption(session.h, WINHTTP_OPTION_SECURE_PROTOCOLS, &proto, sizeof(proto));
    }
    Handle conn(WinHttpConnect(session.h, kApiHost, INTERNET_DEFAULT_HTTPS_PORT, 0));
    if (!conn.h) { r.error = GetLastError(); return r; }
    Handle req(WinHttpOpenRequest(conn.h, L"GET", kApiPath, NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                                  WINHTTP_FLAG_SECURE | WINHTTP_FLAG_REFRESH));
    if (!req.h) { r.error = GetLastError(); return r; }
    // Bez przekierowań (zostajemy na stałym adresie) i bez ciasteczek. Certyfikat serwera sprawdza Windows
    // (łańcuch do zaufanego urzędu i nazwa api.github.com) — flag ignorowania błędów certyfikatu nie ustawiamy.
    DWORD redirect = WINHTTP_OPTION_REDIRECT_POLICY_NEVER;
    WinHttpSetOption(req.h, WINHTTP_OPTION_REDIRECT_POLICY, &redirect, sizeof(redirect));
    DWORD features = WINHTTP_DISABLE_COOKIES;
    WinHttpSetOption(req.h, WINHTTP_OPTION_DISABLE_FEATURE, &features, sizeof(features));

    std::wstring headers = L"Accept: application/vnd.github+json\r\nX-GitHub-Api-Version: 2022-11-28\r\n";
    if (SafeHeaderValue(ifNoneMatch)) headers += L"If-None-Match: " + ifNoneMatch + L"\r\n";
    if (!WinHttpSendRequest(req.h, headers.c_str(), (DWORD)-1L, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) ||
        !WinHttpReceiveResponse(req.h, NULL)) {
        r.error = GetLastError();
        return r;
    }
    DWORD status = 0, cb = sizeof(status);
    if (!WinHttpQueryHeaders(req.h, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &status, &cb, WINHTTP_NO_HEADER_INDEX)) {
        r.error = GetLastError();
        return r;
    }
    r.httpStatus = status;
    wchar_t etag[256] = {};
    DWORD cbEtag = sizeof(etag) - sizeof(wchar_t);
    if (WinHttpQueryHeaders(req.h, WINHTTP_QUERY_ETAG, WINHTTP_HEADER_NAME_BY_INDEX, etag, &cbEtag, WINHTTP_NO_HEADER_INDEX) && SafeHeaderValue(etag))
        r.etag = etag;
    if (status == 304) { r.ok = true; r.notModified = true; return r; }
    if (status != 200) return r;

    std::vector<char> buf(16 * 1024);
    while (true) {
        DWORD rd = 0;
        if (!WinHttpReadData(req.h, buf.data(), (DWORD)buf.size(), &rd)) { r.error = GetLastError(); r.body.clear(); return r; }
        if (rd == 0) break;
        if (rd > buf.size() || r.body.size() + rd > kMaxResponseBytes) { r.body.clear(); r.error = ERROR_INSUFFICIENT_BUFFER; return r; }
        r.body.append(buf.data(), rd);
    }
    r.ok = true;
    return r;
}

}  // namespace sb::update
