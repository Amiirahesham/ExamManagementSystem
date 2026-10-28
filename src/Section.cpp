#include "Section.h"

#include "ExamException.h"

namespace ems {

Section::Section(std::string subjectCode, std::string code, std::string name)
    : subjectCode_(std::move(subjectCode)), code_(std::move(code)), name_(std::move(name)) {
    if (subjectCode_.empty() || code_.empty()) throw ValidationException("Section needs subject and section codes");
    if (name_.empty()) throw ValidationException("Section name must not be empty");
}

void Section::display(std::ostream& os) const {
    os << subjectCode_ << '/' << code_ << " - " << name_;
}

} // namespace ems
