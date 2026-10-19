// SignBridge — © 2026 Craftprojects Sp. z o.o. (craftprojects.pl)
// Parser protokołu (protocol.h): granice dla długości z ramki, także na x86 (32-bitowy size_t)
#include "check.h"
#include "protocol.h"

using namespace sb::proto;

namespace {
std::vector<uint8_t> LenThenData(uint32_t len, size_t dataBytes) {
    Writer w; w.u32(len);
    std::vector<uint8_t> b = w.data();
    b.resize(b.size() + dataBytes, 0x41);
    return b;
}
}  // namespace

TEST(protocol_roundtrip_sign) {
    SignReq in; in.thumbprint = L"00112233445566778899AABBCCDDEEFF00112233"; in.padding = PAD_PSS; in.hashAlg = L"SHA256"; in.saltLen = 32;
    in.hash.assign(32, 0x5A);
    SignReq out = {};
    CHECK(DecodeSign(EncodeSign(in), out));
    CHECK(out.thumbprint == in.thumbprint && out.padding == in.padding && out.hashAlg == in.hashAlg && out.saltLen == 32 && out.hash == in.hash);
}

// i_ + n zawijało się na x86 dla n bliskiego 0xFFFFFFFF -> odczyt poza buforem
TEST(protocol_bytes_huge_length_rejected) {
    for (uint32_t n : { 0xFFFFFFFFu, 0xFFFFFFFDu, 0x80000000u, 5u }) {
        std::vector<uint8_t> b = LenThenData(n, 4);
        Reader r(b.data(), b.size());
        std::vector<uint8_t> v = r.bytes();
        CHECK(!r.ok());
        CHECK(v.empty());
    }
}

// (size_t)n * 2 zawijało się na x86 dla n >= 0x80000000
TEST(protocol_wstr_huge_length_rejected) {
    for (uint32_t n : { 0x80000002u, 0x80000000u, 0xFFFFFFFFu, 3u }) {
        std::vector<uint8_t> b = LenThenData(n, 4);
        Reader r(b.data(), b.size());
        std::wstring s = r.wstr();
        CHECK(!r.ok());
        CHECK(s.empty());
    }
}

TEST(protocol_exact_and_truncated) {
    std::vector<uint8_t> b = LenThenData(2, 4);   // 2 znaki = 4 bajty — dokładnie mieści się
    Reader r(b.data(), b.size());
    CHECK(r.wstr().size() == 2 && r.ok());
    CHECK(r.u8() == 0 && !r.ok());               // nic nie zostało
    const uint8_t three[3] = { 1, 2, 3 };
    Reader r2(three, 3);
    r2.u32();
    CHECK(!r2.ok());
    Auth a = {};
    std::vector<uint8_t> enc = EncodeAuth({ PROTO_VERSION, {}, L"serwer" });
    enc.pop_back();
    CHECK(!DecodeAuth(enc, a));
}

TEST(protocol_certs_count_limit) {
    Writer w; w.u32(10001);
    Certs c;
    CHECK(!DecodeCerts(w.data(), c));
}
