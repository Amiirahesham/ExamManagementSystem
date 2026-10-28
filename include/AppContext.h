#ifndef EMS_APP_CONTEXT_H
#define EMS_APP_CONTEXT_H

#include <iosfwd>
#include <random>

namespace ems {

class ExamManager;
class FileManager;
class ITimeSource;

// Plain bundle of collaborators handed to console workflows (keeps I/O injectable for tests).
struct AppContext {
    ExamManager& manager;
    const FileManager& files;
    std::istream& in;
    std::ostream& out;
    const ITimeSource& clock;
    std::mt19937& rng;
};

} // namespace ems
#endif
