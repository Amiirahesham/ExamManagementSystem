#include "Question.h"

#include <stdexcept>

#include "ExamException.h"
#include "Util.h"

namespace ems {

int Question::liveCount_ = 0;
constexpr std::size_t Question::kMaxAnswers;   // required in C++14 (ODR-use)

Question::Question(std::string subjectCode, std::string sectionCode, int number, std::string text)
    : subjectCode_(std::move(subjectCode)), sectionCode_(std::move(sectionCode)), number_(number),
      text_(std::move(text)) {
    if (subjectCode_.empty() || sectionCode_.empty()) throw ValidationException("Question needs subject and section codes");
    if (number_ <= 0) throw ValidationException("Question number must be positive");
    if (text_.empty()) throw ValidationException("Question header must not be empty");
    ++liveCount_;
}

Question::~Question() { --liveCount_; }

void Question::addAnswer(const Answer& answer) {
    for (const Answer& a : answers_)
        if (a.id() == answer.id())
            throw DuplicateIdException("Duplicate ans_id " + util::toString(answer.id()) + " in question " +
                                       subjectCode_ + "/" + util::toString(number_));
    if (answers_.size() >= kMaxAnswers)
        throw ValidationException("Question " + subjectCode_ + "/" + util::toString(number_) +
                                  " has more than " + util::toString(kMaxAnswers) + " answers");
    answers_.push_back(answer);
}

const Answer& Question::operator[](std::size_t position) const {
    if (position >= answers_.size())
        throw std::out_of_range("Answer index " + util::toString(position) + " out of range (0.." +
                                util::toString(answers_.size()) + ")");
    return answers_[position];
}

std::set<int> Question::correctIds() const {
    std::set<int> ids;
    for (const Answer& a : answers_) if (a.isCorrect()) ids.insert(a.id());
    return ids;
}

bool Question::operator==(const Question& other) const {
    return subjectCode_ == other.subjectCode_ && number_ == other.number_;
}

void Question::requireKnownIds(const std::set<int>& ids) const {
    for (int id : ids) {
        bool found = false;
        for (const Answer& a : answers_) if (a.id() == id) { found = true; break; }
        if (!found) throw InvalidSelectionException("Answer id " + util::toString(id) + " does not exist for this question");
    }
}

void Question::printAnswers(std::ostream& os, const std::set<int>& selectedIds) const {
    char letter = 'a';
    for (const Answer& a : answers_) a.display(os, letter++, selectedIds.count(a.id()) != 0);
}

} // namespace ems
