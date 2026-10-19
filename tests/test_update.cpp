// SignBridge — © 2026 Craftprojects Sp. z o.o. (craftprojects.pl)
// Powiadomienie o nowej wersji (updatecheck.h): porównanie wersji, parser odpowiedzi API (także złośliwej
// i obciętej — bez awarii i odczytu poza buforem na x86 i x64), stały adres przycisku „Aktualizuj”
#include "check.h"
#include "updatecheck.h"
#include "resource.h"
#include <cstring>

using namespace sb::update;

namespace {
Version V(const char* s) { Version v; ParseVersion(std::string(s), v); return v; }

// Skrócona, ale realistyczna odpowiedź GET /repos/{owner}/{repo}/releases/latest
const char kSample[] =
    "{\r\n"
    "  \"url\": \"https://api.github.com/repos/craftprojects-sp-zoo/cp-signbridge-official/releases/1\",\n"
    "  \"html_url\": \"https://evil.example/pobierz.exe\",\n"
    "  \"id\": 123456789,\n"
    "  \"author\": { \"login\": \"x\", \"id\": 1, \"site_admin\": false, \"type\": \"User\" },\n"
    "  \"tag_name\": \"v1.0.1\",\n"
    "  \"target_commitish\": \"main\",\n"
    "  \"name\": \"SignBridge 1.0.1 \\u2014 poprawki \\\"bezpiecze\\u0144stwa\\\"\",\n"
    "  \"draft\": false,\n"
    "  \"prerelease\": false,\n"
    "  \"created_at\": \"2026-10-19T09:00:00Z\",\n"
    "  \"assets\": [ { \"name\": \"SignBridge.msi\", \"size\": 1.5e6, \"browser_download_url\": \"https://evil.example/a.msi\", \"uploader\": null },\n"
    "              { \"name\": \"SHA256SUMS.txt\", \"size\": -0.25E-3, \"label\": \"\" } ],\n"
    "  \"body\": \"## Zmiany\\r\\n- a\\tb\\/c\\\\d\\b\\f\",\n"
    "  \"reactions\": { \"+1\": 0, \"nested\": [[[]], {}, [1, 2, {\"a\": [true, false, null]}]] }\n"
    "}\n";

bool Parse(const std::string& s, ReleaseInfo& r) { return ParseRelease(s.data(), s.size(), r); }
}  // namespace

TEST(update_version_parse) {
    Version v;
    CHECK(ParseVersion(std::string("v1.0.1"), v) && v.v[0] == 1 && v.v[1] == 0 && v.v[2] == 1 && v.v[3] == 0);
    CHECK(ParseVersion(std::string("1.1.0.0"), v) && v.v[0] == 1 && v.v[1] == 1);
    CHECK(ParseVersion(std::wstring(L"V2.10.3"), v) && v.v[1] == 10);
    CHECK(ParseVersion(std::string("65535.0.0"), v) && v.v[0] == 65535);
    CHECK(ParseVersion(std::string("7"), v) && v.v[0] == 7 && v.v[1] == 0);
    for (const char* bad : { "", "v", "1.", ".1", "1..2", "1.2.3.4.5", "v1.0.1-rc1", " 1.0.0", "1.0.0 ", "65536.0.0",
                             "123456.0", "1,0,0", "vv1.0", "1.0.x", "-1.0.0", "1.0.0\n", "\xEF\xBC\x91.0.0" }) {
        Version w;
        if (ParseVersion(std::string(bad), w)) { std::printf("  przyjęto błędną wersję: '%s'\n", bad); CHECK(false); }
    }
}

TEST(update_version_compare) {
    CHECK(Compare(V("v1.0.1"), V("1.1.0.0")) < 0);    // 1.0.1 < 1.1.0 — zbudowana 1.1.0 nie pokazuje powiadomienia
    CHECK(Compare(V("v1.0.1"), V("1.0.0.0")) > 0);    // test z wersją lokalną 1.0.0 — powiadomienie
    CHECK(Compare(V("v1.1.0"), V("1.1.0.0")) == 0);
    CHECK(Compare(V("1.10.0"), V("1.9.9")) > 0);       // liczbowo, nie tekstowo
    CHECK(Compare(V("1.1.0.1"), V("1.1.0")) > 0);
    CHECK(Compare(V("2.0"), V("1.65535.65535.65535")) > 0);
    CHECK(ToDisplay(V("v1.0.1")) == L"1.0.1");
    CHECK(ToDisplay(V("1.2.3.4")) == L"1.2.3.4");
}

TEST(update_parse_sample) {
    ReleaseInfo r;
    CHECK(Parse(kSample, r));
    CHECK(r.hasTag && r.tag == "v1.0.1" && !r.draft && !r.prerelease);
    Version newer;
    CHECK(Decide(r, V(SB_VERSION_STR), newer) == Verdict::NotNewer);
    CHECK(Decide(r, V("1.0.0.0"), newer) == Verdict::Newer && ToDisplay(newer) == L"1.0.1");
    CHECK(Decide(r, V("1.0.1.0"), newer) == Verdict::NotNewer);
}

