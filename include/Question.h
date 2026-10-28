#ifndef EMS_QUESTION_H
#define EMS_QUESTION_H

#include <cstddef>
#include <ostream>
#include <set>
#include <string>
#include <vector>

#include "Answer.h"

namespace ems {

// Abstract question. Identity key = (subjectCode, number); see README for the schema decision.
// COMPOSITION: a Question owns its Answer objects and cannot be copied.
class Question {
public:
    Question(std::string subjectCode, std::string sectionCode, int number, std::string text);
    virtual ~Question();
    Question(const Question&) = delete;
    Question& operator=(const Question&) = delete;

    // ---- polymorphic interface ----
    virtual int typeId() const = 0;                       // 1 = single, 2 = multiple (question_types.txt)
    virtual void display(std::ostream& os, const std::set<int>& selectedIds) const = 0;
    virtual double grade(const std::set<int>& selectedIds) const = 0;   // 1.0 exact match, else 0.0
    virtual void validate() const = 0;                    // business rules 5 and 6
    virtual void validateSelection(const std::set<int>& selectedIds) const = 0;

    void print(std::ostream& os) const { display(os, std::set<int>()); }

    void addAnswer(const Answer& answer);                 // rejects duplicate ans_id
    const Answer& operator[](std::size_t position) const; // zero-based; throws std::out_of_range
    std::size_t answerCount() const { return answers_.size(); }
    const std::vector<Answer>& answers() const { return answers_; }
    std::set<int> correctIds() const;

    const std::string& subjectCode() const { return subjectCode_; }
    const std::string& sectionCode() const { return sectionCode_; }
    int number() const { return number_; }
    const std::string& text() const { return text_; }

    bool operator==(const Question& other) const;        // identity comparison
    bool operator!=(const Question& other) const { return !(*this == other); }

    static int liveCount() { return liveCount_; }         // static member: live Question objects

    static constexpr std::size_t kMaxAnswers = 10;        // letters a..j

protected:
    void requireKnownIds(const std::set<int>& ids) const;
    void printAnswers(std::ostream& os, const std::set<int>& selectedIds) const;

private:
    std::string subjectCode_, sectionCode_;
    int number_;
    std::string text_;
    std::vector<Answer> answers_;
    static int liveCount_;
};

} // namespace ems
#endif
