#include "Exam.h"

#include "ExamException.h"

namespace ems {

Exam::Exam(std::string code, std::string name, int durationMinutes, int maxScore, int passScore)
    : code_(std::move(code)), name_(std::move(name)), duration_(durationMinutes), maxScore_(maxScore),
      passScore_(passScore) {
    validate();
}

void Exam::validate() const {
    if (code_.empty()) throw ValidationException("Exam code must not be empty");
    if (name_.empty()) throw ValidationException("Exam name must not be empty (" + code_ + ")");
    if (duration_ <= 0) throw ValidationException("Invalid exam duration for " + code_);
    if (maxScore_ <= 0) throw ValidationException("max_score must be positive for " + code_);
    if (passScore_ < 0 || passScore_ > maxScore_)   // rule 12
        throw ValidationException("pass_score must be between 0 and max_score for " + code_);
}

int Exam::totalQuestions() const {
    int total = 0;
    for (const ExamDetail& d : details_) total += d.questionCount();
    return total;
}

void Exam::addDetail(const ExamDetail& detail) {
    detail.validate();
    for (const ExamDetail& d : details_)
        if (d == detail)
            throw DuplicateIdException("Duplicate exam detail " + detail.subjectCode() + "/" + detail.sectionCode() +
                                       " for exam " + code_);
    details_.push_back(detail);
}

std::ostream& operator<<(std::ostream& os, const Exam& e) {
    os << "Exam code  : " << e.code() << '\n'
       << "Exam name  : " << e.name() << '\n'
       << "Exam time  : " << e.durationMinutes() << " min\n"
       << "Max score  : " << e.maxScore() << '\n'
       << "Pass score : " << e.passScore() << '\n';
    for (const ExamDetail& d : e.details())
        os << "  " << d.subjectCode() << '/' << d.sectionCode() << " x " << d.questionCount() << '\n';
    return os;
}

} // namespace ems
