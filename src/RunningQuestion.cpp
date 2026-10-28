#include "RunningQuestion.h"

#include <algorithm>

#include "ExamException.h"
#include "Util.h"

namespace ems {

constexpr int RunningQuestion::kMarksPerQuestion;   // required in C++14 (ODR-use)

RunningQuestion::RunningQuestion(int reservationId, int position, int questionNumber, int score,
                                 std::string subjectCode, std::string sectionCode)
    : reservationId_(reservationId), position_(position), questionNumber_(questionNumber), score_(score),
      subjectCode_(std::move(subjectCode)), sectionCode_(std::move(sectionCode)) {
    if (reservationId_ <= 0 || position_ <= 0 || questionNumber_ <= 0)
        throw ValidationException("res_id, quest_no and quest_real_no must be positive");
    if (score_ < 0 || score_ > kMarksPerQuestion) throw ValidationException("quest_score must be 0 or 1");
}

RunningQuestion RunningQuestion::fromQuestion(int reservationId, int position, const Question& q) {
    RunningQuestion rq(reservationId, position, q.number(), 0, q.subjectCode(), q.sectionCode());
    for (const Answer& a : q.answers()) rq.addAnswer(RunningAnswer(a.id(), a.isCorrect(), false));
    rq.link(q);
    return rq;
}

void RunningQuestion::setClassification(const std::string& subjectCode, const std::string& sectionCode) {
    subjectCode_ = subjectCode;
    sectionCode_ = sectionCode;
}

void RunningQuestion::addAnswer(const RunningAnswer& answer) {
    for (const RunningAnswer& a : answers_)
        if (a.answerId() == answer.answerId())
            throw DuplicateIdException("Duplicate ans_no " + util::toString(answer.answerId()) + " for res_id " +
                                       util::toString(reservationId_) + " quest_no " + util::toString(position_));
    answers_.push_back(answer);
}

void RunningQuestion::sortAnswers() {
    std::sort(answers_.begin(), answers_.end(),
              [](const RunningAnswer& a, const RunningAnswer& b) { return a.answerId() < b.answerId(); });
}

std::set<int> RunningQuestion::selectedIds() const {
    std::set<int> ids;
    for (const RunningAnswer& a : answers_) if (a.isSelected()) ids.insert(a.answerId());
    return ids;
}

std::set<int> RunningQuestion::correctIds() const {
    std::set<int> ids;
    for (const RunningAnswer& a : answers_) if (a.isCorrect()) ids.insert(a.answerId());
    return ids;
}

void RunningQuestion::setSelection(const std::set<int>& ids) {
    if (!question_) throw ValidationException("Question is not linked to the question bank");
    for (int id : ids) {
        const bool known = std::any_of(answers_.begin(), answers_.end(),
                                       [id](const RunningAnswer& a) { return a.answerId() == id; });
        if (!known) throw InvalidSelectionException("Answer id " + util::toString(id) + " is not part of this question");
    }
    question_->validateSelection(ids);                      // runtime polymorphism (single vs multiple)
    for (RunningAnswer& a : answers_) a.setSelected(ids.count(a.answerId()) != 0);
}

int RunningQuestion::calculateScore() {
    if (!question_) throw ValidationException("Question is not linked to the question bank");
    score_ = (question_->grade(selectedIds()) >= 1.0) ? kMarksPerQuestion : 0;
    return score_;
}

} // namespace ems
