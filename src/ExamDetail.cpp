#include "ExamDetail.h"

#include "ExamException.h"

namespace ems {

ExamDetail::ExamDetail(std::string subjectCode, std::string sectionCode, int questionCount)
    : subjectCode_(std::move(subjectCode)), sectionCode_(std::move(sectionCode)), questionCount_(questionCount) {
    validate();
}

void ExamDetail::validate() const {
    if (subjectCode_.empty() || sectionCode_.empty()) throw ValidationException("Exam detail needs subject and section codes");
    if (questionCount_ <= 0) throw ValidationException("no_questions must be positive for " + subjectCode_ + "/" + sectionCode_);
}

} // namespace ems
