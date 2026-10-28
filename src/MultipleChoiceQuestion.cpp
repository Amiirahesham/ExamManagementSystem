#include "MultipleChoiceQuestion.h"

#include "ExamException.h"
#include "Util.h"

namespace ems {

void MultipleChoiceQuestion::display(std::ostream& os, const std::set<int>& selectedIds) const {
    os << ' ' << text() << "\n (Multiple choice - select ALL correct answers, e.g. \"a,c\")\n";
    printAnswers(os, selectedIds);
}

double MultipleChoiceQuestion::grade(const std::set<int>& selectedIds) const {
    const std::set<int> correct = correctIds();
    return (!selectedIds.empty() && selectedIds == correct) ? 1.0 : 0.0;   // exact match, no partial credit
}

void MultipleChoiceQuestion::validate() const {
    const std::string id = subjectCode() + "/" + util::toString(number());
    if (answerCount() < 2) throw ValidationException("Multiple-choice question " + id + " needs at least 2 answers");
    if (correctIds().size() < 2)
        throw ValidationException("Multiple-choice question " + id + " must have at least two correct answers (found " +
                                  util::toString(correctIds().size()) + ")");
}

void MultipleChoiceQuestion::validateSelection(const std::set<int>& selectedIds) const {
    requireKnownIds(selectedIds);
}

} // namespace ems
