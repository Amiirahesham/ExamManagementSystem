#ifndef EMS_RUNNING_QUESTION_H
#define EMS_RUNNING_QUESTION_H

#include <set>
#include <string>
#include <vector>

#include "Question.h"
#include "RunningAnswer.h"

namespace ems {

// A row of exam_running.txt plus its exam_running_ans.txt rows (COMPOSITION of RunningAnswer).
// The Question is only referenced (ASSOCIATION) - it stays owned by ExamManager.
class RunningQuestion {
public:
    static constexpr int kMarksPerQuestion = 1;            // quest_score is 0 or 1 raw mark

    RunningQuestion(int reservationId, int position, int questionNumber, int score,
                    std::string subjectCode, std::string sectionCode);
    // Snapshot constructor used when an exam is generated.
    static RunningQuestion fromQuestion(int reservationId, int position, const Question& question);

    int reservationId() const { return reservationId_; }
    int position() const { return position_; }
    int questionNumber() const { return questionNumber_; }   // quest_real_no
    int score() const { return score_; }                     // quest_score
    const std::string& subjectCode() const { return subjectCode_; }
    const std::string& sectionCode() const { return sectionCode_; }
    const Question* question() const { return question_; }
    const std::vector<RunningAnswer>& answers() const { return answers_; }

    void setClassification(const std::string& subjectCode, const std::string& sectionCode);
    void link(const Question& question) { question_ = &question; }
    void addAnswer(const RunningAnswer& answer);             // rejects duplicate ans_no
    void sortAnswers();

    std::set<int> selectedIds() const;
    std::set<int> correctIds() const;
    bool isAnswered() const { return !selectedIds().empty(); }
    void setSelection(const std::set<int>& ids);             // validated polymorphically
    int calculateScore();                                    // exact-match grading; updates score()

    bool operator<(const RunningQuestion& o) const { return position_ < o.position_; }

private:
    int reservationId_, position_, questionNumber_, score_;
    std::string subjectCode_, sectionCode_;
    const Question* question_ = nullptr;
    std::vector<RunningAnswer> answers_;
};

} // namespace ems
#endif
