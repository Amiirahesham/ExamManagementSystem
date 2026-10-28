#ifndef EMS_SUBJECT_H
#define EMS_SUBJECT_H

#include <ostream>
#include <string>

namespace ems {

class Subject {
public:
    Subject(std::string code, std::string name, int hours);
    const std::string& code() const { return code_; }
    const std::string& name() const { return name_; }
    int hours() const { return hours_; }
    void display(std::ostream& os) const;
private:
    std::string code_, name_;
    int hours_;
};

} // namespace ems
#endif
