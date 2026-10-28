#include "PasswordHasher.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#ifdef EMS_USE_OPENSSL
#include <openssl/evp.h>
#endif

#include "ExamException.h"
#include "Platform.h"
#include "Util.h"

namespace ems {

// C++14 requires an out-of-class definition for ODR-used static constexpr members.
constexpr unsigned PasswordHasher::kDefaultIterations;
constexpr unsigned PasswordHasher::kMaxIterations;

namespace {

const std::uint32_t kK[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};

inline std::uint32_t rotr(std::uint32_t x, int n) { return (x >> n) | (x << (32 - n)); }

class Sha256 {
public:
    Sha256() { reset(); }
    void reset() {
        h_ = {{0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19}};
        total_ = 0;
        bufLen_ = 0;
    }
    void update(const unsigned char* data, std::size_t len) {
        total_ += len;
        while (len > 0) {
            const std::size_t take = std::min<std::size_t>(64 - bufLen_, len);
            std::memcpy(buf_ + bufLen_, data, take);
            bufLen_ += take; data += take; len -= take;
            if (bufLen_ == 64) { transform(buf_); bufLen_ = 0; }
        }
    }
    std::array<unsigned char, 32> finish() {
        const std::uint64_t bits = total_ * 8;
        buf_[bufLen_++] = 0x80;
        if (bufLen_ > 56) {
            std::memset(buf_ + bufLen_, 0, 64 - bufLen_);
            transform(buf_);
            bufLen_ = 0;
        }
        std::memset(buf_ + bufLen_, 0, 56 - bufLen_);
        for (int i = 0; i < 8; ++i) buf_[56 + i] = static_cast<unsigned char>(bits >> (56 - 8 * i));
        transform(buf_);
        std::array<unsigned char, 32> out{};
        for (int i = 0; i < 8; ++i)
            for (int j = 0; j < 4; ++j) out[i * 4 + j] = static_cast<unsigned char>(h_[i] >> (24 - 8 * j));
        return out;
    }
private:
    void transform(const unsigned char* block) {
        std::uint32_t w[64];
        for (int i = 0; i < 16; ++i)
            w[i] = (std::uint32_t(block[i * 4]) << 24) | (std::uint32_t(block[i * 4 + 1]) << 16) |
                   (std::uint32_t(block[i * 4 + 2]) << 8) | std::uint32_t(block[i * 4 + 3]);
        for (int i = 16; i < 64; ++i) {
            const std::uint32_t s0 = rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
            const std::uint32_t s1 = rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
            w[i] = w[i - 16] + s0 + w[i - 7] + s1;
        }
        std::uint32_t a = h_[0], b = h_[1], c = h_[2], d = h_[3], e = h_[4], f = h_[5], g = h_[6], h = h_[7];
        for (int i = 0; i < 64; ++i) {
            const std::uint32_t S1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
            const std::uint32_t ch = (e & f) ^ (~e & g);
            const std::uint32_t t1 = h + S1 + ch + kK[i] + w[i];
            const std::uint32_t S0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
            const std::uint32_t mj = (a & b) ^ (a & c) ^ (b & c);
            const std::uint32_t t2 = S0 + mj;
            h = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2;
        }
        h_[0] += a; h_[1] += b; h_[2] += c; h_[3] += d; h_[4] += e; h_[5] += f; h_[6] += g; h_[7] += h;
    }