TEST(update_draft_prerelease_ignored) {
    ReleaseInfo r;
    Version n;
    CHECK(Parse("{\"tag_name\":\"v9.0.0\",\"draft\":true,\"prerelease\":false}", r) && r.draft);
    CHECK(Decide(r, V("1.0.0"), n) == Verdict::Ignored);
    CHECK(Parse("{\"prerelease\":true,\"tag_name\":\"v9.0.0\"}", r) && r.prerelease);
    CHECK(Decide(r, V("1.0.0"), n) == Verdict::Ignored);
    CHECK(Parse("{\"tag_name\":\"nightly-2026\"}", r));
    CHECK(Decide(r, V("1.0.0"), n) == Verdict::Ignored);
    CHECK(Parse("{\"tag_name\":\"v9.0.0\"}", r));
    CHECK(Decide(r, V("1.0.0"), n) == Verdict::Newer);
}

TEST(update_parse_rejects_malformed) {
    const char* bad[] = {
        "", " ", "null", "[]", "\"v1.0.0\"", "{", "}", "{}", "{\"draft\":false}",
        "{\"tag_name\":}", "{\"tag_name\":1}", "{\"tag_name\":null}", "{\"tag_name\":[\"v1\"]}",
        "{\"tag_name\":\"v1.0.0\",}", "{\"tag_name\":\"v1.0.0\" \"x\":1}", "{\"tag_name\":\"v1.0.0\"}x",
        "{\"tag_name\":\"v1.0.0\"}{}", "{\"tag_name\":\"v1\",\"tag_name\":\"v9\"}",
        "{\"tag_name\":\"v1\",\"draft\":\"false\"}", "{\"tag_name\":\"v1\",\"draft\":0}", "{\"tag_name\":\"v1\",\"prerelease\":nul}",
        "{\"tag_name\":\"v1\",\"draft\":false,\"draft\":true}",
        "{\"tag_name\":\"v1\\x\"}", "{\"tag_name\":\"v1\\u12\"}", "{\"tag_name\":\"v1\\u12G4\"}", "{\"tag_name\":\"v1\\",
        "{\"tag_name\":\"v1\nx\"}", "{\"tag_name\":\"v1\",\"a\":-}", "{\"tag_name\":\"v1\",\"a\":1.}", "{\"tag_name\":\"v1\",\"a\":1e}",
        "{\"tag_name\":\"v1\",\"a\":[1,]}", "{\"tag_name\":\"v1\",\"a\":{\"b\"}}", "{\"tag_name\":\"v1\",\"a\":{1:2}}",
        "{\"tag_name\":\"v1\",\"a\":tru}", "{\"tag_name\":\"v1\",\"a\":[}", "{tag_name:\"v1\"}", "{'tag_name':'v1'}",
    };
    for (const char* s : bad) {
        ReleaseInfo r;
        if (Parse(s, r)) { std::printf("  przyjęto błędny JSON: %s\n", s); CHECK(false); }
        CHECK(!r.hasTag && r.tag.empty());
    }
    // NUL wewnątrz dokumentu
    std::string nul = "{\"tag_name\":\"v1.0.0\"}"; nul[13] = '\0';
    ReleaseInfo r;
    CHECK(!Parse(nul, r));
    CHECK(!ParseRelease(nullptr, 10, r));
}

TEST(update_parse_limits) {
    ReleaseInfo r;
    std::string tag(kMaxTagLen, '1');
    CHECK(Parse("{\"tag_name\":\"" + tag + "\"}", r) && r.tag.size() == kMaxTagLen);
    CHECK(!Parse("{\"tag_name\":\"" + tag + "1\"}", r));
    CHECK(!Parse("{\"tag_name\":\"" + std::string(100000, 'A') + "\"}", r));
    // bardzo głębokie zagnieżdżenie — odrzucone bez przepełnienia stosu
    std::string deep = "{\"tag_name\":\"v1\",\"a\":" + std::string(200000, '[') + std::string(200000, ']') + "}";
    CHECK(!Parse(deep, r));
    std::string ok = "{\"tag_name\":\"v1\",\"a\":" + std::string(kMaxDepth - 1, '[') + std::string(kMaxDepth - 1, ']') + "}";
    CHECK(Parse(ok, r));
    std::string tooDeep = "{\"tag_name\":\"v1\",\"a\":" + std::string(kMaxDepth, '[') + std::string(kMaxDepth, ']') + "}";
    CHECK(!Parse(tooDeep, r));
    // długie wartości poza interesującymi polami są dozwolone
    CHECK(Parse("{\"body\":\"" + std::string(300000, 'x') + "\",\"tag_name\":\"v2.0.0\",\"n\":" + std::string(5000, '9') + "}", r) && r.tag == "v2.0.0");
    // BOM UTF-8
    CHECK(Parse("\xEF\xBB\xBF{\"tag_name\":\"v1.2.3\"}", r) && r.tag == "v1.2.3");
}

