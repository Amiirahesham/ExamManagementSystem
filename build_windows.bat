@echo off
rem Builds the Student module with TDM-GCC 5.1.0 (or any MinGW g++) using -std=c++14.
rem Run from the project folder (the one that contains main.cpp, src, include, data).
setlocal
set FLAGS=-std=c++14 -O2 -Wall -Wextra -Iinclude
set LINK=-static-libgcc -static-libstdc++
set SRCS=src/Answer.cpp src/Exam.cpp src/ExamDetail.cpp src/ExamManager.cpp src/ExamReservation.cpp src/ExamResult.cpp src/ExamSession.cpp src/FileManager.cpp src/MultipleChoiceQuestion.cpp src/PasswordHasher.cpp src/Platform.cpp src/Question.cpp src/RunningQuestion.cpp src/Section.cpp src/SingleChoiceQuestion.cpp src/Student.cpp src/Subject.cpp src/User.cpp src/Util.cpp
where g++ >nul 2>nul || (echo g++ was not found. Open the "TDM-GCC Command Prompt" or add TDM-GCC\bin to PATH. & exit /b 1)
if not exist bin mkdir bin
echo [1/3] exam_student.exe
g++ %FLAGS% main.cpp %SRCS% %LINK% -o bin\exam_student.exe || goto :fail
echo [2/3] run_tests.exe
g++ %FLAGS% tests/test_student.cpp %SRCS% %LINK% -o bin\run_tests.exe || goto :fail
echo [3/3] hash_password.exe
g++ %FLAGS% tools/hash_password.cpp src/PasswordHasher.cpp src/Util.cpp src/Platform.cpp %LINK% -o bin\hash_password.exe || goto :fail
echo.
echo Build OK.  Run tests:   bin\run_tests.exe data
echo            Run the app: bin\exam_student.exe --data data
if /i "%1"=="test" bin\run_tests.exe data
exit /b 0
:fail
echo BUILD FAILED
exit /b 1
