#ifndef EMS_RUNNING_ANSWER_H
#define EMS_RUNNING_ANSWER_H

namespace ems {

// A row of exam_running_ans.txt (without the key columns): answer id, correct flag, selected flag.
class RunningAnswer {
public:
    RunningAnswer(int answerId, bool correct, bool selected)
        : answerId_(answerId), correct_(correct), selected_(selected) {}
    int answerId() const { return answerId_; }
    bool isCorrect() const { return correct_; }
    bool isSelected() const { return selected_; }
    void setSelected(bool value) { selected_ = value; }
private:
    int answerId_;
    bool correct_;
    bool selected_;
};

} // namespace ems
#endif