// Proste „fuzzowanie”: każde obcięcie poprawnej odpowiedzi jest odrzucane; podmiana dowolnego bajtu na znaki
// specjalne nie powoduje awarii ani odczytu poza buforem (kopia na stercie dokładnie o długości danych).
TEST(update_parse_fuzz_truncated_and_mutated) {
    const std::string full = kSample;
    const size_t end = full.rfind('}') + 1;   // dalej tylko "\n"
    for (size_t n = 0; n < full.size(); ++n) {
        std::vector<char> buf(full.begin(), full.begin() + n);   // dokładnie n bajtów na stercie
        ReleaseInfo r;
        bool ok = ParseRelease(buf.empty() ? nullptr : buf.data(), buf.size(), r);
        if (ok != (n >= end)) { std::printf("  obcięcie do %u B: wynik %d\n", (unsigned)n, (int)ok); CHECK(false); }
    }
    const char subst[] = { '\0', '"', '\\', '{', '}', '[', ']', ':', ',', 'u', '\n', '\xFF', '\x80', '0', 't' };
    size_t parsed = 0;
    for (size_t i = 0; i < full.size(); ++i) {
        for (char c : subst) {
            std::vector<char> buf(full.begin(), full.end());
            buf[i] = c;
            ReleaseInfo r;
            if (ParseRelease(buf.data(), buf.size(), r)) {
                ++parsed;
                CHECK(r.hasTag && r.tag.size() <= kMaxTagLen);
            }
        }
    }
    CHECK(parsed > 0);   // część podmian (np. w treści tekstu) daje nadal poprawny JSON
    // losowe bajty (stałe ziarno — powtarzalnie)
    unsigned seed = 12345;
    for (int k = 0; k < 20000; ++k) {
        size_t len = (seed = seed * 1103515245u + 12345u) % 64;
        std::vector<char> buf(len ? len : 1);
        for (size_t i = 0; i < len; ++i) buf[i] = "{}[]\":,\\tfnu0123456789.e-v \x01\xFF"[(seed = seed * 1103515245u + 12345u) % 30];
        ReleaseInfo r;
        if (ParseRelease(buf.data(), len, r)) CHECK(r.hasTag && r.tag.size() <= kMaxTagLen);
    }
}

TEST(update_button_url_is_fixed) {
    const wchar_t* expected = L"https://github.com/craftprojects-sp-zoo/cp-signbridge-official/releases/latest";
    CHECK(wcscmp(UpdateButtonUrl(), expected) == 0);
    CHECK(UpdateButtonUrl() == kReleasePageUrl);
    // odpowiedź z innymi adresami (html_url, browser_download_url) nie zmienia adresu przycisku
    ReleaseInfo r;
    CHECK(Parse(kSample, r));
    CHECK(wcscmp(UpdateButtonUrl(), expected) == 0);
    CHECK(wcscmp(kApiHost, L"api.github.com") == 0);
    CHECK(wcscmp(kApiPath, L"/repos/craftprojects-sp-zoo/cp-signbridge-official/releases/latest") == 0);
}

TEST(update_schedule_and_snooze) {
    const ULONGLONG day = 24 * 3600, now = 1800000000ull;
    CHECK(CheckDue(0, now));
    CHECK(!CheckDue(now - day + 60, now));
    CHECK(CheckDue(now - day, now));
    CHECK(CheckDue(now + 3600, now));          // zegar cofnięty
    Version shown;
    Version cur = V("1.1.0.0");
    CHECK(!ShouldShow(L"1.0.1", cur, L"", 0, now, shown));
    CHECK(!ShouldShow(L"1.1.0", cur, L"", 0, now, shown));
    CHECK(!ShouldShow(L"bzdura", cur, L"", 0, now, shown));
    CHECK(ShouldShow(L"1.2.0", cur, L"", 0, now, shown) && ToDisplay(shown) == L"1.2.0");
    CHECK(!ShouldShow(L"1.2.0", cur, L"1.2.0", now + day, now, shown));       // odłożone
    CHECK(ShouldShow(L"1.2.0", cur, L"1.2.0", now - 1, now, shown));          // termin minął
    CHECK(ShouldShow(L"1.3.0", cur, L"1.2.0", now + day, now, shown));        // nowsza niż odłożona
    CHECK(ShouldShow(L"1.2.0", cur, L"1.2.0", now + 365 * day, now, shown));  // zapis spoza zakresu — ignoruj
}
