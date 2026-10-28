// Examination Management System - Student console application.
// Usage: exam_student [--data <directory>]     (default directory: ./data)
#include <iostream>
#include <random>
#include <string>

#include "AppContext.h"
#include "ExamException.h"
#include "ExamManager.h"
#include "ExamTimer.h"
#include "FileManager.h"
#include "Platform.h"
#include "Student.h"

#if defined(__unix__) || defined(__APPLE__)
#include <termios.h>
#include <unistd.h>
#elif defined(_WIN32)
#include <conio.h>
#include <windows.h>
#endif

namespace {

// Interactive terminal: Windows shows "*" per key, POSIX hides the input; plain getline otherwise (pipes, tests).
bool readPassword(const std::string& prompt, std::string& password) {
    std::cout << prompt << std::flush;
#if defined(__unix__) || defined(__APPLE__)
    if (isatty(STDIN_FILENO)) {
        termios oldTerm{};
        tcgetattr(STDIN_FILENO, &oldTerm);
        termios noEcho = oldTerm;
        noEcho.c_lflag &= static_cast<tcflag_t>(~ECHO);
        tcsetattr(STDIN_FILENO, TCSANOW, &noEcho);
        std::cout << "(typing is hidden) " << std::flush;
        const bool ok = static_cast<bool>(std::getline(std::cin, password));
        tcsetattr(STDIN_FILENO, TCSANOW, &oldTerm);
        std::cout << '\n';
        return ok;
    }
#elif defined(_WIN32)
    HANDLE h = GetStdHandle(STD_INPUT_HANDLE);
    DWORD mode = 0;
    if (GetConsoleMode(h, &mode)) {                          // real console: read keys, show '*' for each one
        password.clear();
        for (;;) {
            const int c = _getch();
            if (c == '\r' || c == '\n') break;
            if (c == 3) { std::cout << '\n'; return false; }   // Ctrl+C
            if (c == 0 || c == 0xE0) { _getch(); continue; }   // function/arrow key: skip its second code
            if (c == 8) {                                      // backspace
                if (!password.empty()) { password.erase(password.size() - 1); std::cout << "\b \b" << std::flush; }
                continue;
            }
            if (c < 32) continue;
            password += static_cast<char>(c);
            std::cout << '*' << std::flush;
        }
        std::cout << '\n';
        return true;
    }
#endif
    return static_cast<bool>(std::getline(std::cin, password));
}

} // namespace

int main(int argc, char** argv) {
    std::string dataDir = "data";
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--data" && i + 1 < argc) dataDir = argv[++i];
        else { std::cerr << "Usage: " << argv[0] << " [--data <directory>]\n"; return 64; }
    }

    try {
        ems::FileManager files(dataDir);
        ems::ExamManager manager;
        files.loadAll(manager);

        ems::SteadyTimeSource clock;
        std::mt19937 rng = ems::platform::makeSeededEngine();   // std::random_device is deterministic on old MinGW
        ems::AppContext context{manager, files, std::cin, std::cout, clock, rng};

        std::cout << "==== Examination Management System - Student ====\n";
        const int maxAttempts = 3;
        for (int attempt = 1; attempt <= maxAttempts; ++attempt) {
            std::string username, password;
            std::cout << "user name: " << std::flush;
            if (!std::getline(std::cin, username)) return 0;
            if (!readPassword("password: ", password)) return 0;
            try {
                ems::Student& student = manager.authenticateStudent(username, password);
                std::cout << "\nWelcome, " << student.fullName() << "!\n";
                student.showMenu(context);
                std::cout << "Goodbye.\n";
                return 0;
            } catch (const ems::AuthenticationException& e) {
                std::cout << e.what() << " (attempt " << attempt << " of " << maxAttempts << ")\n";
            } catch (const ems::AccessDeniedException& e) {
                std::cout << "Access denied: " << e.what() << '\n';
                return 3;
            }
        }
        std::cout << "Too many failed login attempts.\n";
        return 2;
    } catch (const ems::FileFormatException& e) {
        std::cerr << "Data file error: " << e.what() << '\n';
    } catch (const ems::FileException& e) {
        std::cerr << "File error: " << e.what() << '\n';
    } catch (const ems::ExamException& e) {
        std::cerr << "Error: " << e.what() << '\n';
    } catch (const std::exception& e) {
        std::cerr << "Unexpected error: " << e.what() << '\n';
    }
    return 1;
}
