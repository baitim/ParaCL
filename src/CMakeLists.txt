cmake_minimum_required(VERSION 3.11)
project(src)

set(INCLUDE_DIR ${CMAKE_SOURCE_DIR}/include)

find_package(FLEX  REQUIRED)
find_package(BISON REQUIRED)

flex_target(lexer
    lexer.l
    ${CMAKE_CURRENT_BINARY_DIR}/lexer.cpp
)

bison_target(parser
    parser.y
    ${CMAKE_CURRENT_BINARY_DIR}/parser.cpp
    COMPILE_FLAGS "-Wcounterexamples --defines=${CMAKE_CURRENT_BINARY_DIR}/parser.tab.hh"
)

add_flex_bison_dependency(lexer parser)

function(pre_build_modify_parser_header target_name)
    add_custom_command(
        TARGET ${target_name} PRE_BUILD
        COMMAND sed -i -e "45s/.*/#ifndef YY_YY_PARACL_SRC_PARSER_TAB_HH/" ${CMAKE_CURRENT_BINARY_DIR}/parser.tab.hh
        COMMAND sed -i -e "46s/.*/#define YY_YY_PARACL_SRC_PARSER_TAB_HH/" ${CMAKE_CURRENT_BINARY_DIR}/parser.tab.hh
        COMMENT "Modifying parser.tab.hh for target ${target_name}"
        DEPENDS ${BISON_parser_OUTPUTS}
    )
endfunction()

add_executable(paracl
    driver.cpp
    ${BISON_parser_OUTPUTS}
    ${FLEX_lexer_OUTPUTS}
)
target_sources(paracl
    PRIVATE
    FILE_SET HEADERS
    BASE_DIRS ${INCLUDE_DIR} ${CMAKE_CURRENT_BINARY_DIR}
)
pre_build_modify_parser_header(paracl)


add_library(ParaCL driver.cpp)
pre_build_modify_parser_header(ParaCL)
target_sources(ParaCL
    PRIVATE
    FILE_SET HEADERS
    BASE_DIRS ${INCLUDE_DIR} ${CMAKE_CURRENT_BINARY_DIR}
)
target_include_directories(ParaCL INTERFACE
    $<BUILD_INTERFACE:${INCLUDE_DIR}>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
    $<INSTALL_INTERFACE:include>
)

install(TARGETS ParaCL EXPORT ParaCLTargets
    INCLUDES DESTINATION include
)

install(DIRECTORY ${INCLUDE_DIR}/ParaCL DESTINATION include)

install(EXPORT ParaCLTargets
    FILE ParaCLConfig.cmake
    DESTINATION lib/cmake/ParaCL
)