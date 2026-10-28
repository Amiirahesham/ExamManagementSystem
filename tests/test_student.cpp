// Self-contained test harness (no external framework).  Usage: run_tests <fixture-data-dir>
// Every test works on a private temporary copy of the fixtures, so the repository data is never touched.
#include <chrono>
#include <cstring>
#include <fstream>
#include <functional>
#include <iostream>
#include <memory>
#include <random>
#include <sstream>
#include <string>
#include <vector>

#include "AppContext.h"
#include "ExamException.h"
#include "ExamManager.h"
#include "ExamSession.h"
#include "ExamTimer.h"
#include "FileManager.h"
#include "MultipleChoiceQuestion.h"
#include "Platform.h"
#include "PasswordHasher.h"
#include "SingleChoiceQuestion.h"
#include "Student.h"
#include "Util.h"

using namespace ems;

// ------------------------------------------------------------------ mini framework
static int g_checks = 0, g_failed = 0;
#define CHECK(cond) do { ++g_checks; if (!(cond)) { ++g_failed; std::cerr << "    FAIL " << __FILE__ << ":" << __LINE__ << "  " #cond "\n"; } } while (0)
#define CHECK_THROWS(Type, expr) do { ++g_checks; bool ok_ = false; \
    try { expr; } catch (const Type&) { ok_ = true; } catch (...) {} \
    if (!ok_) { ++g_failed; std::cerr << "    FAIL " << __FILE__ << ":" << __LINE__ << "  expected " #Type " from: " #expr "\n"; } } while (0)

struct TestCase { std::string name; std::function<void()> fn; };
static std::vector<TestCase>& registry() { static std::vector<TestCase> r; return r; }
struct Registrar { Registrar(const std::string& n, std::function<void()> f) { registry().push_back({n, std::move(f)}); } };
#define TEST(name) static void name(); static Registrar reg_##name(#name, name); static void name()

static std::string g_fixture;

// ------------------------------------------------------------------ helpers
class ManualTimeSource : public ITimeSource {
public:
    SteadyClock::time_point now() const override { return base_ + offset_; }
    void advance(std::chrono::seconds s) { offset_ += s; }
private:
    SteadyClock::time_point base_ = SteadyClock::now();
    std::chrono::seconds offset_{0};
};

// Private scratch copy of the fixtures under ./ems_test_tmp (relative to the working directory).
class TempData {
public:
    explicit TempData(const std::string& fixture) {
        static int counter = 0;
        platform::makeDirectory("ems_test_tmp");                                    // may already exist
        path_ = "ems_test_tmp/t" + util::toString(++counter) + "_" +
                util::toString(static_cast<long long>(SteadyClock::now().time_since_epoch().count() % 1000000007LL));
        if (!platform::makeDirectory(path_))
            throw std::runtime_error("cannot create " + path_ + ": " + platform::lastErrorText());
        const std::vector<std::string>& names = FileManager::fileNames();
        for (std::size_t i = 0; i < names.size(); ++i)
            if (!platform::copyFile(platform::joinPath(fixture, names[i]), file(names[i])))
                throw std::runtime_error("cannot copy fixture " + names[i] + " from " + fixture);
    }
    ~TempData() {
        const std::vector<std::string>& names = FileManager::fileNames();
        for (std::size_t i = 0; i < names.size(); ++i) {
            platform::removeFile(file(names[i]));
            platform::removeFile(file(names[i]) + ".bak");
            platform::removeFile(file(names[i]) + ".tmp");
        }
        platform::removeDirectory(path_);
    }
    TempData(const TempData&) = delete;
    TempData& operator=(const TempData&) = delete;
    std::string path() const { return path_; }
    std::string file(const std::string& name) const { return platform::joinPath(path_, name); }
private:
    std::string path_;
};

static std::string readFile(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    std::stringstream ss; ss << in.rdbuf(); return ss.str();
}
static void writeFile(const std::string& path, const std::string& text) { std::ofstream(path, std::ios::binary | std::ios::trunc) << text; }
static void appendLine(const std::string& path, const std::string& line) { std::ofstream(path, std::ios::binary | std::ios::app) << line << "\n"; }
static void replaceInFile(const std::string& path, const std::string& from, const std::string& to) {
    std::string t = readFile(path);
    const auto p = t.find(from);
    if (p == std::string::npos) throw std::runtime_error("test setup: '" + from + "' not found in " + path);
    t.replace(p, from.size(), to);
    writeFile(path, t);
}
static int countLinesStarting(const std::string& text, const std::string& prefix) {
    std::istringstream in(text); std::string line; int n = 0;
    while (std::getline(in, line)) if (line.compare(0, prefix.size(), prefix) == 0) ++n;
    return n;
}

struct World {
    TempData data;
    FileManager files;
    std::unique_ptr<ExamManager> mgr;
    ManualTimeSource clock;
    std::mt19937 rng{42};
    explicit World(const std::string& fixture = g_fixture) : data(fixture), files(data.path()), mgr(new ExamManager) { files.loadAll(*mgr); }
    void reload() { mgr.reset(new ExamManager); files.loadAll(*mgr); }
    std::unique_ptr<ExamSession> open(const std::string& user, int id) { return ExamSession::open(*mgr, files, user, id, clock, rng); }
    AppContext context(std::istream& in, std::ostream& out) { return AppContext{*mgr, files, in, out, clock, rng}; }
    std::string read(const std::string& f) const { return readFile(data.file(f)); }
};

static void selectCorrect(ExamSession& s) { s.select(s.current().question()->correctIds()); }
static void answerAllCorrectly(ExamSession& s) {
    for (std::size_t i = 0; i < s.questionCount(); ++i) { selectCorrect(s); if (i + 1 < s.questionCount()) s.next(); }
}
static std::set<int> wrongSingle(const Question& q) {
    for (const Answer& a : q.answers()) if (!a.isCorrect()) return {a.id()};
    return {};
}

template <typename Ex>
static bool loadFails(const std::function<void(const TempData&)>& corrupt) {
    TempData d(g_fixture);
    corrupt(d);
    FileManager fm(d.path());
    ExamManager m;
    try { fm.loadAll(m); } catch (const Ex&) { return true; } catch (...) { return false; }
    return false;
}

// ------------------------------------------------------------------ password hashing
TEST(hasher_matches_published_test_vectors) {
    CHECK(PasswordHasher::sha256Hex("abc") == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    CHECK(PasswordHasher::sha256Hex("") == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    CHECK(PasswordHasher::sha256Hex("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq") ==
          "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");   // two-block message
    CHECK(PasswordHasher::pbkdf2HmacSha256Hex("password", "salt", 1, 32) == "120fb6cffcf8b32c43e7225256c4f837a86548c92ccc35480805987cb70be17b");
    CHECK(PasswordHasher::pbkdf2HmacSha256Hex("password", "salt", 2, 32) == "ae4d0c95af6b46d32d0adff928f06dd02a303f8ef3c251dfd6e2d85a95474c43");
    CHECK(PasswordHasher::pbkdf2HmacSha256Hex("password", "salt", 4096, 32) == "c5e478d59288c841aa530db6845c4c8d962893a001ce4e11a4963873aa98134a");
}

TEST(hasher_salts_verifies_and_rejects_malformed) {
    const std::string a = PasswordHasher::hash("secret", 1000), b = PasswordHasher::hash("secret", 1000);
    CHECK(a != b);                                         // random salt
    CHECK(a.find("secret") == std::string::npos);
    CHECK(PasswordHasher::verify("secret", a));
    CHECK(!PasswordHasher::verify("Secret", a));
    CHECK(!PasswordHasher::verify("secret", "secret"));    // plaintext is not a hash
    CHECK(!PasswordHasher::verify("secret", "pbkdf2-sha256$0$00$00"));
    CHECK(!PasswordHasher::isWellFormed("pbkdf2-sha256$99999999999$" + std::string(32, 'a') + "$" + std::string(64, 'b')));
    CHECK(PasswordHasher::isWellFormed(a));
    CHECK_THROWS(ValidationException, Student("A", "B", "a@b.c", "u", "plaintext-password"));   // never stored
}

// ------------------------------------------------------------------ loading / schema
TEST(all_twelve_files_load) {
    World w;
    CHECK(FileManager::fileNames().size() == 12);
    CHECK(w.mgr->userCount() == 4 && w.mgr->examCount() == 5 && w.mgr->questionCount() == 3);
    CHECK(w.mgr->reservationCount() == 9);
    CHECK(w.mgr->findAttempt(1) && w.mgr->findAttempt(1)->size() == 4);     // legacy 4-column rows
    CHECK(w.mgr->findAttempt(9) && w.mgr->findAttempt(9)->size() == 3);     // extended 6-column rows
    CHECK(w.mgr->statusName(ReservationStatus::Finished) == "Finish");
    CHECK(w.mgr->questionTypeName(2) == "Multiple Choice");
    CHECK(w.mgr->exam("000-CCPP").totalQuestions() == 30);
    CHECK(!w.mgr->findReservation(2)->curQuestion().has_value());          // empty columns -> no value
}

TEST(nine_column_reservations_and_workbook_null_tokens_are_accepted) {
    TempData d(g_fixture);
    writeFile(d.file("exam_reservations.txt"), "1|shooter|2025-01-05|000-CCPP|22|850|4|10|700\n2|shooter|2025-03-05|INT-WEB|Null|Null|2|Null|600\n");
    writeFile(d.file("exam_running.txt"), "");                                      // no orphan attempt rows may remain
    writeFile(d.file("exam_running_ans.txt"), "");
    FileManager fm(d.path()); ExamManager m;
    fm.loadAll(m);
    CHECK(m.findReservation(1)->maxScore() == 1000);       // derived from exams.txt
    CHECK(!m.findReservation(2)->score().has_value());
}

TEST(malformed_and_missing_files_give_controlled_errors) {
    CHECK((loadFails<FileException>([](const TempData& d) { platform::removeFile(d.file("exams.txt")); })));
    CHECK((loadFails<FileException>([](const TempData& d) { platform::removeFile(d.file("exam_running_ans.txt")); })));
    CHECK((loadFails<FileFormatException>([](const TempData& d) { appendLine(d.file("questions.txt"), "00C|001|9|too few fields"); })));
    CHECK((loadFails<FileFormatException>([](const TempData& d) { appendLine(d.file("exams.txt"), "BAD|Name|abc|1000|700"); })));
    CHECK((loadFails<FileFormatException>([](const TempData& d) { appendLine(d.file("exams.txt"), "BAD|Name|60|1000|1500"); })));   // rule 12
    CHECK((loadFails<FileFormatException>([](const TempData& d) { appendLine(d.file("sections.txt"), "NOPE|001|X"); })));            // rule 3
    CHECK((loadFails<FileFormatException>([](const TempData& d) { appendLine(d.file("questions.txt"), "00C|009|9|Q|1"); })));       // rule 4
    CHECK((loadFails<FileFormatException>([](const TempData& d) { appendLine(d.file("exam_users.txt"), "A|B|a@b.c|newuser|plaintext|STUDENT"); })));
    CHECK((loadFails<FileFormatException>([](const TempData& d) { appendLine(d.file("exam_running_ans.txt"), "9|9|1|0|0"); })));   // orphan row
    CHECK((loadFails<FileFormatException>([](const TempData& d) { appendLine(d.file("exam_reservations.txt"), "10|ghost|2026-01-01|QUICK-C|||2||600|1000"); })));
    CHECK((loadFails<FileFormatException>([](const TempData& d) { appendLine(d.file("exam_reservations.txt"), "10|ali100|2026-02-30|QUICK-C|||2||600|1000"); })));
}

TEST(duplicates_and_invalid_question_rules_are_rejected) {
    CHECK((loadFails<DuplicateIdException>([](const TempData& d) {
        replaceInFile(d.file("exam_users.txt"), "|mona2000|", "|ali100|"); })));                                   // rule 1
    CHECK((loadFails<DuplicateIdException>([](const TempData& d) { appendLine(d.file("exams.txt"), "PCEP|Dup|10|100|50"); })));   // rule 2
    CHECK((loadFails<DuplicateIdException>([](const TempData& d) { appendLine(d.file("questions.txt"), "00C|001|1|Again|1"); })));
    CHECK((loadFails<DuplicateIdException>([](const TempData& d) { appendLine(d.file("exam_details.txt"), "QUICK-C|00C|001|1"); })));
    CHECK((loadFails<DuplicateIdException>([](const TempData& d) { appendLine(d.file("question_answers.txt"), "00C|1|1|Dup|0"); })));
    CHECK((loadFails<DuplicateIdException>([](const TempData& d) { appendLine(d.file("exam_reservations.txt"), "5|ali100|2026-01-01|QUICK-C|||2||600|1000"); })));
    CHECK((loadFails<InvalidQuestionTypeException>([](const TempData& d) { appendLine(d.file("questions.txt"), "00C|001|9|Q|3"); })));
    CHECK((loadFails<ValidationException>([](const TempData& d) {                                                 // rule 5
        replaceInFile(d.file("question_answers.txt"), "00C|1|1|It is a unary operator|0", "00C|1|1|It is a unary operator|1"); })));
    CHECK((loadFails<ValidationException>([](const TempData& d) {                                                 // rule 6
        replaceInFile(d.file("question_answers.txt"), "00C|3|3|switch case statement|1", "00C|3|3|switch case statement|0"); })));
}

// ------------------------------------------------------------------ OOP features
TEST(operators_polymorphism_and_composition) {
    World w;
    Exam e("X-1", "Name", 10, 100, 50);
    e += ExamDetail("00C", "001", 2);
    CHECK(e.details().size() == 1);
    CHECK_THROWS(DuplicateIdException, e += ExamDetail("00C", "001", 3));       // no duplicate details
    CHECK_THROWS(ValidationException, Exam("X-2", "Name", 10, 100, 200));       // pass > max
    CHECK_THROWS(ValidationException, ExamDetail("00C", "001", 0));
    std::ostringstream os; os << e;
    CHECK(os.str().find("X-1") != std::string::npos && os.str().find("00C/001") != std::string::npos);

    const Question& single = w.mgr->question("00C", 1);
    const Question& multi = w.mgr->question("00C", 3);
    CHECK(single == w.mgr->question("00C", 1) && single != multi);
    CHECK(single[0].id() == 1 && single[3].isCorrect());
    CHECK_THROWS(std::out_of_range, single[4]);

    std::vector<const Question*> base = {&single, &multi};                      // runtime polymorphism
    CHECK(base[0]->typeId() == 1 && base[1]->typeId() == 2);
    CHECK(base[0]->grade({4}) == 1.0 && base[0]->grade({3}) == 0.0 && base[0]->grade({3, 4}) == 0.0 && base[0]->grade({}) == 0.0);
    CHECK(base[1]->grade({1, 3}) == 1.0 && base[1]->grade({1}) == 0.0 && base[1]->grade({1, 2, 3}) == 0.0);   // exact match
    CHECK_THROWS(InvalidSelectionException, base[0]->validateSelection({1, 2}));
    base[1]->validateSelection({1, 2, 3});
    CHECK_THROWS(InvalidSelectionException, base[1]->validateSelection({99}));
    CHECK(Question::liveCount() == 3);
    CHECK(User::liveCount() == 4);
}

// ------------------------------------------------------------------ input validation / C++14 helpers
TEST(input_validation_helpers) {
    CHECK(util::parseInt(" 42 ", "x") == 42 && util::parseInt("-7", "x") == -7 && util::parseInt("0", "x") == 0);
    CHECK(util::parseInt("2147483647", "x") == 2147483647 && util::parseInt("-2147483648", "x") == -2147483647 - 1);
    const char* bad[] = {"", "   ", "abc", "12a", "-", "+5", "1.5", "2147483648", "99999999999999999999"};
    for (std::size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); ++i) CHECK_THROWS(ValidationException, util::parseInt(bad[i], "field"));
    CHECK(util::isValidDate("2024-02-29") && util::isValidDate("2025-12-31"));
    CHECK(!util::isValidDate("2025-02-29") && !util::isValidDate("2025-13-01") && !util::isValidDate("2025-1-01") &&
          !util::isValidDate("abcd-ef-gh") && !util::isValidDate("2025-04-31") && !util::isValidDate(""));
    CHECK(util::toString(-123) == "-123" && util::toString(0) == "0" && util::toString(1000000) == "1000000");
    CHECK(!util::parseOptionalInt("Null", "f").has_value() && !util::parseOptionalInt("", "f").has_value() &&
          !util::parseOptionalInt("NULL", "f").has_value() && util::parseOptionalInt("7", "f").value() == 7);
    CHECK_THROWS(ValidationException, util::parseOptionalInt("x", "f"));
    CHECK(util::split("a||b", '|').size() == 3 && util::trim("  hi \t") == "hi");
}

TEST(optional_int_replacement) {
    OptionalInt o;
    CHECK(!o && !o.has_value() && o.value_or(9) == 9);
    CHECK_THROWS(std::logic_error, (void)*o);
    o = 5;
    CHECK(o && *o == 5 && o.value_or(9) == 5 && o == OptionalInt(5) && o != OptionalInt());
    o.reset();
    CHECK(!o && o == OptionalInt());
}

TEST(platform_layer_replaces_files_and_generates_random_bytes) {
    TempData d(g_fixture);
    const std::string a = d.file("a.txt"), b = d.file("b.txt");
    writeFile(a, "old"); writeFile(b, "new");
    CHECK(platform::fileExists(a) && !platform::fileExists(d.file("nope.txt")) && !platform::fileExists(d.path()));
    CHECK(platform::replaceFile(b, a));                                             // overwrites an existing target
    CHECK(readFile(a) == "new" && !platform::fileExists(b));
    CHECK(platform::copyFile(a, b) && readFile(b) == "new");
    CHECK(platform::removeFile(a) && platform::removeFile(b) && !platform::removeFile(a));
    CHECK(platform::joinPath("dir", "f") == "dir/f" && platform::joinPath("dir/", "f") == "dir/f" && platform::joinPath("", "f") == "f");
    unsigned char x[16] = {0}, y[16] = {0};
    platform::secureRandomBytes(x, sizeof(x)); platform::secureRandomBytes(y, sizeof(y));
    CHECK(std::memcmp(x, y, sizeof(x)) != 0);                                       // not the constant stream of old MinGW
    CHECK(platform::makeSeededEngine()() != platform::makeSeededEngine()());
    CHECK(PasswordHasher::hash("pw", 1000) != PasswordHasher::hash("pw", 1000));    // random salts
}

// ------------------------------------------------------------------ login
TEST(login_success) {
    World w;
    Student& s = w.mgr->authenticateStudent("ali100", "aliwika123");
    CHECK(s.fullName() == "Ali Elhoseny" && s.role() == Role::Student);
    CHECK(s.passwordHash().find("aliwika123") == std::string::npos);
    CHECK(w.mgr->authenticateStudent("shooter", "kokowawa").username() == "shooter");     // workbook credentials
}

TEST(login_rejected_for_bad_credentials_and_non_students) {
    World w;
    std::string wrongPw, unknownUser;
    try { w.mgr->authenticate("ali100", "wrong"); } catch (const AuthenticationException& e) { wrongPw = e.what(); }
    try { w.mgr->authenticate("nobody", "wrong"); } catch (const AuthenticationException& e) { unknownUser = e.what(); }
    CHECK(!wrongPw.empty() && wrongPw == unknownUser);                           // no user enumeration
    CHECK_THROWS(AuthenticationException, w.mgr->authenticate("", ""));
    CHECK_THROWS(AuthenticationException, w.mgr->authenticate("ali100", ""));
    CHECK_THROWS(AuthenticationException, w.mgr->authenticateStudent("ali100", "ALIWIKA123"));
    CHECK_THROWS(AccessDeniedException, w.mgr->authenticateStudent("admin", "admin123"));   // correct password, wrong role
    CHECK_THROWS(AuthenticationException, w.mgr->authenticateStudent("admin", "nope"));
}

// ------------------------------------------------------------------ reservations
TEST(reservations_are_listed_per_student) {
    World w;
    CHECK(w.mgr->reservationsFor("shooter").size() == 4);
    CHECK(w.mgr->reservationsFor("ali100").size() == 3);
    CHECK(w.mgr->reservationsFor("mona2000").size() == 2);
    CHECK(w.mgr->reservationsFor("admin").empty());

    std::istringstream in; std::ostringstream out;
    AppContext ctx = w.context(in, out);
    w.mgr->authenticateStudent("shooter", "kokowawa").viewReservations(ctx);
    const std::string s = out.str();
    CHECK(s.find("user: Youssef Shawky") != std::string::npos);
    CHECK(s.find("Introduction to programming using C and C++") != std::string::npos);
    CHECK(s.find("Finish") != std::string::npos && s.find("850") != std::string::npos && s.find("2025-03-05") != std::string::npos);
    CHECK(s.find("Certified Associate Python") != std::string::npos);
    CHECK(s.find("ali100") == std::string::npos);
}

TEST(exam_details_screen) {
    World w;
    std::istringstream in; std::ostringstream out;
    AppContext ctx = w.context(in, out);
    w.mgr->authenticateStudent("shooter", "kokowawa").showExamDetails(ctx, *w.mgr->findReservation(1));
    const std::string s = out.str();
    CHECK(s.find("000-CCPP") != std::string::npos && s.find("90 min") != std::string::npos);
    CHECK(s.find("C++ Language") != std::string::npos && s.find("Inheritance") != std::string::npos);
    CHECK(s.find("Total Number of Questions: 30") != std::string::npos);
}

TEST(start_requires_a_valid_reservation) {
    World w;
    CHECK_THROWS(ReservationStateException, w.open("ali100", 3));        // belongs to mona2000
    CHECK_THROWS(ReservationStateException, w.open("ali100", 999));      // does not exist
    CHECK_THROWS(ReservationStateException, w.open("nobody", 5));
    CHECK_THROWS(ReservationStateException, w.open("ali100", 8));        // status "Reserve": not activated
    CHECK_THROWS(ValidationException, w.open("shooter", 2));             // INT-WEB has no exam_details yet
    CHECK_THROWS(NotFoundException, w.mgr->exam("NO-SUCH-EXAM"));        // unknown examination code
    w.reload();
    CHECK(w.mgr->findReservation(8)->status() == ReservationStatus::Reserved);
    CHECK(w.mgr->findReservation(2)->status() == ReservationStatus::Active);   // nothing changed
}

TEST(insufficient_questions_reject_generation_without_side_effects) {
    World w;
    const std::string before = w.read("exam_running.txt") + w.read("exam_reservations.txt");
    try { w.open("ali100", 7); CHECK(false); }
    catch (const InsufficientQuestionsException& e) {
        const std::string m = e.what();
        CHECK(m.find("00C/001") != std::string::npos && m.find("needs 5") != std::string::npos && m.find("00CPP/002") != std::string::npos);
    }
    CHECK(w.read("exam_running.txt") + w.read("exam_reservations.txt") == before);
    w.reload();
    CHECK(w.mgr->findReservation(7)->status() == ReservationStatus::Active);
}

// ------------------------------------------------------------------ exam generation / navigation
TEST(generation_is_random_complete_and_persisted) {
    World w;
    auto s = w.open("ali100", 5);
    CHECK(!s->resumed() && s->questionCount() == 3 && s->currentIndex() == 0);
    int sec1 = 0, sec2 = 0;
    for (std::size_t i = 0; i < s->questionCount(); ++i) {
        CHECK(s->at(i).position() == static_cast<int>(i) + 1);
        (s->at(i).sectionCode() == "001" ? sec1 : sec2)++;
    }
    CHECK(sec1 == 2 && sec2 == 1);
    CHECK(s->reservation().status() == ReservationStatus::Running && *s->reservation().remTimeMinutes() == 15);
    CHECK(countLinesStarting(w.read("exam_running.txt"), "5|") == 3);
    CHECK(countLinesStarting(w.read("exam_running_ans.txt"), "5|") == 12);
    CHECK(platform::fileExists(w.data.file("exam_reservations.txt.bak")));                  // previous version kept

    std::set<int> orders;                                                          // different seeds -> different orders
    for (unsigned seed = 1; seed <= 12; ++seed) {
        World x; x.rng.seed(seed);
        auto t = x.open("ali100", 5);
        orders.insert(t->at(0).questionNumber() * 100 + t->at(1).questionNumber() * 10 + t->at(2).questionNumber());
    }
    CHECK(orders.size() > 1);
}

TEST(single_and_multiple_choice_selection_rules) {
    World w;
    auto s = w.open("ali100", 5);
    bool sawSingle = false, sawMulti = false;
    for (std::size_t i = 0; i < s->questionCount(); ++i) {
        const Question& q = *s->current().question();
        if (q.typeId() == 1) {
            sawSingle = true;
            CHECK_THROWS(InvalidSelectionException, s->select({1, 2}));            // two options in single choice
            CHECK(s->current().selectedIds().empty());                              // nothing changed
            s->select({2});
            s->select({3});                                                         // answer modification
            CHECK(s->current().selectedIds() == std::set<int>({3}));
        } else {
            sawMulti = true;
            s->select({1, 3});                                                      // several options accepted
            CHECK(s->current().selectedIds() == std::set<int>({1, 3}));
            s->select({1, 2, 3});
            CHECK_THROWS(InvalidSelectionException, s->select({7}));                // unknown answer id
        }
        if (i + 1 < s->questionCount()) s->next();
    }
    CHECK(sawSingle && sawMulti);
    s->select({});                                                                  // clearing is allowed
    CHECK(!s->current().isAnswered());
}

TEST(navigation_restores_saved_answers) {
    World w;
    auto s = w.open("ali100", 5);
    CHECK(!s->previous());                                                          // already first
    s->select({1});
    CHECK(s->next() && s->currentIndex() == 1);
    CHECK(s->next() && s->currentIndex() == 2);
    CHECK(!s->next());                                                              // already last
    CHECK(s->previous() && s->previous() && s->currentIndex() == 0);
    CHECK(s->current().selectedIds() == std::set<int>({1}));                        // saved answer restored
    CHECK(s->answeredCount() == 1);
}

TEST(autosave_writes_every_change_to_disk) {
    World w;
    auto s = w.open("ali100", 5);
    s->select({2});
    CHECK(countLinesStarting(w.read("exam_running_ans.txt"), "5|1|2|") == 1);
    CHECK(w.read("exam_running_ans.txt").find("5|1|2|") != std::string::npos);
    s->next();
    w.reload();
    CHECK(*w.mgr->findReservation(5)->curQuestion() == 2);
    const auto* attempt = w.mgr->findAttempt(5);
    CHECK(attempt && (*attempt)[0].selectedIds() == std::set<int>({2}));
}

// ------------------------------------------------------------------ resume
TEST(interrupted_exam_is_resumed_with_progress_and_remaining_time) {
    World w;
    std::vector<int> order;
    {
        auto s = w.open("ali100", 5);
        for (std::size_t i = 0; i < 3; ++i) order.push_back(s->at(i).questionNumber());
        selectCorrect(*s);
        s->next();
        w.clock.advance(std::chrono::seconds(100));
        s->suspend();
    }                                                                               // session object gone = "program exit"
    w.reload();                                                                     // brand new manager from the files
    auto r = w.open("ali100", 5);
    CHECK(r->resumed() && r->currentIndex() == 1);
    CHECK(r->remaining() == std::chrono::seconds(14 * 60));                        // 900 s - 100 s, rounded up to whole minutes
    for (std::size_t i = 0; i < 3; ++i) CHECK(r->at(i).questionNumber() == order[i]);
    CHECK(r->at(0).selectedIds() == r->at(0).question()->correctIds());
    CHECK(r->previous() && r->current().isAnswered());
}

TEST(fixture_running_exam_from_the_text_files_can_be_resumed_and_graded) {
    World w;
    auto s = w.open("shooter", 9);                                                  // exam_running.txt rows of reservation 9
    CHECK(s->resumed() && s->currentIndex() == 1 && s->remaining() == std::chrono::seconds(9 * 60));
    CHECK(s->at(0).questionNumber() == 2 && s->at(0).selectedIds() == std::set<int>({2}));
    ExamResult r = s->submit();                                                     // 1 of 3 correct, rest unanswered
    CHECK(r.earnedMarks == 1 && r.totalMarks == 3 && r.score == 333 && !r.passed);
}

TEST(resume_detects_changed_bank_and_corrupt_attempt_data) {
    {   World w;
        replaceInFile(w.data.file("question_answers.txt"), "00C|1|4|All of the above|1", "00C|1|4|All of the above|0");
        replaceInFile(w.data.file("question_answers.txt"), "00C|1|1|It is a unary operator|0", "00C|1|1|It is a unary operator|1");
        w.reload();
        CHECK_THROWS(ValidationException, w.open("shooter", 9));                   // bank no longer matches the snapshot
    }
    {   World w;
        replaceInFile(w.data.file("exam_running.txt"), "9|3|1|0|00C|001", "9|3|99|0|00C|001");
        w.reload();
        CHECK_THROWS(NotFoundException, w.open("shooter", 9));                     // question 99 does not exist
    }
}

// ------------------------------------------------------------------ grading
TEST(grading_all_correct_passes_with_section_report) {
    World w;
    auto s = w.open("ali100", 5);
    answerAllCorrectly(*s);
    ExamResult r = s->submit();
    CHECK(r.score == 1000 && r.passed && r.earnedMarks == 3 && r.totalMarks == 3 && r.maxScore == 1000 && r.passScore == 600);
    CHECK(r.sections.size() == 2);
    CHECK(r.sections[0].sectionName == "Operators" && r.sections[0].questions == 2 && r.sections[0].correct == 2);
    CHECK(r.sections[1].sectionName == "Control Flow" && r.sections[1].questions == 1 && r.sections[1].correct == 1);
    CHECK(r.sections[0].subjectName == "C Language");
    std::ostringstream os; os << r;
    CHECK(os.str().find("Congratulation") != std::string::npos && os.str().find("Your Score: 1000") != std::string::npos);
    w.reload();                                                                     // persisted
    CHECK(w.mgr->findReservation(5)->status() == ReservationStatus::Finished && *w.mgr->findReservation(5)->score() == 1000);
    ExamResult stored = w.mgr->storedResult(*w.mgr->findReservation(5));
    CHECK(stored.score == 1000 && stored.passed && stored.earnedMarks == 3);
}

TEST(grading_multiple_choice_uses_exact_match_and_score_is_normalised) {
    World w;
    auto s = w.open("mona2000", 6);
    for (std::size_t i = 0; i < s->questionCount(); ++i) {
        const Question& q = *s->current().question();
        if (q.typeId() == 2) s->select({*q.correctIds().begin()});                 // only ONE of the two correct options
        else selectCorrect(*s);
        if (i + 1 < s->questionCount()) s->next();
    }
    ExamResult r = s->submit();
    CHECK(r.earnedMarks == 2 && r.score == 667 && r.passed);                       // round(2/3 * 1000), pass_score 600
    CHECK(r.sections[1].correct == 0);
}

TEST(grading_wrong_answers_fail) {
    World w;
    auto s = w.open("ali100", 5);
    for (std::size_t i = 0; i < s->questionCount(); ++i) {
        s->select(wrongSingle(*s->current().question()));
        if (i + 1 < s->questionCount()) s->next();
    }
    ExamResult r = s->submit();
    CHECK(r.earnedMarks == 0 && r.score == 0 && !r.passed);
    std::ostringstream os; os << r;
    CHECK(os.str().find("Fail") != std::string::npos);
}

TEST(score_normalisation_and_rounding) {
    CHECK(ExamManager::computeScore(3, 3, 1000) == 1000);
    CHECK(ExamManager::computeScore(2, 3, 1000) == 667);
    CHECK(ExamManager::computeScore(1, 3, 1000) == 333);
    CHECK(ExamManager::computeScore(0, 3, 1000) == 0);
    CHECK(ExamManager::computeScore(7, 30, 1000) == 233);
    CHECK(ExamManager::computeScore(1, 0, 1000) == 0);
}

TEST(stored_result_of_workbook_finished_exam) {
    World w;
    const ExamReservation& r1 = *w.mgr->findReservation(1);
    ExamResult r = w.mgr->storedResult(r1);
    CHECK(r.score == 850 && r.passed && r.passScore == 700);                       // stored score is authoritative
    CHECK(r.earnedMarks == 3 && r.totalMarks == 4);                                // from the legacy quest_score rows
    CHECK_THROWS(ReservationStateException, w.mgr->storedResult(*w.mgr->findReservation(5)));
}

// ------------------------------------------------------------------ time
TEST(timeout_auto_submits_and_rejects_late_answers) {
    World w;
    auto s = w.open("ali100", 5);
    selectCorrect(*s);
    w.clock.advance(std::chrono::seconds(15 * 60 - 1));
    CHECK(!s->expired() && s->remaining() == std::chrono::seconds(1));
    w.clock.advance(std::chrono::seconds(2));
    CHECK(s->expired() && s->remaining() == std::chrono::seconds(0));
    CHECK_THROWS(TimeExpiredException, s->select({1}));                            // late answer rejected
    CHECK(s->isFinished() && s->result().timedOut && s->result().earnedMarks == 1 && s->result().score == 333);
    CHECK_THROWS(ExamFinishedException, s->select({1}));
    w.reload();
    CHECK(w.mgr->findReservation(5)->status() == ReservationStatus::Finished);
    CHECK(*w.mgr->findReservation(5)->remTimeMinutes() == 0);
}

TEST(resumed_exam_whose_time_ran_out_is_finished_on_first_use) {
    World w;
    replaceInFile(w.data.file("exam_reservations.txt"), "9|shooter|2026-11-04|QUICK-C|2||3|9|600|1000", "9|shooter|2026-11-04|QUICK-C|2||3|0|600|1000");
    w.reload();
    auto s = w.open("shooter", 9);
    CHECK(s->finishIfExpired() && s->isFinished() && s->result().timedOut);
    w.reload();
    CHECK(w.mgr->findReservation(9)->isFinished());
}

TEST(timer_counts_with_std_chrono_independently_of_user_actions) {
    ManualTimeSource clock;
    ExamTimer t(clock, std::chrono::seconds(125));
    CHECK(t.remaining() == std::chrono::seconds(125) && t.remainingMinutesCeil() == 3 && !t.expired());
    clock.advance(std::chrono::seconds(65));
    CHECK(t.remaining() == std::chrono::seconds(60) && t.remainingMinutesCeil() == 1);
    clock.advance(std::chrono::seconds(60));
    CHECK(t.expired() && t.remainingMinutesCeil() == 0);
}

// ------------------------------------------------------------------ finished-exam protection
TEST(finished_exams_cannot_be_resumed_or_modified) {
    World w;
    CHECK_THROWS(ExamFinishedException, w.open("shooter", 1));                     // rule 9 (workbook's finished exam)
    CHECK_THROWS(ExamFinishedException, ExamSession::checkStartable(*w.mgr, "shooter", 1));
    CHECK_THROWS(ExamFinishedException, w.mgr->findReservation(1)->updateProgress(2, 5));
    CHECK_THROWS(ExamFinishedException, w.mgr->findReservation(1)->requireModifiable());
    const std::string before = w.read("exam_reservations.txt") + w.read("exam_running.txt") + w.read("exam_running_ans.txt");
    CHECK_THROWS(ExamFinishedException, w.files.saveProgress(*w.mgr, *w.mgr->findReservation(1), w.mgr->attempt(1)));   // storage level
    CHECK(w.read("exam_reservations.txt") + w.read("exam_running.txt") + w.read("exam_running_ans.txt") == before);

    auto s = w.open("ali100", 5);                                                  // rule 11 after final submission
    answerAllCorrectly(*s);
    s->submit();
    CHECK_THROWS(ExamFinishedException, s->select({1}));
    CHECK_THROWS(ExamFinishedException, s->next());
    CHECK_THROWS(ExamFinishedException, s->submit());
    CHECK_THROWS(ExamFinishedException, w.open("ali100", 5));
    w.reload();
    CHECK_THROWS(ExamFinishedException, w.open("ali100", 5));                      // also after "restart"
}

// ------------------------------------------------------------------ persistence safety
TEST(saving_preserves_other_students_rows_and_legacy_rows) {
    World w;
    ExamManager m2; w.files.loadAll(m2);                                           // a second "process", loaded BEFORE ali100 starts
    auto s = w.open("ali100", 5);
    s->select({1});
    const std::string running = w.read("exam_running.txt");
    CHECK(countLinesStarting(running, "1|") == 4 && countLinesStarting(running, "9|") == 3 && countLinesStarting(running, "5|") == 3);
    const std::string res = w.read("exam_reservations.txt");
    CHECK(res.find("1|shooter|2025-01-05|000-CCPP|22|850|4|10|700|1000") != std::string::npos);
    CHECK(res.find("9|shooter|2026-11-04|QUICK-C|2||3|9|600|1000") != std::string::npos);
    CHECK(countLinesStarting(w.read("exam_running_ans.txt"), "1|") == 8);

    // mona2000 starts a different exam through the stale manager that never saw ali100's progress
    auto t = ExamSession::open(m2, w.files, "mona2000", 6, w.clock, w.rng);
    t->select({1});
    CHECK(countLinesStarting(w.read("exam_running.txt"), "5|") == 3 && countLinesStarting(w.read("exam_running.txt"), "6|") == 3);
    w.reload();
    CHECK(w.mgr->findAttempt(5)->at(0).selectedIds() == std::set<int>({1}));      // ali100's data survived mona2000's save
    CHECK(w.mgr->findReservation(5)->isRunning() && w.mgr->findReservation(6)->isRunning());
}

TEST(a_corrupt_file_is_never_overwritten_and_memory_stays_consistent) {
    World w;
    auto s = w.open("ali100", 5);
    appendLine(w.data.file("exam_running.txt"), "garbage-line");
    const std::string corrupted = w.read("exam_running.txt");
    CHECK_THROWS(FileFormatException, s->select({1}));
    CHECK(s->current().selectedIds().empty());                                     // reverted
    CHECK_THROWS(FileFormatException, s->next());
    CHECK(s->currentIndex() == 0);
    CHECK(w.read("exam_running.txt") == corrupted);                                // untouched
}

TEST(failed_start_leaves_no_trace) {
    World w;
    appendLine(w.data.file("exam_running_ans.txt"), "x|y");                        // corrupt AFTER the manager was loaded
    CHECK_THROWS(FileFormatException, w.open("ali100", 5));
    CHECK(countLinesStarting(w.read("exam_running.txt"), "5|") == 0);
    CHECK(w.mgr->findReservation(5)->status() == ReservationStatus::Active);
}

TEST(file_format_tolerates_crlf_bom_comments_and_blank_lines) {
    TempData d(g_fixture);
    std::string t = readFile(d.file("subjects.txt"));
    std::string crlf;
    for (char c : t) { if (c == '\n') crlf += "\r\n"; else crlf += c; }
    writeFile(d.file("subjects.txt"), "\xEF\xBB\xBF" + crlf + "\r\n   \r\n# trailing comment\r\n");
    FileManager fm(d.path()); ExamManager m;
    fm.loadAll(m);
    CHECK(m.subject("00C").name() == "C Language");
}

// ------------------------------------------------------------------ end-to-end console workflow
TEST(end_to_end_student_workflow) {
    World w;
    Student& ali = w.mgr->authenticateStudent("ali100", "aliwika123");
    std::istringstream in(
        "1\n"                       // view reservations
        "2\n1\n"                    // exam details of reservation #1 (QUICK-C)
        "3\n1\n1\n"                 // start reservation #1, confirm
        "zz\n"                      // invalid option -> rejected
        "a\nn\na\nn\na\n"           // answer Q1..Q3 with option a
        "s\ny\n"                    // submit
        "4\n1\n"                    // show stored result
        "9\n"                       // invalid menu choice
        "0\n");
    std::ostringstream out;
    AppContext ctx = w.context(in, out);
    ali.showMenu(ctx);
    const std::string s = out.str();
    for (const char* needle : {"Student menu", "Reserved Exams", "C Language Quick Check", "Total Number of Questions: 3",
                               "if you ready to start", "Remaining Time:", "Q 1) of 3", "Rejected: Invalid option", "Answered: 3/3",
                               "Pass Score: 600", "Your Score:", "Invalid choice."})
        CHECK(s.find(needle) != std::string::npos);
    w.reload();
    CHECK(w.mgr->findReservation(5)->isFinished());
    CHECK(w.mgr->findReservation(5)->score().has_value());
}

TEST(end_to_end_quit_and_resume_from_console) {
    World w;
    {
        std::istringstream in("3\n2\n1\na\nn\nq\n0\n");                            // mona2000: #2 = reservation 6
        std::ostringstream out;
        AppContext ctx = w.context(in, out);
        w.mgr->authenticateStudent("mona2000", "monaooa123").showMenu(ctx);
        CHECK(out.str().find("Progress saved") != std::string::npos);
    }
    w.reload();
    CHECK(w.mgr->findReservation(6)->isRunning() && *w.mgr->findReservation(6)->curQuestion() == 2);
    std::istringstream in("3\n2\n1\np\nq\n0\n");                              // resume, go back to Q1, save & quit
    std::ostringstream out;
    AppContext ctx = w.context(in, out);
    w.mgr->authenticateStudent("mona2000", "monaooa123").showMenu(ctx);
    CHECK(out.str().find("Resuming your examination") != std::string::npos);
    CHECK(out.str().find("Q 2) of 3") != std::string::npos);
    CHECK(out.str().find("[x]") != std::string::npos);                             // saved answer shown again
}

TEST(console_blocks_finished_exam_and_survives_closed_input) {
    World w;
    {
        std::istringstream in("3\n1\n0\n");                                        // shooter #1 = finished reservation 1
        std::ostringstream out;
        AppContext ctx = w.context(in, out);
        w.mgr->authenticateStudent("shooter", "kokowawa").showMenu(ctx);
        CHECK(out.str().find("cannot be resumed") != std::string::npos);
        CHECK(out.str().find("Remaining Time") == std::string::npos);
    }
    {
        std::istringstream in("4\n1\n0\n");                                        // result of finished exam
        std::ostringstream out;
        AppContext ctx = w.context(in, out);
        w.mgr->authenticateStudent("shooter", "kokowawa").showMenu(ctx);
        CHECK(out.str().find("Your Score: 850") != std::string::npos);
    }
    {
        std::istringstream in("3\n1\n1\na\n");                                     // input ends abruptly mid-exam (ali100 #1)
        std::ostringstream out;
        AppContext ctx = w.context(in, out);
        w.mgr->authenticateStudent("ali100", "aliwika123").showMenu(ctx);
        CHECK(out.str().find("progress was saved") != std::string::npos);
    }
    w.reload();
    CHECK(w.mgr->findReservation(5)->isRunning());
}

// ------------------------------------------------------------------ runner
int main(int argc, char** argv) {
    if (argc < 2) { std::cerr << "Usage: " << argv[0] << " <fixture-data-dir>\n"; return 64; }
    g_fixture = argv[1];
    int failedTests = 0;
    for (const TestCase& t : registry()) {
        const int before = g_failed;
        try { t.fn(); }
        catch (const std::exception& e) { ++g_failed; std::cerr << "    EXCEPTION in " << t.name << ": " << e.what() << "\n"; }
        const bool ok = g_failed == before;
        if (!ok) ++failedTests;
        std::cout << (ok ? "[ PASS ] " : "[ FAIL ] ") << t.name << "\n";
    }
    platform::removeDirectory("ems_test_tmp");                                       // only succeeds when empty
    std::cout << "\n" << registry().size() - failedTests << "/" << registry().size() << " tests passed, "
              << g_checks - g_failed << "/" << g_checks << " checks passed\n";
    return g_failed == 0 ? 0 : 1;
}
