#include "SingleChoiceQuestion.h"

#include "ExamException.h"
#include "Util.h"

namespace ems {

void SingleChoiceQuestion::display(std::ostream& os, const std::set<int>& selectedIds) const {
    os << ' ' << text() << "\n (Single choice - select exactly ONE answer)\n";
    printAnswers(os, selectedIds);
}

double SingleChoiceQuestion::grade(const std::set<int>& selectedIds) const {
    const std::set<int> correct = correctIds();
    return (selectedIds.size() == 1 && selectedIds == correct) ? 1.0 : 0.0;
}

void SingleChoiceQuestion::validate() const {
    const std::string id = subjectCode() + "/" + util::toString(number());
    if (answerCount() < 2) throw ValidationException("Single-choice question " + id + " needs at least 2 answers");
    if (correctIds().size() != 1)
        throw ValidationException("Single-choice question " + id + " must have exactly one correct answer (found " +
                                  util::toString(correctIds().size()) + ")");
}

void SingleChoiceQuestion::validateSelection(const std::set<int>& selectedIds) const {
    if (selectedIds.size() > 1)
        throw InvalidSelectionException("This is a single-choice question: select only ONE answer");
    requireKnownIds(selectedIds);
}

} // namespace ems
