#ifndef EMS_EXAM_SESSION_H
#define EMS_EXAM_SESSION_H

#include <chrono>
#include <cstddef>
#include <memory>
#include <random>
#include <set>
#include <string>
#include <vector>

#include "Exam.h"
#include "ExamManager.h"
#include "ExamReservation.h"
#include "ExamResult.h"
#include "ExamTimer.h"
#include "FileManager.h"
#include "RunningQuestion.h"

namespace ems {

// One student's attempt (start or resume). Enforces rules 8-11, autosaves after every change
// and tracks the remaining time with std::chrono. All state changes are persisted BEFORE being
// committed in memory, so a failed save never leaves memory and disk diverging.
class ExamSession {
public:
    // Throws unless the reservation exists, belongs to `username` and is Active or Running.
    static void checkStartable(const ExamManager& manager, const std::string& username, int reservationId);
    static std::unique_ptr<ExamSession> open(ExamManager& manager, const FileManager& files,
                                             const std::string& username, int reservationId,
                                             const ITimeSource& clock, std::mt19937& rng);

    bool resumed() const { return resumed_; }
    const Exam& exam() const { return exam_; }
    const ExamReservation& reservation() const { return reservation_; }
    std::size_t questionCount() const { return questions_.size(); }
    std::size_t currentIndex() const { return index_; }
    const RunningQuestion& current() const { return questions_[index_]; }
    const RunningQuestion& at(std::size_t index) const { return questions_.at(index); }
    std::size_t answeredCount() const;
    bool isFinished() const { return finished_; }
    std::chrono::seconds remaining() const { return timer_.remaining(); }
    bool expired() const { return timer_.expired(); }

    void select(const std::set<int>& answerIds);           // empty set clears; single/multiple rules
    bool next();                                           // false at the last question
    bool previous();                                       // false at the first question
    void suspend();                                        // save position + remaining time now
    bool finishIfExpired();                                // auto-submits when time is over
    ExamResult submit();                                   // final submission (rule 11 afterwards)
    const ExamResult& result() const;

private:
    ExamSession(ExamManager& manager, const FileManager& files, ExamReservation& reservation,
                const Exam& exam, std::vector<RunningQuestion>& questions, const ITimeSource& clock,
                std::chrono::seconds remaining, std::size_t index, bool resumed);
    void enforceOpen();
    void autosave();
    ExamResult finalize(bool timedOut);

    ExamManager& manager_;
    const FileManager& files_;
    ExamReservation& reservation_;
    const Exam& exam_;
    std::vector<RunningQuestion>& questions_;
    ExamTimer timer_;
    std::size_t index_;
    bool resumed_;
    bool finished_ = false;
    ExamResult result_;
};

} // namespace ems
#endif
