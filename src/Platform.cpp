#include "Platform.h"

#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <vector>

#include "ExamException.h"
#include "Util.h"

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <wincrypt.h>
#include <direct.h>
#include <sys/stat.h>
#include <sys/types.h>
#else
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif

namespace ems {
namespace platform {

#if defined(_WIN32)
const char* const kLineEnding = "\r\n";
#else
const char* const kLineEnding = "\n";
#endif

std::string joinPath(const std::string& directory, const std::string& name) {
    if (directory.empty()) return name;
    const char last = directory[directory.size() - 1];
    if (last == '/' || last == '\\') return directory + name;
    return directory + "/" + name;
}

bool fileExists(const std::string& path) {
#if defined(_WIN32)
    struct _stat st;                                        // underscore names work even with -std=c++14 (strict ANSI)
    if (_stat(path.c_str(), &st) != 0) return false;
    return (st.st_mode & _S_IFMT) == _S_IFREG;
#else
    struct stat st;
    if (::stat(path.c_str(), &st) != 0) return false;
    return S_ISREG(st.st_mode);
#endif
}

bool copyFile(const std::string& from, const std::string& to) {
    std::ifstream in(from.c_str(), std::ios::binary);
    if (!in) return false;
    std::ofstream out(to.c_str(), std::ios::binary | std::ios::trunc);
    if (!out) return false;
    out << in.rdbuf();
    out.flush();
    return static_cast<bool>(out) && !in.bad();
}

bool removeFile(const std::string& path) { return std::remove(path.c_str()) == 0; }

bool replaceFile(const std::string& temp, const std::string& target) {
#if defined(_WIN32)
    return MoveFileExA(temp.c_str(), target.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#else
    return std::rename(temp.c_str(), target.c_str()) == 0;
#endif
}

bool makeDirectory(const std::string& path) {
#if defined(_WIN32)
    return _mkdir(path.c_str()) == 0;
#else
    return ::mkdir(path.c_str(), 0755) == 0;
#endif
}

bool removeDirectory(const std::string& path) {
#if defined(_WIN32)
    return _rmdir(path.c_str()) == 0;
#else
    return ::rmdir(path.c_str()) == 0;
#endif
}

std::string lastErrorText() {
#if defined(_WIN32)
    return "Windows error code " + util::toString(static_cast<unsigned long>(GetLastError())) +
           " (errno: " + std::strerror(errno) + ")";
#else
    return std::strerror(errno);
#endif
}

namespace {
bool tryRandomBytes(unsigned char* buffer, std::size_t length) {
#if defined(_WIN32)
    HCRYPTPROV provider = 0;
    if (!CryptAcquireContextA(&provider, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT | CRYPT_SILENT)) return false;
    const BOOL ok = CryptGenRandom(provider, static_cast<DWORD>(length), buffer);
    CryptReleaseContext(provider, 0);
    return ok != 0;
#else
    std::FILE* f = std::fopen("/dev/urandom", "rb");
    if (!f) return false;
    const std::size_t got = std::fread(buffer, 1, length, f);
    std::fclose(f);
    return got == length;
#endif
}
} // namespace

void secureRandomBytes(unsigned char* buffer, std::size_t length) {
    if (!tryRandomBytes(buffer, length))
        throw FileException("No secure random source is available on this system");
}

std::mt19937 makeSeededEngine() {
    unsigned char raw[32];
    std::vector<unsigned int> words;
    if (tryRandomBytes(raw, sizeof(raw))) {
        for (std::size_t i = 0; i + 4 <= sizeof(raw); i += 4)
            words.push_back(static_cast<unsigned int>(raw[i]) | (static_cast<unsigned int>(raw[i + 1]) << 8) |
                            (static_cast<unsigned int>(raw[i + 2]) << 16) | (static_cast<unsigned int>(raw[i + 3]) << 24));
    } else {                                                // shuffling does not need cryptographic quality
        words.push_back(static_cast<unsigned int>(std::chrono::steady_clock::now().time_since_epoch().count()));
        words.push_back(static_cast<unsigned int>(std::chrono::system_clock::now().time_since_epoch().count()));
        words.push_back(static_cast<unsigned int>(reinterpret_cast<std::size_t>(&words)));
    }
    std::seed_seq seq(words.begin(), words.end());
    return std::mt19937(seq);
}

} // namespace platform
} // namespace ems
