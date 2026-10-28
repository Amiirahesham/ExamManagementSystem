#ifndef EMS_EXAM_DETAIL_H
#define EMS_EXAM_DETAIL_H

#include <string>

namespace ems {

// Number of questions required from one subject/section (a row of exam_details.txt).
class ExamDetail {
public:
    ExamDetail(std::string subjectCode, std::string sectionCode, int questionCount);
    const std::string& subjectCode() const { return subjectCode_; }
    const std::string& sectionCode() const { return sectionCode_; }
    int questionCount() const { return questionCount_; }
    void validate() const;
    bool operator==(const ExamDetail& o) const {          // same subject + section
        return subjectCode_ == o.subjectCode_ && sectionCode_ == o.sectionCode_;
    }
private:
    std::string subjectCode_, sectionCode_;
    int questionCount_;
};

} // namespace ems
#endif
