cmake_minimum_required(VERSION 3.11)
project(src)

set(INCLUDE_DIR ${CMAKE_SOURCE_DIR}/include)

find_package(FLEX  REQUIRED)
find_package(BISON REQUIRED)

flex_target(lexer
    lexer.l
    ${CMAKE_CURRENT_BINARY_DIR}/lexer.cpp
)

if(DEFINED ENV{CONAN_PACKAGE})
    set(BISON_PARSER_TAB_HH "parser.tab.hh")
else()
    set(BISON_PARSER_TAB_HH "${CMAKE_CURRENT_BINARY_DIR}/parser.tab.hh")
endif()

bison_target(parser
    parser.y
    ${CMAKE_CURRENT_BINARY_DIR}/parser.cpp
    COMPILE_FLAGS "-Wcounterexamples --defines=${BISON_PARSER_TAB_HH}"
)

add_flex_bison_dependency(lexer parser)

if(DEFINED ENV{CONAN_PACKAGE})
    add_custom_target(copy_parser_tab ALL
        COMMAND ${CMAKE_COMMAND} -E rename
        ${CMAKE_CURRENT_SOURCE_DIR}/parser.tab.hh
        ${CMAKE_CURRENT_BINARY_DIR}/parser.tab.hh
        COMMENT "Copying parser.tab.hh from source directory to binary directory"
        DEPENDS ${BISON_parser_OUTPUTS}
    )
endif()

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
if(DEFINED ENV{CONAN_PACKAGE})
    add_dependencies(paracl copy_parser_tab)
endif()

add_library(ParaCL
    driver.cpp
    ${BISON_parser_OUTPUTS}
    ${FLEX_lexer_OUTPUTS}
)
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
if(DEFINED ENV{CONAN_PACKAGE})
    add_dependencies(ParaCL copy_parser_tab)
endif()

install(TARGETS ParaCL EXPORT ParaCLTargets
    INCLUDES DESTINATION include
)

install(DIRECTORY ${INCLUDE_DIR}/ParaCL DESTINATION include)

install(EXPORT ParaCLTargets
    FILE ParaCLConfig.cmake
    DESTINATION lib/cmake/ParaCL
)