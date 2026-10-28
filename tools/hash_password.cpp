// Prints a salted PBKDF2-HMAC-SHA256 hash for the 5th column of exam_users.txt.
// Usage: hash_password <password> [iterations]     (default 100000; allowed 1000..10000000)
#include <iostream>

#include "ExamException.h"
#include "PasswordHasher.h"
#include "Util.h"

int main(int argc, char** argv) {
    if (argc < 2 || argc > 3) { std::cerr << "Usage: " << argv[0] << " <password> [iterations]\n"; return 64; }
    try {
        unsigned iterations = ems::PasswordHasher::kDefaultIterations;
        if (argc == 3) {
            const int n = ems::util::parseInt(argv[2], "iterations");
            if (n < 1000 || n > static_cast<int>(ems::PasswordHasher::kMaxIterations)) {
                std::cerr << "iterations must be between 1000 and 10000000\n";
                return 64;
            }
            iterations = static_cast<unsigned>(n);
        }
        std::cout << ems::PasswordHasher::hash(argv[1], iterations) << '\n';
    } catch (const ems::ExamException& e) {
        std::cerr << "Error: " << e.what() << '\n';
        return 1;
    }
    return 0;
}
