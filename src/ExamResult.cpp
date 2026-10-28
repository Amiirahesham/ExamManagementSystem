#include "ExamResult.h"

#include <iomanip>

namespace ems {

std::ostream& operator<<(std::ostream& os, const ExamResult& r) {
    os << '\n';
    if (r.timedOut) os << "*** Time expired - your examination was submitted automatically. ***\n";
    os << (r.passed ? "Congratulation you do a good job\n" : "Sorry, you did not reach the passing score. Keep practising!\n");
    os << '\n' << std::left << std::setw(22) << "Subject" << std::setw(20) << "Section" << std::right
       << std::setw(13) << "no_questions" << std::setw(9) << "correct" << std::setw(9) << "%" << '\n';
    os << std::string(73, '-') << '\n';
    for (const SectionResult& s : r.sections) {
        os << std::left << std::setw(22) << s.subjectName.substr(0, 21) << std::setw(20) << s.sectionName.substr(0, 19)
           << std::right << std::setw(13) << s.questions << std::setw(9) << s.correct << std::setw(8)
           << std::fixed << std::setprecision(1) << s.percent() << "%\n";
    }
    os << std::string(73, '-') << '\n'
       << "Overall: " << r.earnedMarks << " / " << r.totalMarks << " correct = " << std::fixed
       << std::setprecision(1) << r.percent() << "%   " << (r.passed ? "Pass" : "Fail") << "\n\n"
       << "Pass Score: " << r.passScore << '\n'
       << "Your Score: " << r.score << " / " << r.maxScore << '\n';
    return os;
}

} // namespace ems
