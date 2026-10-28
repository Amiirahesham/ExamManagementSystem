# C++14 build (GCC >= 5). On Windows/TDM-GCC prefer build_windows.bat or the Code::Blocks project.
# Build: make            (student app + tests + hash tool)   Run tests: make test
CXX      ?= g++
CXXFLAGS ?= -std=c++14 -Wall -Wextra -Wpedantic -O2
INC      := -Iinclude
SRC      := $(filter-out src/main.cpp,$(wildcard src/*.cpp))
OBJ      := $(SRC:src/%.cpp=build/%.o)

all: exam_student tests/run_tests hash_password

build/%.o: src/%.cpp $(wildcard include/*.h)
	@mkdir -p build
	$(CXX) $(CXXFLAGS) $(INC) -c $< -o $@

exam_student: main.cpp $(OBJ)
	$(CXX) $(CXXFLAGS) $(INC) main.cpp $(OBJ) -o $@

tests/run_tests: tests/test_student.cpp $(OBJ)
	$(CXX) $(CXXFLAGS) $(INC) tests/test_student.cpp $(OBJ) -o $@

hash_password: tools/hash_password.cpp build/PasswordHasher.o build/Util.o build/Platform.o
	$(CXX) $(CXXFLAGS) $(INC) tools/hash_password.cpp build/PasswordHasher.o build/Util.o build/Platform.o -o $@

test: tests/run_tests
	./tests/run_tests data

clean:
	rm -rf build exam_student hash_password tests/run_tests

.PHONY: all test clean
