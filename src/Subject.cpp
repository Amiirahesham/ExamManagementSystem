#include "Subject.h"

#include "ExamException.h"

namespace ems {

Subject::Subject(std::string code, std::string name, int hours)
    : code_(std::move(code)), name_(std::move(name)), hours_(hours) {
    if (code_.empty()) throw ValidationException("Subject code must not be empty");
    if (name_.empty()) throw ValidationException("Subject name must not be empty (" + code_ + ")");
    if (hours_ < 0) throw ValidationException("Subject hours must not be negative (" + code_ + ")");
}

void Subject::display(std::ostream& os) const {
    os << code_ << " - " << name_ << " (" << hours_ << " h)";
}

} // namespace ems
