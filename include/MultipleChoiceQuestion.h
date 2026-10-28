#ifndef EMS_MULTIPLE_CHOICE_QUESTION_H
#define EMS_MULTIPLE_CHOICE_QUESTION_H

#include "Question.h"

namespace ems {

class MultipleChoiceQuestion : public Question {
public:
    using Question::Question;
    int typeId() const override { return 2; }
    void display(std::ostream& os, const std::set<int>& selectedIds) const override;
    double grade(const std::set<int>& selectedIds) const override;   // exact-match only
    void validate() const override;                       // at least two correct answers
    void validateSelection(const std::set<int>& selectedIds) const override;
};

} // namespace ems
#endif
