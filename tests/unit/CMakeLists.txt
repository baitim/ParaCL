find_package(GTest REQUIRED)

add_executable(unit_paracl paracl_unit_tests.cpp)
target_link_libraries(unit_paracl PRIVATE GTest::GTest)

set(RUN_TESTS ./unit_paracl --gtest_color=yes)
add_test(
    NAME unit_paracl_target
    COMMAND ${RUN_TESTS}
)