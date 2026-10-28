# Examination Management System — Student Module (C++14 / TDM-GCC 5.1.0 edition)

Console application implementing the **Student** side of the system in `Examination_Management_System.pdf` / `exam_db.xlsx`:
login, reservations, exam details, start/resume, randomised questions, single/multiple choice, next/previous, answer changes,
autosave, `std::chrono` timing, grading, section report, pass/fail and persistence in the 12 text files.

**Language level: strict `-std=c++14`.** No `<optional>`, `<filesystem>`, `string_view`, structured bindings, `if constexpr`, inline variables,
and none of the `std::to_string` / `std::stoi` / `std::random_device` APIs that are missing or unreliable on older MinGW builds (see "TDM-GCC pitfalls").

## What was actually verified (and what was not)
| Check | Result |
|---|---|
| **GCC 5.5.0** (Linux build of the GCC 5 line; closest available to 5.1.0), `-std=c++14 -Wall -Wextra -Wpedantic`, `-O0` and `-O2` | all sources: 0 warnings; **39/39 tests, 250/250 checks pass**. At `-O2` GCC 5.5 prints one `-Wstrict-overflow` note from libstdc++'s chrono inlined into one *test* function (not from `src/`) |
| GCC 13.3 `-std=c++14` + AddressSanitizer/UBSan | 39/39 pass, no findings |
| **MinGW-w64 GCC 13 (win32 threads), strict `-std=c++14`, 64-bit `.exe`** — exercises the real Windows branches (`MoveFileEx`, `CryptGenRandom`, `_mkdir`, CRLF output) | built with 0 warnings; **39/39 tests pass when run under Wine 9.0**; app run under Wine: rejected login, then login + reservations screen |
| MinGW-w64 32-bit `.exe` (TDM-GCC's default target is 32-bit) | compiles with 0 warnings; **could not be executed** here (Wine had no 32-bit support) |
| Optional OpenSSL backend (`-DEMS_USE_OPENSSL`) | builds; 39/39 pass; hashes made by it verify with the built-in code and vice-versa |
| Code::Blocks `.cbp` | the three targets' file lists and flags were rebuilt and tested with GCC 5.5; **not opened in Code::Blocks itself** |
| **TDM-GCC 5.1.0 on real Windows**, `build_windows.bat`, `CMakeLists.txt`, the hidden-password prompt on a real console | **not verified** (not available here) |

## Folder tree
```
ExamManagementSystem/
├── README.md  build_windows.bat  Makefile  CMakeLists.txt  ExamManagementSystem.cbp  .gitignore
├── main.cpp                         login screen + Student menu
├── include/   (22+ headers)         ExamException.h Util.h OptionalInt.h Platform.h PasswordHasher.h ExamTimer.h AppContext.h
│                                    User.h Student.h Subject.h Section.h Answer.h Question.h SingleChoiceQuestion.h
│                                    MultipleChoiceQuestion.h Exam.h ExamDetail.h ExamReservation.h RunningQuestion.h
│                                    RunningAnswer.h ExamResult.h ExamManager.h FileManager.h ExamSession.h
├── src/       one .cpp per header that needs one (+ Platform.cpp, Util.cpp)
├── data/      the 12 fixture files (workbook sample data + marked additions)
├── tests/test_student.cpp           self-contained harness, 39 tests (no framework)
└── tools/hash_password.cpp          prints a salted hash for exam_users.txt
```

## Build — TDM-GCC 5.1.0 (Windows)
Open the **TDM-GCC Command Prompt** (or put `TDM-GCC-xx\bin` on PATH), `cd` into the project folder, then either run

```
build_windows.bat test
```

or compile by hand (file list written out explicitly, so no wildcard expansion is needed):

```
g++ -std=c++14 -O2 -Wall -Wextra -Iinclude main.cpp src/Answer.cpp src/Exam.cpp src/ExamDetail.cpp src/ExamManager.cpp src/ExamReservation.cpp src/ExamResult.cpp src/ExamSession.cpp src/FileManager.cpp src/MultipleChoiceQuestion.cpp src/PasswordHasher.cpp src/Platform.cpp src/Question.cpp src/RunningQuestion.cpp src/Section.cpp src/SingleChoiceQuestion.cpp src/Student.cpp src/Subject.cpp src/User.cpp src/Util.cpp -static-libgcc -static-libstdc++ -o exam_student.exe
g++ -std=c++14 -O2 -Wall -Wextra -Iinclude tests/test_student.cpp src/Answer.cpp src/Exam.cpp src/ExamDetail.cpp src/ExamManager.cpp src/ExamReservation.cpp src/ExamResult.cpp src/ExamSession.cpp src/FileManager.cpp src/MultipleChoiceQuestion.cpp src/PasswordHasher.cpp src/Platform.cpp src/Question.cpp src/RunningQuestion.cpp src/Section.cpp src/SingleChoiceQuestion.cpp src/Student.cpp src/Subject.cpp src/User.cpp src/Util.cpp -static-libgcc -static-libstdc++ -o run_tests.exe
g++ -std=c++14 -O2 -Iinclude tools/hash_password.cpp src/PasswordHasher.cpp src/Util.cpp src/Platform.cpp -o hash_password.exe
run_tests.exe data
exam_student.exe --data data
```
**Code::Blocks:** open `ExamManagementSystem.cbp` from inside the unzipped folder; targets *Student App* (default), *Tests*, *Hash Tool*; `-std=c++14` and the include path are preset.
In *Settings → Compiler → Toolchain executables* point to the TDM-GCC folder. **Linux/macOS/MSYS:** `make`, `make test`.
The tests create/remove a scratch folder `ems_test_tmp/` in the working directory and never modify `data/`. Running the **app** writes to
`exam_reservations.txt`, `exam_running.txt`, `exam_running_ans.txt` (plus `.bak` copies) — use `--data <copy>` to keep the fixtures pristine.
Fixture logins (from the workbook): `shooter/kokowawa`, `ali100/aliwika123`, `mona2000/monaooa123`; added `admin/admin123` (ADMIN — rejected by the Student app).

## TDM-GCC 5.1.0 pitfalls that the code deliberately avoids
| Risk on old MinGW | What the project does |
|---|---|
| `<optional>`, `<filesystem>` do not exist | `OptionalInt` (tiny class) and `Platform.h` (stat/_mkdir/MoveFileEx/rename wrappers) |
| `std::to_string`, `std::stoi/stoll/stoul` missing/broken in some MinGW setups | `util::toString`, hand-written strict `util::parseInt` |
| `std::random_device` is deterministic before GCC 9.2 (same "random" salt and question order every run!) | `CryptGenRandom` (Windows) or `/dev/urandom` via `platform::secureRandomBytes`; `makeSeededEngine()` for shuffling; test checks two draws differ |
| `rename()` fails if the target exists on Windows | `MoveFileExA(..., MOVEFILE_REPLACE_EXISTING \| MOVEFILE_WRITE_THROUGH)` |
| `-std=c++14` (strict ANSI) hides POSIX names like `stat`/`S_IFREG` | underscore names (`_stat`, `_S_IFREG`, `_mkdir`) on Windows |
| C++14 needs out-of-class definitions of ODR-used `static constexpr` members | added in `Question.cpp`, `RunningQuestion.cpp`, `PasswordHasher.cpp` |
| `std::array` brace elision in C++11/14 | explicit double braces |
| `steady_clock` may be backed by the system clock in GCC 5 / win32 threads | documented limitation: a manual clock change during an exam could shift the timer |

## Schema inconsistencies found and decisions taken
| # | Finding | Decision |
|---|---|---|
| 1 | Workbook `exam_users` has plaintext passwords and **no role**; PDF adds `role` + hash. | 6 columns `firstname\|lastname\|email\|username\|password_hash\|role`. Plaintext in the file is **rejected at load** (`User` accepts only a well-formed hash). |
| 2 | PDF sample makes `shooter` ADMIN, but workbook screens/reservations show him taking exams. | Fixture: `shooter` = STUDENT; extra `admin` = ADMIN. |
| 3 | Reservations: workbook has 10 columns (`…pass_sore, max_score`), PDF 9. Workbook writes empty as the text `Null`. | Reader accepts **9 or 10** columns and `Null`/empty; missing `max_score` falls back to the exam's. Writer emits 10. |
| 4 | Workbook `pass_sore` = 700 on all rows; PDF/`exams.txt`: 600 (INT-WEB), 800 (PCAP). | Fixture uses the PDF values; the reservation keeps a snapshot of pass/max score. |
| 5 | Status names differ (`Reserve/Finish` vs `Reserved/Finished`); PDF listing garbled. | Names are data-driven from `reservation_status.txt`; codes 1–4 fixed. Only **Active** can be started, **Running** resumed; **Reserve** = not yet activated; **Finish** is final. Date is informational. |
| 6 | `exam_running` has 4 columns; `quest_real_no` is ambiguous across subjects. | Reader accepts the original **4 columns** (resolved through the exam's subjects, error if ambiguous) and **6** (`…\|subject_code\|section_code` appended). Writer emits 6. |
| 7 | `quest_score` scale undefined (0/1) vs max_score 1000. | `quest_score` = raw marks (0/1). `score = round(earned/total × max_score)`; pass ⇔ `score ≥ pass_score`. |
| 8 | Workbook screen 5 averages section percentages; PDF is question-weighted. | Question-weighted (PDF). |
| 9 | `question_no` unique per subject; `question_answers` has no section column. | Question key = `(subject_code, question_no)`; duplicates rejected. |
| 10 | `rem_time` is in **minutes** (screen 4). | Stored as whole minutes (rounded up); timing in seconds with `std::chrono`. |
| 11 | Sample question 2 (`a = b = c *= 5`, c=20) evaluates to **100**; no option is 100, yet "10" is marked correct. | Data kept as-is (Admin's content); flagged. |
| 12 | Sample data cannot start any exam (`000-CCPP` needs 30 questions, bank has 3; other exams have no `exam_details`). | Kept untouched (gives the "insufficient questions" case). Added exam **QUICK-C** + reservations 5–9 (marked `# --- fixture addition`). |
| 13 | Files have no header row; PDF's `#` lines are labels. | `#` comments, blank lines, CRLF and UTF-8 BOM accepted on read; not preserved when the 3 student-owned files are rewritten. |

## Design
* **Responsibilities.** `FileManager` = persistence. `ExamManager` = in-memory aggregate of the 12 tables (`add*()` enforce rules 1–6, 12) + `generateExam`/`gradeExam` (as the PDF assigns them).
  `ExamSession` = one attempt (rules 7–11, autosave, timer). `Student` = console workflow. Only `exam_reservations`, `exam_running`, `exam_running_ans` are written by this module.
* **Safe I/O.** temp file → flush/check → `.bak` of previous version → atomic replace. Each save **re-reads the current files and replaces only this reservation's rows** (other students' progress survives);
  a malformed file aborts the save untouched; memory changes only after a successful save. Order: running_ans → running → reservation.
* **Fail-fast loading.** Missing → `FileException`; bad record → `FileFormatException` (`file:line`); duplicates → `DuplicateIdException`; unknown type → `InvalidQuestionTypeException`; rule violations → `NotFound/ValidationException`.
* **Resume.** A Running exam is re-linked to the bank; the frozen `corr_ans` snapshot must still equal the bank or the resume is refused.
* **Timing.** deadline = `steady_clock` now + remaining; checked before every action; late answers rejected, attempt graded and finished automatically. `ITimeSource` lets tests inject a manual clock.
* **OOP map.** Encapsulation everywhere · `Question → Single/MultipleChoiceQuestion`, `User → Student` · runtime polymorphism: `Question::display/grade/validate/validateSelection`, `User::showMenu`, `ITimeSource` ·
  association `Student↔ExamReservation`, `RunningQuestion→Question` · aggregation exams/attempts reference bank questions · composition `Question`⊃`Answer`, `Exam`⊃`ExamDetail`, `RunningQuestion`⊃`RunningAnswer` ·
  ctor/dtor + statics `Question::liveCount()`, `User::liveCount()` · operators `<<` (Exam, ExamResult), `==` (Question), `+=` (Exam += ExamDetail), `[]` (Question, bounds-checked) ·
  STL `vector, map, unordered_map, set, unique_ptr` (`std::make_unique` is C++14 and present in GCC 5.1).

## Password storage — dependency decision
Passwords are never stored in plaintext; the `User` constructor refuses anything that is not a well-formed hash. Format: `pbkdf2-sha256$<iterations>$<salt hex>$<hash hex>`.

| Option | Verdict for TDM-GCC 5.1.0 |
|---|---|
| libsodium (Argon2id) — best algorithm | needs a MinGW build of the library + headers matching TDM-GCC; not guaranteed, not installable by default → **optional extension point only** |
| OpenSSL `PKCS5_PBKDF2_HMAC` | no OpenSSL ships with TDM-GCC; works if you have a MinGW build: compile with `-DEMS_USE_OPENSSL` and link `-lcrypto` (**implemented; tested on Linux only**) |
| Windows CNG `BCryptDeriveKeyPBKDF2` | depends on TDM's bundled headers/import library and Windows ≥ 7; could not be verified → not used |
| **Default: built-in PBKDF2-HMAC-SHA256** (RFC 8018 / RFC 2104 / FIPS 180-4) | no dependency. This is the **standardised algorithm, not an invented one**; checked against published SHA-256 and PBKDF2 test vectors (3+3 in the tests) and cross-checked against OpenSSL |

**Limitations of the fallback (be aware):** it is our own implementation of the standard (not independently audited); PBKDF2 is not memory-hard, so it is weaker against GPU attacks than Argon2id/scrypt;
100 000 iterations is **below the current OWASP guidance (≈600 000 for PBKDF2-HMAC-SHA256)** — chosen to keep logins responsive on old machines; the iteration count is stored in each hash, so raise it with `hash_password <pw> 600000`;
the code does not guarantee constant-time behaviour beyond the final comparison; `std::random_device` is not used (see pitfalls). Unknown users cost the same hashing work and give the same error text. **Not a substitute for a vetted library in production.**
Generate a hash for a new user: `hash_password <password> [iterations]`.

## Interfaces for the future modules
* **Admin:** derive `Admin : public User`, override `showMenu`, add one `case` in `FileManager::loadUsers` (ADMIN rows are plain `User`s today). Reuse `ExamManager::add*()`; add update/remove there and `FileManager::save*` for the 9 Admin-owned files (`writeRecords` is the atomic writer to reuse).
* **ExamManager:** extend the existing class rather than duplicate. The Student side needs only `findReservation/reservationsFor`, `generateExam`, `gradeExam`, `storedResult`.

## Tests (`tests/test_student.cpp`, 39 tests)
hash vectors/salting · 12 files load (4- and 6-column running rows, 9/10-column reservations, `Null`) · malformed/missing/duplicate/invalid-type/rule 3–6,12 files · input validation helpers · `OptionalInt` · platform layer (atomic replace, random bytes) ·
operators & polymorphism · successful and invalid login, non-student login · reservations · exam details · start without valid reservation · insufficient questions (no side effects) · generation/randomness/persistence ·
single vs multiple selection rules · navigation · answer modification · autosave · save/resume (new manager from disk, remaining time) · fixture's interrupted exam · changed-bank detection ·
grading (exact match, normalisation, section report, pass/fail) · stored result of the workbook's finished exam · timeout · finished-exam restrictions (UI, session, reservation, storage) · merge-safe saving · corrupt file never overwritten · CRLF/BOM · end-to-end console workflows.

## Known limitations
No inter-process file locking (merge-on-save narrows, not removes, races) · a crash loses time since the last autosave in the student's favour; clock is paused while away; `rem_time` rounding can return up to 59 s per resume ·
blocking console input: expiry is noticed when the next line arrives · `steady_clock` caveat above · the 32-bit build and real TDM-GCC 5.1.0 were not run · class diagram/screenshots/testing report of PDF §14 are not generated.
