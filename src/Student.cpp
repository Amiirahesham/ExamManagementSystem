#include "Student.h"

#include <iomanip>
#include <iostream>

#include "AppContext.h"
#include "ExamException.h"
#include "ExamManager.h"
#include "ExamSession.h"
#include "FileManager.h"
#include "Util.h"

namespace ems {

namespace {

// Returns false when the input stream is exhausted (treated as "quit").
bool promptLine(AppContext& ctx, const std::string& prompt, std::string& out) {
    ctx.out << prompt << std::flush;
    if (!std::getline(ctx.in, out)) return false;
    out = util::trim(out);
    return true;
}

std::vector<const ExamReservation*> freshReservations(AppContext& ctx, const std::string& username) {
    ctx.files.reloadStudentData(ctx.manager);              // always show what is really on disk
    return ctx.manager.reservationsFor(username);
}

void printTable(AppContext& ctx, const std::vector<const ExamReservation*>& list) {
    ctx.out << "\nReserved Exams :\n"
            << std::left << std::setw(4) << " #" << std::setw(46) << "exam Name" << std::setw(12) << "Date"
            << std::setw(10) << "Status" << "Score\n"
            << std::string(78, '-') << '\n';
    int n = 1;
    for (const ExamReservation* r : list) {
        std::string name = ctx.manager.hasExam(r->examCode()) ? ctx.manager.exam(r->examCode()).name() : r->examCode();
        if (name.size() > 44) name = name.substr(0, 41) + "...";
        ctx.out << std::left << std::setw(4) << n++ << std::setw(46) << name << std::setw(12) << r->date()
                << std::setw(10) << ctx.manager.statusName(r->status())
                << (r->score() ? util::toString(*r->score()) : std::string()) << '\n';
    }
}

std::string formatRemaining(std::chrono::seconds s) {
    return util::toString(s.count() / 60) + " min " + util::toString(s.count() % 60) + " sec";
}

std::set<int> parseChoice(const std::string& input, const Question& q) {
    std::set<int> ids;
    for (char c : input) {
        if (c == ' ' || c == ',' || c == ';') continue;
        const int idx = std::tolower(static_cast<unsigned char>(c)) - 'a';
        if (idx < 0 || idx >= static_cast<int>(q.answerCount()))
            throw InvalidSelectionException(std::string("Invalid option '") + c + "'. Valid options: a-" +
                                            static_cast<char>('a' + q.answerCount() - 1));
        ids.insert(q[static_cast<std::size_t>(idx)].id());
    }
    if (ids.empty()) throw InvalidSelectionException("No option given");
    return ids;
}

} // namespace

Student::Student(std::string firstName, std::string lastName, std::string email, std::string username,
                 std::string passwordHash)
    : User(std::move(firstName), std::move(lastName), std::move(email), std::move(username),
           std::move(passwordHash), Role::Student) {}

void Student::showMenu(AppContext& ctx) {
    for (;;) {
        ctx.out << "\n=== Student menu - " << fullName() << " ===\n"
                << " 1) View reserved examinations\n"
                << " 2) View examination details\n"
                << " 3) Start / resume an examination\n"
                << " 4) View the result of a finished examination\n"
                << " 0) Logout\n";
        std::string choice;
        if (!promptLine(ctx, "Choice: ", choice)) return;
        try {
            if (choice == "1") viewReservations(ctx);
            else if (choice == "2") {
                if (const ExamReservation* r = chooseReservation(ctx, "details")) showExamDetails(ctx, *r);
            }
            else if (choice == "3") startExam(ctx);
            else if (choice == "4") viewResult(ctx);
            else if (choice == "0" || util::toLower(choice) == "q") return;
            else ctx.out << "Invalid choice.\n";
        } catch (const ExamFinishedException& e) {
            ctx.out << "Not allowed: " << e.what() << '\n';
        } catch (const ExamException& e) {
            ctx.out << "Error: " << e.what() << '\n';
        } catch (const std::exception& e) {
            ctx.out << "Unexpected error: " << e.what() << '\n';
        }
    }
}

void Student::viewReservations(AppContext& ctx) const {
    const auto list = freshReservations(ctx, username());
    ctx.out << "\nuser: " << fullName() << '\n';
    if (list.empty()) { ctx.out << "You have no reserved examinations.\n"; return; }
    printTable(ctx, list);
}

const ExamReservation* Student::chooseReservation(AppContext& ctx, const std::string& purpose) const {
    const auto list = freshReservations(ctx, username());
    if (list.empty()) { ctx.out << "You have no reserved examinations.\n"; return nullptr; }
    printTable(ctx, list);
    for (;;) {
        std::string input;
        if (!promptLine(ctx, "Enter the reservation number for " + purpose + " (or q to cancel): ", input)) return nullptr;
        if (util::toLower(input) == "q") return nullptr;
        try {
            const int n = util::parseInt(input, "reservation number");
            if (n >= 1 && n <= static_cast<int>(list.size())) return list[static_cast<std::size_t>(n) - 1];
        } catch (const ValidationException&) { /* fall through to the message */ }
        ctx.out << "Please enter a number between 1 and " << list.size() << ".\n";
    }
}

void Student::showExamDetails(AppContext& ctx, const ExamReservation& r) const {
    const Exam& e = ctx.manager.exam(r.examCode());
    ctx.out << "\nexam_code  : " << e.code() << "\nExam_name  : " << e.name() << "\nExam_time  : "
            << e.durationMinutes() << " min\nmax_score  : " << r.maxScore() << "\npass_score : " << r.passScore() << "\n\n"
            << std::left << std::setw(26) << "Subject" << std::setw(22) << "section" << "no_questions\n"
            << std::string(60, '-') << '\n';
    for (const ExamDetail& d : e.details()) {
        ctx.out << std::left << std::setw(26) << ctx.manager.subject(d.subjectCode()).name()
                << std::setw(22) << ctx.manager.section(d.subjectCode(), d.sectionCode()).name()
                << d.questionCount() << '\n';
    }
    ctx.out << "\nTotal Number of Questions: " << e.totalQuestions() << '\n';
}

void Student::startExam(AppContext& ctx) {
    const ExamReservation* r = chooseReservation(ctx, "the examination to start");
    if (!r) return;
    const int id = r->id();
    ExamSession::checkStartable(ctx.manager, username(), id);   // rules 8 and 9, before any screen
    showExamDetails(ctx, *r);
    if (r->isRunning())
        ctx.out << "\nThis examination was interrupted earlier; it will be resumed where you stopped.\n";
    std::string answer;
    if (!promptLine(ctx, "\nif you ready to start press 1 or q to quit : ", answer) || answer != "1") return;

    std::unique_ptr<ExamSession> session = ExamSession::open(ctx.manager, ctx.files, username(), id, ctx.clock, ctx.rng);
    if (session->resumed()) ctx.out << "\nResuming your examination...\n";
    runExam(ctx, *session);
}

void Student::runExam(AppContext& ctx, ExamSession& session) const {
    for (;;) {
        if (session.finishIfExpired()) {
            ctx.out << session.result();
            return;
        }
        const RunningQuestion& rq = session.current();
        const Question& q = *rq.question();
        std::string where = rq.subjectCode() + "/" + rq.sectionCode();
        try { where = ctx.manager.subject(rq.subjectCode()).name() + " - " + ctx.manager.section(rq.subjectCode(), rq.sectionCode()).name(); }
        catch (const NotFoundException&) {}

        ctx.out << "\nRemaining Time: " << formatRemaining(session.remaining()) << "     Answered: "
                << session.answeredCount() << "/" << session.questionCount() << "\n"
                << "Q " << rq.position() << ") of " << session.questionCount() << "   [" << where << "]\n";
        q.display(ctx.out, rq.selectedIds());
        std::string letters;
        for (std::size_t i = 0; i < q.answerCount(); ++i) letters += std::string(i ? "," : "") + static_cast<char>('a' + i);
        ctx.out << "\nyour choice : (" << letters << ") or [n->next, p->previous, s->submit, q->save & quit, clear]\n";

        std::string input;
        if (!promptLine(ctx, "> ", input)) {
            try { session.suspend(); } catch (const ExamException&) {}
            ctx.out << "\nInput closed - your progress was saved.\n";
            return;
        }
        const std::string cmd = util::toLower(input);
        try {
            if (cmd.empty()) continue;
            if (cmd == "n") {
                if (!session.next()) ctx.out << "This is the last question.\n";
            } else if (cmd == "p") {
                if (!session.previous()) ctx.out << "This is the first question.\n";
            } else if (cmd == "clear") {
                session.select(std::set<int>());
            } else if (cmd == "q") {
                session.suspend();
                ctx.out << "Progress saved. You can resume this examination later.\n";
                return;
            } else if (cmd == "s") {
                const std::size_t open = session.questionCount() - session.answeredCount();
                std::string confirm;
                const std::string warn = open ? "You have " + util::toString(open) + " unanswered question(s). " : "";
                if (!promptLine(ctx, warn + "Submit the examination now? Answers cannot be changed afterwards (y/n): ", confirm)) continue;
                if (util::toLower(confirm) == "y") {
                    ctx.out << session.submit();
                    return;
                }
            } else {
                session.select(parseChoice(cmd, q));
            }
        } catch (const TimeExpiredException& e) {
            ctx.out << '\n' << e.what() << '\n' << session.result();
            return;
        } catch (const ExamFinishedException& e) {
            ctx.out << e.what() << '\n';
            return;
        } catch (const InvalidSelectionException& e) {
            ctx.out << "Rejected: " << e.what() << '\n';
        } catch (const ExamException& e) {
            ctx.out << "Error: " << e.what() << '\n';
        }
    }
}

void Student::viewResult(AppContext& ctx) const {
    const ExamReservation* r = chooseReservation(ctx, "the result");
    if (!r) return;
    if (!r->isFinished()) { ctx.out << "This examination is not finished yet - there is no result.\n"; return; }
    ctx.out << ctx.manager.storedResult(*r);
}

} // namespace ems
