#ifndef EMS_USER_H
#define EMS_USER_H

#include <string>

namespace ems {

struct AppContext;

enum class Role { Student, Admin };
std::string toString(Role role);
Role parseRole(const std::string& text);                   // throws ValidationException

// Base class of every account. Only a password HASH is ever stored (ctor rejects anything else).
// A future Admin module derives `Admin : public User` and overrides showMenu().
class User {
public:
    User(std::string firstName, std::string lastName, std::string email, std::string username,
         std::string passwordHash, Role role);
    virtual ~User();
    User(const User&) = delete;
    User& operator=(const User&) = delete;

    const std::string& firstName() const { return firstName_; }
    const std::string& lastName() const { return lastName_; }
    const std::string& email() const { return email_; }
    const std::string& username() const { return username_; }
    const std::string& passwordHash() const { return passwordHash_; }
    Role role() const { return role_; }
    std::string fullName() const { return firstName_ + " " + lastName_; }

    bool verifyPassword(const std::string& plainPassword) const;

    // Default: no console menu for this role in the current build.
    virtual void showMenu(AppContext& context);

    static int liveCount() { return liveCount_; }

private:
    std::string firstName_, lastName_, email_, username_, passwordHash_;
    Role role_;
    static int liveCount_;
};

} // namespace ems
#endif
