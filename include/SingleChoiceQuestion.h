#ifndef EMS_SINGLE_CHOICE_QUESTION_H
#define EMS_SINGLE_CHOICE_QUESTION_H

#include "Question.h"

namespace ems {

class SingleChoiceQuestion : public Question {
public:
    using Question::Question;
    int typeId() const override { return 1; }
    void display(std::ostream& os, const std::set<int>& selectedIds) const override;
    double grade(const std::set<int>& selectedIds) const override;
    void validate() const override;                       // exactly one correct answer
    void validateSelection(const std::set<int>& selectedIds) const override;   // at most one id
};

} // namespace ems
#endif
