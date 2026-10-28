#include "Answer.h"

#include "ExamException.h"

namespace ems {

Answer::Answer(int id, std::string text, bool correct) : id_(id), text_(std::move(text)), correct_(correct) {
    if (id_ <= 0) throw ValidationException("Answer id must be positive");
    if (text_.empty()) throw ValidationException("Answer text must not be empty");
}

void Answer::display(std::ostream& os, char letter, bool selected) const {
    os << "  " << letter << ") [" << (selected ? 'x' : ' ') << "] " << text_ << '\n';
}

} // namespace ems
