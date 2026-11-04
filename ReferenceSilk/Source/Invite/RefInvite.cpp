// Reference · Invite — see RefInvite.h
#include "RefInvite.h"

#include <algorithm>
#include <cstdio>
#include <sstream>

namespace refinv
{
namespace
{
// ---------------------------------------------------------------- SHA-256
struct Sha256
{
    uint32_t h[8] = { 0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19 };
    uint8_t buf[64] {};
    size_t used = 0;
    uint64_t bits = 0;

    static uint32_t rotr (uint32_t x, int n) { return (x >> n) | (x << (32 - n)); }

    void block (const uint8_t* p)
    {
        static const uint32_t k[64] = {
            0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5, 0xd807aa98, 0x12835b01, 0x243185be,
            0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa,
            0x5cb0a9dc, 0x76f988da, 0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967, 0x27b70a85,
            0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85, 0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
            0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070, 0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f,
            0x682e6ff3, 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
        };
        uint32_t w[64];
        for (int i = 0; i < 16; ++i)
            w[i] = (uint32_t) p[i * 4] << 24 | (uint32_t) p[i * 4 + 1] << 16 | (uint32_t) p[i * 4 + 2] << 8 | (uint32_t) p[i * 4 + 3];
        for (int i = 16; i < 64; ++i)
        {
            const uint32_t s0 = rotr (w[i - 15], 7) ^ rotr (w[i - 15], 18) ^ (w[i - 15] >> 3);
            const uint32_t s1 = rotr (w[i - 2], 17) ^ rotr (w[i - 2], 19) ^ (w[i - 2] >> 10);
            w[i] = w[i - 16] + s0 + w[i - 7] + s1;
        }
        uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4], f = h[5], g = h[6], hh = h[7];
        for (int i = 0; i < 64; ++i)
        {
            const uint32_t S1 = rotr (e, 6) ^ rotr (e, 11) ^ rotr (e, 25);
            const uint32_t ch = (e & f) ^ (~e & g);
            const uint32_t t1 = hh + S1 + ch + k[i] + w[i];
            const uint32_t S0 = rotr (a, 2) ^ rotr (a, 13) ^ rotr (a, 22);
            const uint32_t mj = (a & b) ^ (a & c) ^ (b & c);
            const uint32_t t2 = S0 + mj;
            hh = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2;
        }
        h[0] += a; h[1] += b; h[2] += c; h[3] += d; h[4] += e; h[5] += f; h[6] += g; h[7] += hh;
    }

    void add (const uint8_t* p, size_t n)
    {
        bits += (uint64_t) n * 8;
        while (n > 0)
        {
            const size_t take = std::min (n, 64 - used);
            std::copy (p, p + take, buf + used);
            used += take; p += take; n -= take;
            if (used == 64) { block (buf); used = 0; }
        }
    }

    std::vector<uint8_t> finish()
    {
        const uint64_t total = bits;
        const uint8_t one = 0x80, zero = 0;
        add (&one, 1);
        while (used != 56) add (&zero, 1);
        uint8_t len[8];
        for (int i = 0; i < 8; ++i) len[i] = (uint8_t) (total >> (56 - 8 * i));
        add (len, 8);
        std::vector<uint8_t> out (32);
        for (int i = 0; i < 8; ++i)
            for (int j = 0; j < 4; ++j) out[(size_t) (i * 4 + j)] = (uint8_t) (h[i] >> (24 - 8 * j));
        return out;
    }
};

std::vector<uint8_t> sha256 (const uint8_t* p, size_t n)
{
    Sha256 s;
    s.add (p, n);
    return s.finish();
}

std::string toHex (const std::vector<uint8_t>& v)
{
    static const char* d = "0123456789abcdef";
    std::string s;
    for (auto b : v) { s += d[b >> 4]; s += d[b & 15]; }
    return s;
}

const char* kAlphabet = "23456789ABCDEFGHJKLMNPQRSTUVWXYZ"; // no 0 O 1 I

std::string base32 (const std::vector<uint8_t>& v, size_t chars)
{
    std::string s;
    uint32_t acc = 0; int have = 0; size_t i = 0;
    while (s.size() < chars && (i < v.size() || have > 0))
    {
        if (have < 5 && i < v.size()) { acc = (acc << 8) | v[i++]; have += 8; }
        if (have < 5) break;
        s += kAlphabet[(acc >> (have - 5)) & 31];
        have -= 5;
    }
    return s;
}

// The signing key, split in two so it never sits in the binary as one block.
const uint8_t kA[32] = { 0x7b, 0xff, 0x45, 0x45, 0x13, 0xfe, 0xe9, 0x39, 0x15, 0x19, 0x4f, 0xf2, 0x6e, 0x75, 0xc9, 0xbf,
                         0xd3, 0xe4, 0xce, 0x0c, 0x3b, 0x89, 0x84, 0x85, 0xb6, 0xc4, 0xca, 0xf5, 0xb9, 0xb7, 0xa9, 0x12 };
const uint8_t kB[32] = { 0x84, 0x0a, 0x96, 0xb9, 0x72, 0x08, 0x59, 0x17, 0x35, 0xf8, 0x4d, 0xa9, 0x94, 0xfd, 0x37, 0x0f,
                         0x74, 0x9e, 0x7d, 0xfe, 0x97, 0xd0, 0x0d, 0x1c, 0xd7, 0x59, 0x4c, 0x35, 0xdd, 0x63, 0x38, 0x5f };

std::vector<uint8_t> key()
{
    std::vector<uint8_t> k (32);
    for (size_t i = 0; i < 32; ++i) k[i] = (uint8_t) (kA[i] ^ kB[i]);
    return k;
}

bool readInt (const std::string& s, int64_t& out)
{
    if (s.empty() || s.size() > 19) return false;
    int64_t v = 0;
    for (char c : s)
    {
        if (c < '0' || c > '9') return false;
        v = v * 10 + (c - '0');
    }
    out = v;
    return true;
}
} // namespace

