#include "ExamManager.h"

#include <algorithm>
#include <set>

#include "ExamException.h"
#include "PasswordHasher.h"
#include "Util.h"

namespace ems {

namespace {
std::string qid(const std::string& subject, int number) { return subject + "/" + util::toString(number); }
}

// ---------------------------------------------------------------- population

void ExamManager::setQuestionTypes(std::map<int, std::string> types) { questionTypes_ = std::move(types); }
void ExamManager::setStatusNames(std::map<int, std::string> names) { statusNames_ = std::move(names); }

void ExamManager::addUser(std::unique_ptr<User> user) {
    if (!user) throw ValidationException("Null user");
    const std::string name = user->username();
    if (users_.count(name)) throw DuplicateIdException("Duplicate username '" + name + "'");   // rule 1
    users_.emplace(name, std::move(user));
}

void ExamManager::addSubject(Subject subject) {
    const std::string code = subject.code();
    if (subjects_.count(code)) throw DuplicateIdException("Duplicate subject code '" + code + "'");
    subjects_.emplace(code, std::move(subject));
}

void ExamManager::addSection(Section section) {
    if (!subjects_.count(section.subjectCode()))                                           // rule 3
        throw NotFoundException("Section " + section.code() + " refers to unknown subject '" + section.subjectCode() + "'");
    const auto key = std::make_pair(section.subjectCode(), section.code());
    if (sections_.count(key))
        throw DuplicateIdException("Duplicate section " + key.first + "/" + key.second);
    sections_.emplace(key, std::move(section));
}

void ExamManager::addExam(Exam exam) {
    const std::string code = exam.code();
    if (exams_.count(code)) throw DuplicateIdException("Duplicate exam code '" + code + "'");   // rule 2
    exams_.emplace(code, std::move(exam));
}

void ExamManager::addExamDetail(const std::string& examCode, const ExamDetail& detail) {
    auto it = exams_.find(examCode);
    if (it == exams_.end()) throw NotFoundException("Exam detail refers to unknown exam '" + examCode + "'");
    section(detail.subjectCode(), detail.sectionCode());   // throws NotFoundException if missing
    it->second += detail;                                  // rejects duplicates
}

void ExamManager::addQuestion(std::unique_ptr<Question> question) {
    if (!question) throw ValidationException("Null question");
    if (!questionTypes_.count(question->typeId()))
        throw InvalidQuestionTypeException("Question type " + util::toString(question->typeId()) + " is not defined in question_types.txt");
    section(question->subjectCode(), question->sectionCode());                            // rule 4
    const auto key = std::make_pair(question->subjectCode(), question->number());
    if (questions_.count(key)) throw DuplicateIdException("Duplicate question " + qid(key.first, key.second));
    sectionIndex_[{question->subjectCode(), question->sectionCode()}].push_back(question.get());
    questions_.emplace(key, std::move(question));
}

void ExamManager::addAnswer(const std::string& subjectCode, int questionNumber, const Answer& answer) {
    auto it = questions_.find({subjectCode, questionNumber});
    if (it == questions_.end())
        throw NotFoundException("Answer refers to unknown question " + qid(subjectCode, questionNumber));
    it->second->addAnswer(answer);
}

void ExamManager::validateIntegrity() const {
    for (const auto& kv : questions_) kv.second->validate();                              // rules 5, 6
}

void ExamManager::syncStudentData(std::map<int, ExamReservation> reservations,
                                  std::map<int, std::vector<RunningQuestion>> attempts) {
    for (const auto& kv : reservations) {                  // validate everything before mutating
        if (!users_.count(kv.second.username()))
            throw NotFoundException("Reservation " + util::toString(kv.first) + " refers to unknown user '" + kv.second.username() + "'");
        if (!exams_.count(kv.second.examCode()))
            throw NotFoundException("Reservation " + util::toString(kv.first) + " refers to unknown exam '" + kv.second.examCode() + "'");
    }
    for (const auto& kv : attempts)
        if (!reservations.count(kv.first))
            throw NotFoundException("Running data refers to unknown reservation " + util::toString(kv.first));

    for (auto it = reservations_.begin(); it != reservations_.end();)
        it = reservations.count(it->first) ? std::next(it) : reservations_.erase(it);
    for (auto& kv : reservations) {
        auto f = reservations_.find(kv.first);
        if (f == reservations_.end()) reservations_.emplace(kv.first, std::move(kv.second));
        else f->second = std::move(kv.second);
    }
    for (auto it = attempts_.begin(); it != attempts_.end();)
        it = attempts.count(it->first) ? std::next(it) : attempts_.erase(it);
    for (auto& kv : attempts) attempts_[kv.first] = std::move(kv.second);
}

// ---------------------------------------------------------------- queries

const Subject& ExamManager::subject(const std::string& code) const {
    auto it = subjects_.find(code);
    if (it == subjects_.end()) throw NotFoundException("Unknown subject '" + code + "'");
    return it->second;
}

const Section& ExamManager::section(const std::string& subjectCode, const std::string& sectionCode) const {
    auto it = sections_.find({subjectCode, sectionCode});
    if (it == sections_.end()) throw NotFoundException("Unknown section " + subjectCode + "/" + sectionCode);
    return it->second;
}

const Exam& ExamManager::exam(const std::string& code) const {
    auto it = exams_.find(code);
    if (it == exams_.end()) throw NotFoundException("Unknown examination code '" + code + "'");
    return it->second;
}

const Question* ExamManager::findQuestion(const std::string& subjectCode, int number) const {
    auto it = questions_.find({subjectCode, number});
    return it == questions_.end() ? nullptr : it->second.get();
}

const Question& ExamManager::question(const std::string& subjectCode, int number) const {
    const Question* q = findQuestion(subjectCode, number);
    if (!q) throw NotFoundException("Unknown question " + qid(subjectCode, number));
    return *q;
}

const std::vector<const Question*>& ExamManager::questionsIn(const std::string& subjectCode,
                                                             const std::string& sectionCode) const {
    static const std::vector<const Question*> kEmpty;
    auto it = sectionIndex_.find({subjectCode, sectionCode});
    return it == sectionIndex_.end() ? kEmpty : it->second;
}

std::string ExamManager::statusName(ReservationStatus status) const {
    auto it = statusNames_.find(toInt(status));
    return it == statusNames_.end() ? "Status " + util::toString(toInt(status)) : it->second;
}

std::string ExamManager::questionTypeName(int typeId) const {
    auto it = questionTypes_.find(typeId);
    return it == questionTypes_.end() ? "Type " + util::toString(typeId) : it->second;
}

// ---------------------------------------------------------------- authentication

User& ExamManager::authenticate(const std::string& username, const std::string& password) {
    static const std::string kDummyHash = PasswordHasher::hash("timing-equaliser");
    const std::string failure = "Invalid username or password";
    if (username.empty() || password.empty()) throw AuthenticationException(failure);
    auto it = users_.find(username);
    if (it == users_.end()) {
        PasswordHasher::verify(password, kDummyHash);       // same work as a real check (timing)
        throw AuthenticationException(failure);
    }
    if (!it->second->verifyPassword(password)) throw AuthenticationException(failure);
    return *it->second;
}

Student& ExamManager::authenticateStudent(const std::string& username, const std::string& password) {
    User& user = authenticate(username, password);
    Student* student = dynamic_cast<Student*>(&user);
    if (!student) throw AccessDeniedException("This account is not a student account");
    return *student;
}

// ---------------------------------------------------------------- reservations & attempts

ExamReservation* ExamManager::findReservation(int id) {
    auto it = reservations_.find(id);
    return it == reservations_.end() ? nullptr : &it->second;
}

const ExamReservation* ExamManager::findReservation(int id) const {
    auto it = reservations_.find(id);
    return it == reservations_.end() ? nullptr : &it->second;
}

std::vector<const ExamReservation*> ExamManager::reservationsFor(const std::string& username) const {
    std::vector<const ExamReservation*> list;
    for (const auto& kv : reservations_) if (kv.second.username() == username) list.push_back(&kv.second);
    return list;                                           // map order = ascending reservation_id
}

std::vector<RunningQuestion>& ExamManager::attempt(int reservationId) { return attempts_[reservationId]; }

const std::vector<RunningQuestion>* ExamManager::findAttempt(int reservationId) const {
    auto it = attempts_.find(reservationId);
    return it == attempts_.end() ? nullptr : &it->second;
}

// ---------------------------------------------------------------- exam logic

std::vector<RunningQuestion> ExamManager::generateExam(const ExamReservation& reservation,
                                                       std::mt19937& rng) const {
    const Exam& e = exam(reservation.examCode());
    if (e.details().empty())
        throw ValidationException("Examination '" + e.code() + "' has no question requirements (exam_details) yet");

    std::string shortage;                                   // rule 7: check everything first
    for (const ExamDetail& d : e.details()) {
        const std::size_t available = questionsIn(d.subjectCode(), d.sectionCode()).size();
        if (available < static_cast<std::size_t>(d.questionCount()))
            shortage += (shortage.empty() ? "" : "; ") + d.subjectCode() + "/" + d.sectionCode() + " needs " +
                        util::toString(d.questionCount()) + " but only " + util::toString(available) + " available";
    }
    if (!shortage.empty()) throw InsufficientQuestionsException("Insufficient questions: " + shortage);

    std::vector<RunningQuestion> result;
    int position = 1;
    for (const ExamDetail& d : e.details()) {
        std::vector<const Question*> pool = questionsIn(d.subjectCode(), d.sectionCode());
        std::shuffle(pool.begin(), pool.end(), rng);
        for (int i = 0; i < d.questionCount(); ++i)
            result.push_back(RunningQuestion::fromQuestion(reservation.id(), position++, *pool[static_cast<std::size_t>(i)]));
    }
    return result;
}

void ExamManager::linkAttempt(std::vector<RunningQuestion>& questions, const Exam& e, bool strict) const {
    if (strict && questions.empty())
        throw ValidationException("The running examination has no stored questions");
    std::sort(questions.begin(), questions.end());
    for (std::size_t i = 0; i < questions.size(); ++i) {
        RunningQuestion& rq = questions[i];
        const std::string where = "res_id " + util::toString(rq.reservationId()) + " quest_no " + util::toString(rq.position());
        if (strict && rq.position() != static_cast<int>(i) + 1)
            throw FileFormatException("exam_running.txt", 0, "question positions are not 1..N (" + where + ")");

        const Question* q = nullptr;
        if (rq.subjectCode().empty()) {                     // legacy 4-column row: resolve through the exam
            std::set<std::string> subjects;
            for (const ExamDetail& d : e.details()) subjects.insert(d.subjectCode());
            std::vector<const Question*> found;
            for (const std::string& s : subjects)
                if (const Question* c = findQuestion(s, rq.questionNumber())) found.push_back(c);
            if (found.size() == 1) q = found.front();
            else if (strict)
                throw FileFormatException("exam_running.txt", 0, "cannot resolve question number " +
                                          util::toString(rq.questionNumber()) + " uniquely (" + where + ")");
        } else {
            q = findQuestion(rq.subjectCode(), rq.questionNumber());
        }
        if (!q) {
            if (strict) throw NotFoundException("Question " + util::toString(rq.questionNumber()) + " of " + where + " no longer exists");
            continue;
        }
        if (rq.subjectCode().empty() || rq.sectionCode().empty()) rq.setClassification(q->subjectCode(), q->sectionCode());
        else if (strict && rq.sectionCode() != q->sectionCode())
            throw ValidationException("Section mismatch between exam_running and question bank (" + where + ")");

        if (strict) {                                       // frozen snapshot must still match the bank
            bool same = rq.answers().size() == q->answerCount();
            for (std::size_t k = 0; same && k < q->answerCount(); ++k) {
                const Answer& a = (*q)[k];
                same = rq.answers()[k].answerId() == a.id() && rq.answers()[k].isCorrect() == a.isCorrect();
            }
            if (!same)
                throw ValidationException("Question " + qid(q->subjectCode(), q->number()) +
                                          " changed after the exam was generated (" + where + "); please contact the administrator");
        }
        rq.link(*q);
    }
}

int ExamManager::computeScore(int earnedMarks, int totalMarks, int maxScore) {
    if (totalMarks <= 0) return 0;
    const long long numerator = static_cast<long long>(earnedMarks) * maxScore;
    return static_cast<int>((2 * numerator + totalMarks) / (2LL * totalMarks));   // round half up
}

ExamResult ExamManager::summarize(const ExamReservation& reservation,
                                  const std::vector<RunningQuestion>& questions) const {
    ExamResult r;
    r.examCode = reservation.examCode();
    r.examName = hasExam(r.examCode) ? exam(r.examCode).name() : r.examCode;
    r.maxScore = reservation.maxScore();
    r.passScore = reservation.passScore();
    std::map<std::pair<std::string, std::string>, std::size_t> index;
    for (const RunningQuestion& rq : questions) {
        const auto key = std::make_pair(rq.subjectCode(), rq.sectionCode());
        auto it = index.find(key);
        if (it == index.end()) {
            SectionResult s;
            s.subjectCode = key.first;
            s.sectionCode = key.second;
            s.subjectName = key.first.empty() ? "(unclassified)" : key.first;
            s.sectionName = key.second.empty() ? "-" : key.second;
            try {
                if (!key.first.empty()) s.subjectName = subject(key.first).name();
                if (!key.first.empty() && !key.second.empty()) s.sectionName = section(key.first, key.second).name();
            } catch (const NotFoundException&) { /* keep codes as labels */ }
            r.sections.push_back(s);
            it = index.emplace(key, r.sections.size() - 1).first;
        }
        SectionResult& s = r.sections[it->second];
        ++s.questions;
        s.correct += rq.score() > 0 ? 1 : 0;
        r.earnedMarks += rq.score();
        r.totalMarks += RunningQuestion::kMarksPerQuestion;
    }
    return r;
}

ExamResult ExamManager::gradeExam(const ExamReservation& reservation, std::vector<RunningQuestion>& questions) const {
    for (RunningQuestion& rq : questions) rq.calculateScore();   // polymorphic exact-match grading
    ExamResult r = summarize(reservation, questions);
    r.score = computeScore(r.earnedMarks, r.totalMarks, reservation.maxScore());
    r.passed = r.score >= reservation.passScore();
    return r;
}

ExamResult ExamManager::storedResult(const ExamReservation& reservation) const {
    if (!reservation.isFinished())
        throw ReservationStateException("Reservation " + util::toString(reservation.id()) + " is not finished yet");
    std::vector<RunningQuestion> copy;
    if (const std::vector<RunningQuestion>* stored = findAttempt(reservation.id())) copy = *stored;
    linkAttempt(copy, exam(reservation.examCode()), false);   // best effort: only section labels
    ExamResult r = summarize(reservation, copy);
    r.score = reservation.score() ? *reservation.score() : computeScore(r.earnedMarks, r.totalMarks, r.maxScore);
    r.passed = r.score >= reservation.passScore();
    return r;
}

} // namespace ems
