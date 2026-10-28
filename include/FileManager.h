#ifndef EMS_FILE_MANAGER_H
#define EMS_FILE_MANAGER_H

#include <cstddef>
#include <map>
#include <string>
#include <vector>

#include "ExamReservation.h"
#include "RunningQuestion.h"

namespace ems {

class ExamManager;

// Text-file persistence for the 12 tables ('|' separated, '#' comment lines allowed, no header row).
// READS all 12 files. WRITES only the three tables owned by the Student side
// (exam_reservations, exam_running, exam_running_ans) - the other nine belong to Admin.
// Every write: temp file -> flush -> .bak of the previous version -> atomic rename.
// Every save re-reads the CURRENT files and replaces only this reservation's rows, so other
// students' data is never overwritten; a malformed file is never overwritten.
class FileManager {
public:
    explicit FileManager(std::string dataDirectory);

    static const std::vector<std::string>& fileNames();     // the 12 file names
    const std::string& dataDirectory() const { return dataDir_; }

    void loadAll(ExamManager& manager) const;
    // Re-read exam_reservations / exam_running / exam_running_ans into the manager.
    void reloadStudentData(ExamManager& manager) const;
    // Persist one attempt (rules 10, 11): running_ans -> running -> reservation (last).
    void saveProgress(const ExamManager& manager, const ExamReservation& reservation,
                      const std::vector<RunningQuestion>& questions) const;

private:
    using Fields = std::vector<std::string>;
    struct Record { std::size_t line; Fields fields; };

    std::vector<Record> readRecords(const std::string& file, std::size_t minFields,
                                    std::size_t maxFields) const;
    void writeRecords(const std::string& file, const std::vector<Fields>& rows) const;

    template <typename Fn>
    void forEachRecord(const std::vector<Record>& records, const std::string& file, Fn fn) const;

    void loadQuestionTypes(ExamManager& m) const;
    void loadStatuses(ExamManager& m) const;
    void loadUsers(ExamManager& m) const;
    void loadSubjects(ExamManager& m) const;
    void loadSections(ExamManager& m) const;
    void loadExams(ExamManager& m) const;
    void loadExamDetails(ExamManager& m) const;
    void loadQuestions(ExamManager& m) const;
    void loadAnswers(ExamManager& m) const;

    std::map<int, ExamReservation> parseReservations(const ExamManager& m) const;
    std::map<int, std::vector<RunningQuestion>> parseAttempts() const;

    std::string dataDir_;
};

} // namespace ems
#endif
