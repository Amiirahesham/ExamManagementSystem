#ifndef EMS_OPTIONAL_INT_H
#define EMS_OPTIONAL_INT_H

#include <stdexcept>

namespace ems {

// Minimal replacement for std::optional<int> (C++17, unavailable in GCC 5.1 / C++14).
// Models the "empty column" of the text files (e.g. cur_question, score, rem_time).
class OptionalInt {
public:
    OptionalInt() : has_(false), value_(0) {}
    OptionalInt(int value) : has_(true), value_(value) {}   // implicit on purpose: `x = 5;`

    bool has_value() const { return has_; }
    explicit operator bool() const { return has_; }
    int operator*() const { return get(); }
    int value() const { return get(); }
    int value_or(int fallback) const { return has_ ? value_ : fallback; }
    void reset() { has_ = false; value_ = 0; }

    bool operator==(const OptionalInt& o) const { return has_ == o.has_ && (!has_ || value_ == o.value_); }
    bool operator!=(const OptionalInt& o) const { return !(*this == o); }

private:
    int get() const {
        if (!has_) throw std::logic_error("OptionalInt: no value");
        return value_;
    }
    bool has_;
    int value_;
};

} // namespace ems
#endif
