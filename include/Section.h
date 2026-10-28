#ifndef EMS_SECTION_H
#define EMS_SECTION_H

#include <ostream>
#include <string>

namespace ems {

class Section {
public:
    Section(std::string subjectCode, std::string code, std::string name);
    const std::string& subjectCode() const { return subjectCode_; }
    const std::string& code() const { return code_; }
    const std::string& name() const { return name_; }
    void display(std::ostream& os) const;
private:
    std::string subjectCode_, code_, name_;
};

} // namespace ems
#endif
