#ifndef EMS_EXAM_TIMER_H
#define EMS_EXAM_TIMER_H

#include <chrono>

namespace ems {

using SteadyClock = std::chrono::steady_clock;

// Abstract time source (runtime polymorphism): production uses SteadyTimeSource,
// tests inject a manually advanced source so timeouts are deterministic.
class ITimeSource {
public:
    virtual ~ITimeSource() = default;
    virtual SteadyClock::time_point now() const = 0;
};

class SteadyTimeSource : public ITimeSource {
public:
    SteadyClock::time_point now() const override { return SteadyClock::now(); }
};

// Counts down against a fixed deadline measured with std::chrono, independent of user actions.
class ExamTimer {
public:
    ExamTimer(const ITimeSource& source, std::chrono::seconds remaining)
        : source_(source), deadline_(source.now() + remaining) {}

    std::chrono::seconds remaining() const {
        auto left = std::chrono::duration_cast<std::chrono::seconds>(deadline_ - source_.now());
        return left.count() > 0 ? left : std::chrono::seconds(0);
    }
    bool expired() const { return source_.now() >= deadline_; }
    // Whole minutes rounded up (the workbook's rem_time column is in minutes).
    int remainingMinutesCeil() const { return static_cast<int>((remaining().count() + 59) / 60); }

private:
    const ITimeSource& source_;
    SteadyClock::time_point deadline_;
};

} // namespace ems
#endif