// ---------------------------------------------------------------- public helpers
std::string sha256Hex (const std::string& data) { return toHex (sha256 ((const uint8_t*) data.data(), data.size())); }

std::vector<uint8_t> hmacSha256 (const std::vector<uint8_t>& keyIn, const std::string& msg)
{
    std::vector<uint8_t> k = keyIn;
    if (k.size() > 64) k = sha256 (k.data(), k.size());
    k.resize (64, 0);
    std::vector<uint8_t> inner (64), outer (64);
    for (size_t i = 0; i < 64; ++i) { inner[i] = (uint8_t) (k[i] ^ 0x36); outer[i] = (uint8_t) (k[i] ^ 0x5c); }
    inner.insert (inner.end(), msg.begin(), msg.end());
    auto ih = sha256 (inner.data(), inner.size());
    outer.insert (outer.end(), ih.begin(), ih.end());
    return sha256 (outer.data(), outer.size());
}

std::string normalise (const std::string& code)
{
    std::string s;
    for (char c : code)
    {
        if (c >= 'a' && c <= 'z') c = (char) (c - 'a' + 'A');
        for (const char* a = kAlphabet; *a; ++a)
            if (*a == c) { s += c; break; }
    }
    return s;
}

std::string pretty (const std::string& code)
{
    const auto n = normalise (code);
    if (n.size() == 9) return n.substr (0, 3) + "-" + n.substr (3, 4) + "-" + n.substr (7);
    if (n.size() == 10) return n.substr (0, 5) + "-" + n.substr (5);
    return n;
}

// ---------------------------------------------------------------- Invite
Invite::Invite (const std::string& deviceId, const std::string& productId, const std::string& prefixIn)
    : prefix (normalise (prefixIn))
{
    cid = sha256Hex (deviceId + "|" + productId).substr (0, 24);
    invite = inviteFor (cid);
    myTag = tagFor (cid);
}

std::string Invite::mac (const std::string& msg, size_t chars) const { return base32 (hmacSha256 (key(), msg), chars); }
std::string Invite::inviteFor (const std::string& c) const { return prefix + mac ("invite|" + prefix + "|" + c, 6); }
std::string Invite::tagFor (const std::string& c) const { return mac ("tag|" + prefix + "|" + c, 4); }
std::string Invite::thanksFor (const std::string& inv, const std::string& t) const { return mac ("thanks|" + normalise (inv) + "|" + t, 6); }
std::string Invite::unlockFor (const std::string& c) const { return toHex (hmacSha256 (key(), "unlocked|" + prefix + "|" + c)).substr (0, 32); }

std::string Invite::save() const
{
    std::ostringstream b;
    b << "v=1\nstart=" << state.start << "\nlast=" << state.last << "\ngave=" << state.gaveFor << "\nunlock=" << state.unlock << "\nthanks=";
    for (size_t i = 0; i < state.thanks.size(); ++i) b << (i ? "," : "") << state.thanks[i];
    b << "\n";
    const auto body = b.str();
    return body + "sig=" + toHex (hmacSha256 (key(), "state|" + cid + "|" + body)).substr (0, 32) + "\n";
}

