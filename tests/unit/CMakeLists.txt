cmake_minimum_required(VERSION 3.11)
project(unit)

find_package(GTest REQUIRED)

add_executable(unit_paracl paracl_unit_tests.cpp)
target_link_libraries(unit_paracl PRIVATE gtest::gtest)

set(RUN_TESTS ./unit_paracl --gtest_color=yes)
add_test(
    NAME unit_paracl_target
    COMMAND ${RUN_TESTS}
)