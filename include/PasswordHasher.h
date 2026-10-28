#ifndef EMS_PASSWORD_HASHER_H
#define EMS_PASSWORD_HASHER_H

#include <cstddef>
#include <string>
#include <vector>

namespace ems {

// Salted, iterated password hashing: PBKDF2-HMAC-SHA256 (RFC 8018 / RFC 2104 / FIPS 180-4),
// Default: built-in implementation of the standardised algorithm (no external dependency, so it builds on
// TDM-GCC 5.1.0). Optional: compile with -DEMS_USE_OPENSSL and link -lcrypto to use OpenSSL instead.
// Both backends produce/accept the same stored format.
// Stored format (no '|' so it is safe inside exam_users.txt):
//     pbkdf2-sha256$<iterations>$<salt hex, 16 bytes>$<hash hex, 32 bytes>
class PasswordHasher {
public:
    static constexpr unsigned kDefaultIterations = 100000;
    static constexpr unsigned kMaxIterations = 10000000;   // DoS guard for hostile files

    static std::string hash(const std::string& password, unsigned iterations = kDefaultIterations);
    static std::string hashWithSalt(const std::string& password, const std::vector<unsigned char>& salt,
                                    unsigned iterations);
    static bool verify(const std::string& password, const std::string& stored);   // never throws
    static bool isWellFormed(const std::string& stored);
    static const char* backendName();                       // which PBKDF2 implementation was compiled in

    // Exposed so the unit tests can check the published test vectors.
    static std::string sha256Hex(const std::string& data);
    static std::string pbkdf2HmacSha256Hex(const std::string& password, const std::string& salt,
                                           unsigned iterations, std::size_t dkLen);
};

} // namespace ems
#endif