bool Invite::parse (const std::string& text, State& out) const
{
    const auto at = text.find ("sig=");
    if (at == std::string::npos) return false;
    const auto body = text.substr (0, at);
    auto sig = text.substr (at + 4);
    while (! sig.empty() && (sig.back() == '\n' || sig.back() == '\r' || sig.back() == ' ')) sig.pop_back();
    if (sig != toHex (hmacSha256 (key(), "state|" + cid + "|" + body)).substr (0, 32)) return false; // edited, or copied from another computer

    State s;
    std::istringstream in (body);
    std::string line;
    bool haveStart = false;
    while (std::getline (in, line))
    {
        const auto eq = line.find ('=');
        if (eq == std::string::npos) continue;
        const auto k = line.substr (0, eq), v = line.substr (eq + 1);
        if (k == "start") haveStart = readInt (v, s.start);
        else if (k == "last") readInt (v, s.last);
        else if (k == "gave") s.gaveFor = normalise (v);
        else if (k == "unlock") s.unlock = v;
        else if (k == "thanks")
        {
            std::istringstream t (v);
            std::string one;
            while (std::getline (t, one, ','))
                if (! one.empty()) s.thanks.push_back (normalise (one));
        }
    }
    if (! haveStart) return false;
    out = s;
    return true;
}

void Invite::load (const std::vector<std::string>& copies, int64_t nowIn)
{
    std::vector<State> good;
    tampered = false;
    for (const auto& c : copies)
    {
        if (c.empty()) continue;
        State s;
        if (parse (c, s)) good.push_back (s);
        else tampered = true;
    }

    State m;
    if (good.empty())
    {
        m.start = tampered ? nowIn - (int64_t) kTrialDays * kDay - 1 : nowIn; // a broken file never gives a new trial
        m.last = nowIn;
    }
    else
    {
        m = good[0];
        for (size_t i = 1; i < good.size(); ++i)
        {
            const auto& g = good[i];
            m.start = std::min (m.start, g.start);
            m.last = std::max (m.last, g.last);
            if (m.gaveFor.empty()) m.gaveFor = g.gaveFor;
            if (m.unlock != unlockFor (cid)) m.unlock = g.unlock;
            for (const auto& t : g.thanks)
                if (std::find (m.thanks.begin(), m.thanks.end(), t) == m.thanks.end()) m.thanks.push_back (t);
        }
    }
    now = std::max (nowIn, m.last); // clock moved back: time does not go backwards
    m.last = now;
    m.start = std::min (m.start, now);
    state = m;
}

bool Invite::unlocked() const { return state.unlock == unlockFor (cid); }

int Invite::daysLeft() const
{
    const int64_t remaining = (int64_t) kTrialDays * kDay - (now - state.start);
    if (remaining <= 0) return 0;
    return (int) ((remaining + kDay - 1) / kDay);
}

GiveResult Invite::giveThanks (const std::string& friendInvite, std::string& thanksOut)
{
    const auto n = normalise (friendInvite);
    if (n.empty()) return GiveResult::empty;
    if (n.size() != prefix.size() + 6 || n.compare (0, prefix.size(), prefix) != 0) return GiveResult::badFormat;
    if (n == invite) return GiveResult::ownCode;
    if (! state.gaveFor.empty() && state.gaveFor != n)
    {
        thanksOut = pretty (myTag + thanksFor (state.gaveFor, myTag)); // the one code this install already made
        return GiveResult::alreadyGaveOther;
    }
    state.gaveFor = n;
    thanksOut = pretty (myTag + thanksFor (n, myTag));
    return GiveResult::ok;
}

RedeemResult Invite::redeem (const std::string& code)
{
    const auto n = normalise (code);
    if (n.empty()) return RedeemResult::empty;
    if (n.size() != 10) return RedeemResult::badFormat;
    const auto t = n.substr (0, 4);
    if (t == myTag) return RedeemResult::ownCode;
    if (n.substr (4) != thanksFor (invite, t)) return RedeemResult::wrongCode;
    for (const auto& have : state.thanks)
        if (have.substr (0, 4) == t)
        {
            if (unlocked()) return RedeemResult::ok;
            return RedeemResult::alreadyUsed;
        }
    state.thanks.push_back (n);
    if ((int) state.thanks.size() >= kInvitesNeeded) state.unlock = unlockFor (cid);
    return RedeemResult::ok;
}
} // namespace refinv
