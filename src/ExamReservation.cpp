#include "ExamReservation.h"

#include "ExamException.h"
#include "Util.h"

namespace ems {

ReservationStatus statusFromInt(int value) {
    if (value < 1 || value > 4) throw ValidationException("Unknown reservation status code " + util::toString(value));
    return static_cast<ReservationStatus>(value);
}

ExamReservation::ExamReservation(int id, std::string username, std::string date, std::string examCode,
                                 OptionalInt curQuestion, OptionalInt score,
                                 ReservationStatus status, OptionalInt remTimeMinutes, int passScore,
                                 int maxScore)
    : id_(id), username_(std::move(username)), date_(std::move(date)), examCode_(std::move(examCode)),
      cur_(curQuestion), score_(score), status_(status), rem_(remTimeMinutes), pass_(passScore), max_(maxScore) {
    if (id_ <= 0) throw ValidationException("reservation_id must be positive");
    if (username_.empty() || examCode_.empty()) throw ValidationException("Reservation needs username and exam code");
    if (!util::isValidDate(date_)) throw ValidationException("Invalid exam_date '" + date_ + "' (expected YYYY-MM-DD)");
    if (max_ <= 0 || pass_ < 0 || pass_ > max_) throw ValidationException("pass_score must not exceed max_score");
    if (cur_ && *cur_ < 1) throw ValidationException("cur_question must be >= 1");
    if (rem_ && *rem_ < 0) throw ValidationException("rem_time must not be negative");
    if (score_ && (*score_ < 0 || *score_ > max_)) throw ValidationException("score must be between 0 and max_score");
}

void ExamReservation::requireModifiable() const {
    if (status_ == ReservationStatus::Finished)
        throw ExamFinishedException("Reservation " + util::toString(id_) + " is finished and cannot be modified");
}

void ExamReservation::start(int durationMinutes) {
    requireModifiable();
    if (status_ != ReservationStatus::Active)
        throw ReservationStateException("Reservation " + util::toString(id_) + " is not active, cannot start");
    if (durationMinutes <= 0) throw ValidationException("Invalid exam duration");
    status_ = ReservationStatus::Running;
    cur_ = 1;
    rem_ = durationMinutes;
    score_.reset();
}

void ExamReservation::updateProgress(int curQuestion, int remMinutes) {
    requireModifiable();
    if (status_ != ReservationStatus::Running)
        throw ReservationStateException("Reservation " + util::toString(id_) + " is not running");
    if (curQuestion < 1 || remMinutes < 0) throw ValidationException("Invalid progress values");
    cur_ = curQuestion;
    rem_ = remMinutes;
}

void ExamReservation::finish(int score, int remMinutes) {
    requireModifiable();
    if (status_ != ReservationStatus::Running)
        throw ReservationStateException("Reservation " + util::toString(id_) + " is not running");
    if (score < 0 || score > max_ || remMinutes < 0) throw ValidationException("Invalid final score");
    status_ = ReservationStatus::Finished;
    score_ = score;
    rem_ = remMinutes;
}

} // namespace ems
