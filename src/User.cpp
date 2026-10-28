#include "User.h"

#include "ExamException.h"
#include "PasswordHasher.h"
#include "Util.h"

namespace ems {

int User::liveCount_ = 0;

std::string toString(Role role) { return role == Role::Student ? "STUDENT" : "ADMIN"; }

Role parseRole(const std::string& text) {
    const std::string t = util::toUpper(util::trim(text));
    if (t == "STUDENT") return Role::Student;
    if (t == "ADMIN") return Role::Admin;
    throw ValidationException("Unknown role '" + text + "' (expected STUDENT or ADMIN)");
}

User::User(std::string firstName, std::string lastName, std::string email, std::string username,
           std::string passwordHash, Role role)
    : firstName_(std::move(firstName)), lastName_(std::move(lastName)), email_(std::move(email)),
      username_(std::move(username)), passwordHash_(std::move(passwordHash)), role_(role) {
    if (firstName_.empty() || lastName_.empty()) throw ValidationException("User needs first and last name");
    if (email_.find('@') == std::string::npos) throw ValidationException("Invalid e-mail address for user " + username_);
    if (username_.empty()) throw ValidationException("Username must not be empty");
    if (!PasswordHasher::isWellFormed(passwordHash_))
        throw ValidationException("Password of user '" + username_ + "' is not a valid password hash (plaintext is never accepted)");
    ++liveCount_;
}

User::~User() { --liveCount_; }

bool User::verifyPassword(const std::string& plainPassword) const {
    return PasswordHasher::verify(plainPassword, passwordHash_);
}

void User::showMenu(AppContext&) {
    throw AccessDeniedException("No console menu is available for role " + toString(role_) + " in this build");
}

} // namespace ems
