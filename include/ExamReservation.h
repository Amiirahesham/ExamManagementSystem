#ifndef EMS_EXAM_RESERVATION_H
#define EMS_EXAM_RESERVATION_H

#include <string>

#include "OptionalInt.h"

namespace ems {

enum class ReservationStatus { Reserved = 1, Active = 2, Running = 3, Finished = 4 };
ReservationStatus statusFromInt(int value);                // throws ValidationException
inline int toInt(ReservationStatus s) { return static_cast<int>(s); }

// A row of exam_reservations.txt. Empty columns are modelled with OptionalInt.
class ExamReservation {
public:
    ExamReservation(int id, std::string username, std::string date, std::string examCode,
                    OptionalInt curQuestion, OptionalInt score, ReservationStatus status,
                    OptionalInt remTimeMinutes, int passScore, int maxScore);

    int id() const { return id_; }
    const std::string& username() const { return username_; }
    const std::string& date() const { return date_; }
    const std::string& examCode() const { return examCode_; }
    const OptionalInt& curQuestion() const { return cur_; }
    const OptionalInt& score() const { return score_; }
    ReservationStatus status() const { return status_; }
    const OptionalInt& remTimeMinutes() const { return rem_; }
    int passScore() const { return pass_; }
    int maxScore() const { return max_; }

    bool isFinished() const { return status_ == ReservationStatus::Finished; }
    bool isRunning() const { return status_ == ReservationStatus::Running; }

    void start(int durationMinutes);                       // Active -> Running
    void updateProgress(int curQuestion, int remMinutes);  // Running only
    void finish(int score, int remMinutes);                // Running -> Finished
    void requireModifiable() const;                        // throws ExamFinishedException

private:
    int id_;
    std::string username_, date_, examCode_;
    OptionalInt cur_, score_;
    ReservationStatus status_;
    OptionalInt rem_;
    int pass_, max_;
};

} // namespace ems
#endif
