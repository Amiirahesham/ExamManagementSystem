#ifndef EMS_ANSWER_H
#define EMS_ANSWER_H

#include <ostream>
#include <string>

namespace ems {

// One answer option of a question (a row of question_answers.txt).
class Answer {
public:
    Answer(int id, std::string text, bool correct);
    int id() const { return id_; }
    const std::string& text() const { return text_; }
    bool isCorrect() const { return correct_; }
    void display(std::ostream& os, char letter, bool selected) const;
private:
    int id_;
    std::string text_;
    bool correct_;
};

} // namespace ems
#endif
