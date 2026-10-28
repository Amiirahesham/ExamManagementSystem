#include "FileManager.h"

#include <algorithm>
#include <fstream>
#include <set>

#include "ExamException.h"
#include "ExamManager.h"
#include "MultipleChoiceQuestion.h"
#include "Platform.h"
#include "SingleChoiceQuestion.h"
#include "Student.h"
#include "Util.h"

namespace ems {

namespace {

const char* const kQuestionTypes = "question_types.txt";
const char* const kStatuses = "reservation_status.txt";
const char* const kUsers = "exam_users.txt";
const char* const kSubjects = "subjects.txt";
const char* const kSections = "sections.txt";
const char* const kExams = "exams.txt";
const char* const kDetails = "exam_details.txt";
const char* const kQuestions = "questions.txt";
const char* const kAnswers = "question_answers.txt";
const char* const kReservations = "exam_reservations.txt";
const char* const kRunning = "exam_running.txt";
const char* const kRunningAns = "exam_running_ans.txt";

std::string at(const std::string& file, std::size_t line, const std::string& msg) {
    return file + ":" + util::toString(line) + ": " + msg;
}

bool parseFlag(const std::string& text, const std::string& field) {
    const int v = util::parseInt(text, field);
    if (v != 0 && v != 1) throw ValidationException("Field '" + field + "' must be 0 or 1");
    return v == 1;
}

std::string flag(bool b) { return b ? "1" : "0"; }

} // namespace

FileManager::FileManager(std::string dataDirectory) : dataDir_(std::move(dataDirectory)) {
    if (dataDir_.empty()) throw ValidationException("Data directory must not be empty");
}

const std::vector<std::string>& FileManager::fileNames() {
    static const std::vector<std::string> names = {kUsers, kExams, kDetails, kSubjects, kSections, kQuestions,
                                                   kAnswers, kQuestionTypes, kReservations, kStatuses, kRunning,
                                                   kRunningAns};
    return names;
}

// ------------------------------------------------------------------ low level I/O

std::vector<FileManager::Record> FileManager::readRecords(const std::string& file, std::size_t minFields,
                                                          std::size_t maxFields) const {
    const std::string path = platform::joinPath(dataDir_, file);
    if (!platform::fileExists(path)) throw FileException("Required data file is missing: " + path);
    std::ifstream in(path.c_str(), std::ios::binary);
    if (!in) throw FileException("Cannot open data file for reading: " + path);

    std::vector<Record> records;
    std::string line;
    std::size_t lineNo = 0;
    while (std::getline(in, line)) {
        ++lineNo;
        if (lineNo == 1 && line.compare(0, 3, "\xEF\xBB\xBF") == 0) line.erase(0, 3);   // UTF-8 BOM
        if (!line.empty() && line.back() == '\r') line.pop_back();
        const std::string t = util::trim(line);
        if (t.empty() || t[0] == '#') continue;
        Fields fields = util::split(line, '|');
        for (std::string& f : fields) f = util::trim(f);
        if (fields.size() < minFields || fields.size() > maxFields)
            throw FileFormatException(file, lineNo, "expected " + (minFields == maxFields
                                      ? util::toString(minFields) : util::toString(minFields) + "-" + util::toString(maxFields)) +
                                      " fields separated by '|' but found " + util::toString(fields.size()));
        records.push_back(Record{lineNo, std::move(fields)});
    }
    if (in.bad()) throw FileException("I/O error while reading " + path);
    return records;
}

void FileManager::writeRecords(const std::string& file, const std::vector<Fields>& rows) const {
    for (const Fields& row : rows)                           // never write something we could not read back
        for (const std::string& f : row)
            if (f.find_first_of("|\r\n") != std::string::npos)
                throw ValidationException("Field contains a forbidden character ('|' or line break): " + f);

    const std::string target = platform::joinPath(dataDir_, file);
    const std::string tmp = target + ".tmp";
    const std::string bak = target + ".bak";
    {
        std::ofstream out(tmp.c_str(), std::ios::binary | std::ios::trunc);
        if (!out) throw FileException("Cannot create temporary file: " + tmp + " (" + platform::lastErrorText() + ")");
        for (std::size_t r = 0; r < rows.size(); ++r) {
            const Fields& row = rows[r];
            for (std::size_t i = 0; i < row.size(); ++i) out << row[i] << (i + 1 < row.size() ? "|" : platform::kLineEnding);
        }
        out.flush();
        if (!out) { out.close(); platform::removeFile(tmp); throw FileException("Write failed for " + tmp); }
    }
    if (platform::fileExists(target) && !platform::copyFile(target, bak)) {   // keep the previous version
        platform::removeFile(tmp);
        throw FileException("Cannot create backup " + bak + " (" + platform::lastErrorText() + ")");
    }
    if (!platform::replaceFile(tmp, target)) {               // atomic replace
        const std::string why = platform::lastErrorText();
        platform::removeFile(tmp);
        throw FileException("Cannot replace " + target + ": " + why);
    }
}

template <typename Fn>
void FileManager::forEachRecord(const std::vector<Record>& records, const std::string& file, Fn fn) const {
    for (const Record& r : records) {
        try {
            fn(r);
        } catch (const FileFormatException&) {
            throw;
        } catch (const DuplicateIdException& e) {
            throw DuplicateIdException(at(file, r.line, e.what()));
        } catch (const InvalidQuestionTypeException& e) {
            throw InvalidQuestionTypeException(at(file, r.line, e.what()));
        } catch (const std::exception& e) {                  // ValidationException, NotFound, out_of_range, ...
            throw FileFormatException(file, r.line, e.what());
        }
    }
}

// ------------------------------------------------------------------ table loaders

void FileManager::loadQuestionTypes(ExamManager& m) const {
    std::map<int, std::string> types;
    forEachRecord(readRecords(kQuestionTypes, 2, 2), kQuestionTypes, [&](const Record& r) {
        const int id = util::parseInt(r.fields[0], "Q_type_id");
        if (r.fields[1].empty()) throw ValidationException("Q_type_name is empty");
        if (!types.emplace(id, r.fields[1]).second) throw DuplicateIdException("Duplicate Q_type_id " + util::toString(id));
    });
    m.setQuestionTypes(std::move(types));
}

void FileManager::loadStatuses(ExamManager& m) const {
    std::map<int, std::string> names;
    forEachRecord(readRecords(kStatuses, 2, 2), kStatuses, [&](const Record& r) {
        const int id = util::parseInt(r.fields[0], "exam_status");
        statusFromInt(id);                                   // must be one of the four known states
        if (r.fields[1].empty()) throw ValidationException("status_description is empty");
        if (!names.emplace(id, r.fields[1]).second) throw DuplicateIdException("Duplicate exam_status " + util::toString(id));
    });
    m.setStatusNames(std::move(names));
}

void FileManager::loadUsers(ExamManager& m) const {
    forEachRecord(readRecords(kUsers, 6, 6), kUsers, [&](const Record& r) {
        const Fields& f = r.fields;
        const Role role = parseRole(f[5]);
        if (role == Role::Student) m.addUser(std::make_unique<Student>(f[0], f[1], f[2], f[3], f[4]));
        else m.addUser(std::make_unique<User>(f[0], f[1], f[2], f[3], f[4], role));   // Admin module extends here
    });
}

void FileManager::loadSubjects(ExamManager& m) const {
    forEachRecord(readRecords(kSubjects, 3, 3), kSubjects, [&](const Record& r) {
        m.addSubject(Subject(r.fields[0], r.fields[1], util::parseInt(r.fields[2], "Sub_time_in_hours")));
    });
}

void FileManager::loadSections(ExamManager& m) const {
    forEachRecord(readRecords(kSections, 3, 3), kSections, [&](const Record& r) {
        m.addSection(Section(r.fields[0], r.fields[1], r.fields[2]));
    });
}

void FileManager::loadExams(ExamManager& m) const {
    forEachRecord(readRecords(kExams, 5, 5), kExams, [&](const Record& r) {
        m.addExam(Exam(r.fields[0], r.fields[1], util::parseInt(r.fields[2], "exam_time"),
                       util::parseInt(r.fields[3], "max_score"), util::parseInt(r.fields[4], "pass_score")));
    });
}

void FileManager::loadExamDetails(ExamManager& m) const {
    forEachRecord(readRecords(kDetails, 4, 4), kDetails, [&](const Record& r) {
        m.addExamDetail(r.fields[0], ExamDetail(r.fields[1], r.fields[2], util::parseInt(r.fields[3], "no_questions")));
    });
}

void FileManager::loadQuestions(ExamManager& m) const {
    forEachRecord(readRecords(kQuestions, 5, 5), kQuestions, [&](const Record& r) {
        const Fields& f = r.fields;
        const int number = util::parseInt(f[2], "question_no");
        const int type = util::parseInt(f[4], "question_type");
        std::unique_ptr<Question> q;
        if (type == 1) q = std::make_unique<SingleChoiceQuestion>(f[0], f[1], number, f[3]);
        else if (type == 2) q = std::make_unique<MultipleChoiceQuestion>(f[0], f[1], number, f[3]);
        else throw InvalidQuestionTypeException("Invalid question_type " + util::toString(type));
        m.addQuestion(std::move(q));
    });
}

void FileManager::loadAnswers(ExamManager& m) const {
    forEachRecord(readRecords(kAnswers, 5, 5), kAnswers, [&](const Record& r) {
        const Fields& f = r.fields;
        m.addAnswer(f[0], util::parseInt(f[1], "question_no"),
                    Answer(util::parseInt(f[2], "ans_id"), f[3], parseFlag(f[4], "iscorrect")));
    });
}

std::map<int, ExamReservation> FileManager::parseReservations(const ExamManager& m) const {
    std::map<int, ExamReservation> out;
    forEachRecord(readRecords(kReservations, 9, 10), kReservations, [&](const Record& r) {
        const Fields& f = r.fields;                          // 9 fields (doc) or 10 fields (workbook, + max_score)
        const int id = util::parseInt(f[0], "reservation_id");
        const int maxScore = (f.size() == 10 && !util::isNullToken(f[9])) ? util::parseInt(f[9], "max_score")
                                                                           : m.exam(f[3]).maxScore();
        ExamReservation res(id, f[1], f[2], f[3], util::parseOptionalInt(f[4], "cur_question"),
                            util::parseOptionalInt(f[5], "score"), statusFromInt(util::parseInt(f[6], "status")),
                            util::parseOptionalInt(f[7], "rem_time"), util::parseInt(f[8], "pass_score"), maxScore);
        if (!out.emplace(id, std::move(res)).second)
            throw DuplicateIdException("Duplicate reservation_id " + util::toString(id));
    });
    return out;
}

std::map<int, std::vector<RunningQuestion>> FileManager::parseAttempts() const {
    std::map<int, std::vector<RunningQuestion>> out;
    std::set<std::pair<int, int>> seen;
    forEachRecord(readRecords(kRunning, 4, 6), kRunning, [&](const Record& r) {
        const Fields& f = r.fields;                          // 4 fields (workbook) or 6 (+ subject, section)
        if (f.size() == 5) throw ValidationException("expected 4 or 6 fields, found 5");
        const int res = util::parseInt(f[0], "res_id");
        const int pos = util::parseInt(f[1], "quest_no");
        RunningQuestion rq(res, pos, util::parseInt(f[2], "quest_real_no"), util::parseInt(f[3], "quest_score"),
                           f.size() == 6 ? f[4] : std::string(), f.size() == 6 ? f[5] : std::string());
        if (!seen.insert({res, pos}).second)
            throw DuplicateIdException("Duplicate (res_id, quest_no) = (" + util::toString(res) + ", " + util::toString(pos) + ")");
        out[res].push_back(std::move(rq));
    });

    std::map<std::pair<int, int>, RunningQuestion*> index;   // safe: vectors no longer grow
    for (auto& kv : out) for (RunningQuestion& rq : kv.second) index[{rq.reservationId(), rq.position()}] = &rq;

    forEachRecord(readRecords(kRunningAns, 5, 5), kRunningAns, [&](const Record& r) {
        const Fields& f = r.fields;
        const int res = util::parseInt(f[0], "res_id");
        const int pos = util::parseInt(f[1], "quest_no");
        auto it = index.find({res, pos});
        if (it == index.end())
            throw ValidationException("answer row refers to missing exam_running record (" + util::toString(res) + ", " + util::toString(pos) + ")");
        const int ansNo = util::parseInt(f[2], "ans_no");
        if (ansNo <= 0) throw ValidationException("ans_no must be positive");
        it->second->addAnswer(RunningAnswer(ansNo, parseFlag(f[3], "corr_ans"), parseFlag(f[4], "user_ans")));
    });
    for (auto& kv : out) {
        std::sort(kv.second.begin(), kv.second.end());
        for (RunningQuestion& rq : kv.second) rq.sortAnswers();
    }
    return out;
}

// ------------------------------------------------------------------ public operations

void FileManager::loadAll(ExamManager& m) const {
    loadQuestionTypes(m);
    loadStatuses(m);
    loadUsers(m);
    loadSubjects(m);
    loadSections(m);
    loadExams(m);
    loadExamDetails(m);
    loadQuestions(m);
    loadAnswers(m);
    m.validateIntegrity();                                   // rules 5 and 6
    reloadStudentData(m);
}

void FileManager::reloadStudentData(ExamManager& m) const {
    auto reservations = parseReservations(m);
    auto attempts = parseAttempts();
    try {
        m.syncStudentData(std::move(reservations), std::move(attempts));
    } catch (const NotFoundException& e) {
        throw FileFormatException(kReservations, 0, e.what());
    }
}

void FileManager::saveProgress(const ExamManager& m, const ExamReservation& reservation,
                               const std::vector<RunningQuestion>& questions) const {
    for (const RunningQuestion& rq : questions)
        if (rq.reservationId() != reservation.id())
            throw ValidationException("Running question belongs to a different reservation");

    auto reservations = parseReservations(m);                // fresh disk state; throws if a file is corrupt
    auto attempts = parseAttempts();
    auto existing = reservations.find(reservation.id());
    if (existing != reservations.end() && existing->second.isFinished())   // rule 11 at the storage level
        throw ExamFinishedException("Reservation " + util::toString(reservation.id()) +
                                    " is already finished; its data cannot be modified");
    reservations.erase(reservation.id());
    reservations.emplace(reservation.id(), reservation);
    attempts[reservation.id()] = questions;

    std::vector<Fields> answerRows, runningRows, reservationRows;
    for (const auto& kv : attempts) {
        for (const RunningQuestion& rq : kv.second) {
            runningRows.push_back({util::toString(rq.reservationId()), util::toString(rq.position()),
                                   util::toString(rq.questionNumber()), util::toString(rq.score()),
                                   rq.subjectCode(), rq.sectionCode()});
            for (const RunningAnswer& a : rq.answers())
                answerRows.push_back({util::toString(rq.reservationId()), util::toString(rq.position()),
                                      util::toString(a.answerId()), flag(a.isCorrect()), flag(a.isSelected())});
        }
    }
    for (const auto& kv : reservations) {
        const ExamReservation& r = kv.second;
        reservationRows.push_back({util::toString(r.id()), r.username(), r.date(), r.examCode(),
                                   util::optionalToField(r.curQuestion()), util::optionalToField(r.score()),
                                   util::toString(toInt(r.status())), util::optionalToField(r.remTimeMinutes()),
                                   util::toString(r.passScore()), util::toString(r.maxScore())});
    }
    // Order matters: if we crash midway the reservation (written last) is never ahead of its answers.
    writeRecords(kRunningAns, answerRows);
    writeRecords(kRunning, runningRows);
    writeRecords(kReservations, reservationRows);
}

} // namespace ems
