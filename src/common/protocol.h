// SignBridge — © 2026 Craftprojects Sp. z o.o. (craftprojects.pl)
// cp-signbridge — protokół między KSP (serwer) a wtyczką mstsc (komputer użytkownika)
// Jedynym transportem jest kanał wirtualny RDP bieżącej sesji (dvc.h / plugin.cpp).
//
// Ramka:  u32 magic 'SBR1' | u32 cbPayload | u16 type | u16 reserved | payload
// Wszystkie liczby little-endian. Napisy: u32 liczba znaków + UTF-16LE (bez terminatora).
// Bajty: u32 długość + dane.
//
// Przebieg (jedna instancja kanału = jedna operacja, prosto i odpornie):
//   serwer  -> AUTH      { u32 clientVersion, bytes reserved (puste), wstr clientName }
//   wtyczka -> AUTH_OK   { wstr agentUser, wstr agentHost }   albo  ERROR
//   serwer  -> LIST_CERTS | SIGN | PING
//   wtyczka -> CERTS | SIGN_OK | PONG | ERROR
// Bez wyzwania i tokenu — tożsamość daje zalogowana sesja RDP.
//
// Wersja protokołu: zmieniać PROTO_VERSION tylko przy niezgodnych zmianach; nowe pola
// dopisywać na końcu komunikatów (parser ignoruje nadmiar).
#pragma once
#include <windows.h>
#include <string>
#include <vector>
#include <cstdint>

