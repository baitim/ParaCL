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
    COMPILE_FLAGS "-Wcounterexamples"
    DEFINES_FILE ${BISON_PARSER_TAB_HH}
)

add_flex_bison_dependency(lexer parser)

if(DEFINED ENV{CONAN_PACKAGE})
    add_custom_target(copy_parser_tab ALL
        COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_CURRENT_SOURCE_DIR}/parser.tab.hh
        ${CMAKE_CURRENT_BINARY_DIR}/parser.tab.hh
        COMMENT "Copying parser.tab.hh from source directory to binary directory"
        DEPENDS ${BISON_parser_OUTPUTS}
    )
endif()

add_executable(paracl
    paracl.cpp
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

if(DEFINED ENV{CONAN_PACKAGE})
    include(GNUInstallDirs)

    add_library(ParaCL
        paracl.cpp
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
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    )

    add_dependencies(ParaCL copy_parser_tab)

    install(TARGETS ParaCL EXPORT ParaCLTargets
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    )

    install(DIRECTORY ${INCLUDE_DIR}/ParaCL DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

    install(EXPORT ParaCLTargets
        FILE ParaCLConfig.cmake
        DESTINATION lib/cmake/ParaCL
    )
endif()