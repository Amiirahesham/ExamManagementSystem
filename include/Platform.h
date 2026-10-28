#ifndef EMS_PLATFORM_H
#define EMS_PLATFORM_H

#include <cstddef>
#include <random>
#include <string>

namespace ems {
namespace platform {

// Thin portability layer used instead of std::filesystem (C++17) so the project builds with
// TDM-GCC 5.1.0 / MinGW using -std=c++14. Paths may use '/' on Windows too.

std::string joinPath(const std::string& directory, const std::string& name);
bool fileExists(const std::string& path);                 // true only for regular files
bool copyFile(const std::string& from, const std::string& to);   // overwrites `to`
bool removeFile(const std::string& path);                 // false if it did not exist / failed
// Atomically replaces `target` with `temp` (Windows: MoveFileEx REPLACE_EXISTING; POSIX: rename).
bool replaceFile(const std::string& temp, const std::string& target);
bool makeDirectory(const std::string& path);              // single level; true if created
bool removeDirectory(const std::string& path);            // must be empty
std::string lastErrorText();                              // description of the last failed call

extern const char* const kLineEnding;                     // "\r\n" on Windows, "\n" elsewhere

// Cryptographically secure random bytes (CryptGenRandom on Windows, /dev/urandom elsewhere).
// std::random_device is NOT used: on MinGW before GCC 9.2 it is deterministic.
// Throws FileException if no secure source is available.
void secureRandomBytes(unsigned char* buffer, std::size_t length);
// Mersenne Twister seeded from secureRandomBytes (falls back to clocks if it fails).
std::mt19937 makeSeededEngine();

} // namespace platform
} // namespace ems
#endif
