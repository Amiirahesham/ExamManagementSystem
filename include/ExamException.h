#ifndef EMS_EXAM_EXCEPTION_H
#define EMS_EXAM_EXCEPTION_H

#include <cstddef>
#include <stdexcept>
#include <string>

#include "Util.h"

namespace ems {

// Root of the project's exception hierarchy (derived from std::runtime_error).
class ExamException : public std::runtime_error {
public:
    explicit ExamException(const std::string& message) : std::runtime_error(message) {}
};

// A data file is missing, unreadable or cannot be written.
class FileException : public ExamException { public: using ExamException::ExamException; };

// A record inside a data file is malformed (wrong field count, bad number, ...).
class FileFormatException : public ExamException {
public:
    FileFormatException(const std::string& file, std::size_t line, const std::string& detail)
        : ExamException(file + (line ? ":" + util::toString(static_cast<long long>(line)) : std::string()) + ": " + detail),
          file_(file), line_(line) {}
    const std::string& file() const { return file_; }
    std::size_t line() const { return line_; }
private:
    std::string file_;
    std::size_t line_;
};

class DuplicateIdException : public ExamException { public: using ExamException::ExamException; };
class ValidationException : public ExamException { public: using ExamException::ExamException; };
class InvalidQuestionTypeException : public ValidationException { public: using ValidationException::ValidationException; };
class NotFoundException : public ExamException { public: using ExamException::ExamException; };
class AuthenticationException : public ExamException { public: using ExamException::ExamException; };
class AccessDeniedException : public ExamException { public: using ExamException::ExamException; };
class InvalidSelectionException : public ExamException { public: using ExamException::ExamException; };
class InsufficientQuestionsException : public ExamException { public: using ExamException::ExamException; };
// Reservation is missing / not owned by the student / in the wrong state.
class ReservationStateException : public ExamException { public: using ExamException::ExamException; };
// Attempt to resume or modify an exam that is already finished (rules 9 and 11).
class ExamFinishedException : public ReservationStateException { public: using ReservationStateException::ReservationStateException; };
// The exam time ran out; the attempt was submitted automatically.
class TimeExpiredException : public ExamException { public: using ExamException::ExamException; };

} // namespace ems
#endif