    std::array<std::uint32_t, 8> h_{};
    unsigned char buf_[64] = {0};
    std::size_t bufLen_ = 0;
    std::uint64_t total_ = 0;
};

#ifdef EMS_USE_OPENSSL
// Optional backend (-DEMS_USE_OPENSSL, link -lcrypto): OpenSSL's PKCS5_PBKDF2_HMAC. Same output format.
std::vector<unsigned char> pbkdf2(const std::string& password, const std::vector<unsigned char>& salt,
                                  unsigned iterations, std::size_t dkLen) {
    std::vector<unsigned char> out(dkLen);
    if (PKCS5_PBKDF2_HMAC(password.data(), static_cast<int>(password.size()), salt.data(),
                          static_cast<int>(salt.size()), static_cast<int>(iterations), EVP_sha256(),
                          static_cast<int>(dkLen), out.data()) != 1)
        throw ExamException("OpenSSL PKCS5_PBKDF2_HMAC failed");
    return out;
}
#else
class HmacSha256 {
public:
    HmacSha256(const unsigned char* key, std::size_t keyLen) {
        unsigned char k[64] = {0};
        if (keyLen > 64) {
            Sha256 s;
            s.update(key, keyLen);
            const auto d = s.finish();
            std::memcpy(k, d.data(), d.size());
        } else if (keyLen > 0) {
            std::memcpy(k, key, keyLen);
        }
        unsigned char ipad[64], opad[64];
        for (int i = 0; i < 64; ++i) { ipad[i] = k[i] ^ 0x36; opad[i] = k[i] ^ 0x5c; }
        inner_.update(ipad, 64);
        outer_.update(opad, 64);
    }
    std::array<unsigned char, 32> compute(const unsigned char* msg, std::size_t len) const {
        Sha256 in = inner_;
        in.update(msg, len);
        const auto innerDigest = in.finish();
        Sha256 out = outer_;
        out.update(innerDigest.data(), innerDigest.size());
        return out.finish();
    }
private:
    Sha256 inner_, outer_;
};

std::vector<unsigned char> pbkdf2(const std::string& password, const std::vector<unsigned char>& salt,
                                  unsigned iterations, std::size_t dkLen) {
    const HmacSha256 mac(reinterpret_cast<const unsigned char*>(password.data()), password.size());
    std::vector<unsigned char> out;
    out.reserve(dkLen);
    for (std::uint32_t block = 1; out.size() < dkLen; ++block) {
        std::vector<unsigned char> msg(salt);
        for (int i = 3; i >= 0; --i) msg.push_back(static_cast<unsigned char>(block >> (8 * i)));
        auto u = mac.compute(msg.data(), msg.size());
        auto t = u;
        for (unsigned i = 1; i < iterations; ++i) {
            u = mac.compute(u.data(), u.size());
            for (std::size_t k = 0; k < t.size(); ++k) t[k] ^= u[k];
        }
        const std::size_t take = std::min<std::size_t>(t.size(), dkLen - out.size());
        out.insert(out.end(), t.begin(), t.begin() + take);
    }
    return out;
}

#endif

std::string toHex(const unsigned char* data, std::size_t len) {
    static const char* digits = "0123456789abcdef";
    std::string s;
    s.reserve(len * 2);
    for (std::size_t i = 0; i < len; ++i) { s += digits[data[i] >> 4]; s += digits[data[i] & 15]; }
    return s;
}

bool fromHex(const std::string& hex, std::vector<unsigned char>& out) {
    if (hex.empty() || hex.size() % 2 != 0) return false;
    out.clear();
    auto val = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    };
    for (std::size_t i = 0; i < hex.size(); i += 2) {
        const int hi = val(hex[i]), lo = val(hex[i + 1]);
        if (hi < 0 || lo < 0) return false;
        out.push_back(static_cast<unsigned char>(hi * 16 + lo));
    }
    return true;
}

const char* const kScheme = "pbkdf2-sha256";
const std::size_t kSaltBytes = 16, kHashBytes = 32;

// Splits a stored hash; returns false if malformed.
bool parseStored(const std::string& stored, unsigned& iterations, std::vector<unsigned char>& salt,
                 std::vector<unsigned char>& digest) {
    const auto parts = util::split(stored, '$');
    if (parts.size() != 4 || parts[0] != kScheme) return false;
    if (parts[1].empty() || parts[1].size() > 8) return false;
    for (char c : parts[1]) if (c < '0' || c > '9') return false;
    unsigned long it = 0;
    for (char c : parts[1]) it = it * 10 + static_cast<unsigned long>(c - '0');
    if (it < 1 || it > PasswordHasher::kMaxIterations) return false;
    iterations = static_cast<unsigned>(it);
    if (!fromHex(parts[2], salt) || salt.size() != kSaltBytes) return false;
    if (!fromHex(parts[3], digest) || digest.size() != kHashBytes) return false;
    return true;
}

} // namespace

std::string PasswordHasher::hashWithSalt(const std::string& password, const std::vector<unsigned char>& salt,
                                         unsigned iterations) {
    const auto digest = pbkdf2(password, salt, iterations, kHashBytes);
    return std::string(kScheme) + "$" + util::toString(iterations) + "$" + toHex(salt.data(), salt.size()) +
           "$" + toHex(digest.data(), digest.size());
}

std::string PasswordHasher::hash(const std::string& password, unsigned iterations) {
    std::vector<unsigned char> salt(kSaltBytes);
    platform::secureRandomBytes(salt.data(), salt.size());   // CryptGenRandom / /dev/urandom (never std::random_device)
    return hashWithSalt(password, salt, iterations);
}

bool PasswordHasher::verify(const std::string& password, const std::string& stored) {
    unsigned iterations = 0;
    std::vector<unsigned char> salt, expected;
    if (!parseStored(stored, iterations, salt, expected)) return false;
    const auto actual = pbkdf2(password, salt, iterations, kHashBytes);
    unsigned char diff = 0;   // constant-time comparison
    for (std::size_t i = 0; i < kHashBytes; ++i) diff |= static_cast<unsigned char>(actual[i] ^ expected[i]);
    return diff == 0;
}

bool PasswordHasher::isWellFormed(const std::string& stored) {
    unsigned iterations = 0;
    std::vector<unsigned char> salt, digest;
    return parseStored(stored, iterations, salt, digest);
}

const char* PasswordHasher::backendName() {
#ifdef EMS_USE_OPENSSL
    return "OpenSSL PKCS5_PBKDF2_HMAC (SHA-256)";
#else
    return "built-in PBKDF2-HMAC-SHA256 (RFC 8018), no external dependency";
#endif
}

std::string PasswordHasher::sha256Hex(const std::string& data) {
    Sha256 s;
    s.update(reinterpret_cast<const unsigned char*>(data.data()), data.size());
    const auto d = s.finish();
    return toHex(d.data(), d.size());
}

std::string PasswordHasher::pbkdf2HmacSha256Hex(const std::string& password, const std::string& salt,
                                                unsigned iterations, std::size_t dkLen) {
    const std::vector<unsigned char> saltBytes(salt.begin(), salt.end());
    const auto d = pbkdf2(password, saltBytes, iterations, dkLen);
    return toHex(d.data(), d.size());
}

} // namespace ems
