#ifndef EMS_EXAM_H
#define EMS_EXAM_H

#include <ostream>
#include <string>
#include <vector>

#include "ExamDetail.h"

namespace ems {

// COMPOSITION: an Exam owns its ExamDetail rows. (Questions are only referenced -> aggregation.)
class Exam {
public:
    Exam(std::string code, std::string name, int durationMinutes, int maxScore, int passScore);

    const std::string& code() const { return code_; }
    const std::string& name() const { return name_; }
    int durationMinutes() const { return duration_; }
    int maxScore() const { return maxScore_; }
    int passScore() const { return passScore_; }
    const std::vector<ExamDetail>& details() const { return details_; }
    int totalQuestions() const;

    void addDetail(const ExamDetail& detail);             // rejects duplicates
    Exam& operator+=(const ExamDetail& detail) { addDetail(detail); return *this; }
    bool operator==(const Exam& o) const { return code_ == o.code_; }
    void validate() const;                                // rule 12 etc.

private:
    std::string code_, name_;
    int duration_, maxScore_, passScore_;
    std::vector<ExamDetail> details_;
};

std::ostream& operator<<(std::ostream& os, const Exam& exam);

} // namespace ems
#endif