namespace sb { namespace proto {

const uint32_t kMagic = 0x31524253;  // "SBR1"
const uint32_t PROTO_VERSION = 1;
const uint32_t kMaxPayload = 4 * 1024 * 1024;

enum MsgType : uint16_t {
    // 1 — zarezerwowany (nieużywany)
    AUTH = 2,
    AUTH_OK = 3,
    LIST_CERTS = 5,
    CERTS = 6,
    SIGN = 7,
    SIGN_OK = 8,
    ERR = 9,
    PING = 10,
    PONG = 11,
};

enum Padding : uint8_t {
    PAD_NONE = 0,   // ECDSA / surowe
    PAD_PKCS1 = 1,
    PAD_PSS = 2,
};

#pragma pack(push, 1)
struct FrameHdr {
    uint32_t magic;
    uint32_t cbPayload;
    uint16_t type;
    uint16_t reserved;
};
#pragma pack(pop)

// --- budowanie ---
class Writer {
public:
    void u8(uint8_t v) { buf_.push_back(v); }
    void u32(uint32_t v) { for (int i = 0; i < 4; ++i) buf_.push_back((uint8_t)(v >> (8 * i))); }
    void bytes(const void* p, uint32_t n) { u32(n); const uint8_t* b = (const uint8_t*)p; buf_.insert(buf_.end(), b, b + n); }
    void bytes(const std::vector<uint8_t>& v) { bytes(v.data(), (uint32_t)v.size()); }
    void wstr(const std::wstring& s) { u32((uint32_t)s.size()); for (wchar_t c : s) { buf_.push_back((uint8_t)c); buf_.push_back((uint8_t)(c >> 8)); } }
    const std::vector<uint8_t>& data() const { return buf_; }
private:
    std::vector<uint8_t> buf_;
};

// --- parsowanie z kontrolą granic ---
class Reader {
public:
    Reader(const uint8_t* p, size_t n) : p_(p), n_(n), i_(0), ok_(true) {}
    bool ok() const { return ok_; }
    uint8_t u8() { if (i_ + 1 > n_) { ok_ = false; return 0; } return p_[i_++]; }
    uint32_t u32() {
        if (i_ + 4 > n_) { ok_ = false; return 0; }
        uint32_t v = 0; for (int k = 0; k < 4; ++k) v |= (uint32_t)p_[i_ + k] << (8 * k);
        i_ += 4; return v;
    }
    std::vector<uint8_t> bytes() {
        uint32_t n = u32();
        if (!ok_ || i_ + n > n_) { ok_ = false; return {}; }
        std::vector<uint8_t> v(p_ + i_, p_ + i_ + n); i_ += n; return v;
    }
    std::wstring wstr() {
        uint32_t n = u32();
        if (!ok_ || i_ + (size_t)n * 2 > n_) { ok_ = false; return L""; }
        std::wstring s; s.reserve(n);
        for (uint32_t k = 0; k < n; ++k) { s.push_back((wchar_t)(p_[i_] | (p_[i_ + 1] << 8))); i_ += 2; }
        return s;
    }
private:
    const uint8_t* p_; size_t n_, i_; bool ok_;
};

// --- komunikaty ---
struct Auth { uint32_t clientVersion; std::vector<uint8_t> reserved; std::wstring clientName; };  // reserved: zawsze puste (zgodność formatu)
struct AuthOk { std::wstring agentUser; std::wstring agentHost; };
struct CertEntry { std::vector<uint8_t> der; };
struct Certs { std::vector<CertEntry> items; };
struct SignReq { std::wstring thumbprint; uint8_t padding; std::wstring hashAlg; uint32_t saltLen; std::vector<uint8_t> hash; };
struct SignOk { std::vector<uint8_t> signature; };
struct Error { uint32_t code; std::wstring message; };

inline std::vector<uint8_t> EncodeAuth(const Auth& m) { Writer w; w.u32(m.clientVersion); w.bytes(m.reserved); w.wstr(m.clientName); return w.data(); }
inline bool DecodeAuth(const std::vector<uint8_t>& b, Auth& m) { Reader r(b.data(), b.size()); m.clientVersion = r.u32(); m.reserved = r.bytes(); m.clientName = r.wstr(); return r.ok(); }

inline std::vector<uint8_t> EncodeAuthOk(const AuthOk& m) { Writer w; w.wstr(m.agentUser); w.wstr(m.agentHost); return w.data(); }
inline bool DecodeAuthOk(const std::vector<uint8_t>& b, AuthOk& m) { Reader r(b.data(), b.size()); m.agentUser = r.wstr(); m.agentHost = r.wstr(); return r.ok(); }

inline std::vector<uint8_t> EncodeCerts(const Certs& m) { Writer w; w.u32((uint32_t)m.items.size()); for (auto& c : m.items) w.bytes(c.der); return w.data(); }
inline bool DecodeCerts(const std::vector<uint8_t>& b, Certs& m) {
    Reader r(b.data(), b.size()); uint32_t n = r.u32(); if (!r.ok() || n > 10000) return false;
    for (uint32_t i = 0; i < n && r.ok(); ++i) { CertEntry e; e.der = r.bytes(); m.items.push_back(e); }
    return r.ok();
}

inline std::vector<uint8_t> EncodeSign(const SignReq& m) { Writer w; w.wstr(m.thumbprint); w.u8(m.padding); w.wstr(m.hashAlg); w.u32(m.saltLen); w.bytes(m.hash); return w.data(); }
inline bool DecodeSign(const std::vector<uint8_t>& b, SignReq& m) { Reader r(b.data(), b.size()); m.thumbprint = r.wstr(); m.padding = r.u8(); m.hashAlg = r.wstr(); m.saltLen = r.u32(); m.hash = r.bytes(); return r.ok(); }

inline std::vector<uint8_t> EncodeSignOk(const SignOk& m) { Writer w; w.bytes(m.signature); return w.data(); }
inline bool DecodeSignOk(const std::vector<uint8_t>& b, SignOk& m) { Reader r(b.data(), b.size()); m.signature = r.bytes(); return r.ok(); }

inline std::vector<uint8_t> EncodeError(const Error& m) { Writer w; w.u32(m.code); w.wstr(m.message); return w.data(); }
inline bool DecodeError(const std::vector<uint8_t>& b, Error& m) { Reader r(b.data(), b.size()); m.code = r.u32(); m.message = r.wstr(); return r.ok(); }

}}  // namespace sb::proto
