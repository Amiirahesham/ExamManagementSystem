#ifndef EMS_EXAM_MANAGER_H
#define EMS_EXAM_MANAGER_H

#include <map>
#include <memory>
#include <random>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "Exam.h"
#include "ExamReservation.h"
#include "ExamResult.h"
#include "Question.h"
#include "RunningQuestion.h"
#include "Section.h"
#include "Student.h"
#include "Subject.h"
#include "User.h"

namespace ems {

// In-memory aggregate of all 12 tables. This build implements the READ / GENERATE / GRADE side
// needed by the Student module; Admin CRUD (update/delete of catalog data) is added later
// on this same class. The add*() methods enforce the uniqueness and reference rules.
//   AGGREGATION: exams reference reusable questions of the bank (by subject/section).
class ExamManager {
public:
    ExamManager() = default;
    ExamManager(const ExamManager&) = delete;
    ExamManager& operator=(const ExamManager&) = delete;

    // ---- population (used by FileManager; reusable by Admin) ----
    void setQuestionTypes(std::map<int, std::string> types);
    void setStatusNames(std::map<int, std::string> names);
    void addUser(std::unique_ptr<User> user);              // rule 1: unique username
    void addSubject(Subject subject);
    void addSection(Section section);                      // rule 3: subject must exist
    void addExam(Exam exam);                               // rule 2: unique exam code
    void addExamDetail(const std::string& examCode, const ExamDetail& detail);
    void addQuestion(std::unique_ptr<Question> question);  // rule 4 + known question type
    void addAnswer(const std::string& subjectCode, int questionNumber, const Answer& answer);
    void validateIntegrity() const;                        // rules 5 and 6 for every question
    // Replace reservation/attempt tables with freshly read ones (keeps existing object addresses).
    void syncStudentData(std::map<int, ExamReservation> reservations,
                         std::map<int, std::vector<RunningQuestion>> attempts);

    // ---- queries ----
    const Subject& subject(const std::string& code) const;
    const Section& section(const std::string& subjectCode, const std::string& sectionCode) const;
    const Exam& exam(const std::string& code) const;       // NotFoundException if unknown
    bool hasExam(const std::string& code) const { return exams_.count(code) != 0; }
    const Question& question(const std::string& subjectCode, int number) const;
    const Question* findQuestion(const std::string& subjectCode, int number) const;
    const std::vector<const Question*>& questionsIn(const std::string& subjectCode,
                                                    const std::string& sectionCode) const;
    std::string statusName(ReservationStatus status) const;
    std::string questionTypeName(int typeId) const;
    std::size_t userCount() const { return users_.size(); }
    std::size_t questionCount() const { return questions_.size(); }
    std::size_t examCount() const { return exams_.size(); }

    // ---- authentication ----
    User& authenticate(const std::string& username, const std::string& password);
    Student& authenticateStudent(const std::string& username, const std::string& password);

    // ---- reservations & attempts ----
    ExamReservation* findReservation(int id);
    const ExamReservation* findReservation(int id) const;
    std::vector<const ExamReservation*> reservationsFor(const std::string& username) const;
    std::size_t reservationCount() const { return reservations_.size(); }
    std::vector<RunningQuestion>& attempt(int reservationId);          // creates empty if absent
    const std::vector<RunningQuestion>* findAttempt(int reservationId) const;
    void eraseAttempt(int reservationId) { attempts_.erase(reservationId); }

    // ---- exam logic ----
    // Random selection per subject/section (rule 7 enforced up-front, nothing changes on failure).
    std::vector<RunningQuestion> generateExam(const ExamReservation& reservation, std::mt19937& rng) const;
    // Resolve question references and verify the snapshot against the bank.
    // strict=true for exams that will be resumed; false for read-only result display.
    void linkAttempt(std::vector<RunningQuestion>& questions, const Exam& exam, bool strict) const;
    // Grades every question (exact match), updates quest_score, builds the section report.
    ExamResult gradeExam(const ExamReservation& reservation, std::vector<RunningQuestion>& questions) const;
    // Result of a finished exam from STORED values (score and quest_score), never recomputed.
    ExamResult storedResult(const ExamReservation& reservation) const;

    static int computeScore(int earnedMarks, int totalMarks, int maxScore);   // normalised, rounded

private:
    ExamResult summarize(const ExamReservation& reservation,
                         const std::vector<RunningQuestion>& questions) const;

    std::map<int, std::string> questionTypes_, statusNames_;
    std::unordered_map<std::string, std::unique_ptr<User>> users_;
    std::map<std::string, Subject> subjects_;
    std::map<std::pair<std::string, std::string>, Section> sections_;
    std::map<std::string, Exam> exams_;
    std::map<std::pair<std::string, int>, std::unique_ptr<Question>> questions_;
    std::map<std::pair<std::string, std::string>, std::vector<const Question*>> sectionIndex_;
    std::map<int, ExamReservation> reservations_;
    std::map<int, std::vector<RunningQuestion>> attempts_;
};

} // namespace ems
#endif
