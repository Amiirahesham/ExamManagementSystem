#ifndef EMS_EXAM_RESULT_H
#define EMS_EXAM_RESULT_H

#include <ostream>
#include <string>
#include <vector>

namespace ems {

struct SectionResult {
    std::string subjectCode, sectionCode, subjectName, sectionName;
    int questions = 0;
    int correct = 0;
    double percent() const { return questions ? 100.0 * correct / questions : 0.0; }
};

struct ExamResult {
    std::string examCode, examName;
    int earnedMarks = 0, totalMarks = 0;
    int score = 0, maxScore = 0, passScore = 0;
    bool passed = false;
    bool timedOut = false;
    std::vector<SectionResult> sections;
    double percent() const { return totalMarks ? 100.0 * earnedMarks / totalMarks : 0.0; }
};

std::ostream& operator<<(std::ostream& os, const ExamResult& result);   // "Screen 5"

} // namespace ems
#endif
