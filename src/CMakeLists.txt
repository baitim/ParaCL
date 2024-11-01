cmake_minimum_required(VERSION 3.11)
project(src)

set(INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../include)

find_package(BISON REQUIRED)
find_package(FLEX  REQUIRED)

flex_target(lexer
    lexer.l
    ${CMAKE_CURRENT_BINARY_DIR}/lexer.cpp
)

bison_target(parser
    parser.y
    ${CMAKE_CURRENT_BINARY_DIR}/parser.cpp
    COMPILE_FLAGS "--defines=${CMAKE_CURRENT_BINARY_DIR}/parser.tab.hh"
)

add_flex_bison_dependency(lexer parser)

add_executable(paracl
    driver.cpp
    ${BISON_parser_OUTPUTS}
    ${FLEX_lexer_OUTPUTS}
)
target_include_directories(paracl PUBLIC ${INCLUDE_DIR})
target_include_directories(paracl PUBLIC ${CMAKE_CURRENT_BINARY_DIR})

target_compile_features(paracl PRIVATE cxx_std_20)