#include "ExamSession.h"

#include <algorithm>

#include "ExamException.h"
#include "Util.h"

namespace ems {

// Rule 8 (valid reservation) and rule 9 (finished exams cannot be resumed).
void ExamSession::checkStartable(const ExamManager& manager, const std::string& username, int reservationId) {
    const ExamReservation* res = manager.findReservation(reservationId);
    if (!res || res->username() != username)               // same message: do not reveal other users' data
        throw ReservationStateException("No valid reservation " + util::toString(reservationId) +
                                        " exists for user '" + username + "'");
    switch (res->status()) {
        case ReservationStatus::Finished:
            throw ExamFinishedException("This examination is finished and cannot be resumed or modified");
        case ReservationStatus::Reserved:
            throw ReservationStateException("This reservation is not active yet (status: " +
                                            manager.statusName(res->status()) + "); it cannot be started");
        case ReservationStatus::Active:
            if (manager.exam(res->examCode()).details().empty())
                throw ValidationException("Examination '" + res->examCode() +
                                          "' has no question requirements (exam_details) yet and cannot be started");
            break;
        case ReservationStatus::Running:
            break;
    }
}

std::unique_ptr<ExamSession> ExamSession::open(ExamManager& manager, const FileManager& files,
                                               const std::string& username, int reservationId,
                                               const ITimeSource& clock, std::mt19937& rng) {
    files.reloadStudentData(manager);                      // work on the current state of the files
    checkStartable(manager, username, reservationId);
    ExamReservation* res = manager.findReservation(reservationId);
    const Exam& exam = manager.exam(res->examCode());

    if (res->status() == ReservationStatus::Active) {      // ---- fresh start
        std::vector<RunningQuestion> generated = manager.generateExam(*res, rng);   // may throw (rule 7)
        ExamReservation started = *res;
        started.start(exam.durationMinutes());
        files.saveProgress(manager, started, generated);   // persist first ...
        *res = started;                                    // ... then commit in memory
        std::vector<RunningQuestion>& stored = manager.attempt(res->id());
        stored = std::move(generated);
        return std::unique_ptr<ExamSession>(new ExamSession(manager, files, *res, exam, stored, clock,
                                                            std::chrono::seconds(exam.durationMinutes() * 60), 0, false));
    }

    // ---- resume of an interrupted exam
    std::vector<RunningQuestion>& stored = manager.attempt(res->id());
    manager.linkAttempt(stored, exam, true);
    const int minutes = std::min(res->remTimeMinutes().value_or(exam.durationMinutes()), exam.durationMinutes());
    std::size_t index = static_cast<std::size_t>(std::max(1, res->curQuestion().value_or(1))) - 1;
    index = std::min(index, stored.size() - 1);            // recover from an out-of-range cur_question
    return std::unique_ptr<ExamSession>(new ExamSession(manager, files, *res, exam, stored, clock,
                                                        std::chrono::seconds(minutes * 60), index, true));
}

ExamSession::ExamSession(ExamManager& manager, const FileManager& files, ExamReservation& reservation,
                         const Exam& exam, std::vector<RunningQuestion>& questions, const ITimeSource& clock,
                         std::chrono::seconds remaining, std::size_t index, bool resumed)
    : manager_(manager), files_(files), reservation_(reservation), exam_(exam), questions_(questions),
      timer_(clock, remaining), index_(index), resumed_(resumed) {}

std::size_t ExamSession::answeredCount() const {
    return static_cast<std::size_t>(std::count_if(questions_.begin(), questions_.end(),
                                                  [](const RunningQuestion& q) { return q.isAnswered(); }));
}

void ExamSession::enforceOpen() {
    if (finished_ || reservation_.isFinished())            // rule 11
        throw ExamFinishedException("The examination has been submitted; answers can no longer be modified");
    if (finishIfExpired())
        throw TimeExpiredException("Time is over: the examination was submitted automatically");
}

void ExamSession::autosave() {
    ExamReservation updated = reservation_;
    updated.updateProgress(static_cast<int>(index_) + 1, timer_.remainingMinutesCeil());
    files_.saveProgress(manager_, updated, questions_);
    reservation_ = updated;
}

void ExamSession::select(const std::set<int>& answerIds) {
    enforceOpen();
    RunningQuestion& rq = questions_[index_];
    const std::set<int> previous = rq.selectedIds();
    rq.setSelection(answerIds);                            // throws before any change if invalid
    try {
        autosave();
    } catch (...) {
        rq.setSelection(previous);                         // keep memory consistent with disk
        throw;
    }
}

bool ExamSession::next() {
    enforceOpen();
    if (index_ + 1 >= questions_.size()) return false;
    ++index_;
    try { autosave(); } catch (...) { --index_; throw; }
    return true;
}

bool ExamSession::previous() {
    enforceOpen();
    if (index_ == 0) return false;
    --index_;
    try { autosave(); } catch (...) { ++index_; throw; }
    return true;
}

void ExamSession::suspend() {
    enforceOpen();
    autosave();
}

bool ExamSession::finishIfExpired() {
    if (finished_ || !timer_.expired()) return false;
    finalize(true);
    return true;
}

ExamResult ExamSession::finalize(bool timedOut) {
    ExamResult result = manager_.gradeExam(reservation_, questions_);
    result.timedOut = timedOut;
    ExamReservation done = reservation_;
    done.finish(result.score, timedOut ? 0 : timer_.remainingMinutesCeil());
    files_.saveProgress(manager_, done, questions_);       // persist, then commit
    reservation_ = done;
    result_ = result;
    finished_ = true;
    return result;
}

ExamResult ExamSession::submit() {
    if (finished_ || reservation_.isFinished())
        throw ExamFinishedException("The examination has already been submitted");
    return finalize(timer_.expired());
}

const ExamResult& ExamSession::result() const {
    if (!finished_) throw ReservationStateException("The examination has not been submitted yet");
    return result_;
}

} // namespace ems
