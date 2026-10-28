#ifndef EMS_STUDENT_H
#define EMS_STUDENT_H

#include "User.h"

namespace ems {

class ExamReservation;
class ExamSession;

// The Student console workflow (login screen lives in main.cpp):
//   Screen 2 viewReservations, Screen 3 showExamDetails, Screen 4 exam loop, Screen 5 result.
// ASSOCIATION: a Student is associated with its ExamReservations through the username.
class Student : public User {
public:
    Student(std::string firstName, std::string lastName, std::string email, std::string username,
            std::string passwordHash);

    void showMenu(AppContext& context) override;           // interactive main menu
    void viewReservations(AppContext& context) const;      // Screen 2
    void showExamDetails(AppContext& context, const ExamReservation& reservation) const;   // Screen 3
    void startExam(AppContext& context);                   // start or resume (Screens 3-5)
    void viewResult(AppContext& context) const;            // result of a finished exam

private:
    const ExamReservation* chooseReservation(AppContext& context, const std::string& purpose) const;
    void runExam(AppContext& context, ExamSession& session) const;   // Screen 4 loop
};

} // namespace ems
#endif
