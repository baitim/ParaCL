set(PYTHON_SCRIPT_RUN_VALID "${CMAKE_SOURCE_DIR}/tests/end_to_end/valid/run_valid_tests.py")
set(PYTHON_SCRIPT_RUN_ERROR "${CMAKE_SOURCE_DIR}/tests/end_to_end/error/run_error_tests.py")

add_test(
    NAME end_to_end_valid_paracl_target
    COMMAND bash -c "python3 ${PYTHON_SCRIPT_RUN_VALID}"
)

add_test(
    NAME end_to_end_error_paracl_target
    COMMAND bash -c "python3 ${PYTHON_SCRIPT_RUN_ERROR}"
)