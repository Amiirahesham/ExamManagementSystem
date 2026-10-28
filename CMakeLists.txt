cmake_minimum_required(VERSION 3.10)
project(ExamManagementSystemStudent CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(NOT MSVC)
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

file(GLOB EMS_SOURCES ${CMAKE_SOURCE_DIR}/src/*.cpp)
add_library(ems STATIC ${EMS_SOURCES})
target_include_directories(ems PUBLIC ${CMAKE_SOURCE_DIR}/include)

add_executable(exam_student main.cpp)
target_link_libraries(exam_student ems)

add_executable(hash_password tools/hash_password.cpp)
target_link_libraries(hash_password ems)

enable_testing()
add_executable(run_tests tests/test_student.cpp)
target_link_libraries(run_tests ems)
add_test(NAME student_tests COMMAND run_tests ${CMAKE_SOURCE_DIR}/data)
